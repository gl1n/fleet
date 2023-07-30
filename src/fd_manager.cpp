#include <asm-generic/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdint>
#include <memory>

#include "fd_manager.h"
#include "mutex.h"

namespace fleet {

FdManager &FdManager::Instance() {
  static FdManager instance;
  return instance;
}

FdCtx::FdCtx(int fd) : _fd(fd) {
  if (_is_init) {
    return;
  }

  struct stat fd_stat;
  // 如果获取_fd状态时出错，则将this设置成未初始化
  if (-1 == fstat(_fd, &fd_stat)) {
    _is_init = false;
    _is_socket = false;
  } else {
    _is_init = true;
    // 通过以下API判断是否是socket
    _is_socket = S_ISSOCK(fd_stat.st_mode);

    // 如果是socket，设置成非阻塞
    if (_is_socket) {
      int flags = fcntl(_fd, F_GETFL, 0);
      if (!(flags & O_NONBLOCK)) {
        fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
      }
      _sys_nonblock = true;  // 默认设置成非阻塞
    } else {
      _sys_nonblock = false;
    }

    _user_nonblock = false;
    _is_close = false;
  }
}

void FdCtx::set_timeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    _recv_timeout = v;
  } else {
    _send_timeout = v;
  }
}

uint64_t FdCtx::get_timeout(int type) {
  if (type == SO_RCVTIMEO) {
    return _recv_timeout;
  } else {
    return _send_timeout;
  }
}

FdCtx::Ptr FdManager::get_FdCtx(int fd) {
  if (fd == -1) {
    return nullptr;
  }

  RWMutexType::ReadLock lock(_fdctxs_mutex);
  auto it = _fdctxs.find(fd);
  // 存在fd
  if (it != _fdctxs.end()) {
    return _fdctxs[fd];
  }
  // 不存在
  return nullptr;
}

FdCtx::Ptr FdManager::create_FdCtx(int fd) {
  if (fd == -1) {
    return nullptr;
  }
  auto ctx = get_FdCtx(fd);
  // 存在
  if (ctx) {
    return ctx;
  }
  // 不存在fd，创建一个
  auto new_ctx =
      std::shared_ptr<FdCtx>(new FdCtx(fd));  // 必须先创建再加锁。因为FdCtx构造函数调用了fcntl，fcntl又调用了get_FdCtx
  RWMutexType::WriteLock lock(_fdctxs_mutex);
  _fdctxs[fd] = new_ctx;
  return _fdctxs[fd];
}

void FdManager::del_FdCtx(int fd) {
  RWMutexType::WriteLock lock(_fdctxs_mutex);
  _fdctxs.erase(fd);  // erase方法可以传入不存在的key
}

}  // namespace fleet