#include "IO/iomanager.h"
#include "IO/timer.h"
#include "Utils/log.h"

static int timeout = 1000;

void test_timer() {}

int main() {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  fleet::IOManager iom(2);

  // 周期不断变大的定时器
  auto timer = iom.add_timer(
      1000, []() { InfoL << "timer callback, timeout = " << timeout; }, true);
  iom.stop();
}