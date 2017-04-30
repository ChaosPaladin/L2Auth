#include "network/CAuthSocket.h"
#include "crypt/DesFrontEnd.h"
#include "crypt/OldCrypt.h"
#include "crypt/Blowfish.h"
#include "db/CAccount.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "model/AccountDB.h"
#include "model/CIPAccessLimit.h"
#include "model/CServerList.h"
#include "model/FrameType.h"
#include "model/LoginUser.h"
#include "model/ServerKind.h"
#include "network/BufferReader.h"
#include "network/CAuthPacket.h"
#include "network/CAuthServer.h"
#include "network/CIOBuffer.h"
#include "network/IPSessionDB.h"
#include "network/PacketUtils.h"
#include "network/WorldSrvServer.h"
#include "network/packets/LoginPackets.h"
#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "utils/CExceptionInit.h"
#include "utils/Unused.h"
#include "utils/Utils.h"

#include "config/CIPList.h"
#include "config/Config.h"
#include "threads/Threading.h"

#include <cmath>

const CAuthSocket::PacketHandler CAuthSocket::handlers[CAuthSocket::HANDLERS_NUMBER] = {&CAuthSocket::packet00_RequestAuthLogin,
                                                                                        &CAuthSocket::packet01_ServerListPacket,
                                                                                        &CAuthSocket::packet02_RequestServerLogin,
                                                                                        &CAuthSocket::packet03_CancelLogin,
                                                                                        &CAuthSocket::packet04_unknownHandler,
                                                                                        &CAuthSocket::packet05_RequestServerList_C1,
                                                                                        &CAuthSocket::packet06_unknownHandler,
                                                                                        &CAuthSocket::packet07_charsHandler,
                                                                                        &CAuthSocket::packet08_unknownHandler};

// 0x00439730
CAuthSocket::CAuthSocket(SOCKET socket)
    : CIOSocket(socket)
    , m_socket(socket)
    , m_clientLoginState(LoginState_Initial)
    , m_uid(0)
    //, m_xorKeySend()
    //, m_xorKeyReceive()
    , m_lastPacketTime(::GetTickCount())
    , m_encrypt_handler()
    , m_decrypt_handler()
    , m_packetSize()
    , m_field_136()
    , m_socketTimeoutTimer(nullptr)
    , m_status(SocketStatus_Init)
    , m_packetHandlers(CAuthSocket::handlers)
    , m_clientIP()
    , m_unused_156(0)
    , m_sessionKey()
//, m_gap164()
{
    AddRef();

    ::InterlockedIncrement(&g_reporter.sockets);

    if (!::CreateTimerQueueTimer(&m_socketTimeoutTimer, CAuthServer::s_timerQueue, &CAuthSocket::SocketExTimerCallback, (void*)m_socket, g_Config.socketTimeOut, g_Config.socketTimeOut, 0))
    {
        g_winlog.AddLog(LOG_ERR, "create socket timer error");
    }

    if (g_Config.protocolVersion == 0x75FE)
    {
        m_encrypt_handler = &OldCrypt::encrypt_75FE;
        m_decrypt_handler = &OldCrypt::decrypt_75FE;
    }
    else if (g_Config.protocolVersion == 0x785A)
    {
        m_encrypt_handler = &Blowfish::EncryptPacket;
        m_decrypt_handler = &Blowfish::DecryptPacket;
    }
}

CAuthSocket::~CAuthSocket()  // 0x0043989F
{
}

CAuthSocket* CAuthSocket::Allocate(SOCKET socket)  // 0x004398BB
{
    return new CAuthSocket(socket);
}

bool CAuthSocket::Send(SOCKET socket, const char* format, ...)  // 0x00410A93
{
    va_list va;
    va_start(va, format);
    CAuthSocket* authSock = g_authServer.FindSocket(socket);
    if (authSock == nullptr)
    {
        return false;
    }

    CIOBuffer* buff = CIOBuffer::Alloc();
    uint8_t* packetBuffer = buff->m_Buffer;
    int packetSize = PacketUtils::VAssemble(&buff->m_Buffer[2], BUFFER_SIZE - 2, format, va);
    if (packetSize != 0)
    {
        --packetSize;
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", socket, format);
        return false;  // FIXED
    }

    if (buff->m_Buffer[2] == LS_PlayOk)
    {
        authSock->m_clientLoginState = LoginState_AcceptedByGS;
    }

    int size = packetSize + 1;
    authSock->m_encrypt_handler(packetBuffer + 2, authSock->m_xorKeySend, &size);
    size = size + g_Config.packetSizeType - 1;
    packetBuffer[0] = char((size + g_Config.packetSizeType - 1) & 0xFF);
    packetBuffer[1] = char((size >> 8) & 0xFF);

    buff->m_dwSize = size + 2;
    authSock->Write(buff);
    authSock->ReleaseRef();
    return true;
}

