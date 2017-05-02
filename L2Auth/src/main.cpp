#pragma once

#include <winsock2.h>

#include <windows.h>

#include "AppInstance.h"
#include "config/CIPList.h"
#include "config/Config.h"
#include "crypt/DesFrontEnd.h"
#include "crypt/DesNewCrypt.h"
#include "crypt/Blowfish.h"
#include "crypt/PwdCrypt.h"
#include "db/CAccount.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "logger/CFileLog.h"
#include "logger/CLogdFilelog.h"
#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "ui/MainWindow.h"
#include "ui/resources.h"

#include "model/CServerList.h"
#include "model/ServerKind.h"

#include "threads/Threading.h"

#include "network/CLogSocket.h"
#include "network/CWantedSocket.h"
#include "network/IPSocket.h"
#include "utils/CExceptionInit.h"
#include "utils/SendMail.h"

#include <streambuf>
#include <process.h>  // _beginthreadex

HINSTANCE hInstance;

int g_buildNumber = 0x9E38;

int __stdcall WinMain(HINSTANCE instance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nShowCmd)  // 0x0042C965
{
    HWND mainWndHandler = ::FindWindowA(0, "AuthServer");  // TODO: add build version, and then find
    if (mainWndHandler)
    {
        ::MessageBoxA(0, "", "", MB_ICONSTOP);
        exit(0);
    }

#if 0  // two most popular BF keys in patchet clients
    const uint8_t bfKey[] = {0x5F, 0x3B, 0x35, 0x2E, 0x5D, 0x39, 0x34, 0x2D, 0x33, 0x31, 0x3D, 0x3D, 0x2D, 0x25, 0x78, 0x54, 0x21, 0x5E, 0x5B, 0x24, 0x0};
#else
    const uint8_t bfKey[] = {0x5B, 0x3B, 0x27, 0x2E, 0x5D, 0x39, 0x34, 0x2D, 0x33, 0x31, 0x3D, 0x3D, 0x2D, 0x25, 0x26, 0x40, 0x21, 0x5E, 0x2B, 0x5D, 0x0};
#endif
    Blowfish::Initialize(bfKey, sizeof(bfKey));

    // 9=3*2;x]11sa;/4sd.%+$@a*'!2z0~g`{z]?n8(\"
    const char desKey[] = {0x39, 0x3d, 0x33, 0x2a, 0x32, 0x3b, 0x78, 0x5d, 0x31, 0x31, 0x73, 0x61, 0x3b, 0x2f, 0x34, 0x73, 0x64, 0x2e, 0x25, 0x2b, 0x24, 0x40, 0x61, 0x2a, 0x27, 0x21, 0x32, 0x7a, 0x30, 0x7e, 0x67, 0x60, 0x7b, 0x7a, 0x5d, 0x3f, 0x6e, 0x38, 0x28, 0x5c, 0x22};
    g_PwdCrypt.DesKeyInit(desKey);

    WNDCLASSEXA wndClass;
    wndClass.cbSize = 48;
    wndClass.style = 64;
    wndClass.lpfnWndProc = MainWindow::wndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = 0;  //::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_16x16));
    wndClass.hCursor = ::LoadCursorA(0, IDC_ARROW);
    wndClass.hbrBackground = 0;
    wndClass.lpszMenuName = 0;
    wndClass.lpszClassName = "AuthServer";
    wndClass.hIconSm = 0;
    ATOM className = ::RegisterClassExA(&wndClass);
    ::hInstance = instance;

    CExceptionInit::Init();

    WSAData wsaData;
    int wsaStartupResult = ::WSAStartup(WINSOCK_VERSION, &wsaData);
    if (wsaStartupResult != 0)
    {
        g_winlog.AddLog(LOG_ERR, "WSAStartup error 0x%x", wsaStartupResult);
        return 0;
    }

    char mainWindowTitle[200];
    sprintf(mainWindowTitle, "AuthServer-%d", g_buildNumber);
    MainWindow::mainWnd = ::CreateWindowExA(
        0,
        (LPCSTR)className,
        mainWindowTitle,
        WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_GROUP | WS_TABSTOP,  //   0x00CF0000u,
        0x80000000,                                                       // x
        0,                                                                // y
        600,                                                              // width
        440,                                                              // height
        0,                                                                // wndParent
        0,                                                                // menu
        hInstance,                                                        //
        0);

    MainWindow::logWnd = ::CreateWindowExA(
        WS_EX_CLIENTEDGE,  // 0x200u,
        (LPCSTR)className,
        "",
        WS_CHILD,  // 0x40000000u,
        0,
        30,
        640,
        720,
        MainWindow::mainWnd,
        0,
        hInstance,
        0);

    MainWindow::reporterWnd = ::CreateWindowExA(
        WS_EX_CLIENTEDGE,
        (LPCSTR)className,
        "",
        WS_CHILD,  // 0x40000000u,
        0,
        0,
        640,
        30,
        MainWindow::mainWnd,
        0,
        hInstance,
        0);

    MainWindow::resetBtn = ::CreateWindowExA(0, "BUTTON", "Reset", WS_CHILD, 600, 0, 40, 30, MainWindow::mainWnd, (HMENU)1, hInstance, 0);
    MainWindow::reloadBtn = ::CreateWindowExA(0, "BUTTON", "Reload", WS_CHILD, 600, 0, 40, 30, MainWindow::mainWnd, (HMENU)2, hInstance, 0);

    g_winlog.SetWnd(MainWindow::logWnd);
    g_reporter.SetWnd(MainWindow::reporterWnd);
    ::SetProcessPriorityBoost(::GetCurrentProcess(), 1);
    ::ShowWindow(MainWindow::mainWnd, nShowCmd);
    ::UpdateWindow(MainWindow::mainWnd);
    ::ShowWindow(MainWindow::logWnd, SW_SHOW);
    ::UpdateWindow(MainWindow::logWnd);
    ::ShowWindow(MainWindow::reporterWnd, SW_SHOW);
    ::UpdateWindow(MainWindow::reporterWnd);
    ::ShowWindow(MainWindow::resetBtn, SW_SHOW);
    ::UpdateWindow(MainWindow::resetBtn);
    ::ShowWindow(MainWindow::reloadBtn, SW_SHOW);
    ::UpdateWindow(MainWindow::reloadBtn);

    g_Config.Load("etc/config.txt");

    if (g_Config.desKey)
    {
        char desKey[0x400];
        memset(desKey, 0, sizeof(desKey));
        strcpy(desKey, g_Config.desKey);
        DesFrontEnd::DesKeyInit(desKey);
    }
    else
    {
        DesFrontEnd::DesKeyInit("TEST");
    }

    HANDLE hObject = 0;
    if (strlen(g_Config.logDirectory))
    {
        g_winlog.SetDirectory(g_Config.logDirectory);
        g_winlog.Enable(1);
        g_fileLog.SetDirectory(g_Config.logDirectory);
        g_actionLog.SetDirectory(g_Config.logDirectory);
        g_errLog.SetDirectory(g_Config.logDirectory);
        g_logdfilelog.SetDirectory(g_Config.logDirectory);
        ::SetTimer(MainWindow::mainWnd, WinParam_UpdateUserNumber, 2000u, 0);
        ::SetTimer(MainWindow::mainWnd, WinParam_UpdateServerStatus, 60000u, 0);

        switch (g_Config.gameID)
        {
            case 0x04:
                CAccount::g_pwdCrypt = &PwdCrypt::EncPwdShalo;
                break;
            case 0x08:
            case 0x10:
            case 0x20:
                CAccount::g_pwdCrypt = &PwdCrypt::EncPwdL2;
                break;
            default:
                CAccount::g_pwdCrypt = &PwdCrypt::EncPwdDev;
                break;
        }

        g_winlog.AddLog(LOG_DBG, "LOADED Config");
        g_winlog.AddLog(LOG_INF, "BuildNumber : %d", g_buildNumber);

        if (g_Config.acceptCallNum == 0)
        {
            g_winlog.AddLog(LOG_ERR, "AcceptCallNull 이 0 이면 어떤 클라이언트도 접속이 안됩니다. 1 로 자동세팅합니다.");
            g_Config.acceptCallNum = 1;
        }

        if (g_Config.socketTimeOut == 0)
        {
            g_winlog.AddLog(LOG_ERR, "SocketTimeOut이 0이면 Connection이 바로 끊기게 됩니다. 180으로 자동세팅합니다.");
            g_Config.socketTimeOut = 180;
        }

        if (g_Config.waitingUserLimit == 0)
        {
            g_winlog.AddLog(
                LOG_ERR,
                "WaitingUserLimit 가 0이면 Connection이 이루어지지 않습니다. "
                "100으로 자동세팅합니다.");
            g_Config.waitingUserLimit = 100;
        }

        if (g_Config.useForbiddenIPList)
        {
            if (g_Config.countryCode)
            {
                g_winlog.AddLog(LOG_INF, "LOAD FORBIDDEN IP LIST");
            }
            else
            {
                g_winlog.AddLog(LOG_INF, "횁짖짹횢 占쏙옙占쏙옙 IP 占쏙옙占쏙옙占 읽어 들입니다.");
            }

            g_blockedIPs.Load("etc\\BlockIPs.txt");
        }

        if ((g_Config.mailServer != NULL) && (::strlen(g_Config.mailServer) != 0))
        {
            if (g_MailService.Init())
            {
                ::SetTimer(MainWindow::mainWnd, WinParam_UpdateDisckFreeSpace, 7200000u, 0);
            }
            else
            {
                g_winlog.AddLog(LOG_ERR, "Sendmail initialize Fail");
            }
        }

        g_linDB.Init(g_Config.DBConnectionNum);
        g_serverList.ReloadServer();
        CDBConn sqlQuery(&g_linDB);
        sqlQuery.Execute("update worldstatus set status=0");

        Threading::CreateIOThread();

        if (g_Config.useLogD)
        {
            SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            sockaddr_in name;
            name.sin_family = AF_INET;
            name.sin_addr = g_Config.logDIP;
            name.sin_port = ::htons(g_Config.logDPort);

            int result = ::connect(socket, (const sockaddr*)&name, sizeof(sockaddr_in));

            g_LogDSocket = CLogSocket::Allocate(socket);
            g_LogDSocket->SetAddress(g_Config.logDIP);
            if (result == SOCKET_ERROR)
            {
                g_LogDSocket->CloseSocket();
            }
            else
            {
                g_LogDSocket->Initialize(Threading::g_hCompletionPortExtra);
            }
        }

        if (g_Config.useIPServer)
        {
            SOCKET ipSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            sockaddr_in ipSocketName;
            ipSocketName.sin_family = AF_INET;
            ipSocketName.sin_addr = g_Config.IPServer;
            ipSocketName.sin_port = ::htons(g_Config.IPPort);

            int result = ::connect(ipSocket, (const sockaddr*)&ipSocketName, sizeof(sockaddr_in));
            g_IPSocket = new IPSocket(ipSocket);
            g_IPSocket->SetAddress(g_Config.IPServer);
            if (result == SOCKET_ERROR)
            {
                g_IPSocket->CloseSocket();
            }
            else
            {
                g_IPSocket->Initialize(Threading::g_hCompletionPort);
            }
        }

        if (g_Config.useWantedSystem)
        {
            SOCKET wantedSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            sockaddr_in wantedName;
            wantedName.sin_family = AF_INET;
            wantedName.sin_addr = g_Config.wantedIP;
            wantedName.sin_port = ::htons(g_Config.wantedPort);

            int result = ::connect(wantedSocket, (const sockaddr*)&wantedName, sizeof(sockaddr_in));
            g_SocketWanted = new CWantedSocket(wantedSocket);
            g_SocketWanted->SetAddress(g_Config.wantedIP);
            if (result == SOCKET_ERROR)
            {
                g_SocketWanted->CloseSocket();
            }
            else
            {
                g_SocketWanted->Initialize(Threading::g_hCompletionPortExtra);
            }
        }
        unsigned int threadId;
        hObject = (HANDLE)_beginthreadex(NULL, 0, &Threading::ListenThread, (void*)0, 0, &threadId);
        g_serverKind.LoadServerList();
    }
    else
    {
        g_winlog.SetDirectory("log");
        g_winlog.Enable(1);
        g_fileLog.SetDirectory(g_Config.logDirectory);
        g_actionLog.SetDirectory(g_Config.logDirectory);
        g_errLog.SetDirectory(g_Config.logDirectory);
        g_winlog.AddLog(LOG_ERR, "Error load config.txt");
        g_logdfilelog.SetDirectory(g_Config.logDirectory);
    }

    tagMSG Msg;
    // If wMsgFilterMin and wMsgFilterMax are both zero, GetMessage returns all available messages
    while (::GetMessageA(&Msg, 0, 0, 0))
    {
        ::TranslateMessage(&Msg);
        ::DispatchMessageA(&Msg);
    }

    if (hObject)
    {
        ::CloseHandle(hObject);
    }

    while (!Threading::g_teminateEvent)
    {
        ::Sleep(1000u);
    }

    ::Sleep(2000u);
    ::WSACleanup();

    return 0;
}
