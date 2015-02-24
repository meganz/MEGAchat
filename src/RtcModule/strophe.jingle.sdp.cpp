#include <algorithm>
#include <atomic>
#include <mstrophepp.h>
#include "strophe.jingle.sdp.h"
#include "karereCommon.h"
#include "StringUtils.h"

// SDP STUFF
namespace sdpUtil
{
using namespace std;
using namespace strophe;
using namespace karere; //for StringUtils

enum {LINEFIND_RET_WHOLE_LINES=1, LINEFIND_MUST_EXIST=2};
static bool hasLine(const LineGroup& str, const string& needle);
static bool hasLine(const LineGroup& str, const string& needle, const LineGroup& sessionpart);
static void rtcpFbToJingle(const LineGroup& sdp, strophe::Stanza elem, const std::string& payloadtype);

void ParsedSdp::parse(const string& strSdp)
{
    media.clear();
    session.clear();
    raw = strSdp;
    vector<string> lines;
    tokenize(raw.c_str(), "\r\n", lines);

    size_t i = 0;
    for (; i<lines.size(); i++)
    {
        string& line = lines[i];
        if (startsWith(line, "m="))
            break;
        session.push_back(line);
    }
    unique_ptr<LineGroup> curMedia;
    for (;i<lines.size(); i++)
    {
        string& line = lines[i];
        if (startsWith(line, "m="))
        {
            if (curMedia)
            {
                if (curMedia->empty())
                    throw runtime_error("Empty 'm=' line group in input sdp");
                media.push_back(std::move(*curMedia));
            }
            curMedia.reset(new LineGroup);
        }
        curMedia->push_back(std::move(line));
    }
    if (!curMedia->empty())
        media.push_back(std::move(*curMedia));

    if (media.empty())
        throw runtime_error("SDP parse: No m-lines found");
}

int ParsedSdp::getMlineIndex(const string& mid)
{
    string start = "m=";
    start.append(mid);
    for (size_t i=0; i<media.size(); i++)
        if (startsWith(media[i][0], start))
            return (int)i;
    return -1;
}

// add content's to a jingle element
Stanza ParsedSdp::toJingle(Stanza elem, const char* creator)
{
    unique_ptr<LineGroup> lines(find_lines(session, "a=group:"));
    // new bundle plan
    if (lines)
        for (auto& line: *lines)
        {
            assert(line.size() > 8);
            LineGroup parts;
            tokenize(line.c_str(), " ", parts);
            if (parts.size() < 2)
                throw runtime_error("Not enough parts of a=group line");
            string& semantics = parts[0];
            // new plan
            Stanza group = elem.c("group",
            {
                {"xmlns", "urn:xmpp:jingle:apps:grouping:0"},
                {"type", semantics}, //TODO: We should not need 'type', the XEP defines only 'semantics'
                {"semantics", semantics}
            });
            for (size_t j = 1; j < parts.size(); j++)
                group.c("content", {{"name", parts[j]}});
        }

    for (auto& m: media)
    {
        MLine mline(m[0]);
        if ((mline.media != "audio") && (mline.media != "video"))
            continue;
        string ssrc = find_line<>(m, "a=ssrc:");
        if (!ssrc.empty())
            ssrc = beforeFirst(ssrc, " "); // take the first

        Stanza content = elem.c("content", {{"creator", creator}, {"name", mline.media}});
        string mid = find_line<>(m, "a=mid:");
        if (!mid.empty())
        {
            // prefer identifier from a=mid if present
            content.setAttr("name", mid.c_str());
        }
        string rtpmap;
        if (hasLine(m, "a=rtpmap:"))
        {
            auto desc = content.c("description",
                 {{"xmlns", "urn:xmpp:jingle:apps:rtp:1"},
                  {"media", mline.media}
                 });
            if (!ssrc.empty())
                desc.setAttr("ssrc", ssrc.c_str());

            for (auto fmt: mline.fmt)
            {
                rtpmap = find_line<>(m, "a=rtpmap:" + fmt+" ");
                if (rtpmap.empty())
                {
                    KR_LOG_WARNING("No rtpmap line found for format %s\n", fmt.c_str());
                    continue;
                }
                auto payload = desc.c("payload-type", *parse_rtpmap(rtpmap, fmt));
                // put any 'a=fmtp:' + mline.fmt[j] lines into <param name=foo value=bar/>
                string strFmtp = find_line<>(m, "a=fmtp:" + fmt+" ");
                if (!strFmtp.empty())
                {
                    auto namevals = parse_fmtp(strFmtp);
                    for (auto& nv: *namevals)
                    {
                        if (!nv.first.empty())
                            payload.c("parameter", {{"name", nv.first}, {"value", nv.second}});
                          else
                            payload.c("parameter", {{"value", nv.second}});
                    }
                }
                rtcpFbToJingle(m, payload, fmt); // XEP-0293 -- map a=rtcp-fb
            }
            auto crypto = find_lines<>(m, "a=crypto:", session);
            if (!crypto->empty())
            {
                auto encr = desc.c("encryption", {{"required", "1"}});
                for (auto& line: *crypto)
                    encr.c("crypto", *parse_crypto(line));
            }

            if (!ssrc.empty())
            {
                auto ssrclines = find_lines<LINEFIND_MUST_EXIST>(m, "a=ssrc:");
                // new style mapping
                desc.c("source",
                {
                    {"ssrc", ssrc},
                    {"xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0"}
                });
                // FIXME: group by ssrc and support multiple different ssrcs
                for (auto& line: *ssrclines)
                {
                    size_t pos = line.find(' ');
                    if (pos == string::npos)
                        throw runtime_error("sdp: ssrc line does not have any space chars");
                    string linessrc = line.substr(0, pos);
                    if (linessrc != ssrc) //next ssrc
                    {
                        ssrc = linessrc;
                        desc.c("source",
                        {
                            {"ssrc", ssrc},
                            {"xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0"}
                        });
                    }

                    string kv = line.substr(pos + 1);
                    auto colon = kv.find(':');
                    if (colon == string::npos)
                        desc.c("parameter", {{"name", trim(kv)}});
                    else
                        desc.c("parameter",
                        {
                            {"name", trim(kv.substr(0, colon))},
                            {"value", trim(kv.substr(colon+1))}
                        });
                }
                // old proprietary mapping removed
            }

            if (hasLine(m, "a=rtcp-mux"))
                desc.c("rtcp-mux");

            // XEP-0293 -- map a=rtcp-fb:*
            rtcpFbToJingle(m, desc, "*");

            // XEP-0294
            auto extmaps = find_lines<>(m, "a=extmap:");
            if (!extmaps->empty())
              for (auto& em: *extmaps)
              {
                  auto extmap = parse_extmap(em);
                  desc.c("rtp-hdrext", *extmap);
                  // TODO: handle params
              }
        } // end of description

        auto transport = content.c("transport", {{"xmlns", "urn:xmpp:jingle:transports:ice-udp:1"}});
        // XEP-0320
        auto fingerprints = find_lines(m, "a=fingerprint:", session);
        for (auto& line: *fingerprints)
        {
            StringMap fpattrs;
            string fp = parse_fingerprint(line, fpattrs);
            // tmp.xmlns = 'urn:xmpp:jingle:apps:dtls:0'; -- FIXME: update receivers first
            string setup = find_line(m, "a=setup:", session);
            if (!setup.empty())
                fpattrs["setup"] = setup;
            transport.c("fingerprint", fpattrs).t(fp);
        }
        auto ice = iceparams(m, session);
        if (!ice->empty())
        {
            transport.setAttrs(*ice);
            // XEP-0176
            auto lines = find_lines<LINEFIND_RET_WHOLE_LINES>(m, "a=candidate:", session); // add any a=candidate lines
            if (!lines->empty())
                for (auto& line: *lines)
                {
                    auto cand = candidateToJingle(line.c_str()+2);
                    transport.c("candidate", *cand);
                }
        }

        if (mline.port == "0")
            // estos hack to reject an m-line
            content.setAttr("senders", "rejected");
          else if (hasLine(m, "a=sendrecv", session))
            content.setAttr("senders", "both");
          else if (hasLine(m, "a=sendonly", session))
            content.setAttr("senders", "initiator");
          else if (hasLine(m, "a=recvonly", session))
            content.setAttr("senders", "responder");
         else if (!hasLine(m, "a=inactive", session))
            content.setAttr("senders", "none");
    }
    return elem;
}

void rtcpFbToJingle(const LineGroup& sdp, Stanza elem, const string& payloadtype)
{ // XEP-0293
    auto lines = find_lines<>(sdp, "a=rtcp-fb:" + payloadtype+" ");
    for (auto& line: *lines)
    {
        vector<string> parts;
        tokenize(line.c_str(), " ", parts);
        if (parts.empty())
            throw runtime_error("rtcp-fb parse: line has no items");
        string& type = parts[0];
        if (type == "trr-int")
        {
            if (parts.size() < 2)
                throw runtime_error("rtcp-fb-trr-int parse: line has less than 2 items");
            elem.c("rtcp-fb-trr-int",
            {
                {"xmlns", "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"},
                {"value", parts[1]}
            });
        }
        else
        {
            StringMap attrs =
            {
                {"xmlns", "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"},
                {"type", type}
            };
            if (parts.size() >= 2)
                attrs["subtype"] = parts[1];
            elem.c("rtcp-fb", attrs);
        }
    }
}

void ParsedSdp::rtcpFbFromJingle(Stanza elem, const string& payloadtype, LineGroup& media)
{ // XEP-0293
    //elem is <payload-type> or <description> element
    //TODO: should be only one, abort loop when found
    elem.forEachChildByAttr("rtcp-fb-trr-int", "xmlns", "urn:xmpp:jingle:apps:rtp:rtcp-fb:0",
      [&media](Stanza child)
      {
         string ln = "a=rtcp-fb:* trr-int ";
         const char* value = child.attrOrNull("value");
         if (value)
            ln += value;
         else
            ln += '0';
         media.push_back(move(ln));
    });
    elem.forEachChildByAttr("rtcp-fb", "xmlns", "urn:xmpp:jingle:apps:rtp:rtcp-fb:0",
      [&media, &payloadtype](Stanza child)
    {
        string ln = "a=rtcp-fb:";
        ln.append(payloadtype).append(" ").append(child.attr("type"));
        const char* subtype = child.attrOrNull("subtype");
        if (subtype)
            ln.append(" ").append(subtype);
        media.push_back(move(ln));
    });
}

// construct an SDP from a jingle stanza
void ParsedSdp::parse(Stanza jingle)
{
    media.clear();
    session.push_back("v=0");
    session.push_back("o=- 1923518516 2 IN IP4 0.0.0.0");  // FIXME
    session.push_back("s=-");
    session.push_back("t=0 0");
// http://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-04#section-8

    jingle.forEachChildByAttr("group", "xmlns", "urn:xmpp:jingle:apps:grouping:0",
    [this](Stanza group)
    {
        string contents;
        group.forEachChild("content", [&contents](Stanza content)
        {
            contents.append(content.attr("name"))+=" ";
        });
        if (!contents.empty())
        {
            contents.resize(contents.size()-1); //remove last space
            const char* groupname = group.attrOrNull("semantics");
            if (!groupname)
                groupname = group.attr("type");
            session.push_back(string("a=group:")+groupname+" "+
               contents);
        }
    });

    jingle.forEachChild("content", [this](Stanza content)
    {
        media.push_back(std::move(*jingle2media(content)));
    });

    // reconstruct msid-semantic -- apparently not necessary
    /*
    var msid = SDPUtil.parse_ssrc(this.raw);
    if (msid.hasOwnProperty('mslabel')) {
        this.session += "a=msid-semantic: WMS " + msid.mslabel + "\r\n";
    }
    */
    for (auto& line: session)
        raw.append(line).append("\r\n");

    for (auto& m: media)
        for (auto& line: m)
            raw.append(line).append("\r\n");
}

MLine::MLine(Stanza content)
{
    Stanza desc = content.child("description");
    media = desc.attr("media");
    port = "1";
    if (strcmp(content.attr("senders"), "rejected") == 0)
        // estos hack to reject an m-line.
        port = "0";

    Stanza transport = content.child("transport");
    if (desc.child("encryption", true) ||
       ((transport && transport.child("fingerprint"))))
        proto = "RTP/SAVPF";
    else
        proto = "RTP/AVPF";

    desc.forEachChild("payload-type", [this](Stanza child)
    {
       fmt.push_back(child.attr("id"));
    });
}

// translate a jingle content element into an an SDP media part
unique_ptr<LineGroup> ParsedSdp::jingle2media(Stanza content)
{
    Stanza desc = content.child("description");
    MLine mline(content);
    unique_ptr<LineGroup> retMedia(new LineGroup);
    LineGroup& media = *retMedia;
    media.push_back(mline.toSdp());
    media.push_back("c=IN IP4 0.0.0.0");
    media.push_back("a=rtcp:1 IN IP4 0.0.0.0");

    auto transport = content.childByAttr("transport", "xmlns", "urn:xmpp:jingle:transports:ice-udp:1", true);
    if (transport)
    {
        const char* ufrag = transport.attrOrNull("ufrag");
        if (ufrag)
            media.push_back("a=ice-ufrag:"+string(ufrag));

        const char* pwd = transport.attrOrNull("pwd");
        if (pwd)
            media.push_back("a=ice-pwd:"+string(pwd));
    }
    transport.forEachChild("fingerprint", [&media](Stanza child)
    {
      // FIXME: check namespace at some point
        media.push_back("a=fingerprint:"+string(child.attr("hash"))+" "+
             child.text().c_str());
        const char* setup = child.attrOrNull("setup");
        if (setup)
             media.push_back(string("a=setup:")+setup);
    });
    string senders = content.attr("senders");
    if (senders == "initiator")
        media.push_back("a=sendonly");
     else if (senders == "responder")
        media.push_back("a=recvonly");
     else if (senders == "none")
        media.push_back("a=inactive");
     else if (senders == "both")
        media.push_back("a=sendrecv");

     media.push_back(string("a=mid:")+content.attr("name"));

    // <description><rtcp-mux/></description>
    // see http://code.google.com/p/libjingle/issues/detail?id=309 -- no spec though
    // and http://mail.jabber.org/pipermail/jingle/2011-December/001761.html
    if (desc.child("rtcp-mux", true))
        media.push_back("a=rtcp-mux");

    Stanza enc = desc.child("encryption", true);
    if (enc)
    {
       enc.forEachChild("crypto", [&media](Stanza crypto)
       {
           string sdpCrypto = "a=crypto:";
           sdpCrypto.append(crypto.attr("tag"))
                .append(" ").append(crypto.attr("crypto-suite"))
                .append(" ").append(crypto.attr("key-params"));
           const char* sesp = crypto.attr("session-params");
           if (sesp)
               sdpCrypto.append(" ").append(sesp);
           media.push_back(std::move(sdpCrypto));
       });
    }
    desc.forEachChild("payload-type", [&media, this](Stanza payload)
    {
        media.push_back(build_rtpmap(payload));
        if (payload.child("parameter", true))
        {
            string fmtp = "a=fmtp:";
            fmtp.append(payload.attr("id")).append(" ");
            bool hasParam = false;
            payload.forEachChild("parameter", [&fmtp, &hasParam, this](Stanza param)
            {
               hasParam = true;
               const char* name = param.attrOrNull("name");
               if (name)
                  fmtp.append(name)+= '=';
               fmtp.append(param.attr("value"))+=';';
            });
            if (hasParam)
                fmtp.resize(fmtp.size()-1);
            media.push_back(std::move(fmtp));
        }
        // xep-0293
        rtcpFbFromJingle(payload, payload.attr("id"), media);
    });

    // xep-0293
    rtcpFbFromJingle(desc, "*", media);

    // xep-0294
    desc.forEachChildByAttr("rtp-hdrext", "xmlns",
      "urn:xmpp:jingle:apps:rtp:rtp-hdrext:0", [&media](Stanza hdrext)
    {
        media.push_back(string("a=extmap:")+
        hdrext.attr("id")+" "+hdrext.attr("uri"));
    });

    content.forEachChildByAttr("transport", "xmlns", "urn:xmpp:jingle:transports:ice-udp:1", [&media](Stanza transport)
    {
      transport.forEachChild("candidate", [&media](Stanza cand)
      {
        media.push_back(candidateFromJingle(cand, true));
      });
    });

    desc.forEachChildByAttr("source", "xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0",
    [&media](Stanza source)
    {
        const char* ssrc = source.attr("ssrc");
        source.forEachChild("parameter", [&media, ssrc](Stanza par)
        {
            string ln = "a=ssrc:";
            ln.append(ssrc).append(" ").append(par.attr("name"));
            const char* val = par.attrOrNull("value");
            if (val)
               ln.append(":").append(val);
            media.push_back(std::move(ln));
        });
    });

    return retMedia;
}

unique_ptr<StringMap> iceparams(const LineGroup& mediadesc, const LineGroup& sessiondesc)
{
    unique_ptr<StringMap> data(new StringMap);
    string ufrag = find_line(mediadesc, "a=ice-ufrag:", sessiondesc);
    string pwd = find_line(mediadesc, "a=ice-pwd:", sessiondesc);
    if (!ufrag.empty() && !pwd.empty())
    {
            (*data)["ufrag"] = ufrag;
            (*data)["pwd"] = pwd;
    }
    return data;
}

MLine::MLine(const string& mline)
{
//should be a debug assertion as it is is giaranteed by the code, but just in case
    if ((mline.size() < 2) || (strncmp(mline.c_str(), "m=", 2) != 0))
        throw runtime_error("MLine::MLine: string is not an mline");

    tokenize(mline.c_str()+2, " ", fmt);
    media = fmt[0];
    port = fmt[1];
    proto = fmt[2];
    fmt.erase(fmt.begin(), fmt.begin()+3);
}

string MLine::toSdp()
{
    string ret;
    ret.append("m=").append(media).append(" ").append(port)
       .append(" ").append(proto).append(" ");
    for (auto& f: fmt)
        ret.append(f)+=" ";
    if (!fmt.empty())
        fmt.resize(ret.size()-1);
    return ret;
}

unique_ptr<StringMap> parse_rtpmap(const string& line, const string& id)
{
    vector<string> parts;
    unique_ptr<StringMap> ret(new StringMap);
    StringMap& data = *ret;
    data["id"] = id;
    tokenize(line.c_str(), "/", parts);
    data["name"] = parts[0];
    data["clockrate"] = parts[1];
    data["channels"] = (parts.size() >= 3) ? parts[2] : "1";
    return ret;
}
string build_rtpmap(Stanza el)
{
    string line = "a=rtpmap:";
    line.append(el.attr("id")).append(" ").append(el.attr("name"))
        .append("/").append(el.attr("clockrate"));
    const char* channels = el.attrOrNull("channels");
    if (channels && strcmp(channels, "1"))
        line.append("/").append(channels);
    return line;
}

unique_ptr<StringMap> parse_crypto(const string& line)
{
   vector<string> parts;
   tokenize(line.c_str(), " ", parts);
   unique_ptr<StringMap> ret(new StringMap);
   StringMap& data = *ret;

   data["tag"] = parts[0];
   data["crypto-suite"] = parts[1];
   data["key-params"] = parts[2];
   if (parts.size() >= 4)
   {
      string params;
      for (size_t i=3; i<parts.size(); i++)
          params.append(parts[i])+=" ";
      params.resize(params.size()-1);
      data["session-params"] = params;
   }
   return ret;
}
string parse_fingerprint(const string& line, StringMap& attrs)
{ // RFC 4572
   vector<string> parts;
   tokenize(line.c_str(), " ", parts);
   attrs.clear();
   attrs["hash"] = parts[0];
   attrs["xmlns"] = "urn:xmpp:tmp:jingle:apps:dtls:0";
//   data.fingerprint = parts[1];
// TODO assert that fingerprint satisfies 2UHEX *(":" 2UHEX) ?
   return parts[1];
}
unique_ptr<vector<pair<string, string> > > parse_fmtp(const string& line)
{
   vector<string> parts;
   unique_ptr<vector<pair<string, string> > > ret(new vector<pair<string, string> >);
   tokenize(line.c_str(), ";", parts);
   for (auto& part: parts)
   {
        string key, value;
        auto eqpos = part.find('=');
        if (eqpos == string::npos)
            key = trim(part);
          else
          {
            key = trim(part.substr(0, eqpos));
            value = trim(part.substr(eqpos+1));
          }
        if (key.empty())
        {
            KR_LOG_WARNING("parse_fmtp: Empty value name in parameter pairs parsing");
            continue;
        }
        if (!value.empty())
            ret->emplace_back(key, value);
        else
            // rfc 4733 (DTMF) style stuff
            ret->emplace_back("", key);
   }
   return ret;
}

unique_ptr<StringMap> parse_extmap(const string& line)
{
   vector<string> parts;
   tokenize(line.c_str(), " ", parts);
   unique_ptr<StringMap> ret(new StringMap);
   StringMap& data = *ret;
   string& value = data["id"] = parts[0];
   size_t slashpos = value.find('/');
   if (slashpos != string::npos)
   {
       data["direction"] = value.substr(slashpos + 1);
       value = value.substr(0, slashpos);
   }
   else
       data["direction"] = "both";

   data["uri"] = parts[1];
   data["xmlns"] = "urn:xmpp:jingle:apps:rtp:rtp-hdrext:0";

//TODO: handle params
//data["params"] = parts;
   return ret;
}
string tillEol(const string text, size_t& pos)
{
    size_t start = pos;
    for (; pos<text.size(); pos++)
    {
       char ch = text[pos];
       if ((ch == '\r') || (ch == '\n'))
            return text.substr(start, pos-start);
    }
    pos = string::npos;
    return text.substr(start);
}
template <int flags>
string find_line(const LineGroup& haystack, const string& needle, size_t& i)
{
    for (; i<haystack.size(); i++)
    {
        const string& line = haystack[i];
        if (line.size() < needle.size())
            continue;
        if (strncmp(line.c_str(), needle.c_str(), needle.size()) == 0)
        {
            i++;
            return (flags & LINEFIND_RET_WHOLE_LINES) ? line : (line.c_str()+needle.size()); //if line.size() == needle.size(), we will be at the terminating NULL, which is valid
        }
    }
    if (flags & LINEFIND_MUST_EXIST)
        throw runtime_error("No line found starting with "+needle);
    i = string::npos;
    return "";
}

template <int flags>
string find_line(const LineGroup& haystack, const string& needle)
{
     size_t i = 0;
     return find_line<flags>(haystack, needle, i);
}
template <int flags>
string find_line(const LineGroup& haystack, const string& needle, const LineGroup& sessionpart)
{
    string ret = find_line<flags&(~LINEFIND_MUST_EXIST)>(haystack, needle);
    if (!ret.empty())
        return ret;
    return find_line<flags>(sessionpart, needle);
}

bool hasLine(const LineGroup& lines, const string& needle)
{
     for (auto& line: lines)
     {
         if (line.size() < needle.size())
             continue;
         if (strncmp(line.c_str(), needle.c_str(), needle.length()) == 0)
             return true;
     }
     return false;
}

bool hasLine(const LineGroup& lines, const string& needle, const LineGroup& sessionpart)
{
    if (hasLine(lines, needle))
         return true;
    return hasLine(sessionpart, needle);
}
/** Returns a unique_ptr to a LineGroup containing all lines starting with \c needle.
 * If none are found, a NULL unique_ptr is returned
 */
template <int flags>
unique_ptr<LineGroup> find_lines(const LineGroup& haystack, const string& needle)
{
   size_t start = 0;
   unique_ptr<LineGroup> lines(new LineGroup);
   for (;;)
   {
       string line;
       line = find_line<flags&(~LINEFIND_MUST_EXIST)>(haystack, needle, start);
       if (start == string::npos)
           break;
       else if (!line.empty())
           lines->push_back(std::move(line));
   }
   if (lines->empty() && (flags & LINEFIND_MUST_EXIST))
       throw runtime_error("No line found starting with "+needle);
   return lines;
}

template <int flags>
unique_ptr<LineGroup> find_lines(const LineGroup& haystack, const string& needle, const LineGroup& sessionpart)
{
    auto lines = find_lines<flags&(~LINEFIND_MUST_EXIST)>(haystack, needle);
    if (!lines->empty())
        return lines;
// search session part
    return find_lines<flags>(sessionpart, needle);
}

unique_ptr<StringMap> candidateToJingle(const string &line)
{
// a=candidate:2979166662 1 udp 2113937151 192.168.2.100 57698 typ host generation 0
//      <candidate component=... foundation=... generation=... id=... ip=... network=... port=... priority=... protocol=... type=.../>
    unique_ptr<StringMap> ret(new StringMap);
    StringMap& candidate = *ret;
    vector<string> elems;
    //this func can be called from 2 different places - from the sdp parsing code, or
    //from the onIceCandidate event handler. The handler received a candidate string
    //starting with 'candidate:', and the sdp has the candidate line as 'a=candidate', but
    //removes the 'a=' part. So here the string starts with 'candidate=', and we have to
    //remove it
    //remove leading 'candidate:'
    tokenize(line.c_str()+10, " ", elems);
    if (elems[6] != "typ")
        throw runtime_error("candidateToJingle: did not find typ in the right place");
    candidate["foundation"] = elems[0];
    candidate["component"] = elems[1];
    string& proto = candidate["protocol"] = elems[2];
    transform(proto.begin(), proto.end(), proto.begin(), ::tolower);
    candidate["priority"] = elems[3];
    candidate["ip"] = elems[4];
    candidate["port"] = elems[5];
        // elems[6] => "typ"
    candidate["type"] = elems[7];
    for (size_t i = 8; i < elems.size(); i += 2)
    {
        string& elem = elems[i];
        if (elem == "raddr")
            candidate["rel-addr"] = elems[i + 1];
         else if (elem == "rport")
            candidate["rel-port"] = elems[i + 1];
         else if (elem == "generation")
            candidate["generation"] = elems[i + 1];
         else
            // TODO
            KR_LOG_WARNING("candidateToJingle: not translating '%s' = '%s'", elems[i].c_str(), elems[i + 1].c_str());
    }
    candidate["network"] = "1";
    static atomic<unsigned> id(0);
    candidate["id"] = to_string(++id); // not applicable to SDP -- FIXME: should be unique, not just random
    return ret;
}
string candidateFromJingle(Stanza cand, bool isInSdp)
{
    string line = isInSdp? "a=candidate:" : "candidate:";
    line.append(cand.attr("foundation")).append(" ")
        .append(cand.attr("component")).append(" ")
        .append(cand.attr("protocol")) //.toUpperCase(); // chrome M23 doesn't like this
        .append(" ").append(cand.attr("priority")).append(" ")
        .append(cand.attr("ip")).append(" ")
        .append(cand.attr("port")).append(" ")
        .append("typ ").append(cand.attr("type")).append(" ");
    string type = cand.attr("type");
    if ((type == "srflx") || (type == "prflx") || (type == "relay"))
    {
        auto reladdr = cand.attrOrNull("rel-addr");
        auto relport = cand.attrOrNull("rel-port");
        if (reladdr && relport)
        {
             line.append("raddr ").append(reladdr).append(" ")
                 .append("rport ").append(relport).append(" ");
        }
    }
    line+= "generation ";
    auto gen = cand.attrOrNull("generation");
    line+= gen?gen:"0";
    return line;
}
}
