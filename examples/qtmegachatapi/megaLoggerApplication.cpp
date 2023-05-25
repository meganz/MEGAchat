#include "megaLoggerApplication.h"
#include <iostream>
using namespace std;
using namespace megachat;

MegaLoggerApplication::MegaLoggerApplication(const char *filename)
{
    mLogConsole=false;
    testlog.open(filename, ios::out | ios::app);
}

MegaLoggerApplication::~MegaLoggerApplication()
{
    testlog.close();
}

void MegaLoggerApplication::log(const char*, int, const char*, const char *message)
{
    testlog << message << endl;
    if(mLogConsole)
       cout << message << endl;
}

void MegaLoggerApplication::postLog(const char *message)
{
    testlog << message << endl;
    if(mLogConsole)
       cout << message << endl;
}

void MegaLoggerApplication::log(int loglevel, const char *message)
{
    string levelStr;
    switch (loglevel)
    {
    case MegaChatApi::LOG_LEVEL_ERROR: levelStr = "err"; break;
        case MegaChatApi::LOG_LEVEL_WARNING: levelStr = "warn"; break;
        case MegaChatApi::LOG_LEVEL_INFO: levelStr = "info"; break;
        case MegaChatApi::LOG_LEVEL_VERBOSE: levelStr = "verb"; break;
        case MegaChatApi::LOG_LEVEL_DEBUG: levelStr = "debug"; break;
        case MegaChatApi::LOG_LEVEL_MAX: levelStr = "debug-verbose"; break;
        default: levelStr = ""; break;
    }
    testlog  << message;
    if(mLogConsole)
       cout << message << endl;
}

bool MegaLoggerApplication::getLogConsole() const
{
    return mLogConsole;
}

void MegaLoggerApplication::setLogConsole(bool logConsole)
{
    mLogConsole = logConsole;
}
