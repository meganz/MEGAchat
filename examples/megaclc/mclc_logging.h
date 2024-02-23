/**
 * @file
 * @brief This file defines the utilities to log messages to the console or to a file as well as
 * some error handling functions that make use of the logging methods.
 *
 * API
 * ---
 * There is one global instance of the DebugOutputWriter class (g_debugOutpuWriter) that is
 * responsible of knowing where the log messages must be written and also apply a filter according
 * to the severity of the notifications. Yo can interact with this object to modify this parameters.
 *
 * Then there are internally two loggers (for now), one for logging messages related to the sdk api
 * and another one to log messages related to the mega chat api. They differ basically in the label
 * the put at the beginning of the message. They are also used to set the loggers in the internals
 * of both APIs using the setLoggers function.
 *
 * To call the logging methods of these loggers you should call the logMsg function that will be
 * responsible of dispatching the messages to the corresponding logger.
 *
 */

#pragma once

#include "mclc_general_utils.h"

#include <mutex>
#include <string>

namespace mclc::clc_log
{

/**
 * @brief This class is meant to manage the writing of the log/debug messages by the Loggers
 * (MegaCLLogger and MegaclcChatChatLogger) into a file and/or the cout.
 *
 * As it is thought to be used by multiple loggers that may be running in different threads it uses
 * a mutex to access the attributes.
 *
 * Usage: There is a variety of methods to enable/disable the writing to a file or to cout and also
 * the minimum level of severity of the messages to write. However the main method to use this class
 * is the `writeOutput`. You should call the method through an instance of this class in a global
 * scope, accessible to all the Loggers you want to coordinate.
 */
class DebugOutputWriter
{
public:
    /**
     * @brief Writes the given message to the file if it was set and to the cout if it was enable.
     *
     * NOTE: The logLevel parameter is compared against an internal level that you can change with
     * the `setLogLevel` method and which has the value of 1 by default (that matches with the
     * `logError` value). If the input logLevel is larger than the internal one the message is not
     * processed. Else, the message will always be written into the file if it was enable but it
     * will be only written to the cout if the logLevel is associated to the error or fatal error
     * level. This is done to avoid massive writing to the cout which could occasionally difficult
     * the command input.
     *
     * @param msg The message to write
     * @param logLevel The severity of the message. See the `LogLevel` enum in the `mega/logging.h`
     * file.
     */
    void writeOutput(const std::basic_string<char>& msg, int logLevel);

    void disableLogToConsole()
    {
        setLogToConsole(false);
    }

    void enableLogToConsole()
    {
        setLogToConsole(true);
    }

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
    void enableLogToFile(const fs::path& fname);

    bool isLoggingToFile() const;

    bool isLoggingToConsole() const;

    std::string getLogFileName() const;

    void setConsoleLogLevel(int newLogLevel);
    int getConsoleLogLevel() const;

    void setFileLogLevel(int newLogLevel);
    int getFileLogLevel() const;

private:
    void setLogToConsole(bool state);

    std::ofstream mLogFile;
    fs::path mLogFilePath;
    mutable std::mutex mLogFileWriteMutex;
    int mConsoleLogLevel = 1;
    int mFileLogLevel = 1;
    bool mLogToConsole = false;
};

/**
 * @class MegaCLLogger
 * @brief Defines the logger that will be attached to the sdk api.
 */
class MegaCLLogger: public m::Logger
{
public:
    void logMsg(const int loglevel, const std::string& message);

private:
    void log(const char* time,
             int loglevel,
             const char*,
             const char* message
#ifdef ENABLE_LOG_PERFORMANCE
             ,
             const char** directMessages = nullptr,
             size_t* directMessagesSizes = nullptr,
             unsigned numberMessages = 0
#endif
             ) override;
};

/**
 * @class MegaclcChatChatLogger
 * @brief Defines the logger that will be attached to the mega chat api.
 */
struct MegaclcChatChatLogger: public c::MegaChatLogger
{
public:
    void logMsg(const int loglevel, const std::string& message);

private:
    void log(int loglevel, const char* message) override;
};

enum ReportOnConsole
{
    NoConsoleReport,
    ReportFailure,
    ReportResult
};

// An enum to specify the logging writer
enum class ELogWriter
{
    SDK,
    MEGA_CHAT,
};

/**
 * @brief A wrapper function around the logging methods of the loggers. Depending on the
 * outputWriter, the message is dispatched to the right logger.
 *
 * NOTE: Where the messages are written to is controlled by the g_debugOutpuWriter global object.
 *
 * @param logLevel The severity level of the message
 * @param message The message to write
 * @param outputWriter An enum specifying the output
 */
void logMsg(const int logLevel, const std::string& message, const ELogWriter outputWriter);

/**
 * @brief Checks if the error code in the given error is ERROR_OK. In that case an info message will
 * be pass to the corresponding logger. Otherwise an error message will be pass. This function also
 * allows you to control if the messages are also display in the standard output through the report
 * argument.
 *
 * This overload version calls the SDK logger under the hood.
 *
 * @param opName A name associated to the process that generates the error.
 * @param e The generated error object.
 * @param report An enum that allows you to control when the message should be displayed in the
 * cout. Cases:
 *     - ReportResult: Report both info and error messages.
 *     - NoConsoleReport: Never report.
 *     - ReportFailure: Only report errors.
 * @return e->getErrorCode() == ERROR_OK (0)
 */
bool check_err(const std::string& opName,
               m::MegaError* e,
               ReportOnConsole report = NoConsoleReport);

/**
 * @brief Overloaded version that calls the mega chat logger under the hood.
 */
bool check_err(const std::string& opName,
               c::MegaChatError* e,
               ReportOnConsole report = NoConsoleReport);

/**
 * @brief Compares the given errors and if they are different, call the corresponding logger to
 * write an error message and returns true. If they are the same returns false and nothing else.
 *
 * @param errCode The error code to check
 * @param expectedErrCode The expected error code
 * @param msg The message to log in case of different error. Note that "ERROR CODE errCode:" will be
 * prefixed.
 * @param outWriter The enum to specify the logger class you want to use.
 * @return errCode != expectedErrCode
 */
bool isUnexpectedErr(const int errCode,
                     const int expectedErrCode,
                     const char* msg,
                     const ELogWriter outWriter);

/**
 * @brief Sets the sdk logger as the output class for the m::SimpleLogger and the mega chat logger
 * to the g_chatApi object.
 */
void setLoggers();

/**
 * @brief This global variable acts like a singleton to control the state of the loggers.
 */
extern DebugOutputWriter g_debugOutpuWriter;

}
