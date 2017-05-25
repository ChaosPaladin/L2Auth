#include <winsock2.h>

#include "network/CIOSocket.h"

#include "network/CIOBuffer.h"
#include "ui/CLog.h"
#include "utils/CExceptionInit.h"

// 0x0041D4D0
CIOSocket::CIOSocket(SOCKET socket)
    : CIOObject()
    , m_lock()
    , m_overlappedRead()
    , m_overlappedWrite()
    , m_pReadBuf(NULL)
    , m_pFirstBuf(NULL)
    , m_pLastBuf(NULL)
    , m_nPendingWrite(0)
    , m_hSocket(socket)
{
    ::InitializeCriticalSection(&m_lock);

    memset(&m_overlappedRead, 0, sizeof(OVERLAPPED));
    memset(&m_overlappedWrite, 0, sizeof(OVERLAPPED));

    m_pReadBuf = CIOBuffer::Alloc();
}

// 0x0041D583
CIOSocket::~CIOSocket()
{
    ::DeleteCriticalSection(&m_lock);

    m_pReadBuf->Release();
    while (m_pFirstBuf)
    {
        CIOBuffer* buffer = m_pFirstBuf;
        m_pFirstBuf = m_pFirstBuf->m_pNext;
        buffer->Free();
    }
}

// 0x0041D614
void CIOSocket::OnClose()
{
    ;
}

// 0x0041D61F
void CIOSocket::CloseSocket()
{
    SOCKET oldSocket = ::InterlockedExchange((long*)&m_hSocket, INVALID_SOCKET);
    if (oldSocket != INVALID_SOCKET)
    {
        OnClose();
        linger optval;
        optval.l_onoff = 1;
        optval.l_linger = 0;
        ::setsockopt(oldSocket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(linger));
        ::closesocket(oldSocket);
        ReleaseRef();
    }
}

// 0x0041D687
void CIOSocket::Initialize(HANDLE existingCompletionPort)
{
    int zero = 0;
    ::setsockopt(m_hSocket, SOL_SOCKET, SO_RCVBUF, (char*)&zero, sizeof(zero));
    zero = 0;
    ::setsockopt(m_hSocket, SOL_SOCKET, SO_SNDBUF, (char*)&zero, sizeof(zero));
    if (::CreateIoCompletionPort((HANDLE)m_hSocket, existingCompletionPort, (ULONG_PTR)this, 0))
    {
        OnCreate();
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "Initilize CompletionPort Error CloseSocket");
        CloseSocket();  // FIXED
    }
}

// 0x0041D722
void CIOSocket::OnCreate()
{
    OnRead();
}

// 0x0041D738
void CIOSocket::OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped)
{
    if (bSuccess)
    {
        if (lpOverlapped == &m_overlappedRead)
        {
            OnReadCallback(dwTransferred);
        }
        else if (lpOverlapped == &m_overlappedWrite)
        {
            OnWriteCallback(dwTransferred);
        }
    }
    else if (lpOverlapped == &m_overlappedRead || lpOverlapped == &m_overlappedWrite)
    {
        CloseSocket();
    }

    ReleaseRef();
}

// 0x0041D7A9
void CIOSocket::OnReadCallback(int dwTransferred)
{
    if (dwTransferred != 0)
    {
        if (m_nRefCount > 0)
        {
            m_pReadBuf->m_dwSize += dwTransferred;
            OnRead();
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "Invalid Socket!");
        }
    }
    else
    {
        CloseSocket();
    }
}

// 0x0041D80B
void CIOSocket::OnWriteCallback(int dwTransferred)
{
    ::EnterCriticalSection(&m_lock);

    if (dwTransferred == m_pFirstBuf->m_dwSize)
    {
        m_nPendingWrite -= m_pFirstBuf->m_dwSize;
        CIOBuffer* buffer = m_pFirstBuf;
        m_pFirstBuf = m_pFirstBuf->m_pNext;
        if (m_pFirstBuf)
        {
            ::LeaveCriticalSection(&m_lock);
            AddRef();

            WSABUF buffers;
            buffers.len = m_pFirstBuf->m_dwSize;
            buffers.buf = (char*)m_pFirstBuf->m_Buffer;
            DWORD bytesSent;
            if (::WSASend(m_hSocket, &buffers, 1u, &bytesSent, 0, &m_overlappedWrite, 0) && ::GetLastError() != ERROR_IO_PENDING)
            {
                int errorCode = ::GetLastError();
                if (errorCode != WSAENOTSOCK && errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED)
                {
                    g_winlog.AddLog(LOG_ERR, "CIOSocket::WriteCallback %x(%x) err=%d", m_hSocket, this, errorCode);
                }
                ReleaseRef();
            }
        }
        else
        {
            m_pLastBuf = 0;
            ::LeaveCriticalSection(&m_lock);
        }
        buffer->Free();
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "different write count %x(%x) %d != %d", m_hSocket, this, dwTransferred, m_pFirstBuf->m_dwSize);

        ::LeaveCriticalSection(&m_lock);
        ReleaseRef();
    }
}

