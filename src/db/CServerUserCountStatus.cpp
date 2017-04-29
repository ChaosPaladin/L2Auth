#include "db/CServerUserCountStatus.h"
#include "config/Config.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "model/CServerList.h"
#include "ui/CReporter.h"

#include <cstring>

CServerUserCountStatus g_userCount;

// 0x00434E0F
CServerUserCountStatus::CServerUserCountStatus()
{
    ::InitializeCriticalSection(&m_criticalSection);
    m_tickCount = ::GetTickCount();
    ::EnterCriticalSection(&m_criticalSection);

    for (int i = 0; i < 100; ++i)
    {
        m_users[i].usersLimit = 0;
        m_users[i].users = 0;
        m_users[i].serverId = i;
    }

    ::LeaveCriticalSection(&m_criticalSection);
    ::CreateTimerQueueTimer(&m_timerHandle, 0, &CServerUserCountStatus::TimerRoutine, this, 300000u, 300000u, 0);
}

// 0x00434ECF
CServerUserCountStatus::~CServerUserCountStatus()
{
    ::DeleteCriticalSection(&m_criticalSection);
    ::DeleteTimerQueueTimer(0, m_timerHandle, 0);
}

// 0x00434EFD
void CServerUserCountStatus::UpdateUserNum(int serverId, uint16_t users, uint16_t socketLimit)
{
    if (serverId <= 100)
    {
        ::EnterCriticalSection(&m_criticalSection);
        m_users[serverId].usersLimit = socketLimit;
        m_users[serverId].users = users;
        ::LeaveCriticalSection(&m_criticalSection);
    }
}

// 0x00434DF7
void NTAPI CServerUserCountStatus::TimerRoutine(void* table, BOOLEAN)
{
    ((CServerUserCountStatus*)table)->UpdateAllServerNumToDB();
}

// 0x00434F73
void CServerUserCountStatus::SetCounter()
{
    ++m_tickCount;

    ::EnterCriticalSection(&m_criticalSection);

    m_users[0].usersLimit = g_Config.socketLimit;
    m_users[0].users = g_reporter.users;

    ::LeaveCriticalSection(&m_criticalSection);
}

// 0x00434FCA
void CServerUserCountStatus::UpdateAllServerNumToDB()
{
    CDBConn sql(&g_linDB);

    SetCounter();
    for (int i = 0; i <= g_serverList.GetMaxServerNum(); ++i)
    {
        ::EnterCriticalSection(&m_criticalSection);

        char sqlQuery[520];
        sprintf(
            sqlQuery,
            "INSERT user_count ( server_id, world_user, limit_user, auth_user, wait_user) VALUES ( "
            "%d, %d, %d, %d, %d )",
            i,
            m_users[i].users,
            m_users[i].usersLimit,
            m_users[0].users,
            g_reporter.sockets);

        ::LeaveCriticalSection(&m_criticalSection);

        sql.Execute(sqlQuery);
        sql.ResetHtmt();
    }
}
