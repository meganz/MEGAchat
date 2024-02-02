#pragma once

#include "mclc_general_utils.h"

#include <mutex>
#include <string>

namespace mclc::clc_log
{

/**
 * @brief This class is meant to manage the writing of the log/debug messages by the Loggers (MegaCLLogger and
 * MegaclcChatChatLogger) into a file and/or the cout.
 *
 * As it is thought to be used by multiple loggers that may be running in different threads it uses a mutex to access
 * the attributes.
 *
 * Usage: There is a variety of methods to enable/disable the writing to a file or to cout and also the minimum level of
 * severity of the messages to write. However the main method to use this class is the `writeOutput`. You should call
 * the method through an instance of this class in a global scope, accessible to all the Loggers you want to coordinate.
 */
class DebugOutputWriter
{
public:
    /**
     * @brief Writes the given message to the file if it was set and to the cout if it was enable.
     *
     * NOTE: The logLevel parameter is compared against an internal level that you can change with the `setLogLevel`
     * method and which has the value of 1 by default (that matches with the `logError` value). If the input
     * logLevel is larger than the internal one the message is not processed. Else, the message will always be written
     * into the file if it was enable but it will be only written to the cout if the logLevel is associated to the error
     * or fatal error level. This is done to avoid massive writing to the cout which could occasionally difficult the
     * command input.
     *
     * @param msg The message to write
     * @param logLevel The severity of the message. See the `LogLevel` enum in the `mega/logging.h` file.
     */
    void writeOutput(const std::basic_string<char>& msg, int logLevel);

    void disableLogToConsole() { setLogToConsole(false); }

    void enableLogToConsole() { setLogToConsole(true); }

    void disableLogToFile();

    /**
     * @brief Enables the writing to a file.
     *
     * If a file was already opened, it will be closed before opening the new one.
     *
     * NOTE: If the file name to open matches the previously opened file, it will be overwritten.
     *
     * @param fname The path to the file to write the messages.
     */
    void enableLogToFile(const std::string& fname);

    bool isLoggingToFile() const;

    bool isLoggingToConsole() const ;

    std::string getLogFileName() const;

    void setLogLevel(int newLogLevel);

private:

    void setLogToConsole(bool state);

    std::ofstream mLogFile;
    std::string mLogFileName;
    mutable std::mutex mLogFileWriteMutex;
    int mCurrentLogLevel = 1;
    bool mLogToConsole = false;
};

class MegaCLLogger : public m::Logger
{
public:
    void logMsg(const int loglevel, const std::string& message);

private:
    void log(const char* time, int loglevel, const char*, const char *message
#ifdef ENABLE_LOG_PERFORMANCE
        , const char** directMessages = nullptr, size_t* directMessagesSizes = nullptr, unsigned numberMessages = 0
#endif
    ) override;
};

struct MegaclcChatChatLogger : public c::MegaChatLogger
{
public:
    void logMsg(const int loglevel, const std::string& message);

private:
    void log(int loglevel, const char *message) override;
};

enum ReportOnConsole { NoConsoleReport, ReportFailure, ReportResult };

bool check_err(const std::string& opName, m::MegaError* e, ReportOnConsole report = NoConsoleReport);

bool check_err(const std::string& opName, c::MegaChatError* e, ReportOnConsole report = NoConsoleReport);

bool isUnexpectedErr(const int errCode, const int expectedErrCode, const char* msg);

extern DebugOutputWriter g_debugOutpuWriter;
extern MegaCLLogger g_apiLogger;
extern MegaclcChatChatLogger g_chatLogger;

}
