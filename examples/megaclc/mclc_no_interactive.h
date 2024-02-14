/**
 * @file
 * @brief This file defines all the utils to manage and process commands run in
 * non interactive mode.
 */

#pragma once

#include <boost/program_options.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace po = boost::program_options;

namespace mclc
{

/**
 * @brief Process the input arguments and evaluates them appropriately.
 *
 * @param argc The number of elements in argv
 * @param argv An array with the arguments passed by the command line. The name
 * of the binary must not be included in the array
 * @return The error code. 0 if everything succeed.
 */
int noInteractiveCommand(int argc, char* argv[]);

namespace clc_noint
{

class JoinCallViaMeetingLink
{
public:
    JoinCallViaMeetingLink();
    int operator()(int argc, char* argv[]);

    static inline const char* OPT_HELP = "help";
    static inline const char* OPT_DEBUG = "debug";
    static inline const char* OPT_VIDEO = "video";
    static inline const char* OPT_AUDIO = "audio";
    static inline const char* OPT_VIDEO_DEV = "video-in-device";
    static inline const char* OPT_WAIT = "wait";
    static inline const char* OPT_EMAIL = "email";
    static inline const char* OPT_PASS = "password";
    static inline const char* OPT_URL = "url";

private:
    po::options_description mDesc;
    void validateInput(const po::variables_map& variablesMap);
    void runCommand(const po::variables_map& variablesMap);
};

class Help
{
public:
    Help();
    int operator()(int argc, char* argv[]);

private:
    std::string mHelpMsg;
};

class Test
{
public:
    int operator()(int argc, char* argv[]);
};

typedef std::variant<JoinCallViaMeetingLink, Help, Test> AvailableCommands;

// clang-format off
static const std::map<std::string_view, std::function<AvailableCommands()>, std::less<>> strToCommands{
    {"help",                   []() { return Help(); }},
    {"-h",                     []() { return Help(); }},
    {"--help",                 []() { return Help(); }},
    {"joinCallViaMeetingLink", []() { return JoinCallViaMeetingLink(); }},
    {"test",                   []() { return Test(); }},
};
// clang-format on

std::optional<AvailableCommands> commandFactory(const std::string_view& commandName);

}

} // namespace mclc
