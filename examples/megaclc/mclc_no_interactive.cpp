#include "mclc_no_interactive.h"

#include "mclc_chat_and_call_actions.h"
#include "mclc_general_utils.h"
#include "mclc_globals.h"
#include "mclc_logging.h"
#include "mclc_prompt.h"
#include "mclc_resources.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

namespace mclc
{

int noInteractiveCommand(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        return 0; // nothing to do
    }
    auto command = clc_noint::commandFactory(args[0]);
    if (!command)
    {
        std::cerr << "Invalid command '" << args[0] << "'. The available options are:\n";
        for (auto [k, _]: clc_noint::strToCommands)
        {
            std::cerr << "    + " + std::string(k) + "\n";
        }
        return 1;
    }
    return std::visit(
        [&args](auto&& cmd)
        {
            return cmd(args);
        },
        *command);
}

namespace clc_noint
{
#ifndef KARERE_DISABLE_WEBRTC
JoinCallViaMeetingLink::JoinCallViaMeetingLink():
    mDesc{"Options for JoinCallViaMeetingLink command"}
{
    // clang-format off
    mDesc.add_options()
        ("help,h", "Show help message")
        ((OPT_DEBUG + std::string(",d")).c_str(), "If specified, log messages will be printed to the file ./<pid>/joinCall.log")
        (OPT_VIDEO, po::value<std::string>()->default_value("no"), "[yes|no] If yes, tries to call with video")
        (OPT_AUDIO, po::value<std::string>()->default_value("no"), "[yes|no] If yes, tries to call with audio")
        (OPT_VIDEO_DEV, po::value<std::string>(), "The input video device name to use in the call")
        (OPT_RECV_NUM_LOWRES, po::value<std::string>()->default_value(OPT_DEFAULT_RECV_NUM_LOWRES), "Number of low resolution videos to retrieve from participants. -1 means try to get video from all the participants")
        (OPT_RECV_NUM_HIRES, po::value<std::string>()->default_value(OPT_DEFAULT_RECV_NUM_HIRES), "Number of high resolution videos to retrieve from participants. -1 means try to get video from all the participants")
        (OPT_WAIT, po::value<std::string>()->default_value(OPT_DEFAULT_WAIT), "Time to stay in the call. 0 means never hang up.")
        ((OPT_EMAIL + std::string(",e")).c_str(), po::value<std::string>(), "User's email to login")
        ((OPT_PASS + std::string(",p")).c_str(), po::value<std::string>(), "User's password to login")
        ((OPT_URL + std::string(",l")).c_str(), po::value<std::string>(), "Link to the chat to join and receive the call")
        ;
    // clang-format on
    mHelpMessage = R"(
The JoinCallViaMeetingLink command performs the following steps:
    1.  Ensures there is no logged in account
    2.  Logs in with the given credentials
    3.  Opens a chat room link in preview mode
    4.  Joins the chat
    5.  Waits for the incoming call
    6.  Answers the call with or without video/audio enabled depending on the
        given options
    7.  Sends the appropriate requests to retrieve the video in high/low
        resolutions for the given ammount of participants.
    8.  Stays in the call for the given amount of time
    9.  Hangs up the call
    10. Logs out

NOTE: The requests for high and low res video of the participants are mutually
exclusive, i.e. only one of both resolutions will be asked for a single user,
having preference the low resolution video. This means that if you set to -1
the number of low res video (or to a value greater or equal to the number of
participants) and all the participants are able to send low resolution, no
matter what number of high res video you write as it will not be satisfied.

)";
}

int JoinCallViaMeetingLink::operator()(const std::vector<std::string>& args)
{
    po::variables_map variablesMap;
    try
    {
        po::store(po::command_line_parser(args).options(mDesc).run(), variablesMap);
        po::notify(variablesMap);

        if (variablesMap.count("help"))
        {
            std::cout << mHelpMessage << mDesc << "\n";
            return 0;
        }
        validateInput(variablesMap);
        runCommand(variablesMap);
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown error.\n";
        return 1;
    }
    return 0;
}

void JoinCallViaMeetingLink::validateInput(const po::variables_map& variablesMap)
{
    const std::vector<const char*> mandatoryFields{OPT_EMAIL, OPT_PASS, OPT_URL};
    std::vector<std::string> errorMsgs;
    for (auto& field: mandatoryFields)
    {
        if (variablesMap.count(field) == 0)
        {
            errorMsgs.push_back("Missing required option " + std::string(field));
        }
    }

    const std::vector<const char*> yesNoFields{OPT_AUDIO, OPT_VIDEO};
    for (auto& field: yesNoFields)
    {
        const std::string value = variablesMap[field].as<std::string>();
        if (value != "yes" && value != "no")
        {
            errorMsgs.push_back("The " + std::string(field) + " option must be yes or no");
        }
    }
    if (!errorMsgs.empty())
    {
        std::string errMsg = "\n- " + str_utils::join(errorMsgs.begin(), errorMsgs.end(), "\n- ");
        throw std::logic_error(errMsg);
    }
}

