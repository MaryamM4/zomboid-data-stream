#include <cstdint>

#ifndef EVENTS_H
#define EVENTS_H

enum class EventCode : uint8_t {
  Register = 'R',
  Verification = 'V', // Verification message
  Chat = 'C',         // Chat message
  Location = 'L',     // Location update (sent by server)
  Movement = 'M',     // Movement (send by clients)
  Action = 'A'        // Action (e.g., attack, interact)
};

#endif // EVENTS_H