#include "IO/iomanager.h"
#include "IO/timer.h"
#include "Utils/log.h"

static int timeout = 1000;
static fleet::Timer::Ptr s_timer;

void test_timer() {}

int main() {
  LOG_DEFAULT;
  fleet::IOManager iom;

  // 周期不断变大的定时器
  fleet::Timer::Ptr s_timer = iom.add_timer(
      1000,
      [&s_timer]() {
        InfoL << "timer callback, timeout = " << timeout;
        timeout += 1000;
        if (timeout < 5000) {
          s_timer->reset(timeout, true);
        } else {
          s_timer->cancel();
        }
      },
      true);
  iom.stop();
}