#pragma once

#include <map>
#include <windows.h>

class CIPAccessLimit
{
public:
    CIPAccessLimit();           // 0x0041E233
    virtual ~CIPAccessLimit();  // 0x0041E296

    bool SetAccessIP(in_addr ipAddress);  // 0x0041E346
    bool DelAccessIP(in_addr ipAddress);  // 0x0041E461

private:
    static int GetIPLockValue(in_addr ipAddress);  // 0x0041E2F2

private:
    CRITICAL_SECTION m_locks[16];
    std::map<unsigned long, int> m_ipLimits[16];
};

extern CIPAccessLimit g_ipAccessLimit;
