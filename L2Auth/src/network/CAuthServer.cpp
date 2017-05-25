#include <WinSock2.h>

#include "config/Config.h"
#include "model/CIPAccessLimit.h"
#include "network/CAuthOverlapped.h"
#include "network/CAuthServer.h"
#include "network/CAuthSocket.h"
#include "threads/Threading.h"
#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "utils/CExceptionInit.h"

#include <Mswsock.h>

CAuthServer g_authServer;

HANDLE CAuthServer::s_timerQueue = NULL;

// 0x00418E51
CAuthServer::CAuthServer()
    : CIOObject()
    , m_sockets()
    , m_lock()
    , m_socket(INVALID_SOCKET)
    , m_acceptSocket(INVALID_SOCKET)
    , m_acceptEvent(NULL)
    , m_authSocketFactory(NULL)
    , m_field_60()
{
    CAuthServer::s_timerQueue = ::CreateTimerQueue();
    if (CAuthServer::s_timerQueue == NULL)
    {
        g_winlog.AddLog(LOG_ERR, "CIOServerEx Constructor create socket timer fails");
    }

    ::InitializeCriticalSectionAndSpinCount(&m_lock, 4000u);
}

// 0x00418F0E
CAuthServer::~CAuthServer()
{
    Close();
    ::DeleteCriticalSection(&m_lock);
    ::DeleteTimerQueueEx(CAuthServer::s_timerQueue, 0);

    if (m_acceptEvent)
    {
        ::WSACloseEvent(m_acceptEvent);
    }
}

// 0x00418FA0
void CAuthServer::OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped)
{
    auth_guard;

    if (bSuccess)
    {
        ::QueueUserWorkItem(&CAuthServer::OnAcceptExCallback, PVOID(this), 0);
        CAuthOverlapped* overlapped = static_cast<CAuthOverlapped*>(lpOverlapped);

        sockaddr_in* remoteAddr;
        sockaddr_in* localSockaddr;

        int localSockaddrLength = 0;
        int remoteAddrLength = sizeof(sockaddr);

        ::GetAcceptExSockaddrs(overlapped->outputBuffer, dwTransferred, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, (LPSOCKADDR*)&localSockaddr, &localSockaddrLength, (LPSOCKADDR*)&remoteAddr, &remoteAddrLength);

        CAuthSocket* userSocket = CreateSocket(overlapped->acceptSocket, *remoteAddr);
        if (userSocket != NULL)
        {
            userSocket->Initialize(Threading::g_hCompletionPort);
        }
        else
        {
            ::closesocket(overlapped->acceptSocket);
        }
        overlapped->Free();
        return;
    }

    if (lpOverlapped != NULL)
    {
        CAuthOverlapped* overlapped = static_cast<CAuthOverlapped*>(lpOverlapped);
        ::closesocket(overlapped->acceptSocket);
        delete overlapped;

        ::QueueUserWorkItem(&CAuthServer::OnAcceptExCallback, PVOID(this), 0);
    }

    auth_vunguard;
}

// 0x004191F7
void CAuthServer::OnEventCallback()
{
    ::WSAResetEvent(m_acceptEvent);
    bool result = WrapAcceptEx();
    if (result)
    {
        ::InterlockedIncrement(&CReporter::g_AcceptExThread);
        g_winlog.AddLog(LOG_INF, "AcceptEx Thread Added %d", CReporter::g_AcceptExThread);
    }
}

// 0x004192BD
void CAuthServer::Run(int port, AuthSocketFactory authSocketFactory)
{
    m_authSocketFactory = authSocketFactory;
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (m_socket == INVALID_SOCKET)
    {
        DWORD lastError = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "acceptex socket error %d", lastError);
        return;
    }

    sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = ::htonl(INADDR_ANY);
    name.sin_port = ::htons(port);
    if (::bind(m_socket, (struct sockaddr*)&name, sizeof(name)))
    {
        int lastError = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "bind(port %d) error %d", port, lastError);
        Close();
        return;
    }

    if (::listen(m_socket, 5))  // maximum length of the queue of pending connections
    {
        int lastError = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "listen error %d", lastError);
        Close();
        return;
    }

    HANDLE ioResult = ::CreateIoCompletionPort((HANDLE)m_socket, Threading::g_hCompletionPort, (ULONG_PTR)this, 0);
    if (ioResult == NULL)
    {
        int lastError = ::GetLastError();
        g_winlog.AddLog(LOG_ERR, "CreateIoCompletionPort: %d %x %x\n", lastError, m_socket, Threading::g_hCompletionPort);
        Close();
        return;
    }

    for (int i = 0; i < g_Config.acceptCallNum; ++i)
    {
        if (!WrapAcceptEx())
        {
            Close();
            return;
        }
    }

    CReporter::g_AcceptExThread = g_Config.acceptCallNum;
    g_winlog.AddLog(LOG_INF, "service ready on port %d", port);
}

