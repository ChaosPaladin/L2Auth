#pragma once

#include "model/LoginFailReason.h"
#include "model/SexAndCentury.h"

#include <windows.h>
#include <sqltypes.h>

class CAccount
{
public:
    CAccount();           // 0x00401000
    virtual ~CAccount();  // 0x00401074

    int MakeBlockInfo(char* reasonBuffer);                                  // 0x00401088
    LoginFailReason Load(const char* accountName);                          // 0x004012A9
    LoginFailReason CheckPassword(const char* account, char* realPass);     // 0x00401ACF
    LoginFailReason CheckNewPassword(const char* accName, char* realPass);  // 0x00401B99

public:
    static void (*g_pwdCrypt)(char* password);

private:
    LoginFailReason LoadEtc();                                       // 0x00401627
    LoginFailReason LoadPassword(const char* acc, char* hashedPwd);  // 0x004018BB

public:
    int blockFlag_custom;
    //    REASON_DATA_STEALER(0x01),
    //    REASON_GENERIC_VIOLATION(0x08),
    //    REASON_7_DAYS_SUSPENDED(0x10),
    //    REASON_PERMANENTLY_BANNED(0x20);
    int blockFlag_standard;
    int warnFlag;
    int payStat;
    int uid;
    int loginFlag;
    int subscriptionFlag;
    int flagUnused;
    int flagUnused2;
    int birthdayEncoded;  // yy-mm-dd: 861112 => 1986-11-12
    int restOfSsn;
    char age;
    SexAndCentury sexAndCentury;
    char account[15];
    char session[14];
    char lastworld;
    TIMESTAMP_STRUCT block_end_date;
};
