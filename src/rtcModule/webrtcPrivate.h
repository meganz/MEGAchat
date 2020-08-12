#ifndef RTCMODULE_H
#define RTCMODULE_H
#include <webrtc.h>
#include <karereId.h>
#include <IRtcStats.h>
#include <serverListProvider.h>
#include <chatd.h>
#include <base/trackDelete.h>
#include <streamPlayer.h>

namespace rtcModule
{
namespace stats { class IRtcStats; }
struct StateDesc
{
    std::vector<std::vector<uint8_t>> transMap;
    const char*(*toStrFunc)(uint8_t);
    void assertStateChange(uint8_t oldState, uint8_t newState) const;
};

namespace stats { class Recorder; }

class Session;
class AudioLevelMonitor : public webrtc::AudioTrackSinkInterface
{
    public:
    AudioLevelMonitor(const Session &session, ISessionHandler &sessionHandler);
    virtual void OnData(const void *audio_data,
                        int bits_per_sample,
                        int sample_rate,
                        size_t number_of_channels,
                        size_t number_of_frames);

private:
    time_t mPreviousTime = 0;
    ISessionHandler &mSessionHandler;
    const Session &mSession;
    bool mAudioDetected = false;
};

class Session: public ISession
{
public:
    class SessionInfo
    {
    public:
        SessionInfo(karere::Id sessionId, SdpKey ownHashKey, bool peerSupportsRenego)
            : mSessionId(sessionId),
              mOwnHashKey(ownHashKey),
              mPeerSupportRenegotiation(peerSupportsRenego)
        {
        }

        SessionInfo(const SessionInfo &sessInfo)
            : mSessionId(sessInfo.mSessionId),
              mOwnHashKey(sessInfo.mOwnHashKey),
              mPeerSupportRenegotiation(sessInfo.mPeerSupportRenegotiation)
        {
        }

        SessionInfo()
            : mPeerSupportRenegotiation(false)
        {
        }

        // random id of the session, sent in the SESSION packet
        karere::Id mSessionId;

        // random hash of the session, sent in the SESSION packet
        SdpKey mOwnHashKey;

