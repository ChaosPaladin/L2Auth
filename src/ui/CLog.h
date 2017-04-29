#pragma once

#include "logger/LogSeverity.h"

#include <stdio.h>
#include <windows.h>

class CLog
{
public:
    CLog(int bufferLength, const char* m_extension);  // 0x0042D920
    virtual ~CLog();                                  // 0x0042D8D0

    void Redraw();                                                // 0x0042D9D3
    void Resize(int right, int botton);                           // 0x0042DB8D
    void Enable(bool enabled);                                    // 0x0042DBC9
    void AddLog(LOG_SEVERITY severity, const char* format, ...);  // 0x0042DBFC
    void SetDirectory(const char* fileName);                      // 0x0042DE5D

    void SetWnd(HWND wnd);  // 0x0042D9BD

private:
    struct LogEntry;

private:
    HWND m_window;
    bool m_enabled;
    const int m_bufferLength;
    int m_currentLine;
    LogEntry* m_outputBuffer;
    CRITICAL_SECTION m_criticalSection;
    RECT m_loginWndRect;
    int m_textSize;
    int m_lineHeight;
    int m_font;
    int m_brush;
    FILE* m_logFile;
    int m_dayOfMonth;
    int m_hour;
    char m_extension[8];
    char m_fileName[MAX_PATH];
};

extern CLog g_winlog;
