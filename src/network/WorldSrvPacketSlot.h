#pragma once

#include "threads/CLock.h"

class WorldSrvPacket;
class WorldSrvSocket;

class WorldSrvPacketSlot
{
public:
    WorldSrvPacketSlot();   // 0x0041E0A0
    ~WorldSrvPacketSlot();  // 0x0041E0D0

    union
    {
        WorldSrvPacket* packet;
        WorldSrvSocket* socket;
    } m_data;

    CLock m_lock;
};
