#include "model/AccountDB.h"
#include "network/CAuthSocket.h"
#include "network/CWantedSocket.h"
#include "network/IPSocket.h"

#include "db/CDBConn.h"
#include "db/DBEnv.h"
#include "logger/CFileLog.h"
#include "model/CServerList.h"
#include "model/ServersProvider.h"
#include "network/CAuthSocket.h"
#include "network/IPSessionDB.h"
#include "network/WorldSrvServer.h"
#include "network/packets/LoginPackets.h"
#include "ui/CLog.h"
#include "ui/CReporter.h"
#include "utils/CExceptionInit.h"
#include "utils/Utils.h"

#include "config/Config.h"

#include <Sql.h>
#include <sqlext.h>

AccountDB g_accountDb;

const int AccountDB::updateKey = 0;
const int AccountDB::updateKey2 = 0;

// 0x00402281
AccountDB::AccountDB()
    : m_accounts()
    , m_spinLock(LockType_CritSection, 0)
{
}

// 0x004022E9
AccountDB::~AccountDB()
{
}

// 0x00402160
bool AccountDB::SendWantedServerLogout(const char* accName, int uid, int serverId)
{
    if (g_Config.useWantedSystem && !CWantedSocket::isReconnecting && serverId > 0)
    {
        char packet[24];
        memset(packet, 0, 24u);
        if (g_Config.gameID == 8)
        {
            packet[0] = 2;
            memcpy(&packet[1], &uid, 4u);
            strcpy(&packet[5], accName);
            packet[19] = serverId;
            time_t now = std::time(0);
            memcpy(&packet[20], &now, 4u);
            if (g_SocketWanted != NULL && g_Config.useWantedSystem && !CWantedSocket::isReconnecting)
            {
                g_winlog.AddLog(LOG_DBG, "Wanted User LogOut, %s", accName);
                CWantedSocket::s_lock.ReadLock();
                g_SocketWanted->AddRef();
                g_SocketWanted->Send("cb", 1, 24, packet);
                g_SocketWanted->ReleaseRef();
                CWantedSocket::s_lock.ReadUnlock();
            }
            return true;
        }

        return true;
    }

    return true;
}

// 0x00402340
int AccountDB::FindAccount(int uid, char* accountName)
{
    int lastworld = -1;
    HANDLE timer = NULL;
    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        ::strncpy(accountName, user.accountName, sizeof(user.accountName));
        timer = user.timerHandler;
        user.timerHandler = NULL;
        lastworld = user.lastworld;

        m_spinLock.Leave();
    }
    else
    {
        m_spinLock.Leave();
    }

    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, NULL);
    }

    return lastworld;
}

// 0x00402411
bool AccountDB::FindAccount(int uid, char* accName, int* loginFlag, int* warnFlag, int* payStat, int* sessionKey)
{
    bool result = false;
    HANDLE timer = NULL;
    m_spinLock.Enter();

    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        ::strncpy(accName, user.accountName, sizeof(user.accountName));
        *loginFlag = user.loginFlag;
        *warnFlag = user.warnFlag;
        *payStat = user.payStat;
        timer = user.timerHandler;
        *sessionKey = user.sessionKey;
        user.timerHandler = NULL;
        result = true;
    }

    m_spinLock.Leave();

    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, 0);
    }

    return result;
}

// 0x00402507
PlayFail AccountDB::UpdateSocket(int uid, SOCKET socket, int sessionKey, int serverId)
{
    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it == m_accounts.end())
    {
        m_spinLock.Leave();
        return PLAY_FAIL_WRONG_ACC_NAME;
    }

    LoginUser& user = it->second;
    if (sessionKey != user.sessionKey)
    {
        m_spinLock.Leave();
        return PLAY_FAIL_SYSTEM_ERROR;
    }

    if (user.loginState == LoginState_LoggedToGS)
    {
        m_spinLock.Leave();
        return PLAY_FAIL_ALREADY_LOGGED;
    }

    user.gameSocket = socket;
    user.selectedGServerId = serverId;
    user.loggedGameServerId = 0;

    m_spinLock.Leave();
    return PLAY_FAIL_NO_ERROR;
}

