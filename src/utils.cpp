#include <cxxabi.h>
#include <execinfo.h>  //for backtrace/backtrace_symbols
#include <pthread.h>
#include <unistd.h>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "log.h"
#include "utils.h"

namespace fleet {

std::string get_error_string() { return std::string(std::strerror(errno)); }

pid_t get_thread_id() { return gettid(); }

std::string get_thread_name() {
  char thread_name[16] = {0};
  pthread_getname_np(pthread_self(), thread_name, 16);

  return thread_name;
}

static void back_trace(int size, int skip, std::vector<std::string> &bt) {
  void **buffer = (void **)malloc(sizeof(void *) * size);
  auto num = backtrace(buffer, size);
  auto raw = backtrace_symbols(buffer, num);
  if (raw == NULL) {
    ErrorL << "backtrace error";
    return;
  }
  for (auto i = skip; i < num; i++) {
    bt.push_back(raw[i]);
  }
  free(buffer);
  free(raw);
}

std::string backtrace_to_string(int size, int skip, const std::string &prefix) {
  std::vector<std::string> bt;
  back_trace(size, 1, bt);
  std::stringstream ss;
  for (auto const &item : bt) {
    ss << prefix << item;
  }
  return ss.str();
}
uint64_t get_elapsed_ms() {
  struct timespec ts;
  // linux中CLOCK_MONOTONIC_RAW表示可以获取从开机以来的时间，不受NTP影响，不统计系统挂起的时间
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
}  // namespace fleet