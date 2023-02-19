#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <cstring>

#include "Fiber/iomanager.h"
#include "Utils/log.h"
#include "Utils/macro.h"

static int sock_fd;
void watch_io_read();

// 用于判断连接是否成功
void do_io_write() {
  InfoL << "write callback";
  int so_err = 0;
  socklen_t len = static_cast<size_t>(so_err);
  getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &so_err, &len);
  if (so_err) {
    InfoL << "connect fail";
    return;
  }
  InfoL << "connect success";
}

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
  if (ret != 0) {
    /* EINPROGRESS, which tells you that the operation is in progress and you should check its status later.
    To check the status later, the socket will become ready for writability */
    if (errno == EINPROGRESS) {
      InfoL << "EINPROGRESS";
      // 注册写事件，只用于判断connect是否成功
      // 非堵塞的TCP套接字connect一般无法立即建立连接，要通过套接字可写来判断connect是否已经完成
      fleet::IOManager::s_get_this()->add_event(sock_fd, fleet::IOManager::WRITE, do_io_write);
      // 注册事件回调，注意：事件是一次性的
      fleet::IOManager::s_get_this()->add_event(sock_fd, fleet::IOManager::READ, do_io_read);
    } else {
      ErrorL << "connect error, errno: " << errno << ", errstr: " << strerror(errno);
    }
  } else {
    ErrorL << "connect error, errno: " << errno << ", errstr: " << strerror(errno);
  }
}

int main(int argc, char **argv) {
  LOG_DEFAULT;
  fleet::IOManager iom(2, false);

  iom.schedule(test_io);
}