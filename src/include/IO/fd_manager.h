#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include "Thread/mutex.h"
namespace fleet {
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  using Ptr = std::shared_ptr<FdCtx>;

  FdCtx(int fd);

  ~FdCtx() {}

  bool is_init() const { return _is_init; }

  bool is_socket() const { return _is_socket; }

  bool is_close() const { return _is_close; }

  bool get_user_nonblock() const { return _user_nonblock; }

  void set_user_nonblock(bool v) { _user_nonblock = v; }

  bool get_sys_nonblock() const { return _sys_nonblock; }
  void set_sys_nonblock(bool v) { _sys_nonblock = v; }

  uint64_t get_timeout(int type);
  void set_timeout(int type, uint64_t v);

 private:
  bool _is_init = false;
  bool _is_socket = false;
  bool _is_close = false;

  bool _user_nonblock = false;
  bool _sys_nonblock = false;

  uint64_t _recv_timeout = UINT64_MAX;  // 默认的timeout是无穷
  uint64_t _send_timeout = UINT64_MAX;  // 默认的timeout是无穷

  int _fd;
};

class FdManager {
 public:
  using RWMutexType = RWMutex;
  /**********单例**********/
 public:
  static FdManager &Instance();
  FdManager(const FdManager &) = delete;             // 禁用复制构造函数
  FdManager &operator=(const FdManager &) = delete;  // 禁用赋值函数

 private:
  FdManager() {}
  /***********************/

 public:
  FdCtx::Ptr get_FdCtx(int fd);
  FdCtx::Ptr create_FdCtx(int fd);

  void del_FdCtx(int fd);

 private:
  RWMutexType _fdctxs_mutex;
  // Mutex _fdctxs_mutex;
  std::unordered_map<int, FdCtx::Ptr> _fdctxs;
};
}  // namespace fleet