void JoinCallViaMeetingLink::runCommand(const po::variables_map& variablesMap)
{
    clc_prompt::setprompt(clc_prompt::COMMAND);
    if (variablesMap.count(OPT_DEBUG) != 0)
    {
        clc_prompt::process_line("debug -pid -console warning -file all joinCall.log");
        if (clc_log::g_debugOutpuWriter.getLogFileName() != "joinCall.log" ||
            clc_log::g_debugOutpuWriter.getFileLogLevel() != m::logMax ||
            clc_log::g_debugOutpuWriter.getConsoleLogLevel() != m::logWarning)
        {
            throw std::logic_error("Unable to set the debug logging");
        }
    }
    clc_log::logMsg(m::logDebug, "Ensuring logout", clc_log::ELogWriter::SDK);
    if (!clc_ccactions::ensureLogout())
    {
        throw std::logic_error("Unable to ensure logout");
    }
    clc_log::logMsg(m::logDebug, "Logging in", clc_log::ELogWriter::SDK);
    auto email = variablesMap[OPT_EMAIL].as<std::string>();
    auto password = variablesMap[OPT_PASS].as<std::string>();
    if (!clc_ccactions::login(email.c_str(), password.c_str()))
    {
        throw std::logic_error("Unable to login");
    }
    clc_log::logMsg(m::logDebug,
                    "Running joinCallViaMeetingLink commnad",
                    clc_log::ELogWriter::SDK);
    clc_prompt::process_line(buildJoinCallCommand(variablesMap).c_str());
    if (!clc_ccactions::logout())
    {
        throw std::logic_error("Unable to logout after joining the call");
    }
    return;
}

std::string JoinCallViaMeetingLink::buildJoinCallCommand(const po::variables_map& variablesMap)
{
    std::ostringstream joinCommand;
    joinCommand << "joinCallViaMeetingLink";
    if (variablesMap[OPT_VIDEO].as<std::string>() == "no")
    {
        joinCommand << " -novideo";
    }
    if (variablesMap[OPT_AUDIO].as<std::string>() == "no")
    {
        joinCommand << " -noaudio";
    }
    joinCommand << " -wait " << variablesMap[OPT_WAIT].as<std::string>();
    joinCommand << " -recvNumLowVideos " << variablesMap[OPT_RECV_NUM_LOWRES].as<std::string>();
    joinCommand << " -recvNumHighVideos " << variablesMap[OPT_RECV_NUM_HIRES].as<std::string>();
    if (variablesMap.count("video-in-device") != 0)
    {
        joinCommand << " -videoInputDevice \"" << variablesMap[OPT_VIDEO_DEV].as<std::string>()
                    << "\"";
    }
    joinCommand << " " << variablesMap[OPT_URL].as<std::string>();
    return joinCommand.str();
}
#endif

Help::Help()
{
    mHelpMsg = R"(
You are using the megaclc app. This app offers a cli oriented client to consume
the MEGAChat API.

NOTE: You can safely kill the execution of the app with a SIGINT (C-c) or a
SIGTERM signal (kill <pid>).

The application needs to store some cache data that will, by default, be saved
in the $HOME/temp_MEGAclc/ directory (it will be created if it doesn't exist).
You can change this behavior with a global option (see bellow).

To use the application in interactive mode you can run the binary without any
argument. This initializes a terminal to run commands interactively. If you
type "help" in the terminal you will see the list of available commands.

To run non interactive commands you must provide the name of the utility you
want to run after the name of the megaclc binary and also the options (if it
accepts any). For example:

    megaclc joinCallViaMeetingLink -h

Currently, the available utilities are:

)";
    for (auto [k, _]: strToCommands)
    {
        mHelpMsg += "    + " + std::string(k) + "\n";
    }
    mHelpMsg += R"(
Apart from the options that each of these commands may accept, there are some
special options that can be set to modify the behavior of the app in both
interactive and non-interactive modes. These are all optionals and it is
important that they are written in the format: --option_flag=value (you could
add quotes around the value if needed but don't forget the = sign). They can be
placed anywhere in the command (of course, after the megaclc executable name).
The available global options are:

)";
    const auto globalOptions = clc_resources::getAvailableGlobalOptionsDescription();
    for (const auto& [flag, desc]: globalOptions)
    {
        mHelpMsg += "    --" + flag + ": " + desc + "\n";
    }
}

int Help::operator()(const std::vector<std::string>& args)
{
    if (args.size() > 1)
    {
        std::cerr << "Help command does not accept additional argumetns\n";
        return 1;
    }
    std::cout << mHelpMsg;
    return 0;
}

std::optional<AvailableCommands> commandFactory(const std::string_view& commandName)
{
    if (auto it = strToCommands.find(commandName); it != strToCommands.end())
    {
        return it->second();
    }
    return {};
}
}

} // namespace mclc
