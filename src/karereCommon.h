#ifndef KARERECOMMON_H
#define KARERECOMMON_H

#include <string>
#include "base/logger.h"
#include "base/cservices.h" //needed for timestampMs()
#include <string.h>
#include <bitset>

// MEGA Meetings types
typedef uint8_t Keyid_t;        // 8-bit id of the encryption key
typedef uint32_t Cid_t;         // 24-bit client id (CID) for meetings (identifies a user in a call)
typedef uint64_t IvStatic_t;    // IV static part (8 bytes)
typedef uint32_t Ctr_t;         // packet Ctr (4 bytes)

enum karereTracks: int8_t
{
    kUndefinedTrack = -1,
    kVthumbTrack    = 0,
    kHiResTrack     = 1,
    kAudioTrack     = 2,
};

constexpr unsigned int kInvalidCid = 0;

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
#define KARERE_RECONNECT_ATTEMPT_TIMEOUT 1000   // starts with 1s (+2s bias), but increments exponentially: 3, 4, 6, 10...
#define KARERE_RECONNECT_MAX_ATTEMPT_TIMEOUT 10000

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

/** @brief Stops the karere services susbsystem and frees global resources
 * used by Karere
 */
void globalCleanup();

/** @cond PRIVATE */

// Note: You can't send camera and screen simultaneously, but you can
// send i.e camera in hi-res and low-res over two tracks
// screen sharing is not used at this moment in native
struct AvFlags
{
protected:
    uint8_t mFlags = 0;
public:
    enum: uint8_t { kEmpty          = 0x00,

                    // audio flags
                    kAudio          = 0x01,

                    // camera flags
                    kCameraLowRes   = 0x02,
                    kCameraHiRes    = 0x04,
                    kCamera         = kCameraLowRes | kCameraHiRes,

                    // screen share flags
                    kScreenLowRes   = 0x08,
                    kScreenHiRes    = 0x10,
                    kScreen         = kScreenLowRes | kScreenHiRes,

                    // Video (screen share | camera) flags
                    kLowResVideo    = kCameraLowRes | kScreenLowRes,
                    kHiResVideo     = kCameraHiRes  | kScreenHiRes,
                    kVideo          = kLowResVideo  | kHiResVideo,

                    // on hold flags
                    kOnHold         = 0x80,
                  };

    AvFlags(uint8_t flags): mFlags(flags){}
    AvFlags(bool audio, bool video) : mFlags(static_cast<uint8_t>((audio ? kAudio : 0) | (video ? kCamera : 0))) {}
    AvFlags(): mFlags(karere::AvFlags::kEmpty){}

    // setters/modifiers
    void set(uint8_t val)       { mFlags = val; }
    void add(uint8_t val)       { mFlags = mFlags | val; }
    void remove(uint8_t val)    { mFlags = static_cast<uint8_t>(mFlags & ~val); }
    void setOnHold(bool enable) { enable ? add(kOnHold) : remove(kOnHold); }

    // getters
    uint8_t value() const                   { return mFlags; }

    // audio flags getters
    bool audio() const                      { return mFlags & kAudio; }

    // camera flags getters
    bool cameraHiRes() const                { return mFlags & kCameraHiRes; }
    bool cameraLowRes() const               { return mFlags & kCameraLowRes; }
    bool camera() const                     { return mFlags & kCamera; }

    // screen share flags getters
    bool screenShareHiRes() const           { return mFlags & kScreenHiRes; }
    bool screenShareLowRes() const          { return mFlags & kScreenLowRes; }
    bool screenShare() const                { return mFlags & kScreen; }

    // video (screen share AND/OR camera) flags getters
    bool video() const                      { return mFlags & kVideo; }
    bool videoHiRes() const                 { return mFlags & kHiResVideo; }
    bool videoLowRes() const                { return mFlags & kLowResVideo; }

    // on hold flags getters
    bool isOnHold() const                   { return mFlags & kOnHold; }

    // check methods
    operator bool() const           { return mFlags != 0; }
    bool operator==(AvFlags other)  { return (mFlags == other.mFlags); }
    bool operator!=(AvFlags other)  { return (mFlags != other.mFlags); }
    bool any() const                { return mFlags != 0; }
    bool has(uint8_t val) const     { return mFlags & val; }

    std::string toString() const
    {
        std::string result;
        if (mFlags & kAudio)
            result+='a';
        if (mFlags & kCameraLowRes)
            result+= "cL";
        if (mFlags & kCameraHiRes)
            result+= "cH";
        if (mFlags & kScreenLowRes)
            result+= "sL";
        if (mFlags & kScreenHiRes)
            result+= "sH";
        if (mFlags & kOnHold)
            result+='h';
        if (result.empty())
            result='-';
        return result;
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

typedef enum
{
    SC_NEW_SCHED        = 0,
    SC_PARENT           = 1,
    SC_TZONE            = 2,
    SC_START            = 3,
    SC_END              = 4,
    SC_TITLE            = 5,
    SC_DESC             = 6,
    SC_ATTR             = 7,
    SC_OVERR            = 8,
    SC_CANC             = 9,
    SC_FLAGS            = 10,
    SC_RULES            = 11,
    SC_FLAGS_SIZE       = 12,
} karere_scheduled_changed_flags_t;
typedef std::bitset<SC_FLAGS_SIZE> karere_sched_bs_t;

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
        return ::promise::Error(what, type);            \
    }

/** @endcond PRIVATE */

}

#endif
