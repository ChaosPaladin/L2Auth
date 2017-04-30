#include <WinSock2.h>

#include "utils/CExceptionInit.h"
#include "utils/CoreDump.h"
#include "utils/Utils.h"

#include <cstdio>
#include <cstring>

// const unsigned int ERR_NO_DISK_SPACE = 0xE0000001; // WinXP only

char CExceptionInit::s_logPath[MAX_PATH];
time_t CExceptionInit::s_startTime;
char* CExceptionInit::s_mailServerIP;
bool CExceptionInit::s_intentionException;
bool CExceptionInit::s_errorReported;

LPTOP_LEVEL_EXCEPTION_FILTER CExceptionInit::s_oldFilter;
CRITICAL_SECTION CExceptionInit::g_criticalSection;
bool CExceptionInit::s_beenHere;
char* CExceptionInit::s_mailTemplate;

CExceptionInit g_exceptionHandler;

// 0x004176C1
CExceptionInit::CExceptionInit()
{
    CExceptionInit::initCritSection();

    char buffer[MAX_PATH];
    strcpy(buffer, "Unknown");
    memset(&buffer[8], 0, 252u);
    if (!::GetModuleFileNameA(0, CExceptionInit::s_logPath, MAX_PATH))
    {
        CExceptionInit::s_logPath[0] = '\0';
    }

    const char* exeName = Utils::getFileName(CExceptionInit::s_logPath);
    // Unused  vvv TODO
    ::lstrcpyA(buffer, exeName);
    char* extension = ::strrchr(buffer, '.');
    if (extension)
    {
        *extension = '\0';
    }
    //         ^^^

    ::lstrcpyA((char*)exeName, "L2AuthDError.txt");
    CExceptionInit::s_startTime = time(0);
    CExceptionInit::SetUpExceptionFilter();
}

// 0x00417788
CExceptionInit::~CExceptionInit()
{
    CExceptionInit::ClearExceptionFilter();
}

// 0x00417A31
LONG CExceptionInit::exception_filter(_EXCEPTION_POINTERS* ex)
{
    // if ( CExceptionInit::s_errorReported || ex->ExceptionRecord->ExceptionCode ==
    // ERR_NO_DISK_SPACE ) FIXED // WinXP only
    if (CExceptionInit::s_errorReported)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    CExceptionInit::enterCritSection();

    if (CExceptionInit::s_mailServerIP == nullptr)
    {
        ::DeleteFileA(CExceptionInit::s_logPath);
    }

    CoreDump::createReport(ex);

    CExceptionInit::leaveCritSection();

    return EXCEPTION_EXECUTE_HANDLER;
}

