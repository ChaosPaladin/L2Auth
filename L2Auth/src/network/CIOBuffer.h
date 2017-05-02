#pragma once

#include "utils/cstdint_support.h"

#define BUFFER_SIZE 8192  // 0x2000

class CIOBuffer
{
public:
    CIOBuffer();           // 0x00417D00
    virtual ~CIOBuffer();  // 0x00417D17

    static CIOBuffer* Alloc();  // 0x00417DD3
    static void FreeAll();      // 0x00417F14

    void AddRef();   // 0x004116D0
    void Release();  // 0x0041E170
    void Free();     // 0x00417EC2

public:
    uint8_t m_Buffer[BUFFER_SIZE];
    int m_dwSize;
    CIOBuffer* m_pNext;
    volatile long m_nRefCount;
};
