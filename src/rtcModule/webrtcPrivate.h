#ifndef WEBRTCSFU_H
#define WEBRTCSFU_H

#include <logger.h>
#include <rtcModule/webrtcAdapter.h>
#include <rtcModule/webrtc.h>
#include <sfu.h>
#include <IVideoRenderer.h>

#include <map>

namespace rtcModule
{
#ifdef KARERE_DISABLE_WEBRTC
class IGlobalCallHandler
{
};
#else

class RtcModuleSfu;
class Call;
class Session;
class AudioLevelMonitor : public webrtc::AudioTrackSinkInterface
{
    public:
    AudioLevelMonitor(Call &call, int32_t cid = -1);
    void OnData(const void *audio_data,
                        int bits_per_sample,
                        int sample_rate,
                        size_t number_of_channels,
                        size_t number_of_frames) override;
    bool hasAudio();
    void onAudioDetected(bool audioDetected);

private:
    time_t mPreviousTime = 0;
    Call &mCall;
    bool mAudioDetected = false;
    int32_t mCid; // -1 represents local audio monitor
};

class Slot
{
public:
    Slot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    virtual ~Slot();
    void createEncryptor(const sfu::Peer &peer);
    void createDecryptor();
    webrtc::RtpTransceiverInterface* getTransceiver();
    Cid_t getCid() const;
    void reassign(Cid_t cid, IvStatic_t iv);
    bool hasTrack(bool send);
    void createDecryptor(Cid_t cid, IvStatic_t iv);
    void enableAudioMonitor(bool enable);
    void enableTrack(bool enable);
    IvStatic_t getIv() const;
    void generateRandomIv();

protected:
    Call &mCall;
    IvStatic_t mIv;
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> mTransceiver;
    std::unique_ptr<AudioLevelMonitor> mAudioLevelMonitor;
    Cid_t mCid = 0;
    bool mAudioLevelMonitorEnabled = false;
};

class VideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    VideoSink();
    virtual ~VideoSink();
    void setVideoRender(IVideoRenderer* videoRenderer);
    virtual void OnFrame(const webrtc::VideoFrame& frame) override;
private:
    std::unique_ptr<IVideoRenderer> mRenderer;
};

class RemoteVideoSlot : public Slot, public VideoSink
{
public:
    RemoteVideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    ~RemoteVideoSlot();
    void addSinkToTrack();
    void reassignVideoSlot(Cid_t cid, IvStatic_t iv);

private:
    bool mSinkAdded = false;
};

class Session : public ISession
{
public:
    Session(const sfu::Peer& peer);
    ~Session();

    const sfu::Peer &getPeer() const;
    void setVThumSlot(RemoteVideoSlot* slot, bool reuse = false);
    void setHiResSlot(RemoteVideoSlot* slot, bool reuse = false);
    void setAudioSlot(Slot* slot);
    void addKey(Keyid_t keyid, const std::string& key);
    void setAvFlags(karere::AvFlags flags);

    Slot* getAudioSlot();
    RemoteVideoSlot* getVthumSlot();
    RemoteVideoSlot* getHiResSlot();

    void disableAudioSlot();
    void setSpeakRequested(bool requested);

    // ISession methods
    karere::Id getPeerid() const override;
    Cid_t getClientid() const override;
    SessionState getState() const override;
    karere::AvFlags getAvFlags() const override;
    bool isAudioDetected() const override;
    bool hasRequestSpeak() const override;
    void setSessionHandler(SessionHandler* sessionHandler) override;
    void setVideoRendererVthumb(IVideoRenderer *videoRederer) override;
    void setVideoRendererHiRes(IVideoRenderer *videoRederer) override;
    void setAudioDetected(bool audioDetected) override;
    bool hasHighResolutionTrack() const override;
    bool hasLowResolutionTrack() const override;

private:
    sfu::Peer mPeer;
    RemoteVideoSlot* mVthumSlot = nullptr;
    RemoteVideoSlot* mHiresSlot = nullptr;
    Slot* mAudioSlot = nullptr;
    std::unique_ptr<SessionHandler> mSessionHandler = nullptr;
    bool mHasRequestSpeak = false;
    bool mAudioDetected = false;
    SessionState mState = kSessStateInProgress;
};

class Call : public karere::DeleteTrackable, public sfu::SfuInterface, public ICall
{
public:
    enum SpeakerState
    {
        kNoSpeaker = 0,
        kPending = 1,
        kActive = 2,
    };

