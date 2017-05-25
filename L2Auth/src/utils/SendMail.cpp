#include "utils/SendMail.h"

#include "config/Config.h"
#include "ui/CLog.h"
#include "utils/CExceptionInit.h"

SendMail g_MailService;

SendMail::SendMail()
    : m_socket(INVALID_SOCKET)
    , m_mailServerAddr()
{
}

// 0x00432A61
void SendMail::SocketClose()
{
    SOCKET oldSocket = ::InterlockedExchange((volatile LONG*)&m_socket, INVALID_SOCKET);
    if (oldSocket != INVALID_SOCKET)
    {
        linger optval;
        optval.l_linger = 0;
        optval.l_onoff = 1;
        setsockopt(oldSocket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
        ::closesocket(oldSocket);
    }
}

// 0x00432AB3
bool SendMail::Init()
{
    int serverNameLength = strlen(g_Config.mailServer);
    if (serverNameLength > 0 && serverNameLength <= 128)
    {
        hostent* host = gethostbyname(g_Config.mailServer);
        // FIXED: if no network connection, returns null
        if (host == NULL)
        {
            return false;
        }
        char serverIp[128];
        memset(serverIp, 0, sizeof(serverIp));
        char* srvIp = ::inet_ntoa(**(in_addr**)host->h_addr_list);
        memcpy(serverIp, srvIp, 20);
        m_mailServerAddr.sin_family = SOCK_DGRAM;
        m_mailServerAddr.sin_port = htons(25u);
        m_mailServerAddr.sin_addr.s_addr = ::inet_addr(serverIp);
        return true;
    }

    return false;
}

// 0x00432C19
bool SendMail::ConnectMailServer()
{
    DWORD lastError = 0;
    m_socket = ::socket(SOCK_DGRAM, AF_UNIX, IPPROTO_IP);
    if (m_socket == INVALID_SOCKET)
    {
        lastError = ::GetLastError();
        return false;
    }

    int optval = 5000;
    if (::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
    {
        lastError = ::GetLastError();
        return false;
    }

    if (::setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
    {
        lastError = ::GetLastError();
        return false;
    }

    char buf[1024];
    memset(buf, 0, 0x400u);
    bool result = false;
    if (::connect(m_socket, (const sockaddr*)&m_mailServerAddr, sizeof(m_mailServerAddr)) == SOCKET_ERROR)
    {
        lastError = ::GetLastError();
    }
    else if (::recv(m_socket, buf, 1024, 0) == SOCKET_ERROR)
    {
        lastError = ::GetLastError();
    }
    else
    {
        result = true;
    }

    if (result)
    {
        if (strncmp(buf, "220", 3u))
        {
            SocketClose();
            return false;
        }
    }
    else
    {
        SocketClose();
    }

    return true;
}

// 0x00432D8F
bool SendMail::SendMessageA(const char* mailTo, const char* subj, const char* body)
{
    auth_guard;

    int subjLen = strlen(subj);
    if (subjLen <= 0 || subjLen > 1024)
    {
        return false;
    }

    int bodyLen = strlen(body);
    if (bodyLen <= 0 || bodyLen > 1024)
    {
        return false;
    }

    bool result = false;
    if (strchr(g_Config.mailFrom, 64))
    {
        if (strchr(mailTo, 64))
        {
            if (strlen(g_Config.mailFrom) > 4 && strlen(g_Config.mailFrom) < 0x400)
            {
                int mailToLen = strlen(mailTo);
                if (mailToLen > 4 && mailToLen < 1024)
                {
                    strcpy(m_mails, mailTo);
                    m_mails[mailToLen] = 0;
                    if (!ConnectMailServer())
                    {
                        return false;
                    }

                    result = PacketHelo();
                    if (result)
                    {
                        result = PacketMailFrom();
                        if (result)
                        {
                            result = PacketRcptTo();
                            if (result)
                            {
                                result = PacketData(subj, body);
                                if (result)
                                {
                                    result = PacketQuit();
                                }
                            }
                        }
                    }
                    if (!result)
                    {
                        g_winlog.AddLog(LOG_ERR, "mailsend error , %s", body);
                    }
                }
            }
        }
    }

    return result;

    auth_vunguard;

    return false;
}

// 0x00432FC9
bool SendMail::PacketHelo()
{
    char buf[1024];
    DWORD lastError = 0;
    memset(buf, 0, 0x400u);
    if (m_socket == -1)
    {
        return false;
    }

    strcpy(buf, "HELO mail.ncsoft.net\r\n");

    bool result = false;
    size_t len = strlen(buf);
    if (::send(m_socket, buf, len, 0) == -1)
    {
        lastError = ::GetLastError();
    }
    else
    {
        memset(buf, 0, 0x400u);
        if (::recv(m_socket, buf, 1024, 0) == -1)
        {
            lastError = ::GetLastError();
        }
        else
        {
            result = true;
        }
    }

    if (result)
    {
        if (strncmp(buf, "250", 3u))
        {
            SocketClose();
            result = false;
        }
    }
    else
    {
        SocketClose();
    }

    return result;
}

// 0x00433100
bool SendMail::PacketMailFrom()
{
    if (m_socket == -1)
    {
        return false;
    }

    DWORD lastError = 0;
    char buf[1024];
    memset(buf, 0, 0x400u);

    bool result = false;
    int len = sprintf(buf, "MAIL From: <%s>\r\n", g_Config.mailFrom);
    if (::send(m_socket, buf, len, 0) == -1)
    {
        lastError = ::GetLastError();
    }
    else
    {
        memset(buf, 0, 0x400u);
        if (::recv(m_socket, buf, 1024, 0) == -1)
        {
            lastError = ::GetLastError();
        }
        else
        {
            result = true;
        }
    }
    if (result)
    {
        if (strncmp(buf, "250", 3u))
        {
            SocketClose();
            result = false;
        }
    }
    else
    {
        SocketClose();
    }

    return result;
}

// 0x0043322F
bool SendMail::PacketRcptTo()
{
    DWORD lastError = 0;
    if (m_socket == -1)
    {
        return false;
    }

    char buf[1024];
    char mailsList[1024];
    bool result = false;
    memset(mailsList, 0, 0x400u);
    strcpy(mailsList, m_mails);
    for (char* i = strtok(mailsList, ";"); i; i = strtok(0, ";"))
    {
        memset(buf, 0, 0x400u);
        int len = sprintf(buf, "RCPT To: <%s>\r\n", i);
        if (::send(m_socket, buf, len, 0) == -1)
        {
            lastError = ::GetLastError();
        }
        else
        {
            memset(buf, 0, 0x400u);
            if (::recv(m_socket, buf, 1024, 0) == -1)
            {
                lastError = ::GetLastError();
            }
            else if (!strncmp(buf, "250", 3u))
            {
                result = true;
            }
        }
    }

    if (!result)
    {
        SocketClose();
    }

    return result;
}

// 0x004333B7
bool SendMail::PacketData(const char* subject, const char* body)
{
    char buf[1024];
    memset(buf, 0, 0x400u);
    if (m_socket == -1)
    {
        return false;
    }

    DWORD lastError = 0;
    bool result = false;
    strcpy(buf, "DATA\r\n");
    size_t len = strlen(buf);

    if (::send(m_socket, buf, len, 0) == -1)
    {
        lastError = ::GetLastError();
    }
    else
    {
        memset(buf, 0, 0x400u);
        if (::recv(m_socket, buf, 1024, 0) == -1)
        {
            lastError = ::GetLastError();
        }
        else
        {
            result = true;
        }
    }
    if (result)
    {
        result = false;
        memset(buf, 0, 0x400u);
        int subjLen = sprintf(buf, "Subject:%s\r\nTo:%s\r\n\r\n", subject, m_mails);
        if (::send(m_socket, buf, subjLen, 0) == -1)
        {
            lastError = ::GetLastError();
        }
        else
        {
            memset(buf, 0, 0x400u);
            int bodyLen = sprintf(buf, "%s\r\n", body);
            if (::send(m_socket, buf, bodyLen, 0) == -1)
            {
                lastError = ::GetLastError();
            }
            else
            {
                if (::send(m_socket, "\r\n.\r\n", strlen("\r\n.\r\n"), 0) == -1)  // 354 End data with <CR><LF>.<CR><LF>
                {
                    lastError = ::GetLastError();
                }
                else
                {
                    memset(buf, 0, 0x400u);
                    if (::recv(m_socket, buf, 1024, 0) == -1)
                    {
                        lastError = ::GetLastError();
                    }
                    else
                    {
                        result = true;
                    }
                }
            }
        }
    }
    if (result)
    {
        if (strncmp(buf, "250", 3u))
        {
            SocketClose();
            result = false;
        }
    }
    else
    {
        SocketClose();
    }

    return result;
}

// 0x00433661
bool SendMail::PacketQuit()
{
    DWORD lastError = 0;
    bool result = false;
    char buf[1024];
    memset(buf, 0, 0x400u);
    if (m_socket == -1)
    {
        return false;
    }

    strcpy(buf, "QUIT\r\n");
    size_t len = strlen(buf);
    if (::send(m_socket, buf, len, 0) == -1)
    {
        lastError = ::GetLastError();
    }
    else
    {
        result = true;
    }

    SendMail::SocketClose();

    return result;
}

// 0x00432BD3
char* SendMail::PrintErrorMessage(DWORD dwMessageId, char* message)
{
    char* buffer = NULL;
    DWORD size = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM, 0, dwMessageId, 0, (LPSTR)&buffer, 0, 0);
    if (size != 0 && ((size + 14) <= 1024))
    {
        buffer[strlen(buffer) - 2] = 0;
        sprintf(message, "%s(0x%x)", buffer, dwMessageId);
    }
    else
    {
        *message = 0;
    }
    if (buffer)
        ::LocalFree(buffer);
    return message;
}
