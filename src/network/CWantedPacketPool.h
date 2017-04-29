#pragma once

#include "network/CWantedPacketSlot.h"

class CWantedPacketPool
{
public:
    ~CWantedPacketPool();

    static CWantedPacketSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern CWantedPacketPool theCWantedPacketPool;
