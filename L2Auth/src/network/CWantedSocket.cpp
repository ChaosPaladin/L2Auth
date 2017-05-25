#include "network/CWantedSocket.h"
#include "network/BufferReader.h"
#include "network/CIOBuffer.h"
#include "network/CWantedPacket.h"
#include "network/PacketUtils.h"

#include "ui/CLog.h"

#include "config/Config.h"
#include "threads/Threading.h"
#include "utils/CExceptionInit.h"

#include <cstdarg>

bool CWantedSocket::isReconnecting = false;
CRWLock CWantedSocket::s_lock;
HANDLE CWantedSocket::s_timer;

CWantedSocket* g_SocketWanted = NULL;

const CWantedSocket::PacketHandler CWantedSocket::handlers[CWantedSocket::HANDLERS_NUMBER] = {&CWantedSocket::packet0_getVersion, &CWantedSocket::packet1, &CWantedSocket::packet2, &CWantedSocket::packet3_dummy_packet};

// 0x0043C0E0
CWantedSocket::CWantedSocket(SOCKET socket)
    : CIOSocket(socket)
    , m_packetSize()
    , m_status(SocketStatus_Init)
    , m_packetHandlers(handlers)
    , m_ipAddress(g_Config.wantedIP)
{
    AddRef();
    CWantedSocket::isReconnecting = false;
}

// 0x0043C16A
CWantedSocket::~CWantedSocket()
{
    g_winlog.AddLog(LOG_ERR, "WantedSocket Deleted");
}

// 0x0043C072
CWantedSocket* CWantedSocket::Allocate(SOCKET socket)
{
    return new CWantedSocket(socket);
}

// 0x0042D400
void CWantedSocket::SetAddress(in_addr ipAddress)
{
    m_ipAddress = ipAddress;
}

// 0x0043C1C7
void CWantedSocket::OnClose()
{
    m_status = SocketStatus_Closed;
    CWantedSocket::isReconnecting = true;
    g_Config.useWantedSystem = false;
    g_winlog.AddLog(LOG_ERR, "*close connection WantedSocket from %s, %x(%x)", IP(), m_hSocket, this);
    AddRef();
    ::CreateTimerQueueTimer(&s_timer, 0, &CWantedSocket::WantedSocketTimerRoutine, this, g_Config.wantedReconnectInterval, 0, 0);
}

// 0x0043C23B
void CWantedSocket::OnTimerCallback()
{
}

// 0x0043C27C
void CWantedSocket::OnRead()
{
    if (m_status == SocketStatus_Closed)
    {
        CloseSocket();
        return;
    }

    int dwRead = 0;
    int buffSize = m_pReadBuf->m_dwSize;
    uint8_t* buffer = m_pReadBuf->m_Buffer;

    while (true)
    {
        // read size only
        while (true)
        {
            if (dwRead >= buffSize)
            {
                return Read(0);
            }

            if (m_status != SocketStatus_Init)
            {
                break;
            }

            if (dwRead + 3 > buffSize)
            {
                return Read(buffSize - dwRead);
            }

            m_packetSize = buffer[dwRead] + (buffer[dwRead + 1] << 8) + 1;  // TODO: - g_Config.PacketSizeType; missed
            if (m_packetSize <= 0 || m_packetSize > BUFFER_SIZE)
            {
                g_winlog.AddLog(LOG_ERR, "%d: bad packet size %d", m_hSocket, m_packetSize);
                CloseSocket();
                return;
            }

            dwRead += 2;
            m_status = SocketStatus_BytesRead;
        }

        if (m_status != SocketStatus_BytesRead)
        {
            CloseSocket();
            return;
        }

        if (m_packetSize + dwRead > buffSize)
        {
            return Read(buffSize - dwRead);
        }

        // if ( buffer[dwRead] >= 3 ) // FIXED
        if (buffer[dwRead] >= HANDLERS_NUMBER)
        {
            break;
        }

        CWantedPacket* packet = CWantedPacket::Alloc();
        packet->m_pSocket = this;
        packet->m_pBuf = m_pReadBuf;

        // 2 bytes size, 3rd byte is packet type
        packet->m_pFunc = m_packetHandlers[buffer[dwRead]];

        AddRef();
        m_pReadBuf->AddRef();

        ::InterlockedIncrement(&CWantedPacket::g_nPendingPacket);
        // TODO: initialized with Threading::g_hCompletionPortExtra insie OnTimeout
        // and on WinMain : g_SocketWanted->Initialize(Threading::g_hCompletionPortExtra)
        packet->PostObject(dwRead, Threading::g_hCompletionPort);

        dwRead += m_packetSize;
        m_status = SocketStatus_Init;
    }

    g_winlog.AddLog(LOG_ERR, "unknown protocol %d", buffer[dwRead]);
    CloseSocket();
}

