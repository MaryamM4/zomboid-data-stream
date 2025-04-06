
#include "TCP.h"
#include "UDP.h"
#include "messages.h"

// For server
#include "Observers.h"
#include "server/TCPServer.h"

#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using namespace std;

#ifndef NETWORKAPI_H
#define NETWORKAPI_H

struct Connection {
  // If A needs to be aware of B, send B's publicID, NOT B's sessionID
  uint32_t publicID;

  int sfd = -1;                    // TCP socket file descriptor
  struct sockaddr_in udpAddr = {}; // UDP address

  Connection(uint32_t publicID, int tcpSfd) : publicID(publicID), sfd(tcpSfd) {}

  Connection(uint32_t publicID, int tcpSfd, sockaddr_in udpAddr)
      : publicID(publicID), sfd(tcpSfd), inAddr(udpAddr) {}
};

class NetworkAPI {
public:
protected:
  uint32_t sessionID;

  virtual void handleIncomingMessage(const char *hdrMssg, const char *mssg) = 0;

  virtual void handleIncomingTCPHeader(const char *header) = 0;
  void handleIncomingUDPHeader(const char *header) = 0;
};

// ============================================================
// ------------------------------------------------------------

class ClientNetworkAPI : NetworkAPI {
  ClientNetworkAPI(char *host, char *tcpPort, char *udpPort) {
    tcpClient = TCPClient(host, tcpPort);
    udpClient = UDP(host, udpPort);
  }

  void start(char *server, char *serverPort_) {
    tcpClient.initSocket();
    tcpClient.connectToServer(server, serverPort);
    udpClient.initSocket();

    // Event header-reading event loops
    tcpClient.pollForHeader(-1, handleIncomingTCPHeader, sizeof(Header));
    udpClient.pollForHeader(-1, handleIncomingUDPHeader, sizeof(Header));

    // Client will not use thread for writing from buffers.
    // Just send-on-invoke.
  }

  void tcpPollSocket() {
    while
      true() {
        if (tcpClient.socketReadyToRead()) {
          // read
        }
      }
  }

  // Sends request over TCP
  // Recieves verification over TCP
  bool connectToGame(string gamename);

  // Send & recieve verification over TCP
  bool registerPlayer();

  // Send request & recieve list over TCP
  vector<String> getGameList();

  // @TODO: Returns true when server sends verification
  // Sends move over UDP, recieves verification over UDP
  bool sendMove(uint32_t xCoord, uint32_t yCoord) {
    Coord2D coord(xCoord, yCoord);
    SerializedMessage mssg(sessionID, coord);
    sendUDPMessage(mssg);
  }

  // Sends mssg over TCP
  void sendChat(char *chatMssg) {
    ChatMessage chatMessage(chatMssg);
    SerializedMessage mssg(sessionID, chatMessage);
    sendTCPMessage(mssg);
  }

private:
  TCPClient tcpClient;
  UDP udpClient;

  template <typename mssgStruct> void sendTCPMessage(mssgStruct mssg) {
    char *mssg = serialize(mssg);
    tcpClient.write(mssg);
  }

  template <typename mssgStruct> void sendUDPMessage(mssgStruct mssg) {
    char *mssg = serialize(mssg);
    tcpClient.write(mssg);
  }

  template <typename mssgStruct>
  sendVerification(bool verificationStatus, const mssgStruct &mssgToVerify);

  void handleIncomingUDPHeader(const char *header) override {
    struct Header hdr = deserialize<Header>(header);
    char *mssg = udpClient.read(hdr.senderID, mssgLength);
    handleIncomingMessage(hdr.mssgType, mssg);
  }

  void handleIncomingTCPHeader(const char *header) override {
    struct Header hdr = deserialize<Header>(header);
    char *mssg = tcpClient.read(hdr.senderID, mssgLength);
    handleIncomingMessage(hdr.mssgType, mssg);
  }

  void handleIncomingMessage(const char *hdrMssg, const char *mssg) {
    // @TODO
  }

}

