#include "network/CAuthOverlappedSlot.h"

CAuthOverlappedSlot::CAuthOverlappedSlot()
    : m_data()
    , m_lock(LockType_WaitLock, 0)
{
    m_data.overlapped = nullptr;
}

CAuthOverlappedSlot::~CAuthOverlappedSlot()
{
}
