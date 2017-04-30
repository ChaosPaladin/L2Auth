#pragma once

class DesFrontEnd
{
public:
    DesFrontEnd();

    static void DesKeyInit(const char* stringKey);    // 0x00415670
    static void DesReadBlock(char* data, int size);   // 0x00415657
    static void DesWriteBlock(char* data, int size);  // 0x00414B30

private:
    static void processData(char* data, int size, int encrypt);
    static void encryptBlock(char* out, char* in);
    static void decryptBlock(char* out, char* in);
};
