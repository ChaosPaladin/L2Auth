#pragma once

#include "network/CIOSocket.h"
#include "network/SocketStatus.h"
#include "threads/CRWLock.h"

#include "utils/cstdint_support.h"

class CWantedPacket;

class CWantedSocket : public CIOSocket
{
public:
    CWantedSocket(SOCKET socket);  // 0x0043C0E0
    ~CWantedSocket();              // 0x0043C16A

    static CWantedSocket* Allocate(SOCKET socket);  // 0x0043C072

    void OnTimerCallback() override;  // 0x0043C23B
    void OnClose() override;          // 0x0043C1C7
    void OnCreate() override;         // 0x0043C25E
    void OnRead() override;           // 0x0043C27C

    void SetAddress(in_addr ipAddress);  // 0x0042D400

    bool Send(const char* format, ...);  // 0x0043C476

public:
    static bool isReconnecting;
    static CRWLock s_lock;

private:
    static void NTAPI WantedSocketTimerRoutine(void* param, BOOLEAN);  // 0x0043BB3F
    char* IP() const;                                                  // 0x0043C246

    static bool packet0_getVersion(CWantedSocket*, uint8_t* buffer);    // 0x0043BF93
    static bool packet1(CWantedSocket*, uint8_t* buffer);               // 0x0043BFC4
    static bool packet2(CWantedSocket*, uint8_t* buffer);               // 0x0043BFDD
    static bool packet3_dummy_packet(CWantedSocket*, uint8_t* buffer);  // 0x0043BF78

private:
    typedef bool (*PacketHandler)(CWantedSocket*, uint8_t*);
    static const int HANDLERS_NUMBER = 4;
    static const PacketHandler handlers[HANDLERS_NUMBER];

    static HANDLE s_timer;

    int m_packetSize;
    SocketStatus m_status;

    const PacketHandler* m_packetHandlers;

    in_addr m_ipAddress;
    char m_allign[8];
};

extern CWantedSocket* g_SocketWanted;
