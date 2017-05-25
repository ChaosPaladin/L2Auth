#include "model/ServerPacketList.h"
#include "model/ServerKind.h"
#include "model/WorldServer.h"

#include <cstdio>
#include <cstring>

// 0x00435FAF
ServerPacketList::ServerPacketList()
    : m_rwLock()
    , m_fameCount()
    , m_usedBuffSize(0)
    , m_serverKindIndex(0)
    , m_frameType(FrameType_DefaultOrCompressed)
{
}

// 0x00435FF8
ServerPacketList::~ServerPacketList()
{
}

// 0x00436017
void ServerPacketList::Init(FrameType frameType, int serverKindIndex)
{
    memset(m_buff, 0, 0x800u);
    m_serverKindIndex = serverKindIndex;
    m_frameType = frameType;

    if (m_frameType != FrameType_DefaultOrCompressed)
    {
        m_usedBuffSize = 2;  // initial offset
    }
    else
    {
        m_usedBuffSize = 1;  // initial offset
    }

    m_fameCount = 0;
}

// 0x00436084
void ServerPacketList::MakeServerListFrame(const WorldServer& worldServer)
{

    switch (m_frameType)
    {
        case FrameType_GameClient:
            MakeServerKind1(worldServer);
            break;
        case FrameType_Extended:
            MakeServerKind2(worldServer);
            break;
        case FrameType_WithKind:
            MakeServerKind3(worldServer);
            break;
        case FrameType_DefaultOrCompressed:
        case FrameType_Compressed:
            MakeServerKind0(worldServer);
            break;
    }
}

// 0x0043612C
void ServerPacketList::MakeServerKind0(const WorldServer& worldServer)
{
    m_rwLock.WriteLock();
    ++m_fameCount;

    int index = m_usedBuffSize;

    m_buff[index++] = worldServer.serverId;
    memcpy(&m_buff[index], &worldServer.outerIP, 4u);
    index += 4;

    memcpy(&m_buff[index], &worldServer.serverPort, 4u);
    m_usedBuffSize = index + 4;

    m_rwLock.WriteUnlock();
}

// 0x004361DF
void ServerPacketList::MakeServerKind1(const WorldServer& worldServer)
{
    m_rwLock.WriteLock();
    ++m_fameCount;

    int index = m_usedBuffSize;

    // 2 bytes - general offset before frames (count & frame type)
    //
    // c serverId     0
    // d serverIp     1-4
    // d serverPort   5-8
    // c age          9
    // c pvp          10
    // d userNumber   11-14

    m_buff[index++] = worldServer.serverId;  // 0            index = 0

    memcpy(&m_buff[index], &worldServer.outerIP, 4u);  // 1-4          index = 1
    index += 4;                                        //              index = 5

    memcpy(&m_buff[index], &worldServer.serverPort, 4u);  // 5-8
    index += 4;                                           //              index = 9

    m_buff[index++] = worldServer.ageLimit;  // 9            index = 9
    m_buff[index] = worldServer.pvp;         // 10           index = 10

    index += 3;  //              index = 13

    memcpy(&m_buff[index], &worldServer.userNumber, 2u);  // 13-15        index = 13

    m_usedBuffSize = index + 3;  // end: 13 + 3 => 15 + 1 (begin of next frame)

    m_rwLock.WriteUnlock();
}

