#include "db/CAccount.h"

#include "config/Config.h"
#include "db/CDBConn.h"
#include "db/DBEnv.h"

#include "crypt/DesNewCrypt.h"

#include <sql.h>
#include <sqlext.h>

#include <cstring>
#include <ctime>

void (*CAccount::g_pwdCrypt)(char*) = nullptr;

// 0x00401000
CAccount::CAccount()
    : blockFlag_custom(0)
    , blockFlag_standard(0)
    , warnFlag()
    , payStat()
    , uid()
    , loginFlag()
    , subscriptionFlag()
    , flagUnused(0)
    , flagUnused2(0)
    , birthdayEncoded(0)
    , restOfSsn(0)
    , age()
    , sexAndCentury(FemaleBorn_1800_to_1899)
    , lastworld(0)
    , block_end_date()
{
    memset(session, 0, sizeof(session));
}

// 0x00401074
CAccount::~CAccount()
{
}

// 0x00401088
int CAccount::MakeBlockInfo(char* reasonBuffer)
{
    int blockCount = 0;
    char* iterator = reasonBuffer;
    ++iterator;

    int reasonCode = 0;
    char blockMsg[256];

    CDBConn sql(&g_linDB);
    sql.bindInt(&reasonCode);
    sql.bindStr(blockMsg, 256);

    if (sql.Execute(" Select reason, msg From block_msg with (nolock) Where uid = %d", uid))
    {
        int totalLength = 1;
        bool notFound = true;
        while (sql.Fetch(&notFound))
        {
            if (notFound)
            {
                break;
            }

            wchar_t msg[256];
            size_t msgLength = 2 * swprintf(msg, L"%s", blockMsg) + 2;
            if ((msgLength + totalLength + 4) >= 0x1000)
            {
                break;
            }
            memcpy(iterator, &reasonCode, 4u);
            iterator += 4;
            memcpy(iterator, msg, msgLength);
            iterator += msgLength;
            ++blockCount;
            totalLength += msgLength + 4;
        }
    }
    memcpy(reasonBuffer, &blockCount, 1u);
    int length = iterator - reasonBuffer;

    return length;
}

// 0x004012A9
LoginFailReason CAccount::Load(const char* accountName)
{
    int accLength = strlen(accountName);
    CDBConn sql(&g_linDB);

    SQLINTEGER len1 = SQL_NTS;
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 14u, 0, (char*)accountName, accLength, &len1);
    SQLINTEGER len2 = 0;
    sql.bindParam(2u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &uid, 0, &len2);
    SQLINTEGER len3 = 0;
    sql.bindParam(3u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &payStat, 0, &len3);
    SQLINTEGER len4 = 0;
    sql.bindParam(4u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &loginFlag, 0, &len4);
    SQLINTEGER len5 = 0;
    sql.bindParam(5u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &warnFlag, 0, &len5);
    SQLINTEGER len6 = 0;
    sql.bindParam(6u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &blockFlag_custom, 0, &len6);
    SQLINTEGER len7 = 0;
    sql.bindParam(7u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &blockFlag_standard, 0, &len7);
    SQLINTEGER len8 = 0;
    sql.bindParam(8u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &subscriptionFlag, 0, &len8);
    SQLINTEGER len9 = 0;
    sql.bindParam(9u, SQL_PARAM_OUTPUT, SQL_C_UTINYINT, SQL_TINYINT, 0, 0, &lastworld, 0, &len9);
    block_end_date.year = -1;
    SQLINTEGER len10 = 0;
    sql.bindParam(10u, SQL_PARAM_OUTPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 23u, 3, &block_end_date, 0, &len10);

    char sqlQuery[256];
    sprintf(sqlQuery, "{CALL dbo.ap_GStat (?,?,?,?,?,?,?,?,?,?) }");

    SQLRETURN sqlFailed = sql.execRaw(sqlQuery, SQL_NTS);
    if (sqlFailed != SQL_SUCCESS)
    {
        sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
        sql.ResetHtmt();
        return REASON_SYSTEM_ERROR_LOGIN_LATER;
    }

    bool notFound = true;
    if (sql.Fetch(&notFound))
    {
        if (notFound)
        {
            sql.ResetHtmt();
            return REASON_ACCESS_FAILED_TRY_AGAIN_LATER;
        }
        sql.ResetHtmt();
        return REASON_SUCCESS;
    }

    sql.ResetHtmt();
    return REASON_ACCESS_FAILED_TRY_AGAIN_LATER;
}

