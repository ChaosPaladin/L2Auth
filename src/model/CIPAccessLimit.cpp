#include "model/CIPAccessLimit.h"
#include "config/Config.h"

CIPAccessLimit g_ipAccessLimit;

// 0x0041E233
CIPAccessLimit::CIPAccessLimit()
{
    for (int i = 0; i < 16; ++i)
    {
        ::InitializeCriticalSection(&m_locks[i]);
    }
}

// 0x0041E296
CIPAccessLimit::~CIPAccessLimit()
{
    for (int i = 0; i < 16; ++i)
    {
        ::DeleteCriticalSection(&m_locks[i]);
    }
}

// 0x0041E346
bool CIPAccessLimit::SetAccessIP(in_addr ipAddress)
{
    if (g_Config.IPAccessLimit <= 0)
    {
        return true;
    }

    bool result = false;
    int index = CIPAccessLimit::GetIPLockValue(ipAddress);
    int newLimit = 1;

    ::EnterCriticalSection(&m_locks[index]);

    auto it = m_ipLimits[index].find(ipAddress.s_addr);
    auto end = std::end(m_ipLimits[index]);
    if (it != end)
    {
        if (it->second < g_Config.IPAccessLimit)
        {
            ++it->second;
            result = true;
        }
    }
    else
    {
        m_ipLimits[index].insert(std::make_pair(ipAddress.s_addr, newLimit));
        result = true;
    }

    ::LeaveCriticalSection(&m_locks[index]);

    return result;
}

// 0x0041E461
bool CIPAccessLimit::DelAccessIP(in_addr ipAddress)
{
    if (g_Config.IPAccessLimit <= 0)
    {
        return true;
    }

    bool result = false;
    int index = CIPAccessLimit::GetIPLockValue(ipAddress);

    ::EnterCriticalSection(&m_locks[index]);

    auto it = m_ipLimits[index].find(ipAddress.s_addr);
    auto end = std::end(m_ipLimits[index]);
    if (it != end)
    {
        if (it->second <= 1)
        {
            m_ipLimits[index].erase(it);
        }
        else
        {
            ++it->second;
        }
        result = true;
    }

    ::LeaveCriticalSection(&m_locks[index]);

    return result;
}

// 0x0041E2F2
int CIPAccessLimit::GetIPLockValue(in_addr ipAddress)
{
    int index = (ipAddress.S_un.S_un_b.s_b1 + ipAddress.S_un.S_un_b.s_b2 + ipAddress.S_un.S_un_b.s_b3 + ipAddress.S_un.S_un_b.s_b4) & 15;
    if (index >= 16)
    {
        index = 15;
    }

    return index;
}
