#pragma once

#include "logger/CFileLog.h"

class CLogdFilelog : public CFileLog
{
public:
    CLogdFilelog(const char* m_extension);  // 0x0042E261
    ~CLogdFilelog();                        // 0x0042E2C4

    void AddLog(int logType, const char* format, ...);  // 0x0042E2E0
    void SetDirectory(const char* aDirName);            // 0x0042E49E

private:
    bool m_halfAnHour;
};

extern CLogdFilelog g_logdfilelog;
