/**
 * @file
 * @brief Some utilities to process and store the state of the prompt during the megaclc execution.
 */

#pragma once

#ifndef NO_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#ifndef WIN32
// avoid warning C4996 : 'strdup' : The POSIX name for this item is deprecated.Instead, use the ISO
// Cand C++ conformant name : _strdup.See online help for details.
inline char* _strdup(const char* _Source)
{
    return strdup(_Source);
}
#endif

namespace mclc::clc_prompt
{

/**
 * @brief Different promts to show depending on the current state.
 */
static const char* const prompts[] = {"", "MEGAclc> ", "Password:", "Pin:"};

enum prompttype
{
    NOPROMPT,
    COMMAND,
    LOGINPASSWORD,
    PIN
};

/**
 * @brief Sets the global variables that stores the state of the prompt
 */
void setprompt(prompttype p);

#if !defined(WIN32) || !defined(NO_READLINE)
// readline callback - exit if EOF, add to history unless password
void store_line(char* l);
#endif

/**
 * @brief Executes the command specified in the input string
 */
void process_line(const char* l);

}
