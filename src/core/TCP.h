#ifndef TCP_H
#define TCP_H

#include <arpa/inet.h> // inet_ntoa
#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h> // For checking if socket actually has data
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h> // writev()
#include <unistd.h>  // write()

#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace std;

class TCP {
public:
  TCP(char *host_, char *port_);
  virtual ~TCP();

  // Server and Client share 'socket' and 'close' network calls,
  // but need different 'socket' implementations.
  virtual bool initSocket() = 0;
  virtual void closeConnection() { close(this->sockfd); }

  // https://linux.die.net/man/2/poll
  bool socketReadyToRead(int sfd) const {
    struct pollfd poll_fd;
    poll_fd.fd = sfd;
    poll_fd.events = POLLIN;

    // 1: Number of sockets to poll.
    // 0: Timeout of 0 makes it non-blocking.
    int pollStatus = poll(&poll_fd, 1, 0);

    if (pollStatus < 0) {
      cerr << "Error: TCP poll(sfd = " << sfd << ")." << endl;

    } else if (poll_fd.revents & POLLIN) {
      // revents is returned events after poll(). If it
      // contains POLLIN, that means there's data to read.
      return true;
    }

    return false;
  }

  bool sfdIsValid(int sfd) const {
    if (sfd <= 0) {
      return false;
    }

    if (fcntl(sfd, F_GETFD) == -1 && errno == EBADF) {
      return false; // Socket not open.
    }

    int err = 0; // Check socket status:
    socklen_t len = sizeof(err);
    if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || err != 0) {
      return false;
    }

    return true;
  }

  bool writeTo(int sfd, const char *data) const {
    if (data == nullptr) {
      return false;
    }

    if (!sfd_is_valid(sfd)) {
      cerr << "TCP writeTo: Cannot write to closed or invalid socket (sfd = "
           << sfd << ")." << endl;
      return false;
    }

    try {
      ssize_t bytes_written = write(sfd, data, strlen(data));

      // Check for errors during write:
      if (bytes_written < 0) {
        std::cerr << "write_to error: (" << errno << "): " << strerror(errno)
                  << std::endl;

      } else if (bytes_written < strlen(data)) {
        std::cerr << "TCP writeTO Error: Not all data was written to client. "
                     "Expected: "
                  << strlen(data) << ", Written: " << bytes_written
                  << std::endl;

      } else {
        return true;
      }

    } catch (...) { // Potentially bad sfd input.
      std::cerr << "TCP writeTo exc error." << std::endl;
    }

    return false;
  }

  char *readFrom(int sfd, size_t bytesToRead) const {
    char *mssg = new char[bytesToRead];

    ssize_t totalRead = 0;

    try {
      while (totalRead < bytesToRead) {
        ssize_t bytesRead =
            read(sfd, mssg + totalRead, bytesToRead - totalRead);

        if (bytesRead == 0) {
          // This can indicate the sender is done,
          break; // so not neccessarily a failure.

        } else if (bytesRead < 0) {
          std::cerr << "TCP readFrom error." << std::endl;
          break;
        }

        totalRead += bytesRead;
      }

    } catch (...) {
      std::cerr << "TCP readFrom exc error.";
    }

    return mssg;
  }

protected:
  // TCPClient: Hostname/IP of the server.
  // TCPServer: Optional. Can use INADDR_ANY, or
  //            can use hot to bind to a specific interface
  char *host = nullptr;

  // TCPClient: Client needs to know where server is accepting conn.
  // TCPServer: Specify which port to bind to, where
  //            it accepts incoming client connections.
  char *port = nullptr;

  // Socket field descriptor.
  // TCPClient: Represents connection w/ the server for reading-from and
  // writing-to. TCPServer: Listening socket used for creating new connections.
  //            Will need additional storage for new client sockets
  //            (client_fds).
  int sfd = -1;
};

#endif // TCP_H