    Call(karere::Id callid, karere::Id chatid, karere::Id callerid, bool isRinging, IGlobalCallHandler &globalCallHandler, MyMegaApi& megaApi, RtcModuleSfu& rtc, bool isGroup, std::shared_ptr<std::string> callKey = nullptr, karere::AvFlags avflags = 0);
    virtual ~Call();
    karere::Id getCallid() const override;
    karere::Id getChatid() const override;
    karere::Id getCallerid() const override;
    bool isAudioDetected() const override;
    CallState getState() const override;
    void addParticipant(karere::Id peer) override;
    void removeParticipant(karere::Id peer) override;
    promise::Promise<void> hangup() override;
    promise::Promise<void> endCall() override;
    promise::Promise<void> join(karere::AvFlags avFlags) override;
    bool participate() override;
    void enableAudioLevelMonitor(bool enable) override;
    void ignoreCall() override;
    void setRinging(bool ringing) override;
    void setOnHold() override;
    void releaseOnHold() override;
    bool isRinging() const override;
    bool isIgnored() const override;
    bool isAudioLevelMonitorEnabled() const override;
    bool hasVideoSlot(Cid_t cid, bool highRes = true) const override;
    int getNetworkQuality() const override;
    bool hasRequestSpeak() const override;

    void setCallerId(karere::Id callerid) override;
    void requestSpeaker(bool add = true) override;
    bool isSpeakAllow() const override;
    void approveSpeakRequest(Cid_t cid, bool allow) override;
    void stopSpeak(Cid_t cid = 0) override;
    std::vector<Cid_t> getSpeakerRequested() override;
    void requestHighResolutionVideo(Cid_t cid) override;
    void requestHiResQuality(Cid_t cid, int quality) override;
    void stopHighResolutionVideo(Cid_t cid) override;
    void requestLowResolutionVideo(const std::vector<Cid_t> &cids) override;
    void stopLowResolutionVideo(const std::vector<Cid_t> &cids) override;

    std::vector<karere::Id> getParticipants() const override;
    std::vector<Cid_t> getSessionsCids() const override;
    ISession* getIsession(Cid_t cid) const override;
    Session* getSession(Cid_t cid);
    bool isOutgoing() const override;
    virtual int64_t getInitialTimeStamp() const override;
    virtual int64_t getFinalTimeStamp() const override;
    static const char *stateToStr(uint8_t state);

    void setCallHandler(CallHandler* callHanlder) override;

    karere::AvFlags getLocalAvFlags() const override;
    void updateAndSendLocalAvFlags(karere::AvFlags flags) override;
    void setAudioDetected(bool audioDetected) override;
    void updateVideoInDevice() override;
    void setState(CallState newState);
    void connectSfu(const std::string& sfuUrl, bool reconnect = false);
    void createTranceiver();
    void getLocalStreams();
    void disconnect(TermCode termCode, const std::string& msg = "");
    std::string getKeyFromPeer(Cid_t cid, Keyid_t keyid);
    bool hasCallKey();
    sfu::Peer &getMyPeer();
    sfu::SfuClient& getSfuClient();
    std::map<Cid_t, std::unique_ptr<Session>>& getSessions();
    void takeVideoDevice();
    void releaseVideoDevice();
    bool hasVideoDevice();
    void updateVideoDevice();
    void freeTracks();
    void updateVideoTracks();

    bool handleAvCommand(Cid_t cid, unsigned av) override;
    bool handleAnswerCommand(Cid_t cid, sfu::Sdp &spd, uint64_t ts, const std::vector<sfu::Peer>&peers, const std::map<Cid_t, sfu::TrackDescriptor> &vthumbs, const std::map<Cid_t, sfu::TrackDescriptor> &speakers) override;
    bool handleKeyCommand(Keyid_t keyid, Cid_t cid, const std::string& key) override;
    bool handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors) override;
    bool handleVThumbsStartCommand() override;
    bool handleVThumbsStopCommand() override;
    bool handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors) override;
    bool handleHiResStartCommand() override;
    bool handleHiResStopCommand() override;
    bool handleSpeakReqsCommand(const std::vector<Cid_t> &speakRequests) override;
    bool handleSpeakReqDelCommand(Cid_t cid) override;
    bool handleSpeakOnCommand(Cid_t cid, sfu::TrackDescriptor speaker) override;
    bool handleSpeakOffCommand(Cid_t cid) override;
    bool handleStatCommand() override;
    bool handlePeerJoin(Cid_t cid, uint64_t userid, int av) override;
    bool handlePeerLeft(Cid_t cid) override;
    bool handleError(unsigned int code, const std::string reason) override;
    bool handleModerator(Cid_t cid, bool moderator) override;

    // PeerConnectionInterface events
    void onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
    void onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void onConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState);
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state);

    // PeerConnectionInterface events (EMPTY)
    void onError();
    void onIceComplete();
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState);
    void onRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
    void onIceCandidate(std::shared_ptr<artc::IceCandText> cand);
    void onRenegotiationNeeded();
    void onDataChannel(webrtc::DataChannelInterface* data_channel);


