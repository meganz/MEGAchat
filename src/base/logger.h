#ifndef MEGA_LOGGER_H_INCLUDED
#define MEGA_LOGGER_H_INCLUDED
#include <stdlib.h> //needed for abort()

#ifdef KRLOGGER_SHARED
    #ifdef _WIN32
        #ifndef MEGA_FULL_STATIC
            #pragma warning(disable: 4251) //Logger class exports STL classes that don't have DLL interface
            #define KRLOGGER_DLLEXPORT __declspec(dllexport)
            #define KRLOGGER_DLLIMPORT __declspec(dllimport)
        #else
            #define KRLOGGER_DLLEXPORT 
            #define KRLOGGER_DLLIMPORT 
        #endif
    #else
        #define KRLOGGER_DLLEXPORT __attribute__ ((visibility("default")))
        #define KRLOGGER_DLLIMPORT
    #endif
    #ifdef KRLOGGER_BUILDING
        #define KRLOGGER_DLLIMPEXP KRLOGGER_DLLEXPORT
    #else
        #define KRLOGGER_DLLIMPEXP KRLOGGER_DLLIMPORT
    #endif
#else
    #define KRLOGGER_DLLEXPORT
    #define KRLOGGER_DLLIMPORT
    #define KRLOGGER_DLLIMPEXP
#endif

typedef unsigned short krLogLevel;
enum
{
//0 is reserved to overwrite completely disabled logging. Used only by logger itself
    krLogLevelError = 1,
    krLogLevelWarn,
    krLogLevelInfo,
    krLOgLevelVerbose,
    krLogLevelDebug,
    krLogLevelDebugVerbose,
    krLogLevelLast = krLogLevelDebugVerbose
};

enum
{
    krLogColorMask = 0x0F,
    krLogNoAutoFlush = 1 << 4,
    krLogNoTimestamps = 1 << 5,
    krLogNoLevel = 1 << 6,
    krLogNoFile = 1 << 7,
    krLogNoConsole = 1 << 8,
    krLogNoLeadingSpace = 1 << 9,
    krLogDontShowEnvConfig = 1 << 10,
    krLogNoStartMessage = 1 << 11,
    krLogNoTerminateMessage = 1 << 12,
    krGlobalFlagMask = krLogNoAutoFlush|krLogNoLevel|krLogNoTimestamps ///flags that override channel flags when they are globally set
};
typedef unsigned char krLogChannelNo;
typedef struct _KarereLogChannel
{
    const char* id;
    const char* display;
    krLogLevel logLevel;
    unsigned flags;
} KarereLogChannel;

enum { krLogChannelCount = 32 };

#ifdef __cplusplus

#include <string>
#include <memory>
#include <mutex>
#include <map>

namespace karere
{
class FileLogger;
class ConsoleLogger;

class KRLOGGER_DLLIMPEXP Logger
{
public:
    class ILoggerBackend;
    struct LogBuffer;
protected:
    std::string mTimeFmt;
    inline void setup();
    void setupFromEnvVar();
    std::unique_ptr<FileLogger> mFileLogger;
    std::unique_ptr<ConsoleLogger> mConsoleLogger;
    volatile unsigned mFlags;
    size_t prependInfo(char *buf, size_t bufSize, const char* prefix, const char* severity, unsigned flags);

    /** This is the low-level log function that does the actual logging
     *  of an assembled single string */
    void logString(krLogLevel level, const char* msg, unsigned flags, size_t len=(size_t)-1);
    std::map<std::string, ILoggerBackend*> mUserLoggers;
public:
    std::recursive_mutex mMutex;
    typedef std::lock_guard<std::recursive_mutex> LockGuard;
    unsigned flags() const { return mFlags;}
    void setFlags(unsigned flags)
    {
        LockGuard lock(mMutex);
        mFlags = flags;
    }
    KarereLogChannel logChannels[krLogChannelCount];
    void setTimestampFmt(const char* fmt) {mTimeFmt = fmt;}
    void logToConsole(bool enable=true);
    void logToConsoleUseColors(bool useColors);
    void logToFile(const char* fileName, size_t rotateSize);
    void setAutoFlush(bool enable=true);
    Logger(unsigned flags = 0, const char* timeFmt="%m-%d %H:%M:%S");
    void logv(const char* prefix, krLogLevel level, unsigned flags, const char* fmtString, va_list aVaList);
    void log(const char* prefix, krLogLevel level, unsigned flags,
                const char* fmtString, ...);
    std::shared_ptr<LogBuffer> loadLog();

    /** @brief Registers a user logger with the specified tag.
     * If a logger with that tag does not already exist, the function returns
     * \c nullptr. If one already exists, the new one replaces it, and the old one
     * is returned.
     */
    ILoggerBackend *addUserLogger(const char* tag, ILoggerBackend* logger);

