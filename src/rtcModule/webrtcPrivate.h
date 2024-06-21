#ifndef WEBRTCSFU_H
#define WEBRTCSFU_H

#include <logger.h>
#include <rtcModule/webrtcAdapter.h>
#include <rtcModule/webrtc.h>
#include <rtcModule/rtcStats.h>
#include <sfu.h>
#include <IVideoRenderer.h>

#include <map>
#include <variant>

namespace rtcModule
{
#ifndef KARERE_DISABLE_WEBRTC
class RtcModuleSfu;
class Call;
class Session;

class AudioLevelMonitor : public webrtc::AudioTrackSinkInterface, public karere::DeleteTrackable
{
    public:
    AudioLevelMonitor(Call &call, void* appCtx, int32_t cid = -1);
    void OnData(const void *audio_data,
                        int bits_per_sample,
                        int sample_rate,
                        size_t number_of_channels,
                        size_t number_of_frames, absl::optional<int64_t> absolute_capture_timestamp_ms) override;
    bool hasAudio();
    void onAudioDetected(bool audioDetected);

private:
    time_t mPreviousTime = 0;
    Call &mCall;
    bool mAudioDetected = false;

    // Note that currently max CID allowed by this class is 65535
    int32_t mCid;
    void* mAppCtx;
};

/**
 * This class represent a generic instance to manage webrtc Transceiver
 * A Transceiver is an element used to send or receive datas
 */
class Slot
{
public:
    virtual ~Slot();
    webrtc::RtpTransceiverInterface* getTransceiver() { return mTransceiver.get(); }
    IvStatic_t getIv() const { return mIv; }
    uint32_t getTransceiverMid() const;
protected:
    Call &mCall;
    IvStatic_t mIv = 0;
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> mTransceiver;

    Slot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
};

/**
 * This class represent a webrtc transceiver for local audio and low resolution video
 */
class LocalSlot : public Slot
{
public:
    LocalSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void createEncryptor();
    void generateRandomIv();
};

/**
 * This class represent a generic instance to manage remote webrtc Transceiver
 */
class RemoteSlot : public Slot
{
public:
    virtual ~RemoteSlot() {}
    virtual void createDecryptor(Cid_t cid, IvStatic_t iv);
    virtual void release();
    Cid_t getCid() const { return mCid; }
    Cid_t getAuxCid()const { return mAuxCid; }
    void setAuxCid(const Cid_t cid) { mAuxCid = cid; }

protected:
    Cid_t mCid = K_INVALID_CID;
    Cid_t mAuxCid = K_INVALID_CID;
    void* mAppCtx;
    RemoteSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx);
    void assign(Cid_t cid, IvStatic_t iv);

private:
    void enableTrack(bool enable, TrackDirection direction);
};

class VideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame>, public karere::DeleteTrackable
{
public:
    enum
    {
        Rtc_Type_Video_source_Unknown          = -1,
        Rtc_Type_Video_source_Local_Camera     = 0,
        Rtc_Type_Video_source_Local_Screen     = 1,
        Rtc_Type_Video_source_Remote           = 2,
    };

    VideoSink(void* appCtx);
    virtual ~VideoSink();
    void setVideoRender(IVideoRenderer* videoRenderer);
    static void processFrame(const webrtc::VideoFrame& frame,
                             const std::unique_ptr<IVideoRenderer>& render,
                             const int sourceType);
    virtual void OnFrame(const webrtc::VideoFrame& frame) override;
private:
    std::unique_ptr<IVideoRenderer> mRenderer;
    void* mAppCtx;
};

/**
 * This class represent a generic instance to manage removte video webrtc Transceiver
 */
class RemoteVideoSlot : public RemoteSlot, public VideoSink
{
public:
    RemoteVideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx);
    ~RemoteVideoSlot();
    void enableTrack();
    void assignVideoSlot(Cid_t cid, IvStatic_t iv, VideoResolution videoResolution);
    void release() override;
    VideoResolution getVideoResolution() const;
    bool hasTrack();

private:
    VideoResolution mVideoResolution = kUndefined;
};

/**
 * This class represent a generic instance to manage remote audio webrtc Transceiver
 */
class RemoteAudioSlot : public RemoteSlot
{
public:
    RemoteAudioSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx);
    void assignAudioSlot(Cid_t cid, IvStatic_t iv);
    bool enableAudioMonitor(const bool enable);
    void createDecryptor(Cid_t cid, IvStatic_t iv) override;
    void release() override;

private:
    std::unique_ptr<AudioLevelMonitor> mAudioLevelMonitor;
    bool mAudioLevelMonitorEnabled = false;
};

/**
 * @brief The Session class
 *
 * A session is used to manage the slots available for a peer
 * in a all. It implements the ISession interface, and provides
 * callbacks through the SessionHandler.
 *
 * The Session itself is created right before the CallHandler::onNewSession()
 */
class Session : public ISession
{
public:
    Session(const sfu::Peer& peer);
    ~Session();

    const sfu::Peer &getPeer() const;
    void setVThumSlot(RemoteVideoSlot* slot);
    void setHiResSlot(RemoteVideoSlot* slot);
    void setAudioSlot(RemoteAudioSlot *slot);
    void addKey(Keyid_t keyid, const std::string& key);
    void setRemoteAvFlags(karere::AvFlags flags);

