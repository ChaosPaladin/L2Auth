#include "ui/CReporter.h"
#include <stdio.h>

LONG CReporter::g_AcceptExThread = 0;
volatile LONG CReporter::g_nRunningThread = 0;

CReporter g_reporter;

// 0x0043275F
CReporter::CReporter()
    : sockets(0)
    , loggedUsers(0)
    , users(0)
    , m_statusLineWindow()
    , m_font(0)
    , m_statusLineRect()
    , m_brush()
    , m_textSize()
    , m_systemTime()
{
    ::GetLocalTime(&m_systemTime);
}

// 0x004327B5
CReporter::~CReporter()
{
}

// 0x004327C9
void CReporter::SetWnd(HWND wnd)
{
    m_statusLineWindow = wnd;
}

// 0x00432819
void CReporter::Redraw()
{
    tagPAINTSTRUCT paint;
    HDC hdc = ::BeginPaint(m_statusLineWindow, &paint);
    if (m_font == 0)
    {
        m_font = ::GetStockObject(16);
        ::SelectObject(hdc, m_font);
        ::GetClientRect(m_statusLineWindow, &m_statusLineRect);
        m_brush = (HBRUSH)::GetStockObject(0);
        ::GetTextExtentPoint32A(hdc, "H", 1, (LPSIZE)&m_textSize);
    }
    else
    {
        ::SelectObject(hdc, m_font);
    }

    ::FillRect(hdc, &m_statusLineRect, m_brush);

    char statusText[128];
    sprintf(statusText, " user %-6d  | socket %-4d | AcceptCall %-3d  | RunningThread %-3d ", users, sockets, g_AcceptExThread, g_nRunningThread);
    ::SetTextColor(hdc, 0);
    ::TextOutA(hdc, 0, 0, statusText, ::strlen(statusText));
    ::EndPaint(m_statusLineWindow, &paint);
}

void CReporter::Reset()
{
    ::GetLocalTime(&m_systemTime);
    ::InvalidateRect(m_statusLineWindow, 0, false);
}

// 0x0043297A
void CReporter::Resize(int width, int height)
{
    m_statusLineRect.right = width;
    m_statusLineRect.bottom = height;
    ::InvalidateRect(m_statusLineWindow, 0, 0);
}
