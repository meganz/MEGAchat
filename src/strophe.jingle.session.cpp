#include "strophe.jingle.session.h"
#include "strophe.jingle.sdp.h"
#include "strophe.jingle.h"
#include "karereCommon.h"
#include "StringUtils.h"

using namespace std;
using namespace promise;
using namespace strophe;
using namespace sdpUtil;

namespace rtcModule
{

JingleSession::JingleSession(Jingle& jingle, const string& myJid, const string& peerjid,
 const string& sid, Connection& connection,
 artc::tspMediaStream sessLocalStream, const AvFlags& avState, const StringMap& props,
 FileTransferHandler* ftHandler)
    :Base(props), mJingle(jingle), mSid(sid), mOwnJid(myJid), mPeerJid(peerjid),
      mLocalAvState(avState), mConnection(connection), mLocalStream(sessLocalStream),
      mFtHandler(ftHandler)
{
    syncAvState();
}

void JingleSession::initiate(bool isInitiator)
{
    if (mState != SESSTATE_NULL)
        throw runtime_error("Attempt to initiate on session " + mSid +
                  "in state " + to_string(mState));

    mIsInitiator = isInitiator;
    mState = SESSTATE_PENDING;
    if (isInitiator)
    {
        mInitiator = jid();
        mResponder = peerJid();
    }
    else
    {
        mInitiator = peerJid();
        mResponder = jid();
    }
    //TODO: make it more elegant to initialize mPeerConn
    artc::myPeerConnection<JingleSession> peerconn(*mJingle.mIceServers, *this, &mJingle.mMediaConstraints);
    mPeerConn = peerconn;

    KR_THROW_IF_FALSE((mPeerConn->AddStream(mLocalStream, NULL)));
}
//PeerConnection events
void JingleSession::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    mJingle.onRemoteStreamAdded(*this, stream);
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
    mRemoteStream = NULL;
    mJingle.onRemoteStreamRemoved(*this, stream);
}
void JingleSession::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{
//TODO: maybe forward to mJingle
}

void JingleSession::onIceComplete()
{
    KR_LOG_DEBUG("onIceComplete");
    mJingle.onIceComplete(*this);
}

//end of event handlers

void JingleSession::terminate(const char* reason, const char* text)
{
    if (mFtHandler)
        mFtHandler->remove(reason, text);

    mState = SESSTATE_ENDED;
    if (mPeerConn.get())
    {
        mPeerConn->Close();
        mPeerConn = NULL;
    }
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
    auto cand = createJingleIq(mPeerJid, "transport-info");
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
//        KR_LOG_COLOR(34, "offer:\n%s\n", strSdp.c_str());
        mLocalSdp.parse(strSdp);
        return mPeerConn.setLocalDescription(sdp);
    })
    .then([this](int)
    {
        auto init = createJingleIq(mPeerJid, "session-initiate");
        mLocalSdp.toJingle(init.second, jCreator());
        addFingerprintMac(init.second);
        return sendIq(init.first, "offer");
    });
}

Promise<int> JingleSession::setRemoteDescription(Stanza elem, const string& desctype)
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

Promise<int> JingleSession::sendAnswer()
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
        auto accept = createJingleIq(mPeerJid, "session-accept");
        mLocalSdp.toJingle(accept.second, jCreator());
        addFingerprintMac(accept.second);
        auto sendPromise = sendIq(accept.first, "answer");
        return when(sendPromise, mPeerConn.setLocalDescription(sdp));
    });
}


Promise<Stanza> JingleSession::sendTerminate(const char* reason, const char* text)
{
    auto term =	createJingleIq(mPeerJid, "session-terminate");
    auto rsn = term.second.c("reason").c(reason?reason:"unknown").parent();
    if (text)
        rsn.c("text").t(text);
    return sendIq(term.first, "set");
}

Promise<Stanza> JingleSession::sendMute(bool muted, const string& what)
{
    auto info = createJingleIq(mPeerJid, "session-info");
    info.second.c(muted ? "mute" : "unmute", {
      {"xmlns", "urn:xmpp:jingle:apps:rtp:info:1"},
      {"name", what.c_str()}
    });
    return sendIq(info.first, "set");
}

void JingleSession::syncAvState()
{
    if (!mLocalStream.get())
        return;
    auto ats = mLocalStream->GetAudioTracks();
    for (auto& at: ats)
        at->set_enabled(mLocalAvState.audio);
    auto vts = mLocalStream->GetVideoTracks();
    for (auto& vt: vts)
        vt->set_enabled(mLocalAvState.video);
}

Promise<int> JingleSession::sendAvState()
{
    return promise::when(
                mLocalAvState.audio?Promise<Stanza>(Stanza()):sendMute(true, "voice"),
                mLocalAvState.video?Promise<Stanza>(Stanza()):sendMute(true, "video"));
}

Promise<int> JingleSession::muteUnmute(bool state, const AvFlags& what)
{
//First do the actual muting, and only then send the signalling
    if (what.audio)
        mLocalAvState.audio = !state;
    if (what.video)
        mLocalAvState.video = !state;
    syncAvState();
    return promise::when(
                what.audio?sendMute(state, "voice"):Promise<Stanza>(Stanza()),
                what.video?sendMute(state, "video"):Promise<Stanza>(Stanza()));
}

void JingleSession::reportError(const string& e, const char* where)
{
    mJingle.onInternalError(e, where);
}

void JingleSession::addFingerprintMac(strophe::Stanza j)
{
    if (at("peerFprMacKey").empty())
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
    VString fprmac(mJingle.crypto().generateMac(
        strFps.c_str(), (*this)["peerFprMacKey"].c_str()));
    j.setAttr("fprmac", fprmac.c_str());
}

pair<Stanza, Stanza> JingleSession::createJingleIq(const string& to, const char* action)
{
    Stanza root(xmpp_conn_get_context(mConnection));
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

Promise<Stanza> JingleSession::sendIq(Stanza iq, const string& origin)
{
    return mConnection.sendIqQuery(iq)
        .fail([this, iq, origin](const promise::Error& err)
        {
            mJingle.onJingleError(this, origin, err.msg(), iq);
            return err;
        });
}

}
