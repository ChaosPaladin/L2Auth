#include "network/CIOBufferSlot.h"

CIOBufferSlot::CIOBufferSlot()
    : m_pBuffer(nullptr)
    , m_lock(LockType_WaitLock, 0)
{
}

CIOBufferSlot::~CIOBufferSlot()
{
}