// 0x004025DE
bool AccountDB::RegAccount(const LoginUser& user, int uid, CAuthSocket* gameClient, int totalTime, int zero)
{
    m_spinLock.Enter();
    std::pair<UserMap::iterator, bool> result = m_accounts.insert(std::make_pair(uid, user));
    const bool isOffline = result.second;  // second == true, means was inserted, not changed
    m_spinLock.Leave();

    bool success = isOffline;
    if (success)
    {
        if (user.notifyGameServer & 0x10)
        {
            WorldServer worldServer = g_serverList.GetAt(1);
            success = WorldSrvServer::SendSocket(worldServer.ipAddress, "cdsdddd", 0, uid, user.accountName, totalTime, user.loginFlag, user.warnFlag, user.payStat);
            if (!success)
            {
                m_spinLock.Enter();
                UserMap::iterator it = m_accounts.find(uid);
                if (it != m_accounts.end())
                {
                    m_accounts.erase(uid);
                }
                m_spinLock.Leave();
                gameClient->Send("cc", LS_LoginFail, REASON_IP_SESSION_REJECTED);
            }
        }
        if (success)
        {
            gameClient->m_clientLoginState = LoginState_Connected;
            int sessionKey = gameClient->GetMd5Key();
            gameClient->Send("cddddddddd", LS_LoginOk, uid, sessionKey, AccountDB::updateKey, AccountDB::updateKey2, user.payStat, totalTime, zero, user.warnFlag, user.loginFlag);
        }
    }
    else
    {
        KickAccount(uid, REASON_ACCOUNT_IN_USE, true);
        gameClient->Send("cc", LS_LoginFail, REASON_ACCOUNT_IN_USE);
    }

    return success;
}

// 0x004027DF
bool AccountDB::KickAccount(int uid, char kickReason, bool sendToClient)
{
    bool found = false;
    int loggedServerId = 0;
    SOCKET gameSocket = INVALID_SOCKET;
    LoginState loginState = LoginState_Initial;
    SexAndCentury sexAndCentury = FemaleBorn_1800_to_1899;
    char accountName[15];
    accountName[0] = 0;
    int birthdayEncoded = 0;
    int restOfSsn = 0;
    int payStat = 0;
    int warnFlag = 0;
    char age = 0;
    int clientCookie = 0;
    in_addr clientIP;
    time_t loginTime = 0;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        gameSocket = user.gameSocket;
        loginState = user.loginState;
        loggedServerId = user.loggedGameServerId;
        sexAndCentury = user.sexAndCentury;
        birthdayEncoded = user.birthdayEncoded;
        restOfSsn = user.restOfSsn;
        payStat = user.payStat;
        clientIP = user.connectedIP;
        loginTime = user.loginTime;
        age = user.age;
        clientCookie = user.clientCookie;
        warnFlag = user.warnFlag;
        ::strncpy(accountName, user.accountName, sizeof(accountName));
        m_accounts.erase(it);

        m_spinLock.Leave();
        found = true;
        accountName[14] = 0;
    }
    else
    {
        m_spinLock.Leave();
    }

    if (!found)
    {
        return found;
    }

    if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
    {
        SendWantedServerLogout(accountName, uid, loggedServerId);
    }

    Utils::StdAccount(accountName);

    if (::strlen(accountName) >= 2 && sexAndCentury < ForeignerMaleBorn_2000_to_2099)
    {
        Utils::WriteLogD(831, accountName, clientIP, payStat, age, sexAndCentury, 0, 0, uid);
    }

    if (loginState != LoginState_LoggedToGS && loginState != LoginState_AcceptedByGS)
    {
        if (payStat < 1000 && payStat > 0)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey > 0)
            {
                if (g_Config.useIPServer)
                {
                    g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
                }
            }
        }

        if (gameSocket != INVALID_SOCKET && sendToClient)
        {
            CAuthSocket::Send(gameSocket, "cc", LS_KickedFromGS, kickReason);
        }
        return found;
    }

    if (loginState == LoginState_LoggedToGS && loggedServerId > 0)
    {
        AccountDB::RecordLogout(uid, loginTime, loggedServerId, clientIP, g_Config.gameID, accountName, payStat, birthdayEncoded, restOfSsn, sexAndCentury, age, clientCookie);
    }

    if (loggedServerId <= g_serverList.m_serverNumber && loggedServerId >= 1)
    {
        WorldServer worldServer = g_serverList.GetAt(loggedServerId);
        WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcs", 1, uid, kickReason, accountName);
        if (payStat < 1000 && payStat > 0)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey > 0)
            {
                if (g_Config.useIPServer)
                {
                    g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
                }
            }
        }
        return found;
    }

    if (payStat < 1000 && payStat > 0)
    {
        int sessionKey = g_IPSessionDB.DelSessionID(uid);
        if (sessionKey > 0)
        {
            if (g_Config.useIPServer)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
            }
        }
    }

    return found;
}

// 0x00402BDA
void NTAPI AccountDB::TimerRoutine(PVOID uid, BOOLEAN /*a2*/)
{
    g_accountDb.TimerCallback((int)uid);
}

