#include "network/WorldSrvSocket.h"
#include "network/BufferReader.h"
#include "network/CAuthSocket.h"
#include "network/CIOBuffer.h"
#include "network/IPSessionDB.h"
#include "network/PacketUtils.h"
#include "network/WorldSrvPacket.h"
#include "network/WorldSrvServer.h"
#include "network/packets/LoginPackets.h"

#include "logger/CFileLog.h"
#include "ui/CLog.h"

#include "db/CDBConn.h"
#include "db/CServerUserCountStatus.h"
#include "db/DBEnv.h"
#include "model/AccountDB.h"
#include "model/CServerList.h"
#include "model/ServerKind.h"
#include "model/ServersProvider.h"
#include "utils/CExceptionInit.h"
#include "utils/Unused.h"

#include "AppInstance.h"
#include "config/Config.h"
#include "threads/Threading.h"

#include <sqlext.h>

#include <cstdarg>
#include <ctime>

const WorldSrvSocket::PacketHandler WorldSrvSocket::handlers[WorldSrvSocket::HANDLERS_NUMBER] = {&WorldSrvSocket::packet00_playOk,
                                                                                                 &WorldSrvSocket::packet01_playFail,
                                                                                                 &WorldSrvSocket::packet02_userLoggedToGs,
                                                                                                 &WorldSrvSocket::packet03_userQuitsWithError,
                                                                                                 &WorldSrvSocket::packet04_userDropped,
                                                                                                 &WorldSrvSocket::packet05_setUserLimit,
                                                                                                 &WorldSrvSocket::packet06_stub,
                                                                                                 &WorldSrvSocket::packet07_stub,
                                                                                                 &WorldSrvSocket::packet08_pingAck,
                                                                                                 &WorldSrvSocket::packet09_updateUserData,
                                                                                                 &WorldSrvSocket::packet10_updateWorldStatus,
                                                                                                 &WorldSrvSocket::packet11_gsReconnected,
                                                                                                 &WorldSrvSocket::packet12_newCharCreated,
                                                                                                 &WorldSrvSocket::packet13_charDeleted,
                                                                                                 &WorldSrvSocket::packet14_charManipulation,
                                                                                                 &WorldSrvSocket::packet15_serverSelected,
                                                                                                 &WorldSrvSocket::packet16_handler,
                                                                                                 &WorldSrvSocket::packet17_userDropped,
                                                                                                 &WorldSrvSocket::packet18_updateGSStatus,
                                                                                                 &WorldSrvSocket::packet19_updateUsersNumber,
                                                                                                 &WorldSrvSocket::packet20_getServerList,
                                                                                                 &WorldSrvSocket::packet21_moveCharToGS};

// 0x00410BAB
WorldSrvSocket::WorldSrvSocket(SOCKET socket)
    : CIOSocket(socket)
    , m_socket(socket)
    , m_serverID(0)
    , m_status_unused(0)
    , m_worldStatus(false)
    , m_accsInUseMap()
    , m_packetSize(0)
    , m_socketStatus(SocketStatus_Init)
    , m_packetHandlers(WorldSrvSocket::handlers)
    , m_worldSrvIP()
{
}

// 0x00410C4F
WorldSrvSocket::~WorldSrvSocket()
{
}

// 0x00410CA3
WorldSrvSocket* WorldSrvSocket::Allocate(SOCKET socket)
{
    return new WorldSrvSocket(socket);
}

// 0x00410D11
void WorldSrvSocket::OnClose()
{
    m_socketStatus = SocketStatus_Closed;

    g_winlog.AddLog(LOG_ERR, "*close connection from %s, %x(%x)", IP(), m_socket, this);

    time_t time = std::time(0);
    tm now = *std::localtime(&time);
    g_errLog.AddLog(LOG_INF, "%d-%d-%d %d:%d:%d,main server connection close from %s, 0x%x\r\n", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec, IP(), m_socket);

    if (m_serverID > 0)
    {
        g_accountDb.RemoveAll(m_serverID);
    }

    if (ServersProvider::SetServerSocket(m_worldSrvIP.s_addr, m_socket) > 0)
    {
        g_worldServServer.RemoveSocket(m_worldSrvIP.s_addr);
    }

    CDBConn sql(&g_linDB);
    sql.Execute("update worldstatus set status=0 where idx=%d", m_serverID);
    ServersProvider::SetServerStatus(m_serverID, 0);
}

