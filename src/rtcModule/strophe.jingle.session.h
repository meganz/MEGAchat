#ifndef STROPHE_JINGLE_SESSION_H
#define STROPHE_JINGLE_SESSION_H
#include <memory>
#include <mstrophepp.h>
#include "promise.h"
#include "karereCommon.h"
#include "webrtcAdapter.h"
#include <talk/app/webrtc/test/fakeconstraints.h>
#include "streamPlayer.h"
#include "strophe.jingle.sdp.h"
#include "IRtcModule.h"

namespace rtcModule
{
class Call;
class Jingle;
class JingleSession;
namespace stats { class Recorder; }

class FileTransferHandler
{
public:
    void remove(const std::string&, const std::string&){}
};

typedef std::vector<strophe::Stanza> StanzaQueue;
const char* makeCallId(const JingleSession&);

class JingleSession
{
public:
    enum State {
        SESSTATE_NULL = 0,
        SESSTATE_ENDED = 1,
        SESSTATE_ACTIVE = 2,
        SESSTATE_ERROR = 3,
        SESSTATE_PENDING = 4
    };
    struct MediaConstraints
    {
        bool audio = false;
        bool video = false;
        int width = 0;
        int height = 0;
    };
    Call& mCall;
    Jingle& mJingle; //needs to be public for makeCallId()
protected:
    std::string mSid;
public:
    AvFlags mRemoteAvState = AvFlags(false,false);
protected:
    std::string mInitiator;
    std::string mResponder;
    bool mIsInitiator;
    State mState = SESSTATE_NULL;
    artc::tspMediaStream mRemoteStream;
    sdpUtil::ParsedSdp mLocalSdp;
    sdpUtil::ParsedSdp mRemoteSdp;
    std::unique_ptr<FileTransferHandler> mFtHandler;
//    bool mLastIceCandidate = false;
    void reportError(const std::string& msg, const char* reason, const char* text, unsigned flags=0);
    void addFingerprintMac(strophe::Stanza jingle);
public:
    artc::myPeerConnection<JingleSession> mPeerConn;
    std::unique_ptr<StanzaQueue> inputQueue;
    karere::Ts tsMediaStart = 0;
    std::unique_ptr<stats::Recorder> mStatsRecorder;

//PeerConnection callback interface
    void onError() {KR_LOG_ERROR("session %s: peerconnection called onError()", mSid.c_str());}
    void onAddStream(artc::tspMediaStream stream);
    void onRemoveStream(artc::tspMediaStream stream);
    void onIceCandidate(std::shared_ptr<artc::IceCandText> candidate);
    void onIceComplete();
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState){}
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState);
    void onRenegotiationNeeded() {}
    //first is the root (iq) stanza, second is the jingle child of the iq element
    std::pair<strophe::Stanza, strophe::Stanza>
    createJingleIq(const std::string& to, const char* action);
//IJingleSession public interface
    bool isCaller() const {return mIsInitiator;}
    int isRelayed() const;
//===
    JingleSession(Call& call, FileTransferHandler* ftHandler=NULL);
    void initiate(bool isInitiator);
    ~JingleSession();
    promise::Promise<strophe::Stanza> accept();
    promise::Promise<strophe::Stanza> sendOffer();
    artc::tspMediaStream& getRemoteStream() {return mRemoteStream;}
    void terminate(const std::string& reason, const std::string& text="", bool nosend=false); //TODO: maybe can be integrated in another place
    inline bool isActive()
    {
         return (mPeerConn && (mState != SESSTATE_ENDED) && (mState != SESSTATE_ERROR)
         && (mPeerConn->signaling_state() != webrtc::PeerConnectionInterface::kClosed));
    }
    inline void checkActive(const char* opname)
    {
        if (!isActive())
            throw std::runtime_error((opname?opname:"")+std::string(": Session '")+mSid+
            "' is not active anymore or peerconnection has been closed");
    }
    inline const char* jCreator() const
    {
        return (mIsInitiator?"initiator":"responder");
    }
    State state() const {return mState;}
    const std::string& sid() const {return mSid;}
    bool isInitiator() const {return mIsInitiator;}
    FileTransferHandler* ftHandler() const {return mFtHandler.get();}
    promise::Promise<strophe::Stanza> sendIceCandidate(std::shared_ptr<artc::IceCandText> candidate);
    promise::Promise<void> setRemoteDescription(strophe::Stanza stanza, const std::string& desctype);
//    void addIceCandidate(strophe::Stanza stanza);
    void addIceCandidates(strophe::Stanza transportInfo);
    promise::Promise<void> sendAnswer();
    promise::Promise<void> answer(strophe::Stanza offer);
    promise::Promise<strophe::Stanza> sendTerminate(const std::string& reason, const std::string& text);
    promise::Promise<void> sendMute(bool unmuted, const std::string& what);
    promise::Promise<void> sendMuteDelta(AvFlags oldf, AvFlags newf);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq,
        const std::string& origin);
};

}

#endif // STROPHE_JINGLE_SESSION_H
