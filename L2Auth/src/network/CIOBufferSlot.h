#pragma once

#include "threads/CLock.h"

class CIOBuffer;

class CIOBufferSlot
{
public:
    CIOBufferSlot();
    ~CIOBufferSlot();

    CIOBuffer* m_pBuffer;
    CLock m_lock;
};
