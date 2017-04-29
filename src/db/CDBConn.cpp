#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "db/SqlConnection.h"
#include "threads/CLock.h"
#include "ui/CLog.h"

#include <Sql.h>
#include <Sqlext.h>
#include <Sqlucode.h>

// 0x00413E9E
CDBConn::CDBConn(DBEnv* dbEnv)
    : m_sqlHandler()
    , m_currentCollumn()
    , m_currentRow()
    , m_sqlConnection(0)
    , m_dbEnv(dbEnv)

{
    while (1)
    {
        m_dbEnv->spinLock.Enter();
        if (m_dbEnv->connectionsHead->prevConnection)
        {
            m_sqlConnection = m_dbEnv->connectionsHead;
            m_dbEnv->connectionsHead = m_dbEnv->connectionsHead->prevConnection;
            m_sqlConnection->free = 0;
        }
        m_dbEnv->spinLock.Leave();
        if (m_sqlConnection)
        {
            break;
        }
        ::Sleep(1000u);
    }
    m_sqlHandler = m_sqlConnection->stmtSqlHandler;
    m_currentCollumn = 1;
    m_currentRow = 1;
}

// 0x00413F5F
CDBConn::~CDBConn()
{
    m_dbEnv->spinLock.Enter();
    if (!m_sqlConnection->free)
    {
        ::SQLFreeStmt(m_sqlHandler, SQL_HANDLE_DBC);
        ::SQLFreeStmt(m_sqlHandler, SQL_NULL_HANDLE);
        ::SQLFreeStmt(m_sqlHandler, SQL_HANDLE_STMT);
        m_sqlConnection->prevConnection = m_dbEnv->connectionsHead;
        m_dbEnv->connectionsHead = m_sqlConnection;
    }
    m_dbEnv->spinLock.Leave();
}

// 0x00414123
bool CDBConn::Execute(const char* format, ...)
{
    va_list va;
    va_start(va, format);

    char sqlQuery[0x2000];
    int sqlQueryLength = vsprintf(sqlQuery, format, va);

    SQLRETURN sqlResult = SQL_ERROR;
    if (sqlQueryLength > 0)
    {
        sqlResult = execRaw(sqlQuery, sqlQueryLength);
    }

    if (sqlResult != SQL_SUCCESS)
    {
        Error(SQL_HANDLE_STMT, m_sqlHandler, sqlQuery);
        return false;
    }

    return true;
}

// 0x004143F1
bool CDBConn::Fetch(bool* notFound)
{
    SQLRETURN sqlreturn = ::SQLFetch(m_sqlHandler);
    if (notFound)
    {
        *notFound = (sqlreturn == SQL_NO_DATA);
        return true;
    }

    if (sqlreturn != SQL_SUCCESS && sqlreturn != SQL_SUCCESS_WITH_INFO)
    {
        if (sqlreturn != SQL_NO_DATA)
        {
            g_winlog.AddLog(LOG_ERR, "Fetch error = %d", sqlreturn);
        }
        return false;
    }

    return true;
}

// 0x00414532
void CDBConn::Error(SQLSMALLINT handlerType, SQLHANDLE handler, const char* sqlQuery)
{
    SQLSMALLINT textLength;
    SQLCHAR errorText[256];
    SQLINTEGER nativeError;
    char sqlState[12];

    SQLRETURN sqlResult = ::SQLGetDiagRec(handlerType, handler, 1, (SQLCHAR*)sqlState, &nativeError, errorText, 256, &textLength);
    if (sqlResult == SQL_SUCCESS)
    {
        if (sqlQuery != nullptr)
        {
            g_winlog.AddLog(LOG_ERR, "sql: %s", sqlQuery);
        }
        g_winlog.AddLog(LOG_ERR, "%s:%s", sqlState, errorText);

        if (strcmp(sqlState, "08S01") == 0)  // Communication link failure
        {
            m_dbEnv->spinLock.Enter();
            if (!m_dbEnv->recoveryTriggered && !m_sqlConnection->free)
            {
                m_dbEnv->RegisterTimer(30000, 1);
                m_dbEnv->recoveryTriggered = 1;
            }
            m_dbEnv->spinLock.Leave();
        }
    }
}

// void CDBConn::ErrorExceptInsert(SQLSMALLINT handlerType, SQLHANDLE handler, const char *sqlQuery)
// 0x00414647
//
//{
//    SQLSMALLINT textLength;
//    SQLCHAR errorText[256];
//    SQLINTEGER nativeError;
//    char Sqlstate[12];

//    SQLRETURN sqlResult = ::SQLGetDiagRec(handlerType, handler, 1, (SQLCHAR *)Sqlstate,
//    &nativeError, errorText, 256, &textLength);
//    if (sqlResult == SQL_SUCCESS)
//    {
//        if ( strcmp(Sqlstate, "23000") != 0 ) // Integrity constraint violation
//        {
//            if ( sqlQuery )
//            {
//                g_winlog.Add(LOG_ERR, format, sqlQuery);
//            }

//            sqlResult = (int)g_winlog.Add(LOG_ERR, "%s:%s", Sqlstate, errorText);
//        }
//    }
//}

// 0x004146F6
SQLRETURN CDBConn::bindByte(char* value)
{
    SQLUSMALLINT collumn = m_currentCollumn;
    ++m_currentCollumn;
    return ::SQLBindCol(m_sqlHandler, collumn, SQL_TINYINT, value, 1, 0);
}

// 0x00414739
SQLRETURN CDBConn::bindBool(bool* value)
{
    SQLUSMALLINT collumn = m_currentCollumn;
    ++m_currentCollumn;
    return ::SQLBindCol(m_sqlHandler, collumn, SQL_TINYINT, value, 1, 0);
}

// 0x0041477C
SQLRETURN CDBConn::bindStr(SQLPOINTER value, SQLINTEGER length)
{
    SQLUSMALLINT collumn = m_currentCollumn;
    ++m_currentCollumn;
    return ::SQLBindCol(m_sqlHandler, collumn, SQL_CHAR, value, length, 0);
}

// 0x00414804
SQLRETURN CDBConn::bindInt(int32_t* value)
{
    SQLUSMALLINT collumn = m_currentCollumn;
    ++m_currentCollumn;
    return ::SQLBindCol(m_sqlHandler, collumn, SQL_INTEGER, value, 4, 0);
}

// 0x004147C1
SQLRETURN CDBConn::bindUInt(uint32_t* value)
{
    SQLUSMALLINT collumn = m_currentCollumn;
    ++m_currentCollumn;
    return ::SQLBindCol(m_sqlHandler, collumn, SQL_INTEGER, value, 4, 0);
}

SQLRETURN CDBConn::bindParam(SQLUSMALLINT index, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLUINTEGER cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER cbValueMax, SQLINTEGER* pcbValue)
{
    return ::SQLBindParameter(m_sqlHandler, index, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
}

// 0x00414A7B
void CDBConn::ResetHtmt()
{
    m_currentCollumn = 1;
    m_currentRow = 1;
    ::SQLFreeStmt(m_sqlHandler, SQL_UNBIND);
    ::SQLFreeStmt(m_sqlHandler, SQL_CLOSE);
}

SQLRETURN CDBConn::execRaw(const char* sqlQuery, int textLength)
{
    return ::SQLExecDirect(m_sqlHandler, (SQLCHAR*)sqlQuery, textLength);
}
