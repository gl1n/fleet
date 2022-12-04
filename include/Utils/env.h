#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Thread/mutex.h"
#include "Utils/uncopyable.h"

namespace fleet {
class Env {
 public:
  /**
   * @brief 初始化，包括记录程序名称与路径，解析命令行选项和参数
   & @details
   命令行选项全部以-开头，后面跟可选项，选项与参数构成key-value结构，被存储到程序的自定义环境中
   * 如果只有key没有value，那么value为空字符串
   */
  bool init(int argc, char **argv);

  /**
   * @brief 添加自定义环境变量，存储在程序内部的map结构中
   */
  void add(const std::string &key, const std::string &val);

  /**
   * @brief 获取是否存在键值为key的自定义环境变量
   */
  bool has(const std::string &key);

  /**
   * @brief 删除键值为key的自定义环境变量
   */
  void del(const std::string &key);

  /**
   * 获取值为key的自定义环境变量，如果未找到，则返回default_value
   */
  std::string get(const std::string &key, const std::string &default_value = "");

  /**
   * @brief 增加命令行帮助选项
   * @param key 选项名
   * @param desc 选项描述
   */
  void add_help(const std::string &key, const std::string &desc);

  /**
   * @brief 删除命令行帮助选项
   * @param key 选项名
   */
  void del_help(const std::string &key);

  /**
   * @brief 打印帮助信息
   */
  void print_help();

  /**
   * @brief 获取exe完整路径
   */
  const std::string &get_exe_full_path() const { return _exe_full_path; }

  /**
   * @brief 获取exe所在目录
   */
  const std::string &get_exe_path() const { return _exe_path; }

  /**
   * @brief 设置系统环境变量，参考setenv(3)
   * @details 如果环境变量名已存在，则覆盖之
   */
  bool set_env(const std::string &key, const std::string &val);

  /**
   * @brief 获取系统环境变量，参考getenv(3)
   */
  std::string get_env(const std::string &key, const std::string &default_value = "");

  /**
   * @param relative_path 相对当前程序的路径
   * @return relative_path的绝对路径
   */
  std::string get_absolute_path(const std::string &relative_path);

 public:
  // 单例
  static Env &Instance();

 private:
  Env() = default;
  ~Env() = default;

  Env(const Env &) = delete;
  Env &operator=(const Env &) = delete;

 private:
  //读写锁
  RWMutex _mutex;
  //存储自定义环境变量
  std::unordered_map<std::string, std::string> _args;
  // 存储帮助选项与描述
  std::vector<std::pair<std::string, std::string>> _helps;

  // 程序名
  std::string _program;
  // 程序完整路径，也就是/proc/$pid/exe软连接指定的路径
  std::string _exe_full_path;
  // 程序所在文件夹，也就是去掉程序名之后的路径
  std::string _exe_path;
};
}  // namespace fleet