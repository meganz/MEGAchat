#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "IRtcModule.h"
#include "strophe.jingle.h"
#include "streamPlayer.h"

namespace disco {class Plugin;}

namespace rtcModule
{
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
struct CallRequest
{
    std::string targetJid;
    bool isFileTransfer;
    std::function<bool()> cancel;
    CallRequest(const std::string& aTargetJid, bool aIsFt, std::function<bool()>&& cancelFunc)
        : targetJid(aTargetJid), isFileTransfer(aIsFt), cancel(cancelFunc){}
};

class RtcModule: public Jingle, public IRtcModule
{
protected:
    typedef Jingle Base;
    std::shared_ptr<artc::InputAudioDevice> mAudioInput;
    std::shared_ptr<artc::InputVideoDevice> mVideoInput;
    std::shared_ptr<artc::StreamPlayer> mLocalPlayer;
    int mLocalStreamRefCount = 0;
    int mLocalVideoRefCount = 0;
    bool mLocalVideoEnabled = false;
    IEventHandler* mEventHandler;
    std::map<std::string, std::shared_ptr<CallRequest> > mCallRequests;
    artc::DeviceManager mDeviceManager;
    std::string mVideoInDeviceName;
    std::string mAudioInDeviceName;
    disco::DiscoPlugin* mDiscoPlugin = nullptr;
public:
    RtcModule(xmpp_conn_t* conn, IEventHandler* handler,
               ICryptoFunctions* crypto, const char* iceServers);
    template <class OkCb, class ErrCb>
    void myGetUserMedia(const AvFlags& av, OkCb okCb, ErrCb errCb, bool allowEmpty=false);
//IRtcModule interface
    virtual int startMediaCall(char* sidOut, const char* targetJid, const AvFlags& av, const char* files[]=NULL,
                      const char* myJid=NULL);
    virtual int hangupBySid(const char* sid, char callType, const char* reason, const char* text);
    virtual int hangupByPeer(const char* peerJid, char callType, const char* reason, const char* text);
    virtual int hangupAll(const char* reason, const char* text);
    virtual int muteUnmute(bool state, const AvFlags& what, const char* jid);
    virtual int getSentAvBySid(const char* sid, AvFlags& av);
    virtual int getSentAvByJid(const char* jid, AvFlags& av);
    virtual int getReceivedAvBySid(const char* sid, AvFlags& av);
    virtual int getReceivedAvByJid(const char* jid, AvFlags& av);
    virtual IJingleSession* getSessionByJid(const char* fullJid, char type='m');
    virtual IJingleSession* getSessionBySid(const char* sid);
    virtual int updateIceServers(const char* iceServers);
    virtual int isRelay(const char* sid);
    virtual IDeviceList* getAudioInDevices();
    virtual IDeviceList* getVideoInDevices();
    virtual int selectAudioInDevice(const char* devname);
    virtual int selectVideoInDevice(const char* devname);
    virtual void destroy() {delete this;}
protected:
    virtual ~RtcModule();
    //=== Implementation methods
    bool hasLocalStream() { return (mAudioInput || mVideoInput); }
    void logInputDevices();
    std::string getLocalAudioAndVideo();
    int getDeviceIdxByName(const std::string& name, const artc::DeviceList& devices);
    void onConnState(const xmpp_conn_event_t status,
                     const int error, xmpp_stream_error_t * const stream_error);

    template <class CB>
    void enumCallsForHangup(CB cb, const char* reason, const char* text);
    void onPresenceUnavailable(strophe::Stanza pres);
    void createLocalPlayer();
    virtual void onIncomingCallRequest(const char* from, const char* sid,
        std::shared_ptr<CallAnswerFunc>& ansFunc,
        std::shared_ptr<std::function<bool()> >& reqStillValid, const AvFlags& peerMedia,
        std::shared_ptr<std::set<std::string> >& files, void** userpPtr);
    void onCallAnswered(JingleSession& sess);
    void removeRemotePlayer(JingleSession& sess);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void onMediaStart(const std::string& sid);
    virtual void onCallTerminated(JingleSession* sess, const char* reason, const char* text,
                                  FakeSessionInfo* noSess);

    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    virtual void onRemoteStreamAdded(JingleSession& sess, artc::tspMediaStream stream);
    //void onRemoteStreamRemoved() - not interested to handle here

    void onJingleError(JingleSession* sess, const std::string& origin,
                       const std::string& stanza, strophe::Stanza orig, char type);
    virtual void discoAddFeature(const char* feature);
    //overriden handler of JinglePlugin
    virtual void onCallCanceled(const char *sid, const char *event, const char *by,
        bool accepted, void** userpPtr);
    void refLocalStream(bool sendsVideo);
    void unrefLocalStream(bool sendsVideo);
    void freeLocalStream();
    /**
        Hangs up all calls and releases any global resources referenced by this instance, such as the reference
        to the local stream and video.
    */
    AvFlags avFlagsOfStream(artc::tspMediaStream& stream, const AvFlags& flags);
    void disableLocalVideo();
    void enableLocalVideo();
    void removeRemoteVideo(JingleSession& sess);
    /**
       Creates a unique string identifying the call,
       that is independent of whether the
       caller or callee generates it. Used only for sending stats
    */
    std::string makeCallId(IJingleSession* sess);
    AvFlags getStreamAv(artc::tspMediaStream& stream);
    template <class F>
    int getAvByJid(const char* jid, AvFlags& av, F&& func);
};
}
#endif
