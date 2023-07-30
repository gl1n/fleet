#include <semaphore.h>
#include <exception>
#include <stdexcept>

#include "mutex.h"

namespace fleet {
Semaphore::Semaphore(uint32_t count) {
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