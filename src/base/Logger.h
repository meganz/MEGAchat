#pragma once
#ifndef LOGGER_SPRINTF_BUF_SIZE
	#define LOGGER_SPRINTF_BUF_SIZE 10240
#endif
#include <stdarg.h>
#include <Poco/Mutex.h>
#ifdef _WIN32
#if !defined(va_copy) && defined(_MSC_VER)
	#define va_copy(d,s) ((d) = (s))
#endif
///windows doesn't have the _r function, but the non _r one is thread safe.
///we map the _r to non _r. NOTE: The caller must use the returned pointer,
///not directly the passed-in struct tm, since it is a dummy here and is not
///used here
    inline struct tm *gmtime_r(const time_t *timep, struct tm *result)
    { return gmtime(timep); }
#endif

static std::string strPrintf(const char* fmtString, ...);
///Copies maximum maxCount chars from src to dest.
///Returns number of chars copied, excluding the terminating zero.
///Zero termination is guaranteed in all cases, even if string is truncated
///In this case, the function returns maxCount-1, as the last character is
///the terminating zero and it is not counted
static size_t myStrncpy(char* dest, const char* src, size_t maxCount);

class MyLogger
{
protected:
	unsigned mRotateSize;
    std::string mFileName;
    FILE* mFile;
    Poco::Mutex mMutex;
	unsigned mLogSize;
    bool mLogToConsole;
    bool mLogTimestamps;
    bool mAutoFlush;
    std::string mTimeFmt;
public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion-null"
    typedef MyAutoHandle<char*, void(*)(void*), &free, NULL> AutoBuf;
#pragma GCC diagnostic push

