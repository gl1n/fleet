#pragma once

#include <list>

namespace fleet {
template <typename T>
class List : public std::list<T> {
 public:
  //构造函数完美转发给基类
  template <typename... Args>
  List(Args &&...args) : std::list<T>(std::forward<Args>(args)...){};

  ~List() = default;

  //将other内的元素加入到this尾部，并对other清空
  void append(List<T> &other) {
    if (other.empty()) {
      return;
    }
    this->insert(this->end(), other.begin(), other.end());
    other.clear();
  }

  //对List中保存的所有内容执行func函数
  template <typename FUNC>
  void for_each(FUNC &&func) {
    for (auto &t : *this) {
      func(t);
    }
  }

  // const版本
  template <typename FUNC>
  void for_each(FUNC &&func) const {
    for (auto &it : *this) {
      func(it);
    }
  }
};
}  // namespace fleet