// ============================================================
// ------------------------------------------------------------

class ServerNetworkAPI : NetworkAPI {
public:
  ServerNetworkAPI(char *host, char *tcpPort, char *udpPort) {
    sessionID = 0;
    tcpServer = TCPServer(host, tcpPort);
    udpServer = UDP(host, udpPort);
  }

  bool allEventsHaveCallbacks() {
    for (const auto eventKey : EventCode) {
      if (Observers.count(eventKey) == 0) {
        return false;
      }
    }
    return true;
  }

  template <typename... Args>
  void start(vector<pair<EventCode, std::function<void(Args...)>>> callbacks) {
    registerCallbacks(callbacks);

    if (!allEventsHaveCallbacks()) {
      cerr << "ServerNetworkAPI Error: Not all events have a registered "
              "callback."
           << endl;
      // @TODO: Error handling
    }

    tcpServer.initSocket();
    tcpServer.acceptConnections(); // Listen & accept(s)

    udpServer.initSocket();
    udpServer.bindSocket();

    // TCP & UDP objects resonsible for read-polling
    tcpServer.pollForHeader(-1, handleIncomingTCPHeader, sizeof(Header));
    udpServer.pollForHeader(-1, handleIncomingUDPHeader, sizeof(Header));

    // Start threads for TCP & UDP to periodically write queued mssgs.
    thread tcpWriteThread(runTCPWrite);
    thread udpWriteThread(runUDPWrite);

    tcpWriteThread.join();
    udpWriteThread.join();
  }

  // Register a callback for a specific event
  template <typename... Args>
  void registerCallback(EventCode, std::function<void(Args...)> callback) {
    if (callback) {
      Observer observer(callback);
      observers.registerObserver(eventType, observer);
    }
  }

  template <typename... Args>
  void registerCallbacks(
      vector<pair<EventCode, std::function<void(Args...)>>> callbacks) {
    if (callbacks) {
      for (const auto &callback : callbacks) {
        registerCallback(callback.first, callback.second);
      }
    }
  }

  template <typename... Args> void boradcastEvent(EventCode code, Args args);

private:
  TCPServer tcpServer;
  UDP udpServer;

  // Maps sessionID to client connections (TCP sfd, UDP addr)
  map<uint32_t, Connection> sessions;

  // Map event (subject) types to callback functions (observer(s))
  Observers observers;

  // Queue mssgs for sendings. Mssg should  already have headers.
  // First value is the SEND-TO sessionID. If negative, mssg will be broadcast.
  queue<pair<uint32_t, const SerializedMessage &>> udpMssgQueue;
  queue<pair<uint32_t, const SerializedMessage &>> tcpMssgQueue;
  int UDP_WRITE_DELAY_MS = 0.01; // @TODO: Replace polling with
  int TCP_WRITE_DELAY_MS = 0.1;  //        condition_variable.

  // Protect objects shared across threads.
  mutex sessionMutex;
  mutex udpQueueMutex;
  mutex tcpQueueMutex;

  // =======================================
  // 0 as a session ID is reserved for the server.
  bool validClientSessionID(uint32_t id) {
    if (id > 0) {
      lock_guard<mutex> lock(sessionMutex);
      if (session.count(id) > 0) {
        return true;
      }
    }
    return false;
  }

  // bool validPublicID(uint32_t id);  // publicID for Game

  // =======================================
  uint32_t determineNewSessionID() {
    lock_guard<mutex> lock(sessionMutex);

    // 0 is reserved for the server.
    for (uint32_t nextID = 1; nextID < UINT32_MAX; nextID++) {
      if (sessions.count(nextID) == 0) {
        return nextID;
      }
    }

    // throw std::runtime_error(
    //     "ServerNetworkAPI: Can't assign new sessionID, reached MAX.");
    return -1;
  }

