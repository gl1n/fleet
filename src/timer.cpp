#include "Utils/timer.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "Utils/utils.h"

namespace fleet {
Timer::Timer(uint64_t period, std::function<void()> cb, bool repeat, TimerManager *manager)
    : _repeat(repeat), _period(period), _cb(cb), _manager(manager) {
  _next = _period + time_since_epoch_millisecs();
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
  _next = _period + time_since_epoch_millisecs();
  _manager->_timers.insert(shared_from_this());
  return true;
}

bool Timer::reset(uint64_t period, bool from_now) {
  if (period == _period && !from_now) {
    // 不需要处理
    return true;
  }

  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (!_cb) {
    // 没有cb也不用处理
    return false;
  }
  auto it = _manager->_timers.find(shared_from_this());
  if (it == _manager->_timers.end()) {
    // 找不到对应的Timer，直接return
    return false;
  }

  _manager->_timers.erase(it);
  // 修改周期
  if (from_now) {
    // from_now 会修改起始时间
    _period = period;
    _next = time_since_epoch_millisecs() + _period;
  } else {
    // !from_now则不会
    _next += period - _period;
    _period = period;
  }
  // 重新放回去
  _manager->add_timer(shared_from_this(), lock);
  return true;
}

TimerManager::TimerManager() {}

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

Timer::Ptr TimerManager::add_condition_timer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void()> weak_cond,
                                             bool repeat) {
  return add_timer(
      ms,
      [cb, weak_cond]() {
        auto weak_ptr = weak_cond.lock();
        if (weak_ptr) {
          cb();
        }
      },
      repeat);
}

uint64_t TimerManager::get_next_timer() {
  RWMutexType::ReadLock lock(_mutex);
  _tickled = false;
  if (_timers.empty()) {
    return UINT64_MAX;
  }

  const Timer::Ptr &next = *_timers.begin();
  auto now_ms = time_since_epoch_millisecs();
  if (now_ms >= next->_next) {
    return 0;  // 有定时器到期了
  } else {
    return next->_next - now_ms;  // 返回最早的到期时间
  }
}

std::vector<std::function<void()>> TimerManager::list_expired_cb() {
  auto now_ms = time_since_epoch_millisecs();
  RWMutexType::WriteLock lock(_mutex);
  // 没有Timer
  if (_timers.empty()) {
    return {};
  }
  // 没有超时的
  if ((*_timers.begin())->_next > now_ms) {
    return {};
  }

  Timer::Ptr now_timer = std::make_shared<Timer>(now_ms);
  // 获取指向第一个大于等于now_timer的迭代器
  auto first_greater = _timers.lower_bound(now_timer);
  // 获取指向第一个大于now_timer的迭代器
  while (first_greater != _timers.end() && (*first_greater)->_next == now_ms) {
    first_greater++;
  }

  std::vector<std::function<void()>> cbs;
  for (auto it = _timers.begin(); it != first_greater;) {
    auto timer = (*it);
    // 将cb放入cbs
    cbs.push_back(timer->_cb);
    // 删除过期Timer
    it = _timers.erase(it);  // 返回值是被删除的下一个
    if ((*it)->_repeat) {
      // 将更新后的Timer重新插入
      timer->_next = now_ms + timer->_period;
      _timers.insert(timer);
    }
  }

  return cbs;
}

bool TimerManager::has_timer() {
  RWMutexType::ReadLock lock(_mutex);
  return !_timers.empty();
}
}  // namespace fleet