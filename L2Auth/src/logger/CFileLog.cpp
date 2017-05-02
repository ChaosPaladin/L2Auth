#include "logger/CFileLog.h"
#include "utils/Unused.h"

#include <cstdio>
#include <ctime>

CFileLog g_fileLog("use");
CFileLog g_actionLog("act");
CFileLog g_errLog("err");

// 0x0042DF47
CFileLog::CFileLog(const char* fileExtension)
{
    ::InitializeCriticalSectionAndSpinCount(&m_critSection, 4000u);
    m_fileHandler = (HANDLE)-1;
    strcpy(m_extension, fileExtension);
}

// 0x0042DF8F
CFileLog::~CFileLog()
{
    ::DeleteCriticalSection(&m_critSection);
    if (m_fileHandler != (HANDLE)-1)
    {
        ::CloseHandle(m_fileHandler);
    }
}

// 0x0042E0DD
void CFileLog::AddLog(LOG_SEVERITY severity, const char* format, ...)
{
    UNUSED(severity);

    va_list va;
    va_start(va, format);
    time_t timeNow = std::time(0);
    struct tm* now = std::localtime(&timeNow);
    ::EnterCriticalSection(&m_critSection);
    if ((now->tm_mday != m_dayOfMonth || now->tm_hour != m_hour) && (m_fileHandler != (HANDLE)-1))
    {
        char newFileName[256];
        sprintf(newFileName, "%s\\%04d-%02d-%02d.%02d.%s", m_dirName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, m_extension);

        HANDLE newFileHandler = ::CreateFileA(newFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        HANDLE oldFileHandler = (HANDLE)::InterlockedExchange((volatile LONG*)&m_fileHandler, (LONG)newFileHandler);  // TODO: x64 unsafe
        if (oldFileHandler)
        {
            ::CloseHandle(oldFileHandler);
        }
        m_dayOfMonth = now->tm_mday;
        m_hour = now->tm_hour;
    }
    ::LeaveCriticalSection(&m_critSection);

    char buffer[1024];
    int result = vsprintf(buffer, format, va);
    if (result > 0 && m_fileHandler != INVALID_HANDLE_VALUE)
    {
        size_t buffLen = strlen(buffer);
        DWORD bytesWritten;
        ::WriteFile(m_fileHandler, buffer, buffLen, &bytesWritten, NULL);
    }
}

// 0x0042DFC6
void CFileLog::SetDirectory(const char* aDirName)
{
    time_t time_now = std::time(0);
    struct tm* now = std::localtime(&time_now);
    strcpy(m_dirName, aDirName);

    char newFileName[256];
    sprintf(newFileName, "%s\\%04d-%02d-%02d.%02d.%s", m_dirName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, m_extension);
    m_fileHandler = ::CreateFileA(newFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fileHandler != INVALID_HANDLE_VALUE)
    {
        ::SetFilePointer(m_fileHandler, 0, 0, FILE_END);
    }

    m_dayOfMonth = now->tm_mday;
    m_hour = now->tm_hour;
}
