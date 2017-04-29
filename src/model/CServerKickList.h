#pragma once

#include "threads/CLock.h"

#include <map>
#include <string>

class WorldSrvSocket;

class CServerKickList
{
public:
    CServerKickList();   // 0x004115B0
    ~CServerKickList();  // 0x00411680

    void AddKickUser(int uid, char* accName);  // 0x004112BF
    void PopKickUser(WorldSrvSocket* socket);  // 0x0041136F

private:
    std::map<int, std::string> m_users;
    CLock m_lock;
};
