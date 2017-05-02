#pragma once

#include <cstdarg>
#include "utils/cstdint_support.h"

namespace PacketUtils
{

int Assemble(uint8_t* buf, int bufLen, const char* format, ...);
int VAssemble(uint8_t* buf, int bufLen, const char* format, va_list ap);  // 0x0043A690
const unsigned char* Disassemble(const unsigned char* packet, const char* format, ...);

}  // namespace PacketUtils
