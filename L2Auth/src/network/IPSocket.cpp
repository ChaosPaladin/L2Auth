#include "network/IPSocket.h"
#include "model/AccountDB.h"
#include "network/BufferReader.h"
#include "network/CAuthServer.h"
#include "network/CAuthSocket.h"
#include "network/CIOBuffer.h"
#include "network/IPPacket.h"
#include "network/IPSessionDB.h"
#include "network/PacketUtils.h"
#include "utils/CExceptionInit.h"
#include "utils/Unused.h"

#include "ui/CLog.h"

#include "AppInstance.h"
#include "config/Config.h"
#include "threads/Threading.h"

#include <cstdarg>
#include <ctime>

bool IPSocket::isReconnecting = false;
CRWLock IPSocket::s_lock;
HANDLE IPSocket::s_timer;

IPSocket* g_IPSocket = NULL;

const IPSocket::PacketHandler IPSocket::handlers[16] = {&IPSocket::packet00_handler,
                                                        &IPSocket::packet_dummy_handler,
                                                        &IPSocket::packet02_handler,
                                                        &IPSocket::packet03_handler,
                                                        &IPSocket::packet04_handler,
                                                        &IPSocket::packet05_handler,
                                                        &IPSocket::packet_dummy_handler,
                                                        &IPSocket::packet_dummy_handler,
                                                        &IPSocket::packet_dummy_handler,
                                                        &IPSocket::packet09_handler,
                                                        &IPSocket::packet10_handler,
                                                        &IPSocket::packet11_handler,
                                                        &IPSocket::packet_dummy_handler,
                                                        &IPSocket::packet13_handler,
                                                        &IPSocket::packet14_handler,
                                                        &IPSocket::packet15_handler};

// 0x00425EF9
IPSocket::IPSocket(SOCKET socket)
    : CIOSocket(socket)
    , m_connectSessionKey(0)
    , m_packetSize()
    , m_status(SocketStatus_Init)
    , m_packetHandlers(IPSocket::handlers)
    , m_serverIP(g_Config.IPServer)
    , m_socketFamily(AF_INET)
    , m_port(htons(g_Config.IPPort))
    , m_ipsocIP(g_Config.IPServer)
{
    AddRef();

    IPSocket::isReconnecting = false;
}

// 0x00425FC4
IPSocket::~IPSocket()
{
    g_winlog.AddLog(LOG_ERR, "IPSocket Deleted");
}

// 0x00425E8B
IPSocket* IPSocket::Allocate(SOCKET socket)
{
    return new IPSocket(socket);
}

void IPSocket::OnTimerCallback()
{
}

// 0x00426021
void IPSocket::OnClose()
{
    m_status = SocketStatus_Closed;
    IPSocket::isReconnecting = true;
    g_Config.useIPServer = false;

    g_winlog.AddLog(LOG_ERR, "*close connection IPServer from %s, %x(%x)", IP(), m_hSocket, this);
    g_IPSessionDB.DellAllWaitingSessionID();
    AddRef();
    ::CreateTimerQueueTimer(&IPSocket::s_timer, 0, IPSocket::IPSocketTimerRoutine, (PVOID)this, g_Config.IPInterval, 0, 0);
}

// 0x004260C2
void IPSocket::OnCreate()
{
    AddRef();
    OnRead();
    Send("cdc", 0, g_buildNumber, g_Config.gameID);
}

// 0x00426100
void IPSocket::OnRead()
{
    if (m_status == SocketStatus_Closed)
    {
        CloseSocket();
        return;
    }

    int dwRead = 0;
    int buffSize = m_pReadBuf->m_dwSize;
    uint8_t* buffer = m_pReadBuf->m_Buffer;

    while (true)
    {
        while (true)
        {
            if (dwRead >= buffSize)
            {
                return Read(0);
            }

            if (m_status != SocketStatus_Init)
            {
                break;
            }

            if (dwRead + 3 > buffSize)
            {
                return Read(buffSize - dwRead);
            }

            m_packetSize = buffer[dwRead] + (buffer[dwRead + 1] << 8) + 1;
            if (m_packetSize <= 0 || m_packetSize > BUFFER_SIZE)
            {
                g_winlog.AddLog(LOG_ERR, "%d: bad packet size %d", m_hSocket, m_packetSize);
                CloseSocket();
                return;
            }
            dwRead += 2;
            m_status = SocketStatus_BytesRead;
        }

        if (m_status != SocketStatus_BytesRead)
        {
            CloseSocket();
            return;
        }

        if (m_packetSize + dwRead > buffSize)
        {
            return Read(buffSize - dwRead);
        }

        if (buffer[dwRead] >= 16)
        {
            break;
        }

        IPPacket* packet = IPPacket::Alloc();
        packet->m_pSocket = this;
        packet->m_pBuf = m_pReadBuf;
        packet->m_pFunc = m_packetHandlers[buffer[dwRead]];
        AddRef();
        m_pReadBuf->AddRef();
        ::InterlockedIncrement(&IPPacket::g_nPendingPacket);

        packet->PostObject(dwRead, Threading::g_hCompletionPort);

        dwRead += m_packetSize;
        m_status = SocketStatus_Init;
    }

    g_winlog.AddLog(LOG_ERR, "unknown protocol %d", buffer[dwRead]);
    CloseSocket();
}

