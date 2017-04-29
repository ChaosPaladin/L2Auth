#pragma once

#include <ctime>
#include <windows.h>

class CExceptionInit
{
public:
    CExceptionInit();   // 0x004176C1
    ~CExceptionInit();  // 0x00417788

    static LONG exception_filter(void* ex)
    {
        return exception_filter((_EXCEPTION_POINTERS*)(ex));
    }
    static LONG exception_filter(_EXCEPTION_POINTERS* ex);  // 0x00417A31
    static void logException(const char* format, ...);      // 0x0041779D
    static void sendExceptionLog(bool fatal);               // 0x00417AB1
    static void Init();                                     // 0x00417B55

private:
    static void SetUpExceptionFilter();                                                                               // 0x00417692
    static void ClearExceptionFilter();                                                                               // 0x004176A7
    static LONG WINAPI RecordExceptionInfo(_EXCEPTION_POINTERS* exceptionInfo);                                       // 0x00416386
    static void intentionalException();                                                                               // 0x00417BBE
    static bool sendFile(SOCKET socket, const char* lpFileName);                                                      // 0x00416943
    static bool sendMail(const char* ipAddress, const char* mailFrom, const char* mailTo, const char* errorLogPath);  // 0x00416486

    static void enterCritSection();   // 0x00417CA0
    static void leaveCritSection();   // 0x00417CC0
    static void deleteCritSection();  // 0x00417CF0
    static void initCritSection();    // 0x00417CE0

public:
    static char s_logPath[MAX_PATH];
    static time_t s_startTime;
    static char* s_mailServerIP;
    static bool s_intentionException;
    static bool s_errorReported;

private:
    static LPTOP_LEVEL_EXCEPTION_FILTER s_oldFilter;
    static CRITICAL_SECTION g_criticalSection;
    static bool s_beenHere;
    static char* s_mailTemplate;
};

extern CExceptionInit g_exceptionHandler;

#define auth_guard                                                                                                                                                                                                                                                                                                             \
    __try                                                                                                                                                                                                                                                                                                                      \
    {

#define auth_vunguard                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                          \
    __except (CExceptionInit::exception_filter(_exception_info()))                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
        CExceptionInit::logException("*%s:%d(%s) exception\n", __FILE__, __LINE__, __TIMESTAMP__);                                                                                                                                                                                                                             \
        CExceptionInit::sendExceptionLog(true);                                                                                                                                                                                                                                                                                \
    }

#define auth_unguard                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                          \
    __except (CExceptionInit::exception_filter(_exception_info()))                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
        CExceptionInit::logException("*%s:%d(%s) exception\n", __FILE__, __LINE__, __TIMESTAMP__);                                                                                                                                                                                                                             \
        CExceptionInit::sendExceptionLog(true);                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                          \
    return {};
