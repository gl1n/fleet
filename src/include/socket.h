#pragma once

#include <sys/socket.h>
#include <memory>

#include "address.h"
#include "uncopyable.h"

namespace fleet {
class Socket : public std::enable_shared_from_this<Socket>, Uncopyable {
 public:
  using Ptr = std::shared_ptr<Socket>;

  enum Type {
    /// TCP类型
    TCP = SOCK_STREAM,
    /// UDP类型
    UDP = SOCK_DGRAM
  };

  enum Family {
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
    UNIX = AF_UNIX,
  };

  static Socket::Ptr create_TCP_Socket(int family);

  static Socket::Ptr create_UDP_Socket(int family);

  static Socket::Ptr create_TCP_Socket4();

  static Socket::Ptr create_UDP_Socket4();

  static Socket::Ptr create_TCP_Socket6();

  static Socket::Ptr create_UDP_Socket6();

  static Socket::Ptr create_Unix_TCP_Socket();

  static Socket::Ptr create_Unix_UDP_Socket();

  Socket(int family, int type, int protocol = 0);

  virtual ~Socket();

  int64_t get_send_timeout();

  void set_send_timeout(int64_t v);

  int64_t get_recv_timeout();

  void set_recv_timeout(int64_t v);

  bool get_option(int level, int option, void *result, socklen_t *len);

  template <class T>
  bool get_option(int level, int option, T &result) {
    socklen_t length = sizeof(T);
    return get_option(level, option, &result, &length);
  }

  bool set_option(int level, int option, const void *result, socklen_t len);

  template <class T>
  bool set_option(int level, int option, const T &value) {
    return set_option(level, option, &value, sizeof(T));
  }

  virtual Socket::Ptr accept();

  virtual bool bind(const Address::Ptr addr);

  virtual bool connect(const Address::Ptr addr, uint64_t timeout_ms = -1);

  virtual bool reconnect(uint64_t timeout_ms = -1);

  virtual bool listen(int backlog = SOMAXCONN);

  virtual bool close();

  virtual int send(const void *buffer, size_t length, int flags = 0);

  virtual int send(const iovec *buffers, size_t length, int flags = 0);

  virtual int send_to(const void *buffer, size_t length, const Address::Ptr to, int flags = 0);

  virtual int send_to(const iovec *buffers, size_t length, const Address::Ptr to, int flags = 0);

  virtual int recv(void *buffer, size_t length, int flags = 0);

  virtual int recv(iovec *buffers, size_t length, int flags = 0);

  virtual int recv_from(void *buffer, size_t length, Address::Ptr from, int flags = 0);

  virtual int recv_from(iovec *buffers, size_t length, Address::Ptr from, int flags = 0);

  Address::Ptr get_remote_Address();

  Address::Ptr get_local_Address();

  int get_family() const { return _family; }

  int get_type() const { return _type; }

  int get_protocol() const { return _protocol; }

  bool is_connected() const { return _is_connected; }

  bool is_valid() const;

  int get_error();

  virtual std::ostream &dump(std::ostream &os) const;

  virtual std::string to_string() const;

  int get_socket() const { return _sock; }

  bool cancel_read();

  bool cancel_write();

  bool cancel_accept();

  bool cancel_all();

 protected:
  void init_Socket();

  void new_Socket();

  virtual bool init(int sock);

 protected:
  int _sock;
  int _family;
  int _type;
  int _protocol;
  bool _is_connected;
  Address::Ptr _local_Address;
  Address::Ptr _remote_Address;
};

// 对<<重载
std::ostream &operator<<(std::ostream &os, const Socket &sock);
}  // namespace fleet