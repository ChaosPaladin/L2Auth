#include "crypt/DesNewCrypt.h"
#include "crypt/DesBackEnd.h"

#include <cstring>

DesNewCrypt g_PwdCrypt;
static keysched pwdkeySched;

DesNewCrypt::DesNewCrypt()
{
}

void DesNewCrypt::DesKeyInit(const char* stringKey)
{
    char compressedKey[8];
    memset(compressedKey, 0, 8u);
    for (int i = 0; *stringKey && i < 40; ++i)
    {
        compressedKey[i % 8] ^= *stringKey++;
    }

    fsetkey(compressedKey, &pwdkeySched);
}

void DesNewCrypt::DesWriteBlock(char* data, int size)
{
    // int trunkatedSize = size;
    // LOBYTE(trunkatedSize) = size & 248;
    int trunkatedSize = size & 0xFFFFFFF8;
    processData(data, trunkatedSize, true);
}

void DesNewCrypt::DesReadBlock(char* data, int size)
{
    // int trunkatedSize = size;
    // LOBYTE(trunkatedSize) = size & 248;
    int trunkatedSize = size & 0xFFFFFFF8;
    processData(data, trunkatedSize, false);
}

void DesNewCrypt::processData(char* data, int size, int encrypt)
{
    char* iterator = data;

    while (size > 0)
    {
        if (encrypt)
        {
            encryptBlock(iterator, iterator);
        }
        else
        {
            decryptBlock(iterator, iterator);
        }
        size -= 8;
        iterator += 8;
    }
}

void DesNewCrypt::encryptBlock(char* out, char* /*in*/)
{
    // TODO: foreign backend! Not native one
    fencrypt(out, 0, &pwdkeySched);
}

void DesNewCrypt::decryptBlock(char* out, char* /*in*/)
{
    // TODO: foreign backend! Not native one
    fencrypt(out, 1, &pwdkeySched);
}
