#include "network/CIOBufferPool.h"
#include "network/CIOBuffer.h"

CIOBufferSlot CIOBufferPool::g_slot[16];
long CIOBufferPool::g_nAlloc = -1;
long CIOBufferPool::g_nFree = 0;

CIOBufferPool theIOBufferPool;

CIOBufferPool::~CIOBufferPool()
{
    CIOBuffer::FreeAll();
}
