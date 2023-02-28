#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/types.h>
#include <cstring>
#include <memory>
#include <string>

#include "Network/address.h"
#include "Utils/log.h"

namespace fleet {

template <class T>
static T create_mask(uint32_t bits) {
  return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <class T>
static uint32_t count_bits(T value) {
  uint32_t result = 0;
  for (; value; ++result) {
    value &= (value - 1);
  }
  return result;
}

Address::Ptr Address::lookup_any(const std::string &host, int family, int type, int protocol) {
  std::vector<Address::Ptr> result;
  if (lookup(result, host, family, type, protocol)) {
    return result[0];
  }
  return nullptr;
}

std::shared_ptr<IPAddress> Address::lookup_any_IPAddress(const std::string &host, int family, int type, int protocol) {
  std::vector<Address::Ptr> result;
  if (lookup(result, host, family, type, protocol)) {
    for (auto &i : result) {
      IPAddress::Ptr v = std::dynamic_pointer_cast<IPAddress>(i);
      // 转型成功则返回
      if (v) {
        return v;
      }
    }
  }
  return nullptr;
}

bool Address::lookup(std::vector<Address::Ptr> &result, const std::string &host, int family, int type, int protocol) {
  std::string node;            // ip部分
  const char *service = NULL;  // 端口（服务）部分

  if (!host.empty() && host[0] == '[') {
    // 此地址是否是ipv6
    const char *right_bracket = (const char *)memchr(host.c_str() + 1, ']', host.size() - 1);
    if (right_bracket) {
      // 如果存在]，则此地址是ipv6
      if (*(right_bracket + 1) == ':') {
        // 如果]的右边是:，取出端口部分
        service = right_bracket + 2;
      }
      // 取出ip的部分
      node = host.substr(1, right_bracket - host.c_str() - 1);
    }
  }

  // 如果此时ip部分还是空的
  if (node.empty()) {
    // 当作ipv4处理，找端口号
    service = (const char *)memchr(host.c_str(), ':', host.size());
    if (service) {
      // 如果有:
      if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
        // 如果没有第二个:
        node = host.substr(0, service - host.c_str());  // 取出ip部分
        ++service;                                      // 取出端口部分
      }
    }
  }
  // 没有端口号
  if (node.empty()) {
    node = host;
  }

  addrinfo hints, *res, *next;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = family;
  hints.ai_socktype = type;
  hints.ai_protocol = protocol;

  int error = getaddrinfo(node.c_str(), service, &hints, &res);
  if (error) {
    DebugL << "getaddress(" << host << ", " << family << ", " << type << ") err=" << error
           << " errstr=" << gai_strerror(error);
    return false;
  }

  // 可能有多个结果，全部保存
  next = res;
  while (next) {
    result.push_back(create(next->ai_addr));
    /// 一个ip/端口可以对应多种接字类型，比如SOCK_STREAM, SOCK_DGRAM, SOCK_RAW，所以这里会返回重复的结果
    DebugL << "family:" << next->ai_family << ", sock type:" << next->ai_socktype;
    next = next->ai_next;
  }

  freeaddrinfo(res);
  return !result.empty();
}

bool Address::get_interface_Addresses(std::multimap<std::string, std::pair<Address::Ptr, uint32_t>> &result,
                                      int family) {
  struct ifaddrs *next, *results;
  if (getifaddrs(&results) != 0) {
    DebugL << "getifaddrs "
              " err="
           << errno << " errstr=" << strerror(errno);
    return false;
  }

  try {
    for (next = results; next; next = next->ifa_next) {
      Address::Ptr addr;
      uint32_t prefix_len = ~0u;
      // AF_UNSPEC能匹配AF_INET和AF_INET6
      if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
        continue;
      }
      switch (next->ifa_addr->sa_family) {
        case AF_INET: {
          addr = create(next->ifa_addr);
          uint32_t netmask = ((sockaddr_in *)next->ifa_netmask)->sin_addr.s_addr;
          prefix_len = count_bits(netmask);
        } break;
        case AF_INET6: {
          addr = create(next->ifa_addr);
          in6_addr &netmask = ((sockaddr_in6 *)next->ifa_netmask)->sin6_addr;
          prefix_len = 0;
          for (int i = 0; i < 16; ++i) {
            prefix_len += count_bits(netmask.s6_addr[i]);
          }
        } break;
        default:
          break;
      }

      if (addr) {
        result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
      }
    }
  } catch (...) {
    ErrorL << "exception";
    freeifaddrs(results);
    return false;
  }
  freeifaddrs(results);
  return !result.empty();
}

