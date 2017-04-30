#pragma once

#include "network/CAuthOverlappedSlot.h"

class CAuthOverlappedPool
{
public:
    ~CAuthOverlappedPool();

    static CAuthOverlappedSlot g_slot[16];
    static long g_nAlloc;
    static long g_nFree;
};

extern CAuthOverlappedPool theAuthPool;
