#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <map>
#include <string>
#include <time.h>
#include <string.h>
#include <logger.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> //we must never include windows.h before winsock2.h
    #ifdef WIN32_LEAN_AND_MEAN
        #include <mmsystem.h>
    #endif
#elif defined (__MACH__)
    #include <mach/mach_time.h>
#else
    #include <sys/time.h>
#endif

#define KARERE_DEFAULT_XMPP_SERVER "xmpp270n001.karere.mega.nz"
#define KARERE_XMPP_DOMAIN "karere.mega.nz"
#define KARERE_LOGIN_TIMEOUT 10000
#define KARERE_RECONNECT_DELAY_MAX 10000
#define KARERE_RECONNECT_DELAY_INITIAL 1000

#define KARERE_DEFAULT_TURN_SERVERS "url=turn:trn530n003.karere.mega.nz:3478?transport=udp, user=inoo20jdnH, pass=02nNKDBkkS"
#if defined(__ANDROID__) && !defined(HAVE_STD_TO_STRING)
//Android is missing std::to_string
#define HAVE_STD_TO_STRING 1
#include <sstream>

namespace std
{
template<typename T>
static inline string to_string(const T& t)
{
    ostringstream os;
    os << t;
    return os.str();
}
}
#endif

#define KR_CHECK_NULLARG(name) \
    do { \
      if (!(name))\
        throw std::runtime_error(std::string(__FUNCTION__)+": Assertion failed: Argument '"+#name+"' is NULL"); \
    } while(0)

#define LINE     "==================================== mpenc stuff ==================================="
#define LINE_END "===================================================================================="
namespace karere
{


///////////// MPENC TEMP SIGNING KEYS //////////////////////

// while waiting for the addition of code for the keys to be added to the
// SDK/client, we are using hard-coded keys.

static const unsigned char PUB_KEY[32] = {20, 122, 218,  85, 160, 200,   4, 178,
        54,  71, 120, 167, 152,  18,  92, 104,
       114, 167, 231, 210, 198,  30,  82, 154,
       107, 244,  82,  27, 105, 132,  57, 135 };

static const unsigned char SEC_KEY[64] = {165,  20,  21, 140,  82,  46,  73,  10,
        108, 212, 186,  39,  71,  31, 119, 135,
        155,   1, 255,  38, 139, 184,  68, 223,
         70,  18, 206, 232, 186, 165,  69, 225,
         20, 122, 218,  85, 160, 200,   4, 178,
         54,  71, 120, 167, 152,  18,  92, 104,
        114, 167, 231, 210, 198,  30,  82, 154,
        107, 244,  82,  27, 105, 132,  57, 135,};

////////////////////////////////////////////////////////////

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
#elif !defined(__MACH__) && defined(_POSIX_MONOTONIC_CLOCK) //macos defines _POSIX_MONOTONIC_CLOCK to -1 but does not implement it
static inline Ts timestampMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (Ts)ts.tv_sec*1000 + (Ts)ts.tv_nsec / 1000000;
}
#elif defined (__MACH__)
extern double _gTimeConversionFactor;
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


#define KR_LINE KR_LOG(LINE)
#define KR_LINE_END KR_LOG(LINE)
#define MPENC_HEADER "?mpENCv1?"
#define MP_LOG(fmt, b) KR_LOG(LINE); \
                  KR_LOG(fmt, b); \
                  KR_LOG(LINE);

#define KR_LOG_RTC_EVENT(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtcevent, fmtString, ##__VA_ARGS__)
#define KR_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_default, fmtString, ##__VA_ARGS__)
#define KR_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_default, fmtString, ##__VA_ARGS__)
#define KR_LOG_WARNING(fmtString,...)  KARERE_LOG_WARNING(krLogChannel_default, fmtString, ##__VA_ARGS__)
#define KR_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_default, fmtString, ##__VA_ARGS__)

#define CHAT_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_textchat, fmtString, ##__VA_ARGS__)
#define CHAT_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_textchat, fmtString, ##__VA_ARGS__)
#define CHAT_LOG_WARNING(fmtString,...)  KARERE_LOG_WARNING(krLogChannel_textchat, fmtString, ##__VA_ARGS__)
#define CHAT_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_textchat, fmtString, ##__VA_ARGS__)

#define JINGLE_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_jingle, fmtString, ##__VA_ARGS__)

#define KR_THROW_IF_FALSE(statement) do {\
    if (!(statement)) {\
        throw std::runtime_error(std::string("Karere: ")+#statement+" failed (returned false)\nAt file "+__FILE__+":"+std::to_string(__LINE__)); \
     } \
 } while(0)

#endif
