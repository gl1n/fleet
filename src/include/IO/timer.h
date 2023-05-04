#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "Thread/mutex.h"
namespace fleet {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
  friend TimerManager;

 public:
  using Ptr = std::shared_ptr<Timer>;

  /**
   * @return true表示成功删除，false表示不存在
   */
  bool cancel();

  bool refresh();

  bool reset(uint64_t period, bool from_now);

 private:
  Timer(uint64_t period, std::function<void()> cb, bool repeat, TimerManager *manager);

  // 创建用于比较的Timer
  Timer(uint64_t next);

 private:
  struct Comparator {
    bool operator()(Timer::Ptr const &lhs, Timer::Ptr const &rhs) { return lhs->_next < rhs->_next; }
  };

 private:
  bool _repeat = false;
  // 执行周期
  uint64_t _period = 0;
  // 绝对执行时间
  uint64_t _next = 0;
  // 回调函数
  std::function<void()> _cb;
  // 定时器管理器
  TimerManager *_manager = nullptr;
};

class TimerManager {
  friend class Timer;

 public:
  using RWMutexType = RWMutex;

  TimerManager();
  // 可能要继承
  virtual ~TimerManager();

  Timer::Ptr add_timer(uint64_t period, std::function<void()> cb, bool repeat = false);

  Timer::Ptr add_condition_timer(uint64_t period, std::function<void()> cb, std::weak_ptr<void> weak_cond,
                                 bool repeat = false);

  uint64_t get_next_timer();

  // 获取要执行的回调列表
  std::vector<std::function<void()>> list_expired_cb();

  bool has_timer();

 protected:
  virtual void on_timer_inserted_front() = 0;

  virtual void add_timer(Timer::Ptr timer, RWMutexType::WriteLock &lock);

 private:
  RWMutexType _timer_list_mutex;
  // 定时器集合
  std::set<Timer::Ptr, Timer::Comparator> _timers;

  // 当有Timer插到最前面时，置为true
  bool _tickled = false;
};
}  // namespace fleet