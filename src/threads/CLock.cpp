#include "threads/CLock.h"

// 0x0042D420
CLock::CLock(LockType lockType, DWORD dwSpinCount)
    : m_criticalSection()
    , m_lock(0)
    , m_syncType(lockType)
{
    if (m_syncType == LockType_WaitLock)
    {
        m_lock = 0;
    }
    else if (lockType == LockType_Spin)
    {
        ::InitializeCriticalSectionAndSpinCount(&m_criticalSection, dwSpinCount);
    }
    else
    {
        ::InitializeCriticalSection(&m_criticalSection);
    }
}

// 0x0042D46B
CLock::~CLock()
{
    if (m_syncType != LockType_WaitLock)
        DeleteCriticalSection(&m_criticalSection);
}

// 0x0042D4B5
void CLock::Enter()
{
    if (m_syncType == LockType_WaitLock)
    {
        if (::InterlockedExchange(&m_lock, 1) == 1)  // already acquired
        {
            Wait();
        }
    }
    else
    {
        ::EnterCriticalSection(&m_criticalSection);
    }
}

// 0x0042D4F0
void CLock::Leave()
{
    if (m_syncType == LockType_WaitLock)
        ::InterlockedExchange(&m_lock, 0);
    else
        ::LeaveCriticalSection(&m_criticalSection);
}

// 0x0042D51F
void CLock::Wait()
{
    if (m_syncType == LockType_WaitLock)
    {
        int count = 4000;
        while (--count >= 0)
        {
            if (!::InterlockedExchange(&m_lock, 1))
                return;
            _mm_pause();
        }
        count = 4000;
        while (--count >= 0)
        {
            ::SwitchToThread();
            if (!::InterlockedExchange(&m_lock, 1))
                return;
        }
        do
            ::Sleep(1000);
        while (::InterlockedExchange(&m_lock, 1));
    }
}
