#ifndef IRTC_MODULE_H
#define IRTC_MODULE_H
/**
 * @file IRtcModule.h
 * @brief Public interface of the webrtc module
 *
 * (c) 2013-2015 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */
#ifdef KARERE_DISABLE_WEBRTC
namespace rtcModule
{
class IVideoRenderer;
class IGlobalHandler {};
class ICallHandler {};
class ISessionHandler {};
class ISession {};
class ICall {};
class IRtcModule;
class RtcModule;
typedef uint8_t TermCode;
}

#else

#include "karereCommon.h" //for AvFlags
#include <karereId.h>
#include <trackDelete.h>
#include <IRtcCrypto.h>

namespace chatd
{
    class Connection;
    class Chat;
    class Client;
}

namespace rtcModule
{
class IVideoRenderer;
struct RtMessage;
class RtcModule;
class IRtcModule;
class Call;
class ICall;
class Session;
class ISession;
class IGlobalHandler;
class ICallHandler;
class ISessionHandler;
class IRtcCrypto;
enum: uint8_t
{
//    RTCMD_CALL_REQUEST = 0, // obsolete, now we have CALLDATA chatd command for call requests
    RTCMD_CALL_RINGING = 1, // notifies caller that there is a receiver and it is ringing | <callid>
    RTCMD_CALL_REQ_DECLINE = 2, // decline incoming call request, with specified Term code
    // (can be only kBusy and kCallRejected) | <callid> <termCode>
    RTCMD_CALL_REQ_CANCEL = 3,  // caller cancels the call requests, specifies the request id | <callid> <termCode>
    RTCMD_CALL_TERMINATE = 4, // hangup existing call, cancel call request. Works on an existing call | <termCode>
    RTCMD_JOIN = 5, // join an existing/just initiated call. There is no call yet, so the command identifies a call request | <callid><anonId>
    RTCMD_SESSION = 6, // join was accepter and the receiver created a session to joiner | <callid><sessionId><anonId><encHashKey>
    RTCMD_SDP_OFFER = 7, // joiner sends an SDP offer | <sessionId><anonId><encHashKey><fprHash><av><SdpOffer.len><sdpOffer>
    RTCMD_SDP_ANSWER = 8, // joinee answers with SDP answer | <sessionId><fprHash><av><sdpAnswer.len><sdpAnswer>
    RTCMD_ICE_CANDIDATE = 9, // both parties exchange ICE candidates | <sessionId><LineIdx><mid.len><mid><cand.len><cand>
    RTCMD_SESS_TERMINATE = 10, // initiate termination of a session | <sessionId><termCode>
    RTCMD_SESS_TERMINATE_ACK = 11, // acknowledge the receipt of SESS_TERMINATE, so the sender can safely stop the stream and
    // it will not be detected as an error by the receiver
    RTCMD_MUTE = 12 // Change audio-video call  <av>
};
enum TermCode: uint8_t
{
    kUserHangup = 0,            // < Normal user hangup
    kCallReqCancel = 1,         // < deprecated, now we have CALL_REQ_CANCEL specially for call requests, but keep this
    // < code to notify the app when the call is cancelled (in contrast to kUserHangup, which is used when call was stablished)
    kCallRejected = 2,          // < Outgoing call has been rejected by the peer OR incoming call has been rejected in
    // < the current device
    kAnsElsewhere = 3,          // < Call was answered on another device of ours
    kRejElsewhere = 4,          // < Call was rejected on another device of ours
    kAnswerTimeout = 5,         // < Call was not answered in a timely manner
    kRingOutTimeout = 6,        // < We have sent a call request but no RINGING received within this timeout - no other
    // < users are online
    kAppTerminating = 7,        // < The application is terminating
    kCallGone = 8,
    kBusy = 9,                  // < Peer is in another call
    kNotFinished = 10,          // < It is no finished value, it is TermCode value while call is in progress
    kNormalHangupLast = 20,     // < Last enum specifying a normal call termination
    kErrorFirst = 21,           // < First enum specifying call termination due to error
    kErrApiTimeout = 22,        // < Mega API timed out on some request (usually for RSA keys)
    kErrFprVerifFailed = 23,    // < Peer DTLS-SRTP fingerprint verification failed, posible MiTM attack
    kErrProtoTimeout = 24,      // < Protocol timeout - one if the peers did not send something that was expected,
    // < in a timely manner
    kErrProtocol = 25,          // < General protocol error
    kErrInternal = 26,          // < Internal error in the client
    kErrLocalMedia = 27,        // < Error getting media from mic/camera
    kErrNoMedia = 28,           // < There is no media to be exchanged - both sides don't have audio/video to send
    kErrNetSignalling = 29,     // < chatd shard was disconnected
    kErrIceDisconn = 30,        // < The media connection got broken, due to network error
    kErrIceFail = 31,           // < Media connection could not be established, because webrtc was unable to traverse NAT.
    // < The two endpoints just couldn't connect to each other in any way(many combinations are tested, via ICE candidates)
    kErrSdp = 32,               // < error generating or setting SDP description
    kErrUserOffline = 33,       // < we received a notification that that user went offline
    kErrSessSetupTimeout = 34,  // < timed out waiting for session
    kErrSessRetryTimeout = 35,  // < timed out waiting for peer to retry a failed session
    kErrorLast = 35,            // < Last enum indicating call termination due to error
    kLast = 35,                 // < Last call terminate enum value
    kPeer = 128,                // < If this flag is set, the condition specified by the code happened at the peer,
                                // < not at our side
    kInvalid = 0x7f
};

static inline bool isTermError(TermCode code)
{
    int errorCode = code & ~TermCode::kPeer;
    return (errorCode >= TermCode::kErrorFirst) && (errorCode <= TermCode::kErrorLast);
}

const char* termCodeToStr(uint8_t code);
const char* rtcmdTypeToStr(uint8_t type);
std::string rtmsgCommandToString(const StaticBuffer& data);

class ISessionHandler
{
public:
    virtual ~ISessionHandler(){}
    virtual void onSessStateChange(uint8_t newState) = 0;
    virtual void onSessDestroy(TermCode reason, bool byPeer, const std::string& msg) = 0;
    virtual void onRemoteStreamAdded(IVideoRenderer*& rendererOut) = 0;
    virtual void onRemoteStreamRemoved() = 0;
    virtual void onPeerMute(karere::AvFlags av, karere::AvFlags oldAv) = 0;
    virtual void onVideoRecv() {}
};

class ICallHandler
{
public:
    /** @brief An \c ICall event handler is likely to need a reference to the underlying
     * ICall object, but in the case of an outgoing call or join, the handler has
     * to be created by the app before the call is created, as \c startCall and \c joinCall
     * accept the handler as parameter and return the \c ICall object.
     * In other words, in the above case, the ICallHandler will not have the ICall
     * object available at its construction. This callback provides it as soon as
     * possible, before startCall/joinCall return, and before any call event occurs
     * on that call. If the app were to assign the ICall reference upon return from
     * startCall/joinCall, events on that call may be generated before that, imposing the need
     * to obtain the ICall object earlier, via this callback.
     */
    virtual void setCall(ICall* call)  = 0;
    virtual void onStateChange(uint8_t newState) {}
    virtual void onDestroy(TermCode reason, bool byPeer, const std::string& msg) = 0;
    virtual ISessionHandler* onNewSession(ISession& sess) { return nullptr; }
    virtual void onLocalStreamObtained(IVideoRenderer*& rendererOut) {}
    virtual void onLocalMediaError(const std::string errors) {}
    virtual void onRingOut(karere::Id peer) {}
    virtual void onCallStarting() {}
    virtual void onCallStarted() {}
};
class IGlobalHandler
{
public:
    /** @brief An incoming call has just been received
     * @param call The incoming call
     * @return The call handler that will receive events about this call
     */
    virtual ICallHandler* onCallIncoming(ICall& call, karere::AvFlags av) = 0;
};

class ISession: public karere::DeleteTrackable
{
protected:
    Call& mCall;
    karere::Id mSid;
    uint8_t mState = kStateInitial;
    bool mIsJoiner;
    karere::Id mPeer;
    karere::Id mPeerAnonId;
    uint32_t mPeerClient;
    karere::AvFlags mPeerAv;
    ISession(Call& call, karere::Id peer, uint32_t peerClient): mCall(call), mPeer(peer), mPeerClient(peerClient){}
public:
    enum: uint8_t
    {
        kStateInitial,
        kStateWaitSdpOffer,         // < Session just created, waiting for SDP offer from initiator
        kStateWaitLocalSdpAnswer,   // < Remote SDP offer has been set, and we are generating SDP answer
        kStateWaitSdpAnswer,        // < SDP offer has been sent by initiator, waiting for SDP answer
        kStateInProgress,
        kStateTerminating,          // < Session is in terminate handshake
        kStateDestroyed             // < Session object is not valid anymore
    };
    static const char* stateToStr(uint8_t state);
    const char* stateStr() const { return stateToStr(mState); }
    uint8_t getState() const { return mState; }
    bool isCaller() const { return !mIsJoiner; }
    Call& call() const { return mCall; }
    karere::Id peerAnonId() const { return mPeerAnonId; }
    karere::Id peer() const { return mPeer; }
    virtual bool isRelayed() const { return false; } //TODO: Implement
    karere::AvFlags receivedAv() const { return mPeerAv; }
    karere::Id sessionId() const {return mSid;}
};

class ICall: public karere::WeakReferenceable<ICall>
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
    RtcModule& mManager;
    chatd::Chat& mChat;
    karere::Id mId;
    uint8_t mState;
    bool mIsGroup;
    bool mIsJoiner;
    ICallHandler* mHandler;
    karere::Id mCallerUser;
    uint32_t mCallerClient;
    TermCode mTermCode;
    ICall(RtcModule& rtcModule, chatd::Chat& chat,
        karere::Id callid, bool isGroup, bool isJoiner, ICallHandler* handler,
        karere::Id callerUser, uint32_t callerClient)
    : WeakReferenceable<ICall>(this), mManager(rtcModule),
      mChat(chat), mId(callid), mIsGroup(isGroup), mIsJoiner(isJoiner),
      mHandler(handler), mCallerUser(callerUser), mCallerClient(callerClient), mTermCode(kNotFinished)
    {}
public:
    chatd::Chat& chat() const { return mChat; }
    RtcModule& manager() const { return mManager; }
    uint8_t state() const { return mState; }
    karere::Id id() const { return mId; }
    bool isCaller() const { return !mIsJoiner; }
    karere::Id caller() const { return mCallerUser; }
    uint32_t callerClient() const { return mCallerClient; }
    void changeHandler(ICallHandler* handler) { mHandler = handler; }
    TermCode termCode() const {return mTermCode; }
    bool isJoiner() { return mIsJoiner; }
    ICallHandler *callHandler() { return mHandler; }
    virtual karere::AvFlags sentAv() const = 0;
    virtual void hangup(TermCode reason=TermCode::kInvalid) = 0;
    virtual bool answer(karere::AvFlags av) = 0;
    virtual bool changeLocalRenderer(IVideoRenderer* renderer) = 0;
    virtual karere::AvFlags muteUnmute(karere::AvFlags av) = 0;
    virtual std::map<karere::Id, karere::AvFlags> avFlagsRemotePeers() const = 0;
    virtual std::map<karere::Id, uint8_t> sessionState() const = 0;
};
struct SdpKey
{
    uint8_t data[32];
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

/** @brief This is the public interface of the RtcModule */
class IRtcModule: public karere::DeleteTrackable
{
protected:
    IGlobalHandler& mHandler;
    std::unique_ptr<IRtcCrypto> mCrypto;
    karere::Id mOwnAnonId;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    IRtcModule(karere::Client& client, IGlobalHandler& handler, IRtcCrypto* crypto,
        karere::Id ownAnonId)
        : mHandler(handler), mCrypto(crypto), mOwnAnonId(ownAnonId), mClient(client) {}
public:
    virtual ~IRtcModule() {}
    karere::Client& mClient;

