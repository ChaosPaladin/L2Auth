#include "network/CAuthOverlapped.h"
#include "network/CAuthOverlappedPool.h"

// 0x00418CE9
CAuthOverlapped* CAuthOverlapped::Alloc()
{
    CAuthOverlappedSlot* slot = &CAuthOverlappedPool::g_slot[::InterlockedIncrement(&CAuthOverlappedPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    CAuthOverlapped* overlapped = slot->m_data.overlapped;
    if (slot->m_data.slot)
    {
        slot->m_data.slot = overlapped->m_pNext;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        overlapped = new CAuthOverlapped();
    }
    memset(overlapped, 0, sizeof(OVERLAPPED));
    overlapped->acceptSocket = INVALID_SOCKET;
    overlapped->m_pNext = NULL;
    return overlapped;
}

// 0x00418D8B
void CAuthOverlapped::Free()
{
    CAuthOverlappedSlot* slot = &CAuthOverlappedPool::g_slot[InterlockedDecrement(&CAuthOverlappedPool::g_nFree) & 0xF];

    slot->m_lock.Enter();
    m_pNext = slot->m_data.slot;
    slot->m_data.overlapped = this;
    slot->m_lock.Leave();
}

// 0x00418DD8
void CAuthOverlapped::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        CAuthOverlappedSlot* slot = &CAuthOverlappedPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            CAuthOverlapped* overlapped = slot->m_data.overlapped;
            if (overlapped == NULL)
            {
                break;
            }

            slot->m_data.slot = overlapped->m_pNext;
            if (overlapped)
            {
                delete overlapped;
            }
        }
        slot->m_lock.Leave();
    }
}
