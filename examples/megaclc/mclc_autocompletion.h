/**
 * @file
 * @brief This file defines all the utilities related with the autocompletion of the commands in the
 * cli.
 */

#pragma once

#include <mega/autocomplete.h>
namespace ac = ::mega::autocomplete;

#include <string>
#include <vector>

namespace mclc::clc_ac
{

/**
 * @brief Looks for the flag in the words vector and if it is there, removes from it and returns
 * true.
 */
bool extractflag(const std::string& flag, std::vector<ac::ACState::quoted_word>& words);

/**
 * @brief Looks for the flag in the words vector and if it is there, assigns the next value in the
 * vector to the param argument and removes from the vector both the flag and the next value.
 */
bool extractflagparam(const std::string& flag,
                      std::string& param,
                      std::vector<ac::ACState::quoted_word>& words);

#ifndef NO_READLINE
#ifdef HAVE_AUTOCOMPLETE
char* longestCommonPrefix(ac::CompletionState& acs);
char** my_rl_completion(const char*, int, int end);
#endif
#endif

/**
 * @brief This function is the one that creates an autocompletion node with all the available
 * commands.
 *
 * If you want to create a new command start registering it in the body of this function. You will
 * find there examples of how to define arguments, flags, optionals... Basically you need to bind a
 * function to its user interface.
 *
 * By convention, all the functions attached to a command are named as "exec_command" where command
 * is the cli name of the command.
 */
ac::ACN autocompleteSyntax();

}