// 0x004362E7
void ServerPacketList::MakeServerKind2(const WorldServer& worldServer)
{
    char serverName[20];

    m_rwLock.WriteLock();
    ++m_fameCount;

    int index = m_usedBuffSize;
    // 0      serverId
    // 1-40   serverName
    // 41-44  serverIP
    // 45-48  serverPort
    // 49     ageLimit
    // 50     pvp
    // 51-54  userNumber
    // 55     serverStatus

    m_buff[index] = worldServer.serverId;  // 0            index = 0
    index = index + 1;                     //              index = 1

    if (strlen(worldServer.serverName) >= 20)
    {
        strncpy(serverName, worldServer.serverName, 19);
        serverName[19] = 0;
        swprintf((wchar_t*)&m_buff[index], L"%S", serverName);
    }
    else
    {
        swprintf((wchar_t*)&m_buff[index], L"%S", worldServer.serverName);
    }
    index = index + 40;  //              index = 41

    memcpy(&m_buff[index], &worldServer.outerIP, 4u);  // 41-44        index = 41
    index += 4;                                        //              index = 45

    memcpy(&m_buff[index], &worldServer.serverPort, 4u);  // 45-48        index = 45
    index += 4;                                           //              index = 49

    m_buff[index++] = worldServer.ageLimit;  // 49           index = 49
    m_buff[index] = worldServer.pvp;         // 50           index = 50

    index += 3;  //              index = 53

    memcpy(&m_buff[index], &worldServer.userNumber, 2u);  // 53-54       index = 53

    m_usedBuffSize = index + 3;  //              index = 55 + 1 => 56, beginning of next frame

    m_rwLock.WriteUnlock();
}

// 0x00436452
void ServerPacketList::MakeServerKind3(const WorldServer& worldServer)
{
    m_rwLock.WriteLock();
    ++m_fameCount;

    int index = m_usedBuffSize;
    m_buff[index++] = worldServer.serverId;

    memcpy(&m_buff[index], &worldServer.outerIP, 4u);
    index += 4;

    memcpy(&m_buff[index], &worldServer.serverPort, 4u);
    index += 4;

    m_buff[index++] = worldServer.ageLimit;
    m_buff[index] = worldServer.pvp;
    index += 3;

    memcpy(&m_buff[index], &worldServer.userNumber, 2u);
    index += 3;

    memcpy(&m_buff[index], &worldServer.kind, 4u);

    m_usedBuffSize = index + 4;
    m_rwLock.WriteUnlock();
}

// 0x00436576
void ServerPacketList::SetServerCount()
{
    m_rwLock.ReadLock();
    m_buff[0] = m_fameCount;
    m_rwLock.ReadUnlock();
}

// 0x004365A3
void ServerPacketList::SetServerStatus(int serverId, bool status)
{
    if (serverId > 0)
    {
        int serverCount = g_serverKind.GetServerCount();
        if (serverId <= serverCount)
        {
            switch (m_frameType)
            {
                case FrameType_GameClient:
                    SetServerStatusKind1(serverId, status);
                    break;
                case FrameType_Extended:
                    SetServerStatusKind2(serverId, status);
                    break;
                case FrameType_WithKind:
                    SetServerStatusKind3(serverId, status);
                    break;
            }
        }
    }
}