    RemoteAudioSlot* getAudioSlot();
    RemoteVideoSlot* getVthumSlot();
    RemoteVideoSlot* getHiResSlot();

    void disableAudioSlot();
    void setAudioDetected(bool audioDetected);    
    void notifyHiResReceived();
    void notifyLowResReceived();
    void disableVideoSlot(VideoResolution videoResolution);
    void setModerator(bool isModerator);

    // ISession methods (called from intermediate layer, upon SessionHandler callbacks and others)
    const karere::Id& getPeerid() const override;
    Cid_t getClientid() const override;
    SessionState getState() const override;
    karere::AvFlags getAvFlags() const override;
    bool isAudioDetected() const override;
    TermCode getTermcode() const override;
    void setTermcode(TermCode termcode) override;
    void setSessionHandler(SessionHandler* sessionHandler) override;
    void setVideoRendererVthumb(IVideoRenderer *videoRenderer) override;
    void setVideoRendererHiRes(IVideoRenderer *videoRenderer) override;
    bool hasHighResolutionTrack() const override;
    bool hasLowResolutionTrack() const override;
    bool isModerator() const override;

private:
    // Data about the partipant in the call relative to this session
    sfu::Peer mPeer;

    // ---- SLOTs -----
    // Ownership is kept by the Call

    RemoteVideoSlot* mVthumSlot = nullptr;
    RemoteVideoSlot* mHiresSlot = nullptr;
    RemoteAudioSlot* mAudioSlot = nullptr;

    // To notify events about the session to the app (intermediate layer)
    std::unique_ptr<SessionHandler> mSessionHandler = nullptr;
    bool mAudioDetected = false;

    // Session starts directly in progress: the SFU sends the tracks immediately from new peer
    SessionState mState = kSessStateInProgress;
    TermCode mTermCode = kInvalidTermCode;
};

/**
 * @brief Configure scalable video coding based on webrtc stats
 *
 * It's only applied to high resolution video
 */
class SvcDriver
{
public:
    static const uint8_t kMaxQualityIndex = 6;
    static const int kMinTimeBetweenSwitches = 6;   // minimum period between SVC switches in seconds

    // boundaries for switching to lower/higher quality.
    // if rtt moving average goes outside of these boundaries, switching occurs.
    static const int kRttLowerHeadroom = 30;
    static const int kRttUpperHeadroom = 250;

    SvcDriver();
    bool setSvcLayer(int8_t delta, int8_t &rxSpt, int8_t &rxTmp, int8_t &rxStmp, int8_t &txSpt);
    uint8_t mCurrentSvcLayerIndex;

    double mPacketLostLower;
    double mPacketLostUpper;
    double mPacketLostCapping;
    double mLowestRttSeen;
    double mRttLower;
    double mRttUpper;
    double mMovingAverageRtt;
    double mMovingAveragePlost;
    double mVtxDelay;
    double mMovingAverageVideoTxHeight;
    time_t mTsLastSwitch;
};


/**
* @brief The Call class
*
* This object is created upon OP_JOINEDCALL (or OP_CALLSTATE).
* It implements ICall interface for the intermediate layer.
*/
class Call final : public karere::DeleteTrackable, public sfu::SfuInterface, public ICall
{
public:
    enum SpeakerState
    {
        kNoSpeaker = 0,
        kPending = 1,
        kActive = 2,
    };

    static constexpr unsigned int kmax_bitrate_kbps = 100 *1024; // max bitrate in KBPS
    static constexpr unsigned int kMediaKeyLen = 16; // length in Bytes of derived ephemeral key
    static constexpr unsigned int kConnectingTimeout = 30; /// Timeout to be joined to the call (kStateInProgress) after a re/connect attempt (kStateConnecting)

    Call(const karere::Id& callid, const karere::Id& chatid, const karere::Id& callerid, bool isRinging, CallHandler& callHandler, MyMegaApi& megaApi, RtcModuleSfu& rtc, bool isGroup, std::shared_ptr<std::string> callKey = nullptr, karere::AvFlags avflags = 0, bool caller = false);
    virtual ~Call();


    // ---- ICall methods ----
    //
    karere::Id getChatid() const override;
    karere::Id getCallerid() const override;
    CallState getState() const override;
    mega::m_time_t getCallWillEndTs() const override;
    int getCallDurationLimitInSecs() const override;
    sfu::SfuInterface::CallLimits getCallLimits() const override;
    bool isOwnClientCaller() const override;
    bool isJoined()  const override;
    bool isOwnPrivModerator() const override;

    // returns true if your user participates of the call
    bool participate() override;
    bool isJoining() const override;
    bool hasVideoSlot(Cid_t cid, bool highRes = true) const override;
    int getNetworkQuality() const override;
    TermCode getTermCode() const override;
    uint8_t getEndCallReason() const override;

    // called upon reception of OP_JOINEDCALL from chatd
    void joinedCallUpdateParticipants(const std::set<karere::Id> &usersJoined) override;

    // add new participant to mParticipants map, and notify stopOutgoingRinging for 1on1 calls if it's required
    void addParticipant(const karere::Id &peer) override;

    // called upon reception of OP_LEFTCALL from chatd
    void removeParticipant(const karere::Id &peer) override;
    // check if our peer is participating in the call (called from chatd)
    bool alreadyParticipating() override;

