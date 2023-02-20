#include "Fiber/iomanager.h"
#include "Utils/log.h"
#include "Utils/timer.h"

static int timeout = 1000;

void test_timer() {}

int main() {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  fleet::IOManager iom(2, true);

  // 周期不断变大的定时器
  auto timer = iom.add_timer(
      1000, []() { InfoL << "timer callback, timeout = " << timeout; }, true);
  iom.stop();
}