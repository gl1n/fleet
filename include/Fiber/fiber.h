#pragma once

#include <sys/types.h>
#include <ucontext.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace fleet {
class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  using Ptr = std::shared_ptr<Fiber>;

  enum State {
    // 初始状态
    INIT,
    // 暂停状态
    HOLD,
    // 就绪态
    READY,
    // 运行态
    RUNNING,
    // 结束态
    TERMINATED,
    // 异常状态
    EXCEPT
  };

 public:
  /**
   * @brief 用于创建用户协程
   * @details 只创建，未执行
   * @param cb 协程入口函数
   * @param stack_size 栈大小
   * @param run_in_scheduler 本协程是否参与调度器调度
   */
  Fiber(std::function<void()> &&cb, size_t stack_size = 0, bool run_in_schduler = true);

  ~Fiber();

  void reuse(std::function<void()> &&cb);

  void call();

  void back();

  uint64_t get_id() const { return _id; }

  State get_state() const { return _state; }

 public:
  // 返回当前所在的协程
  static Fiber::Ptr get_this();

  static void yield_to_hold();

  static void yield_to_ready();

  // 将get_id()封装成静态方法
  static uint64_t get_fiber_id();

 private:
  // 用于创建主协程
  Fiber();
  /**
   * 协程入口函数
   */
  static void main_func();

 private:
  // 协程id
  u_int64_t _id = 0;
  // 协程栈大小
  u_int64_t _stack_size = 0;
  // 协程状态
  State _state = READY;
  // 协程上下文
  ucontext_t _ctx;
  // 协程栈地址
  void *_stack = nullptr;
  // 协程入口函数
  std::function<void()> _cb;
  // 是否参与调度器调优
  bool _run_in_scheduler;
};
}  // namespace fleet