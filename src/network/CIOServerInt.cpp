#include <WinSock2.h>

#include "network/CIOServerInt.h"
#include "network/CSocketInt.h"
#include "threads/Threading.h"
#include "ui/CLog.h"

CIOServerInt g_CIOServerInt;

// 0x00418A67
CIOServerInt::CIOServerInt()
    : CIOServer()
    , m_lock()
    , m_factoryMethod(nullptr)
{
    ::InitializeCriticalSectionAndSpinCount(&m_lock, 4000u);
}

// 0x00418A9F
CIOServerInt::~CIOServerInt()
{
    ::DeleteCriticalSection(&m_lock);
    ::CloseHandle(Threading::g_hCompletionPortExtra);
}

// 0x00418AD5
void CIOServerInt::OnIOCallback(BOOL /*bSuccess*/, DWORD /*dwTransferred*/, LPOVERLAPPED /*lpOverlapped*/)
{
    sockaddr_in clientAddress;
    int clientAddressLength = sizeof(clientAddress);
    SOCKET newSocket = ::accept(m_socket, (sockaddr*)&clientAddress, &clientAddressLength);

    if (newSocket == INVALID_SOCKET)
    {
        int error = ::WSAGetLastError();
        if ((error != WSAEWOULDBLOCK) && (m_socket != INVALID_SOCKET))
        {
            g_winlog.AddLog(LOG_ERR, "accept error: %d", error);
        }
        return;
    }

    CIOSocket* socket = CreateSocket(newSocket, &clientAddress);
    if (socket == nullptr)
    {
        ::closesocket(newSocket);
        return;
    }

    socket->Initialize(Threading::g_hCompletionPortExtra);
}

// 0x00418BF7
void CIOServerInt::Run(int port, CSocketIntFactory* factoryMethod)
{
    m_factoryMethod = factoryMethod;
    if (CIOServer::Create(port))
    {
        g_winlog.AddLog(LOG_INF, "interactive server ready on port %d", port);
    }
}

// 0x00418B75
CIOSocket* CIOServerInt::CreateSocket(SOCKET socket, sockaddr_in* clientAddress)
{
    CSocketInt* interactiveSocket = m_factoryMethod(socket);
    interactiveSocket->SetAddress(clientAddress->sin_addr.s_addr);
    g_winlog.AddLog(LOG_INF, "*new interactive socket connection from %d.%d.%d.%d", clientAddress->sin_addr.S_un.S_un_b.s_b1, clientAddress->sin_addr.S_un.S_un_b.s_b2, clientAddress->sin_addr.S_un.S_un_b.s_b3, clientAddress->sin_addr.S_un.S_un_b.s_b4);

    return interactiveSocket;
}
