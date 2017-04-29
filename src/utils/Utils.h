#pragma once

#include <Sqltypes.h>
#include <windows.h>

#include <cstdint>
#include <ctime>

namespace Utils
{
// Capitalizes acc name
int StdAccount(char* accName);                                                                                               // 0x0043ADF4
void WriteLogD(int type, char* accName, in_addr connectedIP, int payStat, int age, int a6, int zero, int variant, int uid);  // 0x0043AFA2
int AnsiToUnicode(const char* multiByteStr, int length, wchar_t* wideCharStr);                                               // 0x0043B6A8
bool CheckAccount(char* accName);                                                                                            // 0x0043B878
bool IsValidNumeric(char* str, int size);                                                                                    // 0x0043B9BA
bool CheckDiskSpace(LPCSTR lpDirectoryName, uint64_t limit);                                                                 // 0x0043BA17
time_t ConvertSQLToTome(const TIMESTAMP_STRUCT& timeStamp, tm* tmStruct);                                                    // 0x0043B7FE
const char* getFileName(const char* str);                                                                                    // 0x0041618B

}  // namespace Utils
