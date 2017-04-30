#include "model/CServerList.h"
#include "config/Config.h"
#include "threads/Threading.h"

#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "network/WorldSrvServer.h"
#include "ui/CLog.h"
#include "utils/CExceptionInit.h"

#include <cstdio>

CServerList g_serverList;

// 0x004337BE
CServerList::CServerList()
    : m_serverCount(0)
    , m_worldPort()
    , m_maxAgeLimit(0)
    , m_rwLock()
    , m_servers()
    , m_tmpServer()
    , m_defaultOrCompressedSize(0)
    , m_serverNumber()
{

    for (int i = 0; i < 10; ++i)
    {
        m_frameLists[i] = nullptr;
        m_frameSizes[i] = 0;
    }
}

// 0x0043387B
CServerList::~CServerList()
{
    for (int i = 1; i < 10; ++i)  // TODO: 0 is not deleted?
    {
        if (m_frameLists[i] != nullptr)
        {
            delete[] m_frameLists[i];
        }
    }
}

// 0x00411590
int CServerList::GetMaxServerNum() const
{
    return m_serverNumber;
}

// 0x0043A0A0
int CServerList::GetAgeLimit() const
{
    return m_maxAgeLimit;
}

// 0x00434D97
bool CServerList::GetServerStatus(int serverId) const
{
    // c          serverNumber   0
    // c          zero           1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16

    int status = 0;
    m_rwLock.ReadLock();
    if (serverId > 0 && serverId <= m_serverCount)
    {
        status = m_frameLists[FrameType_GameClient][16 * serverId + 1];
    }
    m_rwLock.ReadUnlock();
    return (status != 0);
}

// 0x00434524
WorldServer CServerList::GetAt(int serverId) const
{
    m_rwLock.ReadLock();
    if (serverId > m_serverNumber || serverId <= 0)
    {
        WorldServer worldServer;
        worldServer.serverId = 0;
        m_rwLock.ReadUnlock();
        return worldServer;
    }

    WorldServer worldServer;
    worldServer = m_servers.at(serverId - 1);
    m_rwLock.ReadUnlock();

    return worldServer;
}

// 0x00434D36
void CServerList::CheckAllServerStatus()
{
    for (int i = 1; i <= m_serverCount; ++i)
    {
        WorldServer worldServer = GetAt(i);
        if (g_worldServServer.GetServerStatus(worldServer.ipAddress))
        {
            SetServerStatus(i, true);
        }
    }
}

// 0x00434659
void CServerList::SetServerUserNum(int /*outerServerIP*/, uint16_t usersOnline, uint16_t usersLimit, int serverId)
{
    // c          serverNumber   0
    // c          zero           1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16   <---- 16-3
    // c          serverStatus   17
    //
    // WithKind:
    // c          serverNumber   0
    // c          zero           1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16   <---- 20-7
    // c          serverStatus   17

    m_rwLock.ReadLock();
    // if ( serverId > 0 )
    if (serverId > 0 && serverId <= m_serverCount)  // FIXED
    {
        memcpy(&m_frameLists[FrameType_GameClient][16 * serverId - 3], &usersOnline, 2u);
        memcpy(&m_frameLists[FrameType_GameClient][16 * serverId - 3 + 2], &usersLimit, 2u);
        memcpy(&m_frameLists[FrameType_WithKind][20 * serverId - 7], &usersOnline, 2u);
        memcpy(&m_frameLists[FrameType_WithKind][20 * serverId - 7 + 2], &usersLimit, 2u);
    }
    m_rwLock.ReadUnlock();
}

// 0x00434CD9
void CServerList::SetServerStatus(int serverId, bool status)
{
    // c          serverNumber   0
    // c          lastWorld      1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16
    // c          serverStatus   17      <---- 16+1
    //
    // WithKind:
    // c          serverNumber   0
    // c          lastWorld      1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16
    // c          serverStatus   17      <---- 20-3

    m_rwLock.WriteLock();
    if (serverId > 0 && serverId <= m_serverCount)
    {
        m_frameLists[FrameType_GameClient][16 * serverId + 1] = status;
        m_frameLists[FrameType_WithKind][20 * serverId - 3] = status;
    }
    return m_rwLock.WriteUnlock();
}

