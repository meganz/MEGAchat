#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <string>
#include <logger.h>
#include <cservices.h> //needed for timestampMs()
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

#define KARERE_LOGIN_TIMEOUT 15000
#define KARERE_RECONNECT_DELAY_INITIAL 1000
#define KARERE_RECONNECT_DELAY_MAX 5000

#define KARERE_DEFAULT_TURN_SERVERS \
   "[{\"host\":\"turn:trn270n001.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn302n001.karere.mega.nz:3478?transport=udp\"}," \
    "{\"host\":\"turn:trn530n001.karere.mega.nz:3478?transport=udp\"}]"

#define KARERE_TURN_USERNAME "inoo20jdnH"
#define KARERE_TURN_PASSWORD "02nNKDBkkS"

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

class MyMegaApi;

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
void globalInit(void(*postFunc)(void*, void*), uint32_t options=0, const char* logPath=nullptr, size_t logSize=0);

/** @brief Stops the karere services susbsystem and frees global resources
 * used by Karere
 */
void globalCleanup();

/** @cond PRIVATE */

struct AvFlags
{
protected:
    uint8_t mFlags;
public:
    enum: uint8_t { kAudio = 1, kVideo = 2 };
    AvFlags(uint8_t flags): mFlags(flags){}
    AvFlags(bool audio, bool video)
    : mFlags((audio ? kAudio : 0) | (video ? kVideo : 0)){}
    AvFlags(): mFlags(0){}
    uint8_t value() const { return mFlags; }
    void set(uint8_t val) { mFlags = val; }
    bool audio() const { return mFlags & kAudio; }
    bool video() const { return mFlags & kVideo; }
    bool operator==(AvFlags other) { return (mFlags == other.mFlags); }
    bool operator!=(AvFlags other) { return (mFlags != other.mFlags); }
    bool any() const { return mFlags != 0; }
    operator bool() const { return mFlags != 0; }
    std::string toString() const
    {
        std::string result;
        if (mFlags & kAudio)
            result+='a';
        if (mFlags & kVideo)
            result+='v';
        if (result.empty())
            result='-';
        return result;
    }
    void setAudio(bool enable)
    {
        mFlags = ((enable ? kAudio : 0) | (video() ? kVideo : 0));
    }
    void setVideo(bool enable)
    {
        mFlags = ((audio() ? kAudio : 0) | (enable ? kVideo : 0));
    }
};

/** @brief Client capability flags. There are defined by the presenced protocol */
//defined here and not in karere::Client to avoid presenced.cpp depending on chatClient.cpp
enum: uint8_t
{
    /** Client has webrtc capabilities */
    kClientCanWebrtc = 0x80,
    /** Client is a mobile application */
    kClientIsMobile = 0x40,
    /** Client can use bit 15 from preferences to handle last-green's visibility */
    kClientSupportLastGreen = 0x20
};

// These are located in the generated karereDbSchema.cpp, generated from dbSchema.sql
extern const char* gDbSchema;
extern const char* gDbSchemaHash;

// If the schema hasn't changed but its usage by the karere lib has,
// the lib can force a new version via this suffix
// Defined in karereCommon.cpp
extern const char* gDbSchemaVersionSuffix;

// If false, do not catch exceptions inside marshall calls, forcing the app to crash
// This option should be used only in development/debugging
extern bool gCatchException;

static inline int64_t timestampMs() { return services_get_time_ms(); }

//logging stuff

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

#define KR_EXCEPTION_TO_PROMISE(type)                 \
    catch(std::exception& e)                          \
    {                                                 \
        const char* what = e.what();                  \
        if (!what) what = "(no exception message)";   \
        return promise::Error(what, type);            \
    }

class Client;
/** @endcond PRIVATE */

/** @brief A logger backend that sends the log output of error messages
 * to a remote server. Can be used by the application to send errors to
 * a remote server.
 *
 *  Usage:
 * \c gLogger.addUserLogger("karere-remote", new RemoteLogger);
 *
 * @note This logger requires a karere::Client instance, since it uses
 * the SDK to send the logs to the remote server. Remember to call
 * \c gLogger.removeUserLogger("karere-remote") before than the Client
 * destruction.
 */
class RemoteLogger: public karere::Logger::ILoggerBackend
{
private:
    std::string mAid;
    MyMegaApi& mApi;
public:
    virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags);
    RemoteLogger(MyMegaApi& api): ILoggerBackend(krLogLevelError), mApi(api){}
    void setAnonymousId(std::string &aid) { this->mAid = aid; }
};

}

#endif
