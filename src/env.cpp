
#include "Utils/env.h"
#include <stdlib.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include "Thread/mutex.h"
#include "Utils/log.h"
#include "Utils/utils.h"

namespace fleet {

bool Env::init(int argc, char **argv) {
  char link[1024] = {0};
  char path[1024] = {0};

  sprintf(link, "/proc/%d/exe", getpid());
  int ret = readlink(link, path, sizeof(path));
  if (ret == -1) {
    ErrorL << "readlink error" << get_error_string();
  }
  _exe_full_path = path;

  auto pos = _exe_full_path.find_last_of('/');
  _exe_path = _exe_full_path.substr(0, pos) + '/';

  _program = argv[0];
  //已经解析到的key
  const char *has_key = nullptr;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strlen(argv[i]) > 1) {
        if (has_key) {
          add(has_key, "");
          has_key = argv[i];
        }
      } else {
        ErrorL << "invalid arg index = " << i << " ,val = " << argv[i];
        return false;
      }
    } else {
      if (has_key) {
        add(has_key, argv[i]);
        has_key = nullptr;
      } else {
        ErrorL << "invalid arg index = " << i << " ,val = " << argv[i];
        return false;
      }
    }
  }
  if (has_key) {
    add(has_key, "");
  }
  return true;
}

void Env::add(const std::string &key, const std::string &val) {
  RWMutex::WriteLock lock(_mutex);
  _args[key] = val;
}

bool Env::has(const std::string &key) {
  RWMutex::ReadLock lock(_mutex);
  auto it = _args.find(key);
  return it != _args.end();
}

void Env::del(const std::string &key) {
  RWMutex::WriteLock lock(_mutex);
  _args.erase(key);
}
std::string Env::get(const std::string &key, const std::string &default_value) {
  RWMutex::ReadLock lock(_mutex);
  auto it = _args.find(key);
  if (it != _args.end()) {
    return it->second;
  } else {
    return default_value;
  }
}

void Env::add_help(const std::string &key, const std::string &desc) {
  del_help(key);
  RWMutex::WriteLock lock(_mutex);
  _helps.emplace_back(key, desc);
}

void Env::del_help(const std::string &key) {
  RWMutex::WriteLock lock(_mutex);
  for (auto it = _helps.begin(); it != _helps.end(); it++) {
    if (it->first == key) {
      _helps.erase(it);
      break;  //不会有同名item，所以直接返回
    }
  }
}

void Env::print_help() {
  RWMutex::ReadLock lock(_mutex);
  std::cout << "Usage: " << _program << " [option]" << std::endl;
  for (auto const &item : _helps) {
    std::cout << std::setw(5) << '-' << item.first << " : " << item.second << std::endl;
  }
}

bool Env::set_env(const std::string &key, const std::string &val) { return setenv(key.data(), val.data(), 1) == 0; }

std::string Env::get_env(const std::string &key, const std::string &default_value) {
  auto ret = getenv(key.data());
  if (ret) {
    //直接返回，少一次std::string的构造
    return ret;
  }
  return default_value;
}
std::string Env::get_absolute_path(const std::string &relative_path) {
  if (relative_path.empty()) {
    return "/";
  }
  if (relative_path[0] == '/') {
    return relative_path;
  }
  return _exe_path + relative_path;
}

Env &Env::Instance() {
  static Env s_instance;
  return s_instance;
}

}  // namespace fleet