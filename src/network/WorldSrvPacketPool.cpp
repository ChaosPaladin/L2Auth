#include "network/WorldSrvPacketPool.h"
#include "network/WorldSrvPacket.h"

WorldSrvPacketSlot WorldSrvPacketPool::g_slot[16];
long WorldSrvPacketPool::g_nAlloc = -1;
long WorldSrvPacketPool::g_nFree = 0;

WorldSrvPacketPool theWorldSrvPool;

// 0x0041E0F0
WorldSrvPacketPool::~WorldSrvPacketPool()
{
    WorldSrvPacket::FreeAll();
}
