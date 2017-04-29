#include "crypt/OldCrypt.h"
#include "config/Config.h"

// 0x0043ABF2
bool OldCrypt::decrypt_75FE(uint8_t* data, char* xorKey, int size)
{
    if (g_Config.encrypt)
    {
        char current = *data;
        *data ^= *xorKey;
        for (int i = 1; i < size; ++i)
        {
            char tmp = data[i];
            data[i] ^= xorKey[i & 7] ^ current;
            current = tmp;
        }

        int64_t* xorVal = reinterpret_cast<int64_t*>(xorKey);
        *xorVal += size;
        return true;
    }

    return true;
}

// 0x0043ACA0
bool OldCrypt::encrypt_75FE(uint8_t* data, char* xorKey, int* size)
{
    if (g_Config.encrypt)
    {
        *data ^= *xorKey;
        for (int i = 1; i < *size; ++i)
        {
            data[i] ^= xorKey[i & 7] ^ data[i - 1];
        }

        int64_t* xorVal = reinterpret_cast<int64_t*>(xorKey);
        *xorVal += *size;
    }

    return true;
}