// 0x00433921
void CServerList::ReloadServer()
{
    int minAgeLimit = m_maxAgeLimit;
    m_rwLock.WriteLock();
    m_worldPort = g_Config.worldPort;
    if (g_Config.readLocalServerList)
    {
        FILE* file = fopen("etc/serverlist", "rt");
        if (file)
        {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            char* str = fgets(buffer, sizeof(buffer), file);
            int server_index = 0;

            while ((str != nullptr) || !feof(file))
            {
                if (strlen(buffer) <= 10)
                {
                    g_winlog.AddLog(LOG_ERR, "can't load serverlist file");
                    return;
                }
                char* serverId = strtok(buffer, ",");
                m_tmpServer.serverId = atoi(serverId);

                char* serverName = strtok(0, ",");
                strcpy(m_tmpServer.serverName, serverName);

                char* innerIpStr = strtok(0, ",");
                m_tmpServer.ipAddress = inet_addr(innerIpStr);
                strcpy(m_tmpServer.innerIPStr, innerIpStr);

                char* outerIpStr = strtok(0, ",");
                strcpy(m_tmpServer.outerIPStr, outerIpStr);

                char* ageLimit = strtok(0, ",");
                m_tmpServer.ageLimit = atoi(ageLimit);

                char* serverKind = strtok(0, ",");
                m_tmpServer.kind = atoi(serverKind);
                m_tmpServer.outerIP = inet_addr(m_tmpServer.outerIPStr);
                m_tmpServer.socket = -1;
                m_tmpServer.pvp = 1;

                minAgeLimit = 12;
                m_tmpServer.userNumber = 0;
                m_servers.push_back(m_tmpServer);                                                    // c    serverCount, then block:
                memcpy(&m_compressedFrameBuffers[9 * server_index + 1], &m_tmpServer.serverId, 1u);  // c serverId
                memcpy(&m_compressedFrameBuffers[9 * server_index + 2], &m_tmpServer.outerIP, 4u);   // d serverIP
                memcpy(&m_compressedFrameBuffers[9 * server_index + 6], &m_worldPort, 4u);           // d serverPort

                ++m_serverCount;
                ++server_index;
                g_winlog.AddLog(LOG_INF, "Server %d loaded,innerip:%s,outerip:%s,%d", m_tmpServer.serverId, m_tmpServer.innerIPStr, m_tmpServer.outerIPStr, m_worldPort);

                memset(buffer, 0, 1024u);
                str = fgets(buffer, 1024, file);
            }

            memcpy(m_compressedFrameBuffers, &m_serverCount, 1u);
            m_defaultOrCompressedSize = 9 * m_serverCount + 1;
            fclose(file);
        }
        m_serverNumber = m_servers.size();
        m_frameLists[FrameType_DefaultOrCompressed] = m_compressedFrameBuffers;
        m_frameSizes[FrameType_DefaultOrCompressed] = m_defaultOrCompressedSize;
        MakeServerListFrame1();

        m_rwLock.WriteUnlock();
    }
    else
    {
        CDBConn sql(&g_linDB);
        sql.bindByte(&m_tmpServer.serverId);
        sql.bindStr(m_tmpServer.serverName, sizeof(m_tmpServer.serverName));
        sql.bindStr(m_tmpServer.outerIPStr, sizeof(m_tmpServer.outerIPStr));
        sql.bindStr(m_tmpServer.innerIPStr, sizeof(m_tmpServer.innerIPStr));
        sql.bindByte(&m_tmpServer.ageLimit);
        sql.bindBool(&m_tmpServer.pvp);
        sql.bindInt(&m_tmpServer.kind);
        sql.bindInt(&m_tmpServer.serverPort);

        if (m_servers.empty())
        {
            m_serverCount = 0;
            m_defaultOrCompressedSize = 0;
            if (sql.Execute("Select id, name, ip, inner_ip, ageLimit, pk_flag, kind, port From "
                            "server Order by id"))
            {
                bool notFound = true;
                sql.Fetch(&notFound);
                int serverIdIndex = 0;
                while (notFound == false)
                {
                    if ((serverIdIndex + 1) < m_tmpServer.serverId)
                    {
                        g_winlog.AddLog(LOG_ERR, "loading serverlist fail, serverid is not serialized");
                        Threading::g_bTerminating = true;
                        break;
                    }

                    m_tmpServer.ipAddress = inet_addr(m_tmpServer.innerIPStr);
                    m_tmpServer.outerIP = inet_addr(m_tmpServer.outerIPStr);
                    m_tmpServer.socket = -1;
                    m_tmpServer.userNumber = 0;

                    if (minAgeLimit == 0 || minAgeLimit > m_tmpServer.ageLimit)
                    {
                        minAgeLimit = m_tmpServer.ageLimit;
                    }
                    m_servers.push_back(m_tmpServer);
                    ++m_serverCount;

                    memcpy(&m_compressedFrameBuffers[9 * serverIdIndex + 1], &m_tmpServer, 1u);
                    memcpy(&m_compressedFrameBuffers[9 * serverIdIndex + 2], &m_tmpServer.outerIP, 4u);
                    memcpy(&m_compressedFrameBuffers[9 * serverIdIndex + 6], &m_tmpServer.serverPort, 4u);
                    g_winlog.AddLog(LOG_INF, "Server %d loaded, innerip:%s,outerip:%s,port:%d,agelimit:%d,pk:%d", m_tmpServer.serverId, m_tmpServer.innerIPStr, m_tmpServer.outerIPStr, m_tmpServer.serverPort, m_tmpServer.ageLimit, m_tmpServer.pvp);

                    sql.Fetch(&notFound);
                    ++serverIdIndex;
                }
                memcpy(m_compressedFrameBuffers, &m_serverCount, 1u);
                m_defaultOrCompressedSize = 9 * m_serverCount + 1;
                m_serverNumber = m_servers.size();
                m_frameLists[FrameType_DefaultOrCompressed] = m_compressedFrameBuffers;
                m_frameSizes[FrameType_DefaultOrCompressed] = m_defaultOrCompressedSize;
            }
        }
        else
        {
            int currentServersSize = m_servers.size();
            if (sql.Execute("Select id, name, ip, inner_ip, ageLimit, pk_flag, kind, port From "
                            "server Order by id"))
            {
                bool notFound = true;
                sql.Fetch(&notFound);
                int serverIdIndex = 0;
                while (notFound == false)
                {
                    if ((serverIdIndex + 1) < m_tmpServer.serverId)
                    {
                        g_winlog.AddLog(LOG_ERR, "loading serverlist fail, serverid is not serialized");
                        Threading::g_bTerminating = true;
                        break;
                    }

                    if (m_tmpServer.serverId > currentServersSize)
                    {
                        m_tmpServer.ipAddress = inet_addr(m_tmpServer.innerIPStr);
                        m_tmpServer.outerIP = inet_addr(m_tmpServer.outerIPStr);
                        m_tmpServer.userNumber = 0;
                        m_tmpServer.socket = -1;
                        m_servers.push_back(m_tmpServer);

                        ++m_serverCount;

                        memcpy(&m_compressedFrameBuffers[9 * serverIdIndex] + m_defaultOrCompressedSize, &m_tmpServer, 1u);
                        memcpy(&m_compressedFrameBuffers[9 * serverIdIndex + 1] + m_defaultOrCompressedSize, &m_tmpServer.outerIP, 4u);
                        memcpy(&m_compressedFrameBuffers[9 * serverIdIndex + 5] + m_defaultOrCompressedSize, &m_tmpServer.serverPort, 4u);
                        g_winlog.AddLog(LOG_INF, "Server %d loaded, innerip:%s,outerip:%s, %d", m_tmpServer.serverId, m_tmpServer.innerIPStr, m_tmpServer.outerIPStr, m_worldPort);
                    }

                    ++serverIdIndex;
                    sql.Fetch(&notFound);
                }

                // TODO: this is buggy as hell!
                if (m_tmpServer.serverId < currentServersSize)  // remove missed servers. TODO:
                                                                // absent in other implementations!
                {
                    for (int i = m_tmpServer.serverId + 1; i <= currentServersSize; ++i)
                    {
                        m_servers.erase(m_servers.begin() + (i - 1));
                    }
                }

                m_serverNumber = m_servers.size();
                int defaultOrCompressedSize = 9 * m_serverCount + 1;
                ::InterlockedExchange((volatile LONG*)&m_defaultOrCompressedSize, (LONG)defaultOrCompressedSize);  // TODO x64 problems
                memcpy(m_compressedFrameBuffers, &m_serverCount, 1u);
            }
        }
        if (m_servers.empty())
        {
            g_winlog.AddLog(LOG_ERR, "Server load fail, please check server table or DB connection string");
        }

        m_serverNumber = m_servers.size();
        m_frameLists[FrameType_DefaultOrCompressed] = m_compressedFrameBuffers;
        m_frameSizes[FrameType_DefaultOrCompressed] = m_defaultOrCompressedSize;

        MakeServerListFrame1();
        MakeServerListFrame2();
        MakeServerListFrame3();

        m_maxAgeLimit = minAgeLimit;
        m_rwLock.WriteUnlock();

        CheckAllServerStatus();
    }
}

