#ifndef MCLC_GENERAL_UTILS_H
#define MCLC_GENERAL_UTILS_H

/**
 * @file
 * @brief This file contains some generic utilities that are useful for other parts of the
 * application.
 */

#include <mega.h>
namespace m = ::mega;

#include <megachatapi.h>
namespace c = ::megachat;

#include <filesystem>
#include <karereId.h>
#include <string>
namespace fs = std::filesystem;
#include <fstream>

namespace mclc
{

namespace cli_utils
{
/**
 * @brief Converts the input arguments received by the main function into a vector of strings
 */
std::vector<std::string> argsToVec(int argc, char* argv[]);
}

namespace path_utils
{

/**
 * @brief Cross platform getid()
 */
unsigned long getProcessId();

/**
 * @brief Returns the directory associated to the running process
 */
fs::path getExeDirectory();

/**
 * @brief Returns the users home directory ($HOME or $USERPROFILE)
 */
fs::path getHomeDirectory();

/**
 * @brief Finds the MegaNode corresponding to the input path
 *
 * @param path path to the file/directory
 * @return An unique ptr to the node
 */
std::unique_ptr<m::MegaNode> GetNodeByPath(const std::string& path);

/**
 * @brief Builds a path object from a string. If it is empty the current working directory will be
 * returned.
 *
 * @param s The string to convert
 * @param mustexist If true, the output path must exists otherwise an error message will be written
 * and the current working directory will be returned.
 * @return The path object
 */
fs::path pathFromLocalPath(const std::string& s, bool mustexist);

}

namespace str_utils
{

/**
 * @brief Extracts a link to a chat from an arbitrary message and returns it as a std::string
 *
 * Chat links look like this:
 * https://mega.nz/chat/E1foobar#EFa7vexblahJwjNglfooxg
 *                      ^handle  ^key
 *
 * @param message The message where the link is expected to be.
 * @return A std::string with the link. If no link is found returns an empty string.
 */
std::string extractChatLink(const char* message);

/**
 * @brief Convert a string into a chat handle
 */
c::MegaChatHandle s_ch(const std::string& s);

/**
 * @brief Convert a chat handle into a string
 */
std::string ch_s(c::MegaChatHandle h);

/**
 * @brief Returns a std::string with the contents of s and deletes s
 */
std::string OwnStr(const char* s);

/**
 * @brief Returns a string with the base64 representation of a node handle
 */
std::string base64NodeHandle(m::MegaHandle h);

/**
 * @brief Returns a string with the base64 representation of a chat handle
 */
std::string base64ChatHandle(m::MegaHandle h);

/**
 * @brief Converts a string with a binary representation of a number into its hexadecimal
 * representation.
 */
std::string tohex(const std::string& binary);

/**
 * @brief The inverse of tohex
 */
std::string tobinary(const std::string& hex);

/**
 * @brief Maps characters [0-9a-z] to numbers sequentially starting from 0. If the char is not in
 * the range, returns 0
 */
unsigned char tobinary(unsigned char c);

/**
 * @brief Reads a file and returns its contents as an string
 */
std::string loadfile(const std::string& filename);

/**
 * @brief Joins all the strings listed in msl with a given separator in between the elements.
 */
std::string joinStringList(const m::MegaStringList& msl, const std::string& separator);

/**
 * @brief Generic implementation of the joinStringList that accepts any kind of iterator of strings
 */
template<typename Iter>
std::string join(Iter begin, Iter end, const std::string& separator)
{
    std::string result;
    if (begin != end)
    {
        result += *begin++;
    }
    while (begin != end)
    {
        result += separator + *begin++;
    }
    return result;
}

}

namespace clc_console
{

/**
 * @class ConsoleLock
 * @brief This struct allows you to lock an output so you can print messages to it without race
 * conditions.
 *
 */
struct ConsoleLock
{
    static std::recursive_mutex outputlock;
    std::ostream& os;
    bool locking = false;
    ConsoleLock(std::ostream& o);

    ConsoleLock(ConsoleLock&& o);
    ~ConsoleLock();

    template<class T>
    std::ostream& operator<<(T&& arg)
    {
        return os << std::forward<T>(arg);
    }
};

/**
 * @brief Returns a temporary object that has locked a mutex. The temporary's destructor will unlock
 * the object.
 *
 * You can get multithreaded non-interleaved console output with just:
 *     conlock(cout) << "some " << "strings " << endl;
 * As the temporary's destructor will run at the end of the outermost enclosing expression.
 *
 * Or, move-assign the temporary to an lvalue to control when the destructor runs (to lock output
 * over several statements). Be careful not to have cout locked across a g_megaApi member function
 * call, as any callbacks that also log could then deadlock.
 *
 * @param o The objects that holds the output resource.
 */
ConsoleLock conlock(std::ostream& o);

}

namespace clc_time
{

/**
 * @brief Blocks the current thread the given amount of milliseconds
 */
void WaitMillisec(unsigned n);

/**
 * @brief Converts the time into a string
 */
std::string timeToLocalTimeString(const int64_t time);

/**
 * @brief Converts the time into a string with the UTC format
 */
std::string timeToStringUTC(const int64_t time);

}
}
#endif // MCLC_GENERAL_UTILS_H
