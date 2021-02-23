#ifndef WEBRTC_H
#define WEBRTC_H

#include "IRtcCrypto.h"
#include "IVideoRenderer.h"
#include "karereId.h"
#include "karereCommon.h"
#include "sdkApi.h"
#include <net/websocketsIO.h>
#include "rtcCrypto.h"

#define TURNSERVER_SHARD -10    // shard number in the DNS cache for TURN servers
#define MAX_TURN_SERVERS 5      // max. number of TURN servers to be managed

namespace rtcModule
{
#ifdef KARERE_DISABLE_WEBRTC
class

{
};
#else

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
    kCallerGone = 8,
    kBusy = 9,                  // < Peer is in another call
    kStreamChange = 10,         // < Session was force closed by a client because it wants to change the media stream
    kNotFinished = 125,         // < It is no finished value, it is TermCode value while call is in progress
    kDestroyByCallCollision = 19,// < The call has finished by a call collision
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
    kErrPeerOffline = 33,       // < we received a notification that that user went offline
    kErrSessSetupTimeout = 34,  // < timed out waiting for session
    kErrSessRetryTimeout = 35,  // < timed out waiting for peer to retry a failed session
    kErrAlready = 36,           // < There is already a call in this chatroom
    kErrNotSupported = 37,      // < Clients that don't support calls send CALL_REQ_CANCEL with this code
    kErrCallSetupTimeout =  38, // < Timed out waiting for a connected session after the call was answered/joined
    kErrKickedFromChat = 39,    // < Call terminated because we were removed from the group chat
    kErrIceTimeout = 40,        // < Sesion setup timed out, because ICE stuck at the 'checking' stage
    kErrStreamRenegotation = 41,// < SDP error during stream renegotiation
    kErrStreamRenegotationTimeout = 42, // < Timed out waiting for completion of offer-answer exchange
    kErrorLast = 42,            // < Last enum indicating call termination due to error
    kLast = 42,                 // < Last call terminate enum value
    kPeer = 128,                // < If this flag is set, the condition specified by the code happened at the peer,
                                // < not at our side
    kInvalid = 0x7f
};

enum CallState: uint8_t
{
    kStateInitial,      // < Call object was initialised
    kStateUserNoParticipating,  // < User is not particpating in the call
    kStateConnecting,   // < Connecting to SFU
    kStateJoining,      // < Joining a call
    kStateInProgress,
    kStateTerminatingUserParticipation, // < Call is waiting for sessions to terminate
    kStateDestroyed    // < Call object is not valid anymore, the call is removed from the system
};

class ISession;
class SessionHandler
{
public:
    virtual void onSpeakRequest(ISession& session, bool requested) = 0;
    virtual void onVThumbReceived(ISession& session) = 0;
    virtual void onHiResReceived(ISession& session) = 0;
};

class ISession
{
public:
    virtual ~ISession(){}
    virtual karere::Id getPeerid() const = 0;
    virtual Cid_t getClientid() const = 0;
    virtual karere::AvFlags getAvFlags() const = 0;
    virtual void setSessionHandler(SessionHandler* sessionHandler) = 0;
    virtual void setVideoRendererVthumb(IVideoRenderer *videoRederer) = 0;
    virtual void setVideoRendererHiRes(IVideoRenderer *videoRederer) = 0;
};

class ICall;
class CallHandler
{
public:
    virtual ~CallHandler(){}
    virtual void onCallStateChange(ICall& call) = 0;
    virtual void onCallRinging(ICall& call) = 0;
    virtual void onNewSession(ISession& session, const ICall& call) = 0;
    virtual void onRemoteAvFlagsChange(ISession& session, const ICall& call) = 0;
};