// 0x004262FA
bool IPSocket::Send(const char* format, ...)
{
    AddRef();
    if (m_status != SocketStatus_Closed && !IPSocket::isReconnecting && g_Config.useIPServer)
    {
        va_list va;
        va_start(va, format);
        CIOBuffer* buff = CIOBuffer::Alloc();
        int packetSize = PacketUtils::VAssemble(&buff->m_Buffer[2], BUFFER_SIZE - 2, format, va);
        if (packetSize != 0)
        {
            --packetSize;
            buff->m_Buffer[0] = char(packetSize & 0xFF);
            buff->m_Buffer[1] = char((packetSize >> 8) & 0xFF);
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", m_hSocket, format);
        }
        buff->m_dwSize = packetSize + 3;
        Write(buff);
        ReleaseRef();
        return true;
    }

    ReleaseRef();
    return false;
}

// 0x00428610
void IPSocket::SetAddress(in_addr ipAddress)
{
    m_serverIP = ipAddress;
}

// 0x00428630
void IPSocket::SetConnectSessionKey(int connectSession)
{
    m_connectSessionKey = connectSession;
}

// 0x004260AA
char* IPSocket::IP() const
{
    return inet_ntoa(m_serverIP);
}

// 0x0042440E
void NTAPI IPSocket::IPSocketTimerRoutine(PVOID /*param*/, BOOLEAN)
{
    g_winlog.AddLog(LOG_WRN, "IPSocketTimerRoutine");

    if (IPSocket::s_timer)
    {
        ::DeleteTimerQueueTimer(0, IPSocket::s_timer, 0);
    }

    IPSocket::s_timer = 0;

    if (IPSocket::isReconnecting == true)
    {
        SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        sockaddr_in name;
        name.sin_family = AF_INET;
        name.sin_addr = g_Config.IPServer;
        name.sin_port = ::htons(g_Config.IPPort);
        int result = ::connect(socket, (const sockaddr*)&name, sizeof(sockaddr_in));

        IPSocket* ipSocket = IPSocket::Allocate(socket);
        ipSocket->SetAddress(g_Config.IPServer);

        if (result == SOCKET_ERROR)
        {
            ipSocket->CloseSocket();
            ipSocket->ReleaseRef();
        }
        else
        {
            IPSocket::s_lock.WriteLock();

            g_IPSocket->ReleaseRef();
            g_IPSocket = ipSocket;

            IPSocket::isReconnecting = false;
            g_Config.useIPServer = true;

            g_IPSocket->Initialize(Threading::g_hCompletionPort);

            IPSocket::s_lock.WriteUnlock();
        }
    }
}

// 0x0042593E
bool IPSocket::packet00_handler(IPSocket* ipSocket, uint8_t* buffer)
{
    int connectSession = BufferReader::ReadInt(&buffer);
    ipSocket->SetConnectSessionKey(connectSession);
    return false;
}

// 0x00425485
bool IPSocket::packet_dummy_handler(IPSocket* /*ipSocket*/, uint8_t* /*buffer*/)
{
    g_winlog.AddLog(LOG_DBG, "Call DummyPacket What What What");
    return false;
}

// 0x004257A1
bool IPSocket::packet02_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    char payStat = BufferReader::ReadByte(&buffer);
    int totalTime = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);

    g_IPSessionDB.AcquireSessionSuccess(uid, sessionKey, false, totalTime, payStat);

    return false;
}

