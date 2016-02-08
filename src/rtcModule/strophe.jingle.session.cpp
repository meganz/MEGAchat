#include "strophe.jingle.session.h"
#include "strophe.jingle.sdp.h"
#include "strophe.jingle.h"
#include "rtcStats.h"
#include "karereCommon.h"
#include "stringUtils.h"
#include "rtcModule.h"
#include "ICryptoFunctions.h"

using namespace std;
using namespace promise;
using namespace strophe;
using namespace sdpUtil;

namespace rtcModule
{

JingleSession::JingleSession(Call& call, FileTransferHandler* ftHandler)
    :mCall(call), mJingle(mCall.mRtc),
    mSid(call.mSid),
    mFtHandler(ftHandler){}

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
    mPeerConn = artc::myPeerConnection<JingleSession>(*mJingle.mIceServers,
        *this, &mJingle.mMediaConstraints);

    KR_THROW_IF_FALSE((mPeerConn->AddStream(*mCall.mLocalStream)));
}
//PeerConnection events
void JingleSession::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    mCall.onRemoteStreamAdded(stream);
}
void JingleSession::onIceCandidate(std::shared_ptr<artc::IceCandText> candidate)
{
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
void JingleSession::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{
//TODO: maybe forward to mJingle
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

Promise<Stanza> JingleSession::sendOffer()
{
    return mPeerConn.createOffer(&mJingle.mMediaConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp)
    {
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
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
    if (!mRemoteSdp.raw.empty())
        throw runtime_error("setRemoteDescription() from stanza: already have remote description");
    mRemoteSdp.parse(elem);
    unique_ptr<webrtc::JsepSessionDescription> jsepSdp(
        new webrtc::JsepSessionDescription(desctype));
    webrtc::SdpParseError error;
    if (!jsepSdp->Initialize(mRemoteSdp.raw, &error))
        throw std::runtime_error("Error parsing SDP: line='"+error.line+"'\nError: "+error.description);
//    KR_LOG_COLOR(34, "translated remote sdp:\n%s\n", mRemoteSdp.raw.c_str());
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
    return mPeerConn.createAnswer(&mJingle.mMediaConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp) mutable
    {
        checkActive("created SDP answer");
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
    //this.localSDP.mangle();
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

}
