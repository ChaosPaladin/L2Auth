#include "network/WorldSrvPacketSlot.h"

// 0x0041E0A0
WorldSrvPacketSlot::WorldSrvPacketSlot()
    : m_lock(LockType_WaitLock, 0)
{
    m_data.packet = nullptr;
}

// 0x0041E0D0
WorldSrvPacketSlot::~WorldSrvPacketSlot()
{
}
