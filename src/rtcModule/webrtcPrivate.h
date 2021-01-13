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




class Call;
class Slot
{
public:
    Slot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    virtual ~Slot();
    void createEncryptor();
    void createDecryptor();
    webrtc::RtpTransceiverInterface* getTransceiver();
    Cid_t getCid() const;
    void setParams(Cid_t cid, IvStatic_t iv);
    void enableTrack(bool enable);

protected:
    Call &mCall;
    IvStatic_t mIv;
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> mTransceiver;
    Cid_t mCid = 0;
};

class VideoSlot : public Slot, public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    VideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void setVideoRender(IVideoRenderer* videoRenderer);
    void OnFrame(const webrtc::VideoFrame& frame) override;

private:
    std::unique_ptr<IVideoRenderer> mRenderer;

};

class Session : public ISession
{
public:
    Session(const sfu::Peer& peer);
    ~Session();
    void setSessionHandler(SessionHandler* sessionHandler) override;
    const sfu::Peer &getPeer() const;
    void setVThumSlot(VideoSlot* slot);
    void setHiResSlot(VideoSlot* slot);
    void setAudioSlot(Slot* slot);
    void addKey(Keyid_t keyid, const std::string& key);
    void setAvFlags(karere::AvFlags flags);

    Slot* getAudioSlot();
    VideoSlot* getVthumSlot();
    VideoSlot* betHiResSlot();

    void setSpeakRequested(bool requested);
    bool hasRequestedSpeaker() const;

private:
    sfu::Peer mPeer;
    VideoSlot* mVthumSlot = nullptr;
    VideoSlot* mHiresSlot = nullptr;
    Slot* mAudioSlot = nullptr;
    SessionHandler* mSessionHandler = nullptr;
    bool mSpeakRequest = false;
};

class Call : public karere::DeleteTrackable, public sfu::SfuInterface, public ICall
{
public:
    Call(karere::Id callid, karere::Id chatid, IGlobalCallHandler &globalCallHandler, MyMegaApi& megaApi, sfu::SfuClient& sfuClient);
    ~Call();
    karere::Id getCallid() const override;
    karere::Id getChatid() const override;
    CallState getState() const override;
    void addParticipant(karere::Id peer) override;
    void removeParticipant(karere::Id peer) override;
    void hangup() override;
    promise::Promise<void> join() override;
    bool participate() override;
    void enableAudioLevelMonitor(bool enable) override;
    void ignoreCall() override;
    bool isRinging() const override;

    bool isModerator() const override;
    void requestModerator() override;
    void requestSpeaker(bool add = true) override;
    bool isSpeakAllow() override;
    void approveSpeakRequest(Cid_t cid, bool allow) override;
    void stopSpeak(Cid_t cid = 0) override;
    std::vector<Cid_t> getSpeakerRequested() override;
    void requestHighResolutionVideo(Cid_t cid) override;
    void stopHighResolutionVideo(Cid_t cid) override;

    void setCallHandler(CallHandler* callHanlder) override;
    void setVideoRendererVthumb(IVideoRenderer *videoRederer) override;
    void setVideoRendererHiRes(IVideoRenderer *videoRederer) override;

    void setState(CallState state);
    void connectSfu(const std::string& sfuUrl);
    void createTranceiver();
    void getLocalStreams();
    void disconnect(TermCode termCode, const std::string& msg = "");
    std::string getKeyFromPeer(Cid_t cid, Keyid_t keyid);

    bool handleAvCommand(Cid_t cid, int av) override;
    bool handleAnswerCommand(Cid_t cid, const std::string&sdp, int mod, const std::vector<sfu::Peer>&peers, const std::map<Cid_t, sfu::VideoTrackDescriptor> &vthumbs, const std::map<Cid_t, sfu::SpeakersDescriptor> &speakers) override;
    bool handleKeyCommand(Keyid_t keyid, Cid_t cid, const std::string& key) override;
    bool handleVThumbsCommand(const std::map<Cid_t, sfu::VideoTrackDescriptor> &videoTrackDescriptors) override;
    bool handleVThumbsStartCommand() override;
    bool handleVThumbsStopCommand() override;
    bool handleHiResCommand(const std::map<Cid_t, sfu::VideoTrackDescriptor> &videoTrackDescriptors) override;
    bool handleHiResStartCommand() override;
    bool handleHiResStopCommand() override;
    bool handleSpeakReqsCommand(const std::vector<Cid_t> &speakRequests) override;
    bool handleSpeakReqDelCommand(Cid_t cid) override;
    bool handleSpeakOnCommand(Cid_t cid, sfu::SpeakersDescriptor speaker) override;
    bool handleSpeakOffCommand(Cid_t cid) override;

