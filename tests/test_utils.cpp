#include "log.h"
#include "macro.h"
#include "utils.h"

void test_backtrace() { InfoL << fleet::backtrace_to_string(64, 2, "\n"); }

int main() {
  LOG_DEFAULT;
  test_backtrace();
  ASSERT(2 == 2);
  ASSERT2(1 == 2, abc);
  return 0;
}