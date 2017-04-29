#pragma once

#include "threads/CLock.h"

class CLogPacket;
class CLogSocket;

class CLogPacketSlot
{
public:
    CLogPacketSlot();
    ~CLogPacketSlot();

    union
    {
        CLogPacket* packet;
        CLogSocket* socket;
    } m_data;
    CLock m_lock;
};