    enum {LOG_TIMESTAMPS=1, LOG_TO_CONSOLE=2, LOG_AUTO_FLUSH=4}; //flags to pass to ctor
    void setRotateSize(unsigned rotateSize) { mRotateSize = rotateSize; }
    void setTimestampFmt(const char* fmt) {mTimeFmt = fmt;}
    void setLogToConsole(bool enable) {mLogToConsole = enable;}
    void setAutoFlush(bool enable) {mAutoFlush = enable;}
MyLogger(const char* logFile, int rotateSize, unsigned flags, const	char* timeFmt="%Y-%m-%d %H:%M:%S")
 :mFile(NULL), mRotateSize(rotateSize),
  mTimeFmt(timeFmt),
  mLogTimestamps(!!(flags&LOG_TIMESTAMPS)),
  mLogToConsole(!!(flags&LOG_TO_CONSOLE)),
  mAutoFlush(!!(flags&LOG_AUTO_FLUSH))
{
	if (logFile)
		startLogging(logFile);
}
void startLogging(const char* fileName)
{
	if (mFile)
		fclose(mFile);
	mFileName = fileName;
	openLogFile();
}

void openLogFile()
{
    mFile = fopen(mFileName.c_str(), "ab+");
    if (!mFile)
        throw std::runtime_error("Logger: Cannot open file "+mFileName);
    fseek(mFile, 0, SEEK_END);
    mLogSize = ftell(mFile); //in a+ mode the position is at the end of file
}

inline void logv(const char* prefix, const char* severity, const char* fmtString, va_list aVaList)
{
	char statBuf[LOGGER_SPRINTF_BUF_SIZE];
	char* buf = statBuf;
    size_t bytesLogged = 0;
    if (mLogTimestamps)
    {
        buf[bytesLogged++] = '[';
        time_t now = time(NULL);
        struct tm tmbuf;
        struct tm* tmval = gmtime_r(&now, &tmbuf);
		bytesLogged += strftime(buf+bytesLogged, LOGGER_SPRINTF_BUF_SIZE-bytesLogged, mTimeFmt.c_str(), tmval);
        buf[bytesLogged++] = ']';
        buf[bytesLogged++] = ':';
        buf[bytesLogged++] = ' ';
    }
    if (prefix)
    {
        bytesLogged += myStrncpy(buf+bytesLogged, prefix, LOGGER_SPRINTF_BUF_SIZE-bytesLogged);
        buf[bytesLogged++] = ':';
		buf[bytesLogged++] = ' ';
    }
    if (severity)
    {
        bytesLogged += myStrncpy(buf+bytesLogged, severity, LOGGER_SPRINTF_BUF_SIZE-bytesLogged);
        buf[bytesLogged++] = ':';
		buf[bytesLogged++] = ' ';
    }
	va_list vaList;
	va_copy(vaList, aVaList);
    int sprintfSpace = LOGGER_SPRINTF_BUF_SIZE-2-bytesLogged;
    int sprintfRv = vsnprintf(buf+bytesLogged, sprintfSpace, fmtString, vaList); //maybe check return value
    if (sprintfRv < 0) //nothing logged if zero, or error if negative, silently ignore the error and return
	{
		va_end(vaList);
		return;
	}
    if (sprintfRv >= sprintfSpace)
	{
	 //static buffer was not enough for the message! Message was truncated
		va_copy(vaList, aVaList); //reuse the arg list. GCC printf invalidaes the arg_list after its used
		size_t bufSize = sprintfRv+bytesLogged+2;
		sprintfSpace = sprintfRv+1;
		buf = (char*)malloc(bufSize);
		if (!buf)
		{
			va_end(vaList);
			printf("Logger: Error: Out of memory: Failed to allocate buffer for sprintf");
			return;
		}
		memcpy(buf, statBuf, bytesLogged);
		sprintfRv = vsnprintf(buf+bytesLogged, sprintfSpace, fmtString, vaList); //maybe check return value
		if (sprintfRv >= sprintfSpace)
		{
			perror("Error: vsnprintf wants to write more data than the size of buffer it requested");
			sprintfRv = sprintfSpace-1;
		}
	}
	va_end(vaList);
    bytesLogged+=sprintfRv;
    buf[bytesLogged] = '\n';
    buf[++bytesLogged] = 0;

    Poco::Mutex::ScopedLock lock(mMutex);

    if (mLogToConsole)
        fputs(buf, stdout); //does not append newline, unlike puts()

    //do not increment mLogSize until we have actually written the data
    if (mLogSize >= mRotateSize)
        rotateLog();
    mLogSize+=bytesLogged;
    int ret = fwrite(buf, 1, bytesLogged, mFile);
    if (mAutoFlush)
        fflush(mFile);
	if (buf != statBuf)
	{
		free(buf);
		buf = NULL;
	}
	if (ret != (int)bytesLogged)
        perror("LOGGER: WARNING: Error writing to log file: ");
}

inline void log(const char* prefix, const char* severity, const char* fmtString, ...)
{
    va_list vaList;
    va_start(vaList, fmtString);
	logv(prefix, severity, fmtString, vaList);
	va_end(vaList);
}

inline void loadLog(AutoBuf& buf, size_t& size) //Logger must be locked!!!
{
    buf.assign((char*)malloc(mLogSize+1));
    if (!buf)
        throw std::runtime_error("Logger::rotate: Out of memory when allocating buffer for log rotation");
    fseek(mFile, 0, SEEK_SET);
    int bytesRead = fread(buf, 1, mLogSize, mFile);
	if (bytesRead != (int)mLogSize)
    {
        if (bytesRead < 0)
            perror("ERROR:Logger::loadInMemory: Error reading log file: ");
        else
			printf("ERROR:Logger::loadInMemory: Could not read enough bytes. Required: %u, read: %d", mLogSize, bytesRead);
        return;
    }
    size = mLogSize;
    buf.handle()[mLogSize] = 0; //zero terminate the string in the buffer
    fseek(mFile, 0, SEEK_END);
}

inline void rotateLog()
{
    AutoBuf buf;
    size_t size;
    loadLog(buf, size);
	int slicePos = mLogSize - (mRotateSize / 2);
    if (slicePos <= 1)
        throw std::runtime_error("Logger::Rotate: The slice offset is less than 1");
	if ((unsigned)slicePos >= mLogSize - 1)
        throw std::runtime_error("Logger::Rotate: The slice offset is at the end of the log. Rotate size is too small");

    long i;
    for (i = slicePos; i >= 0; i--)
        if (buf.handle()[i] == '\n')
            break;
//i shoud point to a \n or be -1
//It is guaranteed that the highest value of i (=half) is
//at least 1 less that the end of the buf (see checks above)
    if (i > 0)
        slicePos = i+1;
//else no new line found in the half->beginning backward scan, so just truncate and dont care about newlines

    fclose(mFile); //writing appends to the end of the file, and now we want to rewrite it
    mFile = NULL;
    FILE* writeFile = fopen(mFileName.c_str(), "wb");
    if (!writeFile)
        throw std::runtime_error("Logger::rotate: Cannot open log file for rewriting");
    fwrite(buf.handle()+slicePos, 1, mLogSize-slicePos, writeFile);
    fclose(writeFile);
    openLogFile();
}

~MyLogger()
{
    if (mFile)
        fclose(mFile);
}
};

static std::string strPrintf(const char* fmtString, ...)
{
    char buf[LOGGER_SPRINTF_BUF_SIZE];
    va_list valist;
    va_start(valist, fmtString);
    int bytesPrinted = vsnprintf(buf, LOGGER_SPRINTF_BUF_SIZE-2, fmtString, valist);
	va_end(valist);
    if (bytesPrinted < 1)
        return std::string();
    buf[bytesPrinted] = '\n';
    buf[++bytesPrinted] = 0;
    return buf;
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
        dptr[maxCount-1] = 0; //guarantee zero termination even if string is truncated
        return maxCount-1; //we ate the last char to put te terinating zero there
    }
    return count-1;
}
