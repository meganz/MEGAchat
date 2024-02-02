#pragma once

#ifndef NO_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifndef WIN32
// avoid warning C4996 : 'strdup' : The POSIX name for this item is deprecated.Instead, use the ISO Cand C++ conformant name : _strdup.See online help for details.
inline char* _strdup(char const* _Source) { return strdup(_Source); }
#endif

namespace mclc::clc_prompt
{

static const char* const prompts[] =
{
    "", "MEGAclc> ", "Password:", "Pin:"
};

enum prompttype
{
    NOPROMPT, COMMAND, LOGINPASSWORD, PIN
};


void setprompt(prompttype p);

#if !defined(WIN32) || !defined(NO_READLINE)
// readline callback - exit if EOF, add to history unless password
void store_line(char* l);
#endif

// execute command
void process_line(const char* l);

}
