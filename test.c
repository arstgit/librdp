#include "rdp.h"

#ifndef __USE_XOPEN2K
#define __USE_XOPEN2K
#endif

#include <assert.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define NI_MAXHOST 1025 // Maximum length of host.
#define NI_MAXSERV 32   // Maximum length of service/port.
#define NI_NUMERICSERV 2

#define EPOLL_MAX_EVENTS 16

rdpSocket *ctx1, *ctx2;
rdpConn *conn1, *conn2;

char *rdpAddressStr(const struct sockaddr *addr, socklen_t addrlen,
                    char *addrStr, int addrStrLen) {
  char host[NI_MAXHOST], service[NI_MAXSERV];

  if (getnameinfo(addr, addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                  NI_NUMERICSERV) == 0)
    snprintf(addrStr, addrStrLen, "(%s, %s)", host, service);
  else
    snprintf(addrStr, addrStrLen, "(?UNKNOWN?)");

  return addrStr;
}

int processIn(int fd) {
  int readCount;
  int events;
  uint8_t buf[1500];
  size_t count = 1024;
  rdpConn *newConn;

  if (fd == rdpSocketGetProp(ctx1, RDP_PROP_FD)) {
    for (;;) {
      readCount = rdpReadPoll(ctx1, buf, count, &newConn, &events);
      if (events & RDP_AGAIN) {
        printf("ctx1 rdp_again\n");
        break;
      }

      if (events & RDP_CONNECTED) {
        printf("ctx1 connected\n");

        ssize_t n;
        n = rdpWrite(conn1, "test1.", 6);
        n = rdpWrite(conn1, "test2.", 6);
        printf("write num: %ld\n", n);
      }

      if (events & RDP_DATA) {
        printf("ctx1 data\n");
        printf("count: %d, data: %s\n", readCount, buf);
        rdpConnClose(newConn);
      }
      if (events & RDP_CONTINUE) {
        printf("ctx1 continue\n");
        continue;
      }
    }
  } else if (fd == rdpSocketGetProp(ctx2, RDP_PROP_FD)) {
    for (;;) {
      readCount = rdpReadPoll(ctx2, buf, count, &newConn, &events);
      if (events & RDP_AGAIN) {
        printf("ctx2 again\n");
        break;
      }

      if (events & RDP_ACCEPT) {
        printf("ctx2 accept\n");
        struct sockaddr_storage addr;
        socklen_t len;
        char info[1024];

        rdpConnGetAddr(newConn, (struct sockaddr *)&addr, &len);

        rdpAddressStr((struct sockaddr *)&addr, len, info, 1024);

        printf("accept from addr: %s\n", info);

        ssize_t n;
        n = rdpWrite(newConn, "test3.", 6);
      }
      if (events & RDP_DATA) {
        printf("ctx2 data\n");
        if (readCount == 0) {
          rdpConnClose(newConn);
          return 0;
        }

        buf[readCount] = '\x00';
        printf("count: %d, data: %s\n", readCount, buf);
      }
      if (events & RDP_CONTINUE) {
        printf("ctx2 continue\n");
        continue;
      }
    }
  } else {
    printf("Wrong fd.");
    exit(1);
  }
  return 1;
}

int main() {
  int s;
  int efd, fd1, fd2;
  struct epoll_event ev, events[EPOLL_MAX_EVENTS];
  int n, i;
  int cnt1;

  ctx1 = rdpSocketCreate(1, "127.0.0.1", "8888");
  assert(ctx1);
  ctx2 = rdpSocketCreate(1, "127.0.0.1", "8889");
  assert(ctx2);

  conn1 = rdpNetConnect(ctx1, "127.0.0.1", "8889");
  assert(conn1);

  efd = epoll_create1(0);
  assert(efd);

  fd1 = rdpSocketGetProp(ctx1, RDP_PROP_FD);
  assert(fd1);

  fd2 = rdpSocketGetProp(ctx2, RDP_PROP_FD);
  assert(fd2);

  memset(&ev, 0, sizeof(ev));

  ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
  ev.data.fd = fd1;
  n = epoll_ctl(efd, EPOLL_CTL_ADD, fd1, &ev);
  assert(n != -1);

  ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
  ev.data.fd = fd2;
  n = epoll_ctl(efd, EPOLL_CTL_ADD, fd2, &ev);
  assert(n != -1);

  cnt1 = 1;
  for (;;) {
    if (cnt1 == 0)
      break;

    int timeout1 = 0, timeout2 = 0;
    timeout1 = rdpSocketIntervalAction(ctx1);
    timeout2 = rdpSocketIntervalAction(ctx2);
    if (timeout1 > timeout2)
      timeout1 = timeout2;
    n = epoll_wait(efd, events, EPOLL_MAX_EVENTS, timeout1);

    for (i = 0; i < n; i++) {
      ev = events[i];
      int fd = ev.data.fd;

      printf("epoll wait, got one event\n");

      if (ev.events & EPOLLERR) {
        assert(0);
      }
      if (ev.events & EPOLLHUP) {
        assert(0);
      }
      if (ev.events & EPOLLOUT) {
        assert(1);
      }
      if (ev.events & EPOLLIN) {
        if (processIn(fd) == 0) {
          goto exit;
        }
      }
    }
  }

exit:
  rdpSocketDestroy(ctx1);
  rdpSocketDestroy(ctx2);
}
