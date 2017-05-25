#pragma once

#include "CIOObject.h"

#include "utils/cstdint_support.h"

class CWantedSocket;
class CIOBuffer;

class CWantedPacket : public CIOObject
{
public:
    CWantedPacket();   // 0x0043C5F0
    ~CWantedPacket();  // 0x0043C640

    static CWantedPacket* Alloc();  // 0x0043BCEC
    static void FreeAll();          // 0x0043BDAE

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x0043BE8B

    void Free();  // 0x0043BE3C

public:
    static LONG g_nPendingPacket;

    CWantedSocket* m_pSocket;
    CIOBuffer* m_pBuf;

    typedef bool (*PacketFunc)(CWantedSocket*, uint8_t* packet);
    PacketFunc m_pFunc;
};