// 0x00401627
LoginFailReason CAccount::LoadEtc()
{
    if (!g_Config.countryCode)
    {
        CDBConn sql(&g_linDB);
        sql.ResetHtmt();
        sql.bindStr(session, 14);

        if (!sql.Execute("Select ssn From user_info with (nolock) Where account = '%s'", account))
        {
            return REASON_SYSTEM_ERROR_LOGIN_LATER;
        }

        bool notFound = true;
        if (!sql.Fetch(&notFound))
        {
            return REASON_ACCOUNT_INFO_INCORRECT_CONTACT_SUPPORT;
        }
        if (notFound)
        {
            return REASON_ACCOUNT_INFO_INCORRECT_CONTACT_SUPPORT;
        }

        time_t sysTime = std::time(0);
        struct tm* now = std::localtime(&sysTime);
        char yearRest = now->tm_year / 10 + '0';
        char modulo = now->tm_year % 10 + '0';
        sexAndCentury = (SexAndCentury)(session[6] - '0');
        if (session[6] != '1' && session[6] != '2' && session[6] != '5' && session[6] != '6')
        {
            // 2000-2099 year
            age = 10 * (session[0] - '0') + session[1] - '0';
        }
        else
        {
            // 1900-1999 year
            age = modulo - session[1] + 10 * (yearRest - session[0]);
        }

        int dayMonthNowEncoded = now->tm_mday + (100 * (now->tm_mon + 1));
        int dayAndMonthEncoded = 10 * (session[4] - '0') + 100 * (session[3] - '0') + 1000 * (session[2] - '0') + session[5] - '0';

        if (dayMonthNowEncoded < dayAndMonthEncoded)
        {
            --age;
        }
        if (age < 0)
        {
            age = 0;
        }

        birthdayEncoded = 10000 * (10 * (session[0] - '0') + session[1] - '0') + dayAndMonthEncoded;
        restOfSsn = atoi(&session[6]);
    }
    return REASON_SUCCESS;
}

// 0x004018BB
LoginFailReason CAccount::LoadPassword(const char* acc, char* hashedPwd)
{
    *hashedPwd = 0;

    CDBConn sql(&g_linDB);
    SQLINTEGER len = SQL_NTS;
    int accLength = strlen(acc);
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 14u, 0, (char*)acc, accLength, &len);

    SQLINTEGER len2 = SQL_NTS;
    sql.bindParam(2u, SQL_PARAM_OUTPUT, SQL_C_BINARY, SQL_BINARY, 16u, 0, hashedPwd, 16, &len2);

    char sqlSelect[256];
    sprintf(sqlSelect, "{CALL dbo.ap_GPwd (?,?) }");

    SQLRETURN sqlFailed = sql.execRaw(sqlSelect, SQL_NTS);
    if (sqlFailed != SQL_SUCCESS)
    {
        sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlSelect);
        sql.ResetHtmt();
        return REASON_SYSTEM_ERROR_LOGIN_LATER;
    }

    bool notFound = true;
    if (sql.Fetch(&notFound))
    {
        if (notFound)
        {
            sql.ResetHtmt();
            return REASON_PASS_WRONG;
        }
        memset(account, 0, 15u);
        strcpy(account, acc);
        sql.ResetHtmt();
        return REASON_SUCCESS;
    }

    sql.ResetHtmt();
    return REASON_PASS_WRONG;
}

