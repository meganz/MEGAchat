#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "streamPlayer.h"
#include <karereId.h>
#include "rtcStats.h"
#include <serverListProvider.h>
#include <chatd.h>
#include <base/trackDelete.h>

namespace chatd
{
    class Connection;
    class Client;
}
namespace rtcModule
{
namespace stats { class IRtcStats; }
enum: uint8_t
{
    RTCMD_CALL_REQUEST = 0, // initiate new call, receivers start ringing
    RTCMD_CALL_RINGING = 1, // notifies caller that there is a receiver and it is ringing
    RTCMD_CALL_REQ_DECLINE = 2, // decline incoming call request, with specified Term code
    // (can be only kBusy and kCallRejected)
    RTCMD_CALL_REQ_CANCEL = 3,  // caller cancels the call requests, specifies the request id
    RTCMD_CALL_TERMINATE = 4, // hangup existing call, cancel call request. Works on an existing call
    RTCMD_JOIN = 5, // join an existing/just initiated call. There is no call yet, so the command identifies a call request
    RTCMD_SESSION = 6, // join was accepter and the receiver created a session to joiner
    RTCMD_SDP_OFFER = 7, // joiner sends an SDP offer
    RTCMD_SDP_ANSWER = 8, // joinee answers with SDP answer
    RTCMD_ICE_CANDIDATE = 9, // both parties exchange ICE candidates
    RTCMD_SESS_TERMINATE = 10, // initiate termination of a session
    RTCMD_SESS_TERMINATE_ACK = 11, // acknowledge the receipt of SESS_TERMINATE, so the sender can safely stop the stream and
    // it will not be detected as an error by the receiver
    RTCMD_MUTE = 12
};
enum TermCode: uint8_t
{
    kUserHangup = 0,         // < Normal user hangup
    kCallReqCancel = 1,      // < Call request was canceled before call was answered
    kCallRejected = 2,       // < Outgoing call has been rejected by the peer OR incoming call has been rejected by
    // <another client of our user
    kAnsElsewhere = 3,       // < Call was answered on another device of ours
    kAnswerTimeout = 5,      // < Call was not answered in a timely manner
    kRingOutTimeout = 6,     // < We have sent a call request but no RINGING received within this timeout - no other
    // < users are online
    kAppTerminating = 7,     // < The application is terminating
    kCallGone = 8,
    kBusy = 9,               // < Peer is in another call
    kNormalHangupLast = 20,  // < Last enum specifying a normal call termination
    kErrorFirst = 21,        // < First enum specifying call termination due to error
    kErrApiTimeout = 22,     // < Mega API timed out on some request (usually for RSA keys)
    kErrFprVerifFailed = 23, // < Peer DTLS-SRTP fingerprint verification failed, posible MiTM attack
    kErrProtoTimeout = 24,   // < Protocol timeout - one if the peers did not send something that was expected,
                             // < in a timely manner
    kErrProtocol = 25,       // < General protocol error
    kErrInternal = 26,       // < Internal error in the client
    kErrLocalMedia = 27,     // < Error getting media from mic/camera
    kErrNoMedia = 28,        // < There is no media to be exchanged - both sides don't have audio/video to send
    kErrNetSignalling = 29,  // < chatd shard was disconnected
    kErrIceDisconn = 30,     // < ice-disconnect condition on webrtc connection
    kErrIceFail = 31,        // <ice-fail condition on webrtc connection
    kErrSdp = 32,            // < error generating or setting SDP description
    kErrUserOffline = 33,    // < we received a notification that that user went offline
    kErrorLast = 33,         // < Last enum indicating call termination due to error
    kLast = 33,              // < Last call terminate enum value
    kPeer = 128,             // < If this flag is set, the condition specified by the code happened at the peer,
                             // < not at our side
    kInvalid = 0x7f
};

bool isTermError(TermCode code)
{
    return (code & 0x7f) >= TermCode::kErrorFirst;
}
class ISessionHandler
{
public:
    virtual void onStateChange(uint8_t newState) = 0;
    virtual void onDestroy(TermCode reason, bool byPeer, const std::string& msg) = 0;
    virtual void onRemoteStreamAdded(IVideoRenderer*& rendererOut) = 0;
    virtual void onRemoteStreamRemoved() = 0;
    virtual void onPeerMute(karere::AvFlags av) = 0;
};

class ICallHandler
{
public:
    virtual void onStateChange(uint8_t newState) = 0;
    virtual void onDestroy(TermCode reason, bool byPeer, std::string& msg) = 0;
    virtual ISessionHandler* onNewSession(Session& sess) = 0;
    virtual void onLocalMediaError(const std::string errors) = 0;
    virtual void onRingOut(karere::Id peer) = 0;
    virtual void onCallStarting() = 0;
    virtual void onCallStarted() = 0;
};
class IGlobalHandler
{
public:
    virtual ICallHandler* onCallIncoming(Call& call) = 0;
    virtual bool onAnotherCall(Call& existingCall, karere::Id userid) = 0;
    virtual bool isGroupChat(karere::Id chatid) = 0;
};
const char* termCodeToStr(TermCode code);
struct StateDesc
{
    std::vector<std::vector<uint8_t>> transMap;
    const char*(*toStrFunc)(uint8_t);
    void assertStateChange(uint8_t oldState, uint8_t newState) const;
};

const char* iceStateToStr(webrtc::PeerConnectionInterface::IceConnectionState);
struct SdpKey
{
    char data[32];
};
struct RtMessage;
class RtcModule;
class Call;
class IGlobalHandler;
class ICallHandler;
class ISessionHandler;
class ICryptoFunctions;

class Session: public karere::DeleteTrackable
{
public:
    enum: uint8_t
    {
        kStateWaitSdpOffer, // < Session just created, waiting for SDP offer from initiator
        kStateWaitSdpAnswer, // < SDP offer has been sent by initiator, waniting for SDP answer
        kStateWaitLocalSdpAnswer, // < Remote SDP offer has been set, and we are generating SDP answer
        kStateInProgress,
        kStateTerminating, // < Session is in terminate handshake
        kStateDestroyed // < Session object is not valid anymore
    };
    static const char* stateToStr(uint8_t state);
    const char* stateStr() const { return stateToStr(mState); }
protected:
    static const StateDesc sStateDesc;
    Call& mCall;
    karere::Id mSid;
    uint8_t mState;
    bool mIsJoiner;
    karere::Id mPeer;
    karere::Id mPeerAnonId;
    uint32_t mPeerClient;
    karere::AvFlags mPeerAv;
    artc::tspMediaStream mRemoteStream;
    std::shared_ptr<artc::StreamPlayer> mRemotePlayer;
    std::string mOwnSdp;
    std::string mPeerSdp;
    SdpKey mOwnSdpKey;
    SdpKey mPeerSdpKey;
    artc::myPeerConnection<Session> mRtcConn;
    std::string mName;
    ISessionHandler* mHandler;
    std::unique_ptr<stats::Recorder> mStatRecorder;
    megaHandle mSetupTimer = 0;
    time_t mTsIceConn = 0;
    std::unique_ptr<promise::Promise<void>> mTerminatePromise;
    void setState(uint8_t state);
    void handleMessage(RtMessage& packet);
    void createRtcConn();
    void sendCmdSession(RtMessage& packet);
    void sendAv(karere::AvFlags av);
    promise::Promise<void> sendOffer();
    void msgSdpOfferSendAnswer(RtMessage& packet);
    void msgSdpAnswer(RtMessage& packet);
    void msgSessTerminateAck(RtMessage& packet);
    void msgSessTerminate(RtMessage& packet);
    void msgIceCandidate(RtMessage& packet);
    void msgMute(RtMessage& packet);
    void mungeSdp(std::string& sdp);
    void submitStats(TermCode termCode, const std::string& errInfo);
    bool verifySdpFingerprints(const std::string& sdp, const SdpKey& peerHash);
    template<class... Args>
    bool cmd(uint8_t type, Args... args);
    void destroy(TermCode code, const std::string& msg="");
    promise::Promise<void> terminateAndDestroy(TermCode code, const std::string& msg="");
    webrtc::FakeConstraints* pcConstraints();
    void handleMsg(RtMessage& packet);
public:
    Session(Call& call, RtMessage& packet);
    //PeerConnection events
    void onAddStream(artc::tspMediaStream stream);
    void onRemoveStream(artc::tspMediaStream stream);
    void onIceCandidate(std::shared_ptr<artc::IceCandText> cand);
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state);
    void onIceComplete();
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState);
    void onDataChannel(webrtc::DataChannelInterface* data_channel);
    void onRenegotiationNeeded() {}
    void onError() {}
    friend class Call;
};
struct EndpointId
{
    karere::Id userid;
    uint32_t clientid;
    EndpointId(karere::Id aUserid, uint32_t aClientid): userid(aUserid), clientid(aClientid){}
    bool operator<(EndpointId other) const
    {
         if (userid.val < other.userid.val)
             return true;
         else if (userid.val > other.userid.val)
             return false;
         else
             return (clientid < other.clientid);
    }
};

