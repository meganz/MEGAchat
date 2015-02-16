#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <map>
#include <string>
#include <time.h>
#include <string.h>
#include <base/cservices.h> //needed for isatty_xxx

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> //we must never include windows.h before winsock2.h
    #ifdef WIN32_LEAN_AND_MEAN
        #include <mmsystem.h>
    #endif
#else
    #include <sys/time.h>
#endif

#define KARERE_DEFAULT_XMPP_SERVER "xmpp270n001.karere.mega.nz"
#define KARERE_XMPP_DOMAIN "karere.mega.nz"

#define KARERE_DEFAULT_TURN_SERVERS "url=turn:trn530n003.karere.mega.nz:3478?transport=udp, user=inoo20jdnH, pass=02nNKDBkkS"
#ifdef __ANDROID__
//Android is missing std::to_string
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

extern int isatty_stdout;
extern int isatty_stderr;
static inline const char* colorOn(const char* escape) { return isatty_stdout?escape:""; }
static inline const char* colorOff() { return isatty_stdout?"\e[0m":"";  }

}

#define KR_LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__)
#define KR_LOG_COLOR(color, fmtString,...) printf("\e[" #color "m" fmtString "\e[0m\n", ##__VA_ARGS__)

#define KR_LOG_DEBUG(fmtString,...) KR_LOG("debug: " fmtString, ##__VA_ARGS__)
#define KR_LOG_WARNING(fmtString,...)    do { \
   if (karere::isatty_stdout) \
      KR_LOG_COLOR(33, "WARNING: " fmtString, ##__VA_ARGS__); \
    else \
      KR_LOG("WARNING: " fmtString, ##__VA_ARGS__); \
    } while (0)

#define KR_LOG_ERROR(fmtString,...) do { \
    if (karere::isatty_stdout) \
      KR_LOG_COLOR(31;1, "ERROR: " fmtString, ##__VA_ARGS__); \
    else \
      KR_LOG("ERROR: " fmtString, ##__VA_ARGS__); \
      } while(0)

#define KR_THROW_IF_FALSE(statement) do {\
    if (!(statement)) {\
        throw std::runtime_error(std::string("Karere: ")+#statement+" failed (returned false)\nAt file "+__FILE__+":"+std::to_string(__LINE__)); \
     } \
 } while(0)

#endif
