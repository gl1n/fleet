#pragma once

#include <pthread.h>
#include <sched.h>
#include <functional>
#include <memory>
#include <string>

#include "Thread/mutex.h"
#include "Utils/uncopyable.h"

namespace fleet {
class Thread : private Uncopyable {
 public:
  using Ptr = std::shared_ptr<Thread>;
  using CB = std::function<void()>;

  Thread(CB &&cb, const std::string &name);

  ~Thread();

  /**
   * @brief 获取线程标识
   */
  pid_t get_id() const { return _pid; }

  /**
   * @brief 获取线程名称
   */
  const std::string &get_name() const { return _name; }

  /**
   * @brief 等待线程执行完成
   */
  void join();

  /**
   * @brief 获取当前的线程指针
   * @details
   如果不是由Thread类创建的线程调用，则返回nullptrr；如果Thread对象已析构，则返回的指针无效
   */
  static Thread *s_get_this();

  /**
   * @brief 获取当前的线程名称。比get_name方法更通用
   */
  static const std::string &s_get_name();

  /**
   * @brief 设置当前线程名称
   */
  static void s_set_name(const std::string &name);

 private:
  // static是为了能够传给pthread_create
  static void *run(void *arg);

 private:
  // 线程标识
  pid_t _pid = -1;
  // 线程操作句柄
  pthread_t _thread = 0;
  // 线程执行函数
  std::function<void()> _cb;
  // 线程名称
  std::string _name;
  // 信号量
  Semaphore _semaphore;
};
}  // namespace fleet