// 0x00436624
void ServerPacketList::SetServerStatusKind1(int serverId, bool status)
{
    // 2 bytes - general offset before frames (count & frame type)
    //
    // c serverId     0
    // d serverIp     1-4
    // d serverPort   5-8
    // c age          9
    // c pvp          10
    // d userNumber   11-14

    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[16 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            m_buff[16 * i + 17] = status;
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x00436699
void ServerPacketList::SetServerStatusKind2(int serverId, bool status)
{
    // 0      serverId
    // 1-40   serverName
    // 41-44  serverIP
    // 45-48  serverPort
    // 49     ageLimit
    // 50     pvp
    // 51-54  userNumber

    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[56 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            m_buff[56 * i + 57] = status;  // initial offset = 2, 55 + 2 = 57
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x0043670E
void ServerPacketList::SetServerStatusKind3(int serverId, bool status)
{
    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[20 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            m_buff[20 * i + 17] = status;
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x00436783
void ServerPacketList::SetServerUserNum(int serverId, uint16_t usersOnline, uint16_t usersLimit)
{
    if (serverId > 0)
    {
        int serverCount = g_serverKind.GetServerCount();
        if (serverId <= serverCount)
        {
            switch (m_frameType)
            {
                case FrameType_GameClient:
                    SetServerUserNum1(serverId, usersOnline, usersLimit);
                    break;
                case FrameType_Extended:
                    SetServerUserNum2(serverId, usersOnline, usersLimit);
                    break;
                case FrameType_WithKind:
                    SetServerUserNum3(serverId, usersOnline, usersLimit);
                    break;
            }
        }
    }
}

// 0x00436804
void ServerPacketList::SetServerUserNum1(int serverId, uint16_t usersOnline, uint16_t usersLimit)
{
    // 2 bytes - general offset before frames (count & frame type)
    //
    // c serverId     0
    // d serverIp     1-4
    // d serverPort   5-8
    // c age          9
    // c pvp          10
    // d userNumber   11-14

    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[16 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            memcpy(&m_buff[16 * i + 13], &usersOnline, 2u);  // initial offset = 2, 11 + 2 = 13
            memcpy(&m_buff[16 * i + 13 + 2], &usersLimit, 2u);
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x00436885
void ServerPacketList::SetServerUserNum2(int serverId, uint16_t usersOnline, uint16_t usersLimit)
{
    // 0      serverId
    // 1-40   serverName
    // 41-44  serverIP
    // 45-48  serverPort
    // 49     ageLimit
    // 50     pvp
    // 51-54  userNumber

    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[56 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            memcpy(&m_buff[56 * i + 53], &usersOnline, 2u);  // initial offse = 2, 51 + 2 = 53
            memcpy(&m_buff[56 * i + 53 + 2], &usersLimit, 2u);
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x00436906
void ServerPacketList::SetServerUserNum3(int serverId, uint16_t usersOnline, uint16_t usersLimit)
{
    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[20 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            memcpy(&m_buff[20 * i + 13], &usersOnline, 2u);  //   initial offse = 2, 11 + 2 = 13
            memcpy(&m_buff[20 * i + 13 + 2], &usersLimit, 2u);
            m_rwLock.ReadUnlock();
        }
    }
}

// 0x00436987
int ServerPacketList::GetServerPacketList(char** buffer)
{
    if (this == (ServerPacketList*)-40)
    {
        return 0;
    }

    *buffer = m_buff;
    return m_usedBuffSize;
}

// 0x004369B6
bool ServerPacketList::ExchangeServePacketList(FrameType frameType, int serverKind, int usedBuffSize, char* buffer)
{
    if ((frameType == m_frameType) && (serverKind == m_serverKindIndex) && (buffer != NULL))
    {
        m_rwLock.WriteLock();

        memcpy(m_buff, buffer, 0x800u);
        m_fameCount = m_buff[0];
        m_usedBuffSize = usedBuffSize;

        m_rwLock.WriteUnlock();
        return true;
    }

    return false;
}

// 0x00436A39
bool ServerPacketList::GetServerStautsKind1(int serverId) const
{
    // 2 bytes - general offset before frames (count & frame type)
    //
    // c serverId     0
    // d serverIp     1-4
    // d serverPort   5-8
    // c age          9
    // c pvp          10
    // d userNumber   11-14

    bool available = false;
    char status = 0;
    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[16 * i + 2] == serverId)  // initial offset = 2
        {
            m_rwLock.ReadLock();
            status = m_buff[16 * i + 17];  // initial offse = 2, 15 + 2 = 17
            m_rwLock.ReadUnlock();
            break;
        }
    }
    if (status == 1)
    {
        available = true;
    }

    return available;
}

// 0x00436AC4
bool ServerPacketList::GetServerStautsKind2(int serverId) const
{
    // 0      serverId
    // 1-40   serverName
    // 41-44  serverIP
    // 45-48  serverPort
    // 49     ageLimit
    // 50     pvp
    // 51-54  userNumber

    bool available = false;
    char status = 0;
    for (int i = 0; i < m_fameCount; ++i)
    {
        if (m_buff[56 * i + 2] == serverId)
        {
            m_rwLock.ReadLock();
            status = m_buff[56 * i + 57];  // initial offset = 2, 55 + 2 = 57
            m_rwLock.ReadUnlock();
            break;
        }
    }
    if (status == 1)
    {
        available = true;
    }

    return available;
}