// 0x00411123
void WorldSrvSocket::OnCreate()
{
    AddRef();
    OnRead();
    Send("cdd", 3, g_buildNumber, 1);
}

// 0x00410E7A
void WorldSrvSocket::OnRead()
{
    if (m_socketStatus == SocketStatus_Closed)
    {
        CloseSocket();
        return;
    }

    int dwRead = 0;
    int buffSize = m_pReadBuf->m_dwSize;
    uint8_t* buffer = m_pReadBuf->m_Buffer;
    int errorNum = 0;

    while (true)
    {
        // read size only
        while (true)
        {
            if (dwRead >= buffSize)
            {
                return Read(0);
            }

            if (m_socketStatus != SocketStatus_Init)
            {
                break;
            }

            if ((dwRead + 3) > buffSize)
            {
                return Read(buffSize - dwRead);
            }

            m_packetSize = buffer[dwRead] + (buffer[dwRead + 1] << 8) + 1 - g_Config.packetSizeType;
            if (m_packetSize <= 0 || m_packetSize > BUFFER_SIZE)
            {
                g_winlog.AddLog(LOG_ERR, "%d: bad packet size %d", m_socket, m_packetSize);
                errorNum = 1;
                g_errLog.AddLog(
                    LOG_INF,
                    "main server connection close. invalid status and protocol %s, errorNum "
                    ":%d\r\n",
                    IP(),
                    errorNum);
                CloseSocket();
                return;
            }

            dwRead += 2;
            m_socketStatus = SocketStatus_BytesRead;
        }

        if (m_socketStatus != SocketStatus_BytesRead)
        {
            CloseSocket();
            return;
        }

        if ((m_packetSize + dwRead) > buffSize)
        {
            return Read(buffSize - dwRead);
        }

        if (buffer[dwRead] >= HANDLERS_NUMBER)
        {
            g_winlog.AddLog(LOG_ERR, "unknown protocol %d", buffer[dwRead]);
            errorNum = 2;
            g_errLog.AddLog(LOG_INF, "main server connection close. invalid status and protocol %s, errorNum :%d\r\n", IP(), errorNum);
            CloseSocket();
            return;
        }

        WorldSrvPacket* packet = WorldSrvPacket::Alloc();
        packet->m_pSocket = this;
        packet->m_pBuf = m_pReadBuf;

        // 2 bytes size, 3rd byte is packet type
        packet->m_pFunc = m_packetHandlers[buffer[dwRead]];

        AddRef();
        m_pReadBuf->AddRef();

        ::InterlockedIncrement(&WorldSrvPacket::g_nPendingPacket);

        packet->PostObject(dwRead, Threading::g_hCompletionPort);

        dwRead += m_packetSize;
        m_socketStatus = SocketStatus_Init;
    }

    errorNum = 3;
    g_errLog.AddLog(LOG_INF, "main server connection close. invalid status and protocol %s, errorNum :%d\r\n", IP(), errorNum);
    CloseSocket();
}

// 0x00411108
char* WorldSrvSocket::IP() const
{
    return inet_ntoa(m_worldSrvIP);
}

// 0x0041115C
void WorldSrvSocket::SetAddress(in_addr ipAddress)
{
    m_worldSrvIP = ipAddress;
    if (!g_Config.supportReconnect)
    {
        CDBConn sql(&g_linDB);
        sql.Execute("update worldstatus set status=1 where idx=%d", m_serverID);
        ServersProvider::SetServerStatus(m_serverID, true);
        m_worldStatus = true;
    }
}

// 0x004111F6
void WorldSrvSocket::Send(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    if (m_socketStatus != SocketStatus_Closed)
    {
        CIOBuffer* buff = CIOBuffer::Alloc();
        int packetSize = PacketUtils::VAssemble(&buff->m_Buffer[2], BUFFER_SIZE - 2, format, va);
        if (packetSize != 0)
        {
            packetSize += g_Config.packetSizeType - 1;
            buff->m_Buffer[0] = char(packetSize & 0xFF);
            buff->m_Buffer[1] = char((packetSize >> 8) & 0xFF);
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", m_socket, format);
        }
        buff->m_dwSize = packetSize;
        Write(buff);
    }
}

