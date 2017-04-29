#pragma once

#include <windows.h>

class CReporter
{
public:
    CReporter();           // 0x0043275F
    virtual ~CReporter();  // 0x004327B5

    void SetWnd(HWND wnd);               // 0x004327C9
    void Redraw();                       // 0x00432819
    void Resize(int width, int height);  // 0x0043297A
    void Reset();                        // 0x004327F0

    static LONG g_AcceptExThread;
    static volatile LONG g_nRunningThread;

public:
    volatile LONG sockets;
    volatile LONG loggedUsers;
    int users;

private:
    HWND m_statusLineWindow;
    HGDIOBJ m_font;
    RECT m_statusLineRect;
    HBRUSH m_brush;
    int m_textSize;
    SYSTEMTIME m_systemTime;
};

extern CReporter g_reporter;
