#pragma once

#include "CIOObject.h"

#include <cstdint>

class CLogSocket;
class CIOBuffer;

class CLogPacket : public CIOObject
{
public:
    CLogPacket();   // 0x0042F440
    ~CLogPacket();  // 0x0042F490

    static CLogPacket* Alloc();  // 0x0042E87C
    static void FreeAll();       // 0x0042E93E

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x0042EA1B

    void Free();  // 0x0042E9CD

public:
    static LONG g_nPendingPacket;

    CLogSocket* m_pSocket;
    CIOBuffer* m_pBuf;

    typedef bool (*PacketFunc)(CLogSocket*, uint8_t* packet);
    PacketFunc m_pFunc;
};
