#include <cstdint>
#include <cstring> // For memcpy
#include <netinet/in.h>
#include <vector>

#include "events.h"

using namespace std;

#ifndef MESSAGES_H
#define MESSAGES_H

/*
template <typename mssgStruct>
mssgStruct deserialize(unsigned char *message) {
  struct mssgStruct *mssg = (struct mssgStruct *)message;
  return mssg;
}
*/

// Reference:
// https://stackoverflow.com/questions/19201488/converting-struct-to-char-and-back

template <typename mssgStruct> mssgStruct deserialize(unsigned char *message) {
  mssgStruct mssg;
  std::memcpy(&mssg, message, sizeof(mssg));
  return mssg;
}

template <typename mssgStruct>
unsigned char *serialize(const mssgStruct &message) {
  unsigned char *mssg = new unsigned char[sizeof(message)];
  memcpy(mssg, &message, sizeof(message));
  return mssg;
}

/**
 * Full message (Header + Message) stored in serialized form.
 * For handling de/serialization for reading/writing over the network.
 */
struct SerializedMessage {
  unsigned char *header = nullptr;
  unsigned char *message = nullptr;

  template <typename mssgStruct>
  SerializedMessage(uint32_t senderID, const mssgStruct &mssg) {
    Header hdrStruct = Header(senderID, mssg);
    header = serialize(hdrStruct);

    message = serialize(mssg);
  }

  SerializedMessage(unsigned char *hdr, unsigned char *mssg)
      : header(hdr), message(mssg) {}

  /**
   * Assumes header comes first.
   */
  SerializedMessage(unsigned char *fullMessage) {
    Header hdr;
    std::memcpy(&hdr, fullMessage, sizeof(Header));
    header = new unsigned char[sizeof(Header)];
    std::memcpy(header, fullMessage, sizeof(Header));

    size_t messageSize = hdr.mssgLength;
    message = new unsigned char[messageSize];
    std::memcpy(message, fullMessage + sizeof(Header), messageSize);
  }

  ~SerializedMessage() {
    delete[] header;
    delete[] message;
  }

  template <typename mssgStruct> mssgStruct getMessage() {
    return deserialize(message);
  }

  Header getHeader() { return deserialize(header); }

  bool mssgTypeMatches() {
    return (header.messageType == getDeserializedMessage().getType());
  }
};

// MUST be the first part of any message.
struct Header {
  uint32_t senderID = 0;   // Or sessionID (32 bits)
  EventCode mssgType;      // Type of message (enum)
  uint32_t mssgLength = 0; // Length of following message in bytes (32 bits)

  Header(EventCode type, uint32_t id, uint32_t length)
      : mssgType(type), senderID(id), mssgLength(length) {}

  template <typename mssgStruct>
  Header(uint32_t id, const mssgStruct &mssg)
      : mssgType(mssg.getType()), senderID(id), mssgLength(mssg.size()) {}

  size_t getSerializedSize() const {
    return sizeof(senderID) + sizeof(mssgLength) + messageType.size() + 1;
  }
};

// https://stackoverflow.com/questions/44261983/c-struct-implement-derived-interface
struct MessageProperties { // Struct Interface.
  virtual EventCode getType() const = 0;
  virtual size_t size() const = 0; // Size of mssg in bytes.

  virtual ~MessageProperties() = default;
};

// @TODO
// Track what the verification is for with requestIDs - Requires management
// OR
// append with the message that was verified - Network congestion
struct Verification : public MessageProperties {
  bool status;

  Verification(bool status) : status(status) {}

  EventCode getType() const override { return EventCode::Verification; }
  size_t size() const override { return sizeof(status); }
};

struct ChatMessage : public MessageProperties {
  string message;
  ChatMessage(const std::string &mssg) { message = mssg; }

  EventCode getType() const override { return EventCode::Chat; }
  size_t size() const override { return message.length(); }
};

struct Coord2D : public MessageProperties {
  // If A is responsible for sending B's coords,
  //  changing the header senderID can cause issues,
  // such as A spawning B, and B not being registered in the server.
  uint32_t objectID;
  uint32_t xCoord;
  uint32_t yCoord;

  Coord2D(unit32_t id, uint32_t x, uint32_t y)
      : objectID(id), xCoord(x), yCoord(y) {}

  EventCode getType() const override { return EventCode::Coord2D; }
  size_t size() const override { return sizeof(xCoord) + sizeof(yCoord); }
};

struct Action : public MessageProperties {
  // If A spawned B, and B commited action on C,
  // it will be as if A commited on C. Deduce impactor from header.

  // If a client machine is in charge of a mob,
  // then it needs to registered the mob,
  // and use the mob's publicID for the header's sessionID.
  uint8_t actionType;
  uint32_t actionValue;
  uint32_t impactedID = 0;

  Action(uint8_t actType, uint32_t actVal)
      : actionType(actType), actionValue(actVal) {}

  Action(uint8_t actType, uint32_t actVal, uint32_t idOfImpacted)
      : actionType(actType), actionValue(actVal), impactedID(idOfImpacted) {}

  EventCode getType() const override { return EventCode::Action; }

  size_t size() const override {
    return sizeof(actionType) + sizeof(actionValue) + sizeof(impactedID);
  }
};

#endif // MESSAGES_H