        // True if the client of this session supports stream renegotiation, received in the JOIN
        bool mPeerSupportRenegotiation;
    };

protected:
    static const StateDesc sStateDesc;
    artc::tspMediaStream mRemoteStream;
    std::shared_ptr<artc::StreamPlayer> mRemotePlayer;
    std::string mOwnSdpOffer;
    std::string mOwnSdpAnswer;
    std::string mPeerSdpOffer;
    std::string mPeerSdpAnswer;
    SdpKey mPeerHash;
    SdpKey mOwnHashKey;
    SdpKey mPeerHashKey;
    artc::myPeerConnection<Session> mRtcConn;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> mVideoSender;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> mAudioSender;
    std::string mName;
    ISessionHandler* mHandler = NULL;
    std::unique_ptr<stats::Recorder> mStatRecorder;
    megaHandle mSetupTimer = 0;
    time_t mTsIceConn = 0;
    promise::Promise<void> mTerminatePromise;
    bool mVideoReceived = false;
    int mNetworkQuality = kNetworkQualityDefault;    // from 0 (worst) to 5 (best)
    long mAudioPacketLostAverage = 0;
    unsigned int mPreviousStatsSize = 0;
    std::unique_ptr<AudioLevelMonitor> mAudioLevelMonitor;
    bool mPeerSupportRenegotiation = false;
    bool mRenegotiationInProgress = false;
    unsigned int mIceDisconnections = 0;
    time_t mIceDisconnectionTs = 0;
    megaHandle mMediaRecoveryTimer = 0;
    time_t mMaxIceDisconnectedTime = 0;
    megaHandle mStreamRenegotiationTimer = 0;
    time_t mTsSdpHandshakeCompleted = 0;
    bool mFechingPeerKeys = false;
    std::vector<RtMessage> mPacketQueue;
    void setState(uint8_t state);
    void handleMessage(RtMessage& packet);
    void sendAv(karere::AvFlags av);
    promise::Promise<void> createRtcConnSendOffer();
    promise::Promise<void> sendOffer();
    void msgSdpAnswer(RtMessage& packet);
    void msgSessTerminateAck(RtMessage& packet);
    void msgSessTerminate(RtMessage& packet);
    void msgIceCandidate(RtMessage& packet);
    void msgMute(RtMessage& packet);
    void msgSdpOfferRenegotiate(RtMessage& packet);
    void msgSdpAnswerRenegotiate(RtMessage& packet);
    void onDataRecv();
    void submitStats(TermCode termCode, const std::string& errInfo);
    bool verifySdpFingerprints(const std::string& sdp);
    template<class... Args>
    bool cmd(uint8_t type, Args... args);
    void destroy(TermCode code, const std::string& msg="");
    promise::Promise<void> terminateAndDestroy(TermCode code, const std::string& msg="");
    int calculateNetworkQuality(const stats::Sample *sample);
    void removeRtcConnection();
    void setStreamRenegotiationTimeout();
    void renegotiationComplete();
    promise::Promise<void> setRemoteAnswerSdp(RtMessage& packet);
    void setOnHold(bool onHold);
    void sendAVFlags() const;
    void handleIceConnectionRecovered();
    void handleIceDisconnected();
    void cancelIceDisconnectionTimer();
    promise::Promise<void> getPeerKeey();
    void processPackets();

public:
    RtcModule& mManager;
    Session(Call& call, RtMessage& packet, const SessionInfo *sessionParameters = nullptr);
    ~Session();
    void pollStats();
    artc::myPeerConnection<Session> rtcConn() const { return mRtcConn; }
    virtual bool videoReceived() const { return mVideoReceived; }
    void manageNetworkQuality(stats::Sample* sample);
    void createRtcConn();
    promise::Promise<void> processSdpOfferSendAnswer();
    void forceDestroy();
    //PeerConnection events
    void onAddStream(artc::tspMediaStream stream);
    void onRemoveStream(artc::tspMediaStream stream);
    void onIceCandidate(std::shared_ptr<artc::IceCandText> cand);
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state);
    void onIceComplete();
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState);
    void onDataChannel(webrtc::DataChannelInterface* data_channel);
    void onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void onRenegotiationNeeded();
    void onError() {}
    void updateAvFlags(karere::AvFlags flags);
    //====
    static bool isTermRetriable(TermCode reason);
    friend class Call;
    friend class stats::Recorder; //needs access to mRtcConn
};

class SessionReconnectInfo
{
public:
    karere::Id getOldSid() const;
    unsigned int getReconnections() const;
    void setOldSid(const karere::Id& oldSid);
    void setReconnections(unsigned int reconnections);

protected:
    karere::Id mOldSid;
    unsigned int mReconnections = 0;
};

class Call: public ICall
{
    enum CallDataState
    {
        kCallDataInvalid                = -1,
        kCallDataNotRinging             = 0,
        kCallDataRinging                = 1,
        kCallDataEnd                    = 2,
        kCallDataSession                = 3,
        kCallDataMute                   = 4,
        kCallDataSessionKeepRinging     = 5  // obsolete
    };

    enum
    {
        kFlagRinging = 0x04
    };

    enum
    {
        // offset in flag's byte for JOIN and SESSION packets (the bit represents support for stream renegotiation)
        kSupportsStreamReneg = 0x04
    };

    enum
    {
        //offset in JOIN packet for byte representing flags (A/V + renegotiation)
        kOffsetFlagsJoin = 16,

