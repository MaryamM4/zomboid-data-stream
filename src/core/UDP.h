#ifndef UDP_H
#define UDP_H

#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

class UDP {
public:
  UDP(char *host_, char *port_) host(host_), port(port_);
  ~UDP();

  void initSocket() {
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0) {
      cerr << "UDP failed to init socket." << endl;
      // @TODO: Error handling
    }

    // Fill out server info
    memset(&writeToAddr, 0, sizeof(writeToAddr));
    servaddr.sin_family = AF_UNSPEC; // IPv4 or IPv6
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(host);
    socklen_t servaddr_len = sizeof(writeToAddr);

    // make socket non-blocking?
  }

  // Only needed for server.
  // For client, it lets them use send() instead of sendto().
  void bindSocket() {
    memset(&writeToAddr, 0, sizeof(writeToAddr));

    // Bind the socket with the server address
    if (bind(sfd, (const struct sockaddr *)&writeToAddr, sizeof(writeToAddr)) <
        0) {
      cerr << "UDP bind failed: (" << errno << ") " << strerror(errno) << "."
           << endl;
      close(sfd);
    }
  }

  // @TODO Could probably just make client bind and use the proper send()
  int write(const char *mssg, size_t mssgLen) {
    if (!writeToAddr) {
      cerr << "UDP cannot send to unitialized writeToAddr." << endl;
      return -1;
    }

    int sendStatus =
        sendto(sfd, mssg, mssgLen, 0, (const struct sockaddr *)&writeToAddr,
               sizeof(writeToAddr));
    return sendStatus;
  }

  int writeTo(struct sockaddr_in addr;, const char *mssg, size_t mssgLen) {
    int sendStatus = sendto(sfd, mssg, mssgLen, 0,
                            (const struct sockaddr *)&addr, sizeof(addr));
    return sendStatus;
  }

  char *read(size_t bytesToRead) {
    char[bytesToRead] mssg;
    int recStatus =
        recvfrom(sfd, mssg, bytesToRead, 0, &writeToAddr, sizeof(writeToAddr));
  }

  /**
   * Returns mssg, and fills out udpConn values
   * with address of sender.
   *
   * Can help server track clients.
   */
  char *read(struct sockaddr_in addr;, size_t bytesToRead) {
    char mssg[bytesToRead];
    int recStatus = recvfrom(sfd, mssg, bytesToRead, 0, &addr, sizeof(addr));
    return mssg;
  }

  // Same as above, but returns udpConn and fills out message.
  sockaddr_in read(char *mssg, size_t bytesToRead) {
    struct sockaddr_in addr;
    int recStatus = recvfrom(sfd, mssg, bytesToRead, 0, &addr, sizeof(addr));
    return addr;
  }

  /**
   * Set timeout to negative for no timeout.
   */
  void pollForHeader(int timeout, void (*readHeaderCallback)(const char *),
                     size_t hdrLength) {

    int ready;
    char hdrBuffer[hdrLength];
    ssize_t s;

    struct pollfd pfds[1];
    pfds[0].fd = sfd;
    pfds[0].events = POLLIN;

    while (true) {
      int numBytes = poll(pfds, 1, timeout);

      if (numBytes < 0) {
        cerr << "UDP poll error for socket " << pfd.fd << "." << endl;
        // @TODO: Handle error.

      } else if (numBytes > 0) {
        char *header = read(hdrLength);
        readHeaderCallback(header);
      }
    }
  }

  void closeConnection() { close(this->sockfd); }

protected:
  const char *host;
  const char *port;
  int sfd; // Socket is shared for ALL in/out communication.

  // For a client, this would be the server address.
  struct sockaddr_in writeToAddr;

  // Have NetworkAPI handle client storage
};

#endif // UDP_H

/*
NOTES:

-----------------------------------
UDP is connectionless;
a server only listens on a socket for incoming mssggrams from any client.
However, a UDP server may listen on multiple sockets (different ports or
interfaces), & each socket is represented by a file descriptor.

For an example on polling multiple sockets, see:
- https://man7.org/linux/man-pages/man2/poll.2.html


while (true) {
    for (pollfd pfd : pfds) {
        int numBytes = poll(pfds, 1, timeout);
        // ...
    }
}

-----------------------------------

*/