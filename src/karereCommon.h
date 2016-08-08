#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <string>
#include <logger.h>
#include <mstrophe.h> //needed for timestampMs()
#include <string.h>
//#include <assert.h>

#ifndef KARERE_SHARED
    #define KARERE_EXPORT
    #define KARERE_IMPORT
    #define KARERE_IMPEXP
#else
    #ifdef _WIN32
        #define KARERE_EXPORT __declspec(dllexport)
        #define KARERE_IMPORT __declspec(import)
    #else
        #define KARERE_EXPORT __attribute__ ((visibility("default")))
        #define KARERE_IMPORT KARERE_EXPORT
    #endif
    #ifdef karere_BUILDING
        #define KARERE_IMPEXP KARERE_EXPORT
    #else
       #define KARERE_IMPEXP KARERE_IMPORT
    #endif
#endif
//we need an always-export macro to export the getAppDir() function to the services lib
//Whether we need a dynamic export is determined not by whether we (libkarere) are
//dynamic lib, but by whether libservices is dynamic. TO simplify things, we always
//export this function from the executable
#ifndef _WIN32
    #define APP_ALWAYS_EXPORT __attribute__ ((visibility("default")))
#else
    #define APP_ALWAYS_EXPORT //__declspec(dllexport)
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> //we must never include windows.h before winsock2.h
    #ifdef WIN32_LEAN_AND_MEAN
        #include <mmsystem.h>
    #endif
#endif

namespace karere { class Client; }

#define KARERE_DEFAULT_XMPP_SERVER "xmpp270n001.karere.mega.nz"
#define KARERE_XMPP_DOMAIN "karere.mega.nz"
#define KARERE_LOGIN_TIMEOUT 15000
#define KARERE_RECONNECT_DELAY_MAX 10000
#define KARERE_RECONNECT_DELAY_INITIAL 1000

#define KARERE_DEFAULT_TURN_SERVERS \
   "[{\"host\":\"turn:trn270n001.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn270n002.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn530n001.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn530n002.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn302n001.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn302n002.karere.mega.nz:3478?transport=udp\"}]"

#define KARERE_TURN_USERNAME "inoo20jdnH"
#define KARERE_TURN_PASSWORD "02nNKDBkkS"

#define KARERE_FALLBACK_XMPP_SERVERS "[\
{\"host\":\"xmpp270n001.karere.mega.nz\",\"port\":443}]"

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
typedef xmpp_ts Ts;
static inline Ts timestampMs()
{
    //use strophe's timestamp function
    return xmpp_time_stamp();
}
struct AvFlags
{
    bool audio;
    bool video;
    AvFlags(bool a, bool v): audio(a), video(v){}
    AvFlags(){}
    AvFlags(const char* str)
        :audio(strchr(str, 'a') != nullptr), video(strchr(str,'v') != nullptr){}
    bool operator==(AvFlags other) { return (audio == other.audio) && (video == other.video); }
    bool operator!=(AvFlags other) { return (audio != other.audio) || (video != other.video); }
    bool any() const { return audio || video; }
    std::string toString() const
    {
        std::string result;
        if (audio)
            result+='a';
        if (video)
            result+='v';
        if (result.empty())
            result='-';
        return result;
    }
};

extern const char* gKarereDbSchema;

//logging stuff
//#define KR_LINE KR_LOG(LINE)
//#define KR_LINE_END KR_LOG(LINE)
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

#define GUI_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_gui, fmtString, ##__VA_ARGS__)
#define GUI_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_gui, fmtString, ##__VA_ARGS__)
#define GUI_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_gui, fmtString, ##__VA_ARGS__)

#define JINGLE_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_jingle, fmtString, ##__VA_ARGS__)
/*
#ifndef NDEBUG
#undef assert
#define assert(cond) do { \
    if (!(cond)) { \
        KR_LOG_ERROR("Assertion failed: '%s', aborting application", #cond); \
        abort(); \
    } \
} while(0)
#endif
*/
#define KR_THROW_IF_FALSE(statement) do {\
    if (!(statement)) {\
        throw std::runtime_error(std::string("Karere: ")+#statement+" failed (returned false)\nAt file "+__FILE__+":"+std::to_string(__LINE__)); \
     } \
 } while(0)

class Client;
//extern std::unique_ptr<Client> gClient;

class RemoteLogger: public Logger::ILoggerBackend
{
public:
    virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags);
    RemoteLogger(): ILoggerBackend(krLogLevelError){}
};

}


#endif
