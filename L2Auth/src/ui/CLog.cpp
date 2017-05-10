#include "ui/CLog.h"

#include <ctime>

CLog g_winlog(128, "winlog");

struct CLog::LogEntry
{
    LogType logType;
    char log[512];
    int logSize;
};

// 0x0042D920
CLog::CLog(int bufferLength, const char* ext)
    : m_window(nullptr)
    , m_enabled(false)
    , m_bufferLength(bufferLength)
    , m_currentLine(0)
    , m_outputBuffer(new LogEntry[bufferLength])
    , m_criticalSection()
    , m_loginWndRect()
    , m_textSize()
    , m_lineHeight()
    , m_font(0)
    , m_brush()
    , m_logFile()
    , m_dayOfMonth()
    , m_hour()
{
    memset(m_outputBuffer, 0, sizeof(LogEntry) * bufferLength);
    ::InitializeCriticalSectionAndSpinCount(&m_criticalSection, 4000u);
    strcpy(m_extension, ext);
}

// 0x0042D8D0
CLog::~CLog()
{
    delete[] m_outputBuffer;
    ::DeleteCriticalSection(&m_criticalSection);
    if (m_logFile)
    {
        fclose(m_logFile);
    }
}

// 0x0042D9D3
void CLog::Redraw()
{
    static const COLORREF color[] = {0x0, 0x80, 0xFF0000, 0xFF};

    tagPAINTSTRUCT painter;
    if (m_enabled)
    {
        HDC hdc = ::BeginPaint(m_window, &painter);
        if (m_font)
        {
            ::SelectObject(hdc, (HGDIOBJ)m_font);
        }
        else
        {
            m_font = (int)::GetStockObject(SYSTEM_FIXED_FONT);
            ::SelectObject(hdc, (HGDIOBJ)m_font);
            ::GetClientRect(m_window, &m_loginWndRect);
            m_brush = (int)::GetStockObject(WHITE_BRUSH);
            ::GetTextExtentPoint32A(hdc, "H", 1, (LPSIZE)&m_textSize);
        }
        int y = m_loginWndRect.bottom;
        int curLine = m_currentLine;
        do
        {
            y -= m_lineHeight;
            curLine = (m_bufferLength - 1) & (curLine - 1);
            if (&m_outputBuffer[curLine] != (LogEntry*)-4)
            {
                ::SetTextColor(hdc, color[m_outputBuffer[curLine].logType]);
                ::TextOutA(hdc, 0, y, m_outputBuffer[curLine].log, m_outputBuffer[curLine].logSize);
            }
            RECT rc;
            rc.left = m_textSize * m_outputBuffer[curLine].logSize;
            rc.right = m_loginWndRect.right;
            rc.top = y;
            rc.bottom = m_lineHeight + y;
            ::FillRect(hdc, &rc, (HBRUSH)m_brush);
        } while (y >= 0);
        ::EndPaint(m_window, &painter);
    }
}

// 0x0042DB8D
void CLog::Resize(int right, int botton)
{
    m_loginWndRect.right = right;
    m_loginWndRect.bottom = botton;
    if (m_enabled)
    {
        ::InvalidateRect(m_window, 0, 0);
    }
}

// 0x0042DBC9
void CLog::Enable(bool enabled)
{
    m_enabled = enabled;
    if (m_enabled)
    {
        ::InvalidateRect(m_window, 0, 0);
    }
}

// 0x0042DBFC
void CLog::AddLog(LogType log_type, const char* format, ...)
{
    char buffer[256];
    char logString[1020];
    int length;
    va_list va;

    va_start(va, format);
    if (m_enabled)
    {
        time_t now = std::time(0);
        struct tm* time_now = std::localtime(&now);
        if ((time_now->tm_mday != m_dayOfMonth || time_now->tm_hour != m_hour) && m_logFile)
        {
            fclose(m_logFile);
            sprintf(buffer, "%s\\%04d-%02d-%02d.%02d.%s", m_fileName, time_now->tm_year + 1900, time_now->tm_mon + 1, time_now->tm_mday, time_now->tm_hour, m_extension);
            m_logFile = fopen(buffer, "a");
            m_dayOfMonth = time_now->tm_mday;
            m_hour = time_now->tm_hour;
        }

        sprintf(logString, "%02d.%02d.%02d %02d:%02d.%02d: ", time_now->tm_year + 1900, time_now->tm_mon + 1, time_now->tm_mday, time_now->tm_hour, time_now->tm_min, time_now->tm_sec);
        length = vsprintf(&logString[20], format, va);

        if ((signed int)length > 0)
        {
            length += 20;
            if ((unsigned int)length > 510)
                length = 510;  // 127*sizeof(char*)==510

            ::EnterCriticalSection(&m_criticalSection);
            m_outputBuffer[m_currentLine].logType = log_type;
            memcpy(m_outputBuffer[m_currentLine].log, logString, length + 1);
            m_outputBuffer[m_currentLine++].logSize = length;
            m_currentLine &= m_bufferLength - 1;
            ::LeaveCriticalSection(&m_criticalSection);
            if (m_logFile)
            {
                fprintf(m_logFile, "%s\r\n", logString);
            }
            if (m_enabled)
            {
                ::InvalidateRect(m_window, 0, 0);
            }
        }
    }
}

// 0x0042DE5D
void CLog::SetDirectory(const char* fileName)
{
    char buffer[256];
    time_t time_now = std::time(0);
    tm* now = std::localtime(&time_now);

    strcpy(m_fileName, fileName);
    sprintf(buffer, "%s\\%04d-%02d-%02d.%02d.%s", m_fileName, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, m_extension);

    m_logFile = fopen(buffer, "a");
    m_dayOfMonth = now->tm_mday;
    m_hour = now->tm_hour;
}

void CLog::SetWnd(HWND wnd)
{
    m_window = wnd;
}
