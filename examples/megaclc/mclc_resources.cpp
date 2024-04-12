#include "mclc_resources.h"

#include "mclc_autocompletion.h"
#include "mclc_globals.h"
#include "mclc_logging.h"

namespace mclc::clc_resources
{

void appAllocate()
{
    using namespace mclc::clc_global;

    // Loggers are stored in global variables so can be setup before instantiating the final apis
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
}

}
