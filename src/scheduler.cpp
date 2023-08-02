#include <cstddef>
#include <memory>
#include <string>

#include "Fiber/fiber.h"
#include "Fiber/scheduler.h"
#include "IO/hook.h"
#include "Thread/thread.h"
#include "Utils/log.h"
#include "Utils/macro.h"
#include "Utils/utils.h"

namespace fleet {
// 保存当前调度器
static thread_local Scheduler *t_scheduler = nullptr;

Scheduler::Scheduler(size_t threads, const std::string &name) : _name(name) {
  ASSERT(threads > 0);
  _thread_count = threads;
}

Scheduler::~Scheduler() {
  ASSERT(_stopping);
  if (s_get_this() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler *Scheduler::s_get_this() { return t_scheduler; }

void Scheduler::start() {
  MutexType::Lock lock(_mutex);
  if (!_stopping) {  // 未启动前应该是默认为true
    return;
  }
  _stopping = false;  // 启动后为false，当调用stop()方法时又变为true

  for (size_t i = 0; i < _thread_count; ++i) {
    auto th = std::make_shared<Thread>([this]() { Scheduler::run(); },
                                       _name + "_" + std::to_string(i));  // 设置线程名，好调试
    _threads.push_back(th);
    _thread_ids.push_back(th->get_id());  // 记录线程id
  }
}

void Scheduler::stop() {
  _auto_stop = true;
  if (_thread_count == 0) {
    _stopping = true;

    // ?
    if (stopping()) {
      return;
    }
  }

  _stopping = true;
  for (size_t i = 0; i < _thread_count; i++) {
    notify();
  }

  // if (!stopping()) {
  //   fleet::Thread::s_set_name(_name);
  //   _thread_ids.push_back(fleet::get_thread_id());
  //   run();
  // }

  std::vector<Thread::Ptr> thrs;
  {
    MutexType::Lock lock(_mutex);
    thrs.swap(_threads);
  }

  for (auto &t : thrs) {
    t->join();
  }
  InfoL << this << " scheduler stopped";
  // 到这里线程资源就可以回收了，无需等到Scheduler对象被析构
}

void Scheduler::run() {
  DebugL << _name << " run";

  t_scheduler = this;  // 记录

  Fiber::s_get_this();  // 创建线程原始协程

  Fiber::Ptr idle_fiber(new Fiber([this]() { idle(); }));

  while (true) {
    bool notify_me = false;

    Task::Ptr task = nullptr;

    {
      MutexType::Lock lock(_task_mutex);
      auto it = _tasks.begin();
      while (it != _tasks.end()) {
        task = *it;
        if (task->thread_id != -1 && task->thread_id != fleet::get_thread_id()) {
          // 如果指定了线程而且指定的线程不是此线程
          ++it;
          notify_me = true;  // 没遍历完
          continue;
        }

        ASSERT(task->fiber || task->cb);  // fiber和cb至少得有一个

        if (task->fiber && task->fiber->get_state() == Fiber::RUNNING) {
          ++it;
          continue;
        }

        it = _tasks.erase(it);   // 从队列中删除此任务
        ++_active_thread_count;  // 线程进入活跃状态
        break;
      }
      notify_me |= (it != _tasks.end());  // 还没有遍历完
    }
    if (notify_me) {
      notify();
    }

    if (task) {           // 拿到任务
      if (task->fiber) {  // 是fiber
        if (task->fiber->get_state() == Fiber::TERMINATED || task->fiber->get_state() == Fiber::EXCEPT) {
          // T或E状态的任务不用处理
          --_active_thread_count;
          continue;
        } else {
          task->fiber->enter();  // 开始执行
          // 执行结束
          --_active_thread_count;

          if (task->fiber->get_state() == Fiber::READY) {
            // 用户将状态设置为READY表示希望调度器自动将此任务放入调度队列
            schedule(task->fiber);
          }
        }

      } else if (task->cb) {  // 是callback
        auto cb_fiber = std::make_shared<Fiber>(std::move(task->cb));

        cb_fiber->enter();
        --_active_thread_count;

        if (cb_fiber->get_state() == Fiber::READY) {
          schedule(cb_fiber);
        }
      }
    } else {
      // 没有拿到任务，则执行idle协程
      if (idle_fiber->get_state() == Fiber::TERMINATED) {
        InfoL << "idle fiber terminated";
        break;  // 整个run的while循环结束
      }
      ++_idle_thread_count;
      idle_fiber->enter();  // 执行idle协程
      --_idle_thread_count;
    }
  }
}

void Scheduler::notify() { InfoL << "notify"; }

bool Scheduler::stopping() {
  MutexType::Lock lock(_mutex);
  return _auto_stop && _stopping && _tasks.empty() && _active_thread_count == 0;
}

void Scheduler::idle() {
  InfoL << "idle";
  while (!stopping()) {
    fleet::Fiber::yield_to_hold();
  }
}

}  // namespace fleet