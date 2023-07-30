
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "env.h"
#include "log.h"
#include "mutex.h"
#include "thread.h"
#include "utils.h"

fleet::Mutex s_mutex;
int count = 0;

int main(int argc, char **argv) {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  fleet::Env::Instance().init(argc, argv);
  std::vector<fleet::Thread::Ptr> ths;
  int arg = 123456;

  for (int i = 0; i < 3; i++) {
    auto cb = [arg]() {
      InfoL << "name: " << fleet::Thread::s_get_name();
      InfoL << " this.name " << fleet::Thread::s_get_this()->get_name();
      InfoL << "thread name: " << fleet::get_thread_name();
      InfoL << "id: " << fleet::get_thread_id();
      InfoL << "this.id: " << fleet::Thread::s_get_this()->get_id();

      InfoL << "arg: " << arg;

      for (int i = 0; i < 10000; i++) {
        fleet::Mutex::Lock lock(s_mutex);
        count++;
      }
    };
    auto pth = std::make_shared<fleet::Thread>(cb, "thread_" + std::to_string(i));

    ths.push_back(pth);
  }

  for (int i = 0; i < 3; i++) {
    ths[i]->join();
  }

  InfoL << "count = " << count;
  return 0;
}