    // called from chatd::onDisconnect() to remove peers from the call when disconnected from chatd
    void onDisconnectFromChatd() override;
    // called from chatd::setState(online) to reconnect to SFU
    void reconnectToSfu() override;

    promise::Promise<void> hangup() override;
    promise::Promise<void> endCall() override;  // only used on 1on1 when incoming call is rejected or moderator in group call to finish it for all participants
    promise::Promise<void> join(karere::AvFlags avFlags) override;

    std::set<Cid_t> enableAudioLevelMonitor(const bool enable) override;
    bool isAudioLevelMonitorEnabled() const override;

    // called when the user wants to "mute" an incoming call (the call is kept in ringing state)
    void ignoreCall() override;
    bool isIgnored() const override;

    void setRinging(bool ringing) override;
    void stopOutgoingRinging() override;
    bool isRinging() const override;    // (always false for outgoing calls)
    bool isOutgoingRinging() const override; // (always false for incomming calls or groupal calls)

    void setOnHold() override;
    void releaseOnHold() override;

    void setCallerId(const karere::Id& callerid) override;
    karere::Id getCallid() const override;
    bool isSpeakRequestEnabled() const override { return mSpeakRequest; }
    bool hasUserPendingSpeakRequest(const karere::Id& uh) const override;
    int getWrJoiningState() const override;
    void setLimits(const uint32_t callDurSecs, const uint32_t numUsers, const uint32_t numClientsPerUser, const uint32_t numClients, const uint32_t divider) const override;
    void addOrRemoveSpeaker(const karere::Id& user, const bool add) override;
    void raiseHandToSpeak(const bool add) override;
    void pushUsersIntoWaitingRoom(const std::set<karere::Id>& users, const bool all) const override;
    void allowUsersJoinCall(const std::set<karere::Id>& users, const bool all) const override;
    void kickUsersFromCall(const std::set<karere::Id>& users) const override;
    void mutePeers(const Cid_t& cid, const unsigned av) const override;

    void requestHighResolutionVideo(Cid_t cid, int quality) override;
    void stopHighResolutionVideo(std::vector<Cid_t> &cids) override;

    void requestLowResolutionVideo(std::vector<Cid_t> &cids) override;
    void stopLowResolutionVideo(std::vector<Cid_t> &cids) override;

    // ask the SFU to get higher/lower (spatial) quality of HighRes video (thanks to SVC), on demand by the app
    void requestHiResQuality(Cid_t cid, int quality) override;

    const std::vector<karere::Id>& getSpeakRequestsList() const override;
    std::set<karere::Id> getSpeakersList () const override;
    std::set<karere::Id> getModerators() const override;
    const std::vector<karere::Id> &getRaiseHandsList() const override;
    std::set<karere::Id> getParticipants() const override;
    std::vector<Cid_t> getSessionsCids() const override;
    ISession* getIsession(Cid_t cid) const override;

    bool isOutgoing() const override;   // true if your user started the call

    int64_t getCallInitialTimeStamp() const override;
    int64_t getFinalTimeStamp() const override;

    karere::AvFlags getLocalAvFlags() const override;
    void updateAndSendLocalAvFlags(karere::AvFlags flags) override;
    const KarereWaitingRoom* getWaitingRoom() const override;
    bool hasUserSpeakPermission(const uint64_t userid) const override;
    bool addDelSpeakRequest(const karere::Id& user, const bool add) override;

    //
    // ------ end ICall methods -----


    Session* getSession(Cid_t cid);
    std::set<Cid_t> getSessionsCidsByUserHandle(const karere::Id& id);
    void setState(CallState newState);
    static const char *stateToStr(CallState state);

    bool connectSfu(const std::string& sfuUrlStr);
    void joinSfu();

    // generates an ephemeral ECDH X25519 keypair and a signature with format: sesskey|<callId>|<clientId>|<pubkey>
    std::string generateSessionKeyPair();

    // get ephemeral ECDH X25519 keypair for the current call session
    const mega::ECDH* getMyEphemeralKeyPair() const;

    void createTransceivers(size_t &hiresTrackIndex);  // both, for sending your audio/video and for receiving from participants
    void getLocalStreams(); // update video and audio tracks based on AV flags and call state (on-hold)
    void disconnectFromSfu(const TermCode &termCode, bool hadParticipants);

    // ordered call disconnect by sending BYE command before performing SFU and media channel disconnect
    void orderedCallDisconnect(TermCode termCode, const std::string &msg, const bool forceDisconnect = false);

    // immediate disconnect (without sending BYE command) from SFU and media channel, and also clear call resources
    void immediateCallDisconnect(const TermCode& termCode);

    // delete call immediately
    void removeCallImmediately(uint8_t reason, TermCode connectionTermCode);

    // clear call resources
    void clearResources(const TermCode& termCode);

    // disconnect from media channel (MyPeerConnection)
    void mediaChannelDisconnect(bool releaseDevices = false);

    // set temporal endCallReason (when call is not destroyed immediately)
    void setTempEndCallReason(uint8_t reason);

    // set speakRequest flag
    void setSpeakRequest(const bool enabled)    { mSpeakRequest = enabled; }

