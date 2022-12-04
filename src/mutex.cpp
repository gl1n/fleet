#include "Thread/mutex.h"
#include <semaphore.h>
#include <sys/types.h>
#include <exception>
#include <stdexcept>

namespace fleet {
Semaphore::Semaphore(u_int32_t count) {
  if (sem_init(&_sem, 0, count)) {
    throw std::runtime_error("sem_init error");
  }
}

Semaphore::~Semaphore() { sem_destroy(&_sem); }

void Semaphore::wait() {
  if (sem_wait(&_sem)) {
    throw std::runtime_error("sem_wait error");
  }
}

void Semaphore::post() {
  if (sem_post(&_sem)) {
    throw std::runtime_error("sem_post error");
  }
}
}  // namespace fleet