#pragma once

#include <windows.h>

class CRWLock
{
public:
    CRWLock();   // 0x0042D5C4
    ~CRWLock();  // 0x0042D5FA

    void ReadLock();     // 0x0042D61C
    void ReadUnlock();   // 0x0042D67A
    void WriteLock();    // 0x0042D6C6
    void WriteUnlock();  // 0x0042D6FF

private:
    CRITICAL_SECTION m_critSection;
    HANDLE m_semaphore;
    int m_readersCount;
};
