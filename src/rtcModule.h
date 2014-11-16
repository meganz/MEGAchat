#ifndef RTCMODULE_H
#define RTCMODULE_H

#include "IRtcModule.h"
#include "strophe.jingle.h"
#include "streamPlayer.h"

namespace karere
{
namespace rtcModule
{
/** This is the class that implements the user-accessible API to webrtc.
 * Since the rtc module may be built by a different compiler (necessary on windows),
 * it is built as a shared object and its API is exposed via virtual interfaces.
 * This means that any public API is implemented via virtual methods in this class
*/
struct AvTrackBundle
{
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video;
};

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

class RtcModule: public IRtcModule, public Jingle
{
protected:
    typedef Jingle Base;
    AvTrackBundle mLocalTracks;
    std::shared_ptr<artc::StreamPlayer> mLocalVideo;
    int mLocalStreamRefCount = 0;
    int mLocalVideoRefCount = 0;
    bool mLocalVideoEnabled = false;
    IEventHandler* mEventHandler;
    std::map<std::string, std::shared_ptr<CallRequest> > mCallRequests;
    artc::DeviceManager mDeviceManager;
public:
    RtcModule(strophe::Connection& conn, IEventHandler* handler,
               ICryptoFunctions* crypto, const char* iceServers);
    ~RtcModule();

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
    //=== Implementation methods
    bool hasLocalStream() { return (mLocalTracks.audio || mLocalTracks.video); }
    void logInputDevices();
    std::string getLocalAudioAndVideo();
    template <class OkCb, class ErrCb>
    void myGetUserMedia(const AvFlags& av, OkCb okCb, ErrCb errCb, bool allowEmpty=false);
    void onConnState(const xmpp_conn_event_t status,
                     const int error, xmpp_stream_error_t * const stream_error);

    template <class CB>
    void enumCallsForHangup(CB cb, const char* reason, const char* text);
    void onPresenceUnavailable(strophe::Stanza pres);
    void createLocalPlayer();
    virtual void onIncomingCallRequest(const char* from, std::shared_ptr<CallAnswerFunc>& ansFunc,
        std::shared_ptr<std::function<bool()> >& reqStillValid, const AvFlags& peerMedia,
        std::shared_ptr<std::set<std::string> >& files);
    void onCallAnswered(JingleSession& sess);
    void removeRemotePlayer(JingleSession& sess);
    /** Called by the remote media player when the first frame is about to be rendered,
     *  analogous to onMediaRecv in the js version
     */
    void onMediaStart(const std::string& sid);
    void onCallTerminated(JingleSession* sess, const char* reason, const char* text,
                          FakeSessionInfo* noSess);

    //onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
    virtual void onRemoteStreamAdded(JingleSession& sess, artc::tspMediaStream stream);
    //void onRemoteStreamRemoved() - not interested to handle here

    void onJingleError(JingleSession* sess, const std::string& origin,
                       const std::string& stanza, strophe::Stanza orig, char type);

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
}
#endif