void CAuthSocket::OnTimerCallback()  // 0x00439FD8
{
    if (m_status == SocketStatus_Closed)
    {
        ReleaseRef();
    }
    else
    {
        if ((::GetTickCount() - m_lastPacketTime) >= (g_Config.socketTimeOut - 500))
        {
            CloseSocket();
        }
        ReleaseRef();
    }
}

void CAuthSocket::OnClose()  // 0x00439929
{
    HANDLE timer = (HANDLE)::InterlockedExchange((LPLONG)&m_socketTimeoutTimer, 0);
    if (timer)
    {
        ::DeleteTimerQueueTimer(CAuthServer::s_timerQueue, timer, 0);
    }

    m_socketTimeoutTimer = 0;
    m_status = SocketStatus_Closed;
    ::InterlockedDecrement(&g_reporter.sockets);

    g_authServer.RemoveSocket(m_socket);

    if (m_clientLoginState != LoginState_AcceptedByGS && m_uid != 0)
    {
        g_accountDb.removeAccountPreLogIn(m_uid, m_socket);
    }
    else
    {
        g_accountDb.UpdateSocket(m_uid, INVALID_SOCKET, m_sessionKey, 0);
    }

    in_addr ipAddress = getaddr();
    g_ipAccessLimit.DelAccessIP(ipAddress);
}

void CAuthSocket::OnRead()  // 0x00439A04
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
        // read size only
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

            if ((dwRead + 3) > buffSize)
            {
                return Read(buffSize - dwRead);
            }

            m_packetSize = buffer[dwRead] + (buffer[dwRead + 1] << 8) + 1 - g_Config.packetSizeType;

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

        if ((m_packetSize + dwRead) > buffSize)
        {
            return Read(buffSize - dwRead);
        }

        if (!m_decrypt_handler(&buffer[2], m_xorKeyReceive, m_packetSize))
        {
            CloseSocket();
            return;
        }

        if (buffer[dwRead] >= HANDLERS_NUMBER)
        {
            break;
        }

        CAuthPacket* packet = CAuthPacket::Alloc();
        packet->m_pSocket = this;
        packet->m_pBuf = m_pReadBuf;

        // 2 bytes size, 3rd byte is packet type
        packet->m_pFunc = m_packetHandlers[buffer[dwRead]];

        AddRef();
        m_pReadBuf->AddRef();

        ::InterlockedIncrement(&CAuthPacket::g_nPendingPacket);

        packet->PostObject(dwRead, Threading::g_hCompletionPort);

        dwRead += m_packetSize;
        m_status = SocketStatus_Init;
        m_lastPacketTime = ::GetTickCount();
    }

    g_winlog.AddLog(LOG_ERR, "unknown protocol %d", buffer[dwRead]);
    CloseSocket();
}

void CAuthSocket::OnCreate()  // 0x00439C87
{
    static const int MAGIC_NUMBER = 0x87546CA1;

    int someRand = std::rand() << 16;
    m_sessionKey = std::rand() | someRand;

    memcpy(&m_xorKeySend[0], &m_sessionKey, sizeof(m_sessionKey));
    memcpy(&m_xorKeySend[4], &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));

    memcpy(&m_xorKeyReceive[0], &m_sessionKey, sizeof(m_sessionKey));
    memcpy(&m_xorKeyReceive[4], &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));

    if (g_Config.encrypt == true)
    {
        NonEncSend("cdd", LS_Init, m_sessionKey, g_Config.protocolVersion);
    }
    else
    {
        NonEncSend("cdd", LS_Init, m_sessionKey, 0);
    }

    //     AddRef(); FIX ? Missed?
    OnRead();
}

