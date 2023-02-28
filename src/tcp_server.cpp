#include "Network/tcp_server.h"
#include <cerrno>
#include <cstring>
#include <sstream>
#include <vector>
#include "Network/socket.h"
#include "Utils/log.h"

namespace fleet {
TCPServer::TCPServer(uint64_t recv_timeout, IOManager *io_worker, IOManager *accept_worker)
    : _io_worker(io_worker), _accept_worker(accept_worker), _recv_timeout(recv_timeout) {}

TCPServer::~TCPServer() { stop(); }

bool TCPServer::bind(Address::Ptr addr) {
  std::vector<Address::Ptr> addrs;
  std::vector<Address::Ptr> fails;
  addrs.push_back(addr);
  return this->bind(addrs, fails);
}

bool TCPServer::bind(const std::vector<Address::Ptr> &addrs, std::vector<Address::Ptr> &fails) {
  for (auto &addr : addrs) {
    // 根据Address创建Socket
    auto sock = Socket::create_TCP_Socket(addr->get_family());
    if (!sock->bind(addr)) {
      ErrorL << "bind fails errno = " << errno << " errstr = " << strerror(errno) << " addr = [" << addr->to_string()
             << "]";
      fails.push_back(addr);
      continue;
    }
    if (!sock->listen()) {
      ErrorL << "listen fails errno = " << errno << " errstr = " << strerror(errno) << " addr = [" << addr->to_string()
             << "]";
      fails.push_back(addr);
      continue;
    }
    _socks.push_back(sock);
  }

  if (!fails.empty()) {
    return false;
  }

  for (auto &sock : _socks) {
    InfoL << "type = " << _type << " name " << _name << " server bind success: " << *sock;  // <<被重载了
  }

  return true;
}
void TCPServer::start() {
  if (_is_running) {
    // 已经处于运行状态了
  }
  _is_running = true;  // 设置成非停止状态
  for (auto &sock : _socks) {
    _accept_worker->schedule(std::bind(&TCPServer::start_accept, shared_from_this(), sock));
  }
}
void TCPServer::stop() {
  _is_running = false;
  auto self = shared_from_this();
  _accept_worker->schedule([this, self]() {
    // lambda表达式复制了一份self，所以在本lambda结束前this不会析构
    for (auto &sock : _socks) {
      sock->cancel_all();
    }
  });
}

// 继承重写相关业务
void TCPServer::handle_client(Socket::Ptr client) { InfoL << "handle_client: " << *client; }

void TCPServer::start_accept(Socket::Ptr sock) {
  while (_is_running) {
    auto client = sock->accept();
    if (client) {
      client->set_recv_timeout(_recv_timeout);
      _io_worker->schedule(std::bind(&TCPServer::handle_client, shared_from_this(), client));
    } else {
      ErrorL << "accept errno = " << errno << " errstr = " << strerror(errno);
    }
  }
}
std::string TCPServer::to_string(const std::string &prefix) {
  std::stringstream ss;
  ss << prefix << "[type = " << _type << " name = " << _name
     << " io_worker = " << (_io_worker ? _io_worker->get_name() : "")
     << " accept = " << (_accept_worker ? _accept_worker->get_name() : "") << "recv_timeout = " << _recv_timeout << "]"
     << std::endl;
  std::string pfx = prefix.empty() ? "    " : prefix;
  for (auto &sock : _socks) {
    ss << pfx << pfx << *sock << std::endl;
  }
  return ss.str();
}

}  // namespace fleet