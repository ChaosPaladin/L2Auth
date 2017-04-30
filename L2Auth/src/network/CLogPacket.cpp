#include "network/CLogPacket.h"

#include "network/CIOBuffer.h"
#include "network/CLogPacketPool.h"
#include "network/CLogPacketSlot.h"
#include "network/CLogSocket.h"
#include "threads/CLock.h"
#include "utils/CExceptionInit.h"

long CLogPacket::g_nPendingPacket;

// 0x0042F440
CLogPacket::CLogPacket()
    : CIOObject()
    , m_pSocket(nullptr)
    , m_pBuf(nullptr)
    , m_pFunc(nullptr)
{
}

// 0x0042F490
CLogPacket::~CLogPacket()
{
}

// 0x0042E87C
CLogPacket* CLogPacket::Alloc()
{
    CLogPacketSlot* slot = &CLogPacketPool::g_slot[InterlockedIncrement(&CLogPacketPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    CLogPacket* packet = slot->m_data.packet;
    if (packet != nullptr)
    {
        slot->m_data.socket = packet->m_pSocket;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        packet = new CLogPacket();
    }
    return packet;
}

// 0x0042E93E
void CLogPacket::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        CLogPacketSlot* slot = &CLogPacketPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            CLogPacket* packet = slot->m_data.packet;
            if (packet == nullptr)
            {
                break;
            }

            slot->m_data.socket = packet->m_pSocket;
            if (packet != nullptr)
            {
                delete packet;
            }
        }
        slot->m_lock.Leave();
    }
}

// 0x0042E9CD
void CLogPacket::Free()
{
    CLogPacketSlot* slot = &CLogPacketPool::g_slot[InterlockedDecrement(&CLogPacketPool::g_nFree) & 0xF];
    slot->m_lock.Enter();
    m_pSocket = slot->m_data.socket;
    slot->m_data.packet = this;
    slot->m_lock.Leave();
}

// 0x0042EA1B
void CLogPacket::OnIOCallback(BOOL /*bSuccess*/, DWORD dwTransferred, LPOVERLAPPED /*lpOverlapped*/)
{
    auth_guard;

    uint8_t* packet = m_pBuf->m_Buffer + dwTransferred;
    if ((m_pFunc)(m_pSocket, packet + 1))
    {
        m_pSocket->CloseSocket();
    }

    m_pSocket->ReleaseRef();
    m_pBuf->Release();
    ::InterlockedDecrement(&CLogPacket::g_nPendingPacket);
    Free();

    auth_vunguard;
}
