#include "network/CSocketInt.h"
#include "config/Config.h"
#include "db/CAccount.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "model/AccountDB.h"
#include "model/CServerList.h"
#include "network/CIOBuffer.h"
#include "network/WorldSrvServer.h"
#include "ui/CLog.h"
#include "utils/Utils.h"

#include <varargs.h>

const CSocketInt::PacketHandler CSocketInt::handlers[CSocketInt::HANDLERS_NUMBER] = {&CSocketInt::packet00_kickByUid,
                                                                                     &CSocketInt::packet01_changeSocketLimit,
                                                                                     &CSocketInt::packet02_getUserNumber,
                                                                                     &CSocketInt::packet03_kickByAccNameFromAuthServer,
                                                                                     &CSocketInt::packet04_changeGmMode,
                                                                                     &CSocketInt::packet05_stub,
                                                                                     &CSocketInt::packet06_changeGmIp,
                                                                                     &CSocketInt::packet07_stub,
                                                                                     &CSocketInt::packet08_changeServerMode,
                                                                                     &CSocketInt::packet09_getLoginFlag,
                                                                                     &CSocketInt::packet10_checkUser,
                                                                                     &CSocketInt::packet11_kickUserFromWorldServer};

// 0x0040F25A
CSocketInt::CSocketInt(SOCKET socket)
    : CIOSocket(socket)
    , m_socket(socket)
    , m_status(SocketStatus_Init)
    , m_packetHandlers(CSocketInt::handlers)
    , m_clientIP()
{
    m_clientIP.s_addr = 0;
}

// 0x0040F2B0
CSocketInt::~CSocketInt()
{
}

// 0x0040F2CC
CSocketInt* CSocketInt::Allocate(SOCKET socket)
{
    return new CSocketInt(socket);
}

// 0x0040F337
void CSocketInt::OnClose()
{
    m_status = SocketStatus_Closed;
    g_winlog.AddLog(LOG_INF, "*close connection from %s, %x(%x)", IP(), m_socket, this);
}

// 0x0040F374
void CSocketInt::OnRead()
{
    // FIXED     v     v     v
    if (m_status == SocketStatus_Closed)
    {
        CloseSocket();
        return;
    }
    //     ^       ^       ^

    int bufferEnd = 0;
    int buffSize = m_pReadBuf->m_dwSize;
    char* buffer = (char*)m_pReadBuf->m_Buffer;

    while (true)
    {
        int bytesLeft = buffSize--;
        if (bytesLeft <= 0)
        {
            break;
        }

        // looking for the end of line:
        if (bufferEnd < 255 && buffer[bufferEnd] != '\n')
        {
            ++bufferEnd;
            continue;
        }

        // then handle this chunk:
        buffer[bufferEnd] = '\0';
        Process(buffer);
        buffer += bufferEnd + 1;

        // and continue for next line (if any)
        bufferEnd = 0;
    }

    Read(bufferEnd);
}

// 0x0040F475
void CSocketInt::OnCreate()
{
    AddRef();
    OnRead();
}

// 0x0040F416
void CSocketInt::Process(const char* command)
{
    int commandId = -1;
    ::sscanf(command, "%d", &commandId);
    if (commandId >= 0 && commandId < CSocketInt::HANDLERS_NUMBER)
    {
        PacketHandler handler = m_packetHandlers[commandId];
        handler(this, command);
    }
}

// 0x0040F45D
char* CSocketInt::IP() const
{
    return ::inet_ntoa(m_clientIP);
}

// 0x0040F493
void CSocketInt::SetAddress(int ipAddress)
{
    m_clientIP.s_addr = ipAddress;
}

// 0x0040F4A9
bool CSocketInt::Send(const char* format, ...)
{
    va_list va;
    va_start(va, format);

    if (m_status == SocketStatus_Closed)
    {
        return false;
    }

    CIOBuffer* buff = CIOBuffer::Alloc();
    int size = vsprintf((char*)buff->m_Buffer, format, va);
    buff->m_dwSize = size;
    Write(buff);

    return true;
}

// 0x0040F529
void CSocketInt::SendBuffer(const char* message, size_t size)
{
    if ((m_status != SocketStatus_Closed) && (size > 0) && (size <= BUFFER_SIZE))
    {
        CIOBuffer* buff = CIOBuffer::Alloc();
        memcpy(buff->m_Buffer, message, size);
        buff->m_dwSize = size;
        Write(buff);
    }
}

// 0x0040E64A
const char* CSocketInt::subcommand(char* out, const char* in)
{
    for (int i = 0; i < 255; ++i)
    {
        *out = *in++;
        if (*out == 0)
        {
            return NULL;
        }

        // subcommand found and copied to the "out", return pointer to current token
        if (*out == '\t' || *out == '\r')
        {
            *out = '\0';
            return in;
        }

        ++out;
    }

    *out = '\0';
    return NULL;
}

