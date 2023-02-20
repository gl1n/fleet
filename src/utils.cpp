#include <bits/chrono.h>
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

#include "Utils/log.h"
#include "Utils/utils.h"

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
uint64_t time_since_epoch_millisecs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}
}  // namespace fleet