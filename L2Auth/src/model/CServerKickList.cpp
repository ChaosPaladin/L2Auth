#include "model/CServerKickList.h"
#include "network/WorldSrvSocket.h"

// 0x004115B0
CServerKickList::CServerKickList()
    : m_users()
    , m_lock(LockType_CritSection, 0)
{
}

// 0x00411680
CServerKickList::~CServerKickList()
{
}

// 0x004112BF
void CServerKickList::AddKickUser(int uid, char* accName)
{
    m_lock.Enter();
    m_users.insert(std::make_pair(uid, std::string(accName)));
    m_lock.Leave();
}

// 0x0041136F
void CServerKickList::PopKickUser(WorldSrvSocket* socket)
{
    m_lock.Enter();
    for (std::map<int, std::string>::iterator it = m_users.begin(); it != m_users.end(); ++it)
    {
        int uid = it->first;
        char accName[15];
        memset(accName, 0, sizeof(accName));
        strncpy(accName, it->second.c_str(), 14u);
        if (uid > 0)
        {
            socket->Send("cdcs", 1, uid, 14, accName);
        }
    }

    m_users.clear();
    m_lock.Leave();
}
