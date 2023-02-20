#include "Utils/timer.h"
#include <memory>
#include "Utils/utils.h"

namespace fleet {
Timer::Timer(uint64_t ms, std::function<void()> cb, bool repeat, TimerManager *manager)
    : _repeat(repeat), _ms(ms), _cb(cb), _manager(manager) {
  _next = _ms + time_since_epoch_millisecs();
}

Timer::Timer(uint64_t next) : _next(next) {}

bool Timer::cancel() {
  // 加锁
  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (_cb) {
    _cb = nullptr;
    auto it = _manager->_timers.find(shared_from_this());
    if (it != _manager->_timers.end()) {
      _manager->_timers.erase(it);
      return true;
    }
  }
  return false;
}

bool Timer::refresh() {
  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (!_cb) {
    // 没有回调直接返回
    return false;
  }
  auto it = _manager->_timers.find(shared_from_this());
  if (it == _manager->_timers.end()) {
    return false;
  }
  // 删掉后重新插入才能保证_timers有序
  _manager->_timers.erase(it);
  _next = _ms + time_since_epoch_millisecs();
  _manager->_timers.insert(shared_from_this());
  return true;
}

TimerManager::TimerManager() { _previous_time = time_since_epoch_millisecs(); }

TimerManager::~TimerManager() {}

Timer::Ptr TimerManager::add_timer(uint64_t ms, std::function<void()> cb, bool repeat) {
  Timer::Ptr timer = std::make_shared<Timer>(ms, cb, repeat, this);

  RWMutexType::WriteLock lock(_mutex);
  // 传lock进去是为了提前释放_mutex，减少加锁时间
  add_timer(timer, lock);
  return timer;
}

// private方法
void TimerManager::add_timer(Timer::Ptr timer, RWMutexType::WriteLock &lock) {
  // 插入Timer
  auto it = _timers.insert(timer).first;  // .first是指向插入的元素的迭代器
  bool at_front = (it == _timers.begin()) && !_tickled;

  // 这时候已经可以解锁了
  lock.unlock();

  if (at_front) {
    _tickled = true;
    // 触发回调
    on_timer_inserted_front();
  }
}

}  // namespace fleet