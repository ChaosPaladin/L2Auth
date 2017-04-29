#pragma once

typedef unsigned int SOCKET;

struct WorldServer
{
    char serverId;
    char outerIPStr[16];
    char innerIPStr[16];
    char serverName[26];
    int outerIP;
    int ipAddress;
    char ageLimit;
    bool pvp;
    int userNumber;
    SOCKET socket;
    int kind;
    int serverPort;
};