        // offset in SESSION packet for byte representing flags (renegotiation)
        kOffsetFlagsSession = 64
    };

protected:
    static const StateDesc sStateDesc;
    std::map<karere::Id, std::shared_ptr<Session>> mSessions;
    std::map<chatd::EndpointId, megaHandle> mSessRetries;
    std::map<chatd::EndpointId, int> mIceFails;
    std::map<chatd::EndpointId, Session::SessionInfo> mSessionsInfo;
    std::map<chatd::EndpointId, SessionReconnectInfo> mSessionsReconnectionInfo;
    std::string mName;
    megaHandle mCallOutTimer = 0;
    bool mCallStartingSignalled = false;
    bool mCallStartedSignalled = false;
    megaHandle mInCallPingTimer = 0;
    promise::Promise<void> mDestroyPromise;
    std::shared_ptr<artc::LocalStreamHandle> mLocalStream;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    rtc::scoped_refptr<artc::VideoManager> mVideoDevice;
    megaHandle mDestroySessionTimer = 0;
    unsigned int mTotalSessionRetry = 0;
    uint8_t mPredestroyState;
    megaHandle mStatsTimer = 0;
    megaHandle mCallSetupTimer = 0;
    bool mNotSupportedAnswer = false;
    bool mIsRingingOut = false;
    bool mHadRingAck = false;
    bool mRecovered = false;
    bool mAudioLevelMonitorEnabled = false;
    CallDataState mLastCallData = kCallDataInvalid;
    karere::AvFlags mLocalFlags;
    void setState(uint8_t newState);
    void handleMessage(RtMessage& packet);
    void msgSession(RtMessage& packet);
    void msgJoin(RtMessage& packet);
    void msgRinging(RtMessage& packet);
    void msgCallReqDecline(RtMessage& packet);
    void msgCallReqCancel(RtMessage& packet);
    void msgSdpOffer(RtMessage& packet);
    void handleReject(RtMessage& packet);
    void handleBusy(RtMessage& packet);
    void getLocalStream(karere::AvFlags av);
    void muteUnmute(karere::AvFlags what, bool state);
    void onClientLeftCall(karere::Id userid, uint32_t clientid);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void clearCallOutTimer();
    void notifyCallStarting(Session& sess);
    void notifySessionConnected(Session& sess);
    void removeSession(Session& sess, TermCode reason);
    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    void onRemoteStreamAdded(artc::tspMediaStream stream);
    void onRemoteStreamRemoved(artc::tspMediaStream);
    promise::Promise<void> destroy(TermCode termcode, bool weTerminate, const std::string& msg="");
    void asyncDestroy(TermCode code, bool weTerminate);
    promise::Promise<void> gracefullyTerminateAllSessions(TermCode code);
    promise::Promise<void> waitAllSessionsTerminated(TermCode code, const std::string& msg="");
    promise::Promise<void> terminateAllSessionInmediately(TermCode code);
    bool startOrJoin(karere::AvFlags av);
    template <class... Args>
    bool cmd(uint8_t type, karere::Id userid, uint32_t clientid, Args... args);
    template <class... Args>
    bool cmdBroadcast(uint8_t type, Args... args);
    void startIncallPingTimer();
    void stopIncallPingTimer(bool endCall = true);
    bool broadcastCallReq();
    bool join(karere::Id userid=0);
    bool rejoin(karere::Id userid, uint32_t clientid);
    void sendInCallCommand();
    bool sendCallData(Call::CallDataState state = CallDataState::kCallDataInvalid);
    void destroyIfNoSessionsOrRetries(TermCode reason);
    bool hasNoSessionsOrPendingRetries() const;
    uint8_t convertTermCodeToCallDataCode();
    bool cancelSessionRetryTimer(karere::Id userid, uint32_t clientid);
    void monitorCallSetupTimeout();
    void enableAudio(bool enable);
    void enableVideo(bool enable);
    bool hasSessionWithUser(karere::Id userId);
    promise::Promise<Buffer *> loadCryptoPeerKey(karere::Id peerid);
    void sendAVFlags();
    friend class RtcModule;
    friend class Session;
public:
    chatd::Chat& chat() const { return mChat; }
    Call(RtcModule& rtcModule, chatd::Chat& chat,
        karere::Id callid, bool isGroup, bool isJoiner, ICallHandler* handler,
        karere::Id callerUser, uint32_t callerClient, bool callRecovered = false);
    ~Call();
    karere::AvFlags sentFlags() const override;
    void hangup(TermCode reason=TermCode::kInvalid) override;
    bool answer(karere::AvFlags av) override;
    bool changeLocalRenderer(IVideoRenderer* renderer) override;
    karere::AvFlags muteUnmute(karere::AvFlags av) override;
    std::map<karere::Id, karere::AvFlags> avFlagsRemotePeers() const override;
    std::map<karere::Id, uint8_t> sessionState() const override;
    void setOnHold(bool setOnHold) override;
    void sendBusy(bool isCallToSameUser);
    uint32_t clientidFromSession(karere::Id userid);
    void updateAvFlags(karere::Id userid, uint32_t clientid, karere::AvFlags flags);
    bool isCaller(karere::Id userid, uint32_t clientid);
    void changeVideoInDevice();
    bool isAudioLevelMonitorEnabled() const override;
    void enableAudioLevelMonitor(bool enable) override;
};

