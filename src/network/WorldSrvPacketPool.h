#pragma once

#include "network/WorldSrvPacketSlot.h"

class WorldSrvPacketPool
{
public:
    ~WorldSrvPacketPool();  // 0x0041E0F0

    static WorldSrvPacketSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern WorldSrvPacketPool theWorldSrvPool;
