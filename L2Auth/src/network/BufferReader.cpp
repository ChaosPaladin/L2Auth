#include "network/BufferReader.h"

namespace
{
// 0x0043A9D8
int LNStrNCpy(char* out, const uint8_t* in, signed int size);
}

namespace BufferReader
{

// 0x004114B0
int8_t ReadByte(uint8_t** buffer)
{
    return *(*buffer)++;
}

// 0x004114E0
int16_t ReadShort(uint8_t** buffer)
{
    int16_t* result = (int16_t*)*buffer;
    *buffer += 2;
    return *result;
}

// 0x00411480
int32_t ReadInt(uint8_t** buffer)
{
    int32_t* result = (int32_t*)*buffer;
    *buffer += 4;
    return *result;
}

// 0x00411530
uint32_t ReadUInt(uint8_t** buffer)
{
    uint32_t* result = (uint32_t*)*buffer;
    *buffer += 4;
    return *result;
}

// 0x00411560
void ReadString(uint8_t** buffer, int size, char* out)
{
    uint8_t* end = &(*buffer)[LNStrNCpy(out, *buffer, size)];
    *buffer = end;
}

}  // namespace BufferReader

namespace
{

// 0x0043A9D8
int LNStrNCpy(char* out, const uint8_t* in, signed int size)
{
    int readSize = 1;
    while (*in != '\0')
    {
        *out = *in++;
        if (++readSize > size)
        {
            break;
        }
        ++out;
    }
    *out = '\0';
    return readSize;
}
}
