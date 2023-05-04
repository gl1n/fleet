
#include <unistd.h>
#include <memory>
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
  fleet::Logger::Instance().set_async();
  fleet::Logger::Instance().add_channel(std::make_shared<fleet::FileChannel>());
  fleet::Logger::Instance().add_channel(std::make_shared<fleet::ConsoleChannel>());

  InfoL << "main";
  fleet::Scheduler sc(1, "test");
  InfoL << "Scheduler constructed";
  sc.start();
  sleep(1);
  InfoL << "schedule";
  sc.schedule(test_fiber);
  sc.stop();
  InfoL << "over";
  return 0;
}