    // set definitive endCallReason
    void setEndCallReason(uint8_t reason);
    std::string endCallReasonToString(const EndCallReason &reason) const;
    std::string connectionTermCodeToString(const TermCode &termcode) const;
    bool isValidConnectionTermcode(TermCode termCode) const;
    void sendStats(const TermCode& termCode);
    static EndCallReason getEndCallReasonFromTermcode(const TermCode& termCode);

    void clearParticipants();
    bool hasCallKey();
    bool isValidWrJoiningState() const;
    bool isConnectedToSfu() const;
    bool isSendingBye() const;
    void clearWrJoiningState();
    void setWrJoiningState(const sfu::WrState status);
    void setPrevCid(Cid_t prevcid);
    Cid_t getPrevCid() const;
    bool checkWrFlag() const;

    void setWrFlag(bool enabled)            { mIsWaitingRoomEnabled = enabled; }
    bool isWrFlagEnabled() const            { return mIsWaitingRoomEnabled;    }
    void clearJoinOffset()                  { mJoinOffset = mega::mega_invalid_timestamp; }
    void setJoinOffset(const int64_t t)     { mJoinOffset = t; }
    int64_t getJoinOffset() const           { return mJoinOffset; }

    // Connection initial timestamp related methods: mConnInitialTs is initialized every time we receive ANSWER command
    // and must be reset when we start as new reconnection attempt
    void clearConnInitialTs()               { mConnInitialTs = mega::mega_invalid_timestamp; }
    void captureConnInitialTs()             { mConnInitialTs = ::mega::m_time(nullptr); }
    int64_t getConnInitialTimeStamp() const { return mConnInitialTs; }

    void captureCallInitialTs()
    {
        // this is captured only the first time we effectively join the call
        // and will persists until the call is destroyed
        if (!mega::isValidTimeStamp(mCallInitialTs))
        {
            mCallInitialTs = ::mega::m_time(nullptr);
        }
    }

    sfu::Peer &getMyPeer();
    sfu::SfuClient& getSfuClient();
    std::map<Cid_t, std::unique_ptr<Session>>& getSessions();
    void takeCameraDevice();
    void releaseCameraDevice();
    bool hasCameraDevice();
    void takeScreenDevice();
    void releaseScreenDevice();
    bool hasScreenDevice();
    void freeLocalVideoTracks();
    // enable/disable video tracks depending on the video's flag and the call on-hold
    void updateVideoTracks();
    void updateNetworkQuality(int networkQuality);
    void setDestroying(bool isDestroying);
    bool isDestroying();
    bool isDisconnecting();
    bool addWrUsers(const sfu::WrUserList& users, const bool clearCurrent);
    void pushIntoWr(const TermCode& termCode);
    bool dumpWrUsers(const sfu::WrUserList& wrUsers, const bool clearCurrent);
    bool checkWrCommandReqs(std::string && commandStr, bool mustBeModerator);
    bool manageAllowedDeniedWrUSers(const std::set<karere::Id>& users, bool allow, std::string && commandStr);
    bool updateUserModeratorStatus(const karere::Id& userid, const bool enable);
    bool updateSpeakersList(const karere::Id& userid, const bool add);
    bool updateSpeakRequestsList(const karere::Id& userid, const bool add);

    // --- SfuInterface methods ---
    bool handleAvCommand(Cid_t cid, unsigned av, uint32_t aMid) override;
    bool handleAnswerCommand(Cid_t cid, std::shared_ptr<sfu::Sdp> spd, uint64_t callJoinOffset, std::vector<sfu::Peer>& peers,
                             const std::map<Cid_t, std::string>& keystrmap, const std::map<Cid_t, sfu::TrackDescriptor>& vthumbs,
                             const std::set<karere::Id>& speakers,
                             const std::vector<karere::Id>& speakReqs,
                             const std::vector<karere::Id>& raiseHands,
                             const std::map<Cid_t, uint32_t>& amidmap) override;
    bool handleKeyCommand(const Keyid_t& keyid, const Cid_t& cid, const std::string& key) override;
    bool handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors) override;
    bool handleVThumbsStartCommand() override;
    bool handleVThumbsStopCommand() override;
    bool handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors) override;
    bool handleHiResStartCommand() override;
    bool handleHiResStopCommand() override;
    bool handleSpeakerAddDelCommand(const uint64_t userid, const bool add) override;
    bool handleSpeakReqAddDelCommand(const uint64_t userid, const bool add) override;
    bool handlePeerJoin(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string> &ivs) override;
    bool handlePeerLeft(Cid_t cid, unsigned termcode) override;
    bool handleBye(const unsigned termCode, const bool wr, const std::string& errMsg) override;
    void onSfuDisconnected() override;
    void onByeCommandSent() override;
    bool handleRaiseHandAddCommand(const uint64_t userid) override;
    bool handleRaiseHandDelCommand(const uint64_t userid) override;
    bool hasRaisedHand(const uint64_t userid) const;
    bool handleModAdd (uint64_t userid) override;
    bool handleModDel (uint64_t userid) override;
    bool handleHello (const Cid_t cid, const unsigned int nAudioTracks,
                      const std::set<karere::Id>& mods, const bool wr, const bool allowed,
                      const bool speakRequest, const sfu::WrUserList& wrUsers, const CallLimits& callLimits) override;

    // --- SfuInterface methods (waiting room related methods) ---
    bool handleWrDump(const sfu::WrUserList& users) override;
    bool handleWrEnter(const sfu::WrUserList& users) override;
    bool handleWrLeave(const karere::Id& user) override;
    bool handleWrAllow(const Cid_t& cid) override;
    bool handleWrDeny() override;
    bool handleWrUsersAllow(const std::set<karere::Id>& users) override;
    bool handleWrUsersDeny(const std::set<karere::Id>& users) override;

    bool handleWillEndCommand(const unsigned int endsIn) override;
    bool handleClimitsCommand(const sfu::SfuInterface::CallLimits& callLimits) override;

    bool handleMutedCommand(const unsigned av, const Cid_t cidPerf) override;

    bool error(unsigned int code, const std::string& errMsg) override;
    bool processDeny(const std::string& cmd, const std::string& msg) override;
    void logError(const char* error) override;

    // PeerConnectionInterface events
    void onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
    void onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void onRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver);
    void onConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState);

    const char* getLoggingName() const
    {
        return mMegaApi.getLoggingName();
    }

