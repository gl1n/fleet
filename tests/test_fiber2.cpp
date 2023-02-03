#include <memory>
#include <vector>

#include "Fiber/fiber.h"
#include "Thread/thread.h"
#include "Utils/log.h"
#include "Utils/utils.h"

int main(int argc, char **argv) {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  InfoL << "main begin";

  auto fiber = std::make_shared<fleet::Fiber>([]() {
    DebugL << "fiber begin";
    fleet::Fiber::yield_to_hold();
    DebugL << "fiber end";
  });
  InfoL << "main after fiber created";
  fiber->call();
  InfoL << "main after swap_in";
  fiber->call();
  InfoL << "main after end";

  return 0;
}