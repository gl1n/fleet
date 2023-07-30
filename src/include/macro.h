#pragma once

#include <assert.h>

#include "log.h"
#include "utils.h"

#if defined __GNUC__ || defined __llvm__

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif

// 断言封装
#define ASSERT(x)                                                                            \
  if (UNLIKELY(!(x))) {                                                                      \
    ErrorL << "ASSERTION: " #x << "\nbacktrace:" << fleet::backtrace_to_string(64, 2, "\n"); \
    assert(x);                                                                               \
  }

#define ASSERT2(x, arg)                                                 \
  if (UNLIKELY(!(x))) {                                                 \
    ErrorL << "ASSERTION: " #x << "\nbacktrace:\n"                      \
           << "mark: " #arg << fleet::backtrace_to_string(64, 2, "\n"); \
    assert(x);                                                          \
  }