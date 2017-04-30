#pragma once

class DesNewCrypt  // CDes
{
public:
    DesNewCrypt();

    void DesKeyInit(const char* stringKey);    // 0x0040A10F
    void DesWriteBlock(char* data, int size);  // 0x0040A07D
    void DesReadBlock(char* data, int size);   // 0x0040A0C6

private:
    void processData(char* data, int size, int encrypt);
    void encryptBlock(char* out, char* in);
    void decryptBlock(char* out, char* in);
};

extern DesNewCrypt g_PwdCrypt;