protected:
    /* if we are connected to chatd, this participant list will be managed exclusively by meetings related chatd commands
     * if we are disconnected from chatd (chatd connectivity lost), but still participating in a call, peerleft and peerjoin SFU commands
     * could also add/remove participants to this list, in order to keep participants up to date */
    std::set<karere::Id> mParticipants;
    karere::Id mCallid;
    karere::Id mChatid;
    karere::Id mCallerId;
    CallState mState = CallState::kStateUninitialized;
    bool mIsRinging = false;

    // list of user handles of all users that have raised hand to speak
    // list is ordered by the time they raised their hand (the last element is the user that raised their hand last)
    // users in this list only indicates that he wants to speak
    std::vector<karere::Id> mRaiseHands;

    // list of user handles of all users that have been given speak permission (moderators not included)
    std::set<karere::Id> mSpeakers;

    // list of speak requests
    // list is ordered by the time they requested to speak (the last element is the user that requested to speak)
    std::vector<karere::Id> mSpeakRequests;

    // (just for 1on1 calls) flag to indicate that outgoing ringing sound is reproducing
    // no need to reset this flag as 1on1 calls, are destroyed when any of the participants hangs up
    bool mIsOutgoingRinging = false;
    bool mIgnored = false;
    bool mIsOwnClientCaller = false; // flag to indicate if our client is the caller

    /* This var is set true, when are going to destroy the call due to any of the following reasons:
      * - BYE command received with non retriable termcode
      * - SFU error received
      * - DELCALLREASON
      * - Our own user doesn't participate in chatroom
      * - We have completed reconnection into an empty chatroom
      * - Reconnection attempt has not succeeded after max timeout
      */
    bool mIsDestroyingCall = false;

    // this flag indicates if we are reconnecting to chatd or not, in order to update mParticipants from chatd or SFU (in case we have lost chatd connectivity)
    bool mIsReconnectingToChatd = false;

    // audio level monitor is enabled or not
    bool mAudioLevelMonitor = false;

    // state of joining status for our own client, when waiting room is enabled
    sfu::WrState mWrJoiningState = sfu::WrState::WR_UNKNOWN;

    int64_t mJoinOffset = 0;    // offset ts when we join within the call respect the call start (millis)
    int64_t mFinalTs = 0;       // end of the call (seconds)

    // duration of call since the last time we effectively join call, until we start a new reconnection attempt or call finish (seconds)
    int64_t mConnInitialTs = 0;

    // duration of the call since the first time we effectively join call, until it finish (seconds)
    int64_t mCallInitialTs = mega::mega_invalid_timestamp;

    // Number of SFU->client audio tracks that the client must allocate. This is equal to the maximum number of simultaneous speakers the call supports.
    uint32_t mNumInputAudioTracks = 0;

    // Number of SFU->client video tracks that the client must allocate. This is equal to the maximum number of simultaneous video tracks the call supports.
    uint32_t mNumInputVideoTracks = 0;

    // timer to check stats in order to detect local audio level (for remote audio level, audio monitor does it)
    megaHandle mVoiceDetectionTimer = 0;

    // The timestamp of the setted end time for the current call
    mega::m_time_t mCallWillEndTs = mega::mega_invalid_timestamp;

    // Information about the limits of the call (kCallLimitDisabled is used for not setted values)
    CallLimits mCallLimits {};

    // speak request flag
    bool mSpeakRequest = false;

    int mNetworkQuality = rtcModule::kNetworkQualityGood;
    bool mIsGroup = false;
    TermCode mTermCode = kInvalidTermCode;
    TermCode mTempTermCode = kInvalidTermCode;
    uint8_t mEndCallReason = kInvalidReason;
    uint8_t mTempEndCallReason = kInvalidReason;

    CallHandler& mCallHandler;
    MyMegaApi& mMegaApi;
    sfu::SfuClient& mSfuClient;
    sfu::SfuConnection* mSfuConnection = nullptr;   // owned by the SfuClient::mConnections, here for convenience

    // represents the Media channel connection (via WebRTC) between the local device and SFU.
    artc::MyPeerConnection<Call> mRtcConn;
    std::string mSdpStr;   // session description provided by WebRTC::createOffer()
    std::unique_ptr<LocalSlot> mAudio;
    std::unique_ptr<LocalSlot> mVThumb;
    bool mVThumbActive = false;  // true when sending low res video
    std::unique_ptr<LocalSlot> mHiRes;
    bool mHiResActive = false;  // true when sending high res video
    std::map<uint32_t, std::unique_ptr<RemoteSlot>> mReceiverTracks;  // maps 'mid' to 'Slot'
    std::map<Cid_t, std::unique_ptr<Session>> mSessions;
    std::unique_ptr<sfu::Peer> mMyPeer;
    Cid_t mPrevCid = K_INVALID_CID;
    uint8_t mMaxPeers = 0; // maximum number of peers (excluding yourself), seen throughout the call

    /* Peer verification promises related methods */

    // add peer to pending verification map upon ANSWER|PEERJOIN commands
    bool addPendingPeer(const Cid_t cid);

    // clear peers pending verification map
    void clearPendingPeers();

    // remove peer from pending verification map
    bool removePendingPeer(const Cid_t cid);

    // check if peer is pending to be verified
    bool isPeerPendingToAdd(const Cid_t cid) const;

    // check if peer has been received upon ANSWER | PEERJOIN command
    bool peerExists(const Cid_t cid) const;

    // complete peer verification resolving the promise associated to it
    bool fullfilPeerPms(const Cid_t cid, const bool ephemKeyVerified);

    // return peer verification promise
    promise::Promise<void>* getPeerVerificationPms(const Cid_t cid);

    // call key for public chats (128-bit key)
    std::string mCallKey;

    // this flag prevents that we start multiple joining attempts for a call
    bool mIsJoining;
    RtcModuleSfu& mRtc;
    artc::VideoCapturerManager* mCameraManager = nullptr;
    artc::VideoCapturerManager* mScreenManager = nullptr;

    megaHandle mConnectTimer = 0;    // Handler of the timeout for call re/connecting
    megaHandle mStatsTimer = 0;
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> mStatConnCallback;
    Stats mStats;
    SvcDriver mSvcDriver;

    /* maps peer cid to ephemeral key verification promise.
     * when a new peer is received (ANSWER | PEERJOIN), we need to verify and derive it's ephemeral key
     * this proccess could not be immediate as we may need to fetch it's public keys from API (ED25519 | CU25519)
     *
     * if during that verification proccess, we receive another command related to that peer Cid, we won't find session for that peer,
     * as we add the new session once the peer ephemeral key has been verified (even if verification failed)
     *
     * with this workarround, we must wait for peer promise completion, before trying to retrieve peer session
     */
    std::map<Cid_t, promise::Promise<void>> mPeersVerification;

    /*
     * List of participants with moderator role
     *
     * This list must be updated with any of the following events, independently if those users
     * currently has answered or not the call.
     *
     * The information about moderator role is only updated from SFU.
     *  1) ANSWER command: When user receives Answer call, SFU will provide a list with current moderators for this call
     *  2) ADDMOD command: informs that a peer has been granted with moderator role
     *  3) DELMOD command: informs that a peer has been removed it's moderator role
     *
     *  Participants with moderator role can:
     *  - End groupal calls for all participants
     *  - Approve/reject speaker requests
     */
    std::set<karere::Id> mModerators;

    /*
     * List of users in the waiting room, and it's permission to JOIN the call (0 = WR_NOT_ALLOWED | 1 = WR_ALLOWED)
     *  - users with permission = WR_NOT_ALLOWED  must wait in the waiting room, until receive WR_ALLOW notification (then they can send JOIN command)
     *  - users with permission = WR_ALLOWED can enter the call directly by sending JOIN command to SFU
     */
    std::unique_ptr<KarereWaitingRoom> mWaitingRoom;

    // symetric cipher for media key encryption
    mega::SymmCipher mSymCipher;

    // ephemeral X25519 EC key pair for current session
    std::unique_ptr<mega::ECDH> mEphemeralKeyPair;

    // this flag indicates if waiting room is enabled or not for this call
    bool mIsWaitingRoomEnabled = false;

    Keyid_t generateNextKeyId();
    void generateAndSendNewMediakey(bool reset = false);
    // associate slots with their corresponding sessions (video)
    void handleIncomingVideo(const std::map<Cid_t, sfu::TrackDescriptor> &videotrackDescriptors, VideoResolution videoResolution);
    // associate slots with their corresponding sessions (audio)
    void addSpeaker(const Cid_t cid, const uint32_t amid);
    const std::string &getCallKey() const;
    // enable/disable audio track depending on the audio's flag, the speaker is allowed and the call on-hold
    void updateAudioTracks();
    void attachSlotToSession (Session& session, RemoteSlot* slot, const bool audio, const VideoResolution hiRes);
    void initStatsValues();
    void enableStats();
    void disableStats();
    void adjustSvcByStats();
    void collectNonRTCStats();
    // ask the SFU to get higher/lower (spatial + temporal) quality of HighRes video (thanks to SVC), automatically due to network quality
    void updateSvcQuality(int8_t delta);
    void resetLocalAvFlags();
    bool isUdpDisconnected() const;
    bool isTermCodeRetriable(const TermCode& termCode) const;
    bool isDisconnectionTermcode(const TermCode& termCode) const;
    Cid_t getOwnCid() const;
    const karere::Id& getOwnPeerId() const;
    void setOwnModerator(bool isModerator);

    // an external event from SFU requires to mute our client (audio flag is already unset from the SFU's viewpoint)
    void muteMyClient(const bool audio, const bool video, const Cid_t cidPerf = K_INVALID_CID);

    // initializes a new pair of keys x25519 (for session key)
    void generateEphemeralKeyPair();

    // generates salt with two of 8-Byte stream encryption iv of the peer and two of our 8-Byte stream encryption iv sorted alphabetically
    std::vector<mega::byte> generateEphemeralKeyIv(const std::vector<std::string>& peerIvs, const std::vector<std::string>& myIvs) const;

    // sets the ephemeral pub key for the peer, stores the peer in `mSessions` and calls back onNewSession()
    void addPeer(sfu::Peer& peer, const std::string& ephemeralPubKeyDerived);

    // parse received ephemeral public key string (publickey:signature)
    std::pair<std::string, std::string>splitPubKey(const std::string &keyStr) const;

    // verify signature for received ephemeral key
    promise::Promise<bool> verifySignature(const Cid_t cid, const uint64_t userid, const std::string& pubkey, const std::string& signature);

    // --- speakers list methods ---
    bool addToSpeakersList (const uint64_t userid)              { return mSpeakers.emplace(userid).second; }
    bool removeFromSpeakersList (const uint64_t userid)         { return mSpeakers.erase(userid); }
    bool isOnSpeakersList (const uint64_t userid) const         { return isSpeakRequestEnabled() && mSpeakers.find(userid) != mSpeakers.end(); }
    void clearSpeakersList()                                    { mSpeakers.clear(); }

    // --- moderators list methods ---
    bool addToModeratorsList (const uint64_t userid)            { return mModerators.emplace(userid).second; }
    bool removeFromModeratorsList (const uint64_t userid)       { return mModerators.erase(userid); }
    bool isOnModeratorsList (const uint64_t userid) const       { return mModerators.find(userid) != mModerators.end(); }
    void clearModeratorsList()                                  { mModerators.clear(); }

    // --- speak requests list methods ---
    void clearSpeakRequestsList()                               { mSpeakRequests.clear(); }

    bool isOnSpeakRequestsList(const uint64_t userid) const
    {
        return std::find(mSpeakRequests.begin(), mSpeakRequests.end(), userid) !=
               mSpeakRequests.end();
    }

    bool addToSpeakRequestsList(const uint64_t userid)
    {
        if (!isOnSpeakRequestsList(userid))
        {
            mSpeakRequests.emplace_back(userid);
            return true;
        }
        return false;
    }

    bool removeFromSpeakRequestsList(const uint64_t userid)
    {
        if (auto it = std::find(mSpeakRequests.begin(), mSpeakRequests.end(), userid);
            it != mSpeakRequests.end())
        {
            mSpeakRequests.erase(it);
            return true;
        }
        return false;
    }
};