LoginFailReason CAuthSocket::CheckPersonalPayStat(LoginUser* userInfo, int uid)  // 0x0040481A
{
    LoginFailReason errorCode = REASON_SUCCESS;
    int serverFlag = userInfo->payStat % 1000 / 100;
    int totalTime = 0;
    if (userInfo->payStat == 0)
    {
        return REASON_USAGE_TERM_EXPIRED;
    }

    if (serverFlag == 2)
    {
        errorCode = AccountDB::UserTimeLogin(uid, userInfo, &totalTime);
    }
    else if (serverFlag == 3)  // free server?
    {
        CDBConn sql(&g_linDB);
        (void)sql;
    }

    if (errorCode != REASON_SUCCESS)
    {
        Send("cc", LS_LoginFail, errorCode);
        return errorCode;
    }

    if (g_accountDb.RegAccount(*userInfo, uid, this, totalTime, 0))
    {
        m_lastPacketTime = ::GetTickCount();
        Utils::WriteLogD(801, userInfo->accountName, userInfo->connectedIP, userInfo->payStat, userInfo->age, userInfo->sexAndCentury, 0, g_reporter.users, uid);
    }
    // TODO: if AccountDB::authenticateUser failed?

    return errorCode;
}

int CAuthSocket::GetMd5Key() const  // 0x004056D0
{
    return m_sessionKey;
}

void NTAPI CAuthSocket::SocketExTimerCallback(PVOID param, BOOLEAN)  // 0x004396FC
{
    SOCKET socket = (SOCKET)(param);
    CAuthSocket* userSocket = g_authServer.FindSocket(socket);
    if (userSocket != nullptr)
    {
        userSocket->OnTimerCallback();
    }
}

void CAuthSocket::SetAddress(int ipAddress)  // 0x00439D3C
{
    m_clientIP.s_addr = ipAddress;
}

bool CAuthSocket::Send(const char* format, ...)  // 0x00439D55
{
    va_list va;
    va_start(va, format);

    if (m_status != SocketStatus_Closed)
    {
        CIOBuffer* buff = CIOBuffer::Alloc();
        int packetSize = PacketUtils::VAssemble(&buff->m_Buffer[2], BUFFER_SIZE - 2, format, va);
        if (packetSize != 0)
        {
            m_encrypt_handler(&buff->m_Buffer[2], m_xorKeySend, &packetSize);

            packetSize += g_Config.packetSizeType - 1;
            buff->m_Buffer[0] = char(packetSize & 0xFF);
            buff->m_Buffer[1] = char((packetSize >> 8) & 0xFF);

            buff->m_dwSize = packetSize;
            Write(buff);

            m_lastPacketTime = ::GetTickCount();
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", m_hSocket, format);
        }
    }

    return true;
}

void CAuthSocket::NonEncSend(const char* format, ...)  // 0x00439E5B
{
    va_list va;
    va_start(va, format);
    if (m_status != SocketStatus_Closed)
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
            g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", m_hSocket, format);
        }

        buff->m_dwSize = packetSize;
        Write(buff);

        m_lastPacketTime = ::GetTickCount();
    }
}

void CAuthSocket::SendPacket(const char* packet, size_t packetSize)  // 0x00439F30
{
    if (m_status != SocketStatus_Closed)
    {
        CIOBuffer* buff = CIOBuffer::Alloc();
        memcpy(buff->m_Buffer, packet, packetSize);

        int payloadSize = packetSize - 2;
        m_encrypt_handler(&buff->m_Buffer[2], m_xorKeySend, &payloadSize);

        packetSize = payloadSize + g_Config.packetSizeType - 1;
        buff->m_Buffer[0] = char(packetSize & 0xFF);
        buff->m_Buffer[1] = char((packetSize >> 8) & 0xFF);

        buff->m_dwSize = packetSize;
        Write(buff);
    }
}

in_addr CAuthSocket::getaddr() const  // 0x0043A080
{
    return m_clientIP;
}

