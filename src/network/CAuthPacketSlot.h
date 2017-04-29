#pragma once

#include "threads/CLock.h"

class CAuthPacket;
class CAuthSocket;

class CAuthPacketSlot
{
public:
    CAuthPacketSlot();  // 0x00431D90
    ~CAuthPacketSlot();

    union
    {
        CAuthPacket* packet;
        CAuthSocket* socket;
    } m_data;
    CLock m_lock;
};