// 0x00411510
int WorldSrvSocket::getaddr() const
{
    return m_worldSrvIP.s_addr;
}

// 0x0040F680
bool WorldSrvSocket::packet00_playOk(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);

    char accountName[15];
    memset(accountName, 0, 14u);

    int otherServer = 0;
    SOCKET socket = g_accountDb.FindSocket(uid, worldSrvSocket->m_serverID, true, &otherServer, accountName);
    accountName[14] = 0;

    if (socket != 0 && uid != 0 && otherServer != 0)
    {
        g_accountDb.KickAccount(uid, 14, true);
        if (otherServer > 0 && otherServer <= g_serverList.m_serverNumber)
        {
            WorldServer worldServer = g_serverList.GetAt(otherServer);
            WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcs", 1, uid, sizeof(accountName), accountName);
        }
        worldSrvSocket->Send("cdcs", 1, uid, 14, accountName);
        return false;
    }

    if (socket != 0 && uid != 0)
    {
        CAuthSocket::Send(socket, "cddc", LS_PlayOk, sessionKey, uid, worldSrvSocket->m_serverID);
    }

    return false;
}

// 0x0040F7B1
bool WorldSrvSocket::packet01_playFail(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    char errorCode = BufferReader::ReadByte(&buffer);
    int uid = BufferReader::ReadInt(&buffer);
    SOCKET socket = g_accountDb.FindSocket(uid, false);
    int payStat = 0;
    in_addr connectedIP = {0};
    time_t loginTime = 0;
    char accName[15];
    bool success = g_accountDb.GetAccountInfoForIPStop(uid, accName, &payStat, &connectedIP, &loginTime);

    if (success && payStat < 1000 && payStat > 0)
    {
        g_IPSessionDB.StopIPCharge(uid, connectedIP, payStat, 0, loginTime, worldSrvSocket->m_serverID, accName);
    }

    if (g_Config.autoKickAccount && errorCode == PLAY_FAIL_ALREADY_LOGGED)
    {
        worldSrvSocket->Send("cdcs", 1, uid, errorCode, accName);
    }

    if (socket != 0 && uid != 0)
    {
        CAuthSocket::Send(socket, "cc", LS_PlayFail, errorCode);
    }

    return false;
}

// 0x0040F8B2
bool WorldSrvSocket::packet02_userLoggedToGs(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    if (g_accountDb.recordGamePlayTime(uid, worldSrvSocket->m_serverID))
    {
        return false;
    }

    g_accountDb.KickAccount(uid, PLAY_FAIL_WRONG_ACC_NAME, true);

    return false;
}

// 0x0040F900
bool WorldSrvSocket::packet03_userQuitsWithError(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    short reason = BufferReader::ReadShort(&buffer);
    int usetime = BufferReader::ReadInt(&buffer);

    if (!g_accountDb.quitGamePlay(uid, worldSrvSocket->m_serverID))
    {
        g_errLog.AddLog(LOG_ERR, "quit game error, uid %d, reason %d, usetime %d\r\n", uid, reason, usetime);
    }

    if (g_Config.oneTimeLogOut)
    {
        g_accountDb.logoutAccount(uid);
    }

    return false;
}

// 0x0040FA11
bool WorldSrvSocket::packet04_userDropped(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    short reason = BufferReader::ReadShort(&buffer);
    UNUSED(reason);
    g_accountDb.quitGamePlay(uid, worldSrvSocket->m_serverID);

    char accountName[15];
    memset(accountName, 0, 15u);
    g_accountDb.removeAccount(uid, accountName);

    return false;
}

// 0x0040FAF4
bool WorldSrvSocket::packet05_setUserLimit(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    uint16_t usersOnline = BufferReader::ReadShort(&buffer);
    uint16_t usersLimit = BufferReader::ReadShort(&buffer);

    ServersProvider::SetServerUserNum(worldSrvSocket->getaddr(), usersOnline, usersLimit, worldSrvSocket->m_serverID);
    g_userCount.UpdateUserNum(worldSrvSocket->m_serverID, usersOnline, usersLimit);

    return false;
}

