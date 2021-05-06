#ifndef WEBRTC_H
#define WEBRTC_H

#include "IRtcCrypto.h"
#include "IVideoRenderer.h"
#include "karereId.h"
#include "karereCommon.h"
#include "sdkApi.h"
#include <net/websocketsIO.h>
#include "rtcCrypto.h"
#include "sfu.h"

#define TURNSERVER_SHARD -10    // shard number in the DNS cache for TURN servers
#define MAX_TURN_SERVERS 5      // max. number of TURN servers to be managed

#define RET_ENUM_NAME(name) case name: return #name

namespace rtcModule
{
#ifdef KARERE_DISABLE_WEBRTC

#else

enum TermCode: uint8_t
{
    kInvalidTermCode = 255,
    kUserHangup = 0,            // < Normal user hangup
    kErrSdp = 32,               // < error generating or setting SDP description
    kRtcDisconn = 64,
    kSigDisconn = 65,
    kSvrShuttingDown = 66,      // < Server is shutting down
    kErrSignaling = 128,
    kErrNoCall = 129,           // < Attempted to join non-existing call
    kUnKnownTermCode = 130,
};

enum CallState: uint8_t
{
    kStateInitial = 0,      // < Call object was initialised
    kStateClientNoParticipating,  // < User is not particpating in the call
    kStateConnecting,   // < Connecting to SFU
    kStateJoining,      // < Joining a call
    kStateInProgress,
    kStateTerminatingUserParticipation, // < Call is waiting for sessions to terminate
    kStateDestroyed    // < Call object is not valid anymore, the call is removed from the system
};

enum SessionState: uint8_t
{
    kSessStateInProgress = 0,
    kSessStateDestroyed    // < Call object is not valid anymore, the call is removed from the system
};

enum CallQuality
{
    kCallQualityHighDef = 0,        // Default hi-res quality
    kCallQualityHighMedium = 1,     // 1 layer lower  (2x lower resolution)
    kCallQualityHighLow = 2,        // 2 layers lower (4x lower resolution)
};

class ISession;
class SessionHandler
{
public:
    virtual ~SessionHandler(){}
    virtual void onSpeakRequest(ISession& session, bool requested) = 0;
    virtual void onVThumbReceived(ISession& session) = 0;
    virtual void onHiResReceived(ISession& session) = 0;
    virtual void onDestroySession(ISession& session) = 0;
    virtual void onAudioRequested(ISession& session) = 0;
    virtual void onRemoteFlagsChanged(ISession& session) = 0;
    virtual void onOnHold(ISession& session) = 0;
    virtual void onRemoteAudioDetected(ISession& session) = 0;
};

class ISession
{
public:
    virtual ~ISession(){}
    virtual karere::Id getPeerid() const = 0;
    virtual Cid_t getClientid() const = 0;
    virtual karere::AvFlags getAvFlags() const = 0;
    virtual SessionState getState() const = 0;
    virtual bool isAudioDetected() const = 0;
    virtual bool hasRequestSpeak() const = 0;
    virtual void setSessionHandler(SessionHandler* sessionHandler) = 0;
    virtual void setVideoRendererVthumb(IVideoRenderer *videoRederer) = 0;
    virtual void setVideoRendererHiRes(IVideoRenderer *videoRederer) = 0;
    virtual void setAudioDetected(bool audioDetected) = 0;
    virtual bool hasHighResolutionTrack() const = 0;
    virtual bool hasLowResolutionTrack() const = 0;
    virtual void notifyHiResReceived() = 0;
    virtual void notifyLowResReceived() = 0;
    virtual void disableVideoSlot(bool hires) = 0;
};

class ICall;
class CallHandler
{
public:
    virtual ~CallHandler(){}
    virtual void onCallStateChange(ICall& call) = 0;
    virtual void onCallRinging(ICall& call) = 0;
    virtual void onNewSession(ISession& session, const ICall& call) = 0;
    virtual void onAudioApproved(const ICall& call) = 0;
    virtual void onLocalFlagsChanged(const ICall& call) = 0;
    virtual void onLocalAudioDetected(const ICall& call) = 0;
    virtual void onOnHold(const ICall& call) = 0;
};

class ICall
{
public:
    virtual karere::Id getCallid() const = 0;
    virtual karere::Id getChatid() const = 0;
    virtual karere::Id getCallerid() const = 0;
    virtual bool isAudioDetected() const = 0;
    virtual CallState getState() const = 0;
    virtual void addParticipant(karere::Id peer) = 0;
    virtual void removeParticipant(karere::Id peer) = 0;
    virtual promise::Promise<void> hangup() = 0;
    virtual promise::Promise<void> endCall() = 0;
    virtual promise::Promise<void> join(karere::AvFlags avFlags) = 0;
    virtual bool participate() = 0;
    virtual void enableAudioLevelMonitor(bool enable) = 0;
    virtual void ignoreCall() = 0;
    virtual void setRinging(bool ringing) = 0;
    virtual void setOnHold() = 0;
    virtual void releaseOnHold() = 0;
    virtual bool isRinging() const = 0;
    virtual bool isIgnored() const = 0;
    virtual bool isAudioLevelMonitorEnabled() const = 0;
    virtual bool hasVideoSlot(Cid_t cid, bool highRes = true) const = 0;
    virtual int getNetworkQuality() const = 0;
    virtual bool hasRequestSpeak() const = 0;
    virtual TermCode getTermCode() const = 0;

