#include "config/Config.h"
#include "ui/CLog.h"
#include "utils/Unused.h"

#include <cstdint>
#include <iostream>
#include <strstream>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

Config g_Config = Config{};

// 0x0040A309
Config::Config()
{
}

// 0x0040A32F
Config::~Config()
{
}

// 0x0040A23F
char* Config::LoadBinary(const char* fileName, size_t* outSize, bool extraZero)
{
    int file = _open(fileName, _O_BINARY /*  0x8000*/, 0);
    if (file >= 0)
    {
        struct _stat fileInfo;
        _stat(fileName, &fileInfo);
        _off_t fileSize = fileInfo.st_size;
        if (extraZero)
        {
            fileSize = fileInfo.st_size + 1;
        }
        char* buffer = new char[fileSize];
        if (buffer)
        {
            _read(file, buffer, fileInfo.st_size);
            _close(file);
            if (outSize)
            {
                *outSize = fileInfo.st_size;
            }
            if (extraZero)
            {
                buffer[fileInfo.st_size] = 0;
            }
            return buffer;
        }

        _close(file);
        return NULL;
    }
    else
    {
        return NULL;
    }
}

enum ParserState
{
    ParserState_NewLine = 0x0,
    ParserState_ParameterFound = 0x2,
    ParserState_Comment = 0x3,
};

// 0x0040A348
void Config::Load(const char* fileName)
{
    size_t outSize = 0;
    const char* buffer = LoadBinary(fileName, &outSize, false);
    if (buffer)
    {
        std::istrstream reader(buffer, outSize);
        reader >> std::noskipws;

        int lineNumber = 1;
        ParserState state = ParserState_NewLine;
        char token[72];
        char paramName[72];
        while (1)
        {
            Symbol read_result = GetToken(reader, token, 1);
            if (read_result == Sym_EoF)
            {
                break;
            }
            if (state == ParserState_Comment)
            {
                // don't change state, untill meet end of commented line
                if (read_result == Sym_NewLine)
                {
                    state = ParserState_NewLine;
                }
            }
            else
            {
                switch (read_result)
                {
                    case Sym_Word:
                        if (state)
                        {
                            if (state == ParserState_ParameterFound)
                            {
                                std::string configValue(token);
                                std::string configKey(paramName);
                                const std::pair<Parameters::iterator, bool> insertResult = values.insert(std::make_pair(configKey, configValue));
                                if (!insertResult.second)
                                {
                                    g_winlog.AddLog(LOG_ERR, "Insert twice %s at line %d in %s", paramName, lineNumber, fileName);
                                }
                            }
                            else if (state != ParserState_Comment)
                            {
                                g_winlog.AddLog(LOG_ERR, "Invalid state at line %d in %s", lineNumber, fileName);
                            }
                        }
                        else
                        {
                            strcpy(paramName, token);
                        }
                        break;
                    case Sym_Eqaul:
                        state = ParserState_ParameterFound;
                        break;
                    case Sym_SemiColon:
                        state = ParserState_Comment;
                        break;
                    case Sym_NewLine:
                        state = ParserState_NewLine;
                        ++lineNumber;
                        break;
                    default:
                        if (state != ParserState_Comment)
                            g_winlog.AddLog(LOG_ERR, "Invalid token %s at line %d in %s", token, lineNumber, fileName);
                        break;
                }
            }
        }
        delete[] buffer;
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "Can't load %s", fileName);
    }

    serverPort = GetInt("serverPort", 0);
    serverExPort = GetInt("serverExPort", 0);
    serverIntPort = GetInt("serverIntPort", 0);
    worldPort = GetInt("WorldPort", 0);
    DBConnectionNum = GetInt("DBConnectionNum", 0);
    numServerThread = GetInt("numServerThread", 0);
    numServerIntThread = GetInt("numServerIntThread", 0);
    gameID = GetInt("GameID", 0);
    socketTimeOut = 1000 * GetInt("SocketTimeOut", 0);
    socketLimit = GetInt("SocketLimit", 0);
    acceptCallNum = GetInt("AcceptCallNum", 0);
    waitingUserLimit = GetInt("WaitingUserLimit", 0);
    packetSizeType = GetInt("PacketSizeType", 0);
    encrypt = GetBool("Encrypt", false);
    logDirectory = Get("logDirectory");
    protocolVersion = GetInt("ProtocolVersion", 0);
    desApply = GetBool("DesApply", false);
    desKey = Get("deskey");
    readLocalServerList = GetBool("ReadLocalServerList", false);
    oneTimeLogOut = GetBool("OneTimeLogOut", false);
    GMCheckMode = GetBool("GMCheckMode", false);
    countryCode = GetInt("CountryCode", 0);
    userData = GetBool("UserData", false);
    devConnectOuter = GetBool("DevConnectOuter", false);
    devServerIP = Get("DevServerIP");
    dumpPacket = GetBool("DumpPacket", false);
    pcCafeFirst = GetBool("PCCafeFirst", false);
    useIPServer = GetBool("UseIPServer", false);
    IPServer = GetInetAddr("IPServer");
    IPPort = GetInt("IPPort", 0);
    IPInterval = 1000 * GetInt("IPInterval", 0);
    useLogD = GetBool("uselogd", false);
    logDIP = GetInetAddr("logdip");
    logDPort = GetInt("logdport", 0);
    logDConnectInterval = 1000 * GetInt("logdconnectinterval", 60);
    restrictGMIP = GetBool("RestrictGMIP", false);
    gmIP = GetInetAddr("GMIP");
    useWantedSystem = GetBool("UseWantedSystem", false);
    wantedIP = GetInetAddr("WantedIP");
    wantedPort = GetInt("WantedPort", 2122);
    wantedReconnectInterval = 1000 * GetInt("WantedReconnectInterval", 1800);
    IPAccessLimit = GetInt("IPAccessLimit", 0);
    freeServer = GetBool("FreeServer", false);
    useForbiddenIPList = GetBool("useForbiddenIPList", false);
    supportReconnect = GetBool("supportReconnect", false);
    mailServer = Get("mailServer");
    mailFrom = Get("mailFrom");
    mailTo = Get("mailTo");
    newServerList = GetBool("NewServerList", false);
    autoKickAccount = GetBool("AutoKickAccount", false);
    newEncrypt = GetBool("NewEncrypt", false);
}