  void startClientRegistration(int clientSfd) {
    // At this point:
    //  Client called TCP connect and UDP connect,
    //  and clientSFD = TCP server accepted a TCP client conn.

    // 1. Call uint32_t sessionID = GameServer::registerClient(...)
    uint32_t objectID; // To be determined by GameServer
    observers.notifyObservers(EventCode.Register, &objectID);

    // If registration denied, close connection and discontinue.
    if (objectID < 0) {
      tcpServer.closeConnection(clientSfd);
      return;
    }

    // Determine sessionID
    uint32_t sessionID = determineNewSessionID();
    if (sessionID < 0) {
      cerr << "ServerNetAPI can't assign new sessionID, reached MAX." << endl;
      return;
    }

    // Add sessionID & TCP sfd to map to not deny client as "unregistered".
    lock_guard<mutex> lock(sessionMutex);
    Connection newClient(objectID, sessionIclientSfd);
    sessions[sessionID] = newclient;

    // 2. TCP send sessionID
    Header hdr sendTCPMssg(sessionID, hdr);

    // 3. UDP addr = UDP recieve sessionID
    struct sockaddr_in udpAddr;
    char *udpMssg = udpServer.read(udpAddr);

    // @TODO: Add retries and timeout

    // -----------------------------  @TODO
    // 4. TCP send recieved ID
    // Client-TCP recieve ID
    // Client verifies it
    // Client-TCP send verification

    // 5. TCP recieve verification
    //    If false/timeout, retry.
    // -----------------------------

    // 6. Complete mapping
    // Update UDP connection value
    sessions[sessionID].udpAddr = udpAddr;
    sessions[sessionID].publicID = objectID;
  }

  void handleIncomingUDPHeader(const char *header) override {
    struct Header hdr = deserialize<Header>(hdrMssg);

    if (hdr.mssgType == EventCode::Register) {
      // sessionID invalid before registration. That's OK.
      startClientRegistration(hdr.senderID);

    } else {
      char *mssg = udpServer.read(hdr.senderID, mssgLength);
      handleIncomingMessage(hdr.mssgType, mssg);
    }
  }

  void handleIncomingTCPHeader(const char *header) override {
    struct Header hdr = deserialize<Header>(hdrMssg);
    if (hdr.mssgType == EventCode::Register) {
      // sessionID invalid before registration. That's OK.
      startClientRegistration(hdr.senderID);

    } else {
      char *mssg = tcpServer.read(hdr.senderID, hdr.mssgLength);
      handleIncomingMessage(hdr.mssgType, mssg);
    }
  }

  void handleIncomingMessage(uint32_t senderID, EventCode code, char *mssg) {
    if (!validSessionID(hdr.senderID)) {
      // @TODO: Log
      return;

      // Trigger callbacks observing event.
    } else if (hdr.mssgType == EventCode::Movement) {
      Coord2D coords = deserialize<Coord2D>(mssg);
      observers.notifyObservers(hdr.mssgType, coords.xCoord, coords.yCoord);

    } else if (hdr.mssgType == EventCode::Chat) {
      ChatMessage chatMssg = deserialize<ChatMessage>(mssg);
      observers.notifyObservers(hdr.mssgType, chatMssg.message);

    } else if (hdr.mssgType == EventCode::Event) {
      Action act = deserialize<Action>(mssg);
      observers.notifyObservers(hdr.mssgType, act.actionType, act.actionValue,
                                act.impactedID);

    } else if (hdr.mssgType == EventCode::Verification) {
      Verification vStatus = deserialize<Verification>(mssg);
      observers.notifyObservers(hdr.mssgType, vStatus.status);
    }
  }

  template <typename mssgStruct>
  void sendTCPMssg(uint32_t clientID, const mssgStruct &mssg) {
    lock_guard<mutex> lock(sessionMutex);

    auto it = sessions.find(clientID);
    if (it != sessions.end()) {
      tcpServer.writeTo(it->second.sfd, mssg);
    }
  }

  template <typename mssgStruct>
  void sendUDPMssg(uint32_t clientID, const mssgStruct &mssg) {
    lock_guard<mutex> lock(sessionMutex);

    auto it = sessions.find(clientID);
    if (it != sessions.end()) {
      udpServer.writeTo(it->second.inAddr, mssg);
    }
  }

