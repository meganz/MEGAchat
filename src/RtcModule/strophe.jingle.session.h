#ifndef STROPHE_JINGLE_SESSION_H
#define STROPHE_JINGLE_SESSION_H
#include <memory>
#include <mstrophepp.h>
#include "Promise.h"
#include "KarereCommon.h"
#include "WebrtcAdapter.h"
#include <talk/app/webrtc/test/fakeconstraints.h>
#include "StreamPlayer.h"
#include "strophe.jingle.sdp.h"
#include "IJingleSession.h"
#include "IRtcModule.h" //needed for StatOptions only

namespace rtcModule
{

class Jingle;
class JingleEventHandler; //can't nest it into Jingle
class JingleSession;

class FileTransferHandler
{
public:
    void remove(const char*, const char*){}
};
//Dummy stats recorder. TODO: Implement
class RtcStats: public IRtcStats
{
public:
    bool isCaller;
    std::string termRsn;
    RtcStats(JingleSession& sess, const StatOptions& options){}
//        :isCaller(sess.isInitiator()){}
};

class BasicStats: public IRtcStats
{
public:
    bool isCaller;
    std::string termRsn;
    BasicStats(const IJingleSession& sess, const char* aTermRsn)
        :isCaller(sess.isCaller()), termRsn(aTermRsn?aTermRsn:""){}
};

class StatsRecorder
{
protected:
    JingleSession& mSession;
    StatOptions mOptions;
public:
    StatsRecorder(JingleSession& sess, const StatOptions& options)
        :mSession(sess), mOptions(options){}
    bool isRelay() const {return false;}
    void start() {}
    std::shared_ptr<RtcStats> terminate(std::string&& callId)
    {
        return std::shared_ptr<RtcStats>(new RtcStats(mSession, mOptions));
    }
};

typedef std::vector<strophe::Stanza> StanzaQueue;

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
protected:
    typedef karere::StringMap Base;
    Jingle& mJingle;
    std::string mSid;
    std::string mOwnJid;
    std::string mPeerJid;
public:
    AvFlags mRemoteAvState;
    AvFlags mLocalAvState;
protected:
    artc::myPeerConnection<JingleSession> mPeerConn;
    ::strophe::Connection& mConnection;
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
    void reportError(const std::string& msg, const char* where);
    void addFingerprintMac(strophe::Stanza jingle);
public:
    std::unique_ptr<StanzaQueue> inputQueue;
    std::shared_ptr<artc::StreamPlayer> remotePlayer;
    karere::Ts tsMediaStart = 0;
    std::unique_ptr<StatsRecorder> mStatsRecorder;

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
    virtual bool isCaller() const {return mIsInitiator;}
    virtual int isRelayed() const
    {
        if (!mStatsRecorder)
            return -1;
        return mStatsRecorder->isRelay()?1:0;
    }
    virtual void setUserData(void* userData, DeleteFunc delFunc)
    {
        delUserData();
        mUserData = userData;
        mUserDataDelFunc = delFunc;
    }
    virtual void* getUserData() const {return mUserData;}
//===
    JingleSession(Jingle& jingle, const	std::string& myJid,
        const std::string& peerJid,	const std::string& sid,
        strophe::Connection& connection, artc::tspMediaStream sessLocalStream,
        const AvFlags& avState, const karere::StringMap& props, FileTransferHandler* ftHandler=NULL);
    void initiate(bool isInitiator);
    ~JingleSession()
    {
        delUserData();
    }
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
    promise::Promise<int> sendAnswer();
    promise::Promise<strophe::Stanza> sendTerminate(const char *reason, const char *text);
    promise::Promise<strophe::Stanza> sendMute(bool muted, const std::string& what);
    void syncAvState();
    promise::Promise<int> sendAvState();
    promise::Promise<int> muteUnmute(bool state, const AvFlags& what);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq, const std::string &origin);
};
}

#endif // STROPHE_JINGLE_SESSION_H
