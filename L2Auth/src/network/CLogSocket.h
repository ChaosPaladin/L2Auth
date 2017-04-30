#pragma once

#include "network/CIOSocket.h"
#include "network/SocketStatus.h"
#include "threads/CRWLock.h"

#include <cstdint>

class CLogPacket;

class CLogSocket : public CIOSocket
{
public:
    CLogSocket(SOCKET socket);  // 0x0042EB84
    ~CLogSocket();              // 0x0042EB23

    static CLogSocket* Allocate(SOCKET socket);  // 0x0042EC33

    void OnTimerCallback() override;  // 0x0042F2C6
    void OnClose() override;          // 0x0042F2D1
    void OnCreate() override;         // 0x0042EEE1
    void OnRead() override;           // 0x0042ECA1

    void SetAddress(in_addr ipAddress);  // 0x0042D3E0
    void Send(const char* format, ...);  // 0x0042EFA9

public:
    static CRWLock s_lock;
    static bool created;
    static bool isReconnecting;

private:
    static void NTAPI onTimeout(void* param, BOOLEAN);  // 0x0042E6CF

    char* IP() const;                     // 0x0042F2AE
    void Send2(const char* format, ...);  // 0x0042F131

    static bool packet0_dummy_handler(CLogSocket*, uint8_t* buffer);  // 0x0042EB08

private:
    typedef bool (*PacketHandler)(CLogSocket*, uint8_t*);
    static const PacketHandler handlers[1];

    static HANDLE s_timer;

    int m_field_92;
    int m_field_96;
    int m_packetSize;
    SocketStatus m_status;
    const PacketHandler* m_packetHandlers;
    in_addr m_logdServIp;
    uint16_t m_socketFamily;
    uint16_t m_logdport;
    in_addr m_logdIP;
};

extern CLogSocket* g_LogDSocket;
