#pragma once

#include "network/CIOServer.h"

#include <map>

class WorldSrvSocket;

class WorldSrvServer : public CIOServer
{
public:
    using WorldSrvSocketFactory = WorldSrvSocket*(SOCKET socket);

public:
    WorldSrvServer();   // 0x0041865F
    ~WorldSrvServer();  // 0x004186D9

    static bool SendSocket(int ipAddress, const char* format, ...);  // 0x00439142

    bool Run(int port, WorldSrvSocketFactory* factoryMethod);  // 0x004188A6

    bool GetServerStatus(int ipAddress) const;  // 0x00418976
    void RemoveSocket(int ipAddress);           // 0x004189DE

protected:
    CIOSocket* CreateSocket(SOCKET socket, sockaddr_in* clientAddress) override;  // 0x0041873A

private:
    WorldSrvSocket* FindSocket(int ipAddress) const;  // 0x004188F9

private:
    typedef std::map<int, WorldSrvSocket*> Sockets;
    Sockets m_sockets;
    mutable CRITICAL_SECTION m_lock;
    WorldSrvSocketFactory* m_factoryMethod;
};

extern WorldSrvServer g_worldServServer;