// 0x00431E60
Config::Symbol Config::GetToken(std::istrstream& stream, char* token, bool lookForWord)
{
    Symbol result;
    int index;
    const char* initToken = token;
    UNUSED(initToken);

    if (stream.eof())
    {
        return Sym_EoF;
    }

    uint8_t symbol;
    do
    {
        stream >> symbol;
    } while (IsWhiteSpace(symbol) && !stream.eof());

    if (stream.eof())
    {
        return Sym_EoF;
    }

    switch (symbol)
    {
        case '.':
            return Sym_Dot;
        case ',':
            return Sym_Comma;
        case ':':
            return Sym_Colon;
        case ';':
            return Sym_SemiColon;
        case '=':
            return Sym_Eqaul;
        case '/':
            return Sym_Slash;
        case '&':
            return Sym_Ampersand;
        case '(':
            return Sym_OpenBracer;
        case ')':
            return Sym_CloseBracer;
        case '\n':
            return Sym_NewLine;
        default:
            index = 0;
    }

    if (symbol == '-')  // negatives or just a '-' symbol
    {
        stream >> symbol;
        if (Config::IsDigit(symbol))
        {
            *token = '-';
            ++token;
            index = 1;
        }
        else
        {
            stream.putback(symbol);
            return Sym_UnknownError;
        }
    }

    if (Config::IsDigit(symbol))
    {
        do
        {
            *token++ = symbol;
            ++index;
            stream >> symbol;
        } while (Config::IsDigit(symbol) && index < 'F' && !stream.eof());

        if (!stream.eof())
        {
            stream.putback(symbol);
        }

        *token = '\0';
        if (lookForWord)
        {
            result = Sym_Word;
        }
        else
        {
            result = Sym_Digit;
        }
    }
    else if (symbol == '"')
    {
        stream >> symbol;
        while (symbol != '"' && symbol != '\n' && index < 'F' && !stream.eof())
        {
            *token++ = symbol;
            ++index;
            stream >> symbol;
        }
        if (symbol == '\n')
        {
            stream.putback(symbol);
        }

        *token = '\0';
        result = Sym_Word;
    }
    else if (Config::IsAlpha(symbol) || (symbol >= 128 && symbol < 254)) // ascii extended
    {
        do
        {
            *token++ = symbol;
            ++index;
            stream >> symbol;
        } while ((Config::IsAlpha(symbol) || (symbol >= 128 && symbol < 254) || symbol == '_') && !stream.eof());

        if (!stream.eof())
        {
            stream.putback(symbol);
            if (stream.bad())
            {
                g_winlog.AddLog(LOG_ERR, "putback");
            }
        }
        *token = '\0';
        result = Sym_Word;
    }
    else
    {
        g_winlog.AddLog(LOG_ERR, "unexpected char '%c'", symbol);
        result = Sym_EoF;
    }

    return result;
}

