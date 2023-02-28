#include <unistd.h>
#include "Thread/mutex.h"
#include "Thread/thread.h"
#include "Utils/log.h"

int main() {
  LOG_DEFAULT;

  fleet::RWMutex mutex;

  // {
  //   不同线程中加了写锁之后就不能加读锁了。
  //   fleet::Thread t1(
  //       [&mutex]() {
  //         fleet::RWMutex::WriteLock lock(mutex);
  //         DebugL << "write";
  //         while (true)
  //           ;
  //       },
  //       "t1");

  //   fleet::RWMutex::ReadLock lock(mutex);
  //   DebugL << "read";
  // }

  {
    // 同一个线程中，先加写锁之后还可以加读锁。相当于可重入锁。
    fleet::RWMutex::WriteLock lock(mutex);
    DebugL << "write";

    fleet::RWMutex::ReadLock lock2(mutex);
    DebugL << "read";
  }

  {
    // 即使在同一个线程内，加了读锁之后，再加写锁会堵塞
    fleet::RWMutex::ReadLock lock(mutex);
    DebugL << "read";

    fleet::RWMutex::WriteLock lock2(mutex);
    DebugL << "write";
  }

  return 0;
}