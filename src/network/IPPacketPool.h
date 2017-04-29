#pragma once

#include "network/IPPacketSlot.h"

class IPPacketPool
{
public:
    ~IPPacketPool();

    static IPPacketSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern IPPacketPool theIPPacketPool;