// 0x00402BF8
void AccountDB::TimerCallback(int uid)
{
    bool removed = false;
    int loggedGameServerId = 0;
    int selectedGServerId = 0;
    int payStat = 0;
    int warnFlag = 0;
    LoginState loginState = LoginState_Initial;
    char accountName[15];
    in_addr clientIP = {0};

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        loginState = user.loginState;
        loggedGameServerId = user.loggedGameServerId;
        selectedGServerId = user.selectedGServerId;
        ::strncpy(accountName, user.accountName, sizeof(accountName));
        warnFlag = user.warnFlag;
        if (loginState == LoginState_LoggedToGS)
        {
            user.timerHandler = NULL;
        }
        else
        {
            payStat = user.payStat;
            clientIP = user.connectedIP;
            m_accounts.erase(it);
            removed = true;
        }
    }
    m_spinLock.Leave();

    accountName[14] = 0;
    if (removed)
    {
        if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
        {
            SendWantedServerLogout(accountName, uid, loggedGameServerId);
        }

        if (payStat < 1000 && payStat > 0)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
            }
        }
    }

    if (loginState == LoginState_AcceptedByGS && removed)  // 5 min timeout
    {
        if (selectedGServerId <= g_serverList.m_serverNumber && selectedGServerId >= 1)
        {
            WorldServer worldServer = g_serverList.GetAt(selectedGServerId);
            WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcs", 1, uid, 0, accountName);
        }

        if (loggedGameServerId <= g_serverList.m_serverNumber && loggedGameServerId >= 1)
        {
            WorldServer worldServer = g_serverList.GetAt(loggedGameServerId);
            WorldSrvServer::SendSocket(worldServer.ipAddress, "cdcs", 1, uid, 0, accountName);
        }
    }
}

// 0x0040403B
PlayFail AccountDB::checkInGame(int uid, int sessionKey) const
{
    m_spinLock.Enter();
    UserMap::const_iterator it = m_accounts.find(uid);
    if (it == m_accounts.end())
    {
        m_spinLock.Leave();
        return PLAY_FAIL_WRONG_ACC_NAME;
    }

    const LoginUser& user = it->second;
    if (user.loginState == LoginState_LoggedToGS)
    {
        m_spinLock.Leave();
        return PLAY_ACCOUNT_IN_USE;
    }

    if (user.sessionKey == sessionKey)
    {
        m_spinLock.Leave();
        return PLAY_FAIL_NO_ERROR;
    }

    m_spinLock.Leave();
    return PLAY_FAIL_SYSTEM_ERROR;
}

// 0x00402E5F
void AccountDB::RemoveAll(int serverId)
{
    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.begin();
    while (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        if (user.loggedGameServerId != serverId && user.selectedGServerId != serverId)
        {
            ++it;
            continue;
        }

        if (user.payStat < 1000)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(it->first);
            if (sessionKey > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, user.connectedIP, user.payStat);
            }
        }

        it = m_accounts.erase(it);
        ::InterlockedDecrement(&g_reporter.loggedUsers);
    }

    m_spinLock.Leave();
}

// 0x00402F6E
void AccountDB::RemoveAll()
{
    m_spinLock.Enter();
    for (UserMap::iterator it = m_accounts.begin(); it != m_accounts.end(); it = m_accounts.erase(it))
    {
        const LoginUser& user = it->second;

        if (user.gameSocket != INVALID_SOCKET)
        {
            CAuthSocket::Send(user.gameSocket, "cc", LS_PlayFail, PLAY_FAIL_GS_ERROR);
        }

        if (user.payStat < 1000 && user.payStat > 0)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(it->first);
            if (sessionKey > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, user.connectedIP, user.payStat);
            }
        }
    }
    m_spinLock.Leave();

    g_reporter.loggedUsers = 0;
}

// 0x0040308E
SOCKET AccountDB::FindSocket(int uid, bool recreate)
{
    HANDLE phNewTimer = NULL;
    if (recreate)
    {
        ::CreateTimerQueueTimer(&phNewTimer, 0, &AccountDB::TimerRoutine, (PVOID)uid, 300000u, 0, 0);
    }

    SOCKET gameSocket = 0;
    HANDLE timer = NULL;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        gameSocket = user.gameSocket;
        timer = user.timerHandler;
        user.timerHandler = phNewTimer;
    }

    m_spinLock.Leave();
    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, NULL);
    }

    return gameSocket;
}

// 0x00403167
SOCKET AccountDB::FindSocket(int uid, int serverId, bool restartTimer, int* selectedGServerId, char* accName)
{
    SOCKET gameSocket = 0;
    HANDLE phNewTimer = NULL;
    HANDLE timer = NULL;
    if (restartTimer)
    {
        ::CreateTimerQueueTimer(&phNewTimer, 0, &AccountDB::TimerRoutine, (PVOID)uid, 300000u, 0, 0);
    }

    *selectedGServerId = 0;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        gameSocket = user.gameSocket;
        timer = user.timerHandler;
        user.timerHandler = phNewTimer;
        if (serverId == user.selectedGServerId)
        {
            user.loggedGameServerId = serverId;
            user.loginState = LoginState_AcceptedByGS;
            ::strncpy(accName, user.accountName, sizeof(user.accountName));  // FIXED, were 14
        }
        else
        {
            *selectedGServerId = user.selectedGServerId;
            ::strncpy(accName, user.accountName, sizeof(user.accountName));  // FIXED, were 14
        }
        user.selectedGServerId = 0;
    }
    m_spinLock.Leave();

    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, NULL);
    }

    return gameSocket;
}

