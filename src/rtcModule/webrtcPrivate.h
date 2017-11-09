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
class Session: public ISession
{
protected:
    static const StateDesc sStateDesc;
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
    promise::Promise<void> mTerminatePromise;
    bool mVideoReceived = false;
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
    void onVideoRecv();
    void submitStats(TermCode termCode, const std::string& errInfo);
    bool verifySdpFingerprints(const std::string& sdp, const SdpKey& peerHash);
    template<class... Args>
    bool cmd(uint8_t type, Args... args);
    void destroy(TermCode code, const std::string& msg="");
    void asyncDestroy(TermCode code, const std::string& msg="");
    promise::Promise<void> terminateAndDestroy(TermCode code, const std::string& msg="");
    webrtc::FakeConstraints* pcConstraints();

public:
    RtcModule& mManager;
    Session(Call& call, RtMessage& packet);
    ~Session();
    artc::myPeerConnection<Session> rtcConn() const { return mRtcConn; }
    virtual bool videoReceived() const { return mVideoReceived; }
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
    //====
    friend class Call;
    friend class stats::Recorder; //needs access to mRtcConn
};

class Call: public ICall
{
protected:
    static const StateDesc sStateDesc;
    std::map<karere::Id, std::shared_ptr<Session>> mSessions;
    std::map<chatd::EndpointId, int> mSessRetries;
    std::unique_ptr<std::set<karere::Id>> mRingOutUsers;
    std::string mName;
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
    void getLocalStream(karere::AvFlags av, std::string& errors);
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
    void asyncDestroy(TermCode code, bool weTerminate);
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
};

class RtcModule: public IRtcModule, public chatd::IRtcHandler
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
    RtcModule(karere::Client& client, IGlobalHandler& handler, IRtcCrypto* crypto,
        const char* iceServers);
    virtual promise::Promise<void> init(unsigned gelbTimeout);
    promise::Promise<void> updateIceServers(unsigned timeoutMs);
    int setIceServers(const karere::ServerList<karere::TurnServerInfo>& servers);
    void onUserJoinLeave(karere::Id chatid, karere::Id userid, chatd::Priv priv);
    virtual ICall& joinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler);
    virtual ICall& startCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler);
    virtual void hangupAll(TermCode reason);
// IRtcHandler - interface to chatd
    void onDisconnect(chatd::Connection& conn);
    void handleMessage(chatd::Chat& chat, const StaticBuffer& msg);
    void onUserOffline(karere::Id chatid, karere::Id userid, uint32_t clientid);
    void onShutdown();
//Implementation of virtual methods of IRtcModule
    virtual void getAudioInDevices(std::vector<std::string>& devices) const;
    virtual void getVideoInDevices(std::vector<std::string>& devices) const;
    virtual bool selectVideoInDevice(const std::string& devname);
    virtual bool selectAudioInDevice(const std::string& devname);
    virtual void loadDeviceList();
    virtual bool isCaptureActive() const;
    virtual void setMediaConstraint(const std::string& name, const std::string &value, bool optional);
    virtual void setPcConstraint(const std::string& name, const std::string &value, bool optional);
    virtual bool isCallInProgress() const;
//==
    ~RtcModule();
protected:
    karere::FallbackServerProvider<karere::TurnServerInfo> mTurnServerProvider;
    webrtc::PeerConnectionInterface::IceServers mIceServers;
    artc::DeviceManager mDeviceManager;
    artc::InputAudioDevice mAudioInput;
    artc::InputVideoDevice mVideoInput;
    webrtc::FakeConstraints mPcConstraints;
    webrtc::FakeConstraints mMediaConstraints;
    std::map<karere::Id, std::shared_ptr<Call>> mCalls;
    IRtcCrypto& crypto() const { return *mCrypto; }
    void msgCallRequest(RtMessage& packet);
    template <class... Args>
    void cmdEndpoint(uint8_t type, const RtMessage& info, Args... args);
    void removeCall(Call& call);
    std::shared_ptr<artc::LocalStreamHandle> getLocalStream(karere::AvFlags av, std::string& errors);
    std::shared_ptr<Call> startOrJoinCall(karere::Id chatid, karere::AvFlags av, ICallHandler& handler, bool isJoin);
    template <class T> T random() const;
    template <class T> void random(T& result) const;
    //=== Implementation methods
    void initInputDevices();
    const cricket::Device* getDevice(const std::string& name, const artc::DeviceList& devices);
    bool selectDevice(const std::string& devname, const artc::DeviceList& devices,
                      std::string& selected);
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
