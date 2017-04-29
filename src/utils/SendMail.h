#pragma once

#include <windows.h>

class SendMail
{
public:
    SendMail();

    bool Init();                                                                // 0x00432AB3
    bool SendMessageA(const char* mailTo, const char* subj, const char* body);  // 0x00432D8F

private:
    static char* PrintErrorMessage(DWORD dwMessageId, char* message);  // 0x00432BD3
    bool ConnectMailServer();                                          // 0x00432C19
    void SocketClose();                                                // 0x00432A61
    bool PacketHelo();                                                 // 0x00432FC9
    bool PacketMailFrom();                                             // 0x00433100
    bool PacketRcptTo();                                               // 0x0043322F
    bool PacketData(const char* subject, const char* body);            // 0x004333B7
    bool PacketQuit();                                                 // 0x00433661

private:
    SOCKET m_socket;
    sockaddr_in m_mailServerAddr;
    char m_mails[1024];
};

extern SendMail g_MailService;
