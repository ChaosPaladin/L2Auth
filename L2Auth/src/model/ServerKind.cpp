#include "model/ServerKind.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "model/ServerPacketList.h"
#include "ui/CLog.h"

#include "network/CAuthServer.h"
#include "network/WorldSrvServer.h"

ServerKind g_serverKind;

// 0x00436B4F
ServerKind::ServerKind()
    : m_serverKindsCount(0)
    , m_servers()
    , m_serverCount(0)
    , m_rwLock()
{
}

// 0x00436BC3
ServerKind::~ServerKind()
{
    for (int frameType = 0; frameType < 4; ++frameType)
    {
        for (int serverKindIndex = 0; serverKindIndex < m_serverKindsCount; ++serverKindIndex)
        {
            if (m_serverFrames[frameType][serverKindIndex])
            {
                ServerPacketList* frameList = m_serverFrames[frameType][serverKindIndex];
                if (frameList != nullptr)
                {
                    delete frameList;
                }
            }
        }
        if (m_serverFrames[frameType] != nullptr)
        {
            delete[] m_serverFrames[frameType];
        }
    }
}

// 0x00436CD0
void ServerKind::LoadServerList()
{
    bool success = true;
    m_rwLock.WriteLock();
    if (m_servers.empty())
    {
        int serverKindNumber = 0;
        bool notFound = true;
        CDBConn sql(&g_linDB);
        sql.bindInt(&serverKindNumber);
        if (sql.Execute("Select Max(kind) From server With (Nolock)"))
        {
            if (sql.Fetch(&notFound))
            {
                if (notFound)
                {
                    m_rwLock.WriteUnlock();
                    sql.ResetHtmt();
                    g_winlog.AddLog(LOG_ERR, "Load ServerKindList fail!");
                    return;
                }

                sql.ResetHtmt();
                while (serverKindNumber != 0)
                {
                    serverKindNumber >>= 1;
                    ++m_serverKindsCount;
                }
                ++m_serverKindsCount;

                for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
                {
                    m_serverFrames[frameType] = new ServerPacketList*[m_serverKindsCount];
                    if (m_serverFrames[frameType] == nullptr)
                    {
                        g_winlog.AddLog(LOG_ERR, "Load ServerKindList Memory Allocation Fail");
                        success = false;
                        break;
                    }

                    memset(m_serverFrames[frameType], 0, sizeof(ServerPacketList*) * m_serverKindsCount);
                    for (int serverKind = 0; serverKind < m_serverKindsCount; ++serverKind)
                    {
                        m_serverFrames[frameType][serverKind] = new ServerPacketList();
                        if (m_serverFrames[frameType][serverKind] == nullptr)
                        {
                            g_winlog.AddLog(LOG_ERR, "Load ServerKindList Memory Allocation Fail");
                            return;
                        }
                        m_serverFrames[frameType][serverKind]->Init((FrameType)frameType, serverKind);
                    }
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
            m_rwLock.WriteUnlock();
            sql.ResetHtmt();
            g_winlog.AddLog(LOG_ERR, "Load ServerKindList fail!");
            return;
        }

        WorldServer worldServer;
        sql.bindByte(&worldServer.serverId);
        sql.bindStr(worldServer.serverName, sizeof(worldServer.serverName));
        sql.bindStr(worldServer.outerIPStr, sizeof(worldServer.outerIPStr));
        sql.bindStr(worldServer.innerIPStr, sizeof(worldServer.innerIPStr));
        sql.bindByte(&worldServer.ageLimit);
        sql.bindBool(&worldServer.pvp);
        sql.bindInt(&worldServer.kind);
        sql.bindInt(&worldServer.serverPort);

        if (sql.Execute("Select id, name, ip, inner_ip, ageLimit, pk_flag, kind, port From server "
                        "Order by id"))
        {
            int serverCount = 0;
            sql.Fetch(&notFound);
            while (notFound == false)
            {
                if (serverCount + 1 < worldServer.serverId)
                {
                    g_winlog.AddLog(LOG_ERR, "loading serverlist fail, serverid is not serialized");
                    success = false;
                    break;
                }
                ++serverCount;
                worldServer.ipAddress = inet_addr(worldServer.innerIPStr);
                worldServer.outerIP = inet_addr(worldServer.outerIPStr);
                worldServer.userNumber = 0;
                worldServer.socket = -1;
                worldServer.serverName[25] = '\0';
                m_servers.push_back(worldServer);

                ++m_serverCount;
                sql.Fetch(&notFound);
            }
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "Loading CServerList Fail.");
            success = false;
        }
    }

    m_rwLock.WriteUnlock();
    if (m_serverCount != 0)
    {
        if (success)
        {
            MakeServerListFrame();
        }
    }
    else
    {
        success = false;
    }
}

// 0x004374A8
void ServerKind::ReloadServerList()
{
    if (m_servers.empty())
    {
        g_winlog.AddLog(LOG_ERR, "Reload ServerKindList Fail!");
        return;
    }

    int serverMaxKindNumber = 0;
    bool notFound = true;

    CDBConn sql(&g_linDB);
    sql.bindInt(&serverMaxKindNumber);

    if (sql.Execute("Select Max(kind) From server With (Nolock)"))
    {
        if (sql.Fetch(&notFound))
        {
            sql.ResetHtmt();
            if (notFound)
            {
                g_winlog.AddLog(LOG_ERR, "Reload ServerKindList Fail!");
            }
            else
            {
                int serverKindsCount = 0;

                while (serverMaxKindNumber)
                {
                    serverMaxKindNumber >>= 1;
                    ++serverKindsCount;
                }

                ServerPacketList** frameLists[4];
                if (++serverKindsCount > m_serverKindsCount)
                {

                    for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
                    {
                        frameLists[frameType] = new ServerPacketList*[serverKindsCount];
                        if (frameLists[frameType] == nullptr)
                        {
                            g_winlog.AddLog(LOG_ERR, "Reload ServerKindList Fail!, Memory Allocation Fail");
                            return;
                        }

                        for (int serverKindIndex = 0; serverKindIndex < m_serverKindsCount; ++serverKindIndex)
                        {
                            if (m_serverFrames[frameType][serverKindIndex] != nullptr)
                            {
                                frameLists[frameType][serverKindIndex] = m_serverFrames[frameType][serverKindIndex];
                            }
                        }

                        for (int serverKindIndex = m_serverKindsCount; serverKindIndex < serverKindsCount; ++serverKindIndex)
                        {
                            frameLists[frameType][serverKindIndex] = new ServerPacketList();
                            if (frameLists[frameType][serverKindIndex])
                            {
                                frameLists[frameType][serverKindIndex]->Init((FrameType)frameType, serverKindIndex);
                            }
                        }
                    }
                }

                WorldServer worldServer;
                sql.bindByte(&worldServer.serverId);
                sql.bindStr(worldServer.serverName, sizeof(worldServer.serverName));
                sql.bindStr(worldServer.outerIPStr, sizeof(worldServer.outerIPStr));
                sql.bindStr(worldServer.innerIPStr, sizeof(worldServer.innerIPStr));
                sql.bindByte(&worldServer.ageLimit);
                sql.bindBool(&worldServer.pvp);
                sql.bindInt(&worldServer.kind);
                sql.bindInt(&worldServer.serverPort);  // FIX: was missed

                if (sql.Execute("Select id, name, ip, inner_ip, ageLimit, pk_flag,kind, port From "
                                "server Order by id"))
                {
                    sql.Fetch(&notFound);
                    int serverCount = 0;

                    while (notFound == false)
                    {
                        int currentServerId = 0;
                        if (currentServerId + 1 < worldServer.serverId)
                        {
                            g_winlog.AddLog(LOG_ERR, "Reloading serverlist fail, serverid is not serialized");
                            return;
                        }
                        if (m_serverCount < ++currentServerId)
                        {
                            worldServer.ipAddress = inet_addr(worldServer.innerIPStr);
                            worldServer.outerIP = inet_addr(worldServer.outerIPStr);
                            worldServer.userNumber = 0;
                            worldServer.socket = -1;
                            ++serverCount;

                            m_rwLock.WriteLock();
                            m_servers.push_back(worldServer);
                            m_rwLock.WriteUnlock();
                        }

                        sql.Fetch(&notFound);
                    }

                    ServerPacketList** serverFrames[4];
                    if (m_serverKindsCount < serverKindsCount)
                    {
                        for (int frameType = 0; frameType < 4; ++frameType)
                        {
                            serverFrames[frameType] = m_serverFrames[frameType];
                            m_serverFrames[frameType] = frameLists[frameType];
                        }

                        for (int frameType = 0; frameType < 4; ++frameType)
                        {
                            if (serverFrames[frameType] != nullptr)
                            {
                                delete[] serverFrames[frameType];
                            }
                        }

                        m_serverKindsCount = serverKindsCount;
                    }

                    MakeServerListFrame();
                    if (serverCount > 0)
                    {
                        m_serverCount += serverCount;
                    }
                }
                else
                {
                    g_winlog.AddLog(LOG_ERR, "Reload CServerList Fail.");
                }
            }
        }
        else
        {
            sql.ResetHtmt();
            g_winlog.AddLog(LOG_ERR, "Reload ServerKindList Fail!");
        }
    }
    else
    {
        sql.ResetHtmt();
        g_winlog.AddLog(LOG_ERR, "Reload ServerKindList Fail!");
    }
}

// 0x00437250
int ServerKind::GetServerCount() const
{
    return m_serverCount;
}

// 0x00437261
WorldServer ServerKind::GetAt(int serverId) const
{
    m_rwLock.ReadLock();
    if (serverId <= 0 || serverId > m_serverCount)
    {
        m_rwLock.ReadUnlock();
        return WorldServer();
    }

    WorldServer worldServer = m_servers[serverId - 1];
    m_rwLock.ReadUnlock();

    return worldServer;
}

// 0x004372E1
int ServerKind::SetServerSocket(int IPaddress, SOCKET socket)
{
    int serverId = -1;
    m_rwLock.WriteLock();
    for (WorldServers::iterator it = m_servers.begin(); it != m_servers.end(); ++it)
    {
        if (it->ipAddress == IPaddress)
        {
            if (it->socket == INVALID_SOCKET)  // new world server
            {
                serverId = it->serverId;
                it->socket = socket;
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

// 0x00437F57
int ServerKind::GetServerMaxKind() const
{
    return m_serverKindsCount;
}

// 0x00437F68
int ServerKind::GetServerPacketList(FrameType frameFormat, int serverKindIndex, char** buffer) const
{
    int size = 0;

    if (frameFormat >= FrameType_DefaultOrCompressed && frameFormat <= FrameType_Compressed)
    {
        if (serverKindIndex >= 0 && serverKindIndex < m_serverKindsCount)
        {
            size = m_serverFrames[frameFormat][serverKindIndex]->GetServerPacketList(buffer);
        }
        else
        {
            size = 0;
        }
    }
    else
    {
        size = 0;
    }

    return size;
}

// 0x0043808B
bool ServerKind::GetServerStatus(int serverId) const
{
    bool available = false;
    if (serverId > 0 && serverId <= m_serverCount)
    {
        available = m_serverFrames[FrameType_GameClient][0]->GetServerStautsKind1(serverId);
    }
    return available;
}

// 0x0043738C
void ServerKind::SetServerStatus(int serverId, bool status)
{
    if (serverId > 0 && serverId <= m_serverCount)
    {
        for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
        {
            for (int serverKindIndex = 0; serverKindIndex < m_serverKindsCount; ++serverKindIndex)
            {
                if (m_serverFrames[frameType][serverKindIndex] != nullptr)
                {
                    m_serverFrames[frameType][serverKindIndex]->SetServerStatus(serverId, status);
                }
            }
        }
    }
}

// 0x0043741A
void ServerKind::SetServerUserNum(int serverId, uint16_t usersOnline, uint16_t usersLimit)
{
    if (serverId > 0 && serverId <= m_serverCount)
    {
        for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
        {
            for (int serverKindIndex = 0; serverKindIndex < m_serverKindsCount; ++serverKindIndex)
            {
                if (m_serverFrames[frameType][serverKindIndex] != nullptr)
                {
                    m_serverFrames[frameType][serverKindIndex]->SetServerUserNum(serverId, usersOnline, usersLimit);
                }
            }
        }
    }
}

// 0x00437BB6
void ServerKind::MakeServerListFrame()
{
    bool success = true;

    ServerPacketList tempFrameList;
    int serverKind = 0;
    for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
    {
        tempFrameList.Init((FrameType)frameType, serverKind);
        for (WorldServers::const_iterator it = m_servers.begin();; ++it)
        {
            WorldServers::const_iterator end = m_servers.end();
            if (it == end)
            {
                break;
            }

            WorldServer worldServer = *it;
            tempFrameList.MakeServerListFrame(worldServer);
        }

        tempFrameList.SetServerCount();

        char* buffer = nullptr;
        int buffSize = tempFrameList.GetServerPacketList(&buffer);
        if (buffSize > 0 && buffer != nullptr)
        {
            if (m_serverFrames[frameType][serverKind] != nullptr)
            {
                success = m_serverFrames[frameType][serverKind]->ExchangeServePacketList((FrameType)frameType, serverKind, buffSize, buffer);
            }
        }
    }

    for (int frameType = 0; frameType < 4; ++frameType)
    {
        int serverKind_Flag = 1;
        bool frameAdded = false;

        for (int serverKindIndex = 1; serverKindIndex < m_serverKindsCount; ++serverKindIndex)
        {
            tempFrameList.Init((FrameType)frameType, serverKindIndex);
            for (WorldServers::const_iterator it = m_servers.begin();; ++it)
            {
                WorldServers::const_iterator end = m_servers.end();
                if (it == end)
                {
                    break;
                }

                if (serverKind_Flag & it->kind)
                {
                    WorldServer worldServer = *it;
                    tempFrameList.MakeServerListFrame(worldServer);
                    frameAdded = true;
                }
            }

            if (frameAdded)
            {
                tempFrameList.SetServerCount();
                char* buffer = 0;
                int buffSize = tempFrameList.GetServerPacketList(&buffer);
                if (buffSize > 0 && buffer != nullptr && m_serverFrames[frameType][serverKindIndex])
                {
                    success = m_serverFrames[frameType][serverKindIndex]->ExchangeServePacketList((FrameType)frameType, serverKindIndex, buffSize, buffer);
                }
            }

            frameAdded = false;
            serverKind_Flag *= 2;
        }
    }

    CheckAllServerStatus();
    if (!success)
    {
        g_winlog.AddLog(LOG_ERR, "ReLoadServerListFrame Fail!!");
    }
}

// 0x00437FCA
void ServerKind::CheckAllServerStatus()
{
    for (int servIndex = 1; servIndex <= m_serverCount; ++servIndex)
    {
        const WorldServer& worldServer = GetAt(servIndex);
        if (g_worldServServer.GetServerStatus(worldServer.ipAddress))
        {
            for (int frameType = FrameType_DefaultOrCompressed; frameType < FrameType_Compressed; ++frameType)
            {
                for (int serverKind = 0; serverKind < m_serverKindsCount; ++serverKind)
                {
                    if (m_serverFrames[frameType][serverKind] != nullptr)
                    {
                        m_serverFrames[frameType][serverKind]->SetServerStatus(servIndex, true);
                    }
                }
            }
        }
    }
}
