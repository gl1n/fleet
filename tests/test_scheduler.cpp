
#include <unistd.h>
#include "Fiber/scheduler.h"
#include "Utils/log.h"
#include "Utils/utils.h"

void test_fiber() {
  static int s_count = 5;
  InfoL << "test in fiber s_count = " << s_count;

  sleep(1);
  if (--s_count >= 0) {
    fleet::Scheduler::s_get_this()->schedule(test_fiber, fleet::get_thread_id());  // 把相同的任务放到同一个线程中
  }
}

int main() {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();

  InfoL << "main";
  fleet::Scheduler sc(3, false, "test");
  sc.start();
  sleep(2);
  InfoL << "schedule";
  sc.schedule(test_fiber);
  sc.stop();
  InfoL << "over";
  return 0;
}