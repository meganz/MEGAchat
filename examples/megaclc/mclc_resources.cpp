#include "mclc_resources.h"

#include "mclc_globals.h"
#include "mclc_logging.h"
#include "mclc_autocompletion.h"


namespace mclc::clc_resources
{

void appAllocate()
{
    using namespace mclc::clc_global;

    clc_log::setLoggers();

    const std::string megaclc_path = "temp_MEGAclc";
#ifdef WIN32
    const std::string basePath = (fs::u8path(getenv("USERPROFILE")) / megaclc_path).u8string();
    fs::create_directories(basePath);
#else
    // No std::fileystem before OSX10.15
    const std::string basePath = getenv("HOME") + std::string{'/'} + megaclc_path;
    mkdir(basePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

    g_megaApi.reset(new m::MegaApi("VmhTTToK", basePath.c_str(), "MEGAclc"));
    g_megaApi->addListener(&g_megaclcListener);
    g_megaApi->addGlobalListener(&g_globalListener);
    g_chatApi.reset(new c::MegaChatApi(g_megaApi.get()));
    g_chatApi->setLogLevel(c::MegaChatApi::LOG_LEVEL_MAX);
    g_chatApi->setLogWithColors(false);
    g_chatApi->setLogToConsole(false);
    g_chatApi->addChatListener(&g_clcListener);
    g_chatApi->addChatCallListener(&g_clcCallListener);

    g_console.reset(new m::CONSOLE_CLASS);

    g_autocompleteTemplate = clc_ac::autocompleteSyntax();
#ifdef WIN32
    static_cast<m::WinConsole*>(console.get())->setAutocompleteSyntax(autocompleteTemplate);
#endif

}

void appClean()
{
    using namespace mclc::clc_global;

    g_megaApi->removeListener(&g_megaclcListener);
    g_megaApi->removeGlobalListener(&g_globalListener);
    g_chatApi->removeChatListener(&g_clcListener);
    g_chatApi->removeChatCallListener(&g_clcCallListener);

    g_chatApi.reset();
    g_megaApi.reset();
    g_console.reset();
}

}
