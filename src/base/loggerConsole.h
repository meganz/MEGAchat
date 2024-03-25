#ifndef LOGGERCONSOLE_H
#define LOGGERCONSOLE_H

#include "logger.h"
#ifndef _WIN32
    #include <unistd.h>
#else
	#include <io.h>
#endif

#if defined(_WIN32) && defined(_MSC_VER)
constexpr auto MegaIsatty = _isatty;
#else
constexpr auto MegaIsatty = isatty;
#endif

namespace karere
{
class ConsoleLogger
{
protected:
    Logger& mLogger; //we want reference to the logger because we may want to set up error/warning colors there
    bool mStdoutIsAtty;
    bool mStderrIsAtty;
public:
    ConsoleLogger(Logger& logger, bool useColors = true)
    : mLogger(logger), mStdoutIsAtty(MegaIsatty(1)), mStderrIsAtty(MegaIsatty(2))
    {}
    void logString(unsigned level, const char* msg, unsigned flags)
    {
        if (level == krLogLevelError)
        {
            if (mStderrIsAtty)
                fprintf(stderr, "\033[1;31m%s%s", msg, "\033[0m");
            else
                fputs(msg, stderr);

            if ((flags & krLogNoAutoFlush) == 0)
                fflush(stderr);
        }
        else if (level == krLogLevelWarn)
        {
            if (mStderrIsAtty)
                fprintf(stderr, "\033[1;33m%s\033[0m", msg);
            else
                fputs(msg, stderr);

            if ((flags & krLogNoAutoFlush) == 0)
                fflush(stderr);
        }
        else //get color from flags
        {
            if (mStdoutIsAtty)
                printf("%s%s\033[0m", stdoutColorSelect(flags), msg);
            else
                fputs(msg, stdout);
        }
        if ((flags & krLogNoAutoFlush) == 0)
            fflush(stdout);
    }
    void setUseColors(bool useColors)
    {
        this->mStdoutIsAtty = MegaIsatty(1) && useColors;
        this->mStderrIsAtty = MegaIsatty(2) && useColors;
    }

    const char* stdoutColorSelect(unsigned flags)
    {
        static const char* colorEscapes[krLogColorMask+1] =
        {
            "\033[30m", "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[37m",
            "\033[1;30m", "\033[1;31m", "\033[1;32m", "\033[1;33m", "\033[1;34m", "\033[1;35m", "\033[1;36m", "\033[1;37m"
        };
        //printf("============== flags: %X, color: %u\n", flags, flags & krLogColorMask);
        return colorEscapes[flags & krLogColorMask];
    }
};
}
#endif // LOGGERCONSOLE_H

