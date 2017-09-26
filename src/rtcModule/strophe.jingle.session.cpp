#include "strophe.jingle.session.h"
#include "strophe.jingle.sdp.h"
#include "strophe.jingle.h"
#include "rtcStats.h"
#include "karereCommon.h"
#include "stringUtils.h"
#include "rtcModule.h"
#include "rtcmPrivate.h"
#include "ICryptoFunctions.h"
#include <regex>

using namespace std;
using namespace promise;
using namespace strophe;
using namespace sdpUtil;

namespace rtcModule
{

void JingleCall::createSession()
{}

JingleSession::~JingleSession()
{}

void JingleSession::initiate(bool isInitiator)
{
    if (mState != SESSTATE_NULL)
        throw runtime_error("Attempt to initiate on session " + mSid +
                  "in state " + to_string(mState));

    mIsInitiator = isInitiator;
    mState = SESSTATE_PENDING;
    if (isInitiator)
    {
        mInitiator = mCall.mOwnJid;
        mResponder = mCall.mPeerJid;
    }
    else
    {
        mInitiator = mCall.mPeerJid;
        mResponder = mCall.mOwnJid;
    }
    //TODO: make it more elegant to initialize mPeerConn
}
//PeerConnection events
void JingleSession::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    mCall.onRemoteStreamAdded(stream);
}
void JingleSession::onIceCandidate(std::shared_ptr<artc::IceCandText> candidate)
{
    KR_LOG_DEBUG("onIceCandidate");
    sendIceCandidate(candidate);
}
void JingleSession::onRemoveStream(artc::tspMediaStream stream)
{
    if (stream != mRemoteStream) //we can't throw here because we are in a callback
    {
        KR_LOG_ERROR("onRemoveStream: Stream is not the remote stream that we have");
        return;
    }
    mRemoteStream = nullptr;
    mCall.onRemoteStreamRemoved(stream);
}

void JingleSession::onIceComplete()
{
    KR_LOG_DEBUG("onIceComplete");
//    mCall.onIceComplete();
}

//end of event handlers

void JingleSession::terminate(const std::string& reason, const std::string& text, bool nosend)
{
    if (mState == SESSTATE_ENDED)
        return;
    mState = SESSTATE_ENDED;

    if (mFtHandler)
        mFtHandler->remove(reason, text);
    if (mStatsRecorder)
        mStatsRecorder->terminate(reason);
    if (mPeerConn.get())
    {
        mPeerConn->Close();
        mPeerConn = NULL;
    }
    if (!nosend)
        sendTerminate(reason, text);
}
//TODO: Move to sdp utils
Promise<Stanza> JingleSession::sendIceCandidate(
                               std::shared_ptr<artc::IceCandText> candidate)
{
    if (!mPeerConn.get()) //peerconnection may have been closed already
        return Error("peerconnection is closed");

    auto transportAttrs = sdpUtil::iceparams(mLocalSdp.media[candidate->sdpMLineIndex], mLocalSdp.session);
    (*transportAttrs)["xmlns"] = "urn:xmpp:jingle:transports:ice-udp:1";
    auto candAttrs = sdpUtil::candidateToJingle(candidate->candidate);

// map to transport-info
    auto cand = createJingleIq(mCall.mPeerJid, "transport-info");
    auto transport = cand.second
      .c("content", {
          {"creator", jCreator()},
          {"name", candidate->sdpMid}
      })
      .c("transport", *transportAttrs)
      .c("candidate", *candAttrs)
      .parent();
// add fingerprint
    if (candidate->sdpMLineIndex >= (int)mLocalSdp.media.size())
        throw runtime_error("sendIceCandidate: sdpMLineIndex is out of range");

    string fpline = sdpUtil::find_line(mLocalSdp.media[candidate->sdpMLineIndex], "a=fingerprint:", mLocalSdp.session);
    if (!fpline.empty())
    {
        map<string, string> fpData;
        string fp = sdpUtil::parse_fingerprint(fpline, fpData);
        fpData["required"] = "true";
        transport.c("fingerprint", fpData).t(fp);
    }
    return sendIq(cand.first, "transportinfo");
}

webrtc::SessionDescriptionInterface* parsedSdpToWebrtcSdp(
    const ParsedSdp& parsed, const std::string& type)
{
    webrtc::SdpParseError err;
    auto sdp = webrtc::CreateSessionDescription(type, parsed.toString(), &err);
    if (!sdp)
    {
        printf("ERROR PARSING SDP\n");
        throw std::runtime_error("Error parsing tweaked-for-encoding SDP string at line "
            +err.line+": "+err.description);
    }
    return sdp;
}

