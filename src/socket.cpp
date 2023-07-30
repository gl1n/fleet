#include "socket.h"
#include "fd_manager.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"
#include "macro.h"

#include <netinet/tcp.h>
#include <unistd.h>

namespace fleet {
Socket::Ptr Socket::create_TCP_Socket(int family) {
  Socket::Ptr sock(new Socket(family, TCP, 0));
  return sock;
}

Socket::Ptr Socket::create_UDP_Socket(int family) {
  Socket::Ptr sock(new Socket(family, UDP, 0));
  sock->new_Socket();
  sock->_is_connected = true;
  return sock;
}

Socket::Ptr Socket::create_TCP_Socket4() { return create_TCP_Socket(IPv4); }

Socket::Ptr Socket::create_UDP_Socket4() { return create_UDP_Socket(IPv4); }

Socket::Ptr Socket::create_TCP_Socket6() { return create_TCP_Socket(IPv6); }

Socket::Ptr Socket::create_UDP_Socket6() { return create_UDP_Socket(IPv6); }

Socket::Ptr Socket::create_Unix_TCP_Socket() {
  Socket::Ptr sock(new Socket(UNIX, TCP, 0));
  return sock;
}

Socket::Ptr Socket::create_Unix_UDP_Socket() {
  Socket::Ptr sock(new Socket(UNIX, UDP, 0));
  return sock;
}

Socket::Socket(int family, int type, int protocol)
    : _sock(-1), _family(family), _type(type), _protocol(protocol), _is_connected(false) {}

Socket::~Socket() { close(); }

int64_t Socket::get_send_timeout() {
  FdCtx::Ptr ctx = FdManager::Instance().get_FdCtx(_sock);
  if (ctx) {
    return ctx->get_timeout(SO_SNDTIMEO);
  }
  return -1;
}

void Socket::set_send_timeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  set_option(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::get_recv_timeout() {
  FdCtx::Ptr ctx = FdManager::Instance().get_FdCtx(_sock);
  if (ctx) {
    return ctx->get_timeout(SO_RCVTIMEO);
  }
  return -1;
}

void Socket::set_recv_timeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  set_option(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::get_option(int level, int option, void *result, socklen_t *len) {
  int rt = getsockopt(_sock, level, option, result, (socklen_t *)len);
  if (rt) {
    DebugL << "get_option sock=" << _sock << " level=" << level << " option=" << option << " errno=" << errno
           << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::set_option(int level, int option, const void *result, socklen_t len) {
  if (setsockopt(_sock, level, option, result, (socklen_t)len)) {
    DebugL << "set_option sock=" << _sock << " level=" << level << " option=" << option << " errno=" << errno
           << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

Socket::Ptr Socket::accept() {
  Socket::Ptr client_sock(new Socket(_family, _type, _protocol));
  int raw_client_sock = ::accept(_sock, nullptr, nullptr);
  if (raw_client_sock == -1) {
    ErrorL << "accept(" << _sock << ") errno=" << errno << " errstr=" << strerror(errno);
    return nullptr;
  }
  if (client_sock->init(raw_client_sock)) {
    return client_sock;
  }
  return nullptr;
}

bool Socket::init(int sock) {
  FdCtx::Ptr ctx = FdManager::Instance().create_FdCtx(sock);
  if (ctx && ctx->is_socket() && !ctx->is_close()) {
    _sock = sock;
    _is_connected = true;
    init_Socket();
    get_local_Address();
    get_remote_Address();
    return true;
  }
  return false;
}

bool Socket::bind(const Address::Ptr addr) {
  // 记录本地端地址
  _local_Address = addr;
  if (!is_valid()) {
    // 重新初始化Socket
    new_Socket();
    if (UNLIKELY(!is_valid())) {
      // 还是无效就返回吧
      return false;
    }
  }

  if (UNLIKELY(addr->get_family() != _family)) {
    ErrorL << "bind sock.family(" << _family << ") addr.family(" << addr->get_family()
           << ") not equal, addr=" << addr->to_string();
    return false;
  }

  if (::bind(_sock, addr->get_addr(), addr->get_addr_len())) {
    ErrorL << "bind error errrno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  get_local_Address();
  return true;
}

bool Socket::reconnect(uint64_t timeout_ms) {
  if (!_remote_Address) {
    ErrorL << "reconnect m_remote_address is null";
    return false;
  }
  _local_Address.reset();
  return connect(_remote_Address, timeout_ms);
}

bool Socket::connect(const Address::Ptr addr, uint64_t timeout_ms) {
  _remote_Address = addr;
  if (!is_valid()) {
    new_Socket();
    if (UNLIKELY(!is_valid())) {
      return false;
    }
  }

  if (UNLIKELY(addr->get_family() != _family)) {
    ErrorL << "connect sock.family(" << _family << ") addr.family(" << addr->get_family()
           << ") not equal, addr=" << addr->to_string();
    return false;
  }

  if (timeout_ms == (uint64_t)-1) {
    if (::connect(_sock, addr->get_addr(), addr->get_addr_len())) {
      ErrorL << "sock=" << _sock << " connect(" << addr->to_string() << ") error errno=" << errno
             << " errstr=" << strerror(errno);
      close();
      return false;
    }
  } else {
    if (::connect_with_timeout(_sock, addr->get_addr(), addr->get_addr_len(), timeout_ms)) {
      ErrorL << "sock=" << _sock << " connect(" << addr->to_string() << ") timeout=" << timeout_ms
             << " error errno=" << errno << " errstr=" << strerror(errno);
      close();
      return false;
    }
  }
  _is_connected = true;
  get_remote_Address();
  get_local_Address();
  return true;
}

bool Socket::listen(int backlog) {
  if (!is_valid()) {
    ErrorL << "listen error sock=-1";
    return false;
  }
  if (::listen(_sock, backlog)) {
    ErrorL << "listen error errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::close() {
  if (!_is_connected && _sock == -1) {
    return true;
  }
  _is_connected = false;
  if (_sock != -1) {
    ::close(_sock);
    _sock = -1;
  }
  return false;
}

int Socket::send(const void *buffer, size_t length, int flags) {
  if (is_connected()) {
    return ::send(_sock, buffer, length, flags);
  }
  return -1;
}

int Socket::send(const iovec *buffers, size_t length, int flags) {
  if (is_connected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buffers;
    msg.msg_iovlen = length;
    return ::sendmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::send_to(const void *buffer, size_t length, const Address::Ptr to, int flags) {
  if (is_connected()) {
    return ::sendto(_sock, buffer, length, flags, to->get_addr(), to->get_addr_len());
  }
  return -1;
}

int Socket::send_to(const iovec *buffers, size_t length, const Address::Ptr to, int flags) {
  if (is_connected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buffers;
    msg.msg_iovlen = length;
    msg.msg_name = to->get_addr();
    msg.msg_namelen = to->get_addr_len();
    return ::sendmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::recv(void *buffer, size_t length, int flags) {
  if (is_connected()) {
    return ::recv(_sock, buffer, length, flags);
  }
  return -1;
}

int Socket::recv(iovec *buffers, size_t length, int flags) {
  if (is_connected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buffers;
    msg.msg_iovlen = length;
    return ::recvmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::recv_from(void *buffer, size_t length, Address::Ptr from, int flags) {
  if (is_connected()) {
    socklen_t len = from->get_addr_len();
    return ::recvfrom(_sock, buffer, length, flags, from->get_addr(), &len);
  }
  return -1;
}

int Socket::recv_from(iovec *buffers, size_t length, Address::Ptr from, int flags) {
  if (is_connected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buffers;
    msg.msg_iovlen = length;
    msg.msg_name = from->get_addr();
    msg.msg_namelen = from->get_addr_len();
    return ::recvmsg(_sock, &msg, flags);
  }
  return -1;
}

Address::Ptr Socket::get_remote_Address() {
  if (_remote_Address) {
    return _remote_Address;
  }

  Address::Ptr result;
  switch (_family) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    default:
      result.reset(new UnknownAddress(_family));
      break;
  }
  socklen_t addrlen = result->get_addr_len();
  if (getpeername(_sock, result->get_addr(), &addrlen)) {
    ErrorL << "getpeername error sock=" << _sock << " errno=" << errno << " errstr=" << strerror(errno);
    return Address::Ptr(new UnknownAddress(_family));
  }
  _remote_Address = result;
  return _remote_Address;
}

Address::Ptr Socket::get_local_Address() {
  if (_local_Address) {
    return _local_Address;
  }

  Address::Ptr result;
  switch (_family) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    default:
      result.reset(new UnknownAddress(_family));
      break;
  }
  socklen_t addrlen = result->get_addr_len();
  if (getsockname(_sock, result->get_addr(), &addrlen)) {
    ErrorL << "getsockname error sock=" << _sock << " errno=" << errno << " errstr=" << strerror(errno);
    return Address::Ptr(new UnknownAddress(_family));
  }
  _local_Address = result;
  return _local_Address;
}

bool Socket::is_valid() const { return _sock != -1; }

int Socket::get_error() {
  int error = 0;
  socklen_t len = sizeof(error);
  if (!get_option(SOL_SOCKET, SO_ERROR, &error, &len)) {
    error = errno;
  }
  return error;
}

std::ostream &Socket::dump(std::ostream &os) const {
  // 将所有成员变量输出到os中
  os << "[Socket sock = " << _sock << ", is_connected = " << _is_connected << ", family = " << _family
     << ", type = " << _type << " protocol = " << _protocol;
  if (_local_Address) {
    os << " local_address = " << _local_Address->to_string();
  }
  if (_remote_Address) {
    os << " remote_address = " << _remote_Address->to_string();
  }
  os << "]";
  return os;
}

std::string Socket::to_string() const {
  std::stringstream ss;
  dump(ss);
  return ss.str();
}

bool Socket::cancel_read() { return IOManager::s_get_this()->del_event(_sock, fleet::IOManager::READ, true); }

bool Socket::cancel_write() { return IOManager::s_get_this()->del_event(_sock, fleet::IOManager::WRITE, true); }

bool Socket::cancel_accept() { return IOManager::s_get_this()->del_event(_sock, fleet::IOManager::READ, true); }

bool Socket::cancel_all() { return IOManager::s_get_this()->del_and_trigger_all(_sock); }

void Socket::init_Socket() {
  int val = 1;
  set_option(SOL_SOCKET, SO_REUSEADDR, val);
  if (_type == SOCK_STREAM) {
    set_option(IPPROTO_TCP, TCP_NODELAY, val);
  }
}

void Socket::new_Socket() {
  _sock = socket(_family, _type, _protocol);
  if (LIKELY(_sock != -1)) {
    init_Socket();
  } else {
    ErrorL << "socket(" << _family << ", " << _type << ", " << _protocol << ") errno=" << errno
           << " errstr=" << strerror(errno);
  }
}

std::ostream &operator<<(std::ostream &os, const Socket &sock) { return sock.dump(os); }
}  // namespace fleet