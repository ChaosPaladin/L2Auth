#pragma once

#include "model/FrameType.h"
#include "model/WorldServer.h"
#include "threads/CRWLock.h"

#include "utils/cstdint_support.h"
#include <vector>

class ServerPacketList;

class ServerKind
{
public:
    ServerKind();           // 0x00436B4F
    virtual ~ServerKind();  // 0x00436BC3

    void LoadServerList();                                                                     // 0x00436CD0
    int GetServerCount() const;                                                                // 0x00437250
    WorldServer GetAt(int serverId) const;                                                     // 0x00437261
    int SetServerSocket(int IPaddress, SOCKET socket);                                         // 0x004372E1
    void SetServerStatus(int serverId, bool status);                                           // 0x0043738C
    void SetServerUserNum(int serverId, uint16_t usersOnline, uint16_t usersLimit);            // 0x0043741A
    void ReloadServerList();                                                                   // 0x004374A8
    int GetServerMaxKind() const;                                                              // 0x00437F57
    int GetServerPacketList(FrameType frameFormat, int serverKindIndex, char** buffer) const;  // 0x00437F68
    bool GetServerStatus(int serverId) const;                                                  // 0x0043808B

private:
    void MakeServerListFrame();   // 0x00437BB6
    void CheckAllServerStatus();  // 0x00437FCA

private:
    int m_serverKindsCount;
    typedef std::vector<WorldServer> WorldServers;
    WorldServers m_servers;
    ServerPacketList** m_serverFrames[4];
    int m_serverCount;
    mutable CRWLock m_rwLock;
};

extern ServerKind g_serverKind;