// 0x004255CA
bool IPSocket::packet03_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    // ms_exc.registration.TryLevel = 0;

    int uid = BufferReader::ReadInt(&buffer);
    int serverId = BufferReader::ReadByte(&buffer);
    int totalTime = BufferReader::ReadInt(&buffer);
    int payStat = BufferReader::ReadInt(&buffer);
    int connectedIp = BufferReader::ReadInt(&buffer);

    char accountName[15];
    int loginFlag = 0;
    int warnFlag = 0;
    int sessionKey = 0;
    SOCKET gameSocket = INVALID_SOCKET;

    bool success = g_accountDb.GetAccountInfo(uid, accountName, &loginFlag, &warnFlag, &sessionKey, &gameSocket);
    if (success)
    {
        accountName[14] = 0;
        CAuthSocket* authSocket = g_authServer.FindSocket(gameSocket);
        if (authSocket != NULL)
        {
            PlayFail result = g_accountDb.AboutToPlay(uid, accountName, totalTime, loginFlag, warnFlag, sessionKey, authSocket, serverId, payStat);
            authSocket->ReleaseRef();

            if (result != PLAY_FAIL_NO_ERROR)
            {
                time_t loginTime = std::time(0);
                g_IPSessionDB.StopIPCharge(uid, in_addr{connectedIp}, payStat, 0, loginTime, serverId, accountName);
            }
        }
        return false;
    }

    //    ms_exc.registration.TryLevel = -1;

    return false;
}

// 0x004254A0
bool IPSocket::packet04_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    char accName[15];
    memset(accName, 0, 15u);
    int uid = BufferReader::ReadInt(&buffer);
    BufferReader::ReadByte(&buffer);
    int unused = BufferReader::ReadInt(&buffer);
    UNUSED(unused);

    BufferReader::ReadInt(&buffer);
    BufferReader::ReadUInt(&buffer);
    BufferReader::ReadString(&buffer, 15, accName);
    accName[14] = 0;

    if (uid != 0)
    {
        g_accountDb.logoutAccount(uid);
    }

    return false;
}

// 0x00425880
bool IPSocket::packet05_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    char status = BufferReader::ReadByte(&buffer);
    if (uid > 0)
    {
        g_IPSessionDB.AcquireSessionFail(uid, 0, (LoginFailReason)status);
    }
    return false;
}

// 0x004259DF
bool IPSocket::packet09_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    BufferReader::ReadUInt(&buffer);
    BufferReader::ReadInt(&buffer);
    BufferReader::ReadInt(&buffer);

    char accountName[16];
    memset(accountName, 0, 16u);
    BufferReader::ReadString(&buffer, 16, accountName);
    int uid = BufferReader::ReadInt(&buffer);
    accountName[14] = 0;
    g_accountDb.KickAccount(uid, 21, true);
    return false;
}

// 0x00425CC7
bool IPSocket::packet10_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    BufferReader::ReadByte(&buffer);
    if (uid > 0)
    {
        g_accountDb.logoutAccount(uid);
    }

    return false;
}

// 0x00425ADF
bool IPSocket::packet11_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    // ms_exc.registration.TryLevel = 0;

    int uid = BufferReader::ReadInt(&buffer);
    int serverId = BufferReader::ReadByte(&buffer);
    int totalTime = BufferReader::ReadInt(&buffer);
    int payStatus = BufferReader::ReadInt(&buffer);
    int connectedIp = BufferReader::ReadInt(&buffer);

    int loginFlag = 0;
    int warnFlag = 0;
    int sessionKey = 0;
    SOCKET socket = INVALID_SOCKET;
    char accName[15];
    memset(accName, 0, 15u);

    bool success = g_accountDb.GetAccountInfo(uid, accName, &loginFlag, &warnFlag, &sessionKey, &socket);
    if (success)
    {
        accName[14] = 0;
        CAuthSocket* authSocket = g_authServer.FindSocket(socket);
        if (authSocket != NULL)
        {
            PlayFail result = g_accountDb.AboutToPlay(uid, accName, totalTime, loginFlag, warnFlag, sessionKey, authSocket, serverId, payStatus);
            authSocket->ReleaseRef();
            if (result != PLAY_FAIL_NO_ERROR)
            {
                time_t loginTime = std::time(0);
                g_IPSessionDB.StopIPCharge(uid, in_addr{connectedIp}, payStatus, 0, loginTime, serverId, accName);
            }
        }
        return false;
    }

    // ms_exc.registration.TryLevel = -1;

    return false;
}

// 0x00425D86
bool IPSocket::packet13_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    if (uid > 0)
    {
        g_accountDb.logoutAccount(uid);
    }
    return false;
}

// 0x00425E53
bool IPSocket::packet14_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    BufferReader::ReadInt(&buffer);
    BufferReader::ReadInt(&buffer);
    return false;
}

// 0x00425DBA
bool IPSocket::packet15_handler(IPSocket* /*ipSocket*/, uint8_t* buffer)
{
    int uid = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);
    int clientIP = BufferReader::ReadInt(&buffer);
    int payStat = BufferReader::ReadInt(&buffer);

    char accName[16];
    if (g_accountDb.FindAccount(uid, accName) <= 0)
    {
        g_IPSessionDB.ReleaseSessionRequest(sessionKey, in_addr{clientIP}, payStat);
    }
    else
    {
        g_IPSessionDB.AddSessionID(uid, sessionKey);
    }

    return false;
}
