#ifndef LOGGER_SPRINTF_BUF_SIZE
	#define LOGGER_SPRINTF_BUF_SIZE 10240
#endif

#include <functional>
#include <iostream>
#include <stdarg.h>
#include <string.h>
#define KRLOGGER_BUILDING //sets DLLIMPEXPs in logger.h to 'export' mode
#include "logger.h"
#include "loggerFile.h"
#include "loggerConsole.h"
#include "../stringUtils.h" //needed for parsing the KRLOG env variable
#include "sdkApi.h"

#ifdef _WIN32
#if !defined(va_copy) && defined(_MSC_VER)
	#define va_copy(d,s) ((d) = (s))
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#define strcasecmp(...) _stricmp(__VA_ARGS__)
#else
#define strcasecmp(...) stricmp(__VA_ARGS__)
#endif

///windows doesn't have the _r function, but the non _r one is thread safe.
///we map the _r to non _r. NOTE: The caller must use the returned pointer,
///not directly the passed-in struct tm, since it is a dummy here and is not
///used here
    inline struct tm *gmtime_r(const time_t *timep, struct tm *result)
    { return gmtime(timep); }
#endif
    krLogLevel karere::Logger::mKarereMaxLogLevel = krLogLevelMax;
    extern "C"
    {
        // this must be in sync with the enums in logger.h
        typedef const char* KarereLogLevelName[2];
        KRLOGGER_DLLEXPORT KarereLogLevelName krLogLevelNames[krLogLevelMax + 1] = {
            {NULL,  "off"    },
            {"ERR", "error"  },
            {"WRN", "warn"   },
            {"nfo", "info"   },
            {"dbg", "debug"  },
            {"vrb", "verbose"}
        };
    }