  /**
   *  Broadcast a message to all except the sender, over TCP.
   */
  template <typename mssgStruct>
  void broadcastTCPMssg(uint32_t senderID, const mssgStruct &mssg) {
    lock_guard<mutex> lock(sessionMutex);

    for (auto conn = sessions.begin(); conn != sessions.end(); ++conn) {
      if (conn->first != senderID) {
        tcpServer.writeTo(conn->second.sfd, mssg);
      }
    }
  }

  /**
   *  Broadcast a message to all except the sender, over UDP.
   */
  template <typename mssgStruct>
  void broadcastUDPMssg(uint32_t senderID, const mssgStruct &mssg) {
    lock_guard<mutex> lock(sessionMutex);

    for (auto conn = sessions.begin(); conn != sessions.end(); ++conn) {
      if (conn->first != senderID) {
        udpServer.writeTo(conn->second, mssg);
      }
    }
  }

  // =======================================

  // TCP & UDP objects responsible for read-polling

  /**
   * Periodically deques message for tcp queue
   * and sends to relevant parties.
   *
   * If the SEND-TO sfd is negative, the mssg is broadcasted.
   * @TODO Use condition_variable to wake up thread only when queue is updated.
   */
  void runTCPWrite() {
    while (true) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(TCP_WRITE_DELAY_MS));

      while (auto mssg = dequeTCPMssg(); mssg.first != 0) {
        if (mssg.first < 0) {
          broadcastTCPMssg(sessionID, mssg.second);
        } else {
          sendTCPMssg(mssg.first, mssg.second);
        }
      }
    }
  }

  /**
   * Periodically deques message for udp queue
   * and sends to relevant parties.
   *
   * If the SEND-TO addr is negative, the mssg is broadcasted.
   * @TODO Use condition_variable to wake up thread only when queue is updated.
   */
  void runUDPWrite() {
    while (true) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(UDP_WRITE_DELAY_MS));

      while (auto mssg = dequeUDPMssg(); mssg.first != 0) {
        if (mssg.first < 0) {
          broadcastUDPMssg(sessionID, mssg.second);
        } else {
          sendUDPMssg(mssg.first, mssg.second);
        }
      }
    }
  }

  pair<uint32_t, const SerializedMessage &> dequeUDPMssg() {
    lock_guard<mutex> lock(udpQueueMutex);

    if (udpMssgQueue.empty()) {
      return {0, SerializedMessage{}};
    }

    pair<uint32_t, const SerializedMessage &> mssg = udpMssgQueue.front();
    udpMssgQueue.pop();
    return mssg;
  }

  pair<uint32_t, const SerializedMessage &> dequeTCPMssg() {
    lock_guard<mutex> lock(tcpQueueMutex);

    if (tcpMssgQueue.empty()) {
      return {0, SerializedMessage{}};
    }

    pair<uint32_t, const SerializedMessage &> mssg = tcpMssgQueue.front();
    tcpMssgQueue.pop();
    return mssg;
  }

  void enqueueTCPMessage(uint32_t sendToID, const SerializedMessage &mssg) {
    lock_guard<mutex> lock(tcpQueueMutex);
    tcpMssgQueue.push({sendToID, mssg});
  }

  void enqueueUDPMessage(uint32_t sendToID, const SerializedMessage &mssg) {
    lock_guard<mutex> lock(udpQueueMutex);
    udpMssgQueue.push({sendToID, mssg});
  }
};

#endif // NETWORKAPI_H

/*
Callback example:
// Example for GameServer
NetworkAPI networkAPI;
// Register a callback for 'M' (move message)
networkAPI.registerCallback('M', [](const char* mssg) {
    // Parse and handle the move message
    cout << "Handling move message: " << mssg << endl;
});

// Register a callback for 'C' (chat message)
networkAPI.registerCallback('C', [](const char* mssg) {
    // Parse and handle the chat message
    cout << "Handling chat message: " << mssg << endl;
});

*/
