#pragma once

#include <windows.h>

enum LockType
{
    LockType_CritSection = 0x0,
    LockType_Spin = 0x1,
    LockType_WaitLock = 0x2,
};

class CLock
{
public:
    CLock(LockType lockType, DWORD dwSpinCount);  // 0x0042D420
    ~CLock();                                     // 0x0042D46B

    void Enter();  // 0x0042D4B5
    void Leave();  // 0x0042D4F0
    void Wait();   // 0x0042D51F

private:
    CRITICAL_SECTION m_criticalSection;
    volatile LONG m_lock;
    LockType m_syncType;
};
