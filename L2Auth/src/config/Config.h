#pragma once

#include <windows.h>

#include <map>
#include <string>

namespace std
{
class istrstream;
}  // namespace std

class Config
{
public:
    Config();   // 0x0040A309
    ~Config();  // 0x0040A32F

    static void ParsePort(const char* fullCommand, const char* subCommand, const char* paramName, int* valueToChange);  // 0x0040AE2F
    static char* LoadBinary(const char* fileName, size_t* outSize, bool extraZero);                                     // 0x0040A23F

    void Load(const char* fileName);                               // 0x0040A348
    const char* Get(const char* paramName) const;                  // 0x0040AC55
    bool GetBool(const char* paramName, bool defaultValue) const;  // 0x0040ACB4
    int GetInt(const char* paramName, int defaultValue) const;     // 0x0040AD2F
    in_addr GetInetAddr(const char* paramName) const;              // 0x0040AD64
    void ParseCmdLine(const char* command);                        // 0x0040ADA9

private:
    static bool IsDigit(char a1);       // 0x00432330
    static bool IsWhiteSpace(char a1);  // 0x00432360
    static bool IsAlpha(char a1);       // 0x004323A0

    enum Symbol
    {
        Sym_Word = 0x0,
        Sym_Dot = 0x1,
        Sym_Comma = 0x2,
        Sym_UnknownError = 0x3,
        Sym_Colon = 0x4,
        Sym_SemiColon = 0x5,
        Sym_Eqaul = 0x6,
        Sym_Slash = 0x7,
        Sym_Ampersand = 0x8,
        Sym_OpenBracer = 0x9,
        Sym_CloseBracer = 0xA,
        Sym_NewLine = 0xB,
        Sym_Digit = 0xC,
        Sym_EoF = 0xD,
    };

    static Symbol GetToken(std::istrstream& stream, char* token, bool lookForWord);  // 0x00431E60

public:
    int serverPort;
    int serverExPort;
    int serverIntPort;
    int worldPort;
    int DBConnectionNum;
    int numServerThread;
    int numServerIntThread;
    bool encrypt;
    int packetSizeType;
    int protocolVersion;
    bool oneTimeLogOut;
    const char* logDirectory;
    int gameID;
    bool desApply;
    const char* desKey;
    bool readLocalServerList;
    bool GMCheckMode;
    bool userData;
    bool devConnectOuter;
    bool dumpPacket;
    bool pcCafeFirst;
    bool useIPServer;
    const char* devServerIP;
    in_addr IPServer;
    int IPPort;
    int IPInterval;
    int IPAccessLimit;
    int countryCode;
    int socketLimit;
    unsigned int socketTimeOut;
    int acceptCallNum;
    int waitingUserLimit;
    bool useLogD;
    in_addr logDIP;
    int logDPort;
    int logDConnectInterval;
    bool restrictGMIP;
    in_addr gmIP;
    bool useWantedSystem;
    in_addr wantedIP;
    unsigned short wantedPort;
    int wantedReconnectInterval;
    bool freeServer;
    bool useForbiddenIPList;
    bool supportReconnect;
    const char* mailServer;
    const char* mailFrom;
    const char* mailTo;
    bool newServerList;
    bool autoKickAccount;
    bool newEncrypt;

private:
    struct comp
    {
        bool operator()(const std::string& lhs, const std::string& rhs) const;
    };

    typedef std::map<std::string, std::string, comp> Parameters;
    Parameters values;
};

extern Config g_Config;
