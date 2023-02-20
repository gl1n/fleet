#include "Fiber/iomanager.h"
#include "Utils/log.h"
#include "Utils/timer.h"

static int timeout = 1000;

void test_timer() {}

int main() {
  LOG_DEFAULT;
  fleet::IOManager iom;

  // 周期不断变大的定时器
  auto timer = iom.add_timer(
      1000, []() { InfoL << "timer callback, timeout = " << timeout; }, true);
  iom.stop();
}