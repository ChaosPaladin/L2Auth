#pragma once

#include "model/LoginState.h"
#include "model/SexAndCentury.h"

#include <ctime>
#include <windows.h>

class LoginUser
{
public:
    time_t loginTime;
    int sessionKey;
    in_addr connectedIP;
    int payStat;
    char loggedGameServerId;
    SOCKET gameSocket;
    LoginState loginState;
    char accountName[15];
    HANDLE timerHandler;
    SexAndCentury sexAndCentury;  // see "Resident registration number"
    int birthdayEncoded;          // yy-mm-dd: 861112 => 1986-11-12
    int restOfSsn;
    int loginFlag;
    int warnFlag;
    char age;
    short clientCookie;
    char lastworld;
    char selectedGServerId;
    int notifyGameServer;
};