// 0x0043C246
char* CWantedSocket::IP() const
{
    return inet_ntoa(m_ipAddress);
}

// 0x0043C25E
void CWantedSocket::OnCreate()
{
    AddRef();
    OnRead();
}

// 0x0043C476
bool CWantedSocket::Send(const char* format, ...)
{
    AddRef();
    if (m_status != SocketStatus_Closed && !CWantedSocket::isReconnecting && g_Config.useWantedSystem && m_hSocket != INVALID_SOCKET)
    {
        va_list va;
        va_start(va, format);

        CIOBuffer* buff = CIOBuffer::Alloc();
        int packetSize = PacketUtils::VAssemble(&buff->m_Buffer[2], BUFFER_SIZE - 2, format, va);
        if (packetSize != 0)
        {
            //--packetSize;
            packetSize += g_Config.packetSizeType - 1;  // FIXED
            buff->m_Buffer[0] = char(packetSize & 0xFF);
            buff->m_Buffer[1] = char((packetSize >> 8) & 0xFF);
        }
        else
        {
            g_winlog.AddLog(LOG_ERR, "%d: assemble too large packet. format %s", m_hSocket, format);
        }

        // buff->m_dwSize = packetSize + 3;
        buff->m_dwSize = packetSize;  // FIXED
        Write(buff);
        ReleaseRef();
        return true;
    }

    ReleaseRef();
    return false;
}

// 0x0043BB3F
void NTAPI CWantedSocket::WantedSocketTimerRoutine(void* /*param*/, BOOLEAN)
{
    if (CWantedSocket::s_timer)
    {
        ::DeleteTimerQueueTimer(0, CWantedSocket::s_timer, 0);
    }

    CWantedSocket::s_timer = 0;

    if (CWantedSocket::isReconnecting == true)
    {
        SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        sockaddr_in name;
        name.sin_family = AF_INET;
        name.sin_addr = g_Config.wantedIP;
        name.sin_port = ::htons(g_Config.wantedPort);

        int result = ::connect(socket, (const sockaddr*)&name, sizeof(sockaddr_in));

        CWantedSocket* wantedSocket = CWantedSocket::Allocate(socket);
        wantedSocket->SetAddress(g_Config.wantedIP);

        if (result == SOCKET_ERROR)
        {
            wantedSocket->CloseSocket();
            wantedSocket->ReleaseRef();
        }
        else
        {
            CWantedSocket::s_lock.WriteLock();

            g_SocketWanted->ReleaseRef();
            g_SocketWanted = wantedSocket;

            CWantedSocket::isReconnecting = false;
            g_Config.useWantedSystem = true;
            g_SocketWanted->Initialize(Threading::g_hCompletionPortExtra);

            CWantedSocket::s_lock.WriteUnlock();
        }
    }
}

// 0x0043BF93
bool CWantedSocket::packet0_getVersion(CWantedSocket*, uint8_t* buffer)
{
    int version = BufferReader::ReadInt(&buffer);
    g_winlog.AddLog(LOG_WRN, "Get Version %d", version);
    return false;
}

// 0x0043BFC4
bool CWantedSocket::packet1(CWantedSocket*, uint8_t* buffer)
{
    BufferReader::ReadInt(&buffer);
    return false;
}

// 0x0043BFDD
bool CWantedSocket::packet2(CWantedSocket*, uint8_t* buffer)
{
    BufferReader::ReadByte(&buffer);
    return false;
}

// 0x0043BF78
bool CWantedSocket::packet3_dummy_packet(CWantedSocket*, uint8_t* /*buffer*/)
{
    g_winlog.AddLog(LOG_DBG, "Call DummyPacket What What What");
    return false;
}
