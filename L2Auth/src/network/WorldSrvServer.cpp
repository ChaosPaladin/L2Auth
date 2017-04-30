#include <WinSock2.h>

#include "network/CIOBuffer.h"
#include "network/PacketUtils.h"
#include "network/WorldSrvServer.h"
#include "network/WorldSrvSocket.h"

#include "model/AccountDB.h"
#include "model/ServersProvider.h"
#include "ui/CLog.h"

#include "config/Config.h"

#include <varargs.h>

WorldSrvServer g_worldServServer;

// 0x0041865F
WorldSrvServer::WorldSrvServer()
    : CIOServer()
    , m_sockets()
    , m_lock()
    , m_factoryMethod(nullptr)
{
    ::InitializeCriticalSectionAndSpinCount(&m_lock, 4000u);
}

// 0x004186D9
WorldSrvServer::~WorldSrvServer()
{
    ::DeleteCriticalSection(&m_lock);
}

// 0x0041873A
CIOSocket* WorldSrvServer::CreateSocket(SOCKET socket, sockaddr_in* clientAddress)
{
    g_winlog.AddLog(LOG_INF, "*new world server connection from %d.%d.%d.%d", clientAddress->sin_addr.S_un.S_un_b.s_b1, clientAddress->sin_addr.S_un.S_un_b.s_b2, clientAddress->sin_addr.S_un.S_un_b.s_b3, clientAddress->sin_addr.S_un.S_un_b.s_b4);

    int serverId = ServersProvider::SetServerSocket(clientAddress->sin_addr.s_addr, socket);
    if (serverId == 0)
    {
        g_winlog.AddLog(LOG_ERR, "non-registered world server %d.%d.%d.%d", clientAddress->sin_addr.S_un.S_un_b.s_b1, clientAddress->sin_addr.S_un.S_un_b.s_b2, clientAddress->sin_addr.S_un.S_un_b.s_b3, clientAddress->sin_addr.S_un.S_un_b.s_b4);

        return nullptr;
    }

    if (serverId == -1)
    {
        g_winlog.AddLog(LOG_ERR, "ip: %d.%d.%d.%d world server socket already used", clientAddress->sin_addr.S_un.S_un_b.s_b1, clientAddress->sin_addr.S_un.S_un_b.s_b2, clientAddress->sin_addr.S_un.S_un_b.s_b3, clientAddress->sin_addr.S_un.S_un_b.s_b4);
        return nullptr;
    }

    WorldSrvSocket* worldSocket = (*m_factoryMethod)(socket);
    worldSocket->setServerId(serverId);
    worldSocket->SetAddress(clientAddress->sin_addr);

    ::EnterCriticalSection(&m_lock);
    m_sockets.insert(std::make_pair(clientAddress->sin_addr.s_addr, worldSocket));
    ::LeaveCriticalSection(&m_lock);

    return worldSocket;
}

// 0x004188A6
bool WorldSrvServer::Run(int port, WorldSrvSocketFactory* factoryMethod)
{
    m_factoryMethod = factoryMethod;
    if (CIOServer::Create(port))
    {
        g_winlog.AddLog(LOG_INF, "server ready on port %d", port);
    }

    // m_listenSocket = m_socket;

    return true;
}

// 0x004188F9
WorldSrvSocket* WorldSrvServer::FindSocket(int ipAddress) const
{
    WorldSrvSocket* socket = nullptr;

    ::EnterCriticalSection(&m_lock);

    auto it = m_sockets.find(ipAddress);
    if (it != std::end(m_sockets))
    {
        socket = it->second;
        socket->AddRef();
    }

    ::LeaveCriticalSection(&m_lock);

    return socket;
}

// 0x00418976
bool WorldSrvServer::GetServerStatus(int ipAddress) const
{
    bool exists = false;
    ::EnterCriticalSection(&m_lock);

    auto it = m_sockets.find(ipAddress);
    if (it != std::end(m_sockets))
    {
        exists = true;
    }

    ::LeaveCriticalSection(&m_lock);

    return exists;
}

// 0x004189DE
void WorldSrvServer::RemoveSocket(int ipAddress)
{
    ::EnterCriticalSection(&m_lock);

    auto it = m_sockets.find(ipAddress);
    if (it != std::end(m_sockets))
    {
        m_sockets.erase(it);
    }

    if (m_sockets.empty())
    {
        g_accountDb.RemoveAll();
    }

    ::LeaveCriticalSection(&m_lock);
}

// 0x00439142
bool WorldSrvServer::SendSocket(int ipAddress, const char* format, ...)
{
    va_list va;
    va_start(va, format);

    WorldSrvSocket* worldSocket = g_worldServServer.FindSocket(ipAddress);
    if (worldSocket == nullptr)
    {
        return false;
    }

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
        g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", worldSocket->getSocket(), format);
    }

    buff->m_dwSize = packetSize;

    worldSocket->Write(buff);
    worldSocket->ReleaseRef();

    return true;
}