protected:
    std::vector<karere::Id> mParticipants; // managed exclusively by meetings related chatd commands
    karere::Id mCallid;
    karere::Id mChatid;
    karere::Id mCallerId;
    CallState mState = CallState::kStateInitial;
    bool mIsRinging = false;
    bool mIgnored = false;
    SpeakerState mSpeakerState = SpeakerState::kPending;
    karere::AvFlags mLocalAvFlags = 0; // local Av flags
    int64_t mInitialTs = 0;
    int64_t mFinalTs = 0;
    bool mAudioDetected = false;
    bool mAudioLevelMonitorEnabled = false;
    std::unique_ptr<AudioLevelMonitor> mAudioLevelMonitor;
    int mNetworkQuality = kNetworkQualityDefault;
    bool mIsGroup = false;

    std::string mSfuUrl;
    IGlobalCallHandler& mGlobalCallHandler;
    MyMegaApi& mMegaApi;
    sfu::SfuClient& mSfuClient;
    sfu::SfuConnection* mSfuConnection = nullptr;

    artc::myPeerConnection<Call> mRtcConn;
    std::string mSdp;
    std::unique_ptr<Slot> mAudio;
    std::unique_ptr<Slot> mVThumb;
    bool mVThumbActive = false;
    std::unique_ptr<Slot> mHiRes;
    bool mHiResActive = false;
    std::map<uint32_t, std::unique_ptr<Slot>> mReceiverTracks;
    std::map<Cid_t, std::unique_ptr<Session>> mSessions;

    std::unique_ptr<CallHandler> mCallHandler;

    // represents own peer
    sfu::Peer mMyPeer;

    // call key for public chats (128-bit key)
    std::string mCallKey;

    RtcModuleSfu& mRtc;
    artc::VideoManager* mVideoManager = nullptr;

    void generateAndSendNewkey();
    void handleIncomingVideo(const std::map<Cid_t, sfu::TrackDescriptor> &videotrackDescriptors, bool hiRes = false);
    void addSpeaker(Cid_t cid, const sfu::TrackDescriptor &speaker);
    void removeSpeaker(Cid_t cid);
    const std::string &getCallKey() const;
    void updateAudioTracks();
    void attachSlotToSession (Cid_t cid, Slot *slot, bool audio, bool hiRes, bool reuse);

};

class RtcModuleSfu : public RtcModule, public VideoSink, public karere::DeleteTrackable
{
public:
    RtcModuleSfu(MyMegaApi& megaApi, IGlobalCallHandler& callhandler, IRtcCrypto* crypto, const char* iceServers);
    void init(WebsocketsIO& websocketIO, void *appCtx, RtcCryptoMeetings *rRtcCryptoMeetings, const karere::Id &myHandle) override;
    ICall* findCall(karere::Id callid) override;
    ICall* findCallByChatid(karere::Id chatid) override;
    bool selectVideoInDevice(const std::string& device) override;
    void getVideoInDevices(std::set<std::string>& devicesVector) override;
    promise::Promise<void> startCall(karere::Id chatid, karere::AvFlags avFlags, bool isGroup, std::shared_ptr<std::string> unifiedKey = nullptr) override;
    void takeDevice() override;
    void releaseDevice() override;
    void addLocalVideoRenderer(karere::Id chatid, IVideoRenderer *videoRederer) override;
    void removeLocalVideoRenderer(karere::Id chatid) override;

    std::vector<karere::Id> chatsWithCall() override;
    unsigned int getNumCalls() override;
    const std::string& getVideoDeviceSelected() const override;
    sfu::SfuClient& getSfuClient() override;

    void removeCall(karere::Id chatid, TermCode termCode = kUserHangup) override;

    void handleJoinedCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersJoined) override;
    void handleLeftCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersLeft) override;
    void handleCallEnd(karere::Id chatid, karere::Id callid, uint8_t reason) override;
    void handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey = nullptr) override;

    void OnFrame(const webrtc::VideoFrame& frame) override;

    artc::VideoManager* getVideoDevice();
    void changeDevice(const std::string& device);
    void openDevice();
    void closeDevice();

private:
    std::map<karere::Id, std::unique_ptr<Call>> mCalls;
    IGlobalCallHandler& mCallHandler;
    MyMegaApi& mMegaApi;
    std::unique_ptr<sfu::SfuClient> mSfuClient;
    std::string mVideoDeviceSelected;
    rtc::scoped_refptr<artc::VideoManager> mVideoDevice;
    unsigned int mDeviceCount = 0;
    std::map<karere::Id, std::unique_ptr<IVideoRenderer>> mRenderers;
    std::map<karere::Id, VideoSink> mVideoSink;
};

void globalCleanup();

#endif
}


#endif // WEBRTCSFU_H
