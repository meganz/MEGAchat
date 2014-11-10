#ifndef STROPHE_JINGLE_SESSION_H
#define STROPHE_JINGLE_SESSION_H
#include <memory>
#include "promise.h"
#include "karereCommon.h"
#include "webrtcAdapter.h"
#include <talk/app/webrtc/test/fakeconstraints.h>
#include "streamPlayer.h"
#include "strophe.jingle.sdp.h"
#include <mstrophepp.h>

namespace karere {
namespace rtcModule {

class Jingle;
class JingleEventHandler; //can't nest it into Jingle

class FileTransferHandler
{
public:
    void remove(const char*, const char*){}
};
//Dummy stats recorder. TODO: Implement
class StatsRecorder
{
public:
    bool isRelay() const {return false;}
};

typedef std::vector<strophe::Stanza> StanzaQueue;
//This is the interface of the session object exposed to the application via events etc
//TODO: Move to public interface header
struct IJingleSession
{
    typedef void(*DeleteFunc)(void*);

    virtual const char* getSid() const = 0;
    virtual const char* getJid() const = 0;
    virtual const char* getPeerJid() const = 0;
    virtual bool isCaller() const = 0;
/** Returns whether the session's udnerlying media connection is relayed via a TURN server or not
 * @return -1 if unknown, 1 if true, 0 if false
 */
    virtual int isRelayed() const = 0;
    virtual void setUserData(void*, DeleteFunc delFunc) = 0;
    virtual void* getUserData() const = 0;
    virtual bool isRealSession() const {return true;}
};

class JingleSession: public IJingleSession, public StringMap
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
    artc::myPeerConnection<JingleSession> mPeerConn;
    std::string mOwnJid;
    std::string mPeerJid;
    std::string mSid;
    strophe::Connection& mConnection;
    Jingle& mJingle;
    std::string mInitiator;
    std::string mResponder;
    bool mIsInitiator;
    State mState = SESSTATE_NULL;
    std::unique_ptr<FileTransferHandler> mFtHandler;
    artc::tspMediaStream mLocalStream;
    artc::tspMediaStream mRemoteStream;
    sdpUtil::ParsedSdp mLocalSdp;
    sdpUtil::ParsedSdp mRemoteSdp;
    webrtc::FakeConstraints mMediaConstraints;
    void* mUserData = nullptr;
    DeleteFunc mUserDataDelFunc = nullptr;
//    bool mLastIceCandidate = false;
    void reportError(const std::string& msg, const char* where);
    void addFingerprintMac(strophe::Stanza jingle);
public:
    std::unique_ptr<StanzaQueue> inputQueue;
    std::shared_ptr<artc::StreamPlayer> remotePlayer;
    AvFlags mRemoteMutedState;
    AvFlags mLocalMutedState;
    Ts tsMediaStart = 0;
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
    strophe::Stanza createJingleIq(const std::string& to, const char* action);
    int getMlineIndex(const sdpUtil::ParsedSdp& sdp, std::string& name);
//IJingleSession public interface
    virtual const char* getSid() const {return mSid.c_str();}
    virtual const char* getJid() const {return mOwnJid.c_str();}
    virtual const char* getPeerJid() const {return mPeerJid.c_str();}
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
        const AvFlags& mutedState, const StringMap& props, FileTransferHandler* ftHandler=NULL);
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
    FileTransferHandler* ftHandler() const {return mFtHandler.get();}
    promise::Promise<strophe::Stanza> sendIceCandidate(std::shared_ptr<artc::IceCandText> candidate);
    promise::Promise<int> setRemoteDescription(strophe::Stanza stanza, const std::string& desctype);
//    void addIceCandidate(strophe::Stanza stanza);
    void addIceCandidates(strophe::Stanza transportInfo);
    promise::Promise<int> sendAnswer();
    promise::Promise<strophe::Stanza> sendTerminate(const std::string& reason, const std::string& text);
    promise::Promise<strophe::Stanza> sendMute(bool muted, const std::string& what);
    void syncMutedState();
    promise::Promise<int> sendMutedState();
    promise::Promise<int> muteUnmute(bool state, const AvFlags& what);
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq, const std::string &origin);
};
}
}

#endif // STROPHE_JINGLE_SESSION_H
