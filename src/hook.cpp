#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "Fiber/fiber.h"
#include "IO/fd_manager.h"
#include "IO/hook.h"
#include "IO/iomanager.h"
#include "IO/timer.h"
#include "Utils/log.h"
#include "Utils/utils.h"

static uint64_t s_connect_timeout = 5000;

namespace fleet {
static thread_local bool t_hook_enable = false;

// 根据需要定义XX宏，然后才能使用HOOK_FUN宏
#define HOOK_FUN(XX) \
  XX(sleep)          \
  XX(usleep)         \
  XX(nanosleep)      \
  XX(socket)         \
  XX(connect)        \
  XX(accept)

void hook_init() {
  static bool is_init = false;
  if (is_init) {
    return;
  }
// 记录系统库的函数
#define XX(name) name##_p = (name##_type)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX)
#undef XX
}  // hook_init() end

struct _HookIniter {
  _HookIniter() { hook_init(); }
};
static _HookIniter s_hook_initer;  // 保证main函数执行前已经完成了hook初始化

bool is_hook_enable() { return t_hook_enable; }

void set_hook_enable(bool flag) { t_hook_enable = flag; }

}  // namespace fleet

template <typename OriginFun, typename... Args>  // 可变模板参数
static ssize_t do_io(int fd, OriginFun func, const char *hook_fun_name, fleet::IOManager::Event event, int timeout_so,
                     Args &&...args) {  // 万能引用
  if (!fleet::t_hook_enable) {
    return func(fd, std::forward<Args>(args)...);  // 完美转发
  }

  auto ctx = fleet::FdManager::Instance().get(fd);

  if (!ctx || ctx->is_close()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->is_socket() || ctx->get_user_nonblock()) {
    return func(fd, std::forward<Args>(args)...);
  }

  uint64_t to = ctx->get_timeout(timeout_so);

  bool retry;
  do {
    retry = false;

    ssize_t n = func(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
      n = func(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) {
      auto iom = fleet::IOManager::s_get_this();
      fleet::Timer::Ptr timer;

      bool is_timeout = false;
      if (to != UINT64_MAX) {
        timer = iom->add_timer(to, [fd, &is_timeout, iom, event]() {
          is_timeout = true;
          iom->del_event(fd, event, true);
        });
      }
      int ret = iom->add_event(fd, event);
      if (ret == 0) {
        fleet::Fiber::s_get_this()->yield_to_hold();
        if (timer) {
          timer->cancel();
        }
        if (is_timeout) {
          errno = ETIMEDOUT;
          return -1;
        }
        retry = true;
      } else {
        if (timer) {
          timer->cancel();
        }
        ErrorL << hook_fun_name << "cannont addEvent(" << socket << ", " << event << ") error";
        return -1;
      }
    }

    return n;
  } while (retry);
}

extern "C" {
// 定义头文件中声明的函数指针
#define XX(name) name##_type name##_p = nullptr;
HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds) {
  // 如果没有开启hook那么直接调用库函数
  if (!fleet::t_hook_enable) {
    return sleep_p(seconds);
  }

  auto fiber_this = fleet::Fiber::s_get_this();
  fleet::IOManager *iom = fleet::IOManager::s_get_this();
  iom->add_timer(seconds * 1000, [iom, fiber_this]() {
    // 定时器结束时重新执行fiber_this
    iom->schedule(fiber_this, fleet::get_thread_id());
  });

  fiber_this->yield_to_hold();  // fiber_this只能由本线程执行，所以定时器的cb只会在yield之后执行
  return 0;
}

// 由于定时器是毫秒级的，所以usleep只能达到毫秒精度
int usleep(useconds_t usec) {
  // 如果没有开启hook那么直接调用库函数
  if (!fleet::t_hook_enable) {
    return usleep_p(usec);
  }

  auto fiber_this = fleet::Fiber::s_get_this();
  fleet::IOManager *iom = fleet::IOManager::s_get_this();
  iom->add_timer(usec / 1000, [iom, fiber_this]() { iom->schedule(fiber_this, fleet::get_thread_id()); });

  fiber_this->yield_to_hold();

  return 0;
}
// 由于定时器是毫秒级的，所以nanosleep只能达到毫秒精度
int nanosleep(const struct timespec *req, struct timespec *rem) {
  // 如果没有开启hook那么直接调用库函数
  if (!fleet::t_hook_enable) {
    return nanosleep_p(req, rem);
  }
  int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;

  auto fiber_this = fleet::Fiber::s_get_this();
  fleet::IOManager *iom = fleet::IOManager::s_get_this();
  iom->add_timer(timeout_ms, [iom, fiber_this]() { iom->schedule(fiber_this, fleet::get_thread_id()); });

  fiber_this->yield_to_hold();

  return 0;
}
int socket(int domain, int type, int protocol) {
  int fd = socket_p(domain, type, protocol);
  if (!fleet::t_hook_enable) {
    return fd;
  }
  if (fd == -1) {
    return fd;
  }
  // 创建关于fd的FdCtx
  fleet::FdManager::Instance().get(fd, true);
  return fd;
}

int connect_with_timeout(int socket, const struct sockaddr *address, socklen_t address_len, uint64_t timeout_ms) {
  if (!fleet::t_hook_enable) {
    return connect_p(socket, address, address_len);
  }
  auto ctx = fleet::FdManager::Instance().get(socket);
  // 不存在socket(fd) 或 socket已关闭
  if (!ctx || ctx->is_close()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->is_socket() && ctx->get_user_nonblock()) {
    return connect_p(socket, address, address_len);
  }

  // ret只有0和-1两种情况
  int ret = connect_p(socket, address, address_len);
  if (ret == 0) {
    return 0;
  } else if (errno != EINPROGRESS) {
    return -1;
  }

  fleet::IOManager *iom = fleet::IOManager::s_get_this();
  fleet::Timer::Ptr timer;

  bool is_timeout = false;
  if (timeout_ms != UINT64_MAX) {
    // timeout_ms有效
    timer = iom->add_timer(timeout_ms, [iom, socket, &is_timeout]() {
      is_timeout = true;
      // 取消对socket的WRITE事件的监听，将trigger_task置为true可以直接执行socket的WRITE事件的回调(协程)，即唤起底下几行代码
      iom->del_event(socket, fleet::IOManager::WRITE, true);
    });
  }

  /**
   * 连接完成时会触发WRITE事件
   */

  ret = iom->add_event(socket, fleet::IOManager::WRITE);
  if (ret == 0) {
    // 成功监听WRITE事件(对应的事件是当前协程)
    fleet::Fiber::s_get_this()->yield_to_hold();
    // 可能是监听到了读事件，也可能是timer超时触发了del_event;
    if (timer) {
      // 不管怎么样，先把timer取消
      timer->cancel();
    }
    if (is_timeout) {
      errno = ETIMEDOUT;
      return -1;
    }
  } else {
    if (timer) {
      timer->cancel();
    }
    ErrorL << "cannont addEvent(" << socket << ", WRITE) error";
    return -1;
  }

  int error = 0;
  socklen_t len = sizeof(int);
  if (-1 == getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len)) {
    return -1;
  }
  if (error) {
    return -1;
  }
  return 0;
}

