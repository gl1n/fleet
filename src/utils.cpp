#include "Utils/utils.h"
#include <pthread.h>
#include <unistd.h>
#include <cstring>

namespace fleet {

std::string get_error_string() { return std::string(std::strerror(errno)); }

pid_t get_thread_id() { return gettid(); }

std::string get_thread_name() {
  char thread_name[16] = {0};
  pthread_getname_np(pthread_self(), thread_name, 16);

  return thread_name;
}
}  // namespace fleet