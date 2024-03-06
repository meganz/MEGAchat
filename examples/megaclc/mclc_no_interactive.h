#ifndef MCLC_NO_INTERACTIVE_H
#define MCLC_NO_INTERACTIVE_H

/**
 * @file
 * @brief This file defines all the utils to manage and process commands run in non interactive
 * mode.
 *
 * The entry point for running non interactive commands is the `noInteractiveCommand` function that
 * receives the input cli arguments without the first element (the name of the megaclc binary). This
 * function will process the input and depending on the requested utility, run the appropriate
 * command.
 *
 * All the dependencies that this function relies on are inside the clc_noint (no-interactive)
 * namespace.
 *
 * The function that converts a command into something that executes the action is called
 * `commandFactory`. To do this job, the function looks up into the `strToCommands` map to find the
 * associated actor to a command. This actors must be something that can be called as a function
 * that takes as arguments: int argc and char* argv[] (the cli arguments without the megaclc
 * binary). For convenience, most of this actors are functors however, as we are not relying on
 * classic polymorphism, they can be functions or lambdas.
 *
 * The steps to add a new command would be:
 *     1. Add an entry to the `strToCommands` map with the name of the command you want to run in
 *     the terminal as the key and a callable without arguments that returns the actor described
 *     above (see the existing examples for inspiration).
 *     2. Add the type of the actor to the `AvailableCommands` variant definition.
 *     3. Define the actor implementation. See for instance the implementation of the
 *     JoinCallViaMeetingLink class to see how to parse cli arguments with boost program_options
 *     library.
 */

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

/**
 * @class JoinCallViaMeetingLink
 * @brief The functor to execute the joinCallViaMeetingLink no interactive command.
 *
 * See the help of the function (-h or --help) for more information.
 */
class JoinCallViaMeetingLink
{
public:
    JoinCallViaMeetingLink();
    int operator()(int argc, char* argv[]);

    // Here the labels associated to the options are declared to facilitate modifications
    static inline const char* OPT_HELP = "help";
    static inline const char* OPT_DEBUG = "debug";
    static inline const char* OPT_VIDEO = "video";
    static inline const char* OPT_AUDIO = "audio";
    static inline const char* OPT_VIDEO_DEV = "video-in-device";
    static inline const char* OPT_WAIT = "wait";
    static inline const char* OPT_DEFAULT_WAIT = "40";
    static inline const char* OPT_EMAIL = "email";
    static inline const char* OPT_PASS = "password";
    static inline const char* OPT_URL = "url";

private:
    po::options_description mDesc; // Description of the command with the options
    std::string mHelpMessage; // Help message with the specifications of the command.

    /**
     * @brief Validates the input variables checking for missing parameters or wrong values.
     *
     * @param variablesMap Parsed program_options variables
     */
    void validateInput(const po::variables_map& variablesMap);

    /**
     * @brief With the validated options executes the functuonality
     *
     * @param variablesMap Parsed program_options variables
     */
    void runCommand(const po::variables_map& variablesMap);

    /**
     * @brief Builds the command to call the joinCallViaMeetingLink function using the input
     * arguments.
     *
     * @param variablesMap Parsed program_options variables
     */
    std::string buildJoinCallCommand(const po::variables_map& variablesMap);
};

/**
 * @class Help
 * @brief The functor that defines the help command operations
 *
 * It basically prints out how you should use the tool and the available commands.
 */
class Help
{
public:
    Help();
    int operator()(int argc, char* argv[]);

private:
    std::string mHelpMsg;
};

typedef std::variant<JoinCallViaMeetingLink, Help> AvailableCommands;

// clang-format off
static const std::map<std::string_view, std::function<AvailableCommands()>, std::less<>> strToCommands{
    {"help",                   []() { return Help(); }},
    {"-h",                     []() { return Help(); }},
    {"--help",                 []() { return Help(); }},
    {"joinCallViaMeetingLink", []() { return JoinCallViaMeetingLink(); }},
};
// clang-format on

/**
 * @brief Given a command name, returns an std::optional value with the corresponding actor.
 * The optional will be empty if the command is not found.
 *
 * @param commandName
 */
std::optional<AvailableCommands> commandFactory(const std::string_view& commandName);

}

} // namespace mclc
#endif // MCLC_NO_INTERACTIVE_H