// 0x00432330
bool Config::IsDigit(char a1)
{
    return a1 >= '0' && a1 <= '9';
}

// 0x00432360
bool Config::IsWhiteSpace(char a1)
{
    return a1 == ' ' || a1 == '\t' || a1 == '\r';
}

// 0x004323A0
bool Config::IsAlpha(char a1)
{
    return a1 >= 'a' && a1 <= 'z' || a1 >= 'A' && a1 <= 'Z';
}

// 0x0040AC55
const char* Config::Get(const char* paramName) const
{
    const Parameters::const_iterator it = values.find(paramName);
    if (it != values.end())
    {
        return it->second.c_str();
    }
    return NULL;
}

// 0x0040ACB4
bool Config::GetBool(const char* paramName, bool defaultValue) const
{
    const char* value = Get(paramName);
    if (value)
    {
        return !_strcmpi(value, "yes") || !_strcmpi(value, "true") || !_strcmpi(value, "1");
    }
    return defaultValue;
}

// 0x0040AD2F
int Config::GetInt(const char* paramName, int defaultValue) const
{
    const char* value = Get(paramName);
    if (value)
    {
        return atoi(value);
    }
    return defaultValue;
}

// 0x0040AD64
in_addr Config::GetInetAddr(const char* paramName) const
{
    const char* value = Get(paramName);
    if (value != NULL)
    {
        in_addr addr;
        addr.s_addr = inet_addr(value);
        return addr;
    }
    return in_addr{0};
}

// 0x0040ADA9
void Config::ParseCmdLine(const char* command)
{
    ParsePort(command, "-serverport=", "serverport", &this->serverPort);
    ParsePort(command, "-serverexport=", "serverexport", &this->serverExPort);
    ParsePort(command, "-serverintport=", "serverintport", &this->serverIntPort);
    ParsePort(command, "-worldport=", "worldport", &this->worldPort);
}

// 0x0040AE2F
void Config::ParsePort(const char* fullCommand, const char* subCommand, const char* paramName, int* valueToChange)
{
    const char* result = strstr(fullCommand, subCommand);
    if (result)
    {
        const char* newValue = &result[strlen(subCommand)];
        if (*newValue >= '0' && *newValue <= '9')
        {
            *valueToChange = atoi(newValue);
            g_winlog.AddLog(LOG_DBG, "Editor Mode, %s=%d\n", paramName, *valueToChange);
        }
    }
}

bool Config::comp::operator()(const std::string& lhs, const std::string& rhs) const
{
    return ::_stricmp(lhs.c_str(), rhs.c_str()) < 0;
}
