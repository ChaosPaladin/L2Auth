#include "config/CIPList.h"
#include "ui/CLog.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>

CIPList g_blockedIPs;

// 0x0041E1DF
CIPList::CIPList()
    : m_addresses()
    , m_lock()
{
}

// 0x0041EE60
CIPList::~CIPList()
{
}

// 0x0041ECDF
bool CIPList::IpExists(in_addr ipAddress) const
{
    m_lock.ReadLock();

    IPRecord range;
    range.startAddress = _byteswap_ulong(ipAddress.s_addr);

    // lower_bounds looking for endAddress, so it checks is ipAddress in one of the range
    // TODO: upper_bound in origin code + change operator<
    std::vector<IPRecord>::const_iterator found = std::lower_bound(m_addresses.begin(), m_addresses.end(), range);
    bool result = (found != m_addresses.end()) && (found->startAddress <= range.startAddress);

    m_lock.ReadUnlock();

    return result;
}

// 0x0041E5DD
bool CIPList::Load(const char* fileName)
{
    std::ifstream reader(fileName, std::ifstream::in);
    if (reader.fail())
    {
        return false;
    }

    m_lock.WriteLock();
    m_addresses.clear();

    char buffer[1024];
    int readLine = 0;
    while (true)
    {
        reader.getline(buffer, sizeof(buffer));
        ++readLine;

        if (reader.fail())
        {
            break;
        }

        IPRecord ipRange;
        char* buff_it = buffer;

        int ip_1 = ::strtol(buffer, &buff_it, 10);
        if (*buff_it == '.')
        {
            ++buff_it;
            int ip_2 = ::strtol(buff_it, &buff_it, 10);
            if (*buff_it == '.')
            {
                ++buff_it;
                int ip_3 = ::strtol(buff_it, &buff_it, 10);

                if (*buff_it == '.')
                {
                    ++buff_it;
                    int ip_4 = ::strtol(buff_it, &buff_it, 10);
                    ipRange.startAddress = ip_4 | (ip_3 << 8) | (ip_2 << 16) | (ip_1 << 24);
                    int nextWhiteSpaceIndex = ::strspn(buff_it, " \t");

                    buff_it += nextWhiteSpaceIndex;

                    if (*buff_it != '-')
                    {
                        if (ip_4)
                        {
                            ipRange.endAddress = ipRange.startAddress + 1;
                        }
                        else
                        {
                            if (ip_3)
                                ipRange.endAddress = ipRange.startAddress + 0x100;
                            else
                                ipRange.endAddress = ipRange.startAddress + 0x10000;
                        }
                        goto LABEL_20;
                    }

                    nextWhiteSpaceIndex = ::strspn(++buff_it, " \t");
                    buff_it += nextWhiteSpaceIndex;
                    ip_1 = ::strtol(buff_it, &buff_it, 10);

                    if (*buff_it == '.')
                    {
                        ++buff_it;
                        ip_2 = ::strtol(buff_it, &buff_it, 10);
                        if (*buff_it == '.')
                        {
                            ++buff_it;
                            ip_3 = ::strtol(buff_it, &buff_it, 10);

                            if (*buff_it == '.')
                            {
                                ++buff_it;
                                ip_4 = ::strtol(buff_it, &buff_it, 10);
                                ipRange.endAddress = (ip_4 | (ip_3 << 8) | (ip_2 << 16) | (ip_1 << 24)) + 1;
                            LABEL_20:
                                if (ipRange.startAddress < ipRange.endAddress)
                                {
                                    IPRecord range;
                                    range.startAddress = ipRange.startAddress - 1;

                                    std::vector<IPRecord>::iterator first = std::lower_bound(m_addresses.begin(), m_addresses.end(), range);
                                    if ((first == m_addresses.end()) || (first->startAddress > ipRange.startAddress - 1))
                                    {
                                    LABEL_28:
                                        while (true)
                                        {
                                            if (first == m_addresses.end() || first->startAddress > ipRange.endAddress)
                                            {
                                                break;
                                            }

                                            if (first->startAddress != ipRange.endAddress)
                                            {
                                                g_winlog.AddLog(
                                                    LOG_ERR,
                                                    "CIPList: address is merged with "
                                                    "%d.%d.%d.%d-%d.%d.%d.%d at line %d",
                                                    (first->startAddress >> 16) >> 16,
                                                    (first->startAddress >> 16) & 0xFF,
                                                    (first->startAddress >> 8) & 0xFF,
                                                    first->startAddress & 0xFF,
                                                    ((first->endAddress - 1) >> 16) >> 16,
                                                    ((first->endAddress - 1) >> 16) & 0xFF,
                                                    ((first->endAddress - 1) >> 8) & 0xFF,
                                                    (first->endAddress - 1),
                                                    readLine);
                                            }

                                            if (ipRange.endAddress < first->endAddress)
                                            {
                                                ipRange.endAddress = first->endAddress;
                                            }

                                            first = m_addresses.erase(first);
                                        }
                                        m_addresses.insert(first, ipRange);
                                    }
                                    else
                                    {
                                        if (first->endAddress != ipRange.startAddress)
                                        {
                                            g_winlog.AddLog(
                                                LOG_ERR,
                                                "CIPList: address is merged with "
                                                "%d.%d.%d.%d-%d.%d.%d.%d at line %d",
                                                (first->startAddress >> 16) >> 16,
                                                (first->startAddress >> 16) & 0xFF,
                                                (first->startAddress >> 8) & 0xFF,
                                                first->startAddress & 0xFF,
                                                ((first->endAddress - 1) >> 16) >> 16,
                                                ((first->endAddress - 1) >> 16) & 0xFF,
                                                ((first->endAddress - 1) >> 8) & 0xFF,
                                                (first->endAddress - 1),
                                                readLine);
                                        }

                                        if (first->endAddress < ipRange.endAddress)
                                        {
                                            ipRange.startAddress = first->startAddress;
                                            first = m_addresses.erase(first);
                                            goto LABEL_28;
                                        }
                                    }
                                }
                                else
                                {
                                    g_winlog.AddLog(
                                        LOG_ERR,
                                        "CIPList: start address address is greater than end "
                                        "address at line %d",
                                        readLine);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    m_lock.WriteUnlock();

    g_winlog.AddLog(LOG_INF, "CIPList loaded from %s", fileName);
    return true;
}

bool CIPList::IPRecord::operator<(const CIPList::IPRecord& other) const
{
    return endAddress < other.startAddress;
    // return other.startAddress < endAddress ;
}
