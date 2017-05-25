#pragma once

#include "utils/cstdint_support.h"

namespace BufferReader
{
int8_t ReadByte(uint8_t** buffer);                       // 0x004114B0
int16_t ReadShort(uint8_t** buffer);                     // 0x004114E0
int32_t ReadInt(uint8_t** buffer);                       // 0x00411480
uint32_t ReadUInt(uint8_t** buffer);                     // 0x00411530
void ReadString(uint8_t** buffer, int size, char* out);  // 0x00411560

}  // namespace BufferReader
