#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "streamPlayer.h"

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
struct StateDesc
{
    std::vector<std::vector<uint8_t>> TransMap;
    const char*(*toStrFunc)(uint8_t);
};

void assertStateChange(uint8_t oldState, uint8_t newState, const StateDesc& desc);
struct SdpKey
{
    char data[32];
};

class Session
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
    AvFlags mPeerAv;
    artc::tspMediaStream mRemoteStream;
    sdpUtil::ParsedSdp mOwnSdp;
    sdpUtil::ParsedSdp mPeerSdp;
    SdpKey mOwnSdpKey;
    SdpPey mPeerSdpKey;
    artc::myPeerConnection<JingleSession> mPeerConn;
    std::string mName;
    std::unique_ptr<stats::Recorder> mStatsRecorder;
};

class Call
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
    std::shared_ptr<artc::LocalStreamHandle> mLocalStream;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    void createSession(const std::string& peerjid, const std::string& me, FileTransferHandler* ftHandler=NULL);
    std::shared_ptr<stats::IRtcStats>
    hangupSession(TermCode termcode, const std::string& text, bool nosend);
    bool startLocalStream(bool allowEmpty);
    void createLocalPlayer();
    void freeLocalStream();
    void removeRemotePlayer();
    void muteUnmute(AvFlags what, bool state);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void onPeerMute(AvFlags affected);
    void onPeerUnmute(AvFlags affected);
    void onMediaStart();
    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    void onRemoteStreamAdded(artc::tspMediaStream stream);
    void onRemoteStreamRemoved(artc::tspMediaStream);
    void destroy(TermCode termcode, const std::string& text="", bool noSendSessTerm=false);
    bool hangup(TermCode termcode, const std::string& text="", bool rejectIncoming=false);
    friend class Jingle;
    friend class RtcModule;
    friend class JingleSession;
public:

    virtual AvFlags sentAv() const;
    virtual AvFlags receivedAv() const;
    bool hangup(const std::string& text="") { return hangup(kUserHangup, text, true); }
    void changeLocalRenderer(IVideoRenderer* renderer);
    std::string id() const;
    int isRelayed() const;
};

class RtcModule
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
protected:
    IGlobalHandler* mHandler;
    artc::DeviceManager mDeviceManager;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    artc::InputAudioDevice mAudioInput;
    artc::InputVideoDevice mVideoInput;
    std::unique_ptr<webrtc::FakeConstraints> mPcConstraints;
    std::map<karere::Id, Call&> mCalls;
public:
    RtcModule(karere::Client& client, IGlobalHandler* handler,
               ICryptoFunctions& crypto, const std::string& iceServers);
    void startMediaCall(IEventHandler* userHandler, const std::string& targetJid,
        AvFlags av, const char* files[]=nullptr, const std::string& myJid="");
    std::shared_ptr<Call> getCallByJid(const char* fullJid, char type='m');
    void getAudioInDevices(std::vector<std::string>& devices) const;
    void getVideoInDevices(std::vector<std::string>& devices) const;
    bool selectVideoInDevice(const std::string& devname);
    bool selectAudioInDevice(const std::string& devname);
    int muteUnmute(AvFlags what, bool state, const std::string& jid);
    ~RtcModule();
protected:
    //=== Implementation methods
    bool hasLocalStream() { return (mAudioInput || mVideoInput); }
    void initInputDevices();
    const cricket::Device* getDevice(const std::string& name, const artc::DeviceList& devices);
    bool selectDevice(const std::string& devname, const artc::DeviceList& devices,
                      std::string& selected);
    std::string getLocalAudioAndVideo();
    bool hasCaptureActive();
    std::shared_ptr<artc::LocalStreamHandle> getLocalStream(std::string& errors);

    void onConnState(const xmpp_conn_event_t status,
                     const int error, xmpp_stream_error_t * const stream_error);

    void onPresenceUnavailable(strophe::Stanza pres);
    virtual void discoAddFeature(const char* feature);
    friend class Call;
    //overriden handler of JinglePlugin
};
}
#endif
