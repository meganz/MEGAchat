// SDP STUFF
namespace sdpUtil
{
enum {SPLITF_NO_FIRST = 1};
static void split(const string& str, const string& sep, vector<string>& ret, unsigned flags = 0, unsigned max=0xffffffff);
static shared_ptr<vector<string> > split(const string& str, const char* sep, unsigned flags = 0, unsigned max=0xffffffff);
static string beforeFirst(const string& str, const char* sep);
static string afterFirst(const string str, const char* sep);
static size_t strArrIndexOf(const vector<string>& arr, const string& str);
struct MLine
{
    string media;
    string port;
    string proto;
    vector<string> fmt;
    MLine(const string& line);
    string toSdp();
};

ParsedSdp::ParsedSdp(const string& strSdp)
{
    raw = strSdp;
    for (size_t i=raw.size()-1; i>0; i--)
    {
        char ch = raw[i];
        if ((ch == '\r') || (ch == '\n'))
            raw.resize(raw.size()-1);
        else
            break;
    }

    split(strSdp, "\r\nm=", media);
    if (media.size() < 2)
        throw runtime_error("SDP parse: No m-lines found");

    session = (media.front() + "\r\n");
    media.erase(media.begin());
    for (size_t i = 0; i < media.size(); i++)
        media[i] = ("m=" + media[i] + "\r\n");
}

// add content's to a jingle element
Stanza ParsedSdp::toJingle(Stanza elem, const char* creator)
{
    unique_ptr<vector<string> > lines;
    // new bundle plan
    if (!(lines = find_lines(session, "a=group:"))->empty())
        for (size_t i = 0; i < lines->size(); i++)
        {
            auto parts = split(lines[i], " ");
            if (parts->size() < 2)
                throw runtime_error("Not enough parts of a=group line");
            string semantics = parts->front().substr(8);
            // new plan
            Stanza group = elem.c("group",
            {
                {"xmlns", "urn:xmpp:jingle:apps:grouping:0"},
                {"type", semantics},
                {"semantics", semantics}
            });
            for (size_t j = 1; j < parts->size(); j++)
                group.c("content", {{"name", (*parts)[j]}});

            // temporary plan, to be removed
            group = elem.c("group", {
                {"xmlns", "urn:ietf:rfc:5888"},
                {"type", semantics}
            });
            for (size_t j = 1; j < parts->size(); j++)
                group.c("content", {{"name", (*parts)[j]}});

        }
    // old bundle plan, to be removed
    vector<string> bundle;
    string strBundle = find_line(session, "a=group:BUNDLE ");
    if (!strBundle.empty())
        split(strBundle, " ", bundle, SPLITF_NO_FIRST);

    for (m: media)
    {
        MLine mline(m);
        if ((mline.media != "audio") && (mline.media != "video"))
            continue;
        string ssrc;
        if (!(ssrc = find_line(m, "a=ssrc:")).empty())
            ssrc = ssrc.substring(7).beforeFirst(" "); // take the first

        Stanza content = elem.c("content", {{"creator", creator}, {"name", mline.media}});
        string mid;
        if (!(mid = find_line(m, "a=mid:")).empty())
        {
            // prefer identifier from a=mid if present
            mid = mid.substr(6);
            content.setAttr("name", mid.c_str());
            size_t idx;
            // old BUNDLE plan, to be removed
            if ((idx = strArrIndexOf(bundle, mid)) != string::npos)
            {
                content.c("bundle", {{"xmlns": "http://estos.de/ns/bundle"}});
                bundle.erase(bundle.begin()+idx);
            }
        }
        string rtpmap;
        if (hasLine(m, "a=rtpmap:"))
        {
            auto desc = content.c("description",
                 {{"xmlns", "urn:xmpp:jingle:apps:rtp:1"},
                  {"media", mline.media}
                 });
            if (ssrc)
                desc.setAttr("ssrc", ssrc.c_str());

            for (auto fmt: mline.fmt)
            {
                rtpmap = find_line(m, "a=rtpmap:" + fmt);
                auto payload = desc.c("payload-type", *parse_rtpmap(rtpmap));
                // put any 'a=fmtp:' + mline.fmt[j] lines into <param name=foo value=bar/>
                string strFmtp;
                if (!(strFmtp = find_line(m, "a=fmtp:" + fmt)).empty())
                {
                    auto namevals = parse_fmtp(strFmtp);
                    for (nv: *namevals)
                    {
                        if (!nv.first.empty())
                            payload.c("parameter", {{"name", nv.first}, {"value", nv.second}});
                          else
                            payload.c("parameter", {{"value", nv.second}});
                    }
                }
                rtcpFbToJingle(m, payload, fmt); // XEP-0293 -- map a=rtcp-fb
            }
            auto crypto = find_lines(m, "a=crypto:", session);
            if (!crypto->empty())
            {
                auto encr = desc.c("encryption", {{"required", "1"}});
                for (auto line: *crypto)
                    encr.c("crypto", *parse_crypto(line));
            }

            if (!ssrc.empty())
            {
                auto ssrclines = find_lines(m, "a=ssrc:");
                // new style mapping
                desc.c("source", {{"ssrc", ssrc}, {"xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0"}});
                // FIXME: group by ssrc and support multiple different ssrcs
                for (auto line: *ssrclines)
                {
                    auto idx = line.find(' ');
                    string linessrc = line.substr(7, idx-7);
                    if (linessrc != ssrc)
                    {
                        ssrc = linessrc;
                        desc.c("source", {{"ssrc", ssrc}, {"xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0"}});
                    }

                    string kv = line.substr(idx + 1);
                    auto colon = kv.find(':');
                    if (colon == string::npos)
                        desc.c("parameter", {{"name", kv}});
                    else
                        desc.c("parameter", {
                            {"name", kv.substr(0, colon)},
                            {"value", kv.substr(colon+1)}
                        });
                }
                // old proprietary mapping removed
            }

            if (hasLine(m, "a=rtcp-mux"))
                desc.c("rtcp-mux", {});

            // XEP-0293 -- map a=rtcp-fb:*
            rtcpFbToJingle(m, desc, "*");

            // XEP-0294
            auto extmaps = find_lines(m, "a=extmap:");
            for (em: *extmaps)
            {
                auto extmap = parse_extmap(em);
                desc.c("rtp-hdrext", *extmap);
                // TODO: handle params
            }
        } // end of description

        auto transport = content.c("transport", {{"xmlns": "urn:xmpp:jingle:transports:ice-udp:1"});
        // XEP-0320
        auto fingerprints = find_lines(m, "a=fingerprint:", session);
        for (line: *fingerprints)
        {
            StringMap fpattrs;
            string fp = parse_fingerprint(line, fpattrs);
            // tmp.xmlns = 'urn:xmpp:jingle:apps:dtls:0'; -- FIXME: update receivers first
            string setup = find_line(m, "a=setup:", session);
            if (!setup.empty())
                fpattrs["setup"] = setup.substr(8);
            transport.c("fingerprint", fpattrs).t(fp);
        });
        auto ice = iceparams(m, session);
        if (!ice->empty())
        {
            transport.setAttrs(*ice);
            // XEP-0176
            auto lines = find_lines(m, "a=candidate:", this.session); // add any a=candidate lines
            if (!lines->empty())
                for (line: *lines)
                {
                    auto cand = candidateToJingle(line);
                    transport.c("candidate", *cand);
                }
        }

        if (mline.port == "0")
            // estos hack to reject an m-line
            elem.setAttr("senders", "rejected");
        else if (hasLine(m, "a=sendrecv", session))
            elem.setAttr("senders", "both");
          else if (hasLine(m, "a=sendonly", session))
            elem.setAttr("senders", "initiator");
          else if (hasLine(m, "a=recvonly", session))
            elem.setAttr("senders", "responder");
         else if (!hasLine(m, "a=inactive", session))
            elem.setAttr("senders", "none");
    }
    return elem;
};

void rtcpFbToJingle(const string& sdp, Stanza elem, const string& payloadtype)
{ // XEP-0293
    auto lines = find_lines(sdp, "a=rtcp-fb:" + payloadtype);
    for (auto line: *lines)
    {
        vector<string> parts;
        split(line, " ", parts);
        if (parts.size() < 2)
            throw runtime_error("rtcp-fb parse: line has less than 2 parts");
        string& type = parts[1];
        if (type == "trr-int")
        {
            if (parts.size() < 3)
                throw runtime_error("rtcp-fb-trr-int parse: line has less than 3 parts");
            elem.c("rtcp-fb-trr-int", {{"xmlns", "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"}, {"value", parts[2]}});
        }
        else
        {
            StringMap attrs = {{"xmlns": "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"}, {"type", type}};
            if (parts.size() >= 3)
                attrs["subtype"] = parts[2];
            elem.c("rtcp-fb", attrs);
        }
    }
};

string ParsedSdp::rtcpFbFromJingle(Stanza elem, const string& payloadtype)
{ // XEP-0293
    //elem is <payload-type> or <description> element
    string media;
    //TODO: should be only one, abort loop when found
    elem.forEachChild("rtcp-fb-trr-int", [media&](Stanza child) mutable
    {
         const char* ns = child.attr("xmlns", true);
         if (!ns || strcmp(ns, "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"))
            return;
         media += "a=rtcp-fb:* trr-int ";
         const char* value = child.attr("value", true);
         if (value)
            media += value;
         else
            media += '0';
         media += "\r\n";
    }
    elem.forEachChild("rtcp-fb", [media&](Stanza child)
    {
        const char& ns = child.attr("xmlns", true);
        if (!ns || strcmp(ns, "urn:xmpp:jingle:apps:rtp:rtcp-fb:0"))
            return;
        media.append("a=rtcp-fb:").append(payloadtype).append(" ")
             .append(child.attr("type"));
        const char* subtype = child.attr("subtype", true);
        if (subtype)
            media.append(" ").append(subtype);
        media.append("\r\n");
    });
    return media;
};

// construct an SDP from a jingle stanza
ParsedSdp::ParsedSdp(Stanza jingle)
{
    raw =
        "v=0\r\n"
        "o=- 1923518516 2 IN IP4 0.0.0.0\r\n"  // FIXME
        "s=-\r\n"
        "t=0 0\r\n";
// http://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-04#section-8


    jingle.forEachChildByAttr("group", "xmlns", "urn:xmpp:jingle:apps:grouping:0", [](Stanza group)
    {
        string contents;
        group.forEachChild("content", [contents&](Stanza content)
        {
            contents.append(content.attr("name"))+=" ";
        });
        if (!contents.empty())
        {
            contents.resize(contents.size()-1); //remove last space
            const char* groupname = group.attr("semantics", true);
            if (!groupname)
                groupname = group.attr("type");
            raw.append("a=group:").append(groupname).append(" ")
               .append(contents).append("\r\n");
        }
    });

    session = raw;
    jingle.forEachChild("content", [this](Stanza content)
    {
        media.push_back(jingle2media(content));
    });

    // reconstruct msid-semantic -- apparently not necessary
    /*
    var msid = SDPUtil.parse_ssrc(this.raw);
    if (msid.hasOwnProperty('mslabel')) {
        this.session += "a=msid-semantic: WMS " + msid.mslabel + "\r\n";
    }
    */
    for (m: media)
        raw.append(m);
};

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
       ((transport && transport.child("fingerprint")))
        proto = "RTP/SAVPF";
    else
        proto = "RTP/AVPF";

    desc.forEachChild("payload-type", [fmt&](Stanza child)
    {
       fmt.push_back(child.attr("id"));
    });
}

// translate a jingle content element into an an SDP media part
string ParsedSdp::jingle2media(Stanza content)
{
    Stanza desc = content.child("description");
    const char* ssrc = desc.attr("ssrc");
    MLine mline(content);
    string media = mline.toSdp();
    media.append("\r\n")
         .append("c=IN IP4 0.0.0.0\r\n")
         .append("a=rtcp:1 IN IP4 0.0.0.0\r\n");

    transport = content.childByAttr("transport", "xmlns", "urn:xmpp:jingle:transports:ice-udp:1", true);
    if (transport)
    {
        const char* ufrag = transport.attr("ufrag", true);
        if (ufrag)
            media.append("a=ice-ufrag:").append(frag).append("\r\n");

        const char* pwd = transport.attr("pwd", true);
        if (pwd)
            media.append("a=ice-pwd:").append(pwd).append("\r\n");
    }
    transport.forEachChild("fingerprint", [media&](Stanza child) mutable
    {
      // FIXME: check namespace at some point
        media.append("a=fingerprint:").append(child.attr("hash"))
             .append(child.text()).append("\r\n");
        const char* setup = child.attr("setup", true);
        if (setup)
             media.append("a=setup:").append(setup).append("\r\n");
    }
    string senders = content.attr("senders");
    if (senders == "initiator")
        media += "a=sendonly\r\n";
     else if (senders == "responder")
        media += "a=recvonly\r\n";
     else if (senders == "none")
        media += "a=inactive\r\n";
     else if (senders == "both")
        media += "a=sendrecv\r\n";

     media.append("a=mid:").append(content.attr("name")).append("\r\n");

    // <description><rtcp-mux/></description>
    // see http://code.google.com/p/libjingle/issues/detail?id=309 -- no spec though
    // and http://mail.jabber.org/pipermail/jingle/2011-December/001761.html
    if (desc.child("rtcp-mux", true))
        media += "a=rtcp-mux\r\n";

    Stanza enc = desc.child("encryption");
    if (enc)
    {
       enc.forEachChild("crypto", [media&](Stanza crypto) mutable
       {
           media.append("a=crypto:").append(crypto.attr("tag"))
                .append(" ").append(crypto.attr("crypto-suite"))
                .append(" ").append(crypto.attr("key-params"));
           const char* sesp = crypto.attr("session-params");
           if (sesp)
               media.append(" ").append(sesp);
           media += "\r\n";
       });
    }
    desc.forEachChild("payload-type", [media&](Stanza payload)
    {
        media.append(build_rtpmap(payload)).append("\r\n");
        if (child.child("parameter"))
        {
            media.append("a=fmtp:").append(payload.attr("id")).append(" ");
            bool hasParam = false;
            payload.forEachChild("parameter", [media&, hasParam&](Stanza param) mutable
            {
               hasParam = true;
               const char* name = param.attr("name", true);
               if (name)
                  media.append(name)+= '=';
               media.append(param.attr('value'))+=';';
            });
            if (hasParam)
                media.resize(media.size()-1);
            media += "\r\n";
        }
        // xep-0293
        media += rtcpFbFromJingle(payload, payload.attr("id"));
    });

    // xep-0293
    media += rtcpFbFromJingle(desc, "*");

    // xep-0294
    desc.forEachChildByAttr("rtp-hdrext", "xmlns",
      "urn:xmpp:jingle:apps:rtp:rtp-hdrext:0", [media&](Stanza hdrext) mutable
    {
        media.append("a=extmap:").append(hdrext.attr("id"))
             .append(" ").append(hdrext.attr("uri")).append("\r\n");
    });

    content.forEachChildByAttr("transport", "xmlns", "urn:xmpp:jingle:transports:ice-udp:1", [media&](Stanza transport) mutable
    {
      transport.forEachChild("candidate", [media&](Stanza cand) mutable
      {
        media += candidateFromJingle(cand);
      });
    });
    bool has
    desc.forEachChildByAttr("source", "xmlns", "urn:xmpp:jingle:apps:rtp:ssma:0",
    [media&](Stanza source) mutable
    {
        const char* ssrc = source.attr("ssrc");
        source.forEachChild("parameter", [media&]{Stanza par)
        {
            media.append("a=ssrc:").append(ssrc).append(" ").append(source.attr("name"));
            const char* val = source.attr("value", true);
            if (val)
               media.append(':').append(val);
            media += "\r\n";
        });
    });

    return media;
};

static unique_ptr<StringMap> ParsedSdp::iceparams(const string& mediadesc, const string& sessiondesc)
{
    unique_ptr<StringMap> data(new StringMap);
    string ufrag = find_line(mediadesc, "a=ice-ufrag:", sessiondesc);
    string pwd = find_line(mediadesc, "a=ice-pwd:", sessiondesc);
    if (!ufrag.empty() && !pwd.empty())
    {
            (*data)["ufrag"] = ufrag.substr(12);
            (*data)["pwd"] = pwd.substr(10);
    }
    return data;
}

void MLine::MLine(const string& line)
{
    split(line.substring(2), " ", fmt);

    data.media = fmt[0];
    data.port = fmt[1];
    data.proto = fmt[2];
    fmt.erase(fmt.begin(), fmt.begin()+3);
}

string MLine::toSdp()
{
    string ret;
    ret.append("m=").append(media).append(" ").append(port)
       .append(" ").append(proto).append(" ");
    for (f: fmt)
        ret.append(f)+=" ";
    if (!fmt.empty())
        fmt.resize(ret.size()-1);
    return ret;
}

unique_ptr<StringMap> ParsedSdp::parse_rtpmap(const string& line)
{
    vector<string> parts;
    split(line.substr(9), " ", parts);
    unique_ptr<StringMap> ret(StringMap);
    StringMap& data = *ret;
    data["id"] = parts[0];
    StringMap parts2;
    split(parts[1], "/", parts2);
    data["name"] = parts2[0];
    data["clockrate"] = parts2[1];
    data["channels"] = (parts2.size() >= 3) ? parts2[2] : "1";
    return ret;
}
string ParsedSdp::build_rtpmap(Stanza el)
{
    string line = "a=rtpmap:" + el.attr("id") + " " + el.attr("name") + "/" + el.attr("clockrate");
    const char* channels = el.attr("channels", true);
    if (channels && strcmp(channels, "1"))
        line.append("/").append(channels);
    return line;
}

unique_ptr<StringMap> ParsedSdp::parse_crypto(const string& line)
{
   vector<string> parts;
   split(line.substring(9), " ", parts);
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
string ParsedSdp::parse_fingerprint(const string& line, StringMap& attrs)
{ // RFC 4572
   vector<string> parts;
   split(line.substring(14), " ", parts);
   attrs.clear();
   attrs["hash"] = parts[0];
   attrs["xmlns"] = "urn:xmpp:tmp:jingle:apps:dtls:0";
//   data.fingerprint = parts[1];
// TODO assert that fingerprint satisfies 2UHEX *(":" 2UHEX) ?
   return parts[1];
}
unique_ptr<vector<pair<string, string> > > ParsedSdp::parse_fmtp(const string& line)
{
   vector<string> parts;
   line = afterFirst(line, " ");
   unique_ptr<vector<pair<string, string> > ret(new vector<pair<stirng, string> >);
   split(line, ";", parts);
   for (auto part: parts)
   {
        string key, value;
        auto eqpos = part.find('=');
        if (eqpos == string::npos)
            key = part;
          else
          {
            key = part.substr(0, eqpos);
            value = part.substr(eqpos+1);
          }
          if (key.empty())
             continue;
          size_t keystart = 0;
          while (key[keystart] == " ")
             keystart++;
          key = key.substring(keystart);
          if (!value.empty())
            ret->emplace_back(key, value);
           else
                // rfc 4733 (DTMF) style stuff
            ret->emplace_back("", key);
    }
    return ret;
}

unique_ptr<StringMap> ParsedSdp::parse_extmap(const string& line)
{
   vector<string> parts;
   split(line.substr(9), " ", parts);
   unique_ptr<StringMap> ret(new StringMap);
   StringMap& data = *ret;
   string& value = data["value"] = parts[0];
   size_t slashpos = value.find('/');
   if (slashpos != string::npos)
   {
       data["direction"] = value.substr(slashpos + 1);
       value = value.substr(0, slashpos);
   }
   else
       data["direction"] = "both";

   data["uri"] = parts[1];
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
       return text.substr(pos);
    }
}

string ParsedSdp::find_line(const string& haystack, const string& needle, size_t& start)
{
    string ret;
    if (haystack.substr(start, needle.size()) == needle)
         ret = tillEol(haystack, start);
    if (!ret.empty())
        return ret;
    string srch = "\r\n"+needle;
    start = haystack.find(srch, start);
    if (start != string::npos)
    {
        start+=2; //we want to return into start, thats why we pass it to tillEol()
        return tillEol(haystack, start);
    }
     else
        return "";
}
string ParsedSdp::find_line(const string& haystack, const string& needle)
{
     size_t start = 0;
     return find_line(haystack, needle, start);
}

string ParsedSdp::find_line(const string& haystack, const string& needle, const string& session)
{
    string ret = find_line(haystack, needle);
    if (ret.empty())
            return find_line(sessionpart, needle);
}
unique_ptr<vector<string> > ParsedSdp::find_lines(const string haystack, const string needle)
{
   size_t start = 0;
   unique_ptr<vector<string> > ret(new vector<string>);
   vector<string>& lines = *ret;
   string line;
   for (;;)
   {
      line = find_line(haystack, needle, start);
      if (start == string::npos)
      {
            if (!line.empty())
                lines.push_back(line);
            break;
      }
      else
         lines.push_back(line);
      return ret;
}

unique_ptr<vector<string> > ParsedSdp::find_lines(const string haystack, const string needle, const string& sessionpart)
{
    auto lines = find_lines(haystack, needle);
    if (!lines.empty())
        return lines;

// search session part
    return find_lines(sessionart, needle);
}
unique_ptr<StringMap> ParsedSdp::candidateToJingle(const string& line)
{
// a=candidate:2979166662 1 udp 2113937151 192.168.2.100 57698 typ host generation 0
//      <candidate component=... foundation=... generation=... id=... ip=... network=... port=... priority=... protocol=... type=.../>
    if (line.substr(0, 12) != 'a=candidate:')
        throw runtime_error("candidateToJingle called with a line that is not a candidate line");
    if (line.substring(line.size() - 2) == "\r\n") // chomp it
            line.resize(line.size()-2);
    unique_ptr<StringMap> ret(new StringMap);
    StringMap& candidate = *ret;
    vector<string> elems;
    split(line, " ", elems);
    if (elems[6] != "typ")
        throw runtime_error("candidateToJingle: did not find typ in the right place");
    candidate["foundation"] = elems[0].substr(12);
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
    static atomic<unsigned> id = 0;
    candidate["id"] = to_string(++id); // not applicable to SDP -- FIXME: should be unique, not just random
    return ret;
}
string ParsedSdp::candidateFromJingle(Stanza cand)
{
    string line = "a=candidate:";
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
        auto reladdr = cand.attr("rel-addr", true);
        auto relport = cand.attr("rel-port", true);
        if (reladdr && relport)
        {
             line.append("raddr ").append(reladdr).append(" ")
                 .append("rport ").append(relport).append(" ");
        }
    }
    line+= "generation ";
    auto gen = cand.attr("generation");
    line+= gen?gen:"0";
    line+= "\r\n";
    return line;
};

void split(const string& str, const char* sep, vector<string>& ret, unsigned flags, unsigned max)
{
    size_t seplen = strlen(sep);
    const char* start = strstr(str.c_str(), sep);
    if (!(flags & SPLITF_NO_FIRST))
    {
        if (start)
            ret.emplace_back(str.c_str(), start-str.c_str());
        else
        {
            ret.emplace_back(str.c_str());
            return;
        }
    }
    for (unsigned count=0; start; count++)
    {
        start+=seplen;
        if (count < max)
        {
            const char* end = strstr(start, sep);
            if (end)
            {
                ret.emplace_back(start, end-start);
                start = end;
                continue;
            }
        }
        //either max exceeded or end == NULL
        ret.emplace_back(start);
        return;
    }
}

shared_ptr<vector<string> > split(const string& str, const char* sep, unsigned flags, unsigned max)
{
    vector<string>* ret = new vector<string>();
    split(str, sep, ret, flags, max);
    return ret;
}

string beforeFirst(const string& str, const char* sep)
{
    const char* pos = strstr(str.c_str(), sep);
    if (pos)
        return str.substr(0, pos-str.c_str());
    else
        return "";
}
string afterFirst(const string str, const char* sep)
{
   const char* pos = strstr(str.c_str(), sep);
   if (pos)
      return str.substr(pos+1);
   else
      return string();
}

size_t strArrIndexOf(const vector<string>& arr, const string& str)
{
    for (auto i: arr)
        if (arr[i] == str)
            return i;
    return string::npos;
}
}
