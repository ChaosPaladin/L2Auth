#include "network/CLogPacketSlot.h"

CLogPacketSlot::CLogPacketSlot()
    : m_lock(LockType_WaitLock, 0)
{
    m_data.packet = NULL;
}

CLogPacketSlot::~CLogPacketSlot()
{
}
