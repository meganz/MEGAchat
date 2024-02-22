#include "mclc_prompt.h"

#include "mclc_general_utils.h"
#include "mclc_globals.h"

#include <mega/autocomplete.h>
namespace ac = ::mega::autocomplete;

#include <iostream>

namespace mclc::clc_prompt
{

void setprompt(prompttype p)
{
    auto cl = clc_console::conlock(std::cout);

    clc_global::g_prompt = p;

    if (p == COMMAND)
    {
        clc_global::g_console->setecho(true);
        clc_global::g_promptLine = _strdup(""); // causes main loop to iterate and update the prompt
    }
    else
    {
        clc_global::g_promptPwBufPos = 0;
#if defined(WIN32) && defined(NO_READLINE)
        static_cast<m::WinConsole*>(clc_global::clc_console.get())->updateInputPrompt(prompts[p]);
#else
        std::cout << prompts[p] << std::flush;
#endif
        clc_global::g_console->setecho(false);
    }
}

// readline callback - exit if EOF, add to history unless password
#if !defined(WIN32) || !defined(NO_READLINE)
void store_line(char* l)
{
    if (!l)
    {
        clc_global::g_console.reset();
        exit(0);
    }

#ifndef NO_READLINE
    if (*l && clc_global::g_prompt == COMMAND)
    {
        add_history(l);
    }
#endif

    clc_global::g_promptLine = l;
}
#endif

// execute command
void process_line(const char* l)
{
    using namespace mclc::clc_global;
    switch (clc_global::g_prompt)
    {
        case PIN:
        {
            std::string pin = l;
            g_chatApi->init(NULL);
            g_megaApi->multiFactorAuthLogin(g_login.c_str(),
                                            g_password.c_str(),
                                            !pin.empty() ? pin.c_str() : NULL);
            {
                clc_console::conlock(std::cout) << "\nLogging in with 2FA..." << std::endl
                                                << std::flush;
            }
            setprompt(NOPROMPT);
            return;
        }

        case LOGINPASSWORD:
            g_password = l;
            g_chatApi->init(NULL);
            g_megaApi->login(g_login.c_str(), g_password.c_str());
            {
                clc_console::conlock(std::cout) << "\nLogging in..." << std::endl;
            }
            setprompt(NOPROMPT);
            return;

        case COMMAND:
            try
            {
                std::string consoleOutput;
                ac::autoExec(std::string(l),
                             std::string::npos,
                             g_autocompleteTemplate,
                             false,
                             consoleOutput,
                             true); // todo: pass correct unixCompletions flag
                if (!consoleOutput.empty())
                {
                    clc_console::conlock(std::cout) << consoleOutput << std::flush;
                }
            }
            catch (std::exception& e)
            {
                clc_console::conlock(std::cout) << "Command failed: " << e.what() << std::endl;
            }
            return;
        case NOPROMPT:
            clc_console::conlock(std::cout)
                << "\nprocess_line: unexpected prompt type: NOPROMPT" << std::endl;
            return;
    }
}

}
