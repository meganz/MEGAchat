#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <map>
#include <string>
#include <time.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> //we must never include windows.h before winsock2.h
    #ifdef WIN32_LEAN_AND_MEAN
        #include <mmsystem.h>
    #endif
#else
    #include <sys/time.h>
#endif
//defines the name of the webrtc adapter layer namespace
#define MEGA_RTCADAPTER_NS rtc
//defines the name of the karere webrtc+jingle module namespace
#define KARERE_RTCMODULE_NS rtcModule

namespace karere
{
typedef std::map<std::string, std::string> StringMap;

//time function
typedef int64_t Ts;

#ifdef _WIN32
//We need to have these static vars extern in order to have the functions static inlined
extern Ts _gLastTimeValue;
extern Ts _gTimeBase;

static inline Ts timestampMs()
{
    Ts now = timeGetTime();
    if (now < _gLastTimeValue)
        _gTimeBase+=0xFFFFFFFF;
    _gLastTimeValue = now;
    now += _gTimeBase;
    return now;
}
#elif defined(_POSIX_MONOTONIC_CLOCK)
static inline Ts timestampMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (Ts)ts.tv_sec*1000 + (Ts)ts.tv_nsec / 1000000;
}
#elif defined (__MACH__)
void _init_mac_timefunc();
static inline Ts timestampMs()
{
    return (double)mach_absolute_time() * _gTimeConversionFactor;
}
#else
static inline Ts timestampMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (Ts)tv.tv_sec * 1000 + (Ts)tv.tv_usec / 1000;
}
#endif

}

#define KR_LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__)
#define KR_LOG_DEBUG(fmtString,...) KR_LOG("debug: " fmtString, ##__VA_ARGS__)
#define KR_LOG_WARNING(fmtString,...) KR_LOG("WARNING: " fmtString, ##__VA_ARGS__)
#define KR_LOG_ERROR(fmtString,...) KR_LOG("ERROR: " fmtString, ##__VA_ARGS__)
#define KR_THROW_IF_FALSE(statement) \
 if (!(statement)) {\
    throw std::runtime_error(std::string("Karere: ")+#statement+" failed (returned false)\nAt file "+__FILE__+":"+std::to_string(__LINE__)); \
 }
#endif // KARERECOMMON_H
