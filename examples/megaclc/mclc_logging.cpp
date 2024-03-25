#include "mclc_logging.h"

#include "mclc_globals.h"
#include "mega/logging.h"

namespace mclc::clc_log
{

void DebugOutputWriter::writeOutput(const std::basic_string<char>& msg, int logLevel)
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    if (logLevel <= mFileLogLevel && mLogFile.is_open())
    {
        mLogFile << msg;
    }
    if (logLevel <= mConsoleLogLevel && mLogToConsole)
    {
        clc_console::conlock(std::cout) << msg;
    }
}

void DebugOutputWriter::disableLogToFile()
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
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
void DebugOutputWriter::enableLogToFile(const fs::path& filePath)
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    mLogFile.close();
    if (filePath.empty())
    {
        clc_console::conlock(std::cout) << "Error: Provided an empty file name\n";
        return;
    }
    mLogFile.open(filePath);
    if (mLogFile.is_open())
    {
        mLogFilePath = filePath;
    }
    else
    {
        clc_console::conlock(std::cout)
            << "Error: Unable to open output file: " << filePath << "\n";
    }
}

bool DebugOutputWriter::isLoggingToFile() const
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    return mLogFile.is_open();
}

bool DebugOutputWriter::isLoggingToConsole() const
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    return mLogToConsole;
}

std::string DebugOutputWriter::getLogFileName() const
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    return mLogFilePath.filename().string();
}

void DebugOutputWriter::setConsoleLogLevel(int newLogLevel)
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    mConsoleLogLevel = newLogLevel;
}

int DebugOutputWriter::getConsoleLogLevel() const
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    return mConsoleLogLevel;
}

void DebugOutputWriter::setFileLogLevel(int newLogLevel)
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    mFileLogLevel = newLogLevel;
}

int DebugOutputWriter::getFileLogLevel() const
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    return mFileLogLevel;
}

void DebugOutputWriter::setLogToConsole(bool state)
{
    std::lock_guard<std::mutex> lock{mLogFileWriteMutex};
    mLogToConsole = state;
}

DebugOutputWriter g_debugOutpuWriter;

void MegaCLLogger::logMsg(const int loglevel, const std::string& message)
{
    log(clc_time::timeToLocalTimeString(std::time(0)).c_str(), loglevel, nullptr, message.c_str());
}

void MegaCLLogger::log(const char* time,
                       int loglevel,
                       const char*,
                       const char* message
#ifdef ENABLE_LOG_PERFORMANCE
                       ,
                       const char** directMessages = nullptr,
                       size_t* directMessagesSizes = nullptr,
                       unsigned numberMessages = 0
#endif
)
{
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\r\n");
#endif
    std::ostringstream os;
    os << "API [" << time << "] " << m::SimpleLogger::toStr(static_cast<m::LogLevel>(loglevel))
       << ": " << message << "\n";
    g_debugOutpuWriter.writeOutput(os.str(), loglevel);
}

void MegaclcChatChatLogger::logMsg(const int loglevel, const std::string& message)
{
    const std::string msg = "[" + clc_time::timeToLocalTimeString(std::time(0)) + "] Level(" +
                            std::to_string(loglevel) + "): " + message;
    log(loglevel, msg.c_str());
}

void MegaclcChatChatLogger::log(int loglevel, const char* message)
{
#ifdef _WIN32
    if (message && *message)
    {
        OutputDebugStringA(message);
        if (message[strlen(message) - 1] != '\n')
            OutputDebugStringA("\r\n");
    }
#endif
    std::ostringstream os;
    os << "CHAT " << message;
    if (*message && message[strlen(message) - 1] != '\n')
    {
        os << std::endl;
    }
    g_debugOutpuWriter.writeOutput(os.str(), loglevel);
}

MegaCLLogger g_apiLogger;
MegaclcChatChatLogger g_chatLogger;

void logMsg(const int logLevel, const std::string& message, const ELogWriter outputWriter)
{
    switch (outputWriter)
    {
        case ELogWriter::SDK:
            g_apiLogger.logMsg(logLevel, message);
            return;
        case ELogWriter::MEGA_CHAT:
            g_chatLogger.logMsg(logLevel, message);
            return;
    }
}

static bool check_err_aux(const std::string& opName,
                          int errCode,
                          const char* errorString,
                          ReportOnConsole report,
                          ELogWriter outputWriter)
{
    if (errCode == c::MegaChatError::ERROR_OK)
    {
        const std::string message = opName + " succeeded.";
        logMsg(m::logInfo, message, outputWriter);
        if (report == ReportResult)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return true;
    }
    else
    {
        const std::string message = opName + " failed. Error: " + std::string{errorString};
        logMsg(m::logError, message, ELogWriter::SDK);
        if (report != NoConsoleReport)
        {
            clc_console::conlock(std::cout) << message << std::endl;
        }
        return false;
    }
}

bool check_err(const std::string& opName, m::MegaError* e, ReportOnConsole report)
{
    return check_err_aux(opName, e->getErrorCode(), e->getErrorString(), report, ELogWriter::SDK);
}

bool check_err(const std::string& opName, c::MegaChatError* e, ReportOnConsole report)
{
    return check_err_aux(opName,
                         e->getErrorCode(),
                         e->getErrorString(),
                         report,
                         ELogWriter::MEGA_CHAT);
}

bool isUnexpectedErr(const int errCode,
                     const int expectedErrCode,
                     const char* msg,
                     const ELogWriter outWriter)
{
    if (errCode != expectedErrCode)
    {
        logMsg(m::logError,
               std::string("ERROR CODE ") + std::to_string(errCode) + ": " + msg,
               outWriter);
        return true;
    }
    return false;
};

void setLoggers()
{
    // Set log levels to max as it will be controlled by the g_debugOutpuWriter
    m::SimpleLogger::setOutputClass(&g_apiLogger);
    m::SimpleLogger::setLogLevel(m::logMax);
    clc_global::g_chatApi->setLoggerObject(&g_chatLogger);
    clc_global::g_chatApi->setLogLevel(c::MegaChatApi::LOG_LEVEL_MAX);
    clc_global::g_chatApi->setLogWithColors(false);
    clc_global::g_chatApi->setLogToConsole(false);
}

}