    void onError();
    void onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
    void onRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
    void onIceCandidate(std::shared_ptr<artc::IceCandText> cand);
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state);
    void onIceComplete();
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState);
    void onDataChannel(webrtc::DataChannelInterface* data_channel);
    void onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void onRenegotiationNeeded();

public:
    std::vector<karere::Id> mParticipants;
    karere::Id mCallid;
    karere::Id mChatid;
    CallState mState = CallState::kStateInitial;
    bool mIsRinging = false;
    bool mModeratorRequested = false;
    bool mSpeakerRequested = false;
    bool mSpeakAllow = false;

    std::string mSfuUrl;
    IGlobalCallHandler& mGlobalCallHandler;
    MyMegaApi& mMegaApi;
    sfu::SfuClient& mSfuClient;
    sfu::SfuConnection* mSfuConnection;

    artc::myPeerConnection<Call> mRtcConn;
    std::string mSdp;
    std::unique_ptr<Slot> mAudio;
    std::unique_ptr<VideoSlot> mVThumb;
    std::unique_ptr<VideoSlot> mHiRes;
    std::map<std::string, std::unique_ptr<Slot>> mReceiverTracks;
    std::map<Cid_t, std::unique_ptr<Session>> mSessions;

    rtc::scoped_refptr<artc::VideoManager> mVideoDevice;

    std::unique_ptr<CallHandler> mCallHandler;

    // represents own peer
    sfu::Peer mMyPeer;

    // call key for public chats (128-bit key)
    std::string mCallKey;

    void generateAndSendNewkey();
    void handleIncomingVideo(const std::map<Cid_t, sfu::VideoTrackDescriptor> &videotrackDescriptors, bool hiRes = false);
    void addSpeaker(Cid_t cid, const sfu::SpeakersDescriptor& speaker);
    void removeSpeaker(Cid_t cid);
    sfu::Peer &getMyPeer();
    const std::string &getCallKey() const;
};

class RtcModuleSfu : public RtcModule, public karere::DeleteTrackable
{
public:
    RtcModuleSfu(MyMegaApi& megaApi, IGlobalCallHandler& callhandler, IRtcCrypto* crypto, const char* iceServers);


    void init(WebsocketsIO& websocketIO, void *appCtx, RtcCryptoMeetings *rRtcCryptoMeetings, karere::Client& karereClient) override;
    void hangupAll() override;
    ICall* findCall(karere::Id callid) override;
    ICall* findCallByChatid(karere::Id chatid) override;
    void loadDeviceList() override;
    bool selectVideoInDevice(const std::string& device) override;
    void getVideoInDevices(std::set<std::string>& devicesVector) override;
    std::string getVideoDeviceSelected() override;
    promise::Promise<void> startCall(karere::Id chatid) override;

    std::vector<karere::Id> chatsWithCall() override;
    unsigned int getNumCalls() override;

    void removeCall(karere::Id chatid, TermCode termCode = kUserHangup) override;

    void handleJoinedCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersJoined) override;
    void handleLefCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id>& usersLeft) override;
    void handleCallEnd(karere::Id chatid, karere::Id callid, uint8_t reason) override;
    void handleNewCall(karere::Id chatid, karere::Id callid) override;

private:
    std::map<karere::Id, std::unique_ptr<Call>> mCalls;
    IGlobalCallHandler& mCallHandler;
    MyMegaApi& mMegaApi;
    std::unique_ptr<sfu::SfuClient> mSfuClient;
};

void globalCleanup();
#endif
}


#endif // WEBRTCSFU_H