int connect(int socket, const struct sockaddr *address, socklen_t address_len) {
  return connect_with_timeout(socket, address, address_len, s_connect_timeout);
}

int accept(int socket, struct sockaddr *address, socklen_t *address_len) {
  // socket是服务端，fd是客户端
  int fd = do_io(socket, accept_p, "accept", fleet::IOManager::READ, SO_RCVTIMEO, address, address_len);
  if (fd >= 0) {
    fleet::FdManager::Instance().get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
  return do_io(fd, read_p, "read", fleet::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, readv_p, "readv", fleet::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  return do_io(sockfd, recv_p, "recvv", fleet::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
  return do_io(sockfd, recvfrom_p, "recvfrom", fleet::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  return do_io(sockfd, recvmsg_p, "recvmsg", fleet::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return do_io(fd, write_p, "write", fleet::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, writev_p, "writev", fleet::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int socket, const void *msg, size_t len, int flags) {
  return do_io(socket, send_p, "send", fleet::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int socket, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
  return do_io(socket, sendto_p, "sendto", fleet::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int socket, const struct msghdr *msg, int flags) {
  return do_io(socket, sendmsg_p, "sendmsg", fleet::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
  if (!fleet::t_hook_enable) {
    return close_p(fd);
  }

  auto ctx = fleet::FdManager::Instance().get(fd);
  if (ctx) {
    auto iom = fleet::IOManager::s_get_this();
    if (iom) {
      iom->del_and_trigger_all(fd);
    }
    fleet::FdManager::Instance().del(fd);
  }
  return close_p(fd);
}
}