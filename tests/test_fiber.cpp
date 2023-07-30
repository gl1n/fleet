#include <memory>
#include <vector>

#include "fiber.h"
#include "log.h"
#include "thread.h"
#include "utils.h"

int main(int argc, char **argv) {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  InfoL << "main begin";

  std::vector<fleet::Thread::Ptr> ths;
  for (int i = 0; i < 3; i++) {
    auto th = std::make_shared<fleet::Thread>(
        []() {
          auto fiber = std::make_shared<fleet::Fiber>([]() {
            DebugL << "fiber begin";
            fleet::Fiber::yield_to_hold();
            DebugL << "fiber end";
          });
          InfoL << "original thread after fiber created";
          fiber->enter();
          InfoL << "original thread after fiber entered";
          fiber->enter();
          InfoL << "original after fiber ended";
        },
        "");
    ths.push_back(th);
  }

  for (auto &t : ths) {
    t->join();
  }

  return 0;
}