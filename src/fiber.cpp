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

// 保存当前线程正在执行的协程
static thread_local Fiber *running_fiber = nullptr;
static thread_local Fiber::Ptr main_fiber_of_this_thread = nullptr;

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
  running_fiber = this;
  _id = s_fiber_id++;

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  ++s_fiber_count;
  // DebugL << "Fiber::Fiber main";
}

Fiber::Fiber(std::function<void()> &&cb, size_t stack_size, bool run_in_schduler)
    : _id(s_fiber_id++), _cb(std::forward<std::function<void()>>(cb)), _run_in_scheduler(run_in_schduler) {
  //如果主协程未创建，先创建主协程
  if (UNLIKELY(!main_fiber_of_this_thread)) {
    // 创建主协程
    // auto main_fiber = std::make_shared<Fiber>();
    auto main_fiber = Fiber::Ptr(new Fiber);
    //判断主协程的指针是否已被current_fiber保存
    ASSERT(running_fiber == main_fiber.get());
    // 保存主协程指针
    main_fiber_of_this_thread = main_fiber;
  }

  s_fiber_count++;
  _stack_size = _stack_size ? _stack_size : FIBER_STACK_SIZE;
  _stack = StackAllocator ::Alloc(_stack_size);

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  _ctx.uc_link = nullptr;  //本contex terminate之后不再自动执行其他context
  _ctx.uc_stack.ss_sp = _stack;
  _ctx.uc_stack.ss_size = _stack_size;

  makecontext(&_ctx, &Fiber::main_func, 0);
}
Fiber::~Fiber() {
  s_fiber_count--;
  //子协程
  if (_stack) {
    ASSERT(_state == TERMINATED || _state == EXCEPT || _state == INIT);
    StackAllocator::Dealloc(_stack, _stack_size);
  } else {         //主协程
    ASSERT(!_cb);  //主协程没有callback

    ASSERT(_state == RUNNING);  //主协程是第一个创建的，所以也是最后一个析构的，所以它析构时肯定处于RUNNING状态
    ASSERT(running_fiber == this);

    running_fiber = nullptr;
  }
  DebugL << "Fiber " << get_id();
}

void Fiber::reuse(std::function<void()> &&cb) {
  ASSERT(_stack);
  ASSERT(_state == TERMINATED || _state == EXCEPT || _state == INIT);

  _cb = std::forward<std::function<void()>>(cb);

  if (getcontext(&_ctx) == -1) {
    ASSERT2(false, getcontext);
  }

  _ctx.uc_link = nullptr;  //本contex terminate之后不再自动执行其他context
  _ctx.uc_stack.ss_sp = _stack;
  _ctx.uc_stack.ss_size = _stack_size;

  makecontext(&_ctx, &Fiber::main_func, 0);
  _state = INIT;
}

void Fiber::swap_in() {
  running_fiber = this;
  ASSERT(_state != RUNNING);
  _state = RUNNING;
  if (swapcontext(&(main_fiber_of_this_thread->_ctx), &_ctx)) {
    ASSERT2(false, swapcontext);
  }
}

void Fiber::swap_out() {
  ASSERT(this == running_fiber);

  running_fiber = main_fiber_of_this_thread.get();

  if (swapcontext(&_ctx, &(main_fiber_of_this_thread->_ctx))) {
    ASSERT2(false, swapcontext);
  }
}
/************************静态方法**********************/
void Fiber::yield_to_hold() {
  Fiber::Ptr cur = running_fiber->shared_from_this();
  ASSERT(cur->_state == RUNNING);
  cur->_state = READY;
  cur->swap_out();
}

void Fiber::yield_to_ready() {
  Fiber::Ptr cur = running_fiber->shared_from_this();
  ASSERT(cur->_state == RUNNING);
  cur->_state = READY;
  cur->swap_out();
}

uint64_t Fiber::get_fiber_id() {
  if (running_fiber) {
    return running_fiber->get_id();
  }
  return 0;
}

void Fiber::main_func() {
  // 只有this调用swap_in才会执行此函数，所以running_fiber一定不为空
  auto cur = running_fiber;
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
  cur->swap_out();

  ASSERT2(false, "never reach");
}
/****************************************************/
}  // namespace fleet