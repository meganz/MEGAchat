/**
 * @file examples/megaclc.cpp
 * (c) 2018-2018 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 *
 * @brief Sample application, interactive command line chat client
 * This program is intended for exploring the chat API, performing testing and so on.
 * It's not well tested and should be considered alpha at best.
 *
 */

#include "mclc_autocompletion.h"
#include "mclc_general_utils.h"
#include "mclc_globals.h"
#include "mclc_logging.h"
#include "mclc_resources.h"

#include <csignal>

namespace mclc
{

/**
 * @brief Main loop of the application.
 */
void megaclc()
{
#ifndef NO_READLINE
    char* saved_line = NULL;
    int saved_point = 0;
    rl_attempted_completion_function = clc_ac::my_rl_completion;
    rl_catch_signals = 0; // Avoid readline to manage the signals for us

    rl_save_prompt();
#elif defined(WIN32) && defined(NO_READLINE)
    static_cast<m::WinConsole*>(clc_global::console.get())
        ->setShellConsole(CP_UTF8, GetConsoleOutputCP());
#else
#error non-windows platforms must use the readline library
#endif

#if defined(WIN32) && defined(NO_READLINE)
    char pw_buf[512]; // double space for unicode
#else
    char pw_buf[256];
#endif

    for (;;)
    {
        if (clc_global::g_prompt == clc_prompt::COMMAND)
        {
#if defined(WIN32) && defined(NO_READLINE)
            static_cast<m::WinConsole*>(clc_global::console.get())
                ->updateInputPrompt(clc_prompt::prompts[clc_prompt::COMMAND]);
#else
            rl_callback_handler_install(clc_prompt::prompts[clc_prompt::COMMAND],
                                        clc_prompt::store_line);

            // display prompt
            if (saved_line)
            {
                rl_replace_line(saved_line, 0);
                free(saved_line);
            }

            rl_point = saved_point;
            rl_redisplay();
#endif
        }

        // command editing loop - exits when a line is submitted or the engine requires the CPU
        while (!clc_global::g_promptLine)
        {
            clc_time::WaitMillisec(1);

#ifdef NO_READLINE
            {
                auto cl = console::conlock(std::cout);
                static_cast<m::WinConsole*>(clc_global::console.get())->consolePeek();
                if (clc_global::prompt >= clc_prompt::COMMAND && !clc_global::line)
                {
                    clc_global::line = static_cast<m::WinConsole*>(clc_global::console.get())
                                           ->checkForCompletedInputLine();
                }
            }
#else
            if (clc_global::g_prompt == clc_prompt::COMMAND)
            {
                rl_callback_read_char();
            }
            else if (clc_global::g_prompt > clc_prompt::COMMAND)
            {
                clc_global::g_console->readpwchar(pw_buf,
                                                  sizeof pw_buf,
                                                  &clc_global::g_promptPwBufPos,
                                                  &clc_global::g_promptLine);
            }
#endif

            if (clc_global::g_signalPresencePeriod > 0 &&
                clc_global::g_signalPresenceLastSent + clc_global::g_signalPresencePeriod <
                    std::time(NULL))
            {
                clc_global::g_chatApi->signalPresenceActivity(&clc_global::g_chatListener);
                clc_global::g_signalPresenceLastSent = std::time(NULL);
            }

            if (clc_global::g_repeatPeriod > 0 && !clc_global::g_repeatCommand.empty() &&
                clc_global::g_repeatLastSent + clc_global::g_repeatPeriod < std::time(NULL))
            {
                clc_global::g_repeatLastSent = std::time(NULL);
                clc_prompt::process_line(clc_global::g_repeatCommand.c_str());
            }
        }

#ifndef NO_READLINE
        // save line
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);

        // remove prompt
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
#endif

        if (clc_global::g_promptLine)
        {
            // execute user command
            clc_prompt::process_line(clc_global::g_promptLine);
            free(clc_global::g_promptLine);
            clc_global::g_promptLine = NULL;

            if (clc_global::g_promptQuitFlag)
            {
                return;
            }
        }
    }
}

}

int main()
{
    std::signal(SIGINT, mclc::clc_prompt::signal_handler);
    std::signal(SIGTERM, mclc::clc_prompt::signal_handler);

    mclc::clc_resources::appAllocate();
    mclc::megaclc();
    mclc::clc_resources::appClean();
}