// 0x0040FB53
bool WorldSrvSocket::packet06_stub(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    BufferReader::ReadInt(&buffer);
    BufferReader::ReadShort(&buffer);
    return false;
}

// 0x0040FB7E
bool WorldSrvSocket::packet07_stub(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    BufferReader::ReadInt(&buffer);
    return false;
}

// 0x0040FB97
bool WorldSrvSocket::packet08_pingAck(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int ping = BufferReader::ReadInt(&buffer);
    worldSrvSocket->Send("cd", 4, ping);
    return false;
}

// 0x0040FBBE
bool WorldSrvSocket::packet09_updateUserData(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);

    char userData[17];
    memcpy(userData, buffer, 16u);
    buffer += 16;
    userData[16] = 0;

    CDBConn sql(&g_linDB);
    SQLINTEGER len = 16;
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BINARY, 16u, 0, userData, 16, &len);
    sql.Execute("UPDATE user_data SET user_data=? WHERE uid = %d", uid);

    return false;
}

// 0x0040FC7A
bool WorldSrvSocket::packet10_updateWorldStatus(WorldSrvSocket* worldSrvSocket, uint8_t* /*buffer*/)
{
    worldSrvSocket->m_accsInUseMap.PopKickUser(worldSrvSocket);
    CDBConn sql(&g_linDB);
    sql.Execute("update worldstatus set status=1 where idx=%d", worldSrvSocket->m_serverID);

    ServersProvider::SetServerStatus(worldSrvSocket->m_serverID, true);
    worldSrvSocket->m_worldStatus = true;

    return false;
}

// 0x0040FD0D
bool WorldSrvSocket::packet11_gsReconnected(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int userNumber = BufferReader::ReadInt(&buffer);

    int userIndex = 0;
    for (userIndex = 0; userIndex < userNumber; ++userIndex)
    {
        char accName[16];
        memset(accName, 0, 15u);
        BufferReader::ReadString(&buffer, 14, accName);

        char age = 0;
        int uid = BufferReader::ReadInt(&buffer);
        int payStat = BufferReader::ReadInt(&buffer);
        int connectedIp = BufferReader::ReadUInt(&buffer);
        int loginFlag = BufferReader::ReadInt(&buffer);
        int warnFlag = BufferReader::ReadInt(&buffer);
        int birthdayEncoded = 0;
        int restOfSsn = 0;
        SexAndCentury sexAndCentury = FemaleBorn_1800_to_1899;

        if (g_Config.countryCode != 0)
        {
            // Non-Korean
            birthdayEncoded = 0;
            restOfSsn = 0;
            sexAndCentury = FemaleBorn_1800_to_1899;
            age = 0;
        }
        else
        {
            // Korean
            CDBConn sql(&g_linDB);
            sql.ResetHtmt();

            char sessionKey[14];
            sql.bindStr(sessionKey, 14);
            bool success = false;

            if (sql.Execute("Select ssn From user_info with (nolock) Where account = '%s'", accName))
            {
                bool notFound = false;
                if (sql.Fetch(&notFound))
                {
                    if (notFound)
                    {
                        success = false;
                    }
                }
                else
                {
                    success = false;
                }
            }
            else
            {
                success = false;
            }

            if (!success)
            {
                continue;
            }

            time_t timeNow = std::time(0);
            tm* now = std::localtime(&timeNow);

            sexAndCentury = (SexAndCentury)(sessionKey[6] - '0');
            if (sessionKey[6] != '1' && sessionKey[6] != '2' && sessionKey[6] != '5' && sessionKey[6] != '6')
            {
                // 2000-2099 year
                age = 10 * (sessionKey[0] - '0') + sessionKey[1] - '0';
            }
            else
            {
                // 1900-1999 year
                int devidedByTen = now->tm_year / 10 + '0';
                int moduloTen = now->tm_year % 10 + '0';
                age = moduloTen - sessionKey[1] + 10 * (devidedByTen - sessionKey[0]);
            }

            int dayMonthNowEncoded = now->tm_mday + 100 * (now->tm_mon + 1);
            int dayAndMonthEncoded = 10 * (sessionKey[4] - '0') + 100 * (sessionKey[3] - '0') + 1000 * (sessionKey[2] - '0') + sessionKey[5] - '0';

            if (dayMonthNowEncoded < dayAndMonthEncoded)
            {
                --age;
            }
            if (age < 0)
            {
                age = 0;
            }

            birthdayEncoded = 10000 * (10 * (sessionKey[0] - '0') + sessionKey[1] - '0') + dayAndMonthEncoded;
            restOfSsn = atoi(&sessionKey[6]);
        }

        LoginUser* accInfo = new LoginUser();
        strncpy(accInfo->accountName, accName, 14u);
        accInfo->accountName[14] = 0;
        accInfo->clientCookie = 0;
        accInfo->lastworld = worldSrvSocket->m_serverID;
        accInfo->loginFlag = loginFlag;
        accInfo->warnFlag = warnFlag;
        accInfo->loginState = LoginState_LoggedToGS;
        accInfo->connectedIP.s_addr = connectedIp;
        accInfo->loginTime = std::time(0);
        accInfo->loggedGameServerId = worldSrvSocket->m_serverID;
        accInfo->selectedGServerId = 0;
        accInfo->gameSocket = INVALID_SOCKET;
        accInfo->sessionKey = 0;
        accInfo->timerHandler = 0;
        accInfo->payStat = payStat;
        accInfo->sexAndCentury = sexAndCentury;
        accInfo->birthdayEncoded = birthdayEncoded;
        accInfo->restOfSsn = restOfSsn;
        accInfo->age = age;

        if (g_accountDb.RegAccountByServer(*accInfo, uid))
        {
            delete accInfo;
        }
        else
        {
            delete accInfo;
            worldSrvSocket->m_accsInUseMap.AddKickUser(uid, accName);
        }
    }

    worldSrvSocket->Send("cd", 5, userIndex);

    return false;
}

