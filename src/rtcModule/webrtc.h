#ifndef KARERE_DISABLE_WEBRTC
#ifndef WEBRTC_H
#define WEBRTC_H
#include "karereId.h"
#include "karereCommon.h"
#include "sdkApi.h"
#include <net/websocketsIO.h>
#include <mega/utils.h>
#include "IVideoRenderer.h"
#include "IRtcCrypto.h"
#include "rtcCrypto.h"
#include "sfu.h"


#define RET_ENUM_RTC_NAME(name) case name: return #name

namespace rtcModule
{
#ifdef KARERE_DISABLE_WEBRTC

#else
enum TermCode: uint8_t
{
    kFlagDisconn                = 64,
    kFlagError                  = 128,
    kFlagMaxValid               = kFlagError | 63,      // < Current max valid value => kErrGeneral

    //==============================================================================================

    kUserHangup                 = 0,                    // < normal user hangup
    kTooManyParticipants        = 1,                    // < there are too many participants
    kLeavingRoom                = 2,                    // < user has been removed from chatroom
    kCallEndedByModerator       = 3,                    // < group or meeting call has been ended by moderator
    kApiEndCall                 = 4,                    // < API/chatd ended call
    kPeerJoinTimeout            = 5,                    // < Nobody joined call
    kPushedToWaitingRoom        = 6,                    // < Our client has been removed from the call and pushed back into the waiting room
    kKickedFromWaitingRoom      = 7,                    // < Revokes the join permission for our user that is into the waiting room
    kTooManyUserClients         = 8,                    // < Too many clients of same user connected

    //==============================================================================================

    kRtcDisconn                 = kFlagDisconn | 0,     // 64 < SFU connection failed
    kSigDisconn                 = kFlagDisconn | 1,     // 65 < socket error on the signalling connection
    kSfuShuttingDown            = kFlagDisconn | 2,     // 66 < SFU server is shutting down
    kChatDisconn                = kFlagDisconn | 3,     // 67 < chatd connection is broken
    kNoMediaPath                = kFlagDisconn | 4,     // 68 < webRTC connection failed, no UDP connectivity
    //==============================================================================================

