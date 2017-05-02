#include "threads/CJob.h"
#include "threads/Threading.h"

#include "network/CAuthServer.h"
#include "network/CAuthSocket.h"
#include "network/CIOServerInt.h"
#include "network/CLogSocket.h"
#include "network/CSocketInt.h"
#include "network/IPSocket.h"
#include "network/WorldSrvServer.h"
#include "network/WorldSrvSocket.h"

#include "config/Config.h"

#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "utils/CExceptionInit.h"

#include "CIOObject.h"

#include "AppInstance.h"

#include <cmath>
#include <ctime>
#include <process.h>  // _beginthreadex

static HANDLE* g_hThread;
static unsigned int* g_nThreadId;
HANDLE Threading::g_hCompletionPort;

static HANDLE* g_hThreadExtra;
static unsigned int* g_nThreadIdExtra;
HANDLE Threading::g_hCompletionPortExtra;

bool Threading::g_teminateEvent = false;
bool Threading::g_bTerminating = false;

// 0x0043A4A3
void Threading::CreateIOThread()
{
    auth_guard;

    unsigned int timerThreadId;
    _beginthreadex(NULL, 0, &Threading::TimerThread, 0, 0, &timerThreadId);

    Threading::g_hCompletionPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    Threading::g_hCompletionPortExtra = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    g_hThread = new HANDLE[sizeof(HANDLE) * g_Config.numServerThread];
    g_nThreadId = new unsigned int[sizeof(unsigned int) * g_Config.numServerThread];

    g_hThreadExtra = new HANDLE[sizeof(HANDLE) * g_Config.numServerIntThread];
    g_nThreadIdExtra = new unsigned int[sizeof(unsigned int) * g_Config.numServerIntThread];

    for (int i = 0; i < g_Config.numServerThread; ++i)
    {
        g_hThread[i] = (HANDLE)_beginthreadex(NULL, 0, &Threading::IOThreadServer, (void*)i, 0, &g_nThreadId[i]);
    }

    for (int i = 0; i < g_Config.numServerIntThread; ++i)
    {
        g_hThreadExtra[i] = (HANDLE)_beginthreadex(NULL, 0, &Threading::IOThreadInt, (void*)i, 0, &g_nThreadIdExtra[i]);
    }

    auth_vunguard;
}

// 0x0043A2F1
unsigned int __stdcall Threading::ListenThread(void*)
{
    auth_guard;

    g_worldServServer.Run(g_Config.serverPort, &WorldSrvSocket::Allocate);
    g_authServer.Run(g_Config.serverExPort, &CAuthSocket::Allocate);
    g_CIOServerInt.Run(g_Config.serverIntPort, &CSocketInt::Allocate);

    g_job.RunEvent();

    g_worldServServer.Close();
    g_authServer.Stop();
    g_CIOServerInt.Close();

    if (g_IPSocket != NULL)
    {
        IPSocket::s_lock.WriteLock();

        if (!IPSocket::isReconnecting)
        {
            g_IPSocket->CloseSocket();
        }
        g_IPSocket->ReleaseRef();

        IPSocket::s_lock.WriteUnlock();
    }

    if (g_LogDSocket != NULL)
    {
        CLogSocket::s_lock.WriteLock();

        if (!CLogSocket::isReconnecting)
        {
            g_LogDSocket->CloseSocket();
        }
        g_LogDSocket->ReleaseRef();

        CLogSocket::s_lock.WriteUnlock();
    }

    delete[] g_hThread;
    delete[] g_nThreadId;
    delete[] g_hThreadExtra;
    delete[] g_nThreadIdExtra;

    Threading::g_teminateEvent = true;

    return 0;

    auth_unguard;
}

// 0x0043A0C0
unsigned int __stdcall Threading::TimerThread(void*)
{
    auth_guard;

    g_job.RunTimer();

    g_winlog.AddLog(LOG_WRN, "Timer thread terminated");
    return 0;

    auth_unguard;
}

// 0x0043A0E7
unsigned int __stdcall Threading::IOThreadServer(void*)
{
    auth_guard;

    time_t now = std::time(0);
    ::srand(static_cast<int>(now));

    while (!Threading::g_bTerminating)
    {
        DWORD dwTransferred = 0;
        OVERLAPPED* overlapped = NULL;
        CIOObject* pObject = NULL;
        BOOL success = ::GetQueuedCompletionStatus(Threading::g_hCompletionPort, &dwTransferred, (PULONG_PTR)&pObject, &overlapped, INFINITE);

        ::InterlockedIncrement(&CReporter::g_nRunningThread);

        pObject->OnIOCallback(success, dwTransferred, overlapped);

        ::InterlockedDecrement(&CReporter::g_nRunningThread);
    }

    g_winlog.AddLog(LOG_WRN, "terminate IOThreadServer");
    g_winlog.AddLog(LOG_WRN, "IOThreadServer Server Exit");

    return 0;

    auth_unguard;
}

// 0x0043A201
unsigned int __stdcall Threading::IOThreadInt(void*)
{
    auth_guard;

    time_t now = std::time(0);
    ::srand(static_cast<int>(now));
    while (!Threading::g_bTerminating)
    {
        DWORD dwTransferred = 0;
        OVERLAPPED* overlapped = NULL;
        CIOObject* pObject = NULL;
        BOOL success = ::GetQueuedCompletionStatus(Threading::g_hCompletionPortExtra, &dwTransferred, (PULONG_PTR)&pObject, &overlapped, INFINITE);
        if (pObject != NULL)  // FIXED: crash in debug, at exit
        {
            pObject->OnIOCallback(success, dwTransferred, overlapped);
        }
    }

    g_winlog.AddLog(LOG_WRN, "terminate IOThreadInt");

    return 0;

    auth_unguard;
}