class RtcModuleSfu;

class RtcCameraVideoSink : public VideoSink
{
public:
    std::map<karere::Id, std::unique_ptr<IVideoRenderer>> mRenderers;

    RtcCameraVideoSink(void *appCtx, const RtcModuleSfu& moduleSfu): VideoSink(appCtx), mModuleSfu(moduleSfu), mAppCtx(appCtx) {}
    void OnFrame(const webrtc::VideoFrame& frame) override;
private:
    const RtcModuleSfu& mModuleSfu;
    void* mAppCtx;
};

class RtcScreenVideoSink : public VideoSink
{
public:
    std::map<karere::Id, std::unique_ptr<IVideoRenderer>> mRenderers;

    RtcScreenVideoSink(void *appCtx, const RtcModuleSfu& moduleSfu): VideoSink(appCtx), mModuleSfu(moduleSfu), mAppCtx(appCtx) {}
    void OnFrame(const webrtc::VideoFrame& frame) override;
private:
    const RtcModuleSfu& mModuleSfu;
    void* mAppCtx;
};

class RtcModuleSfu : public RtcModule, public karere::DeleteTrackable
{
public:
    enum class RtcDevType: int
    {
        TYPE_CAPTURER_CAMERA = 0,
        TYPE_CAPTURER_SCREEN = 1,
    };

