#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <map>
#include <string>

//defines the name of the webrtc adapter layer namespace
#define MEGA_RTCADAPTER_NS rtc
//defines the name of the karere webrtc+jingle module namespace
#define KARERE_RTCMODULE_NS rtcModule

namespace karere
{
typedef std::map<std::string, std::string> StringMap;

//time function
typedef int64_t ts;

#ifdef _WIN32
//We need to have these static vars extern in order to have the functions static inlined
extern xmpp_ts _stropheLastTimeValue;
extern xmpp_ts _stropheTimeBase;

static inline xmpp_ts time_stamp()
{
    xmpp_ts now = timeGetTime();
    if (now < _stropheLastTimeValue)
        _stropheTimeBase+=0xFFFFFFFF;
    _stropheLastTimeValue = now;
    now += _stropheTimeBase;
    return now;
}
#elif defined(_POSIX_MONOTONIC_CLOCK)
static inline xmpp_ts time_stamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (xmpp_ts)ts.tv_sec*1000 + (xmpp_ts)ts.tv_nsec / 1000000;
}
#elif defined (__MACH__)
void _strophe_init_mac_timefunc();
static inline xmpp_ts time_stamp()
{
    return (double)mach_absolute_time() * _stropheTimeConversionFactor;
}
#else
static inline xmpp_ts time_stamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (xmpp_ts)tv.tv_sec * 1000 + (xmpp_ts)tv.tv_usec / 1000;
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