    kErrSignaling               = kFlagError | 0,       // 128 < signalling error | if client doesn't supports waiting rooms
    kErrNoCall                  = kFlagError | 1,       // 129 < attempted to join non-existing call
    kErrAuth                    = kFlagError | 2,       // 130 < authentication error
    kErrApiTimeout              = kFlagError | 3,       // 131 < ping timeout between SFU and API
    kErrSdp                     = kFlagError | 4,       // 132 < error generating or setting SDP description
    kErrorProtocolVersion       = kFlagError | 5,       // 133 < SFU protocol version not supported
    kErrorCrypto                = kFlagError | 6,       // 134 < Cryptographic error
    kErrClientGeneral           = kFlagError | 62,      // 190 < Client general error
    kErrGeneral                 = kFlagError | 63,      // 191 < SFU general error
    kUnKnownTermCode            = kFlagError | 126,     // 254 < unknown error
    kInvalidTermCode            = kFlagError | 127,     // 255 < invalid connection termcode
};

enum CallState: uint8_t
{
    kStateInitial = 0,                  // < Call object was initialised
    kStateClientNoParticipating,        // < User is not partipating in the call
    kStateConnecting,                   // < Connecting to SFU
    kInWaitingRoom,                     // < In a waiting room
    kStateJoining,                      // < Joining a call
    kStateInProgress,                   // < Call is joined (upon ANSWER)
    kStateTerminatingUserParticipation, // < Call is waiting for sessions to terminate
    kStateDestroyed,                    // < Call object is not valid anymore, the call is removed from the system
    kStateUninitialized,                // < Call object is uninitialized
};

enum EndCallReason: uint8_t
{
    kEnded          = 1,   /// normal hangup of on-going call
    kRejected       = 2,   /// incoming call was rejected by callee
    kNoAnswer       = 3,   /// outgoing call didn't receive any answer from the callee
    kFailed         = 4,   /// on-going call failed
    kCancelled      = 5,   /// outgoing call was cancelled by caller before receiving any answer from the callee
    kEndedByMod     = 6,   /// group or meeting call has been ended by moderator
    kInvalidReason  = 255, /// invalid endcall reason
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

enum VideoResolution
{
    kUndefined  = kUndefinedTrack,
    kLowRes     = kVthumbTrack,
    kHiRes      = kHiResTrack,
};

enum TrackDirection
{
    kSend = 0,
    kRecv = 1,
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
    virtual void onPermissionsChanged(ISession& session) = 0;
};

class ISession
{
public:
    virtual ~ISession(){}
    virtual const karere::Id& getPeerid() const = 0;
    virtual Cid_t getClientid() const = 0;
    virtual karere::AvFlags getAvFlags() const = 0;
    virtual SessionState getState() const = 0;
    virtual bool isAudioDetected() const = 0;
    virtual bool hasRequestSpeak() const = 0;
    virtual TermCode getTermcode() const = 0;
    virtual void setTermcode(TermCode termcode) = 0;
    virtual void setSessionHandler(SessionHandler* sessionHandler) = 0;
    virtual void setVideoRendererVthumb(IVideoRenderer *videoRederer) = 0;
    virtual void setVideoRendererHiRes(IVideoRenderer *videoRederer) = 0;
    virtual bool hasHighResolutionTrack() const = 0;
    virtual bool hasLowResolutionTrack() const = 0;
    virtual bool isModerator() const = 0;
};

class ICall;
class CallHandler
{
public:
    virtual ~CallHandler(){}
    virtual void onCallStateChange(ICall& call) = 0;
    virtual void onCallError(rtcModule::ICall &call, int code, const std::string &errMsg) = 0;
    virtual void onCallRinging(ICall& call) = 0;
    virtual void onNewSession(ISession& session, const ICall& call) = 0;
    virtual void onAudioApproved(const ICall& call) = 0;
    virtual void onLocalFlagsChanged(const ICall& call) = 0;
    virtual void onOnHold(const ICall& call) = 0;
    virtual void onAddPeer(const ICall &call, karere::Id peer) = 0;
    virtual void onRemovePeer(const ICall &call,  karere::Id peer) = 0;
    virtual void onNetworkQualityChanged(const rtcModule::ICall &call) = 0;
    virtual void onStopOutgoingRinging(const ICall& call) = 0;
    virtual void onPermissionsChanged(const ICall& call) = 0;
    virtual void onWrUsersAllow(const rtcModule::ICall& call, const ::mega::MegaHandleList* user) = 0;
    virtual void onWrUsersDeny(const rtcModule::ICall& call, const ::mega::MegaHandleList* user) = 0;
    virtual void onWrUserDump(const rtcModule::ICall& call) = 0;
    virtual void onWrAllow(const rtcModule::ICall& call) = 0;
    virtual void onWrDeny(const rtcModule::ICall& call) = 0;
    virtual void onWrUsersEntered(const rtcModule::ICall& call, const mega::MegaHandleList* users) = 0;
    virtual void onWrUsersLeave(const rtcModule::ICall& call, const mega::MegaHandleList* users) = 0;
    virtual void onWrPushedFromCall(const rtcModule::ICall& call) = 0;
    virtual void onCallDeny(const rtcModule::ICall& call, const std::string& cmd, const std::string& msg) = 0;
};

class KarereWaitingRoom;
class ICall
{
public:
    virtual karere::Id getCallid() const = 0;
    virtual karere::Id getChatid() const = 0;
    virtual karere::Id getCallerid() const = 0;
    virtual CallState getState() const = 0;
    virtual bool isOwnClientCaller() const = 0;
    virtual bool isJoined() const = 0;
    virtual bool isOwnPrivModerator() const = 0;

    virtual void addParticipant(const karere::Id &peer) = 0;
    virtual void joinedCallUpdateParticipants(const std::set<karere::Id> &usersJoined) = 0;
    virtual void removeParticipant(const karere::Id &peer) = 0;

    // called by chatd client when the connection to chatd is closed
    virtual void onDisconnectFromChatd() = 0;
    virtual void reconnectToSfu() = 0;

    virtual promise::Promise<void> hangup() = 0;
    virtual promise::Promise<void> endCall() = 0;  // only used on 1on1 when incoming call is rejected or moderator in group call to finish it for all participants
    virtual promise::Promise<void> join(karere::AvFlags avFlags) = 0;

