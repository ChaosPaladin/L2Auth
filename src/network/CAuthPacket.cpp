#include "network/CAuthPacket.h"

#include "network/CAuthPacketPool.h"
#include "network/CAuthPacketSlot.h"
#include "network/CAuthSocket.h"
#include "network/CIOBuffer.h"
#include "threads/CLock.h"

long CAuthPacket::g_nPendingPacket;

// 0x00431DF0
CAuthPacket::CAuthPacket()
    : CIOObject()
    , m_pSocket(nullptr)
    , m_pBuf(nullptr)
    , m_pFunc(nullptr)
{
}

// 0x00431E10
CAuthPacket::~CAuthPacket()
{
}

// 0x00431B58
CAuthPacket* CAuthPacket::Alloc()
{

    CAuthPacketSlot* slot = &CAuthPacketPool::g_slot[InterlockedIncrement(&CAuthPacketPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    CAuthPacket* packet = slot->m_data.packet;
    if (packet != nullptr)
    {
        slot->m_data.socket = packet->m_pSocket;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        packet = new CAuthPacket();
    }
    return packet;
}

// 0x00431C69
void CAuthPacket::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        CAuthPacketSlot* slot = &CAuthPacketPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            CAuthPacket* packet = slot->m_data.packet;
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

// 0x00431C1A
void CAuthPacket::Free()
{
    CAuthPacketSlot* slot = &CAuthPacketPool::g_slot[InterlockedDecrement(&CAuthPacketPool::g_nFree) & 0xF];
    slot->m_lock.Enter();
    m_pSocket = slot->m_data.socket;
    slot->m_data.packet = this;
    slot->m_lock.Leave();
}

// 0x0043BE8B
void CAuthPacket::OnIOCallback(BOOL /*bSuccess*/, DWORD dwTransferred, LPOVERLAPPED /*lpOverlapped*/)
{
    uint8_t* packet = m_pBuf->m_Buffer + dwTransferred;
    if ((m_pFunc)(m_pSocket, packet + 1))
    {
        m_pSocket->CloseSocket();
    }

    m_pSocket->ReleaseRef();
    m_pBuf->Release();
    ::InterlockedDecrement(&CAuthPacket::g_nPendingPacket);
    Free();
}
