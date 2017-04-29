#pragma once

#include <windows.h>

class Threading
{
public:
    static void CreateIOThread();                               // 0x0043A4A3
    static unsigned int __stdcall ListenThread(void* arglist);  // 0x0043A2F1

public:
    static bool g_teminateEvent;
    static HANDLE g_hCompletionPort;
    static HANDLE g_hCompletionPortExtra;
    static bool g_bTerminating;

private:
    static unsigned int __stdcall TimerThread(void* arglist);     // 0x0043A0C0
    static unsigned int __stdcall IOThreadServer(void* arglist);  // 0x0043A0E7
    static unsigned int __stdcall IOThreadInt(void* arglist);     // 0x0043A201
};
