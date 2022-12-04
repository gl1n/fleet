#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include "Utils/uncopyable.h"

namespace fleet {
/**
 * @brief 信号量
 */
class Semaphore : private Uncopyable {
 public:
  /**
   * @brief 构造函数
   * @param count 信号值的大小
   */
  Semaphore(u_int32_t count = 0);

  /**
   * @brief 析构函数
   */
  ~Semaphore();

  void wait();

  void post();

 private:
  sem_t _sem;
};

template <class T>
class ScopedLockImpl {
 public:
  ScopedLockImpl(T &mutex) : _mutex(mutex) { _mutex.lock(); }
  // RAII 析构时解锁
  ~ScopedLockImpl() { _mutex.unlock(); }

 private:
  T &_mutex;
};

template <class T>
class ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T &mutex) : _mutex(mutex) { _mutex.rdlock(); }
  ~ReadScopedLockImpl() { _mutex.unlock(); }

 private:
  T &_mutex;
};

template <class T>
class WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T &mutex) : _mutex(mutex) { _mutex.wrlock(); }
  ~WriteScopedLockImpl() { _mutex.unlock(); }

 private:
  T &_mutex;
};

/**
 * @brief 互斥量
 */
class Mutex : private Uncopyable {
 public:
  // lockguard
  using Lock = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&_mutex, nullptr); }

  ~Mutex() { pthread_mutex_destroy(&_mutex); }

  void lock() { pthread_mutex_lock(&_mutex); }

  void unlock() { pthread_mutex_unlock(&_mutex); }

 private:
  // posix mutex
  pthread_mutex_t _mutex;
};

/**
 * @brief 读写互斥量
 */
class RWMutex : private Uncopyable {
 public:
  using ReadLock = ReadScopedLockImpl<RWMutex>;
  using WriteLock = WriteScopedLockImpl<RWMutex>;

  RWMutex() { pthread_rwlock_init(&_mutex, nullptr); }

  ~RWMutex() { pthread_rwlock_destroy(&_mutex); }

  void rdlock() { pthread_rwlock_rdlock(&_mutex); }

  void wrlock() { pthread_rwlock_wrlock(&_mutex); }

  void unlock() { pthread_rwlock_unlock(&_mutex); }

 private:
  pthread_rwlock_t _mutex;
};

class SpinLock : private Uncopyable {
 public:
  using Lock = ScopedLockImpl<SpinLock>;

  SpinLock() { pthread_spin_init(&_mutex, 0); }

  ~SpinLock() { pthread_spin_destroy(&_mutex); }

  void lock() { pthread_spin_lock(&_mutex); }

  void unlock() { pthread_spin_unlock(&_mutex); }

 private:
  //系统提供的自旋锁
  pthread_spinlock_t _mutex;
};

}  // namespace fleet