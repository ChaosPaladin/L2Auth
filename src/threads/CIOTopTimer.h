#pragma once

#include "CIOObject.h"

class CIOTopTimer : public CIOObject
{
public:
    CIOTopTimer();   // 0x0042C4D0
    ~CIOTopTimer();  // 0x0042C550

    void OnTimerCallback() override;                                                           // 0x0042ABFF
    void OnEventCallback() override;                                                           // 0x0042C500
    virtual void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped);  // 0x0042C510 miss-overriden in origin code
};
