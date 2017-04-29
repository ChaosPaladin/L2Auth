#include "network/WorldSrvPacket.h"

#include "network/CIOBuffer.h"
#include "network/WorldSrvPacketPool.h"
#include "network/WorldSrvPacketSlot.h"
#include "network/WorldSrvSocket.h"
#include "threads/CLock.h"
#include "ui/CLog.h"

long WorldSrvPacket::g_nPendingPacket;

// 0x0041E100
WorldSrvPacket::WorldSrvPacket()
    : CIOObject()
    , m_pSocket(nullptr)
    , m_pBuf(nullptr)
    , m_pFunc(nullptr)
{
}

// 0x0041E150
WorldSrvPacket::~WorldSrvPacket()
{
}

// 0x0041DE1D
WorldSrvPacket* WorldSrvPacket::Alloc()
{
    WorldSrvPacketSlot* slot = &WorldSrvPacketPool::g_slot[InterlockedIncrement(&WorldSrvPacketPool::g_nAlloc) & 0xF];
    slot->m_lock.Enter();

    WorldSrvPacket* packet = slot->m_data.packet;
    if (packet != nullptr)
    {
        slot->m_data.socket = packet->m_pSocket;
        slot->m_lock.Leave();
    }
    else
    {
        slot->m_lock.Leave();
        packet = new WorldSrvPacket();
    }
    return packet;
}

// 0x0041DF2E
void WorldSrvPacket::FreeAll()
{
    for (int i = 0; i < 16; ++i)
    {
        WorldSrvPacketSlot* slot = &WorldSrvPacketPool::g_slot[i];
        slot->m_lock.Enter();
        while (true)
        {
            WorldSrvPacket* packet = slot->m_data.packet;
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

// 0x0041DEDF
void WorldSrvPacket::Free()
{
    WorldSrvPacketSlot* slot = &WorldSrvPacketPool::g_slot[InterlockedDecrement(&WorldSrvPacketPool::g_nFree) & 0xF];
    slot->m_lock.Enter();
    m_pSocket = slot->m_data.socket;
    slot->m_data.packet = this;
    slot->m_lock.Leave();
}

// 0x0041DFBC
void WorldSrvPacket::OnIOCallback(BOOL /*bSuccess*/, DWORD dwTransferred, LPOVERLAPPED /*lpOverlapped*/)
{
    uint8_t* packet = m_pBuf->m_Buffer + dwTransferred;
    if ((m_pFunc)(m_pSocket, packet + 1))
    {
        m_pSocket->CloseSocket();
        g_winlog.AddLog(LOG_ERR, "ServerClose:PacketServerClose");
    }

    m_pSocket->ReleaseRef();
    m_pBuf->Release();
    ::InterlockedDecrement(&WorldSrvPacket::g_nPendingPacket);
    Free();
}
