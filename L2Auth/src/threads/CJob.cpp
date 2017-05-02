#include "threads/CJob.h"
#include "CIOObject.h"
#include "threads/CIOTopTimer.h"
#include "utils/CExceptionInit.h"

CJob g_job;

// 0x0042A9F7
CJob::CJob()
    : m_timerInstance(new CIOTopTimer())
    , m_lock(LockType_CritSection, 0)
    , m_nextTimer(::GetTickCount() + 24 * 60 * 60 * 1000)
    , m_event1()
    , m_manualResetEvent()
    , m_events()
    , m_clients()
    , m_timerQueue()
    , m_stopThread(false)
    , m_isCompleted()
{
    m_event1 = ::CreateEventA(0, 0, 0, 0);
    m_manualResetEvent = ::CreateEventA(0, 1, 0, 0);

    CIOTimer timer = CIOTimer(m_timerInstance, m_nextTimer);
    m_timerQueue.push(timer);

    m_events.push_back(m_manualResetEvent);
    m_clients.push_back(m_timerInstance);
}

// 0x0042AB4A
CJob::~CJob()
{
    ::CloseHandle(m_event1);
    ::CloseHandle(m_manualResetEvent);
}

// 0x0042A5FC
void CJob::RunTimer()
{
    auth_guard;

    HANDLE Handles[2];
    Handles[0] = m_manualResetEvent;
    Handles[1] = m_event1;
    do
    {
        DWORD now = ::GetTickCount();
        int dwMilliseconds = m_nextTimer - now;
        if (dwMilliseconds <= 0)
        {
            m_lock.Enter();
            CIOTimer timer = m_timerQueue.top();
            for (CIOObject* obj = timer.m_pObject;; obj = timer.m_pObject)
            {
                m_timerQueue.pop();
                obj->OnTimerCallback();
                const CIOTimer& next = m_timerQueue.top();
                timer.m_pObject = next.m_pObject;
                timer.m_dwTime = next.m_dwTime;
                dwMilliseconds = timer.m_dwTime - now;
                if (dwMilliseconds > 0)
                {
                    break;
                }
            }
            m_nextTimer = timer.m_dwTime;
            m_lock.Leave();
        }
        ::WaitForMultipleObjects(2u, Handles, 0, dwMilliseconds);
    } while (!m_stopThread);

    m_lock.Enter();
    while (!m_timerQueue.empty())
    {
        const CIOTimer& timer = m_timerQueue.top();
        timer.m_pObject->ReleaseRef();
        m_timerQueue.pop();
    }
    m_lock.Leave();

    auth_vunguard;
}

// 0x0042A7A4
void CJob::RunEvent()
{
    auth_guard;

    while (true)
    {
        const HANDLE& first = m_events.front();
        size_t size = m_events.size();
        DWORD signaledIndex = ::WaitForMultipleObjects(size, &first, false, INFINITE);
        if (m_stopThread)
        {
            break;
        }

        if (signaledIndex < m_events.size())
        {
            CIOObject* object = m_clients[signaledIndex];
            object->OnEventCallback();
        }
    }

    clean();

    auth_vunguard;
}

// 0x0042A8C8
bool CJob::PushTimer(CIOObject* timerInstance, unsigned int delay, int id)
{
    DWORD nextTimeout = ::GetTickCount() + delay;
    m_lock.Enter();
    if (m_stopThread)
    {
        m_lock.Leave();
        return false;
    }

    m_timerQueue.push(CIOTimer(timerInstance, nextTimeout));
    if ((m_nextTimer - nextTimeout) <= 0)
    {
        m_lock.Leave();
    }
    else
    {
        m_nextTimer = nextTimeout;
        m_lock.Leave();
        if (id != 0)
        {
            ::SetEvent(m_event1);
        }
    }

    return true;
}

// 0x0042A96A
bool CJob::PushEvent(HANDLE event, CIOObject* client)
{
    m_lock.Enter();
    if (m_stopThread)
    {
        m_lock.Leave();
        return false;
    }

    if (m_events.size() < 64)
    {
        m_events.push_back(event);
        m_clients.push_back(client);
        m_lock.Leave();
        return true;
    }

    m_lock.Leave();
    return false;
}

// 0x0042ABD0
void CJob::SetTerminate()
{
    m_stopThread = true;
    ::SetEvent(m_manualResetEvent);
    ::SetEvent(m_event1);
}

void CJob::clean()
{
    for (std::vector<CIOObject*>::iterator i = m_clients.begin(); ; i = m_clients.erase(i))
    {
        if (i == m_clients.end())
        {
            break;
        }
    }
}