    virtual bool participate() = 0;
    virtual bool isJoining() const = 0;
    virtual void enableAudioLevelMonitor(bool enable) = 0;
    virtual void ignoreCall() = 0;
    virtual void setRinging(bool ringing) = 0;
    virtual void stopOutgoingRinging() = 0;
    virtual void setOnHold() = 0;
    virtual void releaseOnHold() = 0;
    virtual bool isRinging() const = 0;
    virtual bool isOutgoingRinging() const = 0;
    virtual bool isIgnored() const = 0;
    virtual bool isAudioLevelMonitorEnabled() const = 0;
    virtual bool hasVideoSlot(Cid_t cid, bool highRes = true) const = 0;
    virtual int getNetworkQuality() const = 0;
    virtual bool hasRequestSpeak() const = 0;
    virtual int getWrJoiningState() const = 0;
    virtual TermCode getTermCode() const = 0;
    virtual uint8_t getEndCallReason() const = 0;

    virtual void setCallerId(const karere::Id &callerid) = 0;
    virtual bool alreadyParticipating() = 0;
    virtual void requestSpeaker(bool add = true) = 0;
    virtual bool isSpeakAllow() const = 0;
    virtual void approveSpeakRequest(Cid_t cid, bool allow) = 0;
    virtual void stopSpeak(Cid_t cid = 0) = 0;
    virtual void pushUsersIntoWaitingRoom(const std::set<karere::Id>& users, const bool all) const = 0;
    virtual void allowUsersJoinCall(const std::set<karere::Id>& users, const bool all) const = 0;
    virtual void kickUsersFromCall(const std::set<karere::Id>& users) const = 0;
    virtual std::vector<Cid_t> getSpeakerRequested() = 0;
    virtual void requestHighResolutionVideo(Cid_t cid, int quality) = 0;
    virtual void requestHiResQuality(Cid_t cid, int quality) = 0;
    virtual void stopHighResolutionVideo(std::vector<Cid_t> &cids) = 0;
    virtual void requestLowResolutionVideo(std::vector<Cid_t> &cids) = 0;
    virtual void stopLowResolutionVideo(std::vector<Cid_t> &cids) = 0;

    virtual std::set<karere::Id> getParticipants() const = 0;
    virtual std::set<karere::Id> getModerators() const = 0;
    virtual std::vector<Cid_t> getSessionsCids() const = 0;
    virtual ISession* getIsession(Cid_t cid) const = 0;
    virtual bool isOutgoing() const = 0;
    virtual int64_t getInitialTimeStamp() const = 0;
    virtual int64_t getFinalTimeStamp() const = 0;
    virtual int64_t getInitialOffsetinMs() const = 0;
    virtual karere::AvFlags getLocalAvFlags() const = 0;
    virtual void updateAndSendLocalAvFlags(karere::AvFlags flags) = 0;
    virtual const KarereWaitingRoom* getWaitingRoom() const = 0;
    virtual bool isAllowSpeak() const = 0;
};

class RtcModule
{
public:
    virtual ~RtcModule(){};
    virtual ICall* findCall(const karere::Id &callid) const = 0;
    virtual ICall* findCallByChatid(const karere::Id &chatid) const = 0;
    virtual bool isCallStartInProgress(const karere::Id &chatid) const = 0;
    virtual bool selectVideoInDevice(const std::string& device) = 0;
    virtual void getVideoInDevices(std::set<std::string>& devicesVector) = 0;
    virtual promise::Promise<void> startCall(const karere::Id &chatid, karere::AvFlags avFlags, bool isGroup, const karere::Id &schedId, std::shared_ptr<std::string> unifiedKey = nullptr) = 0;
    virtual void takeDevice() = 0;
    virtual void releaseDevice() = 0;
    virtual void addLocalVideoRenderer(const karere::Id &chatid, IVideoRenderer *videoRederer) = 0;
    virtual void removeLocalVideoRenderer(const karere::Id &chatid) = 0;

    virtual std::vector<karere::Id> chatsWithCall() = 0;
    virtual unsigned int getNumCalls() = 0;
    virtual const std::string& getVideoDeviceSelected() const = 0;
    virtual sfu::SfuClient& getSfuClient() = 0;
    virtual DNScache& getDnsCache() = 0;

    virtual void orderedDisconnectAndCallRemove(rtcModule::ICall* iCall, EndCallReason reason, TermCode connectionTermCode) = 0;

