#pragma once

#include "network/CAuthPacketSlot.h"

class CAuthPacketPool
{
public:
    ~CAuthPacketPool();  // 0x00431DE0

    static CAuthPacketSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern CAuthPacketPool theAuthPacketPool;
