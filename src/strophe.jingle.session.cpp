#include "strophe.jingle.session.h"
//#include "strophe.jingle.h"

using namespace std;
using namespace promise;
using namespace strophe;
namespace karere
{
namespace rtcModule
{
//temp definition of Jingle class
struct JingleEventHandler
{
    void onRemoteStreamAdded(rtc::tspMediaStream stream);
    void onRemoteStreamRemoved(rtc::tspMediaStream stream);
};

class Jingle
{
public:
    webrtc::PeerConnectionInterface::IceServers iceServers;
    JingleEventHandler eventHandler;
};

JingleSession::JingleSession(Jingle& jingle, const string& myJid, const string& peerjid,
 const string& sid, Connection& connection,
 rtc::tspMediaStream sessLocalStream, const AvFlags& mutedState)
	:mMyJid(myJid), mSid(sid), mConnection(connection), mJingle(jingle),
      mPeerJid(peerjid), mLocalMutedState(mutedState), mLocalStream(sessLocalStream)
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
		mInitiator = mMyJid;
		mResponder = mPeerJid;
	}
	else
	{
		mInitiator = mPeerJid;
		mResponder = mMyJid;
	}
    
   //TODO: Add constraints
	webrtc::FakeConstraints pcmc;
    pcmc.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);
    rtc::myPeerConnection<JingleSession> peerconn(mJingle.iceServers, *this, NULL);
    mPeerConn = peerconn;

	KR_THROW_IF_FALSE((mPeerConn->AddStream(mLocalStream, NULL)));
}
//PeerConnection events
void JingleSession::onAddStream(rtc::tspMediaStream stream)
{
	mRemoteStream = stream;
    mJingle.eventHandler.onRemoteStreamAdded(stream);
}
void JingleSession::onIceCandidate(std::shared_ptr<rtc::IceCandText> candidate)
{
	sendIceCandidate(candidate);
}
void JingleSession::onRemoveStream(rtc::tspMediaStream stream)
{
    if (stream != mRemoteStream) //we can't throw here because we are in a callback
	{
		KR_LOG_ERROR("onRemoveStream: Stream is not the remote stream that we have");
		return;
	}
	mRemoteStream = NULL;
	mJingle.eventHandler.onRemoteStreamRemoved(stream);
}
void JingleSession::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{
    if (newState == webrtc::PeerConnectionInterface::kIceConnectionConnected)
		mStartTime = getTimeMs();
    else if (newState == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
		mEndTime = getTimeMs();
//TODO: maybe forward to mJingle.eventHandler
}

void JingleSession::onIceComplete()
{
	KR_LOG_DEBUG("onIceComplete");
}

//end of event handlers

void JingleSession::terminate(const string& reason)
{
	mState = SESSTATE_ENDED;
	if (mPeerConn.get())
    {
		mPeerConn->Close();
		mPeerConn = NULL;
    }
}

Promise<Stanza> JingleSession::sendIceCandidate(std::shared_ptr<rtc::IceCandText> candidate)
{
	if (!mPeerConn.get()) //peerconnection may have been closed already
		return 0;

	auto transportAttrs = SdpUtil::iceparams(mLocalSdp.media[candidate->sdpMLineIndex], mLocalSdp->session);
	auto candAttrs = SdpUtil::candidateToJingle(candidate->candidate);
//TODO: put the xmlns in iceparams() function
	transportAttrs.emplace_back("xmlns", "urn:xmpp:jingle:transports:ice-udp:1");

// map to transport-info
	auto cand = createJingleIq(mPeerJid, "transport-info");
	auto fpNode = cand
	  .c("content", {
		  {"creator", jCreator()},
		  {"name", candidate.sdpMid}
	  })
	  .c("transport", transportAttrs)
	  .c("candidate", candAttrs)
	  .parent();
// add fingerprint
	if (SdpUtil::find_line(mLocalSdp.media[candidate.sdpMLineIndex], "a=fingerprint:", mLoclSdp.session))
	{
		map<string, string> fpData;
		SdpUtil::parse_fingerprint(fpData,
			SdpUtil::find_line(mLocalSdp.media[candidate.sdpMLineIndex], "a=fingerprint:", mLocalSdp.session));
		fpData["required"] = "true";
		fpNode = fpNode.c("fingerprint").t(fpData["fingerprint"]).parent();
		fpData.erase("fingerprint");
		fpNode.attrs(fpData);
	}
	return sendIq(cand, "transportinfo");
}

Promise<Stanza> JingleSession::sendOffer()
{
	return mPeerConn.createOffer(NULL)
	.then([this](webrtc::SessionDescriptionInterface* sdp)
	{
		mLocalSdp.reset(new SdpUtil::ParsedSdp(sdp));
		auto cands = SDPUtil.find_lines(mLocalSdp.raw, "a=candidate:");
		for (auto c: cands)
		{
			auto cand = SdpUtil::parse_icecandidate(*c);
            if (cand.type == "srflx")
				mHadStunCandidate = true;
            else if (cand.type == "relay")
				mHadTurnCandidate = true;
		}
//change: js code updates the sdp sent to setLocalDesc from parsedSdp.raw, but it seems to be just the same as the original
		return mPeerConn.setLocalDescription(sdp);
	})
	.then([this](sspSdpText)
	{
		auto init = createJingleIq(mPeerJid, "session-initiate");
		mLocalSdp->toJingle(init, jCreator());
		addFingerprintHmac(init);
		return sendIq(init, "offer");
	});
}

Promise<int> setRemoteDescription(XMPP::Stanza* elem, const string& desctype)
{
	if (mRemoteSdp.get())
		throw runtime_error("setRemoteDescription() from stanza: already have remote description");
	mRemoteSdp.reset(new SdpUtils::ParsedSdp(elem));
	unique_ptr<webrtc::JsepSessionDescription> jsepSdp(
		new webrtc::JsepSessionDescription(desctype));
	webrtc::SdpParseError error;
	if (!jsepSdp->Initialize(mRemoteSdp.raw, &error))
		throw std::runtime_error("Error parsing SDP: line "+error.line+"\nError: "+error.description);
     
	return mPeerConn.setRemoteDescription(jsepSdp.release());
}

void JingleSession::addIceCandidates(strophe::Stanza transportInfo)
{
	if (!isActive())
        return;
    // operate on each content element
	transportInfo.forEachChild("content", [this](strophe::Stanza& content)
	{
		string mid = content.attr("name");
		// would love to deactivate this, but firefox still requires it
//		int mLineIdx = getMlineIndex(mRemoteSdp, mid);
//TODO: Make sure onmy sdpMid is enough (setting mLineIndex to 0)
		// TODO: check ice-pwd and ice-ufrag?
		content.child("transport").forEachChild("candidate")([this](strophe::Stanza& jcand)
		{
			string line = SdpUtil::candidateFromJingle(jcand);
			unique_ptr<webrtc::JsepIceCandidate> cand(
			  new webrtc::JsepIceCandidate(mid, 0));
			webrtc::SdpParseError err;
			if (!cand->Initialize(line, &err))
				throw runtime_error("Error parsing ICE candidate:\nline "+err.line+" Error:" +err.description);

			mPeerConn->AddIceCandidate(cand.release());
		});
	});
}

int JingleSession::getMlineIndex(unique_ptr<ParsedSdp>& sdp, string& name)
{
	for (int i = 0; i < sdp->media.size(); i++)
	{
		auto& media = sdp->media[i];
		if (SdpUtil::find_line(media, "a=mid:" + name) ||
			media.find("m=" + name) == 0)
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
	.then([this](webrtc::SessionDescriptionInterface* sdp)
	{
		checkActive("created SDP answer");
		mLocalSdp.reset(new SdpUtil::ParsedSdp(sdp));
    //this.localSDP.mangle();
		auto accept = createJingleIq(mPeerJid, "session-accept");
		mLocalSdp->toJingle(accept, jCreator());
		addFingerprintHmac(accept);
		auto sendPromise = sendIq(accept, "answer");
		return when(sendPromise, mPeerConn.setLocalDescription(sdp));
	});
}


Promise<Stanza> JingleSession::sendTerminate(const string& reason, const string& text)
{
	auto term =	createJingleIq(mPeerJid, "session-terminate");
	auto reason = term.c("reason").c(reason.empty()?"unknown":reason.c_str()).parent();
	if (!text.empty())
		reason.c("text").t(text);
	return mConnection.sendIq(term, "terminate");
}

Promise<Stanza> JingleSession::sendMute(bool muted, const string& what)
{
	auto info = createJingleIq(mPeerJid, "session-info");
	info.c(muted ? "mute" : "unmute", {
	  {"xmlns", "urn:xmpp:jingle:apps:rtp:info:1"},
	  {"name", what.c_str()}
	});
	return mConnection.sendIq(info);
}

void JingleSession::syncMutedState()
{
	if (!mLocalStream.get())
        return;
	auto ats = mLocalStream->GetAudioTracks();
	for (auto& at: ats)
		at->set_enabled(!mMutedState.audioMuted);
	auto vts = mLocalStream->GetVideoTracks();
	for (auto& vt: vts)
		vt->set_enabled(!mMutedState.videoMuted);
}

void JingleSession::sendMutedState()
{
	if (mMutedState.audio)
		sendMute(true, "voice");
	if (mMutedState.video)
		sendMute(true, "video");
}

void JingleSession::muteUnmute(bool state, const AvFlags& what)
{
//First do the actual muting, and only then send the signalling
    if (what.audio)
		mMutedState.audio = state;
    if (what.video)
		mMutedState.video = state;
    syncMutedState();
    if (what.audio)
		sendMute(state, "voice");
    if (what.video)
		sendMute(state, "video");
}

void JingleSession::reportError(const string& e, const char* where)
{
	mJingle.eventHandler.onInternalError(e, where);
}

void JingleSession::addFingerprintHmac(strophe::Stanza jiq)
{
	if (!mPeerNonce)
		throw std::runtime_error("addFingerprintHmac: No peer nonce has been received");
	set<string> fps;
	auto j = jiq.child("jingle");
	j.forEachChild("content", [fps&](strophe::Stanza& content)
	{
	   content.forEachChild("transport", [fps&](strophe::Stanza& transport)
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
	auto fprmac = mJingle.generateHmac(strFps, mPeerNonce);
	j.attr("fprmac", fprmac);
}

Stanza JingleSession::createJingleIq(const char* to, const char* action)
{
	Stanza s(mConnection.context());
	s.init("iq", {
	  {"type", "set"},
	  {"to", to.c_str()}
	});

	s.c("jingle",{
	  {"xmlns", "urn:xmpp:jingle:1"},
	  {"action", action.c_str()},
	  {"initiator", mInitiator.c_str()},
	  {"sid": mSid.c_str()}
	});
	return s;
}

Promise<Stanza> JingleSession::sendIq(Stanza iq, const char* type)
{
    return mConnection.sendIqQuery(iq, type, NULL);
}
}
}
