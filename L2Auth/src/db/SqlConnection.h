#pragma once

#include <Sqltypes.h>
#include <windows.h>

struct SqlConnection;

struct SqlConnection
{
    bool free;
    SQLHANDLE dbcSqlHandler;
    SQLHANDLE stmtSqlHandler;
    SqlConnection* prevConnection;
};