bool CAuthSocket::packet00_RequestAuthLogin(CAuthSocket* authSock, uint8_t* buffer)  // 0x0043818F
{
    in_addr ipAddress = authSock->getaddr();
    if (g_Config.useForbiddenIPList && g_blockedIPs.IpExists(ipAddress))
    {
        authSock->Send("cc", LS_LoginFail, REASON_RESTRICTED_IP);
        return false;
    }

    authSock->m_lastPacketTime = ::GetTickCount();

    char account[16];
    char password[17];
    memset(account, 0, sizeof(account));
    memset(password, 0, sizeof(password));

    char credentials[30];
    if (g_Config.desApply)
    {
        memcpy(credentials, buffer, sizeof(credentials));
        buffer += sizeof(credentials);

        DesFrontEnd::DesReadBlock(credentials, sizeof(credentials));

        strncpy(account, credentials, 14u);
        strncpy(password, &credentials[14], 16);
    }
    else
    {
        memcpy(account, buffer, 14u);
        buffer += 14;
        memcpy(password, buffer, 16u);
        buffer += 16;
    }

    int notifyGameServer = BufferReader::ReadInt(&buffer);
    short clientCookie = BufferReader::ReadShort(&buffer);

    CAccount userTable;
    LoginFailReason error;
    if (g_Config.newEncrypt)
    {
        error = userTable.CheckNewPassword(account, password);
    }
    else
    {
        error = userTable.CheckPassword(account, password);
    }

    if (error != REASON_SUCCESS)
    {
        authSock->Send("cc", LS_LoginFail, error);
        return false;
    }

    if (userTable.blockFlag_standard != 0)
    {
        // TODO BUGGY!!
        const TIMESTAMP_STRUCT& blockEndDate = userTable.block_end_date;
        authSock->Send(
            "cdd",
            LS_AccountKicked,
            userTable.blockFlag_standard,
            *&blockEndDate.year,    // year + month
            *&blockEndDate.day,     // day + hour
            *&blockEndDate.minute,  // minute + second
            blockEndDate.fraction);
        return false;
    }

    if (userTable.blockFlag_custom != 0)
    {
        char blockPkg[0x1000];
        memset(blockPkg, 0, sizeof(blockPkg));

        int blockMsgLength = userTable.MakeBlockInfo(&blockPkg[3]);
        int blockPacketSize = g_Config.packetSizeType + blockMsgLength;
        blockPkg[0] = char(blockPacketSize & 0xFF);
        blockPkg[1] = char((blockPacketSize >> 8) & 0xFF);
        blockPkg[2] = LS_AccountBlocked;
        authSock->SendPacket(blockPkg, blockPacketSize);
        return false;
    }

    if (g_Config.countryCode == 0)  // Korea
    {
        int age = userTable.age;
        if (age < g_serverList.GetAgeLimit())  // TODO: newServerList (Table) is not handled here
        {
            authSock->Send("cc", LS_LoginFail, REASON_UNDER_18_YEARS_KR);
            return false;
        }
    }

    // 0x10 - GM?
    if ((userTable.loginFlag & 0x10) && g_Config.restrictGMIP && (authSock->getaddr().s_addr != g_Config.gmIP.s_addr))
    {
        in_addr ipAddress = authSock->getaddr();
        CDBConn sql(&g_linDB);
        sql.Execute("INSERT gm_illegal_login ( account, ip ) VALUES ( '%s', '%d.%d.%d.%d' )", userTable.account, ipAddress.S_un.S_un_b.s_b1, ipAddress.S_un.S_un_b.s_b2, ipAddress.S_un.S_un_b.s_b3, ipAddress.S_un.S_un_b.s_b4);

        authSock->Send("cc", LS_LoginFail, REASON_SYSTEM_ERROR);  // not a gm IP
        return false;
    }

    if (g_Config.freeServer && userTable.payStat != 0)
    {
        userTable.payStat = 1002;
    }

    LoginUser* userInfo = new LoginUser();
    userInfo->connectedIP = authSock->getaddr();
    userInfo->sessionKey = authSock->GetMd5Key();
    userInfo->payStat = userTable.payStat;
    userInfo->gameSocket = authSock->m_hSocket;
    userInfo->sexAndCentury = userTable.sexAndCentury;
    userInfo->timerHandler = nullptr;
    userInfo->loginState = LoginState_Connected;
    userInfo->birthdayEncoded = userTable.birthdayEncoded;
    userInfo->restOfSsn = userTable.restOfSsn;
    userInfo->loginFlag = userTable.loginFlag;
    userInfo->warnFlag = userTable.warnFlag;
    userInfo->age = userTable.age;
    userInfo->clientCookie = clientCookie;
    strncpy(userInfo->accountName, userTable.account, sizeof(userInfo->accountName));
    userInfo->accountName[14] = 0;
    userInfo->lastworld = userTable.lastworld;
    userInfo->loginTime = std::time(0);
    userInfo->loggedGameServerId = 0;
    userInfo->selectedGServerId = 0;
    userInfo->notifyGameServer = notifyGameServer;  // 8 (dont) or 16 (report)
    authSock->m_uid = userTable.uid;

    if (!g_Config.useIPServer)
    {
        authSock->CheckPersonalPayStat(userInfo, userTable.uid);
        delete userInfo;
        return false;
    }

    if (g_Config.pcCafeFirst)
    {
        int errorCode = g_IPSessionDB.AcquireSessionRequest(userInfo, userTable.uid);
        if (errorCode != 0)
        {
            if (errorCode == 11)
            {
                authSock->Send("cc", LS_LoginFail, REASON_ACCOUNT_IN_USE);
                delete userInfo;
            }
            else
            {
                authSock->CheckPersonalPayStat(userInfo, userTable.uid);
                delete userInfo;
            }
        }
        return false;
    }

    int payStatFlag = (userTable.payStat % 1000) / 100;
    if (userTable.payStat != 0 && payStatFlag != 2)
    {
        authSock->CheckPersonalPayStat(userInfo, userTable.uid);
        delete userInfo;
        return false;
    }

    int errorCode = g_IPSessionDB.AcquireSessionRequest(userInfo, userTable.uid);
    if (errorCode != 0)
    {
        if (errorCode == 11)
        {
            authSock->Send("cc", LS_LoginFail, REASON_ACCESS_FAILED);  //  acc in use
            delete userInfo;
        }
        else
        {
            authSock->CheckPersonalPayStat(userInfo, userTable.uid);
            delete userInfo;
        }
    }

    return false;
}