/*
 * Partial implementation of the WebsocketsClient, just for the purpose of
 * resolving the IPs behind the ICE servers in order to be added to cache.
 */
class DnsResolver : public WebsocketsClient
{
public:
    DnsResolver() {}
    virtual ~DnsResolver() {}

    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl) = delete;
    int wsGetNoNameErrorCode(WebsocketsIO *websocketIO) = delete;
    bool wsSendMessage(char *msg, size_t len) = delete;  // returns true on success, false if error
    void wsDisconnect(bool immediate) = delete;
    bool wsIsConnected() = delete;
    void wsCloseCbPrivate(int errcode, int errtype, const char *preason, size_t reason_len) = delete;

    void wsConnectCb() override {}
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t /*preason_len*/, bool disconnectByServer) override {}
    void wsHandleMsgCb(char *data, size_t len) override {}
    void wsSendMsgCb(const char *, size_t) override {}
};

class RtcModule: public IRtcModule, public chatd::IRtcHandler
{
public:
    enum {
        kApiTimeout = 20000,
        kCallAnswerTimeout = 40000,
        kIncallPingInterval = 4000,
        kMediaGetTimeout = 20000,
        kSessSetupTimeout = 25000,
        kCallSetupTimeout = 35000,
        kRetryCallTimeout = 30000,
        kSessFinishTimeout = 1000,
        kStreamRenegotiationTimeout = 10000,
        kIceTimeout = 18000,
        kMediaConnRecoveryTimeout = 15000,
    };

    enum Resolution
    {
        hd = 0,
        low,
        vga,
        notDefined
    };

