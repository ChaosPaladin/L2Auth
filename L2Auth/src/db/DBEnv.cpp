#include "db/DBEnv.h"
#include "crypt/DesFrontEnd.h"
#include "db/CDBConn.h"
#include "db/SqlConnection.h"
#include "ui/CLog.h"
#include "ui/resources.h"

#include "AppInstance.h"
#include "config/Config.h"

#include <Sqlext.h>
#include <Winreg.h>
#include <sql.h>

#include <ctime>

DBEnv g_linDB;

// 0x004133DF
DBEnv::DBEnv()
    : CIOObject()
    , spinLock(LockType_WaitLock, 0)
    , sqlEnvHandle(SQL_NULL_HANDLE)
    , sqlConnections(NULL)
    , connectionsHead(NULL)
    , connectionNumber(0)
    , recoveryTriggered(false)
{
    m_nRefCount = 1;
}

// 0x0041346F
DBEnv::~DBEnv()
{
    if (sqlEnvHandle)
    {
        Destroy();
    }
    if (sqlConnections)
    {
        delete[] sqlConnections;
        sqlConnections = NULL;
    }
}

// 0x004134F8
void DBEnv::Init(int dbConnectionNumber)
{
    connectionNumber = dbConnectionNumber;
    sqlConnections = new SqlConnection[connectionNumber];
    ::SQLSetEnvAttr((SQLHENV)SQL_NULL_HANDLE, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, SQL_TINYINT);

    SQLRETURN allocRes = ::SQLAllocHandle(SQL_HANDLE_ENV, (SQLHANDLE)SQL_NULL_HANDLE, &sqlEnvHandle);
    if (allocRes != SQL_SUCCESS && allocRes != SQL_SUCCESS_WITH_INFO || (allocRes = ::SQLSetEnvAttr(sqlEnvHandle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0)) != SQL_SUCCESS && allocRes != SQL_SUCCESS_WITH_INFO)
    {
        g_winlog.AddLog(LOG_ERR, "db env allocation failed");
        return;
    }

    if (Login(false))
    {
        AllocSQLPool();
        return;
    }

    g_winlog.AddLog(LOG_ERR, "db login failed");
    ::SQLFreeHandle(SQL_HANDLE_ENV, sqlEnvHandle);
    sqlEnvHandle = SQL_NULL_HANDLE;
}

// 0x004135F2
bool DBEnv::Login(bool reconnect)
{
    if (!LoadConnStrFromReg())
    {
        ::DialogBoxParamA(hInstance, MAKEINTRESOURCE(DLG_DB_CONNECTION), NULL, &DBEnv::LoginDlgProc, (LPARAM)this);
    }

    SQLHANDLE dbcHandler;
    SQLRETURN sqlResult = ::SQLAllocHandle(SQL_HANDLE_DBC, sqlEnvHandle, &dbcHandler);
    if (sqlResult != SQL_SUCCESS && sqlResult != SQL_SUCCESS_WITH_INFO)
    {
        g_winlog.AddLog(LOG_WRN, "hdbc allocation failed");
        return false;
    }

    if (reconnect == true)
    {
        ::SQLSetConnectAttr(dbcHandler, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)20, 0);
    }
    else
    {
        ::SQLSetConnectAttr(dbcHandler, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)60, 0);
    }

    ::SQLSetConnectAttr(dbcHandler, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)60, 0);

    while (true)
    {
        SQLCHAR szConnStrOut[1024];
        SQLSMALLINT pcchConnStrOut;
        int len = strlen(connectionStr);
        sqlResult = ::SQLDriverConnect(dbcHandler, NULL, (SQLCHAR*)connectionStr, len, szConnStrOut, sizeof(szConnStrOut), &pcchConnStrOut, 0);

        if (sqlResult == SQL_SUCCESS || sqlResult == SQL_SUCCESS_WITH_INFO)
        {
            break;
        }

        if (!reconnect)
        {
            ::DialogBoxParamA(hInstance, MAKEINTRESOURCE(DLG_DB_CONNECTION), NULL, &DBEnv::LoginDlgProc, (LPARAM)this);
        }
    }

    ::SQLDisconnect(dbcHandler);
    ::SQLFreeHandle(SQL_HANDLE_DBC, dbcHandler);

    return true;
}

