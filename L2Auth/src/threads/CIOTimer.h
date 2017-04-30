#pragma once

class CIOObject;

class CIOTimer
{
public:
    CIOTimer(CIOObject* object, unsigned int delay);  // 0x0042C480

    bool operator<(const CIOTimer& rTimer) const;  // 0x0042C4B0

public:
    int m_dwTime;
    CIOObject* m_pObject;
};