// 0x0041779D
void CExceptionInit::logException(const char* format, ...)
{
    if (CExceptionInit::s_errorReported)
    {
        return;
    }

    va_list va;
    va_start(va, format);
    CExceptionInit::enterCritSection();

    SYSTEMTIME now;
    now.wYear = 0;
    now.wMonth = 0;
    now.wDayOfWeek = 0;
    now.wDay = 0;
    now.wHour = 0;
    now.wMinute = 0;
    now.wSecond = 0;
    now.wMilliseconds = 0;
    ::GetLocalTime(&now);
    HANDLE hFile = ::CreateFileA(CExceptionInit::s_logPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == (HANDLE)INVALID_HANDLE_VALUE)
    {
        CExceptionInit::leaveCritSection();
        return;
    }

    ::SetFilePointer(hFile, 0, 0, FILE_END);
    DWORD currentThread = ::GetCurrentThreadId();
    char buffer[1024];
    int len = ::_snprintf(buffer, 1024u, "[(%d) %04d/%02d/%02d %02d:%02d:%02d]: ", currentThread, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    if (len >= 0)
    {
        DWORD bytesWritten;
        ::WriteFile(hFile, buffer, len, &bytesWritten, 0);

        len = ::vsnprintf(buffer, sizeof(buffer), format, va);
        if (len >= 0)
        {
            for (int i = 0; i < len; i += 2)
            {
                char* newLine = (char*)std::memchr(&buffer[i], '\n', len - i);
                if (newLine == nullptr)
                {
                    break;
                }

                if (len < sizeof(buffer))
                {
                    ++len;
                }

                // inserting \r near to  \n
                i = newLine - buffer;
                memmove(newLine + 1, newLine, len - i - 1);
                *newLine = '\r';
            }
            ::WriteFile(hFile, buffer, len, &bytesWritten, 0);
        }
    }

    ::CloseHandle(hFile);
    CExceptionInit::leaveCritSection();
}

// 0x00417AB1
void CExceptionInit::sendExceptionLog(bool fatal)
{
    if (CExceptionInit::s_errorReported)
    {
        return;
    }

    CExceptionInit::enterCritSection();
    if ((CExceptionInit::s_mailServerIP != nullptr) && CExceptionInit::sendMail(CExceptionInit::s_mailServerIP, "server@ncsoft.co.kr", "darkangel@ncsoft.co.kr", CExceptionInit::s_logPath))
    {
        ::DeleteFileA(CExceptionInit::s_logPath);
    }

    CExceptionInit::leaveCritSection();
    if (fatal)
    {
        CExceptionInit::s_errorReported = true;
        ::ExitProcess(0);
    }
}

// 0x00417692
void CExceptionInit::SetUpExceptionFilter()
{
    CExceptionInit::s_oldFilter = ::SetUnhandledExceptionFilter(&CExceptionInit::RecordExceptionInfo);
}

// 0x00417B55
void CExceptionInit::Init()
{
    if (CExceptionInit::s_mailServerIP == nullptr)
    {
        return;
    }

    HANDLE lastReportExists = ::CreateFileA(CExceptionInit::s_logPath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (lastReportExists == INVALID_HANDLE_VALUE)
    {
        return;
    }

    ::CloseHandle(lastReportExists);

    CExceptionInit::sendMail(CExceptionInit::s_mailServerIP, "server@ncsoft.co.kr", "darkangel@ncsoft.co.kr", CExceptionInit::s_logPath);

    ::DeleteFileA(CExceptionInit::s_logPath);
}

// 0x004176A7
void CExceptionInit::ClearExceptionFilter()
{
    if (CExceptionInit::s_oldFilter != nullptr)
    {
        ::SetUnhandledExceptionFilter(CExceptionInit::s_oldFilter);
    }
}

// 0x00416386
LONG WINAPI CExceptionInit::RecordExceptionInfo(_EXCEPTION_POINTERS* exceptionInfo)
{
    if (CExceptionInit::s_beenHere)
    {
        if (CExceptionInit::s_oldFilter)
        {
            return CExceptionInit::s_oldFilter(exceptionInfo);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CExceptionInit::s_beenHere = true;

    // if ( !CExceptionInit::s_errorReported  && (exceptionInfo->ExceptionRecord->ExceptionCode !=
    // ERR_NO_DISK_SPACE )) // WinXP only
    if (!CExceptionInit::s_errorReported)  // FIXED
    {
        CExceptionInit::enterCritSection();

        if (CExceptionInit::s_mailServerIP == nullptr)
        {
            ::DeleteFileA(CExceptionInit::s_logPath);
        }

        CoreDump::createReport(exceptionInfo);

        if ((CExceptionInit::s_mailServerIP != nullptr) && CExceptionInit::sendMail(CExceptionInit::s_mailServerIP, "server@ncsoft.co.kr", "darkangel@ncsoft.co.kr", CExceptionInit::s_logPath))
        {
            ::DeleteFileA(CExceptionInit::s_logPath);
        }

        CExceptionInit::leaveCritSection();
    }

    if (CExceptionInit::s_oldFilter)
    {
        return CExceptionInit::s_oldFilter(exceptionInfo);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

/*!
 * \brief CExceptionInit::sendMail
 * \param ipAddress
 * \param mailFrom
 * \param mailTo - recepiens, separated by null-characted '\0'
 * \param errorLogPath
 * \return
 */
// 0x00416486
bool CExceptionInit::sendMail(const char* ipAddress, const char* mailFrom, const char* mailTo, const char* errorLogPath)
{
    WSAData wsData;
    if (::WSAStartup(WINSOCK_VERSION, &wsData))
    {
        return false;
    }

    SOCKET socket = ::socket(SOCK_DGRAM, AF_UNIX, IPPROTO_IP);
    if (socket == INVALID_SOCKET)
    {
        ::WSACleanup();
        return false;
    }

    int optval = 5000;
    int sockOptRes = ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&optval, sizeof(optval));
    if (sockOptRes == SOCKET_ERROR)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    optval = 5000;
    sockOptRes = ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&optval, sizeof(optval));
    if (sockOptRes == SOCKET_ERROR)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    sockaddr_in name;
    name.sin_family = SOCK_DGRAM;
    name.sin_addr.s_addr = 0;
    name.sin_port = 0;
    if (::bind(socket, (const sockaddr*)&name, sizeof(name)) == SOCKET_ERROR)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    name.sin_family = SOCK_DGRAM;

    if (ipAddress[0] < '0' || '9' < ipAddress[0])  // TODO: isalpha/isnum
    {
        hostent* host = ::gethostbyname(ipAddress);
        if (host == nullptr)
        {
            ::closesocket(socket);
            ::WSACleanup();
            return false;
        }
        memcpy(&name.sin_addr, *(const void**)host->h_addr_list, host->h_length);
    }
    else
    {
        name.sin_addr.s_addr = ::inet_addr(ipAddress);
    }

    name.sin_port = htons(25u);

    if (::connect(socket, (const sockaddr*)&name, sizeof(name)) == SOCKET_ERROR)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    char buf[0x800];
    int rcvResult = ::recv(socket, buf, sizeof(buf), 0);
    int headerLen = ::strlen("HELO main1.ncsoft.co.kr\r\n");
    if (::send(socket, "HELO main1.ncsoft.co.kr\r\n", headerLen, 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    rcvResult = ::recv(socket, buf, sizeof(buf), 0);

    char mailFromBuff[256];
    ::sprintf(mailFromBuff, "MAIL From: <%s>\r\n", mailFrom);
    int mailFromLen = ::strlen(mailFromBuff);
    if (::send(socket, mailFromBuff, mailFromLen, 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    rcvResult = ::recv(socket, buf, sizeof(buf), 0);
    while (*mailTo)
    {
        char mailToBuff[256];
        ::sprintf(mailToBuff, "RCPT To: <%s>\r\n", mailTo);
        int rcptLen = ::strlen(mailToBuff);
        if (::send(socket, mailToBuff, rcptLen, 0) <= 0)
        {
            ::closesocket(socket);
            ::WSACleanup();
            return false;
        }
        rcvResult = ::recv(socket, buf, sizeof(buf), 0);
        mailTo += ::strlen(mailTo) + 1;
    }

    const char data[] = "DATA\r\n";
    if (::send(socket, data, ::strlen(data), 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    rcvResult = ::recv(socket, buf, sizeof(buf), 0);
    int templateLen = ::strlen(CExceptionInit::s_mailTemplate);
    if (::send(socket, CExceptionInit::s_mailTemplate, templateLen, 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    if (!CExceptionInit::sendFile(socket, errorLogPath))
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    const char newLine[] = "\r\n.\r\n";
    if (::send(socket, newLine, ::strlen(newLine), 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    rcvResult = ::recv(socket, buf, sizeof(buf), 0);
    const char quit[] = "QUIT\r\n";
    if (::send(socket, quit, ::strlen(quit), 0) <= 0)
    {
        ::closesocket(socket);
        ::WSACleanup();
        return false;
    }

    rcvResult = ::recv(socket, buf, sizeof(buf), 0);

    ::closesocket(socket);
    ::WSACleanup();
    return true;
}

// 0x00416943
bool CExceptionInit::sendFile(SOCKET socket, const char* lpFileName)
{
    HANDLE hFile = ::CreateFileA(lpFileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (hFile == (HANDLE)INVALID_HANDLE_VALUE)
    {
        // return true;
        return false;  // FIXED
    }

    DWORD bytesRead;
    char buffer[1024];
    do
    {
        if ((::ReadFile(hFile, buffer, 0x400u, &bytesRead, 0) == FALSE) || (bytesRead == 0))
        {
            ::CloseHandle(hFile);
            // return true;
            return false;  // FIXED
        }
    } while (::send(socket, buffer, bytesRead, 0) > 0);

    // return false;
    return true;  // FIXED
}

// 0x00417BBE
void CExceptionInit::intentionalException()
{
    if (CExceptionInit::s_intentionException)
    {
        CExceptionInit::s_intentionException = false;
        CExceptionInit::logException("Intentional Exception\n");
    }
}

// 0x00417CA0
void CExceptionInit::enterCritSection()
{
    ::EnterCriticalSection(&CExceptionInit::g_criticalSection);
}

// 0x00417CC0
void CExceptionInit::leaveCritSection()
{
    ::LeaveCriticalSection(&CExceptionInit::g_criticalSection);
}

// 0x00417CE0
void CExceptionInit::initCritSection()
{
    ::InitializeCriticalSection(&CExceptionInit::g_criticalSection);
}

// 0x00417CF0
void CExceptionInit::deleteCritSection()
{
    ::DeleteCriticalSection(&CExceptionInit::g_criticalSection);
}
