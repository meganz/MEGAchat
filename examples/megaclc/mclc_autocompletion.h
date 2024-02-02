#pragma once

#include <mega/autocomplete.h>
namespace ac = ::mega::autocomplete;

#include <vector>
#include <string>


namespace mclc::clc_ac
{

bool extractflag(const std::string& flag, std::vector<ac::ACState::quoted_word>& words);

bool extractflagparam(const std::string& flag, std::string& param, std::vector<ac::ACState::quoted_word>& words);

#ifndef NO_READLINE
#ifdef HAVE_AUTOCOMPLETE
char* longestCommonPrefix(ac::CompletionState& acs);
char** my_rl_completion(const char *, int , int end);
#endif
#endif


ac::ACN autocompleteSyntax();

}