// 0x0040E560
void CSocketInt::packet00_kickByUid(CSocketInt* socketInt, const char* command)
{
    char uidStr[256];
    const char* token = subcommand(uidStr, command);
    if (token != NULL)
    {
        subcommand(uidStr, token);
        int uidLen = ::strlen(uidStr);
        for (int i = 0; i < uidLen; ++i)
        {
            if (!::isdigit(uidStr[i]))
            {
                socketInt->Send("0");
                return;
            }
        }

        int uid = ::atoi(uidStr);
        g_accountDb.KickAccount(uid, 11, true);
        socketInt->Send("1");
    }
    else
    {
        socketInt->Send("0");
    }
}

// 0x0040E917
void CSocketInt::packet01_changeSocketLimit(CSocketInt* socketInt, const char* command)
{
    char buffer[268];

    const char* token = subcommand(buffer, command);
    if (token != NULL)
    {
        subcommand(buffer, token);
        int socketLimit = ::atoi(buffer);
        if (socketLimit > 0 && socketLimit < 12000)
        {
            g_Config.socketLimit = socketLimit;
            g_winlog.AddLog(LOG_DBG, "Change socketlimit, %d", socketLimit);
            socketInt->Send("1");
        }
    }
}

// 0x0040E6C2
void CSocketInt::packet02_getUserNumber(CSocketInt* socketInt, const char* /*command*/)
{
    int users = g_accountDb.GetUserNum();
    socketInt->Send("%d", users);
}

// 0x0040E6E3
void CSocketInt::packet03_kickByAccNameFromAuthServer(CSocketInt* socketInt, const char* command)
{
    char accountName[256];
    memset(accountName, 0, 32u);

    const char* token = subcommand(accountName, command);
    if (token != NULL)
    {
        subcommand(accountName, token);
        accountName[14] = 0;

        CDBConn sql(&g_linDB);
        int uid = 0;
        sql.bindInt(&uid);
        sql.Execute("SELECT uid FROM user_account with (NOLOCK) WHERE account = '%s'", accountName);

        bool notFound = true;
        if (sql.Fetch(&notFound))
        {
            if (notFound)
            {
                socketInt->Send("0,InvalidAccount");
            }
            else
            {
                g_accountDb.KickAccount(uid, 11, true);
                socketInt->Send("1");
            }
        }
        else
        {
            socketInt->Send("0,InvalidAccount");
        }
    }
    else
    {
        socketInt->SendBuffer("0,incorrect format", sizeof("0,incorrect format"));
    }
}

// 0x0040E849
void CSocketInt::packet04_changeGmMode(CSocketInt* socketInt, const char* command)
{
    char buffer[256];
    const char* token = subcommand(buffer, command);
    if (token != NULL)
    {
        memset(buffer, 0, 32u);
        subcommand(buffer, token);
        if (buffer[0] == '0')
        {
            g_Config.GMCheckMode = false;
            g_winlog.AddLog(LOG_DBG, "GM ONLY MODE OFF");
            socketInt->Send("1");
        }
        else
        {
            g_Config.GMCheckMode = true;
            g_winlog.AddLog(LOG_DBG, "GM ONLY MODE ON");
            socketInt->Send("1");
        }
    }
    else
    {
        socketInt->Send("0,Incorrect Format");
    }
}

// 0x0040E9BE
void CSocketInt::packet05_stub(CSocketInt* /*socketInt*/, const char* /*command*/)
{
}

// 0x0040E9C3
void CSocketInt::packet06_changeGmIp(CSocketInt* socketInt, const char* command)
{
    char buffer[256];

    const char* token = subcommand(buffer, command);
    if (token != NULL)
    {
        subcommand(buffer, token);
        buffer[16] = 0;
        unsigned long ipAddr = ::inet_addr(buffer);
        if (ipAddr == -1)
        {
            socketInt->Send("0");
        }
        else
        {
            in_addr addr;
            addr.s_addr = ipAddr;

            g_Config.gmIP = addr;

            g_winlog.AddLog(LOG_DBG, "Change GMIP, %d.%d.%d.%d", addr.S_un.S_un_b.s_b1, addr.S_un.S_un_b.s_b2, addr.S_un.S_un_b.s_b3, addr.S_un.S_un_b.s_b4);

            socketInt->Send("1");
        }
    }
    else
    {
        socketInt->Send("0");
    }
}

// 0x0040EA99
void CSocketInt::packet07_stub(CSocketInt* /*socketInt*/, const char* /*command*/)
{
}

// 0x0040EA9E
void CSocketInt::packet08_changeServerMode(CSocketInt* socketInt, const char* command)
{
    char buffer[256];

    const char* token = subcommand(buffer, command);
    if (token == NULL)
    {
        socketInt->Send("0");
        return;
    }
    subcommand(buffer, token);
    if (buffer[0] == '0')
    {
        g_Config.freeServer = false;
        g_winlog.AddLog(LOG_DBG, "Config FreeServer is chaged to false");
        socketInt->Send("1");
        return;
    }

    if (buffer[0] == '1')
    {
        g_Config.freeServer = true;
        g_winlog.AddLog(LOG_DBG, "Config FreeServer is changed to true");
        socketInt->Send("1");
        return;
    }

    socketInt->Send("0");
}

