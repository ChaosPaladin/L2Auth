#include "threads/CRWLock.h"
#include "ui/CLog.h"

// 0x0042D5C4
CRWLock::CRWLock()
    : m_critSection()
    , m_semaphore(0)
    , m_readersCount(0)
{
    const int INITIAL_COUNT = 1;
    const int MAX_COUNT = 1;
    m_semaphore = ::CreateSemaphoreA(NULL, INITIAL_COUNT, MAX_COUNT, NULL);
    ::InitializeCriticalSection(&m_critSection);
}

// 0x0042D5FA
CRWLock::~CRWLock()
{
    ::CloseHandle(m_semaphore);
    ::DeleteCriticalSection(&m_critSection);
}

// 0x0042D61C
void CRWLock::ReadLock()
{
    ::EnterCriticalSection(&m_critSection);
    if (++m_readersCount == 1)  // was 0, so first read lock
    {
        //  --semaphore
        if (::WaitForSingleObject(m_semaphore, INFINITE) != WAIT_OBJECT_0)  // wait for write lock released
        {
            g_winlog.AddLog(LOG_ERR, "ReadLock failed on dataLock");
        }
    }
    ::LeaveCriticalSection(&m_critSection);
}

// 0x0042D67A
void CRWLock::ReadUnlock()
{
    long previousCount;
    ::EnterCriticalSection(&m_critSection);
    if (!--m_readersCount)
    {
        ::ReleaseSemaphore(m_semaphore, 1, &previousCount);  // ++semaphore
    }
    ::LeaveCriticalSection(&m_critSection);
}

// 0x0042D6C6
void CRWLock::WriteLock()
{
    ::WaitForSingleObject(m_semaphore, INFINITE);  //  --semaphore
}

// 0x0042D6FF
void CRWLock::WriteUnlock()
{
    long previousCount;
    ::ReleaseSemaphore(m_semaphore, 1, &previousCount);  // ++semaphore
    if (previousCount != 0)
    {
        g_winlog.AddLog(LOG_ERR, "WriteUnlock semaphore was not locked");
    }
}