// 0x004101AC
bool WorldSrvSocket::packet12_newCharCreated(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int unknown = BufferReader::ReadInt(&buffer);
    int uid = BufferReader::ReadInt(&buffer);
    int charId = BufferReader::ReadInt(&buffer);
    short binarySize = BufferReader::ReadShort(&buffer);

    AccountDB::CreateChar(uid, charId, worldSrvSocket->m_serverID);
    SOCKET socket = g_accountDb.FindSocket(uid, false);
    CAuthSocket::Send(socket, "ccddchb", LS_CharSelection, unknown, uid, charId, worldSrvSocket->m_serverID, binarySize, binarySize, buffer);

    return false;
}

// 0x0041026C
bool WorldSrvSocket::packet13_charDeleted(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    char unknown = BufferReader::ReadByte(&buffer);
    int uid = BufferReader::ReadInt(&buffer);
    int charId = BufferReader::ReadInt(&buffer);

    AccountDB::DelChar(uid, charId, worldSrvSocket->m_serverID);
    SOCKET socket = g_accountDb.FindSocket(uid, false);
    CAuthSocket::Send(socket, "ccddc", LS_CharDeleted, unknown, uid, charId, worldSrvSocket->m_serverID);

    return false;
}

// 0x00410307
bool WorldSrvSocket::packet14_charManipulation(WorldSrvSocket* worldSrvSocket, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    char unknown = BufferReader::ReadByte(&buffer);
    short binarySize = BufferReader::ReadShort(&buffer);

    if (unknown > 0)
    {
        SOCKET authSocket = g_accountDb.FindSocket(uid, false);
        CAuthSocket::Send(authSocket, "cdccchb", LS_CharManipulation, uid, 11, unknown, worldSrvSocket->m_serverID, binarySize, binarySize, buffer);
    }

    return false;
}

