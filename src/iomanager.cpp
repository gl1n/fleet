#include <error.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Fiber/fiber.h"
#include "Fiber/scheduler.h"
#include "IO/fd_manager.h"
#include "IO/hook.h"
#include "IO/iomanager.h"
#include "Utils/log.h"
#include "Utils/macro.h"

namespace fleet {

IOManager::IOManager(size_t threads, bool use_main_thread, const std::string &name)
    : Scheduler(threads, use_main_thread, name) {
  _epfd = epoll_create(1000);
  ASSERT(_epfd > 0);

  set_hook_enable(true);  // 主线程要早点开，让后面的pipe hook

  int ret = pipe(_notify_fds);
  ASSERT(ret == 0);

  epoll_event epev;
  epev.events = EPOLLIN | EPOLLET;  // 监听读事件，边缘触发
  epev.data.fd = _notify_fds[0];    // 让epoll监听管道的可读事件

  // 设置成非阻塞
  ret = fcntl(_notify_fds[0], F_SETFL, O_NONBLOCK);
  ASSERT(ret != -1);

  // 加入监听
  ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, _notify_fds[0], &epev);
  ASSERT(ret == 0);

  start();  // Scheduler继承来的方法，开辟线程池处理任务队列
}

IOManager::~IOManager() {
  stop();
  close(_epfd);
  close(_notify_fds[0]);
  close(_notify_fds[1]);
}
int IOManager::add_event(int fd, Event event, const std::function<void()> &cb) {
  // 判断是否fd有对应的FdContext
  _mutex.wrlock();
  auto it = _fd_contexts.find(fd);
  if (it == _fd_contexts.end()) {
    _fd_contexts[fd] = std::make_shared<FdTask>();
  }
  _mutex.unlock();

  FdTask::Ptr fd_ctx = _fd_contexts[fd];
  FdTask::MutexType::Lock lock(fd_ctx->mutex);
  // 不能重复加入相同的事件
  if (UNLIKELY(fd_ctx->events & event)) {
    ErrorL << "事件重复! "
           << "fd=" << fd << " fd_ctx->events=" << fd_ctx->events << " event=" << event;
    ASSERT(!(fd_ctx->events & event));
  }
  // 判断op
  int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  epoll_event epev;
  epev.events = EPOLLET | fd_ctx->events | event;
  epev.data.fd = fd;
  int rt = epoll_ctl(_epfd, op, fd, &epev);
  if (rt) {
    ErrorL << "epoll_ctl(" << _epfd << ", " << op << ", " << fd << ", " << static_cast<int>(epev.events) << "): " << rt
           << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events=" << fd_ctx->events;
    return -1;
  }

  ++_pending_event_count;  // 带触发的IO事件数+1

  // 更新
  fd_ctx->events = static_cast<Event>(fd_ctx->events | event);

  FdTask::Task &task = fd_ctx->get_task(event);
  if (cb) {
    // 有cb则回调是cb
    task.cb = cb;
  } else {
    // 没有cb则回调是此协程(yield_to_hold之后等待IO事件重新执行？)
    task.fiber = Fiber::s_get_this();
    ASSERT(task.fiber->get_state() == Fiber::RUNNING);  // 当前的状态应该是RUNNING
  }
  return 0;
}

bool IOManager::del_event(int fd, Event event, bool trigger_task) {
  {
    RWMutexType::ReadLock lock(_mutex);
    auto it = _fd_contexts.find(fd);
    if (it == _fd_contexts.end()) {
      // 不存在该fd对应的事件
      return false;
    }
  }
  // 存在
  FdTask::Ptr fd_ctx = _fd_contexts[fd];

  FdTask::MutexType::Lock lock(fd_ctx->mutex);
  // fd没有对应的事件
  if (UNLIKELY(!(event & fd_ctx->events))) {
    return false;
  }
  // 清除event
  Event new_events = static_cast<Event>(fd_ctx->events & ~event);
  // 判断op
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epev;
  epev.events = EPOLLET | new_events;

  int rt = epoll_ctl(_epfd, op, fd, &epev);
  if (rt) {
    ErrorL << "epoll_ctl(" << _epfd << ", " << op << ", " << fd << ", " << static_cast<int>(epev.events) << "): " << rt
           << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events=" << fd_ctx->events;
    return false;
  }

  if (!trigger_task) {
    // 更新
    fd_ctx->events = new_events;
    auto &task = fd_ctx->get_task(event);
    fd_ctx->reset_task(task);
  } else {
    // trigger_event里不仅更新task，还将回调放入Scheduler中
    fd_ctx->trigger_event(event);
  }
  if (!new_events) {
    RWMutexType::WriteLock lock(_mutex);
    _fd_contexts.erase(fd);
  }
  // 待触发事件-1
  --_pending_event_count;

  return true;
}
bool IOManager::del_and_trigger_all(int fd) {
  {
    RWMutexType::ReadLock lock(_mutex);
    auto it = _fd_contexts.find(fd);
    if (it == _fd_contexts.end()) {
      // 不存在该fd对应的事件
      return false;
    }
  }
  // 存在
  FdTask::Ptr fd_ctx = _fd_contexts[fd];

  FdTask::MutexType::Lock lock(fd_ctx->mutex);
  // 没有任何事件
  if (!fd_ctx->events) {
    return false;
  }

  // 删除全部事件
  int op = EPOLL_CTL_DEL;
  epoll_event epev;
  epev.events = 0;
  int rt = epoll_ctl(_epfd, op, fd, &epev);
  if (rt) {
    ErrorL << "epoll_ctl(" << _epfd << ", " << op << ", " << fd << ", " << static_cast<int>(epev.events) << "): " << rt
           << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events=" << fd_ctx->events;
    return false;
  }

  // 触发全部已注册事件
  if (fd_ctx->events & Event::READ) {
    fd_ctx->trigger_event(Event::READ);
    --_pending_event_count;
  }
  if (fd_ctx->events & Event::WRITE) {
    fd_ctx->trigger_event(Event::WRITE);
    --_pending_event_count;
  }
  {
    RWMutexType::WriteLock lock(_mutex);
    _fd_contexts.erase(fd);
  }

  ASSERT(fd_ctx->events == 0);
  return true;
}
IOManager *IOManager::s_get_this() { return dynamic_cast<IOManager *>(Scheduler::s_get_this()); }

