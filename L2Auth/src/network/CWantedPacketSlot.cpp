#include "network/CWantedPacketSlot.h"

CWantedPacketSlot::CWantedPacketSlot()
    : m_lock(LockType_WaitLock, 0)
{
    m_data.packet = nullptr;
}

CWantedPacketSlot::~CWantedPacketSlot()
{
}
