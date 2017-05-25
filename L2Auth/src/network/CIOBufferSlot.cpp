#include "network/CIOBufferSlot.h"

CIOBufferSlot::CIOBufferSlot()
    : m_pBuffer(NULL)
    , m_lock(LockType_WaitLock, 0)
{
}

CIOBufferSlot::~CIOBufferSlot()
{
}
