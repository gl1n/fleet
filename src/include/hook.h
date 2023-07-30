#pragma once

#include <stdint.h>
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

/****sleep****/
// 定义函数签名为unsigned int(unsignned int seconds)的函数指针类型
// extern的作用是声明一个类型为sleep_type的变量但不对其定义。
// sleep_p是个指针sleep_type，未定义(在对应的cpp文件中定义，因为头文件中不能进行定义，除非是类内的成员)，
typedef unsigned int (*sleep_type)(unsigned int seconds);
extern sleep_type sleep_p;

typedef int (*usleep_type)(useconds_t usec);
extern usleep_type usleep_p;

typedef int (*nanosleep_type)(const struct timespec *req, struct timespec *rem);
extern nanosleep_type nanosleep_p;

/****socket****/
typedef int (*socket_type)(int domain, int type, int protocol);
extern socket_type socket_p;

typedef int (*connect_type)(int socket, const struct sockaddr *address, socklen_t address_len);
extern connect_type connect_p;

typedef int (*accept_type)(int socket, struct sockaddr *address, socklen_t *address_len);
extern accept_type accept_p;

/****read****/
typedef ssize_t (*read_type)(int fd, void *buf, size_t count);
extern read_type read_p;

typedef ssize_t (*readv_type)(int fd, const struct iovec *iov, int iovcnt);
extern readv_type readv_p;

typedef ssize_t (*recv_type)(int sockfd, void *buf, size_t len, int flags);
extern recv_type recv_p;

typedef ssize_t (*recvfrom_type)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr,
                                 socklen_t *addrlen);
extern recvfrom_type recvfrom_p;

typedef ssize_t (*recvmsg_type)(int sockfd, struct msghdr *msg, int flags);
extern recvmsg_type recvmsg_p;

/****write****/
typedef ssize_t (*write_type)(int fd, const void *buffer, size_t count);
extern write_type write_p;

typedef ssize_t (*writev_type)(int fd, const struct iovec *iov, int iovcnt);
extern writev_type writev_p;

typedef ssize_t (*send_type)(int socket, const void *msg, size_t len, int flags);
extern send_type send_p;

typedef ssize_t (*sendto_type)(int socket, const void *msg, size_t len, int flags, const struct sockaddr *to,
                               socklen_t tolen);
extern sendto_type sendto_p;

typedef ssize_t (*sendmsg_type)(int socket, const struct msghdr *msg, int flags);
extern sendmsg_type sendmsg_p;

typedef int (*close_type)(int fd);
extern close_type close_p;

//
typedef int (*fcntl_type)(int fd, int cmd, ...);
extern fcntl_type fcntl_p;

// 不用这个了
typedef int (*ioctl_type)(int fd, int request, ... /* arg */);
extern ioctl_type ioctl_p;

typedef int (*getsockopt_type)(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
extern getsockopt_type getsockopt_p;

typedef int (*setsockopt_type)(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
extern setsockopt_type setsockopt_p;

typedef int (*pipe_type)(int fd[2]);
extern pipe_type pipe_p;

// 编译期（链接前）需要知道此connnect_with_timeout的声明
// 而其他函数如write、read的声明在各自原头文件中，只要include未hook前的头文件即可，故本文件无需再对write、read做声明
int connect_with_timeout(int socket, const struct sockaddr *address, socklen_t address_len, uint64_t timeout_ms);
}