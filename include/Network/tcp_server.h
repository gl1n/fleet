#pragma once

#include <netinet/tcp.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "IO/iomanager.h"
#include "Network/address.h"
#include "Network/socket.h"
#include "Utils/uncopyable.h"

namespace fleet {
class TCPServer : public std::enable_shared_from_this<TCPServer>, private Uncopyable {
 public:
  using Ptr = std::shared_ptr<TCPServer>;

  TCPServer(uint64_t recv_timeout, IOManager *io_worker = IOManager::s_get_this(),
            IOManager *_accept_worker = IOManager::s_get_this());

  virtual ~TCPServer();

  virtual bool bind(Address::Ptr addr);

  virtual bool bind(const std::vector<Address::Ptr> &addrs, std::vector<Address::Ptr> &fails);

  virtual void start();

  virtual void stop();

  uint64_t get_recv_timeout() const { return _recv_timeout; }

  std::string const &get_name() const { return _name; }

  void set_recv_timeout(uint64_t t) { _recv_timeout = t; }

  void set_name(const std::string &n) { _name = n; }

  bool is_running() const { return _is_running; }

  std::string to_string(const std::string &prefix = "");

 protected:
  virtual void handle_client(Socket::Ptr client);

  virtual void start_accept(Socket::Ptr sock);

 protected:
  // 可能会同时监听多个Socket
  std::vector<Socket::Ptr> _socks;

  IOManager *_io_worker;

  IOManager *_accept_worker;
  // 接收超时时间
  uint64_t _recv_timeout;
  // 服务器名称
  std::string _name = "fleet/1.0";
  // 服务器类型
  std::string _type = "tcp";
  // 是否在运行
  bool _is_running = false;
};
}  // namespace fleet