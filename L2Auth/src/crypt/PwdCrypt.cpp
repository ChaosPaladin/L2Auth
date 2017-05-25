#include "crypt/PwdCrypt.h"

#include <cstring>
#include "utils/cstdint_support.h"

#define SLOBYTE(x) (*((int8_t*)&(x)))

// Generates a 16-byte hash from a 16-byte string.
// This is how the password will be stored in the database.
void PwdCrypt::EncPwdL2(char* password)  // 0x0043AB0C
{
    char temp = 0;
    int tempHash[4];
    char hash[16];

    memcpy(&tempHash[0], password, 16);

    tempHash[0] = tempHash[0] * 0x3407f + 0x269735;
    tempHash[1] = tempHash[1] * 0x340ff + 0x269741;
    tempHash[2] = tempHash[2] * 0x340d3 + 0x269935;
    tempHash[3] = tempHash[3] * 0x3433d + 0x269ACD;

    memcpy(hash, &tempHash[0], 16);

    for (int i = 0; i < 16; i++)
    {
        temp = temp ^ hash[i] ^ password[i];
        if (temp == 0)
        {
            hash[i] = 0x66;
        }
        else
        {
            hash[i] = temp;
        }
    }

    memcpy(password, hash, 16);
}

void PwdCrypt::EncPwdShalo(char* password)  // 0x0043AA2B
{
    char temp = 0;
    int tempHash[4];
    char hash[16];

    memcpy(&tempHash[0], password, 16);

    tempHash[0] = tempHash[0] * 0x343FD + 0x269735;
    tempHash[1] = tempHash[1] * 0x34127 + 0x269741;
    tempHash[2] = tempHash[2] * 0x33FFB + 0x269935;
    tempHash[3] = tempHash[3] * 0x33B1D + 0x269ACD;

    memcpy(hash, &tempHash[0], 16);

    for (int i = 0; i < 16; i++)
    {
        temp = temp ^ hash[i] ^ password[i];
        if (temp == 0)
        {
            hash[i] = 0x66;
        }
        else
        {
            hash[i] = temp;
        }
    }

    memcpy(password, hash, 16);
}

void PwdCrypt::EncPwdDev(char*)  // 0x0043ABED
{
}
