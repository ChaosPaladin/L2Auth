#pragma once

#include "CIOObject.h"

#include <cstdint>

class WorldSrvSocket;
class CIOBuffer;

class WorldSrvPacket : public CIOObject
{
public:
    WorldSrvPacket();   // 0x0041E100
    ~WorldSrvPacket();  // 0x0041E150

    static WorldSrvPacket* Alloc();  // 0x0041DE1D
    static void FreeAll();           // 0x0041DF2E

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x0041DFBC
    void Free();                                                                                // 0x0041DEDF

public:
    static LONG g_nPendingPacket;

    WorldSrvSocket* m_pSocket;
    CIOBuffer* m_pBuf;

    typedef bool (*PacketFunc)(WorldSrvSocket*, uint8_t* packet);
    PacketFunc m_pFunc;
};