// 0x004032CA
bool AccountDB::removeAccount(int uid, char* accName)
{
    bool success = false;
    int serverId = 0;
    int payStat = 0;
    int warnFlag = 0;
    in_addr clientIP;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        ::strncpy(accName, user.accountName, sizeof(user.accountName));
        warnFlag = user.warnFlag;
        payStat = user.payStat;
        clientIP = user.connectedIP;
        m_accounts.erase(it);
        success = true;
    }
    m_spinLock.Leave();

    accName[14] = 0;
    if (success)
    {
        if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
        {
            SendWantedServerLogout(accName, uid, serverId);
        }

        if (payStat < 1000)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
            }
        }
    }

    return success;
}

// 0x0040340F
bool AccountDB::removeAccountPreLogIn(int uid, SOCKET socket)
{
    bool success = false;
    char age = 0;
    SexAndCentury sexAndCentury = FemaleBorn_1800_to_1899;
    int serverId = 0;
    int warnFlag = 0;
    int payStat;
    in_addr clientIP;
    char accName[15];

    m_spinLock.Enter();

    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        if (user.loginState != LoginState_AcceptedByGS)
        {
            if (user.gameSocket == socket)
            {
                payStat = user.payStat;
                clientIP = user.connectedIP;
                //::strcpy(accName, user.accountName); FIXED
                ::strncpy(accName, user.accountName, sizeof(user.accountName));
                age = user.age;
                sexAndCentury = user.sexAndCentury;
                serverId = user.loggedGameServerId;
                warnFlag = user.warnFlag;
                m_accounts.erase(it);
                success = true;
            }
        }
    }
    m_spinLock.Leave();
    accName[14] = 0;

    if (payStat < 1000)
    {
        if (success)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
            }
        }
    }

    if (success)
    {
        if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
        {
            SendWantedServerLogout(accName, uid, serverId);
        }

        if (strlen(accName) >= 2 && sexAndCentury < ForeignerMaleBorn_2000_to_2099)
        {
            Utils::WriteLogD(831, accName, clientIP, payStat, age, sexAndCentury, 0, 0, uid);
        }
    }

    return true;
}

// 0x004035F9
bool AccountDB::logoutAccount(int uid, int sessionKey)
{

    bool success = false;
    int payStat = 0;
    char age = 0;
    int serverId = 0;
    int warnFlag = 0;
    char accountName[15];
    in_addr clientIP;
    char sexAndCentury;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        if (user.sessionKey == sessionKey)
        {
            if (user.loginState == LoginState_LoggedToGS)
            {
                m_spinLock.Leave();
                success = g_accountDb.quitGamePlay(uid, 0);
                if (success)
                {
                    success = g_accountDb.logoutAccount(uid);
                }

                return success;
            }

            ::strncpy(accountName, user.accountName, sizeof(user.accountName));
            clientIP = user.connectedIP;
            payStat = user.payStat;
            sexAndCentury = user.sexAndCentury;
            age = user.age;
            serverId = user.loggedGameServerId;
            warnFlag = user.warnFlag;
            m_accounts.erase(it);
            success = true;
        }
    }
    m_spinLock.Leave();
    accountName[14] = 0;

    if (success)
    {
        if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
        {
            SendWantedServerLogout(accountName, uid, serverId);
        }

        if (payStat < 1000 && payStat > 0)
        {
            int session = g_IPSessionDB.DelSessionID(uid);
            if (session > 0)
            {
                g_IPSessionDB.ReleaseSessionRequest(session, clientIP, payStat);
            }
        }

        if (strlen(accountName) >= 2 && sexAndCentury < ForeignerMaleBorn_2000_to_2099)
        {
            Utils::WriteLogD(831, accountName, clientIP, payStat, age, sexAndCentury, 0, 0, uid);
        }
    }

    return success;
}

