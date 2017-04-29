#pragma once

#include <windows.h>

enum WinProcParam
{
    WinParam_UpdateUserNumber = 0x66,
    WinParam_UpdateServerStatus = 0x67,
    WinParam_UpdateDisckFreeSpace = 0x6E
};

class MainWindow
{
public:
    static HWND mainWnd;
    static HWND reporterWnd;
    static HWND logWnd;
    static HWND resetBtn;
    static HWND reloadBtn;

    static LRESULT CALLBACK wndProc(HWND hWndParent, UINT Msg, WPARAM wParam, LPARAM lParam);

private:
    static BOOL CALLBACK closeAuthFunc(HWND hDlg, UINT command, WPARAM param, LPARAM);
};
