#include <cstdlib>
#include "Utils/env.h"
#include "Utils/log.h"

int main(int argc, char **argv) {
  LOG_DEFAULT;
  auto &env = fleet::Env::Instance();
  env.add_help("h", "print this help messagge");
  bool is_print_help = false;
  //解析失败，打印help消息
  if (!env.init(argc, argv)) {
    is_print_help = true;
  }
  //有-h，也打印help
  if (env.has("h")) {
    is_print_help = true;
  }
  if (is_print_help) {
    env.print_help();
    EXIT_SUCCESS;
  }

  InfoL << "exe: " << env.get_exe_full_path();
  InfoL << "cwd: " << env.get_exe_path();
  InfoL << "absulute path of test: " << env.get_absolute_path("test");

  env.add("key1", "value1");
  InfoL << "key1: " << env.get("key1");
  env.add("key2", "value2");
  InfoL << "key2: " << env.get("key2");

  env.set_env("key3", "value3");
  InfoL << "key3: " << env.get_env("key3");

  InfoL << env.get_env("PATH");

  return 0;
}