#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "../core/TCP.h"
#include "../core/messages.h"

#include <functional>
#include <string>
#include <thread>
#include <vector>

class TCPServer : public TCP {
public:
  TCPServer(char *host_, char *port_) : host(host_), port(port_);

  bool initSocket() override {
    int sfd_ = -1;
    try {
      struct addrinfo hints, *result, *result_ptr;

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
      hints.ai_socktype = SOCK_STREAM; // TCP

      // If the host is provided, resolve and bind to it.
      string bindingError = "No host provided.";
      if (this->host != nullptr) {
        int status = getaddrinfo(this->host, this->port, &hints, &result);

        if (status != 0) {
            cerr << "TCP server failed to resolve host " << std::string(this->host) << "'. Error: ") << gai_strerror(status);
        }

        // Loop through results and try to bind
        for (result_ptr = result; result_ptr != nullptr;
             result_ptr = result_ptr->ai_next) {
          sfd_ = socket(result_ptr->ai_family, result_ptr->ai_socktype,
                        result_ptr->ai_protocol);
          if (sfd_ == -1) {
            continue; // Try the next result
          }

          // SO_REUSEADDR allows immediatley re-using a port.
          const int on = 1; // Set after creating socket and before binding.
          if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                         sizeof(int)) < 0) {
            cerr << "TCP Server setsockopt SO_REUSEADDR failed: (" << errno
                 << ") " << strerror(errno) << endl;
          }

          if (::bind(sfd, result_ptr->ai_addr, result_ptr->ai_addrlen) == -1) {
            close(sfd); // Binding failed, close and try next result
            continue;
          }

          // SUCCESSFULL binding, discontinue search.
          this->sfd_ = sfd;
          break;
        }

        if (result_ptr == nullptr) {
          cerr << "Server failed to bind to any resolved address for host '"
               << std::string(this->host) << "' and port '"
               << std::string(this->port) << "'" << endl;
        }

        if (result) {
          freeaddrinfo(result); // Free memory
        }
      }

      if (sfd_ < 0) {
        std::cerr << "Server will attempt to bind to all available network "
                     "interfaces."
                  << std::endl;

        sockaddr_in acceptSockAddr;
        memset(&acceptSockAddr, 0, sizeof(acceptSockAddr)); // Zero out struct
        acceptSockAddr.sin_family = AF_INET; // Address Family Internet
        // acceptSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Bind to all
        // available interfaces (DOESN't WORK!)
        acceptSockAddr.sin_addr.s_addr = htonl(AF_UNSPEC); // Allow IPv4 or IPv6
        acceptSockAddr.sin_port = htons(stoi(this->port)); // Port

        // [1.1] Open TCP socket
        sfd_ = socket(acceptSockAddr.sin_family, hints.ai_socktype, 0);
        if (sfd_ == -1) { // Discontinue on error
          // throw std::runtime_error("Server failed to create socket.");
          cerr << "TCP Server failed to create socket. Error: ("
               << std::to_string(errno) + ") " << std::string(strerror(errno));

        } else {

          // SO_REUSEADDR allows immediatley re-using a port.
          const int on = 1; // Set after creating socket and before binding.
          if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                         sizeof(int)) < 0) {
            cerr
                << "TCP Server (bind_to_all_available) setsockopt SO_REUSEADDR "
                   "failed: ("
                << errno << ") " << strerror(errno) << std::endl;

            // [2] Bind the socket
          } else if (::bind(sfd, (sockaddr *)&acceptSockAddr,
                            sizeof(acceptSockAddr)) == -1) {
            // Discontinue on error.
            cerr << "TCP Server (bind to all) failed to bind socket. Error: ("
                 << std::to_string(errno) + ") "
                 << std::string(strerror(errno));
          }
        }

        this->sfd = sfd_;
      }

    } catch (...) {
      std::cerr << "Failed to start the TCP server." << std::endl;
      return false;
    }

    return (this->sfd > 0);
  }

  // Listen & Accept-Loop
  bool acceptConnections();
};

#endif // TCPSERVER_H