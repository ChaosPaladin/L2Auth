#include "network/IPPacketSlot.h"

IPPacketSlot::IPPacketSlot()
    : m_lock(LockType_WaitLock, 0)
{
    m_data.packet = NULL;
}

IPPacketSlot::~IPPacketSlot()
{
}
