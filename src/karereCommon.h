#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <string>
#include <logger.h>
#include <mstrophe.h> //needed for timestampMs()
#include <string.h>

/** @cond PRIVATE */

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

/** @endcond PRIVATE */

namespace karere
{
class Client;
typedef std::map<std::string, std::string> StringMap;

/** @brief Globally initializes the karere library and starts the services
 * subsystem. Must be called before any karere code is used.
 * @param logPath The full path to the log file.
 * @param logSize The rotate size of the log file, in kilobytes. Once the log
 * file reaches this size, its first half is truncated. So the log size at
 * any moment is at least logSize / 2, and at most logSize
 * @param postFunc The function that posts a void* to the application's message loop.
 * See the documentation in gcm.h for details about this function
 * @param options Various flags that modify the behaviour of the karere
 * services subsystem. Normally this is 0
 */
void globalInit(const std::string& logPath, size_t logSize, void(*postFunc)(void*), uint32_t options=0);

/** @brief Stops the karere services susbsystem and frees global resources
 * used by Karere
 */
void globalCleanup();

/** @cond PRIVATE */

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

#define API_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_megachatapi, fmtString, ##__VA_ARGS__)
#define API_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_megachatapi, fmtString, ##__VA_ARGS__)
#define API_LOG_WARNING(fmtString,...)  KARERE_LOG_WARNING(krLogChannel_megachatapi, fmtString, ##__VA_ARGS__)
#define API_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_megachatapi, fmtString, ##__VA_ARGS__)
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

#define KR_EXCEPTION_TO_PROMISE(prefix)        \
    catch(std::exception& e)                   \
    {                                          \
        prefix##_LOG_ERROR("%s() exception: %s", __FUNCTION__, e.what());  \
        return promise::Error(e.what());       \
    }


/** @brief Used to keep track of deletion of a lambda-captured object
  * pointer/reference - the instance may get deleted before the lambda is called
  * e.g. an attribute is fetched
  */
class TrackDelete
{
public:
    struct SharedData
    {
        bool mDeleted = false;
        uint32_t mRefCount = 0;
    };
    class Handle
    {
    protected:
        SharedData* mData;
        Handle(SharedData* shared)
        : mData(shared) { mData->mRefCount++; }
        Handle& operator=(Handle& other) = delete;
    public:
        Handle(const Handle& other): Handle(other.mData){}
        ~Handle()
        {
            if (--(mData->mRefCount) <= 0)
                delete mData;
        }
        bool deleted() const { return mData->mDeleted; }
        void throwIfDeleted() const
        {
            if (mData->mDeleted)
                throw std::runtime_error("TrackDelete: Instance has been deleted");
        }
        friend class TrackDelete;
    };

protected:
    Handle mSharedDataHandle;
public:
    Handle getWeakPtr() const { return Handle(mSharedDataHandle.mData); }
    TrackDelete(): mSharedDataHandle(new SharedData()){}
    ~TrackDelete() { mSharedDataHandle.mData->mDeleted = true; }
};

class Client;
/** @endcond PRIVATE */

/** @brief A logger backend that sends the log output of error messages
 * to a remote server. Can be used by the application to send errors to
 * a remote server.
 *
 *  Usage:
 * \c gLogger.addUserLogger("karere-remote", new RemoteLogger);
 */
class RemoteLogger: public Logger::ILoggerBackend
{
public:
    virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags);
    RemoteLogger(): ILoggerBackend(krLogLevelError){}
};

}

#endif
