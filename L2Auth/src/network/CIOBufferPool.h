#pragma once

#include "network/CIOBufferSlot.h"

class CIOBufferPool
{
public:
    ~CIOBufferPool();

    static CIOBufferSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern CIOBufferPool theIOBufferPool;
