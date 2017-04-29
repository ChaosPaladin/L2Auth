#pragma once

#include "network/CIOSocket.h"
#include "network/SocketStatus.h"

class CSocketInt : public CIOSocket
{
public:
    CSocketInt(SOCKET socket);  // 0x0040F25A
    ~CSocketInt();              // 0x0040F2B0

    static CSocketInt* Allocate(SOCKET socket);  // 0x0040F2CC

    void OnClose() override;   // 0x0040F337
    void OnCreate() override;  // 0x0040F475
    void OnRead() override;    // 0x0040F374

    void SetAddress(int ipAddress);  // 0x0040F493

private:
    static void packet00_kickByUid(CSocketInt* socketInt, const char* command);                    // 0x0040E560
    static void packet01_changeSocketLimit(CSocketInt* socketInt, const char* command);            // 0x0040E917
    static void packet02_getUserNumber(CSocketInt* socketInt, const char* command);                // 0x0040E6C2
    static void packet03_kickByAccNameFromAuthServer(CSocketInt* socketInt, const char* command);  // 0x0040E6E3
    static void packet04_changeGmMode(CSocketInt* socketInt, const char* command);                 // 0x0040E849
    static void packet05_stub(CSocketInt* socketInt, const char* command);                         // 0x0040E9BE
    static void packet06_changeGmIp(CSocketInt* socketInt, const char* command);                   // 0x0040E9C3
    static void packet07_stub(CSocketInt* socketInt, const char* command);                         // 0x0040EA99
    static void packet08_changeServerMode(CSocketInt* socketInt, const char* command);             // 0x0040EA9E
    static void packet09_getLoginFlag(CSocketInt* socketInt, const char* command);                 // 0x0040EB66
    static void packet10_checkUser(CSocketInt* socketInt, const char* command);                    // 0x0040EDD3
    static void packet11_kickUserFromWorldServer(CSocketInt* socketInt, const char* command);      // 0x0040EFC9

    static const char* subcommand(char* out, const char* in);  // 0x0040E64A

    void Process(const char* command);                  // 0x0040F416
    bool Send(const char* format, ...);                 // 0x0040F4A9
    void SendBuffer(const char* message, size_t size);  // 0x0040F529
    char* IP() const;                                   // 0x0040F45D

private:
    typedef void (*PacketHandler)(CSocketInt*, const char*);
    static const int HANDLERS_NUMBER = 12;
    static const PacketHandler handlers[HANDLERS_NUMBER];

    SOCKET m_socket;
    SocketStatus m_status;
    const PacketHandler* m_packetHandlers;
    in_addr m_clientIP;
};
