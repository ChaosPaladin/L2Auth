#pragma once

#include "model/FrameType.h"
#include "threads/CRWLock.h"

#include <cstdint>

struct WorldServer;

class ServerPacketList
{
public:
    ServerPacketList();           // 0x00435FAF
    virtual ~ServerPacketList();  // 0x00435FF8

    void Init(FrameType frameType, int serverKindIndex);                                                // 0x00436017
    bool ExchangeServePacketList(FrameType frameType, int serverKind, int usedBuffSize, char* buffer);  // 0x004369B6

    void MakeServerListFrame(const WorldServer& worldServer);  // 0x00436084
    void SetServerCount();                                     // 0x00436576
    int GetServerPacketList(char** buffer);                    // 0x00436987

    void SetServerStatus(int serverId, bool status);                                 // 0x004365A3
    bool GetServerStautsKind1(int serverId) const;                                   // 0x00436A39
    void SetServerUserNum(int serverId, uint16_t usersOnline, uint16_t usersLimit);  // 0x00436783

private:
    void MakeServerKind0(const WorldServer& worldServer);  // 0x0043612C
    void MakeServerKind1(const WorldServer& worldServer);  // 0x004361DF
    void MakeServerKind2(const WorldServer& worldServer);  // 0x004362E7
    void MakeServerKind3(const WorldServer& worldServer);  // 0x00436452

    void SetServerStatusKind1(int serverId, bool status);  // 0x00436624
    void SetServerStatusKind2(int serverId, bool status);  // 0x00436699
    void SetServerStatusKind3(int serverId, bool status);  // 0x0043670E

    void SetServerUserNum1(int serverId, uint16_t usersOnline, uint16_t usersLimit);  // 0x00436804
    void SetServerUserNum2(int serverId, uint16_t usersOnline, uint16_t usersLimit);  // 0x00436885
    void SetServerUserNum3(int serverId, uint16_t usersOnline, uint16_t usersLimit);  // 0x00436906

    bool GetServerStautsKind2(int serverId) const;  // 0x00436AC4

private:
    mutable CRWLock m_rwLock;
    int m_fameCount;
    char m_buff[2048];
    int m_usedBuffSize;
    int m_serverKindIndex;
    FrameType m_frameType;
};