bool Address::get_interface_Addresses(std::vector<std::pair<Address::Ptr, uint32_t>> &result, const std::string &iface,
                                      int family) {
  if (iface.empty() || iface == "*") {
    if (family == AF_INET || family == AF_UNSPEC) {
      result.push_back(std::make_pair(Address::Ptr(new IPv4Address()), 0u));
    }
    if (family == AF_INET6 || family == AF_UNSPEC) {
      result.push_back(std::make_pair(Address::Ptr(new IPv6Address()), 0u));
    }
    return true;
  }

  std::multimap<std::string, std::pair<Address::Ptr, uint32_t>> results;

  if (!get_interface_Addresses(results, family)) {
    return false;
  }

  auto its = results.equal_range(iface);
  for (; its.first != its.second; ++its.first) {
    result.push_back(its.first->second);
  }
  return !result.empty();
}

int Address::get_family() const { return get_addr()->sa_family; }

std::string Address::to_string() const {
  std::stringstream ss;
  insert(ss);
  return ss.str();
}

Address::Ptr Address::create(const sockaddr *addr) {
  if (addr == nullptr) {
    return nullptr;
  }

  Address::Ptr result;
  switch (addr->sa_family) {
    // ipv4
    case AF_INET:
      result.reset(new IPv4Address(*(const sockaddr_in *)addr));
      break;
    // ipv6
    case AF_INET6:
      result.reset(new IPv6Address(*(const sockaddr_in6 *)addr));
      break;
    default:
      result.reset(new UnknownAddress(*addr));
      break;
  }
  return result;
}

bool Address::operator<(const Address &rhs) const {
  socklen_t minlen = std::min(get_addr_len(), rhs.get_addr_len());
  int result = memcmp(get_addr(), rhs.get_addr(), minlen);
  if (result < 0) {
    return true;
  } else if (result > 0) {
    return false;
  } else if (get_addr_len() < rhs.get_addr_len()) {
    return true;
  }
  return false;
}

bool Address::operator==(const Address &rhs) const {
  return get_addr_len() == rhs.get_addr_len() && memcmp(get_addr(), rhs.get_addr(), get_addr_len()) == 0;
}

bool Address::operator!=(const Address &rhs) const { return !(*this == rhs); }

IPAddress::Ptr IPAddress::create(const char *address, uint16_t port) {
  addrinfo hints, *results;
  memset(&hints, 0, sizeof(addrinfo));

  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = AF_UNSPEC;

  int error = getaddrinfo(address, NULL, &hints, &results);
  if (error) {
    DebugL << "IPAddress::create(" << address << ", " << port << ") error=" << error << " errno=" << errno
           << " errstr=" << strerror(errno);
    return nullptr;
  }

  try {
    IPAddress::Ptr result = std::dynamic_pointer_cast<IPAddress>(Address::create(results->ai_addr));
    if (result) {
      result->set_port(port);
    }
    freeaddrinfo(results);
    return result;
  } catch (...) {
    freeaddrinfo(results);
    return nullptr;
  }
}

IPv4Address::Ptr IPv4Address::create(const char *address, uint16_t port) {
  IPv4Address::Ptr rt(new IPv4Address);
  rt->_addr.sin_port = port;
  int result = inet_pton(AF_INET, address, &rt->_addr.sin_addr);
  if (result <= 0) {
    DebugL << "IPv4Address::create(" << address << ", " << port << ") rt=" << result << " errno=" << errno
           << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

IPv4Address::IPv4Address(const sockaddr_in &address) { _addr = address; }

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  _addr.sin_port = port;
  _addr.sin_addr.s_addr = address;
}

sockaddr *IPv4Address::get_addr() { return (sockaddr *)&_addr; }

const sockaddr *IPv4Address::get_addr() const { return (sockaddr *)&_addr; }

socklen_t IPv4Address::get_addr_len() const { return sizeof(_addr); }

std::ostream &IPv4Address::insert(std::ostream &os) const {
  uint32_t addr = _addr.sin_addr.s_addr;
  os << (addr & 0xff) << "." << ((addr >> 8) & 0xff) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 24) & 0xff);
  os << ":" << ntohs(_addr.sin_port);
  return os;
}

IPAddress::Ptr IPv4Address::broadcast_address(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(_addr);
  baddr.sin_addr.s_addr |= create_mask<uint32_t>(prefix_len);
  return IPv4Address::Ptr(new IPv4Address(baddr));
}

IPAddress::Ptr IPv4Address::network_address(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(_addr);
  baddr.sin_addr.s_addr &= ~create_mask<uint32_t>(prefix_len);
  return IPv4Address::Ptr(new IPv4Address(baddr));
}

IPAddress::Ptr IPv4Address::subnet_mask(uint32_t prefix_len) {
  sockaddr_in subnet;
  memset(&subnet, 0, sizeof(subnet));
  subnet.sin_family = AF_INET;
  subnet.sin_addr.s_addr = ~create_mask<uint32_t>(prefix_len);
  return IPv4Address::Ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::get_port() const { return _addr.sin_port; }

void IPv4Address::set_port(uint16_t v) { _addr.sin_port = v; }

IPv6Address::Ptr IPv6Address::create(const char *address, uint16_t port) {
  IPv6Address::Ptr rt(new IPv6Address);
  rt->_addr.sin6_port = port;
  int result = inet_pton(AF_INET6, address, &rt->_addr.sin6_addr);
  if (result <= 0) {
    DebugL << "IPv6Address::create(" << address << ", " << port << ") rt=" << result << " errno=" << errno
           << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

IPv6Address::IPv6Address() {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6 &address) { _addr = address; }

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin6_family = AF_INET6;
  _addr.sin6_port = port;
  memcpy(&_addr.sin6_addr.s6_addr, address, 16);
}

sockaddr *IPv6Address::get_addr() { return (sockaddr *)&_addr; }

const sockaddr *IPv6Address::get_addr() const { return (sockaddr *)&_addr; }

socklen_t IPv6Address::get_addr_len() const { return sizeof(_addr); }

std::ostream &IPv6Address::insert(std::ostream &os) const {
  os << "[";
  uint16_t *addr = (uint16_t *)_addr.sin6_addr.s6_addr;
  bool used_zeros = false;
  for (size_t i = 0; i < 8; ++i) {
    if (addr[i] == 0 && !used_zeros) {
      continue;
    }
    if (i && addr[i - 1] == 0 && !used_zeros) {
      os << ":";
      used_zeros = true;
    }
    if (i) {
      os << ":";
    }
    os << std::hex << (int)addr[i] << std::dec;
  }

  if (!used_zeros && addr[7] == 0) {
    os << "::";
  }

  os << "]:" << _addr.sin6_port;
  return os;
}

IPAddress::Ptr IPv6Address::broadcast_address(uint32_t prefix_len) {
  sockaddr_in6 baddr(_addr);
  baddr.sin6_addr.s6_addr[prefix_len / 8] |= create_mask<uint8_t>(prefix_len % 8);
  for (int i = prefix_len / 8 + 1; i < 16; ++i) {
    baddr.sin6_addr.s6_addr[i] = 0xff;
  }
  return IPv6Address::Ptr(new IPv6Address(baddr));
}

IPAddress::Ptr IPv6Address::network_address(uint32_t prefix_len) {
  sockaddr_in6 baddr(_addr);
  baddr.sin6_addr.s6_addr[prefix_len / 8] &= create_mask<uint8_t>(prefix_len % 8);
  for (int i = prefix_len / 8 + 1; i < 16; ++i) {
    baddr.sin6_addr.s6_addr[i] = 0x00;
  }
  return IPv6Address::Ptr(new IPv6Address(baddr));
}

IPAddress::Ptr IPv6Address::subnet_mask(uint32_t prefix_len) {
  sockaddr_in6 subnet;
  memset(&subnet, 0, sizeof(subnet));
  subnet.sin6_family = AF_INET6;
  subnet.sin6_addr.s6_addr[prefix_len / 8] = ~create_mask<uint8_t>(prefix_len % 8);

  for (uint32_t i = 0; i < prefix_len / 8; ++i) {
    subnet.sin6_addr.s6_addr[i] = 0xff;
  }
  return IPv6Address::Ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::get_port() const { return _addr.sin6_port; }

void IPv6Address::set_port(uint16_t v) { _addr.sin6_port = v; }

UnknownAddress::UnknownAddress(int family) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr &addr) { _addr = addr; }

sockaddr *UnknownAddress::get_addr() { return (sockaddr *)&_addr; }

const sockaddr *UnknownAddress::get_addr() const { return &_addr; }

socklen_t UnknownAddress::get_addr_len() const { return sizeof(_addr); }

std::ostream &UnknownAddress::insert(std::ostream &os) const {
  os << "[UnknownAddress family=" << _addr.sa_family << "]";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Address &addr) { return addr.insert(os); }
}  // namespace fleet