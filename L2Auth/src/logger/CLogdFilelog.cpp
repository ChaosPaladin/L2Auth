#include "logger/CLogdFilelog.h"
#include "utils/Unused.h"

#include <cstdio>
#include <ctime>

CLogdFilelog g_logdfilelog("log");

// 0x0042E261
CLogdFilelog::CLogdFilelog(const char* extension)
    : CFileLog(extension)
    , m_halfAnHour(false)
{
    time_t timeNow = std::time(0);
    tm* now = std::localtime(&timeNow);
    m_halfAnHour = (now->tm_min >= 30);
}

// 0x0042E2C4
CLogdFilelog::~CLogdFilelog()
{
}

// 0x0042E2E0
void CLogdFilelog::AddLog(int logType, const char* format, ...)
{
    UNUSED(logType);

    va_list va;
    va_start(va, format);
    time_t timeNow = std::time(0);
    tm* now = std::localtime(&timeNow);

    bool isSecondHalfOfHour = now->tm_min >= 30;

    ::EnterCriticalSection(&m_critSection);

    char newFileName[256];
    if (now->tm_mday != m_dayOfMonth || now->tm_hour != m_hour || isSecondHalfOfHour != m_halfAnHour)
    {
        sprintf(newFileName, "%s\\%04d-%02d-%02d-100%02d-201-authd-in%d.%s", m_dirName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, isSecondHalfOfHour, m_extension);

        HANDLE newFileHandler = ::CreateFileA(newFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE oldFileHandler = (HANDLE)::InterlockedExchange((volatile LONG*)&m_fileHandler, (LONG)newFileHandler);  // TODO: x64 problem
        if (oldFileHandler && oldFileHandler != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(oldFileHandler);
        }

        m_dayOfMonth = now->tm_mday;
        m_hour = now->tm_hour;
        m_halfAnHour = isSecondHalfOfHour;
    }
    ::LeaveCriticalSection(&m_critSection);

    char buffer[1024];
    int result = vsprintf(buffer, format, va);
    if (result > 0 && m_fileHandler != INVALID_HANDLE_VALUE)
    {
        size_t buffLen = strlen(buffer);
        DWORD NumberOfBytesWritten;
        ::WriteFile(m_fileHandler, buffer, buffLen, &NumberOfBytesWritten, 0);
    }
}

// 0x0042E49E
void CLogdFilelog::SetDirectory(const char* aDirName)
{
    char newFileName[256];

    time_t now_time = std::time(0);
    tm* now = std::localtime(&now_time);
    strcpy(m_dirName, aDirName);

    if (now->tm_min >= 30)
    {
        sprintf(newFileName, "%s\\%04d-%02d-%02d-100%02d-201-authd-in1.%s", m_dirName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, m_extension);
        m_halfAnHour = true;
    }
    else
    {
        sprintf(newFileName, "%s\\%04d-%02d-%02d-100%02d-201-authd-in0.%s", m_dirName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, m_extension);
        m_halfAnHour = false;
    }
    m_fileHandler = ::CreateFileA(newFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fileHandler != (HANDLE)-1)
    {
        ::SetFilePointer(m_fileHandler, 0, 0, FILE_END);
    }

    m_dayOfMonth = now->tm_mday;
    m_hour = now->tm_hour;
}
