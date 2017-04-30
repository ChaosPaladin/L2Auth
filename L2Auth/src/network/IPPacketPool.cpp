#include "network/IPPacketPool.h"
#include "network/IPPacket.h"

IPPacketSlot IPPacketPool::g_slot[16];
long IPPacketPool::g_nAlloc = -1;
long IPPacketPool::g_nFree = 0;

IPPacketPool theIPPacketPool;

IPPacketPool::~IPPacketPool()
{
    IPPacket::FreeAll();
}
