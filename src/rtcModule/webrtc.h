#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "strophe.jingle.h"
#include "streamPlayer.h"

namespace rtcModule
{
namespace stats { class IRtcStats; }

class Call
{
protected:
    std::shared_ptr<artc::LocalStreamHandle> mLocalStream;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    std::shared_ptr<artc::StreamPlayer> mRemotePlayer;
    void createSession(const std::string& peerjid, const std::string& me, FileTransferHandler* ftHandler=NULL);
    std::shared_ptr<stats::IRtcStats>
    hangupSession(TermCode termcode, const std::string& text, bool nosend);
    bool startLocalStream(bool allowEmpty);
    void createLocalPlayer();
    void freeLocalStream();
    void removeRemotePlayer();
    void muteUnmute(AvFlags what, bool state);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void onPeerMute(AvFlags affected);
    void onPeerUnmute(AvFlags affected);
    void onMediaStart();
    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    void onRemoteStreamAdded(artc::tspMediaStream stream);
    void onRemoteStreamRemoved(artc::tspMediaStream);
    void destroy(TermCode termcode, const std::string& text="", bool noSendSessTerm=false);
    bool hangup(TermCode termcode, const std::string& text="", bool rejectIncoming=false);
    friend class Jingle;
    friend class RtcModule;
    friend class JingleSession;
public:
    using JingleCall::JingleCall;
    virtual AvFlags sentAv() const;
    virtual AvFlags receivedAv() const;
    bool hangup(const std::string& text="") { return hangup(kUserHangup, text, true); }
    void changeLocalRenderer(IVideoRenderer* renderer);
    std::string id() const;
    int isRelayed() const;
};

class RtcModule
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
protected:
    IGlobalHandler* mHandler;
    artc::DeviceManager mDeviceManager;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    artc::InputAudioDevice mAudioInput;
    artc::InputVideoDevice mVideoInput;
    std::unique_ptr<webrtc::FakeConstraints> mPcConstraints;
    std::map<karere::Id, Call&> mCalls;
public:
    RtcModule(karere::Client& client, IGlobalHandler* handler,
               ICryptoFunctions& crypto, const std::string& iceServers);
    void startMediaCall(IEventHandler* userHandler, const std::string& targetJid,
        AvFlags av, const char* files[]=nullptr, const std::string& myJid="");
    std::shared_ptr<Call> getCallByJid(const char* fullJid, char type='m');
    void getAudioInDevices(std::vector<std::string>& devices) const;
    void getVideoInDevices(std::vector<std::string>& devices) const;
    bool selectVideoInDevice(const std::string& devname);
    bool selectAudioInDevice(const std::string& devname);
    int muteUnmute(AvFlags what, bool state, const std::string& jid);
    ~RtcModule();
protected:
    //=== Implementation methods
    bool hasLocalStream() { return (mAudioInput || mVideoInput); }
    void initInputDevices();
    const cricket::Device* getDevice(const std::string& name, const artc::DeviceList& devices);
    bool selectDevice(const std::string& devname, const artc::DeviceList& devices,
                      std::string& selected);
    std::string getLocalAudioAndVideo();
    bool hasCaptureActive();
    std::shared_ptr<artc::LocalStreamHandle> getLocalStream(std::string& errors);

    void onConnState(const xmpp_conn_event_t status,
                     const int error, xmpp_stream_error_t * const stream_error);

    void onPresenceUnavailable(strophe::Stanza pres);
    virtual void discoAddFeature(const char* feature);
    friend class Call;
    //overriden handler of JinglePlugin
};
}
#endif
