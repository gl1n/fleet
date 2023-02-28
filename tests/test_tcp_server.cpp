#include <unistd.h>
#include <cstdint>
#include <cstring>
#include "IO/iomanager.h"
#include "Network/address.h"
#include "Network/socket.h"
#include "Network/tcp_server.h"
#include "Utils/log.h"
#include "Utils/macro.h"

//
class EchoServer : public fleet::TCPServer {
 public:
  EchoServer(uint64_t recv_timeout) : fleet::TCPServer(recv_timeout) {}

 protected:
  void handle_client(fleet::Socket::Ptr client) override {
    InfoL << "new client: " << client->to_string();
    char buf[4096];  // 不用static，因为可能会有多线程竞争，不安全
    memset(buf, 0, sizeof(buf));
    int n = client->recv(buf, sizeof(buf));
    InfoL << "recv: " << buf;
    client->send(buf, n);
    client->close();
  }
};

void run() {
  fleet::TCPServer::Ptr server(new EchoServer(3000));
  auto addr = fleet::Address::lookup_any_IPAddress("0.0.0.0:1234");
  ASSERT(addr);

  while (!server->bind(addr)) {
    sleep(2);
  }

  InfoL << "bind success, " << server->to_string();

  server->start();
}

int main() {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  fleet::Logger::Instance().set_level(fleet::LogLevel::Info);

  fleet::IOManager iom(1, true);
  iom.schedule(run);

  iom.stop();

  return 0;
}