class Call: public karere::WeakReferenceable<Call>
{
public:
    enum: uint8_t
    {
        kStateInitial,      // < Call object was initialised
        kStateHasLocalStream,
        kStateReqSent,      // < Call request sent
        kStateRingIn,       // < Call request received, ringing
        kStateJoining,      // < Joining a call
        kStateInProgress,
        kStateTerminating, // < Call is waiting for sessions to terminate
        kStateDestroyed    // < Call object is not valid anymore
    };
    static const char* stateToStr(uint8_t state);
    const char* stateStr() const { return stateToStr(mState); }
protected:
    static const StateDesc sStateDesc;
    std::map<karere::Id, std::shared_ptr<Session>> mSessions;
    std::map<EndpointId, int> mSessRetries;
    std::unique_ptr<std::set<karere::Id>> mRingOutUsers;
    RtcModule& mManager;
    karere::Id mChatid;
    karere::Id mId;
    std::string mName;
    ICallHandler* mHandler;
    uint8_t mState;
    chatd::Connection& mShard;
    bool mIsGroup;
    bool mIsJoiner;
    karere::Id mCallerUser = 0;
    uint32_t mCallerClient = 0;
    megaHandle mCallOutTimer = 0;
    bool mCallStartingSignalled = false;
    bool mCallStartedSignalled = false;
    megaHandle mInCallPingTimer = 0;
    promise::Promise<void> mDestroyPromise;
    std::shared_ptr<artc::LocalStreamHandle> mLocalStream;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    void setState(uint8_t newState);
    void handleMessage(RtMessage& packet);
    void msgCallTerminate(RtMessage& packet);
    void msgSession(RtMessage& packet);
    void msgJoin(RtMessage& packet);
    void msgRinging(RtMessage& packet);
    void msgCallReqDecline(RtMessage& packet);
    void msgCallReqCancel(RtMessage& packet);
    void handleReject(RtMessage& packet);
    void handleBusy(RtMessage& packet);
    bool startLocalStream(bool allowEmpty);
    void createLocalPlayer();
    void freeLocalStream();
    void removeRemotePlayer();
    void muteUnmute(karere::AvFlags what, bool state);
    void onUserOffline(karere::Id userid, uint32_t clientid);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void clearCallOutTimer();
    void notifyNewSession(Session& sess);
    void notifySessionConnected(Session& sess);
    void removeSession(Session& sess, TermCode reason);
    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    void onRemoteStreamAdded(artc::tspMediaStream stream);
    void onRemoteStreamRemoved(artc::tspMediaStream);
    promise::Promise<void> destroy(TermCode termcode, bool weTerminate, const std::string& msg="");
    promise::Promise<void> gracefullyTerminateAllSessions(TermCode code);
    promise::Promise<void> waitAllSessionsTerminated(TermCode code, const std::string& msg="");
    bool startOrJoin(karere::AvFlags av);
    template <class... Args>
    bool cmd(uint8_t type, karere::Id userid, uint32_t clientid, Args... args);
    template <class... Args>
    bool cmdBroadcast(uint8_t type, Args... args);
    void startIncallPingTimer();
    void stopIncallPingTimer();
    bool broadcastCallReq();
    bool join(karere::Id userid=0);
    friend class RtcModule;
    friend class Session;
public:
    chatd::Connection& shard() const { return mShard; }
    Call(RtcModule& rtcModule, karere::Id chatid, chatd::Connection& shard,
        karere::Id callid, bool isGroup, bool isJoiner, ICallHandler* handler,
        karere::Id callerUser, uint32_t callerClient);
    uint8_t state() const { return mState; }
    karere::Id id() const { return mId; }
    karere::Id chatid() const { return mChatid; }
    virtual karere::AvFlags sentAv() const;
    virtual karere::AvFlags receivedAv() const;
    void hangup(TermCode reason=TermCode::kInvalid);
    bool answer(karere::AvFlags av);
    void changeLocalRenderer(IVideoRenderer* renderer);
    void getLocalStream(karere::AvFlags av, std::string& errors);
    karere::AvFlags muteUnmute(karere::AvFlags av);
    karere::AvFlags localAv() const;
//    bool isRelayed() const;
};

class RtcModule: public chatd::IRtcHandler
{
public:
    enum {
        kApiTimeout = 20000,
        kCallAnswerTimeout = 40000,
        kRingOutTimeout = 6000,
        kIncallPingInterval = 4000,
        kMediaGetTimeout = 20000,
        kSessSetupTimeout = 20000
    };
    int maxbr = 0;
    RtcModule(karere::Client& client, IGlobalHandler* handler, ICryptoFunctions& crypto,
        const karere::ServerList<karere::TurnServerInfo>& servers);
    void onDisconnect(chatd::Connection& conn);
    int setIceServers(const karere::ServerList<karere::TurnServerInfo>& servers);
    void handleMessage(chatd::Connection& conn, const StaticBuffer& msg);
    void onUserJoinLeave(karere::Id chatid, karere::Id userid, chatd::Priv priv);
    std::shared_ptr<Call> joinCall(karere::Id chatid, karere::AvFlags av, ICallHandler* handler);
    std::shared_ptr<Call> startCall(karere::Id chatid, karere::AvFlags av, ICallHandler* handler);
    void onUserOffline(karere::Id chatid, karere::Id userid, uint32_t clientid);
    void onShutdown();
    ~RtcModule();
protected:
    karere::Client& mClient;
    karere::Id mOwnAnonId;
    ICryptoFunctions& mCrypto;
    IGlobalHandler* mHandler;
    std::shared_ptr<webrtc::PeerConnectionInterface::IceServers> mIceServers;
    artc::DeviceManager mDeviceManager;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    artc::InputAudioDevice mAudioInput;
    artc::InputVideoDevice mVideoInput;
    webrtc::FakeConstraints mPcConstraints;
    std::map<karere::Id, std::shared_ptr<Call>> mCalls;
    void msgCallRequest(RtMessage& packet);
    template <class... Args>
    void cmdEndpoint(uint8_t type, const RtMessage& info, Args... args);
    void removeCall(Call& call);
    std::shared_ptr<artc::LocalStreamHandle> getLocalStream(karere::AvFlags av, std::string& errors);
    std::shared_ptr<Call> startOrJoinCall(karere::Id chatid, karere::AvFlags av,
        ICallHandler* handler, bool isJoin);
    void getAudioInDevices(std::vector<std::string>& devices) const;
    void getVideoInDevices(std::vector<std::string>& devices) const;
    bool selectVideoInDevice(const std::string& devname);
    bool selectAudioInDevice(const std::string& devname);
protected:
    webrtc::FakeConstraints mediaConstraints;
    //=== Implementation methods
    void initInputDevices();
    const cricket::Device* getDevice(const std::string& name, const artc::DeviceList& devices);
    bool selectDevice(const std::string& devname, const artc::DeviceList& devices,
                      std::string& selected);
    bool isCaptureActive() const { return (mAudioInput || mVideoInput); }
    friend class Call;
    friend class Session;
};

struct RtMessage
{
public:
    enum { kHdrLen = 23, kPayloadOfs = kHdrLen+1 };
    chatd::Connection& shard;
    uint8_t opcode;
    uint8_t type;
    karere::Id chatid;
    karere::Id userid;
    karere::Id callid;
    uint32_t clientid;
    StaticBuffer payload;
    static const char* typeToStr(uint8_t type);
    const char* typeStr() const { return typeToStr(type); }
    RtMessage(chatd::Connection& aShard, const StaticBuffer& msg);
};

class RtMessageComposer: public chatd::Command
{
protected:
    using Command::read; // hide all read/write methods, as they will include the
    using Command::write; // whole command, not the payload
public:
    /** Creates an RtMessage as a base for derived classes (with userid/clientid)
     * @param opcode The chatd command opcode. Can be OP_RTMSG_BROADCAST,
     * OP_RTMSG_USER, OP_RTMSG_CLIENT
     * @param type The payload-specific type. This is the first byte of the payload
     * @param reserve How much bytes to reserve in the buffer for the payload data.
     * This does not include the payload type byte.
     * @param hdrlen The length of the header. This is used to calculate the data
     * length field from the total buffer length. Does not include the payload type byte,
     * which is part of the payload data.
     */
    RtMessageComposer(uint8_t opcode, uint8_t type, karere::Id chatid, karere::Id userid, uint32_t clientid, uint16_t reserve=32)
        : Command(opcode, RtMessage::kPayloadOfs+reserve, RtMessage::kHdrLen) //third param - command payload size doesn't include the opcode byte
    {
        //(opcode.1 chatid.8 userid.8 clientid.4 len.2) (type.1 data.(len-1))
        //              ^                                          ^
        //          header.23                             payload.len
        write<uint64_t>(1, chatid.val);
        write<uint64_t>(9, userid.val);
        write<uint32_t>(17, chatid.val);
        write<uint8_t>(RtMessage::kHdrLen, type);
        updateLenField();
    }
    void updateLenField()
    {
        assert(dataSize()-RtMessage::kHdrLen >= 1);
        Buffer::write<uint16_t>(RtMessage::kHdrLen-2, dataSize()-RtMessage::kHdrLen);
    }
    template<class T> void doPayloadAppend(T arg) { Buffer::append(arg); }
    void doPayloadAppend(karere::Id arg) { Buffer::append(arg.val); }
    template<class T, class... Args> void doPayloadAppend(T arg1, Args... args)
    {
        doPayloadAppend(arg1);
        doPayloadAppend(args...);
    }
public:
    template<class T, typename=typename std::enable_if<std::is_pod<T>::value>::type>
    void payloadWrite(size_t offset, T val)
    {
        write<T>(RtMessage::kPayloadOfs+offset, val);
    }
    template<class T, class... Args>
    void payloadAppend(T arg1, Args... args)
    {
        doPayloadAppend(arg1, args...);
        updateLenField();
    }
};

struct VidEncParams
{
    /** @brief Minimum bitrate for video encoding, in kbits/s */
    uint16_t minBitrate = 0;
    /** @brief Maximum bitrate for video encoding, in kbits/s */
    uint16_t maxBitrate = 0;
    /** @brief Maximum spatial quantization for video encoding.
      * This specifies the maximum size of image area to be encoded with one
      * color - the bigger this value is, the more coarse and blocky the image
      * is. Value of 1 should disable quantization.
      */
    uint16_t maxQuant = 0;

    /** @brief The target buffer latency of the video stream, in milliseconds */
    uint16_t bufLatency = 0;

};

}
#endif
