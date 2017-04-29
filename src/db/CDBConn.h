#pragma once

#include <windows.h>
#include <Sqltypes.h>

#include <cstdint>

class DBEnv;
struct SqlConnection;

class CDBConn
{
public:
    CDBConn(DBEnv* dbEnv);                  // 0x00413E9E
    ~CDBConn();                             // 0x00413F5F
    bool Execute(const char* format, ...);  // 0x00414123
    SQLRETURN execRaw(const char* sqlQuery, int textLength);
    bool Fetch(bool* notFound);                                                    // 0x004143F1
    void Error(SQLSMALLINT handlerType, SQLHANDLE handler, const char* sqlQuery);  // 0x00414532
    SQLRETURN bindByte(char* value);                                               // 0x004146F6
    SQLRETURN bindBool(bool* value);                                               // 0x00414739
    SQLRETURN bindStr(SQLPOINTER value, SQLINTEGER length);                        // 0x0041477C
    SQLRETURN bindInt(int32_t* value);                                             // 0x00414804
    SQLRETURN bindUInt(uint32_t* value);                                           // 0x004147C1
    SQLRETURN bindParam(SQLUSMALLINT index, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLUINTEGER cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER cbValueMax, SQLINTEGER* pcbValue);

    void ResetHtmt();  // 0x00414A7B

    SQLHSTMT getHandler() const
    {
        return m_sqlHandler;
    }

private:
    /*
    static void ErrorExceptInsert(SQLSMALLINT handlerType, SQLHANDLE handler, const char *sqlQuery);
    // 0x00414647
    */

private:
    SQLHSTMT m_sqlHandler;
    int m_currentCollumn;
    int m_currentRow;
    SqlConnection* m_sqlConnection;
    DBEnv* m_dbEnv;
};
