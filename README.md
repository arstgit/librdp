# librtp

Reliable datagram protocol. More aggressive bandwidth usage.


## Prerequisites
Environment: 
  - Linux.

## Compile & Install

```
  $ make
  $ make install
```

## Usage

```c
// rdpSocketCreate() opens one fd internally.
rdpSocket *ctx = rdpSocketCreate(1, "127.0.0.1", "8888");

// Establish a connection.
rdpConn *conn = rdpNetConnect(ctx, "www.example.com", "8889");

int efd = epoll_create1(0);

// This is the fd rdpSocketCreate() opened.
int fd = rdpSocketGetProp(ctx, RDP_PROP_FD);

struct epoll_event ev, epollEvents[10];
ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
ev.data.fd = fd;
epoll_ctl(efd, EPOLL_CTL_ADD, fd1, &ev);

for (;;) {
  // rdpSocketIntervalAction() is needed to execute some time related
  // operations.
  int timeout = rdpSocketIntervalAction(ctx);

  n = epoll_wait(efd, epollEvents, 10, timeout);

  for (i = 0; i < n; i++) {
    ev = epollEvents[i];

    if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
      goto exit;
    }

    if (ev.events & EPOLLIN) {
      unsigned char buf[1024];
      size_t len = 1024;
      rdpConn *newConn;
      int events;

      for (;;) {
        // rdpReadPoll() gives us information about connection establishment, data arrivation, EOF etc.
        // It's safe stop invoking rdpReadPoll() and break the loop only after got an RDP_ERROR or RDP_AGAIN.
        ssize_t readCount = rdpReadPoll(ctx, buf, len, &newConn, &events);
        if (events & RDP_ERROR) {
          // Usually this is caused of some wrong arguments were passed to
          // rdpReadPoll().
          goto exit;
        }

        if (events & RDP_AGAIN) {
          // Here break the rdpReadPoll loop safely.
          break;
        }

        if (events & RDP_POLLOUT) {
          // Corresponding rdpConn can be fetched through newConn here.
          // Here can avoid getting an EAGAIN error when invoking rdpWrite().
          for (;;) {
            int n = rdpWrite(newConn, "echo something back", 19);
            if (n == -1 && errno == EAGAIN) {
              printf(
                  "rdpWrite EAGAIN, try again when got an RDP_POLLOUT again");
            }
            break;
          }
        }

        if (events & RDP_ACCEPT) {
          // Got an connection from the other end, usually it's from a client.
          //
          // Corresponding rdpConn can be fetched through newConn here.

          // rdpConnClose() must be invoked explicitly at some point after
          // usage.
          printf("new connection arrived.");
        }

        if (events & RDP_CONNECTED) {
          // rdpNetConnect() succeeded.
          //
          // Corresponding rdpConn can be fetched through newConn here.

          int n = rdpWrite(newConn, "hello.", 6);
          if (n == -1 && errno == EAGAIN) {
            printf("rdpWrite EAGAIN, try again when got an RDP_POLLOUT");
          }
        }

        if (events & RDP_DATA) {
          // Received data have been ordered and copied into buf
          // or an EOF arrived.
          //
          // Corresponding rdpConn can be fetched through newConn here.

          if (readCount > 0) {
            // Data arrived.
            printf("buf: %s, buf len: %d", buf, readCount);

            int n = rdpWrite(newConn, "hello.", 6);
            if (n == -1 && errno == EAGAIN) {
              printf("rdpWrite EAGAIN, try again when got an RDP_POLLOUT");
            }
          }

          if (readCount == 0) {
            // Got an EOF from the other end.
            printf("EOF");

            rdpConnClose(newConn);
          }
        }

        if (events & RDP_CONTINUE) {
          // Continue, RDP_CONTINUE means rdpReadPoll() must be invoked again.
          continue;
        }
      }
    }
  }
}

exit: 
  rdpSocketDestroy(ctx);
```


## Test
```
  $ make clean && make test
```
