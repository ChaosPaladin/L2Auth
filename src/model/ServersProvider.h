#pragma once

#include "model/WorldServer.h"
#include <cstdint>

class ServersProvider  // TODO: wrap all operations from ServerPacketList and ServerKind
{
public:
    static int SetServerSocket(int IPaddress, SOCKET socket);                                                  // 0x004350FC
    static void SetServerStatus(int serverID, bool status);                                                    // 0x00435145
    static bool GetServerStatus(int serverId);                                                                 // 0x0043517B
    static WorldServer GetWorldServer(int serverId);                                                           // 0x004351B9
    static void SetServerUserNum(int outerServerIP, uint16_t usersOnline, uint16_t usersLimit, int serverId);  // 0x0043522B
};
