#pragma once

#include "CIOObject.h"

class CIOBuffer;

class CIOSocket : public CIOObject
{
public:
    CIOSocket(SOCKET socket);  // 0x0041D4D0
    ~CIOSocket();              // 0x0041D583

    void OnIOCallback(BOOL bSuccess, DWORD dwTransferred, LPOVERLAPPED lpOverlapped) override;  // 0x0041D738

    void Read(size_t dwLeft);     // 0x0041D995
    void Write(CIOBuffer* buff);  // 0x0041DB1A

    void CloseSocket();                              // 0x0041D61F
    void Initialize(HANDLE existingCompletionPort);  // 0x0041D687

    SOCKET getSocket() const
    {
        return m_hSocket;
    }

protected:
    virtual void OnClose();   // 0x0041D614
    virtual void OnCreate();  // 0x0041D722
    virtual void OnRead() = 0;
    virtual void OnReadCallback(int dwTransferred);   // 0x0041D7A9
    virtual void OnWriteCallback(int dwTransferred);  // 0x0041D80B

protected:
    CRITICAL_SECTION m_lock;
    OVERLAPPED m_overlappedRead;
    OVERLAPPED m_overlappedWrite;
    CIOBuffer* m_pReadBuf;
    CIOBuffer* m_pFirstBuf;
    CIOBuffer* m_pLastBuf;
    int m_nPendingWrite;
    SOCKET m_hSocket;
};
