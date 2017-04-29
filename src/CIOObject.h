#pragma once

#include <windows.h>

class CIOObject
{
public:
    virtual ~CIOObject();                                                                      // 0x00418081
    virtual void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped);  // 0x0041817D
    virtual void OnTimerCallback();                                                            // 0x0041818A
    virtual bool RegisterTimer(unsigned int delay, int id);                                    // 0x0041811C
    virtual void OnEventCallback();                                                            // 0x0041819D
    virtual bool RegisterEvent(HANDLE eventObject);                                            // 0x00418154

    int AddRef();                                      // 0x00418095
    void ReleaseRef();                                 // 0x004180B3
    BOOL PostObject(DWORD id, HANDLE completionPort);  // 0x004180FB

protected:
    CIOObject();  // 0x00418060

protected:
    volatile long m_nRefCount;
};