Promise<Stanza> JingleSession::sendOffer()
{
    return mPeerConn.createOffer(mCall.pcConstraints
        ? mCall.pcConstraints.get()
        : &mJingle.pcConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp)
    {
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
        if (tweakEncoding(mLocalSdp))
        {
            printf("SDP: %s\n", mLocalSdp.toString().c_str());
            webrtc::SessionDescriptionInterface* newSdp = parsedSdpToWebrtcSdp(mLocalSdp, sdp->type());
            delete sdp;
            sdp = newSdp;
        }
        return mPeerConn.setLocalDescription(sdp);
    })
    .then([this]()
    {
        auto init = createJingleIq(mCall.mPeerJid, "session-initiate");
        mLocalSdp.toJingle(init.second, jCreator());
        addFingerprintMac(init.second);
        return sendIq(init.first, "offer");
    });
}

Promise<void> JingleSession::setRemoteDescription(Stanza elem, const string& desctype)
{
    if (!mRemoteSdp.session.empty())
        throw runtime_error("setRemoteDescription() from stanza: already have remote description");
    mRemoteSdp.parse(elem);
    unique_ptr<webrtc::JsepSessionDescription> jsepSdp(
        new webrtc::JsepSessionDescription(desctype));
    webrtc::SdpParseError error;
    if (!jsepSdp->Initialize(mRemoteSdp.toString(), &error))
        throw std::runtime_error("Error parsing SDP: line='"+error.line+"'\nError: "+error.description);

//  KR_LOG_COLOR(34, "translated remote sdp:\n%s\n", mRemoteSdp.raw.c_str());
    return mPeerConn.setRemoteDescription(jsepSdp.release());
}

void JingleSession::addIceCandidates(Stanza transportInfo)
{
    if (!isActive())
    {
        KR_LOG_WARNING("ICE candidate received but isActive() is false");
        return;
    }
//    KR_LOG_COLOR(31, "cand: %s\n", transportInfo.dump().c_str());
    // operate on each content element
    transportInfo.forEachChild("content", [this](Stanza content)
    {
        string mid = content.attr("name");
        int mLineIdx = mRemoteSdp.getMlineIndex(mid);
        if (mLineIdx < 0)
            throw runtime_error("addIceCandidates: Could not find an m-line with id "+mid);
        // would love to deactivate this, but firefox still requires it
//		int mLineIdx = getMlineIndex(mRemoteSdp, mid);
//TODO: Make sure only sdpMid is enough (setting mLineIndex to 0)
        // TODO: check ice-pwd and ice-ufrag?
        content.child("transport").forEachChild("candidate", [this, &mid, mLineIdx](Stanza jcand)
        {
            string line = sdpUtil::candidateFromJingle(jcand);
            unique_ptr<webrtc::JsepIceCandidate> cand(
              new webrtc::JsepIceCandidate(mid, mLineIdx));
            webrtc::SdpParseError err;
            if (!cand->Initialize(line, &err))
                throw runtime_error("Error parsing ICE candidate:\nline: '"+err.line+"'\nError:" +err.description);

            //KR_LOG_COLOR(34, "cand: mid=%s, %s\n", mid.c_str(), line.c_str());

            mPeerConn->AddIceCandidate(cand.release());
        });
    });
}

Promise<void> JingleSession::sendAnswer()
{
    checkActive("sendAnswer");
//must first send the sdp, and then setLocalDescription because
//ICE candidates may start being generated before we had a chance to send
//the sdp, so the peer will receive ICE candidates before the answer
    return mPeerConn.createAnswer(mCall.pcConstraints
        ? mCall.pcConstraints.get() : &mJingle.pcConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp) mutable
    {
        checkActive("created SDP answer");
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
        if (tweakEncoding(mLocalSdp))
        {
            auto newSdp = parsedSdpToWebrtcSdp(mLocalSdp, sdp->type());
            delete sdp;
            sdp = newSdp;
        }

        auto accept = createJingleIq(mCall.mPeerJid, "session-accept");
        mLocalSdp.toJingle(accept.second, jCreator());
        addFingerprintMac(accept.second);
        auto sendPromise = sendIq(accept.first, "answer");
        return when(sendPromise, mPeerConn.setLocalDescription(sdp));
    });
}


Promise<Stanza> JingleSession::sendTerminate(const std::string& reason, const std::string& text)
{
    auto term =	createJingleIq(mCall.mPeerJid, "session-terminate");
    auto rsn = term.second.c("reason").c(reason.c_str()).parent();
    if (!text.empty())
        rsn.c("text").t(text.c_str());
    return sendIq(term.first, "set");
}

