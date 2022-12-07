#include "Utils/log.h"
#include "Utils/macro.h"
#include "Utils/utils.h"

void test_backtrace() { InfoL << fleet::backtrace_to_string(64, 2, "\n"); }

int main() {
  LOG_DEFAULT;
  test_backtrace();
  ASSERT(2 == 2);
  ASSERT(1 == 2);
  return 0;
}