// 0x004038C3
bool AccountDB::logoutAccount(int uid)
{
    bool success = false;
    SexAndCentury sexAndCentury = FemaleBorn_1800_to_1899;
    int payStat = 0;
    char age = 0;
    int serverId = 0;
    int warnFlag = 0;
    char accName[15];  // fixed, were 16
    in_addr clientIP;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        ::strncpy(accName, user.accountName, sizeof(user.accountName));
        clientIP = user.connectedIP;
        sexAndCentury = user.sexAndCentury;
        payStat = user.payStat;
        age = user.age;
        warnFlag = user.warnFlag;  // FIXED: warnFlag wasn't taken
        serverId = user.loggedGameServerId;
        m_accounts.erase(it);
        m_spinLock.Leave();
        success = true;
    }
    else
    {
        m_spinLock.Leave();
    }

    accName[14] = 0;

    if (success)
    {
        if (g_Config.useWantedSystem && (warnFlag & 4) == 4)
        {
            SendWantedServerLogout(accName, uid, serverId);
        }

        if (payStat < 1000 && payStat > 0)
        {
            int sessionKey = g_IPSessionDB.DelSessionID(uid);
            if (sessionKey)
            {
                g_IPSessionDB.ReleaseSessionRequest(sessionKey, clientIP, payStat);
            }
        }

        if (strlen(accName) >= 2 && sexAndCentury < ForeignerMaleBorn_2000_to_2099)
        {
            Utils::WriteLogD(831, accName, clientIP, payStat, age, sexAndCentury, 0, 0, uid);
        }
    }

    return success;
}

// 0x00403AA1
bool AccountDB::recordGamePlayTime(int uid, int serverId)
{
    bool success = false;
    HANDLE timer = NULL;
    char accName[15];
    memset(accName, 0, sizeof(accName));
    SexAndCentury sexAndCentury = FemaleBorn_1800_to_1899;
    int payStat = 0;
    char age = 0;
    int warnFlag = 0;
    in_addr connectedIP;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        user.loginTime = std::time(0);
        user.loggedGameServerId = serverId;
        user.loginState = LoginState_LoggedToGS;
        timer = user.timerHandler;
        user.timerHandler = 0;
        user.selectedGServerId = 0;
        ::strncpy(accName, user.accountName, sizeof(user.accountName));
        connectedIP = user.connectedIP;
        sexAndCentury = user.sexAndCentury;
        payStat = user.payStat;
        age = user.age;
        warnFlag = user.warnFlag;
        success = true;
    }
    m_spinLock.Leave();

    if (payStat < 1000 && payStat > 0 && success)
    {
        g_IPSessionDB.ConfirmIPCharge(uid, connectedIP, payStat, serverId);
    }

    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, NULL);
    }

    if (success)
    {
        ::InterlockedIncrement(&g_reporter.loggedUsers);
        Utils::WriteLogD(802, accName, connectedIP, payStat, age, sexAndCentury, 0, g_reporter.loggedUsers, uid);
    }

    if (g_SocketWanted != NULL && g_Config.useWantedSystem && (warnFlag & 4) == 4 && success)
    {
        char buffer[28];
        memset(buffer, 0, sizeof(buffer));
        if (g_Config.gameID == 8)
        {
            buffer[0] = 2;
        }

        memcpy(&buffer[1], &uid, 4u);
        ::strncpy(&buffer[5], accName, 14u);
        buffer[19] = serverId;
        time_t now = std::time(0);
        memcpy(&buffer[20], &now, 4u);
        memcpy(&buffer[24], &connectedIP, 4u);
        if (g_Config.useWantedSystem && !CWantedSocket::isReconnecting)
        {
            CWantedSocket::s_lock.ReadLock();
            g_SocketWanted->AddRef();
            g_SocketWanted->Send("cb", 0, 28, sizeof(buffer));
            g_SocketWanted->ReleaseRef();
            CWantedSocket::s_lock.ReadUnlock();
        }
    }

    return success;
}

// 0x00403E28
bool AccountDB::quitGamePlay(int uid, int serverId)
{
    bool success = false;
    HANDLE phNewTimer = NULL;
    if (!g_Config.oneTimeLogOut)
    {
        ::CreateTimerQueueTimer(&phNewTimer, NULL, &AccountDB::TimerRoutine, (PVOID)uid, g_Config.socketTimeOut, 0, 0);
    }

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    LoginUser quitUser;
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        if ((user.loggedGameServerId == serverId) || (serverId == 0))
        {
            quitUser = user;
            user.loginState = LoginState_Connected;
            user.lastworld = user.loggedGameServerId;
            user.loggedGameServerId = 0;
            user.timerHandler = phNewTimer;
            success = true;
        }
        m_spinLock.Leave();
    }
    else
    {
        m_spinLock.Leave();
    }

    if (success)
    {
        quitUser.accountName[14] = 0;
        if ((quitUser.warnFlag & 4) == 4)
        {
            SendWantedServerLogout(quitUser.accountName, uid, quitUser.loggedGameServerId);
        }

        if (quitUser.loggedGameServerId > 0)
        {
            AccountDB::RecordLogout(uid, quitUser.loginTime, quitUser.loggedGameServerId, quitUser.connectedIP, g_Config.gameID, quitUser.accountName, quitUser.payStat, quitUser.birthdayEncoded, quitUser.restOfSsn, quitUser.sexAndCentury, quitUser.age, quitUser.clientCookie);
        }
    }

    return success;
}

