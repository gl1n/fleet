#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <cstring>

#include "iomanager.h"
#include "log.h"
#include "macro.h"

static int sock_fd;
void watch_io_read();

void do_io_read() {
  InfoL << "read callback";
  char buf[1024] = {0};
  int readlen = 0;
  readlen = read(sock_fd, buf, sizeof(buf));
  if (readlen > 0) {
    buf[readlen] = '\0';
    InfoL << "read " << readlen << " bytes, read: " << buf;
  } else if (readlen == 0) {
    InfoL << "peer closed";  // 对端断开连接也会触发读事件
    close(sock_fd);
    return;
  } else {
    ErrorL << "err, errno = " << errno << ", errstr = " << strerror(errno);
    close(sock_fd);
    return;
  }
  // 当前出去read回调内，不能直接重新add_event，否则会造成重复注册
  fleet::IOManager::s_get_this()->schedule(watch_io_read);
}

void watch_io_read() {
  InfoL << "watch_io_read";
  fleet::IOManager::s_get_this()->add_event(sock_fd, fleet::IOManager::READ, do_io_read);
}

void test_io() {
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT(sock_fd > 0);
  fcntl(sock_fd, F_SETFL, O_NONBLOCK);

  sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(80);
  std::string baidu = "39.156.66.10";
  inet_pton(AF_INET, baidu.c_str(), &servaddr.sin_addr.s_addr);

  int ret = connect(sock_fd, (const sockaddr *)&servaddr, sizeof(servaddr));
  if (ret == 0) {
    fleet::IOManager::s_get_this()->add_event(sock_fd, fleet::IOManager::READ, do_io_read);
  } else {
    ErrorL << "connect error, errno: " << errno << ", errstr: " << strerror(errno);
  }
}

int main(int argc, char **argv) {
  LOG_DEFAULT;
  fleet::Logger::Instance().set_async();
  fleet::IOManager iom(1);

  iom.schedule(test_io);
}