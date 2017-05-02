#pragma once

#include "CIOObject.h"

#include "utils/cstdint_support.h"

class CAuthSocket;
class CIOBuffer;

class CAuthPacket : public CIOObject
{
public:
    CAuthPacket();   // 0x00431DF0
    ~CAuthPacket();  // 0x00431E10

    static CAuthPacket* Alloc();  // 0x00431B58
    static void FreeAll();        // 0x00431C69

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x00431CF7

    void Free();  // 0x00431C1A

public:
    static LONG g_nPendingPacket;

    CAuthSocket* m_pSocket;
    CIOBuffer* m_pBuf;

    typedef bool (*PacketFunc)(CAuthSocket*, uint8_t* packet);
    PacketFunc m_pFunc;
};
