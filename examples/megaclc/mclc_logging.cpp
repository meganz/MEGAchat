#include "mclc_logging.h"

namespace mclc::clc_log
{

void DebugOutputWriter::writeOutput(const std::basic_string<char>& msg, int logLevel)
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    if (logLevel > mCurrentLogLevel)
    {
        return;
    }
    if (mLogFile.is_open())
    {
        mLogFile << msg;
    }
    // To avoid cout saturation only errors are printed.
    if (mLogToConsole && logLevel <= m::logError)
    {
        std::cout << msg;
    }
}

void DebugOutputWriter::disableLogToFile()
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    mLogFile.close();
}

/**
 * @brief Enables the writing to a file.
 *
 * If a file was already opened, it will be closed before opening the new one.
 *
 * NOTE: If the file name to open matches the previously opened file, it will be overwritten.
 *
 * @param fname The path to the file to write the messages.
 */
void DebugOutputWriter::enableLogToFile(const std::string& fname)
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    mLogFile.close();
    if (fname.length() == 0)
    {
        clc_console::conlock(std::cout) << "Error: Provided an empty file name" << std::endl;
        return;
    }
    mLogFile.open(fname.c_str());
    if (mLogFile.is_open())
    {
        mLogFileName = fname;
    }
    else
    {
        clc_console::conlock(std::cout) << "Error: Unable to open output file: " << fname << std::endl;
    }
}

bool DebugOutputWriter::isLoggingToFile() const
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    return mLogFile.is_open();
}

bool DebugOutputWriter::isLoggingToConsole() const 
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    return mLogToConsole;
}

std::string DebugOutputWriter::getLogFileName() const
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    return mLogFileName;
}

void DebugOutputWriter::setLogLevel(int newLogLevel)
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    mCurrentLogLevel = newLogLevel;
}

void DebugOutputWriter::setLogToConsole(bool state)
{
    std::lock_guard<std::mutex>lock{mLogFileWriteMutex};
    mLogToConsole = state;
}

DebugOutputWriter g_debugOutpuWriter;


void MegaCLLogger::logMsg(const int loglevel, const std::string& message)
{
    log(clc_time::timeToLocalTimeString(std::time(0)).c_str(), loglevel, nullptr, message.c_str());
}

void MegaCLLogger::log(const char* time, int loglevel, const char*, const char *message
#ifdef ENABLE_LOG_PERFORMANCE
    , const char** directMessages = nullptr, size_t* directMessagesSizes = nullptr, unsigned numberMessages = 0
#endif
)
{
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\r\n");
#endif
    std::ostringstream os;
    os << "API [" << time << "] " << m::SimpleLogger::toStr(static_cast<m::LogLevel>(loglevel)) << ": " << message << std::endl;
    const auto msg = os.str();
    g_debugOutpuWriter.writeOutput(msg, loglevel);
}

void MegaclcChatChatLogger::logMsg(const int loglevel, const std::string& message)
{
    const std::string msg = "[" + clc_time::timeToLocalTimeString(std::time(0)) + "] Level(" + std::to_string(loglevel) + "): " + message;
    log(loglevel, msg.c_str());
}

void MegaclcChatChatLogger::log(int loglevel, const char *message)
{
#ifdef _WIN32
    if (message && *message)
    {
        OutputDebugStringA(message);
        if (message[strlen(message)-1] != '\n')
            OutputDebugStringA("\r\n");
    }
#endif
    std::ostringstream os;
    os << "CHAT " << message;
    if (*message && message[strlen(message) - 1] != '\n')
    {
        os << std::endl;
    }
    const auto msg = os.str();
    g_debugOutpuWriter.writeOutput(msg, loglevel);
}

MegaCLLogger g_apiLogger;
MegaclcChatChatLogger g_chatLogger;

bool check_err(const std::string& opName, m::MegaError* e, ReportOnConsole report)
{
    if (e->getErrorCode() == c::MegaChatError::ERROR_OK)
    {
        const std::string message = opName + " succeeded.";
        g_apiLogger.logMsg(m::MegaApi::LOG_LEVEL_INFO, message);
        if (report == ReportResult)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return true;
    }
    else
    {
        const std::string message = opName + " failed. Error: " + std::string{e->getErrorString()};
        g_apiLogger.logMsg(m::MegaApi::LOG_LEVEL_ERROR, message);
        if (report != NoConsoleReport)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return false;
    }
}

bool check_err(const std::string& opName, c::MegaChatError* e, ReportOnConsole report)
{
    if (e->getErrorCode() == c::MegaChatError::ERROR_OK)
    {
        const std::string message = opName + " succeeded.";
        g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, message);
        if (report == ReportResult)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return true;
    }
    else
    {
        const std::string message = opName + " failed. Error: " + std::string{e->getErrorString()};
        g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_ERROR, message);
        if (report != NoConsoleReport)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return false;
    }
}

bool isUnexpectedErr(const int errCode, const int expectedErrCode, const char* msg)
{
    if (errCode != expectedErrCode)
    {
        g_chatLogger.logMsg(m::logError, std::string("ERROR CODE ") + std::to_string(errCode) + ": " + msg);
        return true;
    }
    return false;
};

}