namespace karere
{
/** Copies maximum maxCount chars from src to dest.
* @returns number of chars copied, excluding the terminating zero.
* Zero termination is guaranteed in all cases, even if string is truncated
* In this case, the function returns maxCount-1, as the last character is
* the terminating zero and it is not counted
*/
static size_t myStrncpy(char* dest, const char* src, size_t maxCount);

void Logger::setKarereMaxLogLevel(const unsigned int v)
{
    const auto newVal = static_cast<krLogLevel>(v);
    if (newVal == mKarereMaxLogLevel || newVal < krLogLevelError || newVal > krLogLevelMax)
    {
        return;
    }
    mKarereMaxLogLevel = newVal;
}

void Logger::logToConsole(bool enable)
{
    LockGuard lock(mMutex);
    if (enable)
    {
        if (mConsoleLogger)
            return;
        mConsoleLogger.reset(new ConsoleLogger(*this));
    }
    else
    {
        if (!mConsoleLogger)
            return;
        mConsoleLogger.reset();
    }
}

void Logger::logToConsoleUseColors(bool useColors)
{
    LockGuard lock(mMutex);
    if (mConsoleLogger)
    {
        mConsoleLogger->setUseColors(useColors);
    }
}

void Logger::logToFile(const char* fileName, size_t rotateSizeKb)
{
    LockGuard lock(mMutex);
    if (!fileName) //disable
    {
        mFileLogger.reset();
        return;
    }
    //re-configure
    mFileLogger.reset(new FileLogger(mFlags, fileName, static_cast<int>(rotateSizeKb*1024)));
}

void Logger::setAutoFlush(bool enable)
{
    LockGuard lock(mMutex);
    if (enable)
        mFlags &= static_cast<unsigned>(~krLogNoAutoFlush);
    else
        mFlags |= krLogNoAutoFlush;
}

Logger::Logger(unsigned aFlags, const char* timeFmt)
    :mTimeFmt(timeFmt), mFlags(aFlags)
{
    setup();
    setupFromEnvVar();
    if ((mFlags & krLogNoStartMessage) == 0)
        log("LOGGER", 0, 0, "========== Application startup ===========\n");
}

// This function should be in a shared utils namespace
int64_t static getCurrentTimeMilliseconds()
{
    namespace ch = std::chrono;

    const auto nowSinceEpoch = ch::system_clock::now().time_since_epoch();
    const int64_t msSinceEpoch = ch::duration_cast<ch::milliseconds>(nowSinceEpoch).count();
    const int64_t milliseconds = msSinceEpoch % 1000;

    return milliseconds;
}

// disable false positive warning in GCC 11+
#if defined(__GNUC__) && !defined(__APPLE__) && !defined(__ANDROID__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

inline size_t Logger::prependInfo(char* buf, size_t bufSize, const char* prefix, const char* severity,
                                  unsigned flags)
{
    size_t bytesLogged = 0;
    if ((mFlags & krLogNoTimestamps) == 0)
    {
        buf[bytesLogged++] = '[';
        time_t now = time(NULL);
        struct tm tmbuf;
        struct tm* tmval = gmtime_r(&now, &tmbuf);
        std::string currentTimeMilliseconds = "." + std::to_string(getCurrentTimeMilliseconds());
        bytesLogged += strftime(buf+bytesLogged, bufSize-bytesLogged, mTimeFmt.c_str(), tmval);
        std::copy(std::begin(currentTimeMilliseconds), std::end(currentTimeMilliseconds), buf+bytesLogged);
        bytesLogged += currentTimeMilliseconds.size();
        buf[bytesLogged++] = ']';
    }
    if (severity)
    {
        buf[bytesLogged++] = '[';
        bytesLogged += myStrncpy(buf+bytesLogged, severity, bufSize-bytesLogged);
        buf[bytesLogged++] = ']';
    }
    if (prefix)
    {
        buf[bytesLogged++] = '[';
        bytesLogged += myStrncpy(buf+bytesLogged, prefix, bufSize-bytesLogged);
        buf[bytesLogged++] = ']';
    }
    if (bytesLogged && ((flags & krLogNoLeadingSpace) == 0))
        buf[bytesLogged++] = ' ';
    return bytesLogged;
}

#if defined(__GNUC__) && !defined(__APPLE__) && !defined(__ANDROID__)
#pragma GCC diagnostic pop
#endif

void Logger::logv(const char* prefix, krLogLevel level, unsigned flags, const char* fmtString,
    va_list aVaList)
{
    flags |= (mFlags & krGlobalFlagMask);
    char statBuf[LOGGER_SPRINTF_BUF_SIZE];
    char* buf = statBuf;
    size_t bytesLogged = prependInfo(buf, LOGGER_SPRINTF_BUF_SIZE, prefix,
        ((flags & krLogNoLevel) && (level > krLogLevelWarn))
            ? NULL
            :krLogLevelNames[level][0], flags);

    va_list vaList;
    va_copy(vaList, aVaList);
    int sprintfSpace = static_cast<int>(LOGGER_SPRINTF_BUF_SIZE-2-bytesLogged);
    int sprintfRv = vsnprintf(buf+bytesLogged, static_cast<size_t>(sprintfSpace), fmtString, vaList); //maybe check return value
    std::function<bool()> isErrorVsnprintf =
        [&vaList, &sprintfRv] ()
        {
            if (sprintfRv < 0)
            { //nothing logged if zero, or error if negative, silently ignore the error and return
                va_end(vaList);
                return true;
            }
            return false;
        };
    if (isErrorVsnprintf())
    {
        return;
    }

    if (sprintfRv >= sprintfSpace)
    {
        //static buffer was not enough for the message! Message was truncated
        va_copy(vaList, aVaList); //reuse the arg list. GCC printf invalidaes the arg_list after its used
        size_t bufSize = static_cast<size_t>(sprintfRv)+bytesLogged+2;
        sprintfSpace = sprintfRv+1;
        buf = new char[bufSize];
        if (!buf)
        {
            va_end(vaList);
            fprintf(stderr, "Logger: ERROR: Out of memory allocating a buffer for sprintf");
            return;
        }
        memcpy(buf, statBuf, bytesLogged);
        sprintfRv = vsnprintf(buf+bytesLogged, static_cast<size_t>(sprintfSpace), fmtString, vaList); //maybe check return value
        if (isErrorVsnprintf())
        {
            delete[] buf;
            return;
        }

        if (sprintfRv >= sprintfSpace)
        {
            perror("Error: vsnprintf wants to write more data than the size of buffer it requested");
        }
    }
    va_end(vaList);
    bytesLogged+=static_cast<size_t>(sprintfRv);
    buf[bytesLogged] = 0;
    logString(level, buf, flags, bytesLogged);
    if (buf != statBuf)
        delete[] buf;
}

/** This is the low-level log function that does the actual logging
 *  of an assembled single string. We still need the log level here, because if the
 *  console color selection.
 */
void Logger::logString(krLogLevel level, const char* msg, unsigned flags, size_t len)
{
    if (!msg)
    {
        assert(false);
        return;
    }

    try
    {
        // This try-catch prevents crashes in app in case that mutex can't be adquire.
        LockGuard lock(mMutex);
        if (len == (size_t)-1)
            len = strlen(msg);

        if (mConsoleLogger && ((flags & krLogNoConsole) == 0))
            mConsoleLogger->logString(level, msg, flags);
        if ((mFileLogger) && ((flags & krLogNoFile) == 0))
            mFileLogger->logString(msg, len, flags);
        if (!mUserLoggers.empty())
        {
            for (auto& logger: mUserLoggers)
            {
                ILoggerBackend* backend = logger.second;
                if(level <= backend->maxLogLevel)
                    backend->log(level, msg, len, flags);
            }
        }
    }
    catch (std::system_error &e)
    {
        std::cerr << "Failed to log message: " << e.what() << std::endl;
    }
}

