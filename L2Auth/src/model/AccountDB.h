#pragma once

#include "model/LoginFailReason.h"
#include "model/LoginUser.h"
#include "model/PlayFail.h"
#include "threads/CLock.h"

#include <map>

class CAuthSocket;

class AccountDB
{
public:
    AccountDB();           // 0x00402281
    virtual ~AccountDB();  // 0x004022E9

    int FindAccount(int uid, char* accountName);                                                                                                                          // 0x00402340
    bool FindAccount(int uid, char* accName, int* loginFlag, int* warnFlag, int* payStat, int* sessionKey);                                                               // 0x00402411
    PlayFail UpdateSocket(int uid, SOCKET socket, int sessionKey, int serverId);                                                                                          // 0x00402507
    bool RegAccount(const LoginUser& userInfo, int uid, CAuthSocket* gameClient, int totalTime, int zero);                                                                // 0x004025DE
    bool KickAccount(int uid, char kickReason, bool sendToClient);                                                                                                        // 0x004027DF
    void RemoveAll(int serverId);                                                                                                                                         // 0x00402E5F
    void RemoveAll();                                                                                                                                                     // 0x00402F6E
    SOCKET FindSocket(int uid, bool recreate);                                                                                                                            // 0x0040308E
    SOCKET FindSocket(int uid, int serverId, bool restartTimer, int* selectedGServerId, char* accName);                                                                   // 0x00403167
    bool removeAccount(int uid, char* accName);                                                                                                                           // 0x004032CA
    bool removeAccountPreLogIn(int uid, SOCKET socket);                                                                                                                   // 0x0040340F
    bool logoutAccount(int uid, int sessionKey);                                                                                                                          // 0x004035F9
    bool logoutAccount(int uid);                                                                                                                                          // 0x004038C3
    bool recordGamePlayTime(int uid, int serverId);                                                                                                                       // 0x00403AA1
    bool quitGamePlay(int uid, int serverId);                                                                                                                             // 0x00403E28
    PlayFail AboutToPlay(int uid, char* accName, int totalTime, int loginFlag, int warnFlag, int sessionKey, CAuthSocket* gameClient, signed int serverId, int payStat);  // 0x00404A52
    bool GetAccountInfo(int uid, char* accountName, int* loginFlag, int* warnFlag, int* sessionKey, SOCKET* socket);                                                      // 0x00404C7A
    bool GetAccountInfoForIPStop(int uid, char* accName, int* payStat, in_addr* connectedIP, time_t* loginTime) const;                                                    // 0x00404D70
    bool RegAccountByServer(const LoginUser& user, int uid);                                                                                                              // 0x00404E2B
    int GetUserNum() const;                                                                                                                                               // 0x0040F5B0

    static LoginFailReason UserTimeLogin(unsigned int uid, LoginUser* userInfo, int* userTime);  // 0x004047E1
    static LoginFailReason CheckUserTime(unsigned int uid, int* userTime);                       // 0x0040492F

    static int GetCharNum(int uid);                                                                            // 0x00404E9A
    static bool CreateChar(int uid, int charId, int serverId);                                                 // 0x00404F3E
    static bool DelChar(int uid, int charId, int serverId);                                                    // 0x00404FDC
    static bool UpdateCharLocation(int uid, int oldCharId, char oldServerId, int newCharId, int newServerId);  // 0x004050DC

private:
    static bool SendWantedServerLogout(const char* accName, int uid, int serverId);                                                                                                                                                    // 0x00402160
    static void NTAPI TimerRoutine(PVOID uid, BOOLEAN a2);                                                                                                                                                                             // 0x00402BDA
    static bool RecordLogout(int uid, time_t loginTime, int lastWorld, in_addr userIp, int gameType, char* accountName, int payStat, int birthdayEncoded, int restOfSsn, SexAndCentury sexAndCentury, int age, int someClientCookie);  // 0x0040418D

    void TimerCallback(int uid);                          // 0x00402BF8
    PlayFail checkInGame(int uid, int sessionKey) const;  // 0x0040403B

private:
    static const int updateKey;
    static const int updateKey2;

    typedef std::map<int, LoginUser> UserMap;
    UserMap m_accounts;
    mutable CLock m_spinLock;
};

extern AccountDB g_accountDb;
