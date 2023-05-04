#include <unistd.h>
#include <cstring>
#include <thread>
#include "Network/tcp_server.h"
#include "Utils/log.h"
#include "Utils/macro.h"

//
class HTTPServer : public fleet::TCPServer {
 public:
  HTTPServer(uint64_t recv_timeout) : fleet::TCPServer(recv_timeout) {}

 protected:
  void handle_client(fleet::Socket::Ptr client) override {
    InfoL << "new client: " << client->to_string();
    char buf[4096];  // 不用static，因为可能会有多线程竞争，不安全
    memset(buf, 0, sizeof(buf));
    int n = client->recv(buf, sizeof(buf));
    if (n > 0) {
      // 找到第一个\n
      auto space = strchr(buf, ' ');
      if (space) {
        *space = 0;
        InfoL << buf;
        if (strcmp(buf, "GET") == 0) {
          char resp[] = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: 13\n\nHello World!";
          client->send(resp, sizeof(resp));
        }
      }
    }
    client->close();
  }
};

void run() {
  fleet::TCPServer::Ptr server(new HTTPServer(1000 * 5));
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

  fleet::IOManager iom(2);
  iom.schedule(run);

  return 0;
}