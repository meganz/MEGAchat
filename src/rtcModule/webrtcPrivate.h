#ifndef RTCMODULE_H
#define RTCMODULE_H
#include <webrtc.h>
#include <karereId.h>
#include "IRtcStats.h"
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
    void setState(uint8_t state);
    void handleMessage(RtMessage& packet);
    void sendAv(karere::AvFlags av);
    promise::Promise<void> sendOffer();
    void msgSdpAnswer(RtMessage& packet);
    void msgSessTerminateAck(RtMessage& packet);
    void msgSessTerminate(RtMessage& packet);
    void msgIceCandidate(RtMessage& packet);
    void msgMute(RtMessage& packet);
    void onVideoRecv();
    void submitStats(TermCode termCode, const std::string& errInfo);
    bool verifySdpFingerprints(const std::string& sdp);
    template<class... Args>
    bool cmd(uint8_t type, Args... args);
    void destroy(TermCode code, const std::string& msg="");
    void asyncDestroy(TermCode code, const std::string& msg="");
    promise::Promise<void> terminateAndDestroy(TermCode code, const std::string& msg="");
    webrtc::FakeConstraints* pcConstraints();
    std::string getDeviceInfo() const;
    int calculateNetworkQuality(const stats::Sample *sample);

public:
    RtcModule& mManager;
    Session(Call& call, RtMessage& packet, SdpKey sdpkey);
    ~Session();
    void pollStats();
    artc::myPeerConnection<Session> rtcConn() const { return mRtcConn; }
    virtual bool videoReceived() const { return mVideoReceived; }
    void manageNetworkQuality(stats::Sample* sample);
    void createRtcConn();
    void veryfySdpOfferSendAnswer();
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
    void updateAvFlags(karere::AvFlags flags);
    //====
    static bool isTermRetriable(TermCode reason);
    friend class Call;
    friend class stats::Recorder; //needs access to mRtcConn
};

class Call: public ICall
{
    enum CallDataState
    {
        kCallDataNotRinging             = 0,
        kCallDataRinging                = 1,
        kCallDataEnd                    = 2,
        kCallDataSession                = 3,
        kCallDataMute                   = 4,
        kCallDataSessionKeepRinging     = 5  // obsolete
    };

    enum
    {
        kCallDataReasonEnded        = 0x01, /// normal hangup of on-going call
        kCallDataReasonRejected     = 0x02, /// incoming call was rejected by callee
        kCallDataReasonNoAnswer     = 0x03, /// outgoing call didn't receive any answer from the callee
        kCallDataReasonFailed       = 0x04, /// on-going call failed
        kCallDataReasonCancelled    = 0x05  /// outgoing call was cancelled by caller before receiving any answer from the callee
    };