    virtual void setCallerId(karere::Id callerid) = 0;
    virtual void requestSpeaker(bool add = true) = 0;
    virtual bool isSpeakAllow() const = 0;
    virtual void approveSpeakRequest(Cid_t cid, bool allow) = 0;
    virtual void stopSpeak(Cid_t cid = 0) = 0;
    virtual std::vector<Cid_t> getSpeakerRequested() = 0;
    virtual void requestHighResolutionVideo(Cid_t cid) = 0;
    virtual void requestHiResQuality(Cid_t cid, int quality) = 0;
    virtual void stopHighResolutionVideo(Cid_t cid) = 0;
    virtual void requestLowResolutionVideo(std::vector<Cid_t> &cids) = 0;
    virtual void stopLowResolutionVideo(std::vector<Cid_t> &cids) = 0;

    virtual std::vector<karere::Id> getParticipants() const = 0;
    virtual std::vector<Cid_t> getSessionsCids() const = 0;
    virtual ISession* getIsession(Cid_t cid) const = 0;
    virtual bool isOutgoing() const = 0;
    virtual int64_t getInitialTimeStamp() const = 0;
    virtual int64_t getFinalTimeStamp() const = 0;

    virtual void setCallHandler(CallHandler* callHanlder) = 0;
    virtual karere::AvFlags getLocalAvFlags() const = 0;
    virtual void updateAndSendLocalAvFlags(karere::AvFlags flags) = 0;
    virtual void setAudioDetected(bool audioDetected) = 0;
};

class RtcModule
{
public:
    virtual ~RtcModule(){};
    virtual void init(WebsocketsIO& websocketIO, void *appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings, const karere::Id &myHandle) = 0;
    virtual ICall* findCall(karere::Id callid) = 0;
    virtual ICall* findCallByChatid(karere::Id chatid) = 0;
    virtual bool selectVideoInDevice(const std::string& device) = 0;
    virtual void getVideoInDevices(std::set<std::string>& devicesVector) = 0;
    virtual promise::Promise<void> startCall(karere::Id chatid, karere::AvFlags avFlags, bool isGroup, std::shared_ptr<std::string> unifiedKey = nullptr) = 0;
    virtual void takeDevice() = 0;
    virtual void releaseDevice() = 0;
    virtual void addLocalVideoRenderer(karere::Id chatid, IVideoRenderer *videoRederer) = 0;
    virtual void removeLocalVideoRenderer(karere::Id chatid) = 0;

    virtual std::vector<karere::Id> chatsWithCall() = 0;
    virtual unsigned int getNumCalls() = 0;
    virtual const std::string& getVideoDeviceSelected() const = 0;
    virtual sfu::SfuClient& getSfuClient() = 0;

    virtual void removeCall(karere::Id chatid, TermCode termCode = kUserHangup) = 0;

    virtual void handleJoinedCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersJoined) = 0;
    virtual void handleLeftCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersLeft) = 0;
    virtual void handleCallEnd(karere::Id chatid, karere::Id callid, uint8_t reason) = 0;
    virtual void handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey = nullptr) = 0;
};


void globalCleanup();


static const uint8_t kNetworkQualityDefault = 2;    // By default, while not enough samples
static const int kAudioThreshold = 100;             // Threshold to consider a user is speaking

class IGlobalCallHandler
{
public:
    virtual ~IGlobalCallHandler(){}
    virtual void onNewCall(ICall& call) = 0;
    virtual void onAddPeer(ICall& call, karere::Id peer) = 0;
    virtual void onRemovePeer(ICall& call, karere::Id peer) = 0;
    virtual void onEndCall(ICall& call) = 0;
};

RtcModule* createRtcModule(MyMegaApi& megaApi, IGlobalCallHandler &callhandler);

enum RtcConstant {
   kMaxCallReceivers = 20,
   kMaxCallAudioSenders = 20,
   kMaxCallVideoSenders = 6,
   kInitialvthumbCount = 10,
   kHiResWidth = 960,
   kHiResHeight = 540,
   kHiResMaxFPS = 30,
   kVthumbWidth = 160,
};

#endif

}


#endif // WEBRTC_H
