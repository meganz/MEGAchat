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
#include "IJingleSession.h"

namespace rtcModule
{

class Jingle;
class JingleEventHandler; //can't nest it into Jingle
class JingleSession;
namespace stats { class Recorder; }

class FileTransferHandler
{
public:
    void remove(const char*, const char*){}
};

typedef std::vector<strophe::Stanza> StanzaQueue;
const char* makeCallId(const IJingleSession*, const Jingle& jingle);

class JingleSession: public IJingleSession, public karere::StringMap
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

    Jingle& mJingle; //needs to be public for makeCallId()
protected:
    typedef karere::StringMap Base;
    std::string mSid;
    std::string mOwnJid;
    std::string mPeerJid;
public:
    AvFlags mRemoteAvState;
    AvFlags mLocalAvState;
protected:
    strophe::Connection& mConnection;
    std::string mInitiator;
    std::string mResponder;
    bool mIsInitiator;
    State mState = SESSTATE_NULL;
    artc::tspMediaStream mLocalStream;
    artc::tspMediaStream mRemoteStream;
    sdpUtil::ParsedSdp mLocalSdp;
    sdpUtil::ParsedSdp mRemoteSdp;
    std::unique_ptr<FileTransferHandler> mFtHandler;
    void* mUserData = nullptr;
    DeleteFunc mUserDataDelFunc = nullptr;
//    bool mLastIceCandidate = false;
    void reportError(const std::string& msg, const char* reason, const char* text, unsigned flags=0);
    void addFingerprintMac(strophe::Stanza jingle);
public:
    artc::myPeerConnection<JingleSession> mPeerConn;
    std::unique_ptr<StanzaQueue> inputQueue;
    std::shared_ptr<artc::StreamPlayer> remotePlayer;
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
    virtual const char* getSid() const {return mSid.c_str();}
    virtual const char* getJid() const {return mOwnJid.c_str();}
    virtual const char* getPeerJid() const {return mPeerJid.c_str();}
    virtual const char* getPeerAnonId() const {return at("peerAnonId").c_str();}
    virtual const char* getCallId() const { return makeCallId(this, mJingle); }
    virtual bool isCaller() const {return mIsInitiator;}
    virtual int isRelayed() const;
    virtual void setUserData(void* userData, DeleteFunc delFunc)
    {
        delUserData();
        mUserData = userData;
        mUserDataDelFunc = delFunc;
    }
    virtual void* getUserData() const {return mUserData;}
//===
    JingleSession(Jingle& jingle, const	std::string& myJid,
        const std::string& peerJid, const std::string& sid,
        strophe::Connection& connection, artc::tspMediaStream sessLocalStream,
        const AvFlags& avState, const karere::StringMap& props, FileTransferHandler* ftHandler=NULL);
    void initiate(bool isInitiator);
    ~JingleSession();
    void delUserData()
    {
        if(mUserData && mUserDataDelFunc)
            mUserDataDelFunc(mUserData);
    }
    promise::Promise<strophe::Stanza> accept();
    promise::Promise<strophe::Stanza> sendOffer();
    artc::tspMediaStream& getLocalStream() {return mLocalStream;}
    artc::tspMediaStream& getRemoteStream() {return mRemoteStream;}
    void terminate(const char* reason, const char* text=NULL); //TODO: maybe can be integrated in another place
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
    State state() const {return mState;}
    const std::string& jid() const {return mOwnJid;}
    const std::string& peerJid() const {return mPeerJid;}
    const std::string& sid() const {return mSid;}
    bool isInitiator() const {return mIsInitiator;}
    FileTransferHandler* ftHandler() const {return mFtHandler.get();}
    promise::Promise<strophe::Stanza> sendIceCandidate(std::shared_ptr<artc::IceCandText> candidate);
    promise::Promise<int> setRemoteDescription(strophe::Stanza stanza, const std::string& desctype);
//    void addIceCandidate(strophe::Stanza stanza);
    void addIceCandidates(strophe::Stanza transportInfo);
    promise::Promise<void> sendAnswer();
    promise::Promise<strophe::Stanza> sendTerminate(const char *reason, const char *text);
    promise::Promise<strophe::Stanza> sendMute(bool muted, const std::string& what);
    void syncAvState();
    promise::Promise<void> sendAvState();
    promise::Promise<void> muteUnmute(bool state, const AvFlags& what);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq,
        const std::string& origin, unsigned flags=0);
};

/** Contains all info about a not-yet-established session, when onCallTerminated is fired and there is no session yet */
struct FakeSessionInfo: public IJingleSession
{
    const Jingle& mJingle;
    const std::string mSid;
    const std::string mPeer;
    const std::string mJid;
    bool mIsInitiator;
    std::string mPeerAnonId;
    FakeSessionInfo(Jingle& jingle, const std::string& aSid, const std::string& aPeer,
        const std::string& aMyJid, bool aInitiator, const std::string& peerAnonId)
    :mJingle(jingle), mSid(aSid), mPeer(aPeer), mJid(aMyJid), mIsInitiator(aInitiator){}
    virtual bool isRealSession() const {return false;}
    virtual const char* getSid() const {return mSid.c_str();}
    virtual const char* getJid() const {return mJid.c_str();}
    virtual const char* getPeerJid() const {return mPeer.c_str();}
    virtual const char* getPeerAnonId() const {return mPeerAnonId.c_str();}
    virtual const char* getCallId() const { return makeCallId(this, mJingle); }
    virtual bool isCaller() const {return mIsInitiator;}
    virtual int isRelayed() const {return false;}
    virtual void setUserData(void*, DeleteFunc delFunc) {}
    virtual void* getUserData() const {return nullptr;}
};

}

#endif // STROPHE_JINGLE_SESSION_H
