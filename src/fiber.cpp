#include <ucontext.h>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <utility>

#include "Fiber/fiber.h"
#include "Utils/log.h"
#include "Utils/macro.h"
#include "Utils/utils.h"

namespace fleet {

// 全局静态变量，默认协程栈大小
static uint64_t FIBER_STACK_SIZE = 128 * 1024;
// 全局静态变量，用于生成协程id
static std::atomic<uint64_t> s_fiber_id{0};
// 全局静态变量，用于统计当前协程数
static std::atomic<uint64_t> s_fiber_count{0};

// 指向当前线程正在执行的协程
static thread_local Fiber *t_running_fiber = nullptr;
// 保存主协程的上下文
static thread_local Fiber::Ptr t_main_fiber_of_this_thread = nullptr;

/**
 * @brief malloc栈内存分配器
 */
class MallocStackAllocatorr {
 public:
  static void *Alloc(size_t size) { return malloc(size); }
  static void Dealloc(void *vp, size_t size) { free(vp); }
};

using StackAllocator = MallocStackAllocatorr;

Fiber::Fiber() {
  _state = RUNNING;
  t_running_fiber = this;
  _id = ++s_fiber_id;  // 协程id非零

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  ++s_fiber_count;
  // DebugL << "Fiber::Fiber main";
}

// 这里cb只能传右值，不能传左值
Fiber::Fiber(std::function<void()> &&cb, size_t stack_size, bool run_in_schduler)
    : _cb(std::move(cb)), _run_in_scheduler(run_in_schduler) {
  get_this();          // 如果没有主协程，创建之
  _id = ++s_fiber_id;  // 要在主协程创建之后取id
  s_fiber_count++;
  _stack_size = _stack_size ? _stack_size : FIBER_STACK_SIZE;
  _stack = StackAllocator ::Alloc(_stack_size);

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  _ctx.uc_link = nullptr;  // 本contex terminate之后不再自动执行其他context
  _ctx.uc_stack.ss_sp = _stack;
  _ctx.uc_stack.ss_size = _stack_size;

  // 将man_func绑定到_ctx上，但不会立即执行
  makecontext(&_ctx, &Fiber::main_func, 0);
}
Fiber::~Fiber() {
  s_fiber_count--;
  // 子协程
  if (_stack) {
    ASSERT(_state == TERMINATED || _state == EXCEPT || _state == INIT);
    StackAllocator::Dealloc(_stack, _stack_size);
  } else {         // 主协程
    ASSERT(!_cb);  // 主协程没有callback

    ASSERT(_state == RUNNING);  // 主协程是第一个创建的，所以也是最后一个析构的，所以它析构时肯定处于RUNNING状态
    ASSERT(t_running_fiber == this);

    t_running_fiber = nullptr;
  }
  DebugL << "Fiber " << get_id();
}

void Fiber::reuse(std::function<void()> &&cb) {
  // 满足这些条件才能reuse
  ASSERT(_stack);
  ASSERT(_state == TERMINATED || _state == EXCEPT || _state == INIT);

  _cb = std::forward<std::function<void()>>(cb);

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  _ctx.uc_link = nullptr;  // 本contex terminate之后不再自动执行其他context
  _ctx.uc_stack.ss_sp = _stack;
  _ctx.uc_stack.ss_size = _stack_size;

  makecontext(&_ctx, &Fiber::main_func, 0);
  _state = INIT;
}

void Fiber::call() {
  t_running_fiber = this;
  ASSERT(_state != RUNNING);
  _state = RUNNING;
  if (swapcontext(&(t_main_fiber_of_this_thread->_ctx), &_ctx) == -1) {
    ASSERT2(false, swapcontext);
  }
}

void Fiber::back() {
  ASSERT(this == t_running_fiber);

  t_running_fiber = t_main_fiber_of_this_thread.get();

  if (swapcontext(&_ctx, &(t_main_fiber_of_this_thread->_ctx)) == -1) {
    ASSERT2(false, swapcontext);
  }
}
/************************静态方法**********************/

Fiber::Ptr Fiber::get_this() {
  if (t_running_fiber) {
    return t_running_fiber->shared_from_this();
  }
  // 创建主协程
  Fiber::Ptr main_fiber(new Fiber);
  ASSERT(t_running_fiber == main_fiber.get());  // 刚创建完主协程
  t_main_fiber_of_this_thread = main_fiber;
  return t_running_fiber->shared_from_this();
}

void Fiber::yield_to_hold() {
  Fiber::Ptr cur = get_this();

  ASSERT(cur->_state == RUNNING);
  cur->_state = HOLD;
  cur->back();
}

void Fiber::yield_to_ready() {
  Fiber::Ptr cur = get_this();
  ASSERT(cur->_state == RUNNING);
  cur->_state = READY;
  cur->back();
}

uint64_t Fiber::get_fiber_id() {
  if (t_running_fiber) {
    return t_running_fiber->get_id();
  }
  return 0;  // 代表线程中没有任何协程运行
}

void Fiber::main_func() {
  // 调用swap_in才会执行此函数，所以running_fiber一定不为空
  auto cur = get_this().get();
  ASSERT(cur);
  try {
    cur->_cb();
    cur->_state = TERMINATED;

  } catch (std::exception &ex) {
    cur->_state = EXCEPT;
    ErrorL << "Fiber Exception: " << ex.what() << " fiber id = " << cur->get_id();
    ErrorL << backtrace_to_string();
  } /* catch (...) {
     cur->_state = EXCEPT;
     ErrorL << "Fiber Exception: " << " fiber id = " << cur->get_id();
     ErrorL << backtrace_to_string();
   }*/
  cur->back();

  ASSERT2(false, "never reach");
}
/****************************************************/
}  // namespace fleet