// 0x00413759
void DBEnv::AllocSQLPool()
{
    for (int i = 0; i < connectionNumber; ++i)
    {
        sqlConnections[i].prevConnection = NULL;
        SQLRETURN sqlResult = ::SQLAllocHandle(SQL_HANDLE_DBC, sqlEnvHandle, &sqlConnections[i].dbcSqlHandler);
        if (sqlResult != SQL_SUCCESS && sqlResult != SQL_SUCCESS_WITH_INFO)
        {
            g_winlog.AddLog(LOG_ERR, "hdbc allocation failed");
            sqlConnections[i].stmtSqlHandler = SQL_NULL_HANDLE;
            sqlConnections[i].dbcSqlHandler = SQL_NULL_HANDLE;
            sqlConnections[i].free = false;
        }
        else
        {
            ::SQLSetConnectAttr(sqlConnections[i].dbcSqlHandler, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)60, 0);
            ::SQLSetConnectAttr(sqlConnections[i].dbcSqlHandler, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)60, 0);
            size_t len = strlen(connectionStr);

            SQLCHAR szConnStrOut[1024];
            SQLSMALLINT pcchConnStrOut;
            sqlResult = ::SQLDriverConnect(sqlConnections[i].dbcSqlHandler, NULL, (SQLCHAR*)connectionStr, len, szConnStrOut, 1024, &pcchConnStrOut, 0);

            if (sqlResult != SQL_SUCCESS && sqlResult != SQL_SUCCESS_WITH_INFO)
            {
                g_winlog.AddLog(LOG_ERR, "hdbc connection failed");
                ::SQLFreeHandle(SQL_HANDLE_DBC, sqlConnections[i].dbcSqlHandler);
                sqlConnections[i].stmtSqlHandler = SQL_NULL_HANDLE;
                sqlConnections[i].dbcSqlHandler = SQL_NULL_HANDLE;
                sqlConnections[i].free = false;
            }
            else
            {
                sqlResult = ::SQLAllocHandle(SQL_HANDLE_STMT, sqlConnections[i].dbcSqlHandler, &sqlConnections[i].stmtSqlHandler);

                if (sqlResult != SQL_SUCCESS && sqlResult != SQL_SUCCESS_WITH_INFO)
                {
                    g_winlog.AddLog(LOG_ERR, "stmt allocation failed");
                    ::SQLFreeHandle(SQL_HANDLE_DBC, sqlConnections[i].dbcSqlHandler);
                    sqlConnections[i].stmtSqlHandler = SQL_NULL_HANDLE;
                    sqlConnections[i].dbcSqlHandler = SQL_NULL_HANDLE;
                    sqlConnections[i].free = false;
                }
                else
                {
                    sqlConnections[i].free = true;
                }
            }
        }
    }

    for (int i = 0;; ++i)
    {
        if (i >= connectionNumber)
        {
            break;
        }

        if (sqlConnections[i].stmtSqlHandler != SQL_NULL_HANDLE)
        {
            if (connectionsHead != NULL)
            {
                sqlConnections[i].prevConnection = connectionsHead;
                connectionsHead = &sqlConnections[i];
            }
            else
            {
                connectionsHead = &sqlConnections[i];
            }
        }
    }
}

// 0x00413AC8
void DBEnv::Destroy()
{
    for (int i = 0; i < connectionNumber; ++i)
    {
        if (sqlConnections[i].stmtSqlHandler != SQL_NULL_HANDLE)
        {
            ::SQLFreeHandle(SQL_HANDLE_STMT, sqlConnections[i].stmtSqlHandler);
        }

        if (sqlConnections[i].dbcSqlHandler != SQL_NULL_HANDLE)
        {
            ::SQLDisconnect(sqlConnections[i].dbcSqlHandler);
            ::SQLFreeHandle(SQL_HANDLE_DBC, sqlConnections[i].dbcSqlHandler);
        }
    }

    ::SQLFreeHandle(SQL_HANDLE_ENV, sqlEnvHandle);
    sqlEnvHandle = SQL_NULL_HANDLE;
}

// 0x00413B7E
bool DBEnv::LoadConnStrFromReg()
{
    const char* valueName = NULL;
    if (g_Config.gameID != 8 && g_Config.gameID != 0x10 && g_Config.gameID != 0x20)
    {
        if (g_Config.gameID == 4)
        {
            valueName = "sldb";
        }
    }
    else
    {
        valueName = "L2Conn";
    }

    bool succeed = false;
    char storedValue[256];
    HKEY key;
    LSTATUS regResult = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\NCSoft\\L2AUTHD", 0, KEY_READ, &key);
    if (regResult == ERROR_SUCCESS)
    {
        unsigned long length = sizeof(storedValue);
        DWORD valueType;
        regResult = ::RegQueryValueExA(key, valueName, 0, &valueType, (LPBYTE)storedValue, &length);
        if (regResult == ERROR_SUCCESS && valueType == REG_BINARY)
        {
            succeed = true;
        }

        ::RegCloseKey(key);
    }
    if (succeed)
    {
        DesFrontEnd::DesReadBlock(storedValue, sizeof(storedValue));
        strcpy(connectionStr, storedValue);
    }
    return succeed;
}