// 0x00401ACF
LoginFailReason CAccount::CheckPassword(const char* account, char* realPass)
{
    char storedHashedPwd[17];
    LoginFailReason errorCode = LoadPassword(account, storedHashedPwd);
    if (errorCode != REASON_SUCCESS)
    {
        return errorCode;
    }

    CAccount::g_pwdCrypt(realPass);
    storedHashedPwd[16] = 0;
    if (strcmp(realPass, storedHashedPwd) == 0)
    {
        errorCode = CAccount::Load(account);
        if (errorCode != REASON_SUCCESS)
        {
            return errorCode;
        }

        if (!g_Config.GMCheckMode || loginFlag & 0x10 || loginFlag & 0x20)
        {
            if (loginFlag & 3)
            {
                return REASON_TEMP_PASS_EXPIRED;
            }
            return CAccount::LoadEtc();
        }

        return REASON_SERVER_MAINTENANCE;
    }

    return REASON_USER_OR_PASS_WRONG;
}

// 0x00401B99
LoginFailReason CAccount::CheckNewPassword(const char* accName, char* realPass)
{
    CDBConn sql(&g_linDB);

    int accLength = strlen(accName);
    SQLINTEGER len = SQL_NTS;
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 14u, 0, (char*)accName, accLength, &len);

    SQLINTEGER pwdLength = 16;
    char storedPwd[17];
    memset(storedPwd, 0, sizeof(storedPwd));
    sql.bindParam(2u, SQL_PARAM_OUTPUT, SQL_C_BINARY, SQL_BINARY, 16u, 0, storedPwd, 16, &pwdLength);

    SQLINTEGER len3 = 0;
    bool passStoredInNewCrypt = false;
    sql.bindParam(3u, SQL_PARAM_OUTPUT, SQL_C_TINYINT, SQL_TINYINT, 0, 0, &passStoredInNewCrypt, 0, &len3);

    char sqlQuery[256];
    sprintf(sqlQuery, "{CALL dbo.ap_GPwdWithFlag(?,?,?) }");
    SQLRETURN sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
    if (sqlResult != SQL_SUCCESS)
    {
        sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
        sql.ResetHtmt();
        return REASON_SYSTEM_ERROR_LOGIN_LATER;
    }

    bool notFound = true;
    if (!sql.Fetch(&notFound))
    {
        sql.ResetHtmt();
        return REASON_PASS_WRONG;
    }

    if (notFound)
    {
        sql.ResetHtmt();
        return REASON_PASS_WRONG;
    }

    char realPassCopy[17];
    strncpy(realPassCopy, realPass, 16u);
    realPassCopy[16] = 0;

    memset(account, 0, 15u);
    strcpy(account, accName);
    sql.ResetHtmt();
    realPass[16] = 0;
    if (!passStoredInNewCrypt)
    {
        CAccount::g_pwdCrypt(realPass);
        if (strcmp(storedPwd, realPass))
        {
            return REASON_USER_OR_PASS_WRONG;
        }
    }

    int realPassLength = strlen(realPassCopy);
    for (int i = realPassLength; i < 16; ++i)
    {
        realPassCopy[i] = 0x10;
    }

    g_PwdCrypt.DesWriteBlock(realPassCopy, 16);
    if (passStoredInNewCrypt)
    {
        if (memcmp(storedPwd, realPassCopy, 16u) != 0)
        {
            // decrypted pwd missmatch
            return REASON_USER_OR_PASS_WRONG;
        }
    }
    else  // force to change pwd
    {
        len = SQL_NTS;
        SQLINTEGER pwdLength = 16;
        accLength = strlen(accName);
        sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 14u, 0, (char*)accName, accLength, &len);
        sql.bindParam(2u, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BINARY, 16u, 0, realPassCopy, 16, &pwdLength);

        sprintf(sqlQuery, "{CALL dbo.ap_SNewPwd(?,?) }");

        sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
        if (sqlResult != SQL_SUCCESS)
        {
            sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
            return REASON_SYSTEM_ERROR_LOGIN_LATER;
        }
        sql.ResetHtmt();
    }

    LoginFailReason error = Load(accName);
    if (error != REASON_SUCCESS)
    {
        return error;
    }

    if (!g_Config.GMCheckMode || loginFlag & 0x10 || loginFlag & 0x20)
    {
        if (loginFlag & 3)
        {
            return REASON_TEMP_PASS_EXPIRED;
        }
        return LoadEtc();
    }

    return REASON_SERVER_MAINTENANCE;
}
