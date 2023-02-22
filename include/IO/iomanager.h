#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "Fiber/scheduler.h"
#include "IO/timer.h"
#include "Thread/mutex.h"

namespace fleet {
class IOManager : public Scheduler, public TimerManager {
 public:
  using Ptr = std::shared_ptr<IOManager>;
  using RWMutexType = RWMutex;
  enum Event {
    NONE = 0x0,  // 无事件
    READ = 0x1,  // 读事件，对应EPOLLIN
    WRITE = 0x4  // 写事件，对应EPOLLOUT
  };

 private:
  struct FdContext {
    using Ptr = std::shared_ptr<FdContext>;
    using MutexType = Mutex;
    struct Task {
      // 事件回调协程
      Fiber::Ptr fiber;
      // 事件回调函数
      std::function<void()> cb;
    };

    // 重置ctx
    void reset_task(Task &task);
    // 处理相应的event
    void trigger_event(Event event);
    // 返回事件对应的任务
    Task &get_task(Event event);

    // 读事件上下文
    Task readCB;
    // 写事件上下文
    Task writeCB;
    // 关注哪些事件
    Event events = Event::NONE;
    FdContext::MutexType mutex;
  };

 public:
  IOManager(size_t threads = 1, bool use_main_thread = true, const std::string &name = "");

  ~IOManager();

  /**
   * @return -1代表失败，0代表成功
   */
  int add_event(int fd, Event event, const std::function<void()> &cb = nullptr);

  // 删除注册的事件
  /**
   * @brief 删除事件
   * @param trigger 是否执行回调
   */
  bool del_event(int fd, Event event, bool trigger_task);

  bool del_and_trigger_all(int fd);

  static IOManager *s_get_this();

 protected:
  // 唤醒idle
  void notify() override;

  bool stopping() override;

  bool stopping(uint64_t &timeout);

  void idle() override;

  void on_timer_inserted_front() override;

 private:
  int _epfd = 0;
  int _notify_fds[2];  // 0是read end, 1是write end
  std::atomic<size_t> _pending_event_count = {0};

  RWMutexType _mutex;
  std::unordered_map<int, FdContext::Ptr> _fd_contexts;
};
}  // namespace fleet