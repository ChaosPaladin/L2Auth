#pragma once

#include <windows.h>

class CAuthOverlappedSlot;

class CAuthOverlapped : public OVERLAPPED
{
public:
    static CAuthOverlapped* Alloc();  // 0x00418CE9
    static void FreeAll();            // 0x00418DD8

    void Free();  // 0x00418D8B

public:
    SOCKET acceptSocket;
    char outputBuffer[256];

private:
    CAuthOverlappedSlot* m_pNext;  // m_pNext
};