// 0x00413C76
void DBEnv::SaveConnStrToReg()
{
    const char* valueName = NULL;
    if (g_Config.gameID != 8 && g_Config.gameID != 0x10 && g_Config.gameID != 0x20)
    {
        if (g_Config.gameID == 4)
        {
            valueName = "sldb";
        }
    }
    else
    {
        valueName = "L2Conn";
    }

    char connectionString[256];
    strcpy(connectionString, connectionStr);
    DesFrontEnd::DesWriteBlock(connectionString, sizeof(connectionString));

    char keyClass[4];
    HKEY key;
    DWORD dwDisposition;
    LSTATUS result = ::RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\NCSoft\\L2AUTHD", 0, keyClass, 0, KEY_WRITE, 0, &key, &dwDisposition);
    if (result == ERROR_SUCCESS)
    {
        ::RegSetValueExA(key, valueName, 0, REG_BINARY, (BYTE*)connectionString, 256u);
        ::RegCloseKey(key);
    }
}

// 0x00414AF0
void DBEnv::OnIOCallback(BOOL /*bSuccess*/, DWORD /*dwTransferred*/, LPOVERLAPPED /*lpOverlapped*/)
{
}

// 0x00413D60
void DBEnv::OnTimerCallback()
{
    if (Login(true))
    {
        spinLock.Enter();
        Destroy();

        ::SQLSetEnvAttr((SQLHENV)SQL_NULL_HANDLE, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, SQL_TINYINT);
        SQLRETURN allocRes = ::SQLAllocHandle(SQL_HANDLE_ENV, (SQLHANDLE)SQL_NULL_HANDLE, &sqlEnvHandle);
        if (allocRes != SQL_SUCCESS && allocRes != SQL_SUCCESS_WITH_INFO)
        {
            spinLock.Leave();
        }
        else
        {
            SQLRETURN setEnvRes = ::SQLSetEnvAttr(sqlEnvHandle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
            if (setEnvRes != SQL_SUCCESS && setEnvRes != SQL_SUCCESS_WITH_INFO)
            {
                spinLock.Leave();
                RegisterTimer(30000, 1);
                g_winlog.AddLog(LOG_ERR, "db recovery failed: env allocation");
            }
            else
            {
                AllocSQLPool();
                recoveryTriggered = 0;
                spinLock.Leave();
                g_winlog.AddLog(LOG_INF, "db recovery succeeded");
            }
        }
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "db recovery failed: login impossible");
        RegisterTimer(30000, 1);
    }
    return ReleaseRef();
}

// 0x00414AE0
void DBEnv::OnEventCallback()
{
}

// 0x00413FD9
BOOL CALLBACK DBEnv::LoginDlgProc(HWND hDlg, UINT msgType, WPARAM wparam, LPARAM lparam)
{
    static DBEnv* instance = NULL;

    if (msgType == WM_INITDIALOG)
    {
        instance = (DBEnv*)lparam;
        ::SendDlgItemMessageA(hDlg, DLG_DB_CONNECTION_FILE_DSN, WM_SETTEXT, 0, (LPARAM) "L2Conn");
        ::SetWindowTextA(hDlg, "L2 ODBC Connection Info");
        return 0;
    }

    if (msgType == WM_COMMAND && wparam == IDOK)
    {
        char* connStr = instance->connectionStr;
        char buffer[64];

        ::SendDlgItemMessageA(hDlg, DLG_DB_CONNECTION_FILE_DSN, WM_GETTEXT, 64u, (LPARAM)buffer);
        strcpy(connStr, "FILEDSN=");
        strcat(connStr, buffer);

        ::SendDlgItemMessageA(hDlg, DLG_DB_CONNECTION_LOGIN_NAME, WM_GETTEXT, 64u, (LPARAM)buffer);
        strcat(connStr, ";UID=");
        strcat(connStr, buffer);

        ::SendDlgItemMessageA(hDlg, DLG_DB_CONNECTION_PASSWORD, WM_GETTEXT, 64u, (LPARAM)buffer);
        strcat(connStr, ";PWD=");
        strcat(connStr, buffer);

        instance->SaveConnStrToReg();

        ::EndDialog(hDlg, 0);
    }

    return 0;
}
