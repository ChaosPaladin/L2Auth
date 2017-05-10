#pragma once

#include "CIOObject.h"
#include "threads/CLock.h"

#include <Sqltypes.h>

struct SqlConnection;

class DBEnv : public CIOObject
{
public:
    DBEnv();   // 0x004133DF
    ~DBEnv();  // 0x0041346F

    void Init(int dbConnectionNumber);  // 0x004134F8

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x00414AF0
    void OnTimerCallback() override;                                                            // 0x00413D60
    void OnEventCallback() override;                                                            // 0x00414AE0

private:
    static BOOL CALLBACK LoginDlgProc(HWND hDlg, UINT msgType, WPARAM wparam, LPARAM lparam);  // 0x00413FD9

    void AllocSQLPool();         // 0x00413759
    void Destroy();              // 0x00413AC8
    bool Login(bool reconnect);  // 0x004135F2
    bool LoadConnStrFromReg();   // 0x00413B7E
    void SaveConnStrToReg();     // 0x00413C76

public:
    CLock spinLock;
    SQLHENV sqlEnvHandle;
    SqlConnection* sqlConnections;
    SqlConnection* connectionsHead;

    char connectionStr[256];
    int connectionNumber;
    int recoveryTriggered;
};

extern DBEnv g_linDB;
