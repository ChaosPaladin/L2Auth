#pragma once

#include "logger/LogSeverity.h"

#include <windows.h>

class CFileLog
{
public:
    CFileLog(const char* fileExtension);                          // 0x0042DF47
    virtual ~CFileLog();                                          // 0x0042DF8F
    void AddLog(LOG_SEVERITY severity, const char* format, ...);  // 0x0042E0DD

    void SetDirectory(const char* aDirName);  // 0x0042DFC6

protected:
    CRITICAL_SECTION m_critSection;
    HANDLE m_fileHandler;
    int m_dayOfMonth;
    int m_hour;
    char m_extension[8];
    char m_dirName[256];
};

extern CFileLog g_fileLog;
extern CFileLog g_actionLog;
extern CFileLog g_errLog;
