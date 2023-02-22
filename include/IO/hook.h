#pragma once

#include <sys/socket.h>
#include <sys/types.h>

namespace fleet {
// 获取线程的hook状态
bool is_hook_enable();
// 调整线程hook的状态
void set_hook_enable(bool flag);
}  // namespace fleet

// C++由于需要支持重载，所以在编译时会将函数名mangle混淆。
// extern "C" 的作用是让函数以C的方式编译，防止函数名被mangle
extern "C" {

/****sleep ****/
// 定义函数签名为unsigned int(unsignned int seconds)的函数指针类型
// extern的作用是声明一个类型为sleep_fun的变量但不对其定义。
// sleep_f是个指针sleep_fun，未定义(在对应的cpp文件中定义，因为头文件中不能进行定义，除非是类内的成员)，
typedef unsigned int (*sleep_type)(unsigned int seconds);
extern sleep_type sleep_p;

typedef int (*usleep_type)(useconds_t usec);
extern usleep_type usleep_p;

typedef int (*nanosleep_type)(const struct timespec *req, struct timespec *rem);
extern nanosleep_type nanosleep_p;

typedef int (*socket_type)(int domain, int type, int protocol);
extern socket_type socket_p;

typedef int (*connect_type)(int socket, const struct sockaddr *address, socklen_t address_len);
extern connect_type connect_p;

typedef int (*accept_type)(int socket, struct sockaddr *address, socklen_t *address_len);
extern accept_type accept_p;
}
