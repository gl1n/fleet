#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fiber.h"
#include "mutex.h"
#include "thread.h"

namespace fleet {
class Scheduler {
 public:
  using Ptr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;  // 方便更换
  using thread_id_t = int;

  Scheduler(size_t threads = 1, const std::string &name = "");

  virtual ~Scheduler();

  const std::string &get_name() const { return _name; };

  // 返回当前协程调度器
  static Scheduler *s_get_this();

  // 创建scheduler的线程
  virtual void start();

  void stop();

  template <class FiberOrCb>
  void schedule(const FiberOrCb &fc, thread_id_t thread_id = -1) {
    auto ft = std::make_shared<Task>(fc);
    if (ft->fiber || ft->cb) {
      MutexType::Lock lock(_task_mutex);
      _tasks.push_back(ft);
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
  bool has_idle_threads() { return _idle_thread_count > 0; }

 private:
  struct Task {
    using Ptr = std::shared_ptr<Task>;

    // 协程
    Fiber::Ptr fiber;
    // 函数
    std::function<void()> cb;
    // 指定线程号
    thread_id_t thread_id;

    Task(const Fiber::Ptr &fb, thread_id_t ti = -1) : fiber(fb), thread_id(ti) {}

    Task(const std::function<void()> &f, thread_id_t ti = -1) : cb(f), thread_id(ti) {}

    Task() = default;

    void reset() {
      fiber = nullptr;
      cb = nullptr;
    }
  };

 private:
  // 待执行的任务队列
  std::list<Task::Ptr> _tasks;
  // 是否自动停止(暂时不知道是什么作用)
  bool _auto_stop = false;
  // 工作线程数
  std::atomic<size_t> _active_thread_count = {0};
  // 空闲线程数
  std::atomic<size_t> _idle_thread_count = {0};
  // 任务锁
  MutexType _task_mutex;

 protected:
  // 线程池
  std::vector<Thread::Ptr> _threads;
  // 调度器名称
  std::string _name;
  //
  std::vector<int> _thread_ids;
  // 线程数量
  size_t _thread_count = 0;
  // 是否正在停止，默认为true
  bool _stopping = true;
  // 启动或关闭时使用
  MutexType _mutex;
};
}  // namespace fleet