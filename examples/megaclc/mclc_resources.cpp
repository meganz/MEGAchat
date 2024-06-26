#include "mclc_resources.h"

#include "mclc_autocompletion.h"
#include "mclc_globals.h"
#include "mclc_logging.h"

namespace mclc::clc_resources
{

namespace
{

fs::path getDefaultMegaclcOutDir()
{
    return path_utils::getHomeDirectory() / "temp_MEGAclc";
}

/**
 * @class GlobalOptions
 * @brief Simple struct with information associated to global options.
 *
 */
struct GlobalOptions
{
    std::filesystem::path outputDir{getDefaultMegaclcOutDir()};
};

/**
 * @class AvailableOption
 * @brief Simple struct to store all the needed information for a global option
 *
 * Members:
 *    + flag: The label associated to the option, i.e. --flag=value
 *    + description: A help message describing the option
 *    + fillOperation: A function that takes a GlobalOptions object and the parsed "value". The
 *    purpose of the function is to set the corresponding information in the object based on value.
 */
struct AvailableOption
{
    std::string flag;
    std::string description;
    std::function<void(GlobalOptions&, const std::string&)> fillOperation;
};

/**
 * @brief A vector with all the available global options specs.
 *
 * To add a new option, add a new entry here and add the needed fields in the GlobalOptions struct
 */
const std::vector<AvailableOption> availableGlobalOptions{
    {"global_outdir",
     "Directory to store cache files", [](GlobalOptions& opts, const std::string& value)
     {
         if (value.empty())
         {
             opts.outputDir = getDefaultMegaclcOutDir();
         }
         else
         {
             opts.outputDir = std::filesystem::path(value);
         }
     }}
};

/**
 * @brief A static object with the global options info accessible to the functions that allocates
 * and clean resources.
 */
GlobalOptions g_globalOptions{};

}

void extractGlobalOptions(std::vector<std::string>& args)
{
    for (const auto& availableOption: availableGlobalOptions)
    {
        if (auto it = std::find_if(args.begin(),
                                   args.end(),
                                   [&flag = availableOption.flag](const std::string& arg)
                                   {
                                       return arg.rfind("--" + flag + "=", 0) == 0;
                                   });
            it != args.end())
        {
            const auto equalPos = (*it).find("=");
            const auto optValue = (*it).substr(equalPos + 1);
            availableOption.fillOperation(g_globalOptions, optValue);
            args.erase(it);
        }
    }
}

std::map<std::string, std::string> getAvailableGlobalOptionsDescription()
{
    std::map<std::string, std::string> result;
    for (const auto& opt: availableGlobalOptions)
    {
        result[opt.flag] = opt.description;
    }
    return result;
}

void appAllocate()
{
    using namespace mclc::clc_global;

    // Loggers are stored in global variables so can be setup before instantiating the final apis
    clc_log::setLoggers();

    fs::create_directories(g_globalOptions.outputDir);

    const std::string outPathStr = g_globalOptions.outputDir.string();
    g_megaApi.reset(new m::MegaApi("VmhTTToK", outPathStr.c_str(), "MEGAclc"));
    g_megaApi->addListener(&g_megaclcListener);
    g_megaApi->addGlobalListener(&g_globalListener);
    g_chatApi.reset(new c::MegaChatApi(g_megaApi.get()));
    g_chatApi->addChatListener(&g_clcListener);
#ifndef KARERE_DISABLE_WEBRTC
    g_chatApi->addChatCallListener(&g_clcCallListener);
#endif
    g_chatApi->addChatRequestListener(&g_chatRequestListener);

    g_console.reset(new m::CONSOLE_CLASS);

    g_autocompleteTemplate = clc_ac::autocompleteSyntax();
#ifdef WIN32
    static_cast<m::WinConsole*>(g_console.get())->setAutocompleteSyntax(g_autocompleteTemplate);
#endif
}

void appClean()
{
    using namespace mclc::clc_global;

    g_megaApi->removeListener(&g_megaclcListener);
    g_megaApi->removeGlobalListener(&g_globalListener);
    g_chatApi->removeChatListener(&g_clcListener);
#ifndef KARERE_DISABLE_WEBRTC
    g_chatApi->removeChatCallListener(&g_clcCallListener);
#endif
    g_chatApi->removeChatRequestListener(&g_chatRequestListener);

    g_chatApi.reset();
    g_megaApi.reset();
    g_console.reset();

    // If the output dir is empty remove it (as we have created it in the appAllocate stage)
    if (fs::exists(g_globalOptions.outputDir) && fs::is_directory(g_globalOptions.outputDir) &&
        fs::is_empty(g_globalOptions.outputDir))
    {
        fs::remove(g_globalOptions.outputDir);
    }
}
}
