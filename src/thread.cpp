#include <bits/types/locale_t.h>
#include <pthread.h>
#include <functional>
#include <stdexcept>
#include <utility>

#include "thread.h"
#include "log.h"
#include "utils.h"

namespace fleet {

// 每一个系统线程都会保存一份独立的thread_local对象
static thread_local Thread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

Thread *Thread::s_get_this() { return t_thread; }

const std::string &Thread::s_get_name() { return t_thread_name; }

void Thread::s_set_name(const std::string &name) {
  if (name.empty()) {
    return;
  }
  if (t_thread) {
    t_thread->_name = name;
  }

  t_thread_name = name;
}

Thread::Thread(CB &&cb, const std::string &name) : _cb(std::move(cb)), _name(name) {
  if (_name.empty()) {
    _name = "UNKNOWN";
  }
  // 创建线程，线程句柄保存在_thread中
  int ret = pthread_create(&_thread, nullptr, &Thread::run, this);

  if (ret != 0) {
    ErrorL << "pthread_create failed, ret = " << ret << " name = " << _name;
    throw std::runtime_error("pthread_create error");
  }

  // 等待线程执行
  _semaphore.wait();
}

Thread::~Thread() {
  // Thread对象声明周期结束时应该detach
  if (_thread) {
    pthread_detach(_thread);
  }
}

void Thread::join() {
  if (_thread) {
    int ret = pthread_join(_thread, nullptr);
    if (ret != 0) {
      ErrorL << "pthread_join failed, ret = " << ret << " name = " << _name;
      throw std::runtime_error("pthread_join error");
    }
  }
  // 不能重新join
  _thread = 0;
}

void *Thread::run(void *arg) {
  // this
  Thread *thread = static_cast<Thread *>(arg);
  t_thread = thread;
  t_thread_name = thread->_name;
  // 获取线程标识
  thread->_pid = get_thread_id();
  // 通过系统调用设置线程名，方便调试
  pthread_setname_np(thread->_thread, thread->_name.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->_cb);

  // 通知主线程新线程已经开始执行
  thread->_semaphore.post();
  cb();
  return 0;
}
}  // namespace fleet