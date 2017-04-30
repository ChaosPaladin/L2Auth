#include "network/CAuthPacketSlot.h"

// 0x00431D90
CAuthPacketSlot::CAuthPacketSlot()
    : m_lock(LockType_WaitLock, 0)
{
    m_data.packet = nullptr;
}

CAuthPacketSlot::~CAuthPacketSlot()
{
}