bool CAuthSocket::packet01_ServerListPacket(CAuthSocket* authSock, uint8_t* buffer)  // 0x0043896C
{
    int uid = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);
    UNUSED(sessionKey);
    authSock->m_uid = uid;

    char accountName[16];
    int lastWorld = g_accountDb.FindAccount(uid, accountName);
    if (lastWorld >= 0)
    {
        authSock->m_lastPacketTime = ::GetTickCount();
        // CServerList Format: cc [cddcchhcdc]
        // c: server list size (number of servers)
        // c: ?
        // [ (repeat for each servers)
        //     c: server id (ignored by client?)
        //     d: server ip
        //     d: server port
        //     c: age limit (used by client?)
        //     c: pvp or not (used by client?)
        //     h: current number of players
        //     h: max number of players
        //     c: 0 if server is down
        //     d: 2nd bit: clock 3rd bit: wont dsiplay server name 4th bit: test server (used by
        //     client?)
        //     c: 0 if you dont want to display brackets in front of sever name ]
        // Server will be considered as Good when the number of online players is less
        // than half the maximum. as Normal between half and 4/5 and Full when there's more
        // than 4/5 of the maximum number of players
        //
        if (g_serverList.m_defaultOrCompressedSize >= 10)
        {
            char serverListPacket[1024];
            memcpy(&serverListPacket[3], g_serverList.m_compressedFrameBuffers, g_serverList.m_defaultOrCompressedSize);
            int packetSize = g_serverList.m_defaultOrCompressedSize + g_Config.packetSizeType;
            serverListPacket[0] = char(packetSize & 0xFF);
            serverListPacket[1] = char((packetSize >> 8) & 0xFF);
            serverListPacket[2] = LS_ServerList;
            authSock->SendPacket(serverListPacket, packetSize);
            return false;
        }

        g_winlog.AddLog(LOG_ERR, "Server List Error, Check lin2DB Server Table");
        authSock->Send("cc", LS_ServerListFail, 6);
        return false;
    }

    return false;
}

