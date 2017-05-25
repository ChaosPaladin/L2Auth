#pragma once

#include "network/CIOServer.h"

class CSocketInt;

class CIOServerInt : public CIOServer
{
public:
    typedef CSocketInt*(*CSocketIntFactory)(SOCKET socket);

public:
    CIOServerInt();   // 0x00418A67
    ~CIOServerInt();  // 0x00418A9F

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x00418AD5
    void Run(int port, CSocketIntFactory factoryMethod);                                       // 0x00418BF7

protected:
    CIOSocket* CreateSocket(SOCKET socket, sockaddr_in* clientAddress) override;  // 0x00418B75

private:
    CRITICAL_SECTION m_lock;
    CSocketIntFactory m_factoryMethod;
};

extern CIOServerInt g_CIOServerInt;
