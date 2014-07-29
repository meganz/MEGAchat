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
    std::vector<std::string> media;
    std::string session;
    std::string raw;
//construct from SDP
    void parse(const std::string& strSdp);
// construct from a jingle stanza
    void parse(strophe::Stanza jingle);
// add contents to a jingle element
    strophe::Stanza toJingle(strophe::Stanza elem, const char* creator);
protected:
    void rtcpFbToJingle(const std::string& sdp, strophe::Stanza elem, const std::string& payloadtype);
    std::string rtcpFbFromJingle(strophe::Stanza elem, const std::string& payloadtype);
// translate a jingle content element into an an SDP media part
    std::string jingle2media(strophe::Stanza content);
};

std::unique_ptr<StringMap> parse_rtpmap(const std::string& line);
std::string build_rtpmap(strophe::Stanza el);
std::unique_ptr<StringMap> parse_crypto(const std::string& line);
std::unique_ptr<std::vector<std::pair<std::string, std::string> > >parse_fmtp(const std::string& line);
std::unique_ptr<StringMap> parse_extmap(const std::string& line);
std::string tillEol(const std::string text, size_t& pos);
std::string find_line(const std::string& haystack, const std::string& needle, size_t& start);
std::string find_line(const std::string& haystack, const std::string& needle);
std::string find_line(const std::string& haystack, const std::string& needle, const std::string& session);
std::unique_ptr<std::vector<std::string> > find_lines(const std::string haystack, const std::string needle);
std::unique_ptr<std::vector<std::string> > find_lines(const std::string haystack, const std::string needle, const std::string& sessionpart);
std::unique_ptr<StringMap> iceparams(const std::string& mediadesc, const std::string& sessiondesc);
std::unique_ptr<StringMap> candidateToJingle(const std::string& line);
std::string candidateFromJingle(strophe::Stanza cand);
std::string parse_fingerprint(const std::string& line, StringMap& attrs);

}