 void Logger::log(const char* prefix, krLogLevel level, unsigned flags,
                const char* fmtString, ...)
{
    va_list vaList;
    va_start(vaList, fmtString);
    logv(prefix, level, flags, fmtString, vaList);
    va_end(vaList);
}

std::shared_ptr<Logger::LogBuffer> Logger::loadLog()
{
    if (!mFileLogger)
        return NULL;
    LockGuard lock(mMutex);
    return mFileLogger->loadLog();
}

Logger::~Logger()
{
    LockGuard lock(mMutex);
    if (!mUserLoggers.empty())
    {
        mUserLoggers.clear();
    }
    if ((mFlags & krLogNoTerminateMessage) == 0)
        log("LOGGER", 0, 0, "========== Application terminate ===========\n");
}

Logger::ILoggerBackend* Logger::addUserLogger(const char* tag, ILoggerBackend* logger)
{
    LockGuard lock(mMutex);
    auto& item = mUserLoggers[tag];
    auto ret = item;
    item = logger;
    return ret;
}

Logger::ILoggerBackend* Logger::removeUserLogger(const char* tag)
{
    LockGuard lock(mMutex);
    auto it = mUserLoggers.find(tag);
    if (it == mUserLoggers.end())
        return nullptr;
    auto ret = it->second;
    mUserLoggers.erase(it);
    return ret;
}

void Logger::setupFromEnvVar()
{
    const char* strConfig = getenv("KRLOG");
    if (!strConfig)
        return;
    struct ParamVal: public std::string
    {
        krLogLevel numVal;
        ParamVal(std::string&& str): std::string(std::forward<std::string>(str)){};
    };

    std::map<std::string, ParamVal> config;
    try
    {
        parseNameValues(strConfig, " ,;:", '=', config);
        //verify log level names
        for (auto& param: config)
        {
            auto level = krLogLevelStrToNum(param.second.c_str());
            if (level == (krLogLevel)-1)
                throw std::runtime_error("can't recognize log level name '"+param.second+"'");
            param.second.numVal = level;
        }
    }
    catch(std::exception& e)
    {
        log("LOGGER", krLogLevelError, 0, "Error parsing KRLOG env variable:\n%s\nEnv settings will not be applied\n", e.what());
        return;
    }
    if ((mFlags & krLogDontShowEnvConfig) == 0)
        log("LOGGER", 0, 0, "KRLOG env configuration variable detected\n");

    //put channels in a map for easier access
    krLogLevel allLevels;
    auto it = config.find("all");
    if(it != config.end()) {
        allLevels = static_cast<krLogLevel>(it->second.numVal);
        config.erase(it);
        if ((mFlags & krLogDontShowEnvConfig) == 0)
            log("LOGGER", 0, 0, "All channels, except below -> '%s'\n", krLogLevelNames[allLevels][1]);

    }
    else
    {
        allLevels = static_cast<krLogLevel>(-1);
    }
    std::map<std::string, KarereLogChannel*> chans;
    for (size_t n = 0; n < krLogChannelLast; n++)
    {
        KarereLogChannel& chan = logChannels[n];
        if (allLevels != (krLogLevel)-1)
            chan.logLevel = allLevels;
        chans[chan.id] = &chan;
    }
    for (auto& item: config)
    {
        auto chan = chans.find(item.first);
        if (chan == chans.end())
        {
            log("LOGGER", krLogLevelError, 0, "Unknown channel in KRLOG env variable: %s. Ignoring\n", item.first.c_str());
            continue;
        }
        chan->second->logLevel = static_cast<krLogLevel>(item.second.numVal);
        if ((mFlags & krLogDontShowEnvConfig) == 0)
            log("LOGGER", 0, 0, "Channel '%s' -> %s\n", item.first.c_str(), krLogLevelNames[chan->second->logLevel][1]);
    }
}

static size_t myStrncpy(char* dest, const char* src, size_t maxCount)
{
    size_t count = 1;
    const char* sptr = src;
    char* dptr = dest;
    for ( ;count <= maxCount; sptr++, dptr++, count++)
    {
        *dptr = *sptr;
         if (*sptr == 0)
            break;
    }
    if (count > maxCount) //copy ermianted because we reached maxCount
    {
        dest[maxCount-1] = 0; //guarantee zero termination even if string is truncated
        return maxCount-1; //we ate the last char to put te terinating zero there
    }
    return count-1;
}

KRLOGGER_DLLEXPORT Logger gLogger;
} //end karere namespace

extern "C"
{
    KRLOGGER_DLLEXPORT KarereLogChannel* krLoggerChannels = karere::gLogger.logChannels;

    KRLOGGER_DLLEXPORT krLogLevel krLogLevelStrToNum(const char* strLevel)
    {
        for (krLogLevel n = 0; n <= krLogLevelMax; ++n)
        {
            auto& name = krLogLevelNames[n];
            if ((strcasecmp(strLevel, name[1]) == 0) ||
                (name[0] && (strcasecmp(strLevel, name[0]) == 0)))
                return n;
        }
        return (krLogLevel)-1;
    }

KRLOGGER_DLLEXPORT void krLoggerLog(krLogChannelNo channel, krLogLevel level,
    const char* fmtString, ...)
{
    va_list vaList;
    va_start(vaList, fmtString);
    auto& chan = karere::gLogger.logChannels[channel];
    karere::gLogger.logv(chan.display, level, chan.flags, fmtString, vaList);
    va_end(vaList);
}
} //end plain-C stuff
