#include <WinSock2.h>

#include "network/CIOServer.h"
#include "network/CIOSocket.h"
#include "threads/Threading.h"
#include "ui/CLog.h"
#include "utils/CExceptionInit.h"

// 0x004182BD
CIOServer::CIOServer()
    : CIOObject()
    , m_hAcceptEvent(NULL)
    , m_socket(INVALID_SOCKET)
{
}

// 0x004182F0
CIOServer::~CIOServer()
{
    Close();
    ::WSACloseEvent(m_hAcceptEvent);
}

// 0x004184ED
void CIOServer::OnIOCallback(BOOL /*bSuccess*/, DWORD /*dwTransferred*/, LPOVERLAPPED /*lpOverlapped*/)
{
    auth_guard;

    sockaddr_in clientAddress;
    int clientAddressLength = sizeof(clientAddress);
    SOCKET newSocket = ::accept(m_socket, (sockaddr*)&clientAddress, &clientAddressLength);

    if (newSocket == INVALID_SOCKET)
    {
        int error = ::WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            return;
        }

        if (m_socket != INVALID_SOCKET)
        {
            g_winlog.AddLog(LOG_ERR, "accept error: %d", error);
            return;
        }

        return;
    }

    CIOSocket* socket = CreateSocket(newSocket, &clientAddress);
    if (socket == nullptr)
    {
        g_winlog.AddLog(LOG_ERR, "ServerClose:CreateSocket Fail");
        ::closesocket(newSocket);
        return;
    }

    socket->Initialize(Threading::g_hCompletionPort);

    auth_vunguard;
}

// 0x00418633
void CIOServer::OnEventCallback()
{
    ::WSAResetEvent(m_hAcceptEvent);
    ::PostQueuedCompletionStatus(Threading::g_hCompletionPort, 0, (DWORD)this, NULL);
}

// 0x0041834E
void CIOServer::Close()
{
    if (m_socket != INVALID_SOCKET)
    {
        SOCKET oldSocket = (SOCKET)::InterlockedExchange((LONG*)(&m_socket), INVALID_SOCKET);
        ::closesocket(oldSocket);
    }
}

// 0x004183AA
bool CIOServer::Create(int port)
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (m_socket == INVALID_SOCKET)
    {
        int err = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "socket error %d", err);
        return false;
    }

    sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = ::htonl(INADDR_ANY);
    name.sin_port = ::htons(port);
    if (::bind(m_socket, (struct sockaddr*)&name, sizeof(name)))
    {
        int err = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "bind error %d", err);
        CIOServer::Close();
        return false;
    }

    if (::listen(m_socket, 5))
    {
        int err = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "listen error %d", err);
        CIOServer::Close();
        return false;
    }

    m_hAcceptEvent = ::WSACreateEvent();
    ::WSAEventSelect(m_socket, m_hAcceptEvent, FD_ACCEPT);

    if (!RegisterEvent(m_hAcceptEvent))
    {
        g_winlog.AddLog(LOG_ERR, "RegisterWait error on port %d", port);
        CIOServer::Close();
        return false;
    }

    return true;
}