// 0x00419109
bool CAuthServer::WrapAcceptEx()
{
    CAuthOverlapped* overlapped = CAuthOverlapped::Alloc();
    overlapped->acceptSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (overlapped->acceptSocket == INVALID_SOCKET)
    {
        int lastError = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "socket error %d", lastError);
    }
    else
    {
        DWORD dwReceived;
        if (::AcceptEx(m_socket, overlapped->acceptSocket, overlapped->outputBuffer, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &dwReceived, overlapped) || ::GetLastError() == ERROR_IO_PENDING)
        {
            return true;
        }

        int error = ::WSAGetLastError();
        g_winlog.AddLog(LOG_ERR, "AcceptEx error %d", error);
    }

    if (overlapped->acceptSocket != INVALID_SOCKET)
    {
        ::closesocket(overlapped->acceptSocket);
    }

    overlapped->Free();

    return false;
}

// 0x00419258
void CAuthServer::Stop()
{
    Close();
}

// 0x0041926B
void CAuthServer::Close()
{
    if (m_socket != INVALID_SOCKET)
    {
        SOCKET oldSocket = (SOCKET)::InterlockedExchange((LPLONG)&m_socket, INVALID_SOCKET);
        ::closesocket(oldSocket);
    }

    if (m_acceptEvent)
    {
        ::WSACloseEvent(m_acceptEvent);
        m_acceptEvent = NULL;
    }
}

// 0x00419454
CAuthSocket* CAuthServer::CreateSocket(SOCKET socket, const sockaddr_in& addr)
{
    CAuthSocket* client = NULL;
    if (g_reporter.sockets < g_Config.socketLimit)
    {
        if (g_ipAccessLimit.SetAccessIP(addr.sin_addr))
        {
            client = (*m_authSocketFactory)(socket);
            client->SetAddress(addr.sin_addr.s_addr);

            ::EnterCriticalSection(&m_lock);
            m_sockets.insert(std::make_pair(socket, client));
            ::LeaveCriticalSection(&m_lock);
        }
        else
        {
            g_winlog.AddLog(LOG_DBG, "AccessLimit Expire,%d.%d.%d.%d", addr.sin_addr.S_un.S_un_b.s_b1, addr.sin_addr.S_un.S_un_b.s_b2, addr.sin_addr.S_un.S_un_b.s_b3, addr.sin_addr.S_un.S_un_b.s_b4);
        }
    }

    return client;
}

// 0x00419532
CAuthSocket* CAuthServer::FindSocket(SOCKET socket) const
{
    CAuthSocket* authSocket = NULL;
    ::EnterCriticalSection(&m_lock);

    std::map<SOCKET, CAuthSocket*>::const_iterator it = m_sockets.find(socket);
    if (it != m_sockets.end())
    {
        authSocket = it->second;
        authSocket->AddRef();
    }

    ::LeaveCriticalSection(&m_lock);

    return authSocket;
}

// 0x004195AF
void CAuthServer::RemoveSocket(SOCKET socket)
{
    ::EnterCriticalSection(&m_lock);
    m_sockets.erase(socket);
    ::LeaveCriticalSection(&m_lock);
}

// 0x004191DD
DWORD WINAPI CAuthServer::OnAcceptExCallback(LPVOID lpThreadParameter)
{
    CAuthServer* me = reinterpret_cast<CAuthServer*>(lpThreadParameter);
    me->WrapAcceptEx();
    return 0;
}
