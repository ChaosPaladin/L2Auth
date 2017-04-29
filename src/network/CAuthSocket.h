#pragma once

#include "model/LoginFailReason.h"
#include "model/LoginState.h"
#include "network/CIOSocket.h"
#include "network/SocketStatus.h"

#include <cstdint>

class CAuthPacket;
class LoginUser;

class CAuthSocket : public CIOSocket
{
public:
    CAuthSocket(SOCKET socket);  // 0x00439730
    ~CAuthSocket();              // 0x0043989F

    static CAuthSocket* Allocate(SOCKET socket);               // 0x004398BB
    static bool Send(SOCKET socket, const char* format, ...);  // 0x00410A93

    void OnTimerCallback() override;  // 0x00439FD8
    void OnClose() override;          // 0x00439929
    void OnCreate() override;         // 0x00439C87
    void OnRead() override;           // 0x00439A04

    void SetAddress(int ipAddress);  // 0x00439D3C

    LoginFailReason CheckPersonalPayStat(LoginUser* accInfo, int uid);  // 0x0040481A
    int GetMd5Key() const;                                              // 0x004056D0

    bool Send(const char* format, ...);  // 0x00439D55

private:
    static void NTAPI SocketExTimerCallback(void* param, BOOLEAN);                      // 0x004396FC
    static bool packet00_RequestAuthLogin(CAuthSocket* authSock_, uint8_t* buffer);     // 0x0043818F
    static bool packet01_ServerListPacket(CAuthSocket* authSock, uint8_t* buffer);      // 0x0043896C
    static bool packet02_RequestServerLogin(CAuthSocket* authSock, uint8_t* buffer);    // 0x00438A69
    static bool packet03_CancelLogin(CAuthSocket* authSock, uint8_t* buffer);           // 0x00438D5A
    static bool packet05_RequestServerList_C1(CAuthSocket* authSock, uint8_t* buffer);  // 0x00438DA3
    static bool packet04_unknownHandler(CAuthSocket* authSock, uint8_t* buffer);        // 0x00439225
    static bool packet06_unknownHandler(CAuthSocket* authSock, uint8_t* buffer);        // 0x0043925D
    static bool packet07_charsHandler(CAuthSocket* authSock, uint8_t* buffer);          // 0x00439436
    static bool packet08_unknownHandler(CAuthSocket* authSock, uint8_t* buffer);        // 0x00439627

    void NonEncSend(const char* format, ...);                // 0x00439E5B
    void SendPacket(const char* packet, size_t packetSize);  // 0x00439F30
    in_addr getaddr() const;                                 // 0x0043A080

private:
    typedef bool (*PacketHandler)(CAuthSocket*, uint8_t*);
    static const int HANDLERS_NUMBER = 9;
    static const PacketHandler handlers[HANDLERS_NUMBER];

    SOCKET m_socket;

public:
    LoginState m_clientLoginState;

private:
    int m_uid;
    char m_xorKeySend[8];
    char m_xorKeyReceive[8];
    unsigned int m_lastPacketTime;
    bool(__cdecl* m_encrypt_handler)(uint8_t*, char*, int*);
    bool(__cdecl* m_decrypt_handler)(uint8_t*, char*, int);
    int m_packetSize;
    int m_field_136;
    HANDLE m_socketTimeoutTimer;
    SocketStatus m_status;
    const PacketHandler* m_packetHandlers;
    in_addr m_clientIP;
    int m_unused_156;
    int m_sessionKey;
    char m_gap164[4172];
};
