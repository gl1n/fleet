#pragma once

#include <ucontext.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace fleet {

class Scheduler;
class IOManager;
class Fiber : public std::enable_shared_from_this<Fiber> {
  friend IOManager;

 public:
  using Ptr = std::shared_ptr<Fiber>;

  enum State {
    INIT,     // 初始状态
    HOLD,     // 暂停状态
    READY,    // 就绪态，调度器应该自动重新将此协程放入任务队列
    RUNNING,  // 运行态，表示正在执行的协程
    /*下面两种状态都是协程被动结束(用户没有调用yield_to_hold或yield_to_ready)时产生*/
    TERMINATED,  // 结束态
    EXCEPT       // 异常状态
  };

 public:
  /**
   * @brief 用于创建用户协程
   * @details 只创建，未执行
   * @param cb 协程入口函数
   * @param stack_size 栈大小
   * @param 是否为scheduler的root_fiber
   */
  Fiber(std::function<void()> &&cb, size_t stack_size = 0, bool root_fiber = false);

  ~Fiber();

  void reuse(std::function<void()> &&cb);

  void enter();

  uint64_t get_id() const { return _id; }

  State get_state() const { return _state; }

 public:
  static void yield_to_hold();

  static void yield_to_ready();

  // 将get_id()封装成静态方法
  static uint64_t get_fiber_id();

  // 返回当前所在的协程，必要时会创建线程原始协程
  static Fiber::Ptr s_get_this();

 private:
  // 用于创建主协程
  Fiber();

  /**
   * 协程入口函数
   */
  static void main_func();

  // 只能从内部调用
  void yield();

 private:
  // 协程id
  uint64_t _id = 0;
  // 协程栈大小
  uint64_t _stack_size = 0;
  // 协程状态
  State _state = READY;
  // 协程上下文
  ucontext_t _ctx;
  // 协程栈地址
  void *_stack = nullptr;
  // 协程入口函数
  std::function<void()> _cb;
  // 是否参与调度器调调度
  bool _run_in_scheduler;
};
}  // namespace fleet