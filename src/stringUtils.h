#ifndef KARERE_STRINGUTILS_H
#define KARERE_STRINGUTILS_H

#include <map>
#include <stdexcept>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>

namespace karere
{
template<class Cont>
static inline void tokenize(const char* src, const char* delims, Cont& cont)
{
    const char* start = src;
    for (;;)
    {
        for(;;)
        {
            if (!*start)
                return; //reached end of string
            if (!strchr(delims, *start)) //skip leading delims
                break;
            start++;
        }
        const char* pos = start+1;
        //at this point it is guaranteed that pos is not a delim and is not at the end of the string
        for(;;)
        {
            if (!*pos)
            {
                cont.emplace_back(start, pos-start);
                return;
            }

            if (strchr(delims, *pos))
            {
                cont.emplace_back(start, pos-start);
                start = pos+1;
                break;
            }
            pos++;
        }
    }
}

inline static size_t trim(const std::string& str, size_t tstart, size_t tend,
                        size_t& start)
{
    if (tstart >= str.size())
        return 0;
    if (tend >= str.size())
    {
        tend = str.size() - 1;
        if (tend < tstart)
            return 0;
    }
    start = str.find_first_not_of(" \t", tstart);
    if ((start > tend) || (start == std::string::npos))
        return 0;
    size_t end = str.find_last_not_of(" \t", tend); //guaranteed to be in range because we have a non-whitespace char at start
    return end - start + 1;
}

static inline std::string trim(const std::string& str, const char* trimChars=" \t")
{
    size_t start = str.find_first_not_of(trimChars);
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(trimChars);
    if (end == std::string::npos)
        throw std::runtime_error("BUG: find_last_not_of(<whitespace>) returned npos for a supposedly non-empty string");
    if ((start == 0) && end == str.length())
        return str;
    else
        return str.substr(start, end-start+1);
}

static inline size_t findFirstOf(const std::string& str, const char* chars, size_t start, size_t end)
{
    for (size_t i=start; i<end; i++)
        if(strchr(chars, str[i]))
            return i;
    return std::string::npos;
}
static inline size_t findFirstNotOf(const std::string& str, const char* chars, size_t start, size_t end)
{
    for (size_t i=start; i<end; i++)
        if(!strchr(chars, str[i]))
            return i;
    return std::string::npos;
}

enum {kTokEnableComments=1};

template <class Cont>
inline static void parseNameValues(const char* str, const char* pairDelims, char nvDelim,
                                   Cont& cont,  int flags=0)
{
    std::vector<std::string> pairs;
    tokenize(str, pairDelims, pairs);
    for (auto& pair: pairs)
    {
        size_t eq = pair.find(nvDelim);
        if ((eq == std::string::npos) || (eq == 0))
            throw std::runtime_error("parseNameValues: No name-value delimiter in line '"+pair+"'");
        size_t pstart;
        size_t plen = trim(pair, 0, pair.size()-1, pstart);
        if (!plen)
            continue; //empty line
        if (pstart >= eq) // only == should be possible
            throw std::runtime_error("parseNameValues: No value name in line '"+pair+"'");
        if ((flags & kTokEnableComments) && (pair[pstart] == '#'))
            continue;
        size_t nend = findFirstOf(pair, " \t", pstart+1, eq);
        if (nend == std::string::npos)
            nend = eq;
        size_t vstart = findFirstNotOf(pair, "\t ", eq+1, plen);
        cont.emplace(typename Cont::value_type(std::string(pair.c_str()+pstart, nend-pstart),
          (vstart!=std::string::npos)?std::string(pair.c_str()+vstart, pstart+plen-vstart):""));
    }
}

static inline std::string replaceOccurrences(const std::string& src, const std::string& from, const std::string& to)
{
    std::string result;
    size_t pos = 0;
    size_t oldEnd = 0;
    while((pos = src.find(from, pos))!=std::string::npos)
    {
        if (pos != oldEnd)
            result.append(&(src[oldEnd]), pos-oldEnd);
        result.append(to);
        oldEnd = pos+from.size();
        pos++;
    }
    if (oldEnd < src.size())
        result.append(&(src[oldEnd]), src.size()-oldEnd);
    return result;
}

static inline std::string xmlUnescape(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    result = replaceOccurrences(text, "&amp;", "&");
    result = replaceOccurrences(result, "&lt;", "<");
    result = replaceOccurrences(result, "&gt;", ">");
    result = replaceOccurrences(result, "&apos;", "'");
    return replaceOccurrences(result, "&quot;", "\"");
}

static inline std::string jsonUnescape(const std::string& text)
{
    return replaceOccurrences(text, "\"", "\\\"");
}

static inline std::string beforeFirst(const std::string& str, const char* sep)
{
    const char* pos = strstr(str.c_str(), sep);
    if (pos)
        return str.substr(0, pos-str.c_str());
    else
        return "";
}

static inline std::string afterFirst(const std::string& str, const char* sep)
{
   const char* pos = strstr(str.c_str(), sep);
   if (pos)
      return str.substr(pos-str.c_str()+1);
   else
      return std::string();
}

template <class A>
static inline size_t strArrIndexOf(const A& arr, const std::string& str)
{
    for (size_t i=0; i<arr.size(); i++)
        if (arr[i] == str)
            return i;
    return std::string::npos;
}

static inline bool startsWith(const std::string& str, const std::string& with)
{
    if (with.empty())
        throw std::runtime_error("startsWith: 'with'' is an empty string");
    if (str.empty() || (str.size() < with.size()))
        return false;

    return (strncmp(str.c_str(), with.c_str(), with.size()) == 0);
}
}
#endif // KARERE_STRING_UTILS_H