bool CAuthSocket::packet02_RequestServerLogin(CAuthSocket* authSock, uint8_t* buffer)  // 0x00438A69
{
    char accName[15];
    memset(accName, 0, sizeof(accName));
    int uid = BufferReader::ReadInt(&buffer);
    int receivedSessionKey = BufferReader::ReadInt(&buffer);
    int selectedGServerId = BufferReader::ReadByte(&buffer);
    int payStat = 0;
    int sessionKey = 0;
    int loginFlag = 0;
    int warnFlag = 0;

    authSock->m_lastPacketTime = ::GetTickCount();
    if (!g_accountDb.FindAccount(uid, accName, &loginFlag, &warnFlag, &payStat, &sessionKey))
    {
        authSock->Send("cc", LS_PlayFail, REASON_INVALID_STATE);
        return false;
    }

    if (sessionKey != receivedSessionKey)
    {
        authSock->Send("cc", LS_PlayFail, REASON_INVALID_STATE);
        g_winlog.AddLog(LOG_ERR, "md5key does not matching");
        return false;
    }

    Utils::StdAccount(accName);
    if (selectedGServerId < 1 || g_serverList.m_serverNumber < selectedGServerId)
    {
        authSock->Send("cc", LS_PlayFail, REASON_IP_SESSION_REJECTED);
        return false;
    }

    if (payStat < 1000 && g_Config.useIPServer)
    {
        int errorCode = g_IPSessionDB.ReadyToIPCharge(uid, authSock->getaddr(), payStat, selectedGServerId);
        if (errorCode == 10)
        {
            authSock->Send("cc", LS_PlayFail, REASON_SYSTEM_ERROR);
        }
        return false;
    }

    int totalTime = 0;
    LoginFailReason timeError = REASON_SUCCESS;
    int payFlags = payStat % 1000 / 100;
    if (payFlags && payFlags == 2)
    {
        timeError = AccountDB::CheckUserTime(uid, &totalTime);
    }

    if (timeError != REASON_SUCCESS)
    {
        authSock->Send("cc", LS_PlayFail, timeError);
        return false;
    }

    authSock->AddRef();
    g_accountDb.AboutToPlay(uid, accName, totalTime, loginFlag, warnFlag, receivedSessionKey, authSock, selectedGServerId, payStat);
    authSock->ReleaseRef();
    return false;
}

bool CAuthSocket::packet03_CancelLogin(CAuthSocket* /*authSock*/, uint8_t* buffer)  // 0x00438D5A
{
    int uid = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);
    return g_accountDb.logoutAccount(uid, sessionKey) != false;
}

bool CAuthSocket::packet05_RequestServerList_C1(CAuthSocket* authSock, uint8_t* buffer)  // 0x00438DA3
{
    int uid = BufferReader::ReadInt(&buffer);
    int sessionKey = BufferReader::ReadInt(&buffer);
    UNUSED(sessionKey);
    char format = BufferReader::ReadByte(&buffer);
    if (format > 4 || format < 0)  // FIXED
    {
        authSock->Send("cc", LS_ServerListFail, 6);
        return false;
    }

    FrameType frameFormat = FrameType_WithKind;
    authSock->m_uid = uid;

    char accountName[16];
    int lastWorld = g_accountDb.FindAccount(uid, accountName);
    if (lastWorld < 0)
    {
        return true;
    }

    char packetBuffer[2048];
    int framesBufferSize = 0;
    authSock->m_lastPacketTime = ::GetTickCount();
    if (g_Config.newServerList)
    {
        int serverKind = BufferReader::ReadInt(&buffer);
        if (serverKind < 0)
        {
            authSock->Send("cc", LS_ServerListFail, 20);
            return false;
        }

        int serverKindIndex = 0;
        while (serverKind > 0)
        {
            ++serverKindIndex;
            serverKind >>= 1;
        }
        if (serverKindIndex < 0 || serverKindIndex >= g_serverKind.GetServerMaxKind())
        {
            authSock->Send("cc", LS_ServerListFail, 6);
            return false;
        }

        char* servListBuffer = 0;
        framesBufferSize = g_serverKind.GetServerPacketList(frameFormat, serverKindIndex, &servListBuffer);
        if (framesBufferSize <= 0)
        {
            authSock->Send("cc", LS_ServerListFail, 20);
            return false;
        }

        memcpy(&packetBuffer[3], servListBuffer, framesBufferSize);
    }
    else
    {
        if (g_serverList.m_frameSizes[frameFormat] < 10)
        {
            g_winlog.AddLog(LOG_ERR, "Server List Error, Check lin2DB Server Table");
            authSock->Send("cc", LS_ServerListFail, 6);
            return false;
        }

        framesBufferSize = g_serverList.m_frameSizes[frameFormat];
        memcpy(&packetBuffer[3], g_serverList.m_frameLists[frameFormat], framesBufferSize);
    }

    if (frameFormat > 0)
    {
        packetBuffer[4] = lastWorld;
    }

    int packetSize = framesBufferSize + g_Config.packetSizeType;
    packetBuffer[0] = char(packetSize & 0xFF);
    packetBuffer[1] = char((packetSize >> 8) & 0xFF);
    packetBuffer[2] = LS_ServerList;
    authSock->SendPacket(packetBuffer, packetSize);

    return false;
}

