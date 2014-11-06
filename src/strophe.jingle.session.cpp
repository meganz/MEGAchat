#include "strophe.jingle.session.h"
#include "strophe.jingle.sdp.h"
#include "strophe.jingle.h"
#include "karereCommon.h"

using namespace std;
using namespace promise;
using namespace strophe;
using namespace sdpUtil;
namespace karere
{
namespace rtcModule
{

JingleSession::JingleSession(Jingle& jingle, const string& myJid, const string& peerjid,
 const string& sid, Connection& connection,
 artc::tspMediaStream sessLocalStream, const AvFlags& mutedState, const StringMap& props,
 FileTransferHandler* ftHandler)
    :StringMap(props), mOwnJid(myJid), mPeerJid(peerjid), mSid(sid), mConnection(connection),
      mJingle(jingle), mLocalMutedState(mutedState), mLocalStream(sessLocalStream),
      mFtHandler(ftHandler)
{
    syncMutedState();
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
    if (newState == webrtc::PeerConnectionInterface::kIceConnectionConnected)
        mStartTime = timestampMs();
    else if (newState == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
        mEndTime = timestampMs();
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
    auto transport = cand
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
    return sendIq(cand, "transportinfo");
}

Promise<Stanza> JingleSession::sendOffer()
{
    return mPeerConn.createOffer(&mMediaConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp)
    {
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
        return mPeerConn.setLocalDescription(sdp);
    })
    .then([this](int)
    {
        auto init = createJingleIq(mPeerJid, "session-initiate");
        mLocalSdp.toJingle(init, jCreator());
        addFingerprintMac(init);
        return sendIq(init, "offer");
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
        throw std::runtime_error("Error parsing SDP: line "+error.line+"\nError: "+error.description);

    return mPeerConn.setRemoteDescription(jsepSdp.release());
}

void JingleSession::addIceCandidates(Stanza transportInfo)
{
    if (!isActive())
        return;
    // operate on each content element
    transportInfo.forEachChild("content", [this](Stanza content)
    {
        string mid = content.attr("name");
        // would love to deactivate this, but firefox still requires it
//		int mLineIdx = getMlineIndex(mRemoteSdp, mid);
//TODO: Make sure only sdpMid is enough (setting mLineIndex to 0)
        // TODO: check ice-pwd and ice-ufrag?
        content.child("transport").forEachChild("candidate", [this, &mid](Stanza jcand)
        {
            string line = sdpUtil::candidateFromJingle(jcand);
            unique_ptr<webrtc::JsepIceCandidate> cand(
              new webrtc::JsepIceCandidate(mid, 0));
            webrtc::SdpParseError err;
            if (!cand->Initialize(line, &err))
                throw runtime_error("Error parsing ICE candidate:\nline "+err.line+" Error:" +err.description);

            mPeerConn->AddIceCandidate(cand.release());
        });
    });
}

int JingleSession::getMlineIndex(const sdpUtil::ParsedSdp& sdp, string& name)
{
    for (size_t i = 0; i < sdp.media.size(); i++)
    {
        auto& media = sdp.media[i];
        if (!sdpUtil::find_line(media, "a=mid:" + name).empty() ||
            (media.find("m=" + name) == 0))
            return i;
        }
    return -1;
}

Promise<int> JingleSession::sendAnswer()
{
    checkActive("sendAnswer");
//must first send the sdp, and then setLocalDescription because
//ICE candidates may start being generated before we had a chance to send
//the sdp, so the peer will receive ICE candidates before the answer
    return mPeerConn.createAnswer(&mMediaConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp) mutable
    {
        checkActive("created SDP answer");
        string strSdp;
        KR_THROW_IF_FALSE(sdp->ToString(&strSdp));
        mLocalSdp.parse(strSdp);
    //this.localSDP.mangle();
        auto accept = createJingleIq(mPeerJid, "session-accept");
        mLocalSdp.toJingle(accept, jCreator());
        addFingerprintMac(accept);
        auto sendPromise = sendIq(accept, "answer");
        return when(sendPromise, mPeerConn.setLocalDescription(sdp));
    });
}


Promise<Stanza> JingleSession::sendTerminate(const string& reason, const string& text)
{
    auto term =	createJingleIq(mPeerJid, "session-terminate");
    auto rsn = term.c("reason").c(reason.empty()?"unknown":reason.c_str()).parent();
    if (!text.empty())
        rsn.c("text").t(text);
    return sendIq(term, "set");
}

Promise<Stanza> JingleSession::sendMute(bool muted, const string& what)
{
    auto info = createJingleIq(mPeerJid, "session-info");
    info.c(muted ? "mute" : "unmute", {
      {"xmlns", "urn:xmpp:jingle:apps:rtp:info:1"},
      {"name", what.c_str()}
    });
    return sendIq(info, "set");
}

void JingleSession::syncMutedState()
{
    if (!mLocalStream.get())
        return;
    auto ats = mLocalStream->GetAudioTracks();
    for (auto& at: ats)
        at->set_enabled(!mLocalMutedState.audio);
    auto vts = mLocalStream->GetVideoTracks();
    for (auto& vt: vts)
        vt->set_enabled(!mLocalMutedState.video);
}

Promise<int> JingleSession::sendMutedState()
{
    return when(mLocalMutedState.audio?sendMute(true, "voice"):Promise<Stanza>(Stanza()),
                mLocalMutedState.video?sendMute(true, "video"):Promise<Stanza>(Stanza())
           );
}

Promise<int> JingleSession::muteUnmute(bool state, const AvFlags& what)
{
//First do the actual muting, and only then send the signalling
    if (what.audio)
        mLocalMutedState.audio = state;
    if (what.video)
        mLocalMutedState.video = state;
    syncMutedState();
    return when(what.audio?sendMute(state, "voice"):Promise<Stanza>(Stanza()),
                what.video?sendMute(state, "video"):Promise<Stanza>(Stanza())
    );
}

void JingleSession::reportError(const string& e, const char* where)
{
    mJingle.onInternalError(e, where);
}

void JingleSession::addFingerprintMac(strophe::Stanza jiq)
{
    if (at("peerFprMacKey").empty())
        throw std::runtime_error("addFingerprintMac: No peer fprMacKey has been received");
    set<string> fps;
    auto j = jiq.child("jingle");
    j.forEachChild("content", [&fps](Stanza content)
    {
       content.forEachChild("transport", [&fps](Stanza transport)
       {
            auto fpnode = transport.child("fingerprint");
            fps.insert(fpnode.attr("hash")+string(" ")+fpnode.text());
       });
    });
    string strFps;
    for(auto& strFp: fps)
    {
      strFps+=strFp;
      strFps+=';';
    }
    strFps.resize(strFps.size()-1); //truncate last ';'
    string fprmac = mJingle.crypto().generateMac(strFps, (*this)["peerFprMacKey"]);
    j.setAttr("fprmac", fprmac.c_str());
}

Stanza JingleSession::createJingleIq(const string& to, const char* action)
{
    Stanza s(xmpp_conn_get_context(mConnection));
    s.init("iq", {
      {"type", "set"},
      {"to", to.c_str()}
    });

    s.c("jingle",{
      {"xmlns", "urn:xmpp:jingle:1"},
      {"action", action},
      {"initiator", mInitiator.c_str()},
      {"sid", mSid.c_str()}
    });
    return s;
}

Promise<Stanza> JingleSession::sendIq(Stanza iq, const string& origin)
{
    return mConnection.sendIqQuery(iq)
        .fail([this, iq, origin](const promise::Error& err)
        {
            mJingle.onJingleError(this, origin, err.msg().c_str(), iq);
            return err;
        });
}

}
}
