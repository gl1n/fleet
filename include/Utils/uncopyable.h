#pragma once

namespace fleet {

/**
 * @brief 不可拷贝基类 It is intended to be used as a private base.
 */
class Uncopyable {
 public:
  Uncopyable() = default;
  ~Uncopyable() = default;

  Uncopyable(const Uncopyable &) = delete;
  Uncopyable &operator=(const Uncopyable &) = delete;

  //传入右值时
  //在g++中，无论是否已经delete掉移动构造函数，都会调用拷贝构造函数
  //在clang中，如果没有显式处理移动构造函数，则会调用拷贝构造；如果定义了移动构造函数，由于移动构造函数优先级高于拷贝构造函数，所以会调用移动构造函数；如果delete了移动构造函数，那么编译不通过
};
}  // namespace fleet