// 0x00404A52
PlayFail AccountDB::AboutToPlay(int uid, char* accName, int totalTime, int loginFlag, int warnFlag, int sessionKey, CAuthSocket* gameClient, signed int serverId, int payStat)
{
    if (!ServersProvider::GetServerStatus(serverId) && gameClient != NULL)
    {
        gameClient->Send("cc", LS_PlayFail, PLAY_FAIL_GS_ERROR);
        return PLAY_FAIL_GS_ERROR;
    }

    bool success = false;
    if (g_Config.userData)
    {
        char userData[16];
        memset(userData, 0, sizeof(userData));
        CDBConn sql(&g_linDB);
        SQLINTEGER len = 0;
        ::SQLBindCol(sql.getHandler(), 1u, SQL_C_BINARY, userData, sizeof(userData), &len);

        sql.Execute("SELECT user_data FROM user_data with (nolock) WHERE  uid = %d", uid);
        sql.Fetch(NULL);

        if (serverId <= g_serverList.m_serverNumber && serverId >= 1)
        {
            WorldServer worldServer = g_serverList.GetAt(serverId);
            success = WorldSrvServer::SendSocket(worldServer.ipAddress, "cdsdddbd", 0, uid, accName, totalTime, loginFlag, warnFlag, sizeof(userData), userData, payStat);
        }
    }
    else if (serverId <= g_serverList.m_serverNumber && serverId >= 1)
    {
        WorldServer worldServer = g_serverList.GetAt(serverId);
        success = WorldSrvServer::SendSocket(worldServer.ipAddress, "cdsdddd", 0, uid, accName, totalTime, loginFlag, warnFlag, payStat);
    }

    if (gameClient != NULL)
    {
        if (success)
        {
            PlayFail errorCode = UpdateSocket(uid, gameClient->getSocket(), sessionKey, serverId);
            if (errorCode != PLAY_FAIL_NO_ERROR)
            {
                gameClient->Send("cc", LS_PlayFail, errorCode);
                return errorCode;
            }
        }
        else
        {
            gameClient->Send("cc", LS_PlayFail, PLAY_FAIL_GS_ERROR);
            return PLAY_FAIL_GS_ERROR;
        }
    }

    return PLAY_FAIL_NO_ERROR;
}

// 0x00404C7A
bool AccountDB::GetAccountInfo(int uid, char* accountName, int* loginFlag, int* warnFlag, int* sessionKey, SOCKET* socket)
{
    bool success = false;
    HANDLE timer = 0;

    m_spinLock.Enter();
    UserMap::iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        LoginUser& user = it->second;
        ::strncpy(accountName, user.accountName, sizeof(user.accountName));
        *loginFlag = user.loginFlag;
        *warnFlag = user.warnFlag;
        *socket = user.gameSocket;
        *sessionKey = user.sessionKey;
        timer = user.timerHandler;
        user.timerHandler = NULL;
        success = true;
    }
    m_spinLock.Leave();

    if (timer != NULL)
    {
        ::DeleteTimerQueueTimer(NULL, timer, NULL);
    }

    return success;
}

// 0x00404D70
bool AccountDB::GetAccountInfoForIPStop(int uid, char* accName, int* payStat, in_addr* connectedIP, time_t* loginTime) const
{
    bool success = false;
    m_spinLock.Enter();
    UserMap::const_iterator it = m_accounts.find(uid);
    if (it != m_accounts.end())
    {
        const LoginUser& user = it->second;
        ::strncpy(accName, user.accountName, sizeof(user.accountName));
        *payStat = user.payStat;
        *connectedIP = user.connectedIP;
        *loginTime = user.loginTime;
        m_spinLock.Leave();
        success = true;
    }
    else
    {
        m_spinLock.Leave();
    }

    return success;
}

// 0x00404E2B
bool AccountDB::RegAccountByServer(const LoginUser& user, int uid)
{
    m_spinLock.Enter();
    std::pair<UserMap::iterator, bool> insertResult = m_accounts.insert(std::make_pair(uid, user));
    bool newUser = insertResult.second;
    m_spinLock.Leave();
    if (!newUser)
    {
        KickAccount(uid, REASON_ACCOUNT_IN_USE, true);
    }

    return newUser;
}

// 0x00404E9A
int AccountDB::GetCharNum(int uid)
{
    int userCharNumber = 0;
    CDBConn sql(&g_linDB);
    sql.bindInt(&userCharNumber);
    if (sql.Execute("SELECT user_char_num FROM user_data with (nolock) WHERE uid = %d", uid))
    {
        bool notFound = false;
        sql.Fetch(&notFound);
    }
    else
    {
        userCharNumber = -1;
    }

    return userCharNumber;
}