    static bool checkValidDevType(const int t)
    {
        return t == static_cast<int>(RtcDevType::TYPE_CAPTURER_CAMERA) ||
               t == static_cast<int>(RtcDevType::TYPE_CAPTURER_SCREEN);
    }

    struct VideoDevice
    {
        std::string id;
    };

    struct ScreenDevice
    {
        long int id;
    };

    typedef std::variant<VideoDevice, ScreenDevice> InputDevice;

    static std::optional<InputDevice> inputDeviceFactory(const int type)
    {
        auto t = static_cast<RtcDevType>(type);
        if (t == RtcDevType::TYPE_CAPTURER_CAMERA) { return VideoDevice();  }
        if (t == RtcDevType::TYPE_CAPTURER_SCREEN) { return ScreenDevice(); }
        return std::nullopt;
    }

    RtcModuleSfu(MyMegaApi &megaApi, CallHandler &callhandler, DNScache &dnsCache,
                 WebsocketsIO& websocketIO, void *appCtx,
                 rtcModule::RtcCryptoMeetings* rRtcCryptoMeetings);
    ICall* findCall(const karere::Id &callid) const override;
    ICall* findCallByChatid(const karere::Id &chatid) const override;
    bool isCallStartInProgress(const karere::Id &chatid) const override;
    bool setVideoCapturerInDevice(const std::string& device, const int type) override;
    void getVideoInDevices(std::set<std::string>& devicesVector) override;
    std::set<std::pair<std::string, long int>> getScreenDevices() override;
    std::string getVideoDeviceNameById(const std::string& id) override;
    std::string getScreenDeviceNameById(const long int id) override;
    promise::Promise<void> startCall(const karere::Id &chatid, karere::AvFlags avFlags, bool isGroup, const bool notRinging, std::shared_ptr<std::string> unifiedKey = nullptr) override;
    void takeCameraDevice() override;
    void releaseCameraDevice() override;
    void takeScreenDevice() override;
    void releaseScreenDevice() override;
    void addLocalCameraRenderer(const karere::Id& chatid, IVideoRenderer *videoRederer) override;
    bool hasLocalCameraRenderer(const karere::Id &chatid) const override;
    bool hasLocalScreenRenderer(const karere::Id &chatid) const override;
    void removeLocalCameraRenderer(const karere::Id& chatid) override;
    void addLocalScreenRenderer(const karere::Id& chatid, IVideoRenderer *videoRederer) override;
    void removeLocalScreenRenderer(const karere::Id& chatid) override;
    void onMediaKeyDecryptionFailed(const std::string& err);

