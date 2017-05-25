#pragma once

#include "utils/cstdint_support.h"

class OldCrypt
{
public:
    static bool decrypt_75FE(uint8_t* data, char* xorKey, int size);   // 0x0043ABF2 : Decrypt
    static bool encrypt_75FE(uint8_t* data, char* xorKey, int* size);  // 0x0043ACA0 : Encrypt
};