// 0x004345A7
int CServerList::SetServerSocket(int ipAddress, SOCKET socket)
{
    int serverId = 0;
    m_rwLock.WriteLock();

    for (auto it = std::begin(m_servers); it != std::end(m_servers); ++it)
    {
        if (it->ipAddress == ipAddress)
        {
            if (it->socket == INVALID_SOCKET)  // new world server
            {
                it->socket = socket;
                serverId = it->serverId;
            }
            else if (it->socket == socket)  // server with socket found, means closing. Return
                                            // serverId
            {
                it->socket = INVALID_SOCKET;
                serverId = it->serverId;
            }
            else  // server is registered with that IP
            {
                serverId = -1;
            }
            break;
        }
    }

    m_rwLock.WriteUnlock();
    return serverId;
}

// 0x004346C7
void CServerList::MakeServerListFrame1()
{
    // c          serverNumber   0
    // c          lastWorld      1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16

    char* buffer = new char[0x800u];
    memset(buffer, 0, 0x800u);
    buffer[0] = m_serverNumber;  // 0          serverNumber
    buffer[1] = 0;               // 1          lastWorld
    char* iter = buffer + 2;
    for (int i = 1; i <= m_serverNumber; ++i)  // block
    {
        WorldServer worldServer = m_servers.at(i - 1);
        *iter = worldServer.serverId;  // c          serverId
        iter = iter + 1;

        memcpy(iter, &worldServer.outerIP, 4u);  // d          serverIp
        iter += 4;

        memcpy(iter, &worldServer.serverPort, 4u);  // d          serverPort
        iter += 4;

        *iter++ = worldServer.ageLimit;  // c          ageLimit
        *iter = worldServer.pvp;         // c          pvp

        iter = iter + 6;  // d          userNumber
    }                     // c          serverStatus

    char* old = 0;
    if (m_frameLists[FrameType_GameClient])
    {
        old = m_frameLists[FrameType_GameClient];
    }

    m_frameSizes[FrameType_GameClient] = 16 * m_serverNumber + 2;
    m_frameLists[FrameType_GameClient] = buffer;

    if (old)
    {
        delete[] old;
    }
}

