#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "strophe.jingle.h"
#include "streamPlayer.h"

namespace rtcModule
{
namespace stats { class IRtcStats; }
/** This is the class that implements the user-accessible API to webrtc.
 * Since the rtc module may be built by a different compiler (necessary on windows),
 * it is built as a shared object and its API is exposed via virtual interfaces.
 * This means that any public API is implemented via virtual methods in this class
*/

/** Before being answered/rejected, initiated calls (via startMediaCall) are put in a map
 * to allow hangup() to operate seamlessly on calls in all states.These states are:
 *  - requested by us but not yet answered(mCallRequests map)
 *  - requested by peer but not yet initiated (mAutoAcceptCalls map)
 *  - in progress (mSessions map)
 */
class Call: public JingleCall
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

class RtcModule: public Jingle
{
protected:
    typedef Jingle Base;
    IGlobalEventHandler* mGlobalHandler;
    artc::DeviceManager mDeviceManager;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    artc::InputAudioDevice mAudioInput;
    artc::InputVideoDevice mVideoInput;
public:
    RtcModule(MyMegaApi *api, xmpp_conn_t* conn, IGlobalEventHandler* handler,
               ICryptoFunctions* crypto, const char* iceServers);
//TODO: we need this virtual because it's called also from the base Jingle class
//However, the class impl knows about the Call class (derived from JingleCall)
//so the only problem is passing ourself as parent as RtcModule rather than Jingle
//which would require casting
    virtual std::shared_ptr<Call>& addCall(CallState aState, bool aIsCaller,
            IEventHandler* aHandler,
            const std::string& aSid, Call::HangupFunc&& aHangupFunc,
            const std::string& aPeerJid, AvFlags localAv,
            bool aIsFt=false, const std::string& aOwnJid = "")
    {
        //we can't use make_shared because the Call ctor is protected
        auto res = emplace(aSid, std::shared_ptr<Call>(new Call(
            *this, aIsCaller, aState, aHandler, aSid,
            std::forward<Call::HangupFunc>(aHangupFunc), aPeerJid, localAv, aIsFt,
            aOwnJid.empty() ? mConn.fullJid() : aOwnJid)));
        if (!res.second)
            throw std::runtime_error("Call with that sid already exists");
        return res.first->second;
    }
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
