#pragma once

#include <queue>
#include <vector>
#include "threads/CIOTimer.h"
#include "threads/CLock.h"

class CIOObject;
class CIOTopTimer;

class CJob
{
public:
    CJob();   // 0x0042A9F7
    ~CJob();  // 0x0042AB4A

    void RunTimer();                                                         // 0x0042A5FC
    void RunEvent();                                                         // 0x0042A7A4
    bool PushTimer(CIOObject* m_timerInstance, unsigned int delay, int id);  // 0x0042A8C8
    bool PushEvent(HANDLE event, CIOObject* client);                         // 0x0042A96A
    void SetTerminate();                                                     // 0x0042ABD0

private:
    void clean();

private:
    CIOTopTimer* m_timerInstance;
    CLock m_lock;
    int m_nextTimer;
    HANDLE m_event1;
    HANDLE m_manualResetEvent;
    std::vector<HANDLE> m_events;
    std::vector<CIOObject*> m_clients;
    std::priority_queue<CIOTimer> m_timerQueue;
    bool m_stopThread;
    int m_isCompleted;
};

extern CJob g_job;