Promise<void> JingleSession::sendMute(bool muted, const string& what)
{
    auto info = createJingleIq(mCall.mPeerJid, "session-info");
    info.second.c(muted ? "mute":"unmute")
            .setAttr("xmlns", "urn:xmpp:jingle:apps:rtp:info:1")
            .setAttr("name", what.c_str());
    return sendIq(info.first, "set")
            .then([](Stanza){});
}

Promise<Stanza>
JingleSession::sendIq(Stanza iq, const string& origin)
{
    return mJingle.mConn.sendIqQuery(iq)
    .fail([this, origin](const promise::Error& err)
    {
        return promise::Error("Error iq response on operation "+origin+": "+err.msg());
    });
}

Promise<void> JingleSession::sendMuteDelta(AvFlags oldf, AvFlags newf)
{
    Promise<void> pms1 = (oldf.audio != newf.audio)
        ? sendMute(!newf.audio, "voice") : promise::_Void();
    Promise<void> pms2 = (oldf.video != newf.video)
        ? sendMute(!newf.video, "video") : promise::_Void();
    return when(pms1, pms2);
}
Promise<void> JingleSession::answer(Stanza offer)
{
    initiate(false);
    // configure session
    return setRemoteDescription(offer, "offer").then([this]()
    {
        return sendAnswer();
    })
    .then([this]()
    {
        return sendMuteDelta(AvFlags(true,true), mCall.mLocalAv);
    });
}

void JingleSession::addFingerprintMac(strophe::Stanza j)
{
    if (mCall.mPeerFprMacKey.empty())
        throw std::runtime_error("addFingerprintMac: No peer fprMacKey has been received");
    multiset<string> fps;
    j.forEachChild("content", [&fps](Stanza content)
    {
       content.forEachChild("transport", [&fps](Stanza transport)
       {
            auto fpnode = transport.child("fingerprint");
            fps.insert(fpnode.attr("hash")+string(" ")+fpnode.text().c_str());
       });
    });
    string strFps;
    for(auto& strFp: fps)
    {
      strFps+=strFp;
      strFps+=';';
    }
    if (!strFps.empty())
        strFps.resize(strFps.size()-1); //truncate last ';'
    string fprmac = mJingle.crypto().generateMac(strFps, mCall.mPeerFprMacKey);
    j.setAttr("fprmac", fprmac.c_str());
}

pair<Stanza, Stanza> JingleSession::createJingleIq(const string& to, const char* action)
{
    Stanza root(mCall.mRtc.mConn);
    return make_pair(root, root.setName("iq")
        .setAttr("type", "set")
        .setAttr("to", to.c_str())
        .c("jingle")
            .setAttr("xmlns", "urn:xmpp:jingle:1")
            .setAttr("action", action)
            .setAttr("initiator", mInitiator.c_str())
            .setAttr("sid", mSid.c_str())
    );
}

int JingleSession::isRelayed() const
{
    if (!mStatsRecorder)
        return -1;
    return mStatsRecorder->isRelay()?1:0;
}

using namespace std::regex_constants;

int JingleSession::findCodecNo(const std::string& sdp, const char* codecName)
{
    //FIXME: code 0 is actually not invalid, it's PCM 8khz
    std::smatch m;
    if (!std::regex_search(sdp, m, std::regex(std::string("a=rtpmap:(\\d+)\\s")+codecName+"/*", ECMAScript | icase))
       || (m.size() < 2))
    {
        return 0;
    }
    return atoi(m[1].str().c_str());
}

bool JingleSession::tweakEncoding(sdpUtil::ParsedSdp& sdp)
{
    sdpUtil::MGroup* video = nullptr;
    for (auto& media: sdp.media)
    {
        if (media.name() == "video")
        {
            video = &media;
            break;
        }
    }
    if (!video)
        return false;
    bool changed = tweakCodec(*video, 100);
    return changed;
}

bool JingleSession::tweakCodec(sdpUtil::MGroup& media, int codecId)
{
    bool changed = false;
    std::string line;
    line.append("a=fmtp:").append(to_string(codecId));
    auto& params = mCall.vidEncParams;
    if (params.minBitrate)
    {
        line.append(" x-google-min-bitrate=")
            .append(std::to_string(params.minBitrate))+=';';
        changed = true;
    }
    if (params.maxBitrate)
    {
        media.insert(media.begin()+1, "b=AS:"+std::to_string(params.maxBitrate));
        changed = true;
    }
    if (params.maxQuant)
    {
        line.append(" x-google-max-quantization=")
            .append(to_string(params.maxQuant))+=';';
        changed = true;
    }
    if (changed)
    {
        line.resize(line.size()-1); //remove last ';'
//        media.push_back(line);
    }
    if (params.bufLatency) //this one should be last as it adds a new line
    {
        media.push_back("a=x-google-buffer-latency:"+std::to_string(params.bufLatency));
        changed = true;
    }
    return changed;
}
}
