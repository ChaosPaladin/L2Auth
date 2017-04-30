#pragma once

#include "threads/CLock.h"

class IPPacket;
class IPSocket;

class IPPacketSlot
{
public:
    IPPacketSlot();
    ~IPPacketSlot();

    union
    {
        IPPacket* packet;
        IPSocket* socket;
    } m_data;
    CLock m_lock;
};
