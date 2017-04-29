#include "crypt/DesBackEnd.h"
#include "crypt/DesFrontEnd.h"

#include <cstring>

static keysched keySched;

void DesFrontEnd::DesKeyInit(const char* stringKey)
{
    char compressedKey[8];
    memset(compressedKey, 0, 8u);
    for (int i = 0; *stringKey && i < 40; ++i)
    {
        compressedKey[i % 8] ^= *stringKey++;
    }

    fsetkey(compressedKey, &keySched);
}

void DesFrontEnd::DesReadBlock(char* data, int size)
{
    // int trunkatedSize = size;
    // LOBYTE(trunkatedSize) = size & 248;
    int trunkatedSize = size & 0xFFFFFFF8;
    DesFrontEnd::processData(data, trunkatedSize, false);
}

void DesFrontEnd::DesWriteBlock(char* data, int size)
{
    // int trunkatedSize = size;
    // LOBYTE(trunkatedSize) = size & 248;
    int trunkatedSize = size & 0xFFFFFFF8;
    DesFrontEnd::processData(data, trunkatedSize, true);
}

void DesFrontEnd::processData(char* data, int size, int encrypt)
{
    char* iterator = data;

    while (size > 0)
    {
        if (encrypt)
        {
            DesFrontEnd::encryptBlock(iterator, iterator);
        }
        else
        {
            DesFrontEnd::decryptBlock(iterator, iterator);
        }
        size -= 8;
        iterator += 8;
    }
}

void DesFrontEnd::encryptBlock(char* out, char* /*in*/)
{
    // TODO: foreign backend! Not native one
    fencrypt(out, 0, &keySched);
}

void DesFrontEnd::decryptBlock(char* out, char* /*in*/)
{
    // TODO: foreign backend! Not native one
    fencrypt(out, 1, &keySched);
}
