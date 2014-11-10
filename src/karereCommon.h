#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <map>
#include <string>
#include <time.h>
#include <string.h>

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
#define MEGA_RTCADAPTER_NS artc
//defines the name of the karere webrtc+jingle module namespace
#define KARERE_RTCMODULE_NS rtcModule
#define KR_CHECK_NULLARG(name) \
    do { \
      if (!(name))\
        throw std::runtime_error(std::string(__FUNCTION__)+": Assertion failed: Argument '"+#name+"' is NULL"); \
    } while(0)

namespace karere
{
typedef std::map<std::string, std::string> StringMap;

//audio&video enabled flags used in many places
struct AvFlags
{
    bool audio = false;
    bool video = false;
};

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
#define KR_THROW_IF_FALSE(statement) do {\
    if (!(statement)) {\
        throw std::runtime_error(std::string("Karere: ")+#statement+" failed (returned false)\nAt file "+__FILE__+":"+std::to_string(__LINE__)); \
     } \
 } while(0)

template<class Cont>
void tokenize(const char* src, const char* delims, Cont& cont)
{
    const char* start = src;
    for (;;)
    {
        for(;;)
        {
            if (!*start)
                return; //reached end of string
            if (!strchr(delims, *start)) //skip leading delims
                break;
            start++;
        }
        const char* pos = start+1;
        //at this point it is guaranteed that pos is not a delim and is not at the end of the string
        for(;;)
        {
            if (!*pos)
            {
                cont.emplace_back(start, pos-start);
                return;
            }

            if (strchr(delims, *pos))
            {
                cont.emplace_back(start, pos-start);
                start = pos+1;
                break;
            }
            pos++;
        }
    }
}

inline static size_t trim(const std::string& str, size_t tstart, size_t tend,
                        size_t& start)
{
    if (tstart >= str.size())
        return 0;
    if (tend >= str.size())
    {
        tend = str.size() - 1;
        if (tend < tstart)
            return 0;
    }
    start = str.find_first_not_of(" \t", tstart);
    if ((start > tend) || (start == std::string::npos))
        return 0;
    size_t end = str.find_last_not_of(" \t", tend); //guaranteed to be in range because we have a non-whitespace char at start
    return end - start + 1;
}
static size_t findFirstOf(const std::string& str, const char* chars, size_t start, size_t end)
{
    for (size_t i=start; i<end; i++)
        if(strchr(chars, str[i]))
            return i;
    return std::string::npos;
}
static size_t findFirstNotOf(const std::string& str, const char* chars, size_t start, size_t end)
{
    for (size_t i=start; i<end; i++)
        if(!strchr(chars, str[i]))
            return i;
    return std::string::npos;
}

enum {kTokEnableComments=1};

template <class Cont>
inline static void parseNameValues(const char* str, const char* pairDelims, char nvDelim,
                                   Cont& cont,  int flags=0)
{
    std::vector<std::string> pairs;
    tokenize(str, pairDelims, pairs);
    for (auto& pair: pairs)
    {
        size_t eq = pair.find(nvDelim);
        if ((eq == std::string::npos) || (eq == 0))
            throw std::runtime_error("parseNameValues: No name-value delimiter in line '"+pair+"'");
        size_t pstart;
        size_t plen = trim(pair, 0, pair.size()-1, pstart);
        if (!plen)
            continue; //empty line
        if (pstart >= eq) // only == should be possible
            throw std::runtime_error("parseNameValues: No value name in line '"+pair+"'");
        if ((flags & kTokEnableComments) && (pair[pstart] == '#'))
            continue;
        size_t nend = findFirstOf(pair, " \t", pstart+1, eq);
        if (nend == std::string::npos)
            nend = eq;
        size_t vstart = findFirstNotOf(pair, "\t ", eq+1, plen);
        cont.emplace(make_pair(std::string(pair.c_str()+pstart, nend-pstart),
          (vstart!=std::string::npos)?std::string(pair.c_str()+vstart, pstart+plen-vstart):""));
    }
}

#endif // KARERECOMMON_H