    enum
    {
        kFlagRinging = 0x04
    };

protected:
    static const StateDesc sStateDesc;
    std::map<karere::Id, std::shared_ptr<Session>> mSessions;
    std::map<chatd::EndpointId, megaHandle> mSessRetries;
    std::map<chatd::EndpointId, int> mIceFails;
    std::map<chatd::EndpointId, std::pair<karere::Id, SdpKey> > mSentSessions;
    std::string mName;
    megaHandle mCallOutTimer = 0;
    bool mCallStartingSignalled = false;
    bool mCallStartedSignalled = false;
    megaHandle mInCallPingTimer = 0;
    promise::Promise<void> mDestroyPromise;
    std::shared_ptr<artc::LocalStreamHandle> mLocalStream;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    megaHandle mDestroySessionTimer = 0;
    unsigned int mTotalSessionRetry = 0;
    uint8_t mPredestroyState;
    megaHandle mStatsTimer = 0;
    megaHandle mCallSetupTimer = 0;
    bool mNotSupportedAnswer = false;
    bool mIsRingingOut = false;
    bool mHadRingAck = false;
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
    void getLocalStream(karere::AvFlags av, std::string& errors);
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
    bool sendCallData(Call::CallDataState state);
    void destroyIfNoSessionsOrRetries(TermCode reason);
    bool hasNoSessionsOrPendingRetries() const;
    uint8_t convertTermCodeToCallDataCode();
    bool cancelSessionRetryTimer(karere::Id userid, uint32_t clientid);
    void monitorCallSetupTimeout();
    friend class RtcModule;
    friend class Session;
public:
    chatd::Chat& chat() const { return mChat; }
    Call(RtcModule& rtcModule, chatd::Chat& chat,
        karere::Id callid, bool isGroup, bool isJoiner, ICallHandler* handler,
        karere::Id callerUser, uint32_t callerClient);
    ~Call();
    virtual karere::AvFlags sentAv() const;
    virtual void hangup(TermCode reason=TermCode::kInvalid);
    virtual bool answer(karere::AvFlags av);
    virtual bool changeLocalRenderer(IVideoRenderer* renderer);
    virtual karere::AvFlags muteUnmute(karere::AvFlags av);
    virtual std::map<karere::Id, karere::AvFlags> avFlagsRemotePeers() const;
    virtual std::map<karere::Id, uint8_t> sessionState() const;
    void sendBusy(bool isCallToSameUser);
    uint32_t clientidFromSession(karere::Id userid);
    void updateAvFlags(karere::Id userid, uint32_t clientid, karere::AvFlags flags);
    bool isCaller(karere::Id userid, uint32_t clientid);
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
        kCallSetupTimeout = 35000
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
    virtual void onDisconnect(chatd::Connection& conn);
    virtual void stopCallsTimers(int shard);
    virtual void handleInCall(karere::Id chatid, karere::Id userid, uint32_t clientid);
    virtual void handleCallTime(karere::Id chatid, uint32_t duration);
    virtual void onKickedFromChatRoom(karere::Id chatid);
    virtual uint32_t clientidFromPeer(karere::Id chatid, karere::Id userid);
//Implementation of virtual methods of IRtcModule
    virtual void init();
    virtual void getAudioInDevices(std::vector<std::string>& devices) const;
    virtual void getVideoInDevices(std::vector<std::string>& devices) const;
    virtual bool selectVideoInDevice(const std::string& devname);
    virtual bool selectAudioInDevice(const std::string& devname);
    virtual void loadDeviceList();
    virtual void setMediaConstraint(const std::string& name, const std::string &value, bool optional);
    virtual void setPcConstraint(const std::string& name, const std::string &value, bool optional);
    virtual bool isCallInProgress(karere::Id chatid) const;
    virtual bool isCallActive(karere::Id chatid = karere::Id::inval()) const;
    virtual void removeCall(karere::Id chatid, bool keepCallHandler = false);
    virtual void removeCallWithoutParticipants(karere::Id chatid);
    virtual void addCallHandler(karere::Id chatid, ICallHandler *callHandler);
    virtual ICallHandler *findCallHandler(karere::Id chatid);
    virtual int numCalls() const;
    virtual std::vector<karere::Id> chatsWithCall() const;
//==
    void updatePeerAvState(karere::Id chatid, karere::Id callid, karere::Id userid, uint32_t clientid, karere::AvFlags av);
    void handleCallDataRequest(chatd::Chat &chat, karere::Id userid, uint32_t clientid, karere::Id callid, karere::AvFlags avFlagsRemote);

    virtual ICall& joinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler, karere::Id callid);
    virtual ICall& startCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler);
    virtual void hangupAll(TermCode reason);
//==
    virtual ~RtcModule() {}
protected:
    const char* mStaticIceSever;
    karere::GelbProvider mIceServerProvider;
    webrtc::PeerConnectionInterface::IceServers mIceServers;
    artc::DeviceManager mDeviceManager;
    webrtc::FakeConstraints mPcConstraints;
    webrtc::FakeConstraints mMediaConstraints;
    std::map<karere::Id, std::shared_ptr<Call>> mCalls;
    std::map<karere::Id, ICallHandler *> mCallHandlers;
    IRtcCrypto& crypto() const { return *mCrypto; }
    template <class... Args>
    void cmdEndpoint(chatd::Chat &chat, uint8_t type, karere::Id chatid, karere::Id userid, uint32_t clientid, Args... args);
    template <class... Args>
    void cmdEndpoint(uint8_t type, const RtMessage& info, Args... args);
    void removeCall(Call& call);
    std::shared_ptr<artc::LocalStreamHandle> getLocalStream(karere::AvFlags av, std::string& errors, Resolution resolution);
    // no callid provided --> start call
    std::shared_ptr<Call> startOrJoinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler, karere::Id callid = karere::Id::inval());
    template <class T> T random() const;
    template <class T> void random(T& result) const;
    //=== Implementation methods
    void initInputDevices();
    const cricket::Device *getDevice(const std::string& name, const artc::DeviceList& devices);
    bool selectDevice(const std::string& devname, const artc::DeviceList& devices,
                      std::string& selected);

    void updateConstraints(Resolution resolution);
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
#define RTCM_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_EVENT(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtcevent, fmtString, ##__VA_ARGS__)

#endif