    std::vector<karere::Id> chatsWithCall() override;
    unsigned int getNumCalls() override;
    const std::optional<std::string>& getCameraDeviceIdSelected() const override;
    const std::optional<long int>& getScreenDeviceIdSelected() const override;
    sfu::SfuClient& getSfuClient() override;
    DNScache& getDnsCache() override;

    void onDestroyCall(rtcModule::ICall* iCall, EndCallReason reason, TermCode connectionTermCode) override;
    void rtcOrderedCallDisconnect(rtcModule::ICall* iCall, TermCode connectionTermCode) override;
    void deleteCall(const karere::Id& callId);

    void handleJoinedCall(const karere::Id &chatid, const karere::Id &callid, const std::set<karere::Id>& usersJoined) override;
    void handleLeftCall(const karere::Id &chatid, const karere::Id &callid, const std::set<karere::Id>& usersLeft) override;
    void handleNewCall(const karere::Id &chatid, const karere::Id &callerid, const karere::Id &callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey = nullptr) override;

    artc::VideoCapturerManager* getCameraDevice();
    artc::VideoCapturerManager* getScreenDevice();
    void changeCameraDevice(const std::string &deviceId, bool shouldOpen);
    void changeScreenDevice(const long int deviceId, bool shouldOpen);
    void openCameraDevice();
    void closeCameraDevice();
    void openScreenDevice();
    void closeScreenDevice();

    void* getAppCtx();
    std::string getDeviceInfo() const;
    unsigned int getNumInputVideoTracks() const override;
    void setNumInputVideoTracks(const unsigned int numInputVideoTracks) override;
    void enableSpeakRequestSupportForCalls(const bool enable) override;
    bool isSpeakRequestSupportEnabled() const override;
    sfu::SfuProtocol getMySfuProtoVersion() const override;

    const char* getLoggingName() const
    {
        return mMegaApi.getLoggingName();
    }

private:
    std::map<karere::Id, std::unique_ptr<Call>> mCalls;
    CallHandler& mCallHandler;
    MyMegaApi& mMegaApi;
    DNScache &mDnsCache;
    std::unique_ptr<sfu::SfuClient> mSfuClient;

    // selected device id for camera device (string format)
    std::optional<std::string> mSelectedCameraDeviceId = std::nullopt;
    // selected device id for screen capturer device (numeric format)
    std::optional<long int> mSelectedScreenDeviceId = std::nullopt;

    std::optional<std::string> getDefaultCameraDeviceId();
    std::optional<long int> getDefaultScreenDeviceId();

    rtc::scoped_refptr<artc::VideoCapturerManager> mCameraCapturerDevice;
    // count of times the device(Camera) has been taken (without being released)
    unsigned int mCameraDeviceTakenCount = 0;
    RtcCameraVideoSink mCameraVideoSink;

    rtc::scoped_refptr<artc::VideoCapturerManager> mScreenCapturerDevice;
    // count of times the device (Screen) has been taken (without being released)
    unsigned int mScreenDeviceTakenCount = 0;
    RtcScreenVideoSink mScreenVideoSink;

    std::map<karere::Id, VideoSink> mVideoSink;
    void* mAppCtx = nullptr;
    std::set<karere::Id> mCallStartAttempts;
    bool mIsSpeakRequestEnabled = false;
    sfu::SfuProtocol mMySfuProtoVersion = sfu::SfuProtocol::SFU_PROTO_PROD; // own client SFU protocol version

    // Current limit for simultaneous input video tracks that call supports. (kMaxCallVideoSenders by default)
    unsigned int mRtcNumInputVideoTracks = getMaxSupportedVideoCallParticipants();
};

#endif
}


#endif // WEBRTCSFU_H