    virtual void handleJoinedCall(const karere::Id &chatid, const karere::Id &callid, const std::set<karere::Id>& usersJoined) = 0;
    virtual void handleLeftCall(const karere::Id &chatid, const karere::Id &callid, const std::set<karere::Id>& usersLeft) = 0;
    virtual void handleNewCall(const karere::Id &chatid, const karere::Id &callerid, const karere::Id &callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey = nullptr) = 0;
};


enum class WrState: int
{
    WR_UNKNOWN      = -1,   // client unknown joining status
    WR_NOT_ALLOWED  = 0,    // client is not allowed to join call (must remains in waiting room)
    WR_ALLOWED      = 1,    // client is allowed to join call (needs to send JOIN command to SFU)
};

static bool isValidWrStatus(const WrState& value)
{
    return (value > WrState::WR_UNKNOWN && value <= WrState::WR_ALLOWED);
}

/**
 * @brief This class represents waiting room users
 *
 * A waiting room, is effectively a list of users pending to enter a call
 */
class KarereWaitingRoom
{
public:
    ~KarereWaitingRoom() = default;
    KarereWaitingRoom() = default;
    KarereWaitingRoom(const KarereWaitingRoom& other) = default;
    KarereWaitingRoom(KarereWaitingRoom&& other) = delete;
    KarereWaitingRoom& operator = (const KarereWaitingRoom& other) = delete;
    KarereWaitingRoom& operator = (KarereWaitingRoom&& other) = delete;

    void clear() { mWaitingRoomUsers.clear(); }

    bool addOrUpdateUserStatus(const uint64_t& userid, const int& status)
    {
        if (!isValidWrStatus(static_cast<WrState>(status)))
        {
            assert(false);
            return false;
        }

        mWaitingRoomUsers[userid] = static_cast<WrState>(status);
        return true;
    }

    bool removeUser(const uint64_t& userid)
    {
        return mWaitingRoomUsers.erase(userid);
    }

    bool updateUsers(const std::set<karere::Id>& users, const WrState& status)
    {
        if (!isValidWrStatus(status) || users.empty())
        {
            return false;
        }

        std::for_each(users.begin(), users.end(), [this, &status](const auto &u)
        {
            mWaitingRoomUsers[u.val] = static_cast<WrState>(status);
        });

        return true;
    }

    size_t size() const { return mWaitingRoomUsers.size(); }

    std::vector<uint64_t> getPeers() const
    {
        std::vector<uint64_t> keys;
        keys.reserve(mWaitingRoomUsers.size());
        std::transform(mWaitingRoomUsers.begin(), mWaitingRoomUsers.end(),
                       std::back_inserter(keys), [](const auto& pair) { return pair.first; });

        return keys;
    }

    int getPeerStatus(const uint64_t& peerid) const
    {
        const auto& it = mWaitingRoomUsers.find(peerid);
        if (it == mWaitingRoomUsers.end())
        {
            return static_cast<int>(WrState::WR_UNKNOWN);
        }

        return static_cast<int>(it->second);
    }

private:
    std::map<uint64_t, WrState> mWaitingRoomUsers;
};


void globalCleanup();

typedef enum
{
    kNetworkQualityBad          = 0,    // Bad network quality detected
    kNetworkQualityGood         = 1,    // Good network quality detected
} netWorkQuality;

static const int kAudioThreshold = 100;             // Threshold to consider a user is speaking

RtcModule* createRtcModule(MyMegaApi& megaApi, CallHandler &callhandler, DNScache &dnsCache,
                           WebsocketsIO& websocketIO, void *appCtx,
                           rtcModule::RtcCryptoMeetings* rRtcCryptoMeetings);
enum RtcConstant {
   kMaxCallReceivers = 20,      // should be inline with webclient value
   kMaxCallAudioSenders = 20,   // should be inline with webclient value
   kMaxCallVideoSenders = 24,   // should be inline with webclient value
   kInitialvthumbCount = 0,     // maximum amount of video streams to receive after joining SFU, by default we won't request any vthumb track
   kHiResWidth = 960,  // px
   kHiResHeight = 540,  // px
   kHiResMaxFPS = 30,
   kVthumbWidth = 160,  // px
   kAudioMonitorTimeout = 2000, // ms
   kStatsInterval = 1000,   // ms
   kTxSpatialLayerCount = 3,
   kRotateKeyUseDelay = 100, // ms
};

#endif

}


#endif // WEBRTC_H
#endif