// 0x00434848
void CServerList::MakeServerListFrame2()
{
    char* buffer = new char[0x1000u];
    memset(buffer, 0, 0x1000u);

    char* iter = buffer;
    buffer[0] = m_serverNumber;
    *++iter = 0;

    ++iter;
    for (int i = 1; i <= m_serverNumber; ++i)
    {
        WorldServer worldServer = m_servers.at(i - 1);
        *iter++ = worldServer.serverId;

        if (strlen(worldServer.serverName) >= 20)
        {
            char serverName[20];
            strncpy(serverName, worldServer.serverName, 19);
            serverName[19] = 0;
            swprintf((wchar_t*)iter, L"%S", serverName);
        }
        else
        {
            swprintf((wchar_t*)iter, L"%S", worldServer.serverName);
        }
        iter += 40;

        memcpy(iter, &worldServer.outerIP, 4u);
        iter += 4;

        memcpy(iter, &worldServer.serverPort, 4u);
        iter += 4;

        *iter++ = worldServer.ageLimit;
        *iter++ = worldServer.pvp;

        iter += 5;
    }

    char* old = nullptr;
    if (m_frameLists[FrameType_Extended] != nullptr)
    {
        old = m_frameLists[FrameType_Extended];
    }

    m_frameSizes[FrameType_Extended] = 56 * m_serverNumber + 2;
    m_frameLists[FrameType_Extended] = buffer;

    if (old != nullptr)
    {
        delete[] old;
    }
}

// 0x00434A4F
void CServerList::MakeServerListFrame3()
{
    // c          serverNumber   0
    // c          lastWorld      1
    // block
    // c          serverId       2
    // d          serverIp       3-6
    // d          serverPort     7-10
    // c          ageLimit       11
    // c          pvp            12
    // d          userNumber     13-16   <---- 20-7
    // c          serverStatus   17

    auth_guard;

    char* buffer = new char[0x800u];
    memset(buffer, 0, 0x800u);
    buffer[0] = m_serverNumber;  // c          serverNumber   0
    buffer[1] = 0;               // c          lastWorld      1

    char* iter = buffer + 2;
    for (int i = 1; i <= m_serverNumber; ++i)  // block
    {
        WorldServer worldServer = m_servers.at(i - 1);
        *iter = worldServer.serverId;  // c          serverId       2
        iter = iter + 1;

        memcpy(iter, &worldServer.outerIP, 4u);  // d          serverIp       3-6
        iter += 4;

        memcpy(iter, &worldServer.serverPort, 4u);  // d          serverPort     7-10
        iter += 4;

        *iter++ = worldServer.ageLimit;  // c          ageLimit       11
        *iter = worldServer.pvp;         // c          pvp            12

        iter += 6;  // d          userNumber     13-16   <---- 20-7

        memcpy(iter, &worldServer.kind, 4u);  // c          serverStatus   17
        iter = iter + 4;                      // d          serverKind     18-21
    }

    char* old = nullptr;
    if (m_frameLists[FrameType_WithKind] != nullptr)
    {
        old = m_frameLists[FrameType_WithKind];
    }

    m_frameSizes[FrameType_WithKind] = 20 * m_serverNumber + 2;
    m_frameLists[FrameType_WithKind] = buffer;

    if (old != nullptr)
    {
        delete[] old;
    }

    auth_vunguard;
}
