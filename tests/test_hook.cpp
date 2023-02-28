#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include "IO/fd_manager.h"
#include "IO/hook.h"
#include "IO/iomanager.h"
#include "Utils/log.h"

void test_sleep() {
  InfoL << "test_sleep begin";
  auto iom = fleet::IOManager::s_get_this();

  iom->schedule([]() {
    sleep(2);
    InfoL << "sleep 2";
  });

  iom->schedule([]() {
    sleep(3);
    InfoL << "sleep 3";
  });
}

void test_sock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, "110.242.68.66", &addr.sin_addr.s_addr);

  InfoL << "begin connect";
  int ret = connect(sock, (const sockaddr *)&addr, sizeof(addr));
  if (ret) {
    ErrorL << "connect ret = " << ret << " errno = " << errno << " errstr = " << strerror(errno);
    return;
  }

  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  ret = send(sock, data, sizeof(data), 0);
  if (ret == -1) {
    ErrorL << "send ret = " << ret << " errno = " << errno << " errstr = " << strerror(errno);
    return;
  }

  char buf[4096] = {0};

  ret = recv(sock, buf, sizeof(buf), 0);
  if (ret == -1) {
    ErrorL << "recv ret = " << ret << " errno = " << errno;
    return;
  }

  InfoL << '\n' << buf;  // 大概有10%的概率没有输出结果，而且没有出现超时的情况
}

int main() {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();

  fleet::IOManager iom;
  // iom.schedule(test_sleep);
  iom.schedule(test_sock);

  InfoL << "main end";

  return 0;
}