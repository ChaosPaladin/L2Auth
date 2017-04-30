#pragma once

#include "CIOObject.h"

#include <map>

class CAuthSocket;

class CAuthServer : public CIOObject
{
public:
    using AuthSocketFactory = CAuthSocket*(SOCKET socket);

public:
    CAuthServer();   // 0x00418E51
    ~CAuthServer();  // 0x00418F0E

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x00418FA0
    void OnEventCallback() override;                                                            // 0x004191F7

    void Stop();                                               // 0x00419258
    void Run(int port, AuthSocketFactory* authSocketFactory);  // 0x004192BD

    void RemoveSocket(SOCKET socket);              // 0x004195AF
    CAuthSocket* FindSocket(SOCKET socket) const;  // 0x00419532

protected:
    virtual CAuthSocket* CreateSocket(SOCKET socket, const sockaddr_in& addr);  // 0x00419454

private:
    static DWORD WINAPI OnAcceptExCallback(LPVOID lpThreadParameter);  // 0x004191DD

    bool WrapAcceptEx();  // 0x00419109
    void Close();         // 0x0041926B

public:
    static HANDLE s_timerQueue;

private:
    std::map<SOCKET, CAuthSocket*> m_sockets;
    mutable CRITICAL_SECTION m_lock;
    SOCKET m_socket;
    SOCKET m_acceptSocket;
    HANDLE m_acceptEvent;
    AuthSocketFactory* m_authSocketFactory;
    int m_field_60;
};

extern CAuthServer g_authServer;
