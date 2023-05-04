#include <asm-generic/errno.h>
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
    while (true) {
      memset(buf, 0, sizeof(buf));
      int n = client->recv(buf, sizeof(buf));
      if (n > 0) {
        // 正常收到客户端的消息
        InfoL << "recv: " << buf;
        client->send(buf, n);
      } else if (n == -1 && errno != ECONNRESET) {
        // 超时
        InfoL << "recv timeout: " << client->to_string();
        client->send("bye", 4);
        client->close();
        break;
      } else {
        // 客户端断开了
        InfoL << "client disconnect: " << client->to_string();
        client->close();
        break;
      }
    }
  }
};

void run() {
  fleet::TCPServer::Ptr server(new EchoServer(1000 * 1000));
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
  // fleet::Logger::Instance().set_level(fleet::LogLevel::Info);

  fleet::IOManager iom(1);
  iom.schedule(run);

  return 0;
}