#include "mclc_resources.h"

#include "mclc_autocompletion.h"
#include "mclc_globals.h"
#include "mclc_logging.h"

namespace mclc::clc_resources
{

namespace
{
fs::path getMegaclcOutDir()
{
    if (const std::string userDefinedOutPath{getenv("MEGACLC_OUT_DIR")}; userDefinedOutPath.empty())
    {
        return path_utils::getHomeDirectory() / "temp_MEGAclc";
    }
    else
    {
        return fs::path(userDefinedOutPath);
    }
}
}

void appAllocate()
{
    using namespace mclc::clc_global;

    // Loggers are stored in global variables so can be setup before instantiating the final apis
    clc_log::setLoggers();

    const fs::path megaclcOutPath = getMegaclcOutDir();
    fs::create_directories(megaclcOutPath);

    g_megaApi.reset(new m::MegaApi("VmhTTToK", megaclcOutPath.c_str(), "MEGAclc"));
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

    if (const fs::path outPath = getMegaclcOutDir();
        fs::exists(outPath) && fs::is_directory(outPath) && fs::is_empty(outPath))
    {
        fs::remove(outPath);
    }
}

}
