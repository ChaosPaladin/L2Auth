#include "CIOObject.h"
#include "threads/CJob.h"
#include "utils/Unused.h"

// 0x00418060
CIOObject::CIOObject()
{
    m_nRefCount = 0;
}

// 0x00418081
CIOObject::~CIOObject()
{
}

// 0x00418095
int CIOObject::AddRef()
{
    ::InterlockedIncrement(&m_nRefCount);  // TODO: x64 unsafe
    return m_nRefCount;
}

// 0x004180B3
void CIOObject::ReleaseRef()
{
    ::InterlockedDecrement(&m_nRefCount);  // TODO: x64 unsafe
    if (m_nRefCount == 0)
    {
        if (this != NULL)
        {
            delete this;
        }
    }
}

// 0x0041811C
bool CIOObject::RegisterTimer(unsigned int delay, int id)
{
    if (g_job.PushTimer(this, delay, id))
    {
        AddRef();
        return true;
    }
    return false;
}

// 0x00418154
bool CIOObject::RegisterEvent(HANDLE eventObject)
{
    return g_job.PushEvent(eventObject, this);
}

// 0x0041817D
void CIOObject::OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped)
{
    UNUSED(bSuccess);
    UNUSED(dwTransferred);
    UNUSED(lpOverlapped);
}

// 0x0041818A
void CIOObject::OnTimerCallback()
{
    ReleaseRef();
}

// 0x0041819D
void CIOObject::OnEventCallback()
{
}

// 0x004180FB
BOOL CIOObject::PostObject(DWORD id, HANDLE completionPort)
{
    return ::PostQueuedCompletionStatus(completionPort, id, (ULONG_PTR)this, NULL);
}