void IOManager::FdTask::trigger_event(Event event) {
  ASSERT(this->events & event);
  this->events = static_cast<Event>(events & ~event);
  Task &task = get_task(event);
  if (task.cb) {
    Scheduler::s_get_this()->schedule(task.cb);
  } else {
    Scheduler::s_get_this()->schedule(task.fiber);
  }
  reset_task(task);
}

IOManager::FdTask::Task &IOManager::FdTask::get_task(Event event) {
  switch (event) {
    case IOManager::READ:
      return readCB;
    case IOManager::WRITE:
      return writeCB;
    default:
      ASSERT2(false, "get_task");
  }
  throw std::invalid_argument("get_task invalid event");
}
void IOManager::FdTask::reset_task(Task &task) {
  task.cb = nullptr;
  task.fiber = nullptr;
}

void IOManager::notify() {
  DebugL << "notify";
  int rt = ::write(_notify_fds[1], "1", 1);
  ASSERT(rt == 1);
}
bool IOManager::stopping() {
  uint64_t timeout = 0;
  return stopping(timeout);
}

bool IOManager::stopping(uint64_t &timeout) {
  timeout = get_next_timer();
  return timeout == UINT64_MAX && _pending_event_count == 0 && Scheduler::stopping();
}

void IOManager::idle() {
  DebugL << "idle";

  constexpr uint64_t MAX_EVENTS = 256;

  epoll_event events[MAX_EVENTS];

  while (true) {
    // 获取下一个定时器的超时时间，顺便判断调度器是否停止
    uint64_t next_timeout = 0;
    if (UNLIKELY(stopping(next_timeout))) {
      DebugL << "name = " << get_name() << "idle stopping exit";
      break;
    }
    // 阻塞在epoll_wait上，等待事件发生
    constexpr uint64_t MAX_TIMEOUT = 5000;
    int ret = 0;
    while (true) {
      if (next_timeout == UINT64_MAX) {
        // 没有定时器了
        next_timeout = MAX_TIMEOUT;
      } else {
        // 有定时器
        next_timeout = std::min(next_timeout, MAX_TIMEOUT);
      }
      ret = epoll_wait(_epfd, events, MAX_EVENTS, next_timeout);
      if (ret < 0 && errno == EINTR) {
        // 被中断
        continue;
      } else {
        // 超时
        break;
      }
    }

    // 收集所有已超时的定时器，执行回调
    auto cbs = list_expired_cb();
    for (auto const &cb : cbs) {
      schedule(cb);
    }

    // 遍历所有发生的事件
    for (int i = 0; i < ret; i++) {
      epoll_event &epev = events[i];
      if (epev.data.fd == _notify_fds[0]) {
        // 通知事件
        static char dump[16];
        int err = 0;
        do {
          if (read(_notify_fds[0], dump, sizeof(dump)) > 0) {
            continue;
          }
          err = errno;
        } while (err != EAGAIN);
      } else {
        auto fd_ctx = _fd_contexts[epev.data.fd];
        FdTask::MutexType::Lock lock(fd_ctx->mutex);

        // 出现错误要触发读写事件
        if (epev.events & (EPOLLERR | EPOLLHUP)) {
          epev.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;  // 触发fd_ctx的读写事件（如果有的话）
        }
        // 将epev的events转成Event
        int real_events = Event::NONE;
        if (epev.events & EPOLLIN) {
          real_events |= Event::READ;
        }
        if (epev.events & EPOLLOUT) {
          real_events |= Event::WRITE;
        }

        if ((fd_ctx->events & real_events) == Event::NONE) {
          // 关注的事件和发生的事件没有交集
          continue;
        }

        // 删除已经发生，重新注册未发生的事件
        int left_events = (fd_ctx->events & ~real_events);
        int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epev.events = EPOLLET | left_events;

        int ret2 = epoll_ctl(_epfd, op, epev.data.fd, &epev);
        if (ret2) {
          ErrorL << "epoll_ctl(" << _epfd << ", " << op << ", " << static_cast<int>(epev.data.fd) << ", "
                 << static_cast<int>(epev.events) << "): " << ret2 << " (" << errno << ") (" << strerror(errno)
                 << ") fd_ctx->events=" << fd_ctx->events;
          continue;
        }

        if (real_events & Event::READ) {
          fd_ctx->trigger_event(Event::READ);
          --_pending_event_count;
        }
        if (real_events & Event::WRITE) {
          fd_ctx->trigger_event(Event::WRITE);
          --_pending_event_count;
        }
      }
    }
    // idle协程yield，让其他任务能够执行
    Fiber::yield_to_hold();
  }
}
void IOManager::on_timer_inserted_front() {
  // 有比之前更快的定时器出现，需要重新epoll_wait()
  notify();
}

}  // namespace fleet