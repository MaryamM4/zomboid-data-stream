# zomboid-data-stream (2024)
Client-server data streaming framework tailored for real-time multiplayer environments. 
Originally designed for a 2D multiplayer game, the NetworkAPI serves as a modular event-driven 
communication layer that supports both TCP and UDP protocols for message delivery.

# Code Compilation & Execution
**Files you need:** 
Both will need files under the “core” directory. Only the server-side will need the files under the 
“server” directory, and only the client-side will need the files under the “client” directory.

**Compilation:** 
Use the linux command to compile the NetworkAPI (if applicable, replace “client” with “server”):
``g++  src/core/*.cpp src/client/* .cpp -o client``

**Execution:** 
Unfortunately, the GameModules were not completed, and the NetworkAPIs are interfaces that cannot be executed stand-alone. 
However, NetworkAPI provides the means of integrating into other modules (callbacks for incoming messages, 
invoking public methods for sending messages), and documentation is pending to make it easier to do so. 


# Network Protocol 
The protocol uses a **client-server architecture** because it simplifies game management. 
Having a server manage shared resources such as map generation, mob spawning, status management, 
player movement/action verification is simpler with a server rather than over P2P.
The server stores a TCP and a UDP connection for each client.

## TCP/UDP Uses:
TCP ensure reliable message delivery, so it's used for important messsages. On the other hand, UDP is used for time-sentisitive updates. 

| **TCP**  | **UDP**  |
|--------------------------------------------------------------------------------------------|-----------------------------------------------------|
| Game setup, including player registration, creating/joining games                          | Player/Mob movements and actions                    |
| Important events like player/mob deaths and score updates for the leaderboard              | Damage updates                                      |
| Mob spawning                                                                               |                                                     |
| Chat messages                                                                              |                                                     |

## Connection Setup
![image](https://github.com/user-attachments/assets/39e5def4-54bc-473f-84a8-7b8b4c006876)

## Message Protocol
When the NetworkAPI sends a message, it must be one of the provided formats 
(see messages.h for the message structures). Every message must be preceded by a header, 
since the socket-readings poll for the header size and determines how the remaining will be handled.

 ![image](https://github.com/user-attachments/assets/fd1462d9-22a8-4617-bdb8-f714548f966d)


Both the Header and the Message will be packed into a SerializedMessage struct, 
which provides de/serialization functions for sending/receiving the packet over the network. 

**For example:** A player sends a Move message with mssgType=”M”, and the server broadcasts to all 
other players a Move message with mssgType=”L” (for location). 

## Suggestions
- When it comes to projectiles, the server should only broadcast the projectile’s original spawn point and rotation. 
  Since the path is pre-set, the GameClient should just render its movement. 
- Collision-detection (such as projectiles hitting mobs, and calculating damage & cause)
  should be done on the server-side to avoid inconsistency between clients. 

# API Usage
**Receiving:** The NetworkAPI is inspired by event-based design and the observer pattern (especially the server-side, 
which can’t afford to poll each client). In the initialization stage, the Game modules need to register callback methods for each event.

For example, say the GameServer wants to “observe” a “registration event”. Thus, it 
provides ServerNetAPI a callback for its own RegisterPlayer(…) function, associated with _EventCode.Register_.

_A current flaw in the design is that the NetworkAPI is “aware” of the parameters of each callback, _
_so the GameModules need to adhere to the API’s format.  _

**Sending:** Additionally, GameModules can invoke send commands on their APIs:

![image](https://github.com/user-attachments/assets/4095e846-ebf9-4500-9a35-1a9d9be8e649)


# Zomboid (Not Implemented)
Zomboid2D is a simple top-down zombie-shooter game. When a player creates a game, 
the server handles map generation and spawning hostile mobs. Players can attack mobs with ranged weapons, 
while mobs deal melee damage to players. 
Points are awarded for mob kills. When a player dies, their points reset, but the leaderboard retains their high score. 
If a player rejoins the game, their leaderboard score updates only if they exceed their previous high score. 
Once no players are left alive, the game ends.

![image](https://github.com/user-attachments/assets/ae132cee-5e1f-4b88-9667-a0687b637539)

The NetworkAPI can support the following features:
- Player registration/game connection, or leaving/disconnecting.
- Mob/player movement/location and actions.
- Chat messaging.
- Leaderboard system.
