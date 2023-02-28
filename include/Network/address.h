#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <map>
#include <memory>
#include <vector>

namespace fleet {

class IPAddress;

class Address {
 public:
  using Ptr = std::shared_ptr<Address>;

  static Address::Ptr create(const sockaddr *addr);

  /**
   * @param host
   * ipv6的格式是[1050:0:0:0:5:600:300c:326b]:service，ipv4的格式是192.168.165.2:service。serivce可以是端口号也可以是服务名
   * @return 所有满足条件的Address
   */
  static bool lookup(std::vector<Address::Ptr> &result, const std::string &host, int family = AF_INET, int type = 0,
                     int protocol = 0);

  /**
   * @return 第一个满足条件的Address
   */
  static Address::Ptr lookup_any(const std::string &host, int family = AF_INET, int type = 0, int protocol = 0);

  /**
   * @return 第一个满足条件且能够转型成IPAddress
   */
  static std::shared_ptr<IPAddress> lookup_any_IPAddress(const std::string &host, int family = AF_INET, int type = 0,
                                                         int protocol = 0);

  static bool get_interface_Addresses(std::multimap<std::string, std::pair<Address::Ptr, uint32_t>> &result,
                                      int family = AF_INET);

  static bool get_interface_Addresses(std::vector<std::pair<Address::Ptr, uint32_t>> &result, const std::string &iface,
                                      int family = AF_INET);

  virtual ~Address() {}

  int get_family() const;

  virtual const sockaddr *get_addr() const = 0;

  virtual sockaddr *get_addr() = 0;

  virtual socklen_t get_addr_len() const = 0;

  virtual std::ostream &insert(std::ostream &os) const = 0;

  std::string to_string() const;

  bool operator<(const Address &rhs) const;

  bool operator==(const Address &rhs) const;

  bool operator!=(const Address &rhs) const;
};

class IPAddress : public Address {
 public:
  using Ptr = std::shared_ptr<IPAddress>;

  static IPAddress::Ptr create(const char *address, uint16_t port = 0);

  virtual IPAddress::Ptr broadcast_address(uint32_t prefix_len) = 0;

  virtual IPAddress::Ptr network_address(uint32_t prefix_len) = 0;

  virtual IPAddress::Ptr subnet_mask(uint32_t prefix_len) = 0;

  virtual uint32_t get_port() const = 0;

  virtual void set_port(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
 public:
  using Ptr = std::shared_ptr<IPv4Address>;

  static IPv4Address::Ptr create(const char *address, uint16_t port = 0);

  IPv4Address(const sockaddr_in &address);

  IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

  const sockaddr *get_addr() const override;
  sockaddr *get_addr() override;
  socklen_t get_addr_len() const override;
  std::ostream &insert(std::ostream &os) const override;

  IPAddress::Ptr broadcast_address(uint32_t prefix_len) override;
  IPAddress::Ptr network_address(uint32_t prefix_len) override;
  IPAddress::Ptr subnet_mask(uint32_t prefix_len) override;
  uint32_t get_port() const override;
  void set_port(uint16_t v) override;

 private:
  sockaddr_in _addr;
};

class IPv6Address : public IPAddress {
 public:
  using Ptr = std::shared_ptr<IPv6Address>;

  static IPv6Address::Ptr create(const char *address, uint16_t port = 0);

  IPv6Address();

  IPv6Address(const sockaddr_in6 &address);

  IPv6Address(const uint8_t address[16], uint16_t port = 0);

  const sockaddr *get_addr() const override;
  sockaddr *get_addr() override;
  socklen_t get_addr_len() const override;
  std::ostream &insert(std::ostream &os) const override;

  IPAddress::Ptr broadcast_address(uint32_t prefix_len) override;
  IPAddress::Ptr network_address(uint32_t prefix_len) override;
  IPAddress::Ptr subnet_mask(uint32_t prefix_len) override;
  uint32_t get_port() const override;
  void set_port(uint16_t v) override;

 private:
  sockaddr_in6 _addr;
};

class UnknownAddress : public Address {
 public:
  using Ptr = std::shared_ptr<UnknownAddress>;

  UnknownAddress(int family);
  UnknownAddress(const sockaddr &addr);

  const sockaddr *get_addr() const override;
  sockaddr *get_addr() override;
  socklen_t get_addr_len() const override;
  std::ostream &insert(std::ostream &os) const override;

 private:
  sockaddr _addr;
};

std::ostream &operator<<(std::ostream &os, const Address &addr);

}  // namespace fleet