    /** @brief Default video encoding parameters. */
    VidEncParams vidEncParams;
    virtual void init() = 0;
    /**
     * @brief Clients exchange an anonymous id for statistics purposes
     * @note Currently, id is not anonymous, since signalling is done via chatd with actual userids
     */
    karere::Id ownAnonId() const { return mOwnAnonId; }

    /** @brief Returns a list of all detected audio input devices on the system */
    virtual void getAudioInDevices(std::vector<std::string>& devices) const = 0;

    /** @brief Returns a list of all detected video input devices on the system */
    virtual void getVideoInDevices(std::vector<std::string>& devices) const = 0;

    /** @brief Selects a video input device to be used for subsequent calls. This can be
     * changed just before a call is made, to allow different calls to use different
     * devices
     * @returns \c true if the specified device was successfully selected,
     * \c false if a device with that name does not exist or could not be selected
     */
    virtual bool selectVideoInDevice(const std::string& devname) = 0;

    /** @brief Selects an audio input device to be used for subsequent calls. This can be
     * changed just before a call is made, to allow different calls to use different
     * devices
     * @returns \c true if the specified device was successfully selected,
     * \c false if a device with that name does not exist or could not be selected
     */
    virtual bool selectAudioInDevice(const std::string& devname) = 0;

    /**
     * @brief Search all audio and video devices at system at that moment.
     */
    virtual void loadDeviceList() = 0;
    
    virtual bool isCaptureActive() const = 0;
    virtual void setMediaConstraint(const std::string& name, const std::string &value, bool optional=false) = 0;
    virtual void setPcConstraint(const std::string& name, const std::string &value, bool optional=false) = 0;
    virtual bool isCallInProgress() const = 0;
    virtual void removeCall(karere::Id chatid) = 0;

    virtual ICall& joinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler) = 0;
    virtual ICall& startCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler) = 0;
    virtual void hangupAll(TermCode reason) = 0;
};
IRtcModule* create(karere::Client& client, IGlobalHandler& handler,
    IRtcCrypto* crypto, const char* iceServers);

}

#endif
#endif

