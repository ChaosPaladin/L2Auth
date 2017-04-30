#include "network/PacketUtils.h"
#include "ui/CLog.h"

#include <cstdarg>
#include <windows.h>

namespace PacketUtils
{

const char* StrNChr(const char* str, char c, int len);
int WcsNLen(const unsigned char* str, int len);

int Assemble(uint8_t* buf, int bufLen, const char* format, ...)
{
    // guard(int Assemble(char *buf, int bufLen, const char *format, ...));

    va_list ap;
    va_start(ap, format);
    int len = VAssemble(buf, bufLen, format, ap);
    va_end(ap);
    return len;

    // unguard;
}

const unsigned char* Disassemble(const unsigned char* packet, const char* format, ...)
{
    // guard(const unsigned char *Disassemble(const unsigned char* packet, const char* format,
    // ...));

    const char* f = format;
    va_list ap;
    int t;
    va_start(ap, format);
    while ((t = *format++))
    {
        switch (t)
        {
            case 'c':
            {
                char* cp = va_arg(ap, char*);
                *cp = *packet++;
            }
            break;
            case 'h':
            {
                short* hp = va_arg(ap, short*);
#ifdef _WIN32
                *hp = ((short*)packet)[0];
#else
                *hp = packet[0] + (packet[1] << 8);
#endif
                packet += 2;
            }
            break;
            case 'd':
            {
                int* dp = va_arg(ap, int*);

#ifdef _WIN32  // CPU specific optimization.
                *dp = ((int*)packet)[0];
#else
                *dp = packet[0] + (packet[1] << 8) + (packet[2] << 16) + (packet[3] << 24);
#endif
                packet += 4;
            }
            break;

            case 'f':
            {
                double* dp = va_arg(ap, double*);

#ifdef _WIN32  // CPU specific optimization.
                ::CopyMemory(dp, packet, 8);
/*
    int i1 = *(int*)packet;
    int i2 = *(((int*)packet)+1);
    *(((int*)dp)+0) = i2;
    *(((int*)dp)+1) = i1;
    */
/*
    *dp = ((double *)packet)[0];
    */
#else
                *dp = packet[0] + (packet[1] << 8) + (packet[2] << 16) + (packet[3] << 24) + (packet[4] << 32) + (packet[5] << 40) + (packet[6] << 48) + (packet[7] << 56)
#endif
                packet += 8;
            }
            break;

            case 's':
            {
                int dstLen = va_arg(ap, int);
                char* dst = va_arg(ap, char*);
                strncpy(dst, (char*)packet, dstLen);
                dst[dstLen - 1] = 0;
                unsigned char* end = (unsigned char*)StrNChr((char*)packet, '\0', dstLen) + 1;
                //            if (end - packet > dstLen)
                //            {
                //                g_winlog.Add(LOG_ERR, "too long string in %s", f);
                //            }
                packet = end;
            }
            break;
            case 'S':
            {
                int len = va_arg(ap, int) / sizeof(wchar_t);
                len = WcsNLen(packet, len - 1);
                wchar_t* dst = va_arg(ap, wchar_t*);
                memcpy(dst, packet, len * sizeof(wchar_t));
                dst[len] = 0;
                packet += (len + 1) * sizeof(wchar_t);
            }
            break;
            default:
                g_winlog.AddLog(LOG_ERR, "unknown type %c in %S", t, f);
        }
    }
    va_end(ap);
    return packet;

    // unguard;
}

// 0x0043A690
int VAssemble(uint8_t* buf, int bufLen, const char* format, va_list ap)
{
    // guard(int VAssemble(char* buf, int bufLen, const char* format, va_list ap));

    uint8_t* start = buf;
    uint8_t* end = buf + bufLen;
    int t;
    char* p;
    int len;
    int i;

    double d;

    while ((t = *format++))
    {
        switch (t)
        {
            case 's':
                p = va_arg(ap, char*);
                if (p)
                {
                    len = strlen(p);
                    if (buf + len + 1 > end)
                    {
                        goto overflow;
                    }
                    strcpy((char*)buf, p);
                    buf += len + 1;
                }
                else
                {
                    // p is null. Is it correct?
                    if (buf + 1 > end)
                    {
                        goto overflow;
                    }
                    *buf++ = 0;
                }
                break;
            case 'S':
            {
                wchar_t* p = va_arg(ap, wchar_t*);
                if (p)
                {
                    len = (wcslen(p) + 1) * sizeof(wchar_t);
                    if (buf + len > end)
                    {
                        goto overflow;
                    }
                    memcpy(buf, p, len);
                    buf += len;
                }
                else
                {
                    // p is null. Is it correct?
                    if (buf + 2 > end)
                    {
                        goto overflow;
                    }
                    *buf++ = 0;
                    *buf++ = 0;
                }
            }
            break;
            case 'b':  // blind copy
                len = va_arg(ap, int);
                p = va_arg(ap, char*);
                if (buf + len > end)
                {
                    goto overflow;
                }
                memcpy(buf, p, len);
                buf += len;
                break;
            case 'c':
                i = va_arg(ap, int);
                if (buf + 1 > end)
                {
                    goto overflow;
                }
                *buf++ = i;
                break;
            case 'h':
                i = va_arg(ap, int);
                if (buf + 2 > end)
                {
                    goto overflow;
                }
#ifdef _WIN32
                *((short*)buf) = i;
                buf += 2;
#else
                *buf++ = i;
                *buf++ = i >> 8;
#endif
                break;
            case 'd':
                i = va_arg(ap, int);
                if (buf + 4 > end)
                {
                    goto overflow;
                }
#ifdef _WIN32
                *((int*)buf) = i;
                buf += 4;
#else
                *buf++ = i;
                *buf++ = i >> 8;
                *buf++ = i >> 16;
                *buf++ = i >> 24;
#endif
                break;

            case 'f':
                d = va_arg(ap, double);
                if (buf + 8 > end)
                {
                    goto overflow;
                }
#ifdef _WIN32
                ::CopyMemory(buf, &d, 8);
                /*
                *(((int*)buf)+0) = *(((int*)&d)+0);
                *(((int*)buf)+1) = *(((int*)&d)+1);
                */
                /*
                *((double *)buf) = d;
                */
                buf += 8;
#else
                *buf++ = d;
                *buf++ = d >> 8;
                *buf++ = d >> 16;
                *buf++ = d >> 24;
                *buf++ = d >> 32 * buf++ = d >> 40;
                *buf++ = d >> 48;
                *buf++ = d >> 56;
#endif
                break;
        }
    }
    return (buf - start);
overflow:
    g_winlog.AddLog(LOG_ERR, "Buffer overflow in Assemble");
    return 0;

    // unguard;
}

const char* StrNChr(const char* str, char c, int len)
{
    // guard(const char *StrNChr(const char *str, char c, int len));

    while (len > 0)
    {
        if (*str == c)
        {
            return str;
        }
        str++;
        len--;
    }
    return str;

    // unguard;
}

int WcsNLen(const unsigned char* str, int len)
{
    // guard(int WcsNLen(const unsigned char *str, int len));

    int i;
    for (i = 0; i < len; i++)
    {
        if (!str[0] && !str[1])
        {
            break;
        }
        str += 2;
    }
    return i;

    // unguard;
}
}  // namespace PacketUtils