// 0x0041D995
void CIOSocket::Read(size_t dwLeft)
{
    m_pReadBuf->m_dwSize -= dwLeft;

    if (m_pReadBuf->m_nRefCount == 1)
    {
        memmove(m_pReadBuf->m_Buffer, &m_pReadBuf->m_Buffer[m_pReadBuf->m_dwSize], dwLeft);
    }
    else
    {
        CIOBuffer* pNextBuf = CIOBuffer::Alloc();
        memcpy(pNextBuf->m_Buffer, &m_pReadBuf->m_Buffer[m_pReadBuf->m_dwSize], dwLeft);
        m_pReadBuf->Release();
        m_pReadBuf = pNextBuf;
    }
    m_pReadBuf->m_dwSize = dwLeft;
    AddRef();

    WSABUF buffers;
    buffers.len = BUFFER_SIZE - m_pReadBuf->m_dwSize;
    buffers.buf = (char*)&m_pReadBuf->m_Buffer[m_pReadBuf->m_dwSize];
    DWORD dwFlag = 0;
    DWORD dwRecv = 0;
    LONG result = ::WSARecv(m_hSocket, &buffers, 1u, &dwRecv, &dwFlag, &m_overlappedRead, 0);
    if (result != ERROR_SUCCESS)
    {
        result = ::GetLastError();
        if (result != ERROR_IO_PENDING)
        {
            DWORD errorCode = ::GetLastError();
            if (errorCode != WSAENOTSOCK && errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED)
            {
                g_winlog.AddLog(LOG_ERR, "CIOSocket::Read %x(%x) err = %d", m_hSocket, this, errorCode);
            }
            CloseSocket();
            ReleaseRef();
        }
    }
}

// 0x0041DB1A
void CIOSocket::Write(CIOBuffer* buff)
{
    if (buff->m_dwSize)
    {
        ::EnterCriticalSection(&m_lock);
        m_nPendingWrite += buff->m_dwSize;
        if (m_pLastBuf)
        {
            if (m_pFirstBuf == m_pLastBuf || buff->m_dwSize + m_pLastBuf->m_dwSize > 0x2000)
            {
                m_pLastBuf->m_pNext = buff;
                m_pLastBuf = buff;
                ::LeaveCriticalSection(&m_lock);
            }
            else
            {
                memcpy(&m_pLastBuf->m_Buffer[m_pLastBuf->m_dwSize], buff->m_Buffer, buff->m_dwSize);
                m_pLastBuf->m_dwSize += buff->m_dwSize;
                ::LeaveCriticalSection(&m_lock);
                buff->Free();
            }
        }
        else
        {
            m_pLastBuf = buff;
            m_pFirstBuf = buff;
            ::LeaveCriticalSection(&m_lock);
            AddRef();

            WSABUF wsabuf;
            wsabuf.len = buff->m_dwSize;
            wsabuf.buf = (char*)buff->m_Buffer;
            DWORD dwSent = 0;
            if (::WSASend(m_hSocket, &wsabuf, 1u, &dwSent, 0, &m_overlappedWrite, 0) && ::GetLastError() != ERROR_IO_PENDING)
            {
                DWORD errorCode = ::GetLastError();
                if (errorCode != WSAENOTSOCK && errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED)
                {
                    g_winlog.AddLog(LOG_ERR, "CIOSocket::Write %x(%x) err=%d", m_hSocket, this, errorCode);
                }

                ReleaseRef();
            }
        }
    }
    else
    {
        buff->Free();
    }
}
