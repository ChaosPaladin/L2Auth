#pragma once

#include "model/FrameType.h"
#include "model/WorldServer.h"
#include "threads/CRWLock.h"

#include "utils/cstdint_support.h"
#include <vector>

class CServerList
{
public:
    CServerList();           // 0x004337BE
    virtual ~CServerList();  // 0x0043387B

    int GetMaxServerNum() const;               // 0x00411590
    int GetAgeLimit() const;                   // 0x0043A0A0
    WorldServer GetAt(int serverId) const;     // 0x00434524
    bool GetServerStatus(int serverId) const;  // 0x00434D97

    void SetServerUserNum(int outerServerIP, uint16_t usersOnline, uint16_t usersLimit, int serverId);  // 0x00434659

    void ReloadServer();  // 0x00433921

    int SetServerSocket(int ipAddress, SOCKET socket);  // 0x004345A7
    void SetServerStatus(int serverId, bool status);    // 0x00434CD9

private:
    void CheckAllServerStatus();  // 0x00434D36
    void MakeServerListFrame1();  // 0x004346C7
    void MakeServerListFrame2();  // 0x00434848
    void MakeServerListFrame3();  // 0x00434A4F

public:
    int m_serverCount;
    int m_worldPort;
    int m_maxAgeLimit;
    mutable CRWLock m_rwLock;
    std::vector<WorldServer> m_servers;
    WorldServer m_tmpServer;
    int m_defaultOrCompressedSize;
    char* m_frameLists[10];
    int m_frameSizes[10];
    char m_compressedFrameBuffers[2048];
    int m_serverNumber;
};

extern CServerList g_serverList;
