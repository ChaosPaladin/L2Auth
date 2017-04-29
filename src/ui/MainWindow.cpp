#include "ui/MainWindow.h"
#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "ui/resources.h"

#include "AppInstance.h"

#include "config/Config.h"

#include "model/AccountDB.h"
#include "model/CServerList.h"
#include "model/ServerKind.h"
#include "model/ServersProvider.h"

#include "threads/CJob.h"
#include "threads/Threading.h"

#include "network/WorldSrvServer.h"

#include "utils/CExceptionInit.h"
#include "utils/Utils.h"

HWND MainWindow::mainWnd;
HWND MainWindow::reporterWnd;
HWND MainWindow::logWnd;
HWND MainWindow::resetBtn;
HWND MainWindow::reloadBtn;

BOOL CALLBACK MainWindow::closeAuthFunc(HWND hDlg, UINT command, WPARAM param, LPARAM)
{
    if (command == WM_COMMAND)
    {
        if (param == IDOK)
        {
            ::DestroyWindow(mainWnd);
            ::EndDialog(hDlg, 0);
        }
        else if (param == IDCANCEL)
        {
            ::EndDialog(hDlg, 0);
        }
    }
    return 0;
}

LRESULT CALLBACK MainWindow::wndProc(HWND hWndParent, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    auth_guard;

    if (Msg > 0x10)
    {
        if (Msg == WM_COMMAND)
        {
            short cmd = (short)wParam;
            if (!(wParam >> 16) && cmd != 1 && cmd == 2)
            {
                g_serverList.ReloadServer();
                g_serverKind.ReloadServerList();
            }
        }
        else if (Msg == WM_TIMER)
        {
            switch (wParam)
            {
                case WinParam_UpdateUserNumber:
                    g_reporter.users = g_accountDb.GetUserNum();
                    ::InvalidateRect(reporterWnd, 0, 0);
                    break;
                case WinParam_UpdateServerStatus:
                    for (int serverId = 1;; ++serverId)
                    {
                        int serverNumber = g_serverList.GetMaxServerNum();
                        if (serverId > serverNumber)
                        {
                            break;
                        }
                        if (ServersProvider::GetServerStatus(serverId))
                        {
                            // WorldServer worldServer = g_serverList.getServerInfo(serverId); //
                            // FIXED
                            WorldServer worldServer = ServersProvider::GetWorldServer(serverId);
                            WorldSrvServer::SendSocket(worldServer.ipAddress, "c", 2);  // ping
                        }
                    }
                    break;
                case WinParam_UpdateDisckFreeSpace:
                    Utils::CheckDiskSpace(g_Config.logDirectory, 400000000);
                    break;
            }
        }
    }
    else
    {
        switch (Msg)
        {
            case WM_CLOSE:
                if (hWndParent == MainWindow::mainWnd)
                {
                    HWND closeAuth = ::CreateDialogParamA(hInstance, MAKEINTRESOURCE(DLG_CLOSE_L2_AUTH), hWndParent, &closeAuthFunc, 0);
                    ::ShowWindow(closeAuth, SW_SHOW);
                    return 0;
                }
                break;
            case WM_DESTROY:
                if (hWndParent == MainWindow::mainWnd)
                {
                    Threading::g_bTerminating = true;
                    g_winlog.Enable(0);
                    g_job.SetTerminate();
                    ::Sleep(2000u);
                    ::PostQuitMessage(0);
                }
                break;
            case WM_SIZE:
            {
                short width = (short)lParam;
                short height = (unsigned int)(lParam >> 16);

                if (hWndParent == MainWindow::mainWnd)
                {
                    ::MoveWindow(reporterWnd, 160, 0, width - 160, 20, 1);
                    ::MoveWindow(logWnd, 0, 20, width, height - 20, 1);
                    ::MoveWindow(resetBtn, 0, 0, 80, 20, 1);
                    ::MoveWindow(reloadBtn, 80, 0, 80, 20, 1);
                }
                else if (hWndParent == logWnd)
                {
                    g_winlog.Resize(width, height);
                }
                else
                {
                    hWndParent = reporterWnd;
                    if (reporterWnd)
                    {
                        g_reporter.Resize(width, height);
                    }
                }
                break;
            }
            case WM_PAINT:
                if (hWndParent == logWnd)
                {
                    g_winlog.Redraw();
                }
                else if (hWndParent == reporterWnd)
                {
                    g_reporter.Redraw();
                }
                break;
        }
    }

    auth_vunguard;

    return ::DefWindowProcA(hWndParent, Msg, wParam, lParam);
}