// 0x00404F3E
bool AccountDB::CreateChar(int uid, int charId, int serverId)
{
    CDBConn sql(&g_linDB);
    if (sql.Execute("INSERT INTO user_char ( uid, world_id, char_id ) values ( %d, %d, %d)", uid, serverId, charId))
    {
        sql.Execute("update user_data set user_char_num = user_char_num + 1 WHERE uid = %d", uid);
        return true;
    }

    return false;
}

// 0x00404FDC
bool AccountDB::DelChar(int uid, int charId, int serverId)
{
    CDBConn sql(&g_linDB);
    SQLINTEGER len1 = 0;
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &uid, 0, &len1);

    SQLINTEGER len2 = 0;
    sql.bindParam(2u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &charId, 0, &len2);

    SQLINTEGER len3 = 0;
    sql.bindParam(3u, SQL_PARAM_INPUT, SQL_C_TINYINT, SQL_TINYINT, 0, 0, &serverId, 0, &len3);
    char sqlQuery[0x100];
    sprintf(sqlQuery, "{CALL dbo.ap_delchar (?,?,?) }");

    SQLRETURN sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
    if (sqlResult == SQL_SUCCESS || sqlResult == SQL_SUCCESS_WITH_INFO)
    {
        return true;
    }

    return false;
}

// 0x004050DC
bool AccountDB::UpdateCharLocation(int uid, int oldCharId, char oldServerId, int newCharId, int newServerId)
{
    CDBConn sql(&g_linDB);
    if (sql.Execute(
            "UPDATE user_char set char_id=%d, world_id=%d WHERE uid =%d and char_id = %d and "
            "world_id = %d",
            newCharId,
            newServerId,
            uid,
            oldCharId,
            oldServerId))
    {
        return true;
    }

    return false;
}

// 0x0040F5B0
int AccountDB::GetUserNum() const
{
    m_spinLock.Enter();
    int size = m_accounts.size();
    m_spinLock.Leave();
    return size;
}

// 0x004047E1
LoginFailReason AccountDB::UserTimeLogin(unsigned int uid, LoginUser* /*userInfo*/, int* userTime)
{
    LoginFailReason errorCode = CheckUserTime(uid, userTime);
    if (errorCode != REASON_SUCCESS)
    {
        errorCode = REASON_NO_TIME_LEFT;  // TODO: buggy, getUserGameTime() might return other reason
    }
    return errorCode;
}

// 0x0040492F
LoginFailReason AccountDB::CheckUserTime(unsigned int uid, int* userTime)
{
    char sqlQuery[0x100];

    LoginFailReason errorCode = REASON_SUCCESS;
    CDBConn sql(&g_linDB);

    SQLINTEGER len1 = 0;
    sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &uid, 0, &len1);
    SQLINTEGER len2 = 0;
    sql.bindParam(2u, SQL_PARAM_OUTPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, userTime, 0, &len2);

    sprintf(sqlQuery, "{CALL dbo.ap_GUserTime (?,?) }");
    SQLRETURN sqlRes = sql.execRaw(sqlQuery, SQL_NTS);
    if (sqlRes != SQL_SUCCESS)
    {
        errorCode = REASON_SYSTEM_ERROR_LOGIN_LATER;
    }
    else
    {
        bool notFound = true;
        sql.Fetch(&notFound);
        if (*userTime <= 0)
        {
            errorCode = REASON_NO_TIME_LEFT;
        }
    }

    sql.ResetHtmt();

    return errorCode;
}

