# librtp

Reliable datagram protocol based on UDP. More aggressive packet sending strategy.


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
// Note: RDP don't have automatic connection destroy mechanism. Once user get a connection handle, from the invocation of rdpNetConnect() or rdpReadPoll() with RDP_ACCEPT event,  it's always user's job to invoke rdpConnClose() to destroy that connection.

// rdpSocketCreate() opens one fd internally.
rdpSocket *ctx = rdpSocketCreate(1, "127.0.0.1", "8888");

// Establish a connection.
rdpConn *conn = rdpNetConnect(ctx, "www.example.com", "8889");

// Arbitrary user data pointer can be attached to rdpConn.
// User can retrive the user data whenever needed. Usually after getting a RDP_DATA/RDP_ACCEPT/RDP_CONNECTED/RDP_CONN_ERROR.
void * userData = NULL;
rdpConnSetUserData(conn, userData);
assert(rdpConnGetUserData(c) == userData);

int efd = epoll_create1(0);

// This is the actual communicating fd rdpSocketCreate() using.
int fd = rdpSocketGetProp(ctx, RDP_PROP_FD);

struct epoll_event ev, epollEvents[10];
ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
ev.data.fd = fd;
epoll_ctl(efd, EPOLL_CTL_ADD, fd1, &ev);

for (;;) {
  // rdpSocketIntervalAction() is needed to do some internal actions in time.
  // Returns a timeout in milliseconds, indicating the time interval to next invoke. Usually passing it to epoll_wait is enough.
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
        // rdpReadPoll() gives us information about connection establishment, data arrival, EOF etc.
        // User should invoke it over and over again until getting a RDP_ERROR or RDP_AGAIN, that's the only safe point jumping out the loop.
        ssize_t readCount = rdpReadPoll(ctx, buf, len, &newConn, &events);
        if (events & RDP_ERROR) {
          // Unexpected error, invalid arguments and internal fault can be the cause.
          // User should check the arguments passing to rdpReadPoll().
          goto exit;
        }

        if (events & RDP_AGAIN) {
          // RDP has drained the underground fd.
          // It's time to break the loop safely.
          break;
        }

        if (events & RDP_CONN_ERROR) {
          // Connection have errors, usually because of receiving a RESET packet.
          // rdpConnClose() should be invoked here.

          userData = rdpConnGetUserData(conn);
          // Do some sweeping if needed.

          if (rdpConnClose(conn) == -1) {
            // Error occured.
          }
          
          continue;
        }

        if (events & RDP_POLLOUT) {
          // Indicate RDP have enough buffer space stroing output data now.
          // It's time to write remaining data to sent if there is.
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
          // New connection incoming.

          struct sockaddr_storage addr;
          socklen_t len;
          // Fetch the address of the other end.
          rdpConnGetAddr(newConn, (struct sockaddr *)&addr, &len);

          // Do something.

          // rdpConnClose() should be invoked here or somewhere else explicitly.
        }

        if (events & RDP_CONNECTED) {
          // New connection established.

          int n = rdpWrite(newConn, "hello.", 6);
          if (n == -1 && errno == EAGAIN) {
            printf("rdpWrite EAGAIN, try again when got an RDP_POLLOUT");
          }
        }

        if (events & RDP_DATA) {
          // Data or EOF arrived.

          if (readCount > 0) {
            // Data arrived.
            printf("buf: %s, buf len: %d", buf, readCount);

            int n = rdpWrite(newConn, "hello.", 6);
            if (n == -1 && errno == EAGAIN) {
              printf("rdpWrite EAGAIN, try again when got an RDP_POLLOUT");
            }
          }

          if (readCount == 0) {
            // Received an EOF from the other end.
            printf("EOF");

            rdpConnClose(newConn);
          }
        }

        if (events & RDP_CONTINUE) {
          // Invoke rdpReadPoll() again immediately.
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
