#include "network/IPPacket.h"

#include "network/CIOBuffer.h"
#include "network/IPPacketPool.h"
#include "network/IPPacketSlot.h"
#include "network/IPSocket.h"
#include "threads/CLock.h"
#include "utils/CExceptionInit.h"

long IPPacket::g_nPendingPacket;

// 0x00426960
IPPacket::IPPacket()
    : CIOObject()
    , m_pSocket(nullptr)
    , m_pBuf(nullptr)
    , m_pFunc(nullptr)
{
}

// 0x004269B0
IPPacket::~IPPacket()
{
}

// 0x004245CF
IPPacket* IPPacket::Alloc()
{
    IPPacketSlot* slot = &IPPacketPool::g_slot[InterlockedIncrement(&IPPacketPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    IPPacket* packet = slot->m_data.packet;
    if (packet != nullptr)
    {
        slot->m_data.socket = packet->m_pSocket;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        packet = new IPPacket();
    }
    return packet;
}

// 0x00424691
void IPPacket::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        IPPacketSlot* slot = &IPPacketPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            IPPacket* packet = slot->m_data.packet;
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

// 0x0042471F
void IPPacket::Free()
{
    IPPacketSlot* slot = &IPPacketPool::g_slot[InterlockedDecrement(&IPPacketPool::g_nFree) & 0xF];
    slot->m_lock.Enter();
    m_pSocket = slot->m_data.socket;
    slot->m_data.packet = this;
    slot->m_lock.Leave();
}

// 0x0042476E
void IPPacket::OnIOCallback(BOOL /*bSuccess*/, DWORD dwTransferred, LPOVERLAPPED /*lpOverlapped*/)
{
    auth_guard;

    uint8_t* packet = m_pBuf->m_Buffer + dwTransferred;
    if ((m_pFunc)(m_pSocket, packet + 1))
    {
        m_pSocket->CloseSocket();
    }

    m_pSocket->ReleaseRef();
    m_pBuf->Release();
    ::InterlockedDecrement(&IPPacket::g_nPendingPacket);
    Free();

    auth_vunguard;
}
