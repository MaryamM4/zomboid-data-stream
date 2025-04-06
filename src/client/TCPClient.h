#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "../core/TCP.h"

class TCPClient : public TCP {
public:
  TCPClient(char *host_, char *port_) : host(host_), port(port_);

  /**
   * @brief Resolves the server's address and established TCP connection.
   *
   * Network calls: (1) socket, and (2) connect.
   */
  bool initSocket() override {
    int sfd = -1;
    int MAX_ATTEMPTS = 3; // Allow a few failures.

    try {
      int attemptCount = 0;

      while (sfd < 0 && attemptCount < MAX_ATTEMPTS) {
        attemptCount++;

        // hints: Instructions on connections to look for.
        // results: Pointer to a linked list of addrinfo structs
        //          (currently empty)
        // result_ptr: For iterating through results.
        struct addrinfo hints, *results, *result_ptr;

        memset(&hints, 0, sizeof hints); // Init. hints as empty.
        hints.ai_family = AF_UNSPEC;     // IPv4 &/or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP

        // Returns a list of address structures (stored in results).
        int status = getaddrinfo(this->host, this->port, &hints, &results);

        if (status != 0) { // Non-zero status indicates failure.
          cerr
              << "TCP Client failed to get a list of address structures (host: "
              << std::string(this->host) + ", port: " << std::string(this->port)
              << ").\n"
              << "Cause of Error: " + std::string(gai_strerror(status)) + "."
              << endl;

        } else {
          // First result might not be valid. Look in results for valid conn.
          for (result_ptr = results; result_ptr != nullptr;
               result_ptr = result_ptr->ai_next) {
            // Try each result until a succesful connection (2).

            sfd = socket(result_ptr->ai_family, result_ptr->ai_socktype,
                         result_ptr->ai_protocol);

            if (sfd == -1) {
              // If the socket fails, try the next address
              continue; // (Don't try connecting)
            }

            if (connect(sfd, result_ptr->ai_addr, result_ptr->ai_addrlen) !=
                -1) {
              break; // Success. Stop iterating through results.
            }

            close(sfd); // Failed. Close this socket and try the next addr.
          }
        }

        if (result_ptr == nullptr) { // No address succeeded.
          cerr << "TCP Client could not find an available address (host: "
               << std::string(this->host)
               << ", port: " + std::string(this->port) << ").\n"
               <<

              "Cause of Error: " << std::string(gai_strerror(status)) << "."
               << endl;

          sfd = -1; // Give some time before next retry.
          std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        if (results) {           // Entire list of results no longer needed.
          freeaddrinfo(results); // Free the memory.
        }
      }

    } catch (...) {
      cerr << "TCPClient failed to initialize the socket." << endl;
    }

    this->sockfd = sfd;
    return (sfd > 0);
  }

  bool write(const char *data) const { return writeTo(sfd, data); }

  char *read(int bytesToRead) const { return readFrom(sfd, bytesToRead); }

  bool socketReadyToRead() { return socketReadyToRead(this.sfd); }
};

#endif // TCPCLIENT_H