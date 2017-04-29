#include "network/CIOBuffer.h"
#include "network/CIOBufferPool.h"

#include <windows.h>

// 0x00417D00
CIOBuffer::CIOBuffer()
{
}

// 0x00417D17
CIOBuffer::~CIOBuffer()
{
}

// 0x004116D0
void CIOBuffer::AddRef()
{
    ::InterlockedIncrement(&m_nRefCount);
}

// 0x0041E170
void CIOBuffer::Release()
{
    if (::InterlockedDecrement(&m_nRefCount) == 0)
    {
        Free();
    }
}

// 0x00417DD3
CIOBuffer* CIOBuffer::Alloc()
{
    CIOBufferSlot* slot = &CIOBufferPool::g_slot[::InterlockedIncrement(&CIOBufferPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    CIOBuffer* newBuffer = slot->m_pBuffer;
    if (slot->m_pBuffer)
    {
        slot->m_pBuffer = newBuffer->m_pNext;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        newBuffer = new CIOBuffer();
    }

    newBuffer->m_dwSize = 0;
    newBuffer->m_nRefCount = 1;
    newBuffer->m_pNext = nullptr;

    return newBuffer;
}

// 0x00417EC2
void CIOBuffer::Free()
{
    CIOBufferSlot* slot = &CIOBufferPool::g_slot[InterlockedDecrement(&CIOBufferPool::g_nFree) & 0xF];

    slot->m_lock.Enter();
    m_pNext = slot->m_pBuffer;
    slot->m_pBuffer = this;
    slot->m_lock.Leave();
}

// 0x00417F14
void CIOBuffer::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        CIOBufferSlot* slot = &CIOBufferPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            CIOBuffer* pBuffer = slot->m_pBuffer;
            if (slot->m_pBuffer == nullptr)
            {
                break;
            }

            slot->m_pBuffer = pBuffer->m_pNext;
            if (pBuffer)
            {
                delete pBuffer;
            }
        }
        slot->m_lock.Leave();
    }
}
