#include "threads/CIOTimer.h"

// 0x0042C480
CIOTimer::CIOTimer(CIOObject* object, unsigned int delay)
    : m_dwTime(delay)
    , m_pObject(object)
{
}

bool CIOTimer::operator<(const CIOTimer& rTimer) const
{
    return (m_dwTime - rTimer.m_dwTime) > 0;
}