// 0x0040EB66
void CSocketInt::packet09_getLoginFlag(CSocketInt* socketInt, const char* command)
{
    char buffer[256];
    const char* token = subcommand(buffer, command);
    if (token == NULL)
    {
        socketInt->Send("-OCHK\t004\t\n");
        return;
    }

    token = subcommand(buffer, token);
    if (token == NULL)
    {
        socketInt->Send("-OCHK\t003\tError\r\n");
        return;
    }

    char accName[16];
    ::strncpy(accName, buffer, sizeof(accName));
    token = subcommand(buffer, token);
    if (token == NULL)
    {
        socketInt->Send("-OCHK\t003\tError\r\n");
        return;
    }

    char password[20];
    memset(password, 0, sizeof(password));
    ::strncpy(password, buffer, sizeof(password));

    char key[64];
    memset(key, 0, sizeof(key));

    subcommand(buffer, token);
    ::strncpy(key, buffer, 63u);

    CAccount userInfoTable;
    LoginFailReason findUserResult = g_Config.newEncrypt ? userInfoTable.CheckNewPassword(accName, password) : userInfoTable.CheckPassword(accName, password);

    if (findUserResult == REASON_ACCESS_FAILED_TRY_AGAIN_LATER)
    {
        socketInt->Send("-OCHK\t201\tError\t%s\r\n", key);
        return;
    }

    if (findUserResult == REASON_USER_OR_PASS_WRONG)
    {
        socketInt->Send("-OCHK\t202\tError\t%s\r\n", key);
        return;
    }

    if (userInfoTable.blockFlag_custom || userInfoTable.blockFlag_standard)
    {
        socketInt->Send("+OCHK\t%d\t%s\t%d\r\n", userInfoTable.loginFlag | 0x80000000, key, userInfoTable.uid);
    }
    else
    {
        socketInt->Send("+OCHK\t%d\t%s\t%d\r\n", userInfoTable.loginFlag, key, userInfoTable.uid);
    }
}

// 0x0040EDD3
void CSocketInt::packet10_checkUser(CSocketInt* socketInt, const char* command)
{
    char buffer[256];

    const char* token = subcommand(buffer, command);
    if (token == NULL)
    {
        socketInt->Send("%d", 1);
        return;
    }

    buffer[0] = 0;
    token = subcommand(buffer, token);
    if (token == NULL)
    {
        socketInt->Send("%d", 1);
        return;
    }

    char accName[16];
    memset(accName, 0, 15u);
    ::strncpy(accName, buffer, 14u);
    accName[14] = 0;

    if (!Utils::CheckAccount(accName))
    {
        socketInt->Send("%d", 2);
        return;
    }

    buffer[0] = 0;
    token = subcommand(buffer, token);
    if (token == NULL)
    {
        socketInt->Send("%d", 1);
        return;
    }

    buffer[16] = 0;

    char password[18];
    memset(password, 0, 17u);
    ::strncpy(password, buffer, 16u);

    CAccount userInfoTable;
    LoginFailReason findUserResult = g_Config.newEncrypt ? userInfoTable.CheckNewPassword(accName, password) : userInfoTable.CheckPassword(accName, password);

    if (findUserResult != REASON_SUCCESS)
    {
        socketInt->Send("%d", 3);
    }
    else
    {
        socketInt->Send("%d", 0);
    }
}

// 0x0040EFC9
void CSocketInt::packet11_kickUserFromWorldServer(CSocketInt* socketInt, const char* command)
{
    g_winlog.AddLog(LOG_WRN, "%s", command);

    char buffer[256];
    const char* token = subcommand(buffer, command);
    if (token == NULL)
    {
        socketInt->Send("%d", 1);
        return;
    }

    buffer[0] = 0;
    token = subcommand(buffer, token);

    char accName[15];
    memset(accName, 0, 15u);
    ::strncpy(accName, buffer, 14u);
    accName[14] = 0;
    if (!Utils::CheckAccount(accName))
    {
        socketInt->Send("%d", 2);
        return;
    }

    CAccount userInfoTable;
    LoginFailReason error = userInfoTable.Load(accName);
    if (error != REASON_SUCCESS)
    {
        socketInt->Send("%c", 2);
        return;
    }

    buffer[0] = 0;
    token = subcommand(buffer, token);
    int parameterLen = ::strlen(buffer);
    if (!Utils::IsValidNumeric(buffer, parameterLen))
    {
        socketInt->Send("%d", 4);
        return;
    }

    int serverId = ::atoi(buffer);
    subcommand(buffer, token);
    if (serverId <= 0 || serverId > g_serverList.m_serverNumber)
    {
        socketInt->Send("%d", 4);
        return;
    }

    WorldServer worldServer = g_serverList.GetAt(serverId);
    if (g_worldServServer.GetServerStatus(worldServer.ipAddress))
    {
        WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcs", 1, userInfoTable.uid, 7, accName);
        socketInt->Send("%d", 0);
    }
    else
    {
        socketInt->Send("%d", 5);
    }
}