bool CAuthSocket::packet04_unknownHandler(CAuthSocket* authSock, uint8_t* /*buffer*/)  // 0x00439225
{
    char pass[16];
    char accName[16];

    memset(accName, 0, sizeof(accName));
    memset(pass, 0, sizeof(pass));
    authSock->m_lastPacketTime = ::GetTickCount();
    return false;
}

bool CAuthSocket::packet06_unknownHandler(CAuthSocket* /*authSock*/, uint8_t* buffer)  // 0x0043925D
{
    int uid = BufferReader::ReadInt(&buffer);

    char accountName[16];
    accountName[0] = 0;
    if (g_accountDb.FindAccount(uid, accountName) < 0)
    {
        return true;
    }

    char lastWorld = 0;
    CDBConn sql(&g_linDB);
    sql.bindByte(&lastWorld);
    if (!sql.Execute("SELECT world_id FROM user_char WITH (NOLOCK) WHERE uid = %d", uid))
    {
        return false;
    }

    int serversNumber = g_serverList.GetMaxServerNum();
    char* serversTable = new char[serversNumber + 1];
    memset(serversTable, 0, serversNumber + 1);

    bool notFound = true;
    while (sql.Fetch(&notFound) && notFound == false)
    {
        if (lastWorld > 0 && lastWorld <= serversNumber)
        {
            serversTable[lastWorld] = 1;
        }
    }

    for (int serverId = 1; serverId <= serversNumber; ++serverId)
    {
        if (serversTable[serverId] != 0)
        {
            WorldServer worldServer = g_serverList.GetAt(serverId);
            WorldSrvServer::SendSocket(worldServer.ipAddress, "cd", 6, uid);
        }
    }

    delete[] serversTable;

    return true;
}

bool CAuthSocket::packet07_charsHandler(CAuthSocket* authSock, uint8_t* buffer)  // 0x00439436
{
    int uid = BufferReader::ReadInt(&buffer);
    char serverId = BufferReader::ReadByte(&buffer);
    short binarySize = BufferReader::ReadShort(&buffer);
    CDBConn sql(&g_linDB);

    int userCharNumber = AccountDB::GetCharNum(uid);
    if (userCharNumber < 0)
    {
        authSock->Send("ccdd", LS_CharSelection, 1, uid, 0);
        return false;
    }

    if (userCharNumber > 10)
    {
        authSock->Send("ccdd", LS_CharSelection, 29, uid, 0);
        return false;
    }
    if (serverId <= 0 || (serverId > g_serverList.GetMaxServerNum()))
    {
        authSock->Send("ccdd", LS_CharSelection, 8, uid, 0);
        return false;
    }

    WorldServer worldServer = g_serverList.GetAt(serverId);
    bool success = WorldSrvServer::SendSocket(worldServer.ipAddress, "cdchb", 7, uid, serverId, binarySize, binarySize, buffer);
    if (!success)
    {
        authSock->Send("ccdd", LS_CharSelection, 8, uid, 0);
    }

    return false;
}

bool CAuthSocket::packet08_unknownHandler(CAuthSocket* authSock, uint8_t* buffer)  // 0x00439627
{
    int uid = BufferReader::ReadInt(&buffer);
    int serverId = BufferReader::ReadInt(&buffer);
    int v9 = BufferReader::ReadInt(&buffer);
    short zero = 0;
    if (serverId <= 0 || serverId > g_serverList.GetMaxServerNum())
    {
        // TODO: bug, 4/3 parametes
        authSock->Send("cdch", LS_CharDeleted, 8, zero);  // ???
        return false;
    }

    WorldServer worldServer = g_serverList.GetAt(serverId);
    bool success = WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcd", 8, uid, v9);
    if (!success)
    {
        // TODO: bug, 4/3 parametes
        authSock->Send("cdch", LS_CharDeleted, 8, zero);  // ???
    }
    return false;
}
