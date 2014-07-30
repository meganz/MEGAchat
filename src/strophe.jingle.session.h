#ifndef STROPHE_JINGLE_SESSION_H
#define STROPHE_JINGLE_SESSION_H
#include <memory>
#include "promise.h"
#include "karereCommon.h"
#include "webrtcAdapter.h"
<<<<<<< HEAD
#include <talk/app/webrtc/test/fakeconstraints.h>
=======
>>>>>>> 660f6eb55ccad6709067f8c4731f94e9f10a4c34
#include "strophe.jingle.sdp.h"
#include <mstrophepp.h>

namespace karere {
namespace rtcModule {

	class Jingle;
	class JingleEventHandler; //can't nest it into Jingle

class JingleSession: public webrtc::PeerConnectionObserver
{
public:
	enum State {
		SESSTATE_NULL = 0,
		SESSTATE_ENDED = 1,
		SESSTATE_ACTIVE = 2,
		SESSTATE_ERROR = 3,
		SESSTATE_PENDING = 4
	};
	struct AvFlags
	{
		bool audio = false;
		bool video = false;
	};
	struct MediaConstraints
	{
		bool audio = false;
		bool video = false;
		int width = 0;
		int height = 0;
	};

protected:
    rtc::myPeerConnection<JingleSession> mPeerConn;
	std::string mMyJid;
	std::string mPeerJid;
	std::string mSid;
    strophe::Connection& mConnection;
	Jingle& mJingle;
	std::string mInitiator;
	std::string mResponder;
	bool mIsInitiator;
	State mState = SESSTATE_NULL;

	AvFlags mLocalMutedState;
	AvFlags mRemoteMutedState;
	rtc::tspMediaStream mLocalStream;
	rtc::tspMediaStream mRemoteStream;
    sdpUtil::ParsedSdp mLocalSdp;
    sdpUtil::ParsedSdp mRemoteSdp;
	int mStartTime = 0;
	int mEndTime = 0;
    webrtc::FakeConstraints mMediaConstraints;
    std::string mPeerNonce;
    bool mLastIceCandidate = false;
    void reportError(const std::string& msg, const char* where);
    void addFingerprintHmac(strophe::Stanza jingle);
//PeerConnection callback interface
public:
	void onError() {KR_LOG_ERROR("session %s: peerconnection called onError()", mSid.c_str());}
	void onAddStream(rtc::tspMediaStream stream);
	void onRemoveStream(rtc::tspMediaStream stream);
	void onIceCandidate(std::shared_ptr<rtc::IceCandText> candidate);
	void onIceComplete();
	void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState){}
	void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState);
	void onRenegotiationNeeded() {}
    strophe::Stanza createJingleIq(const std::string& to, const char* action);
    int getMlineIndex(const sdpUtil::ParsedSdp& sdp, std::string& name);
//==

	JingleSession(Jingle& jingle, const	std::string& myJid,
		const std::string& peerJid,	const std::string& sid,
        strophe::Connection& connection, rtc::tspMediaStream sessLocalStream,
        const AvFlags& mutedState);
	void initiate(bool isInitiator);
<<<<<<< HEAD
    promise::Promise<strophe::Stanza> accept();
    promise::Promise<strophe::Stanza> sendOffer();
=======
	promise::Promise<int> accept();
    promise::Promise<int> sendOffer();
>>>>>>> 660f6eb55ccad6709067f8c4731f94e9f10a4c34
    void terminate(const std::string& reason); //TODO: maybe not needed
	inline bool isActive()
	{
         return ((mPeerConn.get() != NULL)
         &&	(mState != SESSTATE_ENDED)
		 && (mState != SESSTATE_ERROR)
         &&	(mPeerConn.get()->signaling_state() !=
			 webrtc::PeerConnectionInterface::kClosed));
	}
	inline void checkActive(const char* opname)
	{
		if (!isActive())
            throw std::runtime_error((opname?opname:"")+std::string(": Session '")+mSid+ "' to '"+mPeerJid+
			"' is not active anymore or peerconnection has been closed");
	}
	inline const char* jCreator() const
	{
        return (mIsInitiator?"initiator":"responder");
	}
    promise::Promise<strophe::Stanza> sendIceCandidate(std::shared_ptr<rtc::IceCandText> candidate);
<<<<<<< HEAD
    promise::Promise<int> setRemoteDescription(strophe::Stanza stanza, const std::string& desctype);
    void addIceCandidate(strophe::Stanza stanza);
    void addIceCandidates(strophe::Stanza transportInfo);
=======
    promise::Promise<int> setRemoteDescription(strophe::Stanza& stanza, const std::string& desctype);
    void addIceCandidate(strophe::Stanza& stanza);
>>>>>>> 660f6eb55ccad6709067f8c4731f94e9f10a4c34
	promise::Promise<int> sendAnswer();
    promise::Promise<strophe::Stanza> sendTerminate(const std::string& reason, const std::string& text);
	promise::Promise<int> sendIq(const karere::StringMap& attrs, const std::string& desc);
    promise::Promise<strophe::Stanza> sendMute(bool muted, const std::string& what);
    void syncMutedState();
    promise::Promise<int> sendMutedState();
    promise::Promise<int> muteUnmute(bool state, const AvFlags& what);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq, const char* type="set");
};
}
}

#endif // STROPHE_JINGLE_SESSION_H