    RtcModule(karere::Client& client, IGlobalHandler& handler, IRtcCrypto* crypto,
        const char* iceServers);
    int setIceServers(const karere::ServerList& servers);
    void addIceServers(const karere::ServerList& servers);
    webrtc::PeerConnectionInterface::IceServer createIceServer(const karere::TurnServerInfo &serverInfo);
    template <class... Args>
    void sendCommand(chatd::Chat& chat, uint8_t opcode, uint8_t command, karere::Id chatid, karere::Id userid, uint32_t clientid, Args... args);
// IRtcHandler - interface to chatd
    virtual void handleMessage(chatd::Chat& chat, const StaticBuffer& msg);
    virtual void handleCallData(chatd::Chat& chat, karere::Id chatid, karere::Id userid, uint32_t clientid, const StaticBuffer& msg);
    virtual void onShutdown();
    virtual void onClientLeftCall(karere::Id chatid, karere::Id userid, uint32_t clientid);
    virtual void stopCallsTimers(int shard);
    virtual void handleInCall(karere::Id chatid, karere::Id userid, uint32_t clientid);
    virtual void handleCallTime(karere::Id chatid, uint32_t duration);
    virtual void onKickedFromChatRoom(karere::Id chatid);
    virtual uint32_t clientidFromPeer(karere::Id chatid, karere::Id userid);
    virtual void retryCalls(int shardNo);
//Implementation of virtual methods of IRtcModule
    virtual void init();
    virtual void getAudioInDevices(std::vector<std::string>& devices) const;
    virtual void getVideoInDevices(std::set<std::string> &devices) const;
    virtual bool selectVideoInDevice(const std::string& devname);
    virtual bool selectAudioInDevice(const std::string& devname);
    virtual std::set<std::pair<std::string, std::string>> loadDeviceList() const;
    virtual std::string getVideoDeviceSelected();
    virtual bool isCallInProgress(karere::Id chatid) const;
    virtual bool isCallActive(karere::Id chatid = karere::Id::inval()) const;
    virtual void removeCall(karere::Id chatid, bool retry = false);
    virtual void removeCallWithoutParticipants(karere::Id chatid);
    virtual void addCallHandler(karere::Id chatid, ICallHandler *callHandler);
    virtual ICallHandler *findCallHandler(karere::Id chatid);
    virtual int numCalls() const;
    virtual std::vector<karere::Id> chatsWithCall() const;
    virtual void abortCallRetry(karere::Id chatid);
    void refreshTurnServerIp() override;
    void updateTurnServers() override;
//==
    void updatePeerAvState(karere::Id chatid, karere::Id callid, karere::Id userid, uint32_t clientid, karere::AvFlags av);
    void handleCallDataRequest(chatd::Chat &chat, karere::Id userid, uint32_t clientid, karere::Id callid, karere::AvFlags avFlagsRemote);

    virtual ICall& joinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler, karere::Id callid);
    virtual ICall& startCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler);
    virtual void hangupAll(TermCode reason);
//==
    karere::WebRtcLogger *getWebRtcLogger();
    std::string getDeviceInfo();
    void launchCallRetry(karere::Id chatid, karere::AvFlags av, bool isActiveRetry = true);
    virtual ~RtcModule();
protected:
    const char* mStaticIceServers;
    karere::GelbProvider mIceServerProvider;
    webrtc::PeerConnectionInterface::IceServers mIceServers;
    std::map<karere::Id, std::shared_ptr<Call>> mCalls;
    std::map<karere::Id, ICallHandler *> mCallHandlers;
    std::map<karere::Id, std::pair<karere::AvFlags, bool>> mRetryCall;
    RtcModule &mManager;
    std::map<karere::Id, megaHandle> mRetryCallTimers;
    std::string mVideoDeviceSelected;

    DnsResolver mDnsResolver;
    unsigned int mDnsRequestId = 0;

    IRtcCrypto& crypto() const { return *mCrypto; }
    template <class... Args>
    void cmdEndpoint(chatd::Chat &chat, uint8_t type, karere::Id chatid, karere::Id userid, uint32_t clientid, Args... args);
    template <class... Args>
    void cmdEndpoint(uint8_t type, const RtMessage& info, Args... args);
    void removeCall(Call& call);
    // no callid provided --> start call
    std::shared_ptr<Call> startOrJoinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler, karere::Id callid = karere::Id::inval());
    template <class T> T random() const;
    template <class T> void random(T& result) const;
    //=== Implementation methods
    void initInputDevices();

    void removeCallRetry(karere::Id chatid, bool retry = true);
    std::shared_ptr<karere::WebRtcLogger> mWebRtcLogger;

    std::string getCachedTurnServers();
    std::string buildTurnServerUrl(const std::string& host, int port, const std::string& path) const;

    friend class Call;
    friend class Session;
public:
};

struct RtMessage
{
public:
    enum { kHdrLen = 23, kPayloadOfs = kHdrLen+1 };
    chatd::Chat& chat;
    uint8_t opcode;
    uint8_t type;
    karere::Id chatid;
    karere::Id userid;
    karere::Id callid;
    uint32_t clientid;
    StaticBuffer payload;
    const char* typeStr() const { return rtcmdTypeToStr(type); }
    RtMessage(chatd::Chat& aChat, const StaticBuffer& msg);
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
        write<uint32_t>(17, clientid);
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

}

#endif
