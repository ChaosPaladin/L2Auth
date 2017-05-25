#pragma once

#include "utils/cstdint_support.h"
#include <windows.h>

class CServerUserCountStatus
{
public:
    CServerUserCountStatus();   // 0x00434E0F
    ~CServerUserCountStatus();  // 0x00434ECF

    void UpdateUserNum(int serverId, uint16_t users, uint16_t socketLimit);  // 0x00434EFD

private:
    static void NTAPI TimerRoutine(void* table, BOOLEAN);  // 0x00434DF7
    void SetCounter();                                     // 0x00434F73
    void UpdateAllServerNumToDB();                         // 0x00434FCA

private:
    struct UsersPerServer
    {
        int serverId;
        int usersLimit;
        int users;
    };

    UsersPerServer m_users[100];
    CRITICAL_SECTION m_criticalSection;
    int m_tickCount;
    HANDLE m_timerHandle;
};

extern CServerUserCountStatus g_userCount;