class ICall
{
public:
    virtual karere::Id getCallid() const = 0;
    virtual karere::Id getChatid() const = 0;
    virtual karere::Id getCallerid() const = 0;
    virtual CallState getState() const = 0;
    virtual void addParticipant(karere::Id peer) = 0;
    virtual void removeParticipant(karere::Id peer) = 0;
    virtual void hangup() = 0;
    virtual promise::Promise<void> join(bool moderator) = 0;
    virtual bool participate() = 0;
    virtual void enableAudioLevelMonitor(bool enable) = 0;
    virtual void ignoreCall() = 0;
    virtual void setRinging(bool ringing) = 0;
    virtual bool isRinging() const = 0;

    virtual void setCallerId(karere::Id callerid) = 0;
    virtual bool isModerator() const = 0;
    virtual void requestModerator() = 0;
    virtual void requestSpeaker(bool add = true) = 0;
    virtual bool isSpeakAllow() = 0;
    virtual void approveSpeakRequest(Cid_t cid, bool allow) = 0;
    virtual void stopSpeak(Cid_t cid = 0) = 0;
    virtual std::vector<Cid_t> getSpeakerRequested() = 0;
    virtual void requestHighResolutionVideo(Cid_t cid) = 0;
    virtual void stopHighResolutionVideo(Cid_t cid) = 0;
    virtual void requestLowResolutionVideo(const std::vector<karere::Id> &cids) = 0;
    virtual void stopLowResolutionVideo(const std::vector<karere::Id> &cids) = 0;

    virtual void setCallHandler(CallHandler* callHanlder) = 0;
    virtual void setVideoRendererVthumb(IVideoRenderer *videoRederer) = 0;
    virtual void setVideoRendererHiRes(IVideoRenderer *videoRederer) = 0;
    virtual karere::AvFlags getLocalAvFlags() const = 0;
    virtual void updateAndSendLocalAvFlags(karere::AvFlags flags) = 0;
};

class RtcModule
{
public:
    virtual void init(WebsocketsIO& websocketIO, void *appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings, const karere::Id &myHandle) = 0;
    virtual void hangupAll() = 0;
    virtual ICall* findCall(karere::Id callid) = 0;
    virtual ICall* findCallByChatid(karere::Id chatid) = 0;
    virtual void loadDeviceList() = 0;
    virtual bool selectVideoInDevice(const std::string& device) = 0;
    virtual void getVideoInDevices(std::set<std::string>& devicesVector) = 0;
    virtual std::string getVideoDeviceSelected() = 0;
    virtual promise::Promise<void> startCall(karere::Id chatid) = 0;

    virtual std::vector<karere::Id> chatsWithCall() = 0;
    virtual unsigned int getNumCalls() = 0;

    virtual void removeCall(karere::Id chatid, TermCode termCode = kUserHangup) = 0;

    virtual void handleJoinedCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersJoined) = 0;
    virtual void handleLefCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersLeft) = 0;
    virtual void handleCallEnd(karere::Id chatid, karere::Id callid, uint8_t reason) = 0;
    virtual void handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging) = 0;
};


void globalCleanup();


static const uint8_t kNetworkQualityDefault = 2;    // By default, while not enough samples

class IGlobalCallHandler
{
public:
    virtual ~IGlobalCallHandler(){}
    virtual void onNewCall(ICall& call) = 0;
    virtual void onAddPeer(ICall& call, karere::Id peer) = 0;
    virtual void onRemovePeer(ICall& call, karere::Id peer) = 0;
    virtual void onEndCall(ICall& call) = 0;
};

RtcModule* createRtcModule(MyMegaApi& megaApi, IGlobalCallHandler &callhandler, IRtcCrypto* crypto, const char* iceServers);

enum RtcConstant {
   kMaxCallReceivers = 20,
   kMaxCallAudioSenders = 20,
   kMaxCallVideoSenders = 6,
   kHiResWidth = 960,
   kHiResHeight = 540,
   kHiResMaxFPS = 30,
   kVthumbWidth = 160,
};

#endif

}


#endif // WEBRTC_H