// 0x0040418D
bool AccountDB::RecordLogout(int uid, time_t loginTime, int lastWorld, in_addr userIp, int gameType, char* accountName, int payStat, int birthdayEncoded, int restOfSsn, SexAndCentury sexAndCentury, int age, int someClientCookie)
{
    char lastIP[16];
    sprintf(lastIP, "%d.%d.%d.%d", userIp.S_un.S_un_b.s_b1, userIp.S_un.S_un_b.s_b2, userIp.S_un.S_un_b.s_b3, userIp.S_un.S_un_b.s_b4);

    const time_t now = std::time(0);
    int usedTime = int(now - loginTime);
    Utils::WriteLogD(804, accountName, userIp, payStat, age, sexAndCentury, 0, usedTime, uid);

    tm timeNow = *std::localtime(&now);
    TIMESTAMP_STRUCT logoutTime;
    logoutTime.year = short(timeNow.tm_year) + 1900;
    logoutTime.month = short(timeNow.tm_mon) + 1;
    logoutTime.day = timeNow.tm_mday;
    logoutTime.hour = timeNow.tm_hour;
    logoutTime.minute = timeNow.tm_min;
    logoutTime.second = timeNow.tm_sec;
    logoutTime.fraction = 0;

    tm timeLogin = *std::localtime(&loginTime);
    TIMESTAMP_STRUCT lastLogin;
    lastLogin.year = short(timeLogin.tm_year) + 1900;
    lastLogin.month = short(timeLogin.tm_mon) + 1;
    lastLogin.day = timeLogin.tm_mday;
    lastLogin.hour = timeLogin.tm_hour;
    lastLogin.minute = timeLogin.tm_min;
    lastLogin.second = timeLogin.tm_sec;
    lastLogin.fraction = 0;

    {
        CDBConn sql(&g_linDB);

        SQLINTEGER len1 = 0;
        sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &uid, 0, &len1);

        SQLINTEGER len2 = 0;
        sql.bindParam(2u, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_TIMESTAMP_LEN, 0, &lastLogin, 0, &len2);

        SQLINTEGER len3 = 0;
        sql.bindParam(3u, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_TIMESTAMP_LEN, 0, &logoutTime, 0, &len3);

        SQLINTEGER len4 = 0;
        sql.bindParam(4u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &gameType, 0, &len4);

        SQLINTEGER len5 = 0;
        sql.bindParam(5u, SQL_PARAM_INPUT, SQL_C_UTINYINT, SQL_TINYINT, 0, 0, &lastWorld, 0, &len5);

        SQLINTEGER len6 = SQL_NTS;
        int ipLength = strlen(lastIP);
        sql.bindParam(6u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 15, 0, lastIP, ipLength, &len6);

        char sqlQuery[256];
        sprintf(sqlQuery, "{CALL dbo.ap_SLog (?,?,?,?,?,?) }");
        SQLRETURN sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
        if (sqlResult != SQL_SUCCESS)
        {
            sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
            sql.ResetHtmt();
        }
        else
        {
            sql.ResetHtmt();
        }
    }

    g_fileLog.AddLog(
        LOG_INF,
        "%d-%d-%d %d:%d:%d,%d-%d-%d %d:%d:%d,%s,%d,%s,%d,%d,%d,%06d%07d,%d,%d,%d,%d\r\n",
        timeNow.tm_year + 1900,
        timeNow.tm_mon + 1,
        timeNow.tm_mday,
        timeNow.tm_hour,
        timeNow.tm_min,
        timeNow.tm_sec,
        timeLogin.tm_year + 1900,
        timeLogin.tm_mon + 1,
        timeLogin.tm_mday,
        timeLogin.tm_hour,
        timeLogin.tm_min,
        timeLogin.tm_sec,
        accountName,
        lastWorld,
        lastIP,
        payStat,
        usedTime,
        usedTime,
        birthdayEncoded,
        restOfSsn,
        sexAndCentury,
        timeNow.tm_wday,
        age,
        someClientCookie);

    int payFlag = payStat % 1000 / 100;

    if (payStat >= 1000 || payStat <= 0)
    {
        if (payFlag == 2)
        {
            CDBConn sql(&g_linDB);
            SQLINTEGER len1 = 0;
            sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &usedTime, 0, &len1);

            SQLINTEGER len2 = 0;
            sql.bindParam(2u, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &uid, 0, &len2);

            char sqlQuery[256];
            sprintf(sqlQuery, "{CALL dbo.ap_SUserTime (?,?) }");
            SQLRETURN sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
            if (sqlResult)
            {
                sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
                sql.ResetHtmt();
            }
            else
            {
                sql.ResetHtmt();
            }
        }
        else if (payFlag == 3)
        {
            CDBConn sql(&g_linDB);
            int accLength = strlen(accountName);
            SQLINTEGER len1 = 0;
            sql.bindParam(1u, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 14u, 0, accountName, accLength, &len1);

            SQLINTEGER len2 = 0;
            sql.bindParam(2u, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_TIMESTAMP_LEN, 0, &lastLogin, 0, &len2);

            SQLINTEGER len3 = 0;
            sql.bindParam(3u, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TIMESTAMP, SQL_TIMESTAMP_LEN, 0, &logoutTime, 0, &len3);

            char sqlQuery[256];
            sprintf(sqlQuery, "{CALL dbo.ap_LogoutWithPoint( ?,?,? )}");
            SQLRETURN sqlResult = sql.execRaw(sqlQuery, SQL_NTS);
            if (sqlResult)
            {
                sql.Error(SQL_HANDLE_STMT, sql.getHandler(), sqlQuery);
                sql.ResetHtmt();
            }
            else
            {
                sql.ResetHtmt();
            }
        }
    }
    else
    {
        g_IPSessionDB.StopIPCharge(uid, userIp, payStat, usedTime, loginTime, lastWorld, accountName);
    }

    ::InterlockedDecrement(&g_reporter.loggedUsers);

    return true;
}