    /** @brief Unregisters the user logger with the specified tag, and returns the
     * instance. The user is responsible for freeing it.
     * \note If a user logger is never unregistered, it will be deleted by the
     * Logger upon its destruction
     */
    ILoggerBackend* removeUserLogger(const char* tag);
    ~Logger();
    struct LogBuffer
    {
        char* data;
        size_t bufSize;
        LogBuffer(char* aData=NULL, size_t aSize=0)
        : data(aData), bufSize(aSize)
        {}
        ~LogBuffer()
        {
            if (data)
                delete[] data;
        }
    };
    class ILoggerBackend
    {
    public:
        krLogLevel maxLogLevel;
        virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags) = 0;
        ILoggerBackend(krLogLevel maxLevel=krLogLevelDebugVerbose): maxLogLevel(maxLevel){}
        virtual ~ILoggerBackend() {}
    };

};

extern KRLOGGER_DLLIMPEXP Logger gLogger;
}

#endif //C++


#define __KR_DEFINE_LOGCHANNELS_ENUM(...)                                           \
    enum { krLogChannel_default = 0, ##__VA_ARGS__, krLogChannelLast }
#ifdef __cplusplus

#define KR_LOGGER_CONFIG_START(...)                                                       \
    __KR_DEFINE_LOGCHANNELS_ENUM(__VA_ARGS__);                                      \
    inline void karere::Logger::setup() {                                           \
        unsigned long long initialized = 0;

#define KR_LOGCHANNEL(id, display, level, flags)                                    \
        logChannels[krLogChannel_##id] = {#id, display, krLogLevel##level, flags};  \
        initialized |= (1 << krLogChannel_##id);

#define KR_LOGGER_CONFIG(...) __VA_ARGS__;

#define KR_LOGGER_CONFIG_END()                                                      \
        if (initialized != ((1 << krLogChannelLast) -1)) {                          \
            fprintf(stderr, "karere::Logger: Not all log channels have beeen configured, please fix loggerChannelConfig.h"); \
            abort();                                                                \
        }                                                                           \
}
#else
#define KR_LOGGER_CONFIG_START(...)  __KR_DEFINE_LOGCHANNELS_ENUM(__VA_ARGS__);
#define KR_LOGCHANNEL(id, display, level, flags)
#define KR_LOGGER_CONFIG(...)
#define KR_LOGGER_CONFIG_END()
#endif


#include <loggerChannelConfig.h>

//The code below is plain C

extern "C" KRLOGGER_DLLIMPEXP KarereLogChannel* krLoggerChannels;
extern "C" KRLOGGER_DLLIMPEXP void krLoggerLog(krLogChannelNo channel, krLogLevel level,
    const char* fmtString, ...);
extern "C" KRLOGGER_DLLIMPEXP void krLoggerLogString(krLogChannelNo channel, krLogLevel level,
    const char* str);
extern "C" KRLOGGER_DLLIMPEXP krLogLevel krLogLevelStrToNum(const char* str);
static inline int krLoggerWouldLog(krLogChannelNo channel, krLogLevel level)
{
    return (level <= krLoggerChannels[channel].logLevel);
}

#define KARERE_LOG(channel, level, fmtString,...)   \
    ((level <= krLoggerChannels[channel].logLevel) ?  \
       krLoggerLog(channel, level, fmtString "\n", ##__VA_ARGS__): void(0))

#ifdef __cplusplus
//C++ style logging with streaming opereator
#define KARERE_LOG_DEBUG(channel, fmtString,...) KARERE_LOG(channel, krLogLevelDebug, fmtString, ##__VA_ARGS__)
#define KARERE_LOG_INFO(channel, fmtString,...) KARERE_LOG(channel, krLogLevelInfo, fmtString, ##__VA_ARGS__)
#define KARERE_LOG_WARNING(channel, fmtString,...) KARERE_LOG(channel, krLogLevelWarn, fmtString, ##__VA_ARGS__)
#define KARERE_LOG_ERROR(channel, fmtString,...) KARERE_LOG(channel, krLogLevelError, fmtString, ##__VA_ARGS__)
#define KARERE_LOG_ALWAYS(channel, fmtString,...) KARERE_LOG(channel, krLogLevelAlways, fmtString, ##__VA_ARGS__)

#define KARERE_LOGPP(channel, level, ...) \
    if (level <= krLoggerChannels[channel].logLevel) \
    do { \
        std::ostringstream oss; \
        oss << __VA_ARGS__; \
        krLoggerLog(channel, level, "%s\n", oss.str().c_str()); \
    } while (false)

#define KARERE_LOGPP_DEBUG(channel,...) KARERE_LOGPP(channel, krLogLevelDebug, ##__VA_ARGS__)
#define KARERE_LOGPP_INFO(channel,...) KARERE_LOGPP(channel, krLogLevelInfo, ##__VA_ARGS__)
#define KARERE_LOGPP_WARN(channel,...) KARERE_LOGPP(channel, krLogLevelWarn, ##__VA_ARGS__)
#define KARERE_LOGPP_ERROR(channel,...) KARERE_LOGPP(channel, krLogLevelError, ##__VA_ARGS__)
#define KARERE_LOGPP_ALWAYS(channel,...) KARERE_LOGPP(channel, krLogLevelAlways, ##__VA_ARGS__)

#endif //C++
#endif
