#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Fiber/fiber.h"
#include "Thread/mutex.h"
#include "Thread/thread.h"

namespace fleet {
class Scheduler {
 public:
  using Ptr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;  // 方便更换

  Scheduler(size_t threads = 1, bool use_main_thread = true, const std::string &name = "");

  virtual ~Scheduler();

  const std::string &get_name() const { return _name; };

  // 返回当前协程调度器
  static Scheduler *s_get_this();

  // 创建scheduler的线程
  void start();

  void stop();

  template <class FiberOrCb>
  void schedule(const FiberOrCb &fc, int thread = -1) {
    {
      MutexType::Lock lock(_mutex);
      schedule_impl(fc, thread);
    }
    notify();
  }

  // 一次性传入多个协程，可以保证协程的顺序，而且只用加一次锁
  template <class Iterator>
  void schedule(Iterator begin, Iterator end, uint64_t thread = -1) {
    {
      MutexType::Lock lock(_mutex);
      auto iter = begin;
      while (iter != end) {
        schedule_impl(*iter, thread);
        iter++;
      }
    }
    notify();
  }

 protected:
  // 任务到来通知
  virtual void notify();

  // 协程调度函数
  void run();

  // 返回是否可以停止
  virtual bool stopping();

  // 协程无任务可调度时执行idle协程
  virtual void idle();

  // 是否有空闲线程
  bool hasIdleThreads() { return _idle_thread_count > 0; }

 private:
  template <class FiberOrCb>
  void schedule_impl(const FiberOrCb &fc, int thread) {
    auto ft = std::make_shared<FiberAndThread>(fc, thread);
    if (ft->fiber || ft->cb) {
      _fiber_and_threads.push_back(ft);
    }
  }

 private:
  struct FiberAndThread {
    using Ptr = std::shared_ptr<FiberAndThread>;

    // 协程
    Fiber::Ptr fiber;
    // 函数
    std::function<void()> cb;

    // 线程id
    uint64_t thread;

    FiberAndThread(const Fiber::Ptr &fb, int thr) : fiber(fb), thread(thr) {}

    FiberAndThread(const std::function<void()> &f, int thr) : cb(f), thread(thr) {}

    FiberAndThread() : thread(-1) {}

    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

 private:
  MutexType _mutex;
  // 线程池
  std::vector<Thread::Ptr> _threads;
  // 待执行的协程队列
  std::list<FiberAndThread::Ptr> _fiber_and_threads;
  // 调度器名称
  std::string _name;
  // 是否使用主线程来处理任务
  bool _use_main_thread;

 protected:
  //
  std::vector<int> _thread_ids;
  // 线程数量
  size_t _thread_count = 0;
  // 工作线程数
  std::atomic<size_t> _active_thread_count = {0};
  // 空闲线程数
  std::atomic<size_t> _idle_thread_count = {0};
  // 是否自动停止(暂时不知道是什么作用)
  bool _auto_stop = false;
  // 是否正在停止，默认为true
  bool _stopping = true;
};
}  // namespace fleet