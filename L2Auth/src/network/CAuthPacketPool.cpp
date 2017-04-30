#include "network/CAuthPacketPool.h"
#include "network/CAuthPacket.h"

CAuthPacketSlot CAuthPacketPool::g_slot[16];
long CAuthPacketPool::g_nAlloc = -1;
long CAuthPacketPool::g_nFree = 0;

CAuthPacketPool theAuthPacketPool;

// 0x00431DE0
CAuthPacketPool::~CAuthPacketPool()
{
    CAuthPacket::FreeAll();
}