// 0x00410448
bool WorldSrvSocket::packet15_serverSelected(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    int unknown = BufferReader::ReadInt(&buffer);
    int serverId = BufferReader::ReadByte(&buffer);

    SOCKET authSocket = g_accountDb.FindSocket(uid, true);

    // if ( serverId >= 1 || serverId <= g_serverList.getServerCount() )// FIXED
    if (serverId >= 1 && serverId <= g_serverList.GetMaxServerNum())
    {
        WorldServer worldServer = ServersProvider::GetWorldServer(serverId);
        if (worldServer.serverId <= 0 || worldServer.kind & 0x80000000)
        {
            CAuthSocket::Send(authSocket, "cc", LS_PlayFail, PLAY_FAIL_SOME_ERROR);
        }
        else
        {
            CAuthSocket::Send(authSocket, "cddd", LS_ServerSelection, unknown, worldServer.outerIP, worldServer.serverPort);
        }
    }
    else
    {
        CAuthSocket::Send(authSocket, "cc", LS_PlayFail, PLAY_FAIL_SOME_ERROR);
    }

    return false;
}

// 0x004105CD
bool WorldSrvSocket::packet16_handler(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* /*buffer*/)
{
    return false;
}

// 0x00410653
bool WorldSrvSocket::packet17_userDropped(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    BufferReader::ReadByte(&buffer);
    g_accountDb.logoutAccount(uid);
    return false;
}

// 0x0041070C
bool WorldSrvSocket::packet18_updateGSStatus(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    int serverId = BufferReader::ReadByte(&buffer);
    bool status = (BufferReader::ReadByte(&buffer) != 0);

    if (serverId > 0 && serverId <= g_serverList.m_serverNumber)
    {
        WorldServer worldServer = ServersProvider::GetWorldServer(serverId);
        if (worldServer.serverId > 0 && worldServer.serverId <= g_serverList.m_serverNumber)
        {
            if (!(worldServer.kind & 0x80000000) || status)
            {
                if (status == false)
                {
                    g_accountDb.RemoveAll(worldServer.serverId);
                }

                CDBConn sql(&g_linDB);
                // sql.exec("update worldstatus set status=%d where idx=%d", worldServer.serverId,
                // status);// FIXED serverID and status parameters missmatch in query
                sql.Execute("update worldstatus set status=%d where idx=%d", status, worldServer.serverId);
                ServersProvider::SetServerStatus(worldServer.serverId, status);
            }
            else
            {
                g_winlog.AddLog(LOG_ERR, "%d serverid is manage servers (not allow protocol)", worldServer.serverId);
            }
        }
    }

    return false;
}

// 0x0041085B
bool WorldSrvSocket::packet19_updateUsersNumber(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    uint16_t usersOnline = BufferReader::ReadShort(&buffer);
    uint16_t usersLimit = BufferReader::ReadShort(&buffer);
    int serverId = BufferReader::ReadByte(&buffer);

    if (serverId > 0 && serverId <= g_serverList.m_serverNumber)
    {
        WorldServer worldServer = ServersProvider::GetWorldServer(serverId);
        if (worldServer.serverId > 0 && worldServer.serverId <= g_serverList.m_serverNumber)
        {
            ServersProvider::SetServerUserNum(worldServer.outerIP, usersOnline, usersLimit, worldServer.serverId);
        }
    }

    return false;
}

// 0x00410970
bool WorldSrvSocket::packet20_getServerList(WorldSrvSocket* worldSrvSocket, uint8_t* /*buffer*/)
{
    int size = 0;
    char packetBuffer[2048];
    char* buff = NULL;

    if (g_Config.newServerList)
    {
        size = g_serverKind.GetServerPacketList(FrameType_DefaultOrCompressed, 0, &buff);
        memcpy(packetBuffer, buff, size);
    }
    else
    {
        size = g_serverList.m_frameSizes[FrameType_DefaultOrCompressed];
        memcpy(packetBuffer, g_serverList.m_frameLists[0], g_serverList.m_frameSizes[0]);
    }

    packetBuffer[size] = 0;
    worldSrvSocket->Send("cb", 9, size, packetBuffer);

    return false;
}

// 0x004103AD
bool WorldSrvSocket::packet21_moveCharToGS(WorldSrvSocket* /*worldSrvSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    int oldCharId = BufferReader::ReadInt(&buffer);
    int oldServerId = BufferReader::ReadInt(&buffer);
    int newCharId = BufferReader::ReadInt(&buffer);
    int newServerId = BufferReader::ReadInt(&buffer);

    AccountDB::UpdateCharLocation(uid, oldCharId, oldServerId, newCharId, newServerId);

    return false;
}
