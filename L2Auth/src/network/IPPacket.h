#pragma once

#include "CIOObject.h"

#include <cstdint>

class IPSocket;
class CIOBuffer;

class IPPacket : public CIOObject
{
public:
    IPPacket();   // 0x00426960
    ~IPPacket();  // 0x004269B0

    static IPPacket* Alloc();  // 0x004245CF
    static void FreeAll();     // 0x00424691

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x0042476E

    void Free();  // 0x0042471F

public:
    static LONG g_nPendingPacket;

    IPSocket* m_pSocket;
    CIOBuffer* m_pBuf;

    typedef bool (*PacketFunc)(IPSocket*, uint8_t* packet);
    PacketFunc m_pFunc;
};
