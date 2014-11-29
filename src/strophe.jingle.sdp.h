#pragma once
// SDP parser
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace strophe
{
    class Stanza;
}

namespace sdpUtil
{
typedef std::map<std::string, std::string> StringMap;
typedef std::vector<std::string> LineGroup;

struct MLine
{
    std::string media;
    std::string port;
    std::string proto;
    std::vector<std::string> fmt;
    MLine(const std::string& line);
    MLine(strophe::Stanza content);
    std::string toSdp();
};

class ParsedSdp
{
public:
    /** Each media element is a group of lines related to one 'm=' block in the SDP */
    std::vector<LineGroup> media;
    /** This is the group of lines that is before the first 'm=' group of lines */
    LineGroup session;
    std::string raw;
//construct from SDP
    void parse(const std::string& strSdp);
// construct from a jingle stanza
    void parse(strophe::Stanza jingle);
//checks if there is an 'm=<name>:' line
    //bool hasMlineWithName(const char* name);
    int getMlineIndex(const std::string& mid);
// add contents to a jingle element
    strophe::Stanza toJingle(strophe::Stanza elem, const char* creator);
protected:
    void rtcpFbFromJingle(strophe::Stanza elem, const std::string& payloadtype, LineGroup& media);
// translate a jingle content element into an an SDP media part
    std::unique_ptr<LineGroup> jingle2media(strophe::Stanza content);
};

std::unique_ptr<StringMap> parse_rtpmap(const std::string& line, const std::string& id);
std::string build_rtpmap(strophe::Stanza el);
std::unique_ptr<StringMap> parse_crypto(const std::string& line);
std::unique_ptr<std::vector<std::pair<std::string, std::string> > >
    parse_fmtp(const std::string& line);
std::unique_ptr<StringMap> parse_extmap(const std::string& line);
template <int flags = 0> std::string
find_line(const LineGroup& haystack, const std::string& needle, size_t& start);
template <int flags = 0> std::string
find_line(const LineGroup& haystack, const std::string& needle);
template <int flags = 0> std::string
find_line(const LineGroup& haystack, const std::string& needle, const LineGroup& session);
template <int flags = 0> std::unique_ptr<LineGroup>
find_lines(const LineGroup& haystack, const std::string& needle);
template <int flags = 0> std::unique_ptr<LineGroup>
find_lines(const LineGroup& haystack, const std::string& needle, const LineGroup& sessionpart);
std::unique_ptr<StringMap> iceparams(const LineGroup& mediadesc, const LineGroup& sessiondesc);
std::unique_ptr<StringMap> candidateToJingle(const std::string& line);
std::string candidateFromJingle(strophe::Stanza cand);
std::string parse_fingerprint(const std::string& line, StringMap& attrs);
}
