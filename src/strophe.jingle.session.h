#ifndef STROPHE_JINGLE_SESSION_H
#define STROPHE_JINGLE_SESSION_H
#include <memory>
#include "promise.h"
#include "karereCommon.h"
#include "webrtcAdapter.h"

namespace strophe {
	class Connection;
	class Stanza;
}

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
	MediaConstraints mMediaConstraints;
	bool mLastIceCandidate = false;
    void reportError(const ::karere::StringMap& info, const std::string& msg);
    void addFingerprintHmac(strophe::Stanza& jingle);
//PeerConnection callback interface
	void onError() {KR_LOG_ERROR("session %s: peerconnection called onError()", mSid.c_str());}
	void onAddStream(rtc::tspMediaStream stream);
	void onRemoveStream(rtc::tspMediaStream stream);
	void onIceCandidate(std::shared_ptr<rtc::IceCandText> candidate);
	void onIceComplete();
	void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState){}
	void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState);
	void onRenegotiationNeeded() {}
//==
public:
	JingleSession(Jingle& jingle, const	std::string& myJid,
		const std::string& peerJid,	const std::string& sid,
        strophe::Connection& connection, rtc::tspMediaStream sessLocalStream,
        const AvFlags& mutedState);
	void initiate(bool isInitiator);
	promise::Promise<int> accept();
    promise::Promise<int> sendOffer();
	void terminate(); //TODO: maybe not needed
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
	void sendIceCandidate(std::shared_ptr<rtc::IceCandText> candidate);
    promise::Promise<int> setRemoteDescription(strophe::Stanza& stanza, const std::string& desctype);
    void addIceCandidate(strophe::Stanza& stanza);
	promise::Promise<int> sendAnswer();
	void sendTerminate(const std::string& reason, const std::string& text);
	promise::Promise<int> sendIq(const karere::StringMap& attrs, const std::string& desc);
	void sendMute(bool muted, const std::string& what);
	void syncMutedState();
	void sendMutedState();
    void muteUnmute(bool state, const AvFlags& what);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq, const char* type="set");
};
}
}

#endif // STROPHE_JINGLE_SESSION_H
