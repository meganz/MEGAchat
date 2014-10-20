
/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "webrtcAdapter.h"
//#include "strophe.jingle.session.h"
#include <mstrophepp.h>

namespace karere
{
namespace rtcModule
{
class JingleSession;
//TODO: Implement
class FileTransferHandler;
class DiscoPlugin;

struct AnswerOptions
{
    artc::tspMediaStream localStream;
    AvFlags muted;
};

class Jingle: strophe::Plugin
{
protected:
/** Contains all info about a not-yet-established session, when onCallTerminated is fired and there is no session yet */
    struct NoSessionInfo
    {
        const char* sid = NULL;
        const char* peer = NULL;
        bool isInitiator=false;
    };
/** Contains all info about an incoming call that has been accepted at the message level and needs to be autoaccepted at the jingle level */
    struct AutoAcceptCallInfo: public StringMap
    {
        Ts tsReceived;
        Ts tsTillJingle;
        std::shared_ptr<AnswerOptions> options;
    };
    typedef std::map<std::string, AutoAcceptCallInfo> AutoAcceptMap;
    std::map<std::string, JingleSession*> mSessions;
    webrtc::FakeConstraints mMediaConstraints;
/** Timeout after which if an iq response is not received, an error is generated */
    int mJingleTimeout = 50000;
/** The period, during which an accepted call request will be valid
* and the actual call answered. The period starts at the time the
* request is received from the network */
    int mJingleAutoAcceptTimeout = 15000;
/** The period within which an outgoing call can be answered by peer */
    int callAnswerTimeout = 50000;
    std::map<std::string, std::shared_ptr<AutoAcceptCallInfo> >mAutoAcceptCalls;
public:
    enum {DISABLE_MIC = 1, DISABLE_CAM = 2, HAS_MIC = 4, HAS_CAM = 8};
    int mediaFlags = 0;
    std::shared_ptr<webrtc::PeerConnectionInterface::IceServers> mIceServers;
    std::shared_ptr<artc::InputDevices> mInputDevices;

//event handler interface
    virtual void onRemoteStreamAdded(JingleSession& sess, artc::tspMediaStream stream){}
    virtual void onRemoteStreamRemoved(JingleSession& sess, artc::tspMediaStream stream){}
    virtual void onJingleError(JingleSession& sess, const std::string& err, strophe::Stanza stanza, strophe::Stanza orig){}
    virtual void onJingleTimeout(JingleSession& sess, const std::string& err, strophe::Stanza orig){}
//    virtual void onIceConnStateChange(JingleSession& sess, event){}
    virtual void onIceComplete(JingleSession& sess){}
//rtcHandler callback interface, called by the connection.jingle object
    virtual void onIncomingCallRequest(const char* from,
     bool(*reqStillValid)(), void(*ansFunc)(bool)){}
    virtual void onCallCanceled(const char* peer, const char* event,
     const char* by, bool accepted){}
    virtual void onCallRequestTimeout(const char* peer) {}
    virtual void onCallAnswered(const char* peer) {}
    virtual void onCallTerminated(JingleSession* sess, const char* reason,
      const char* text, const NoSessionInfo* info=NULL){}
    virtual bool onCallIncoming(JingleSession& sess){return true;}
    virtual void onRinging(JingleSession& sess){}
    virtual void onMuted(JingleSession& sess, const AvFlags& affected){}
    virtual void onUnmuted(JingleSession& sess, const AvFlags& affected){}
    virtual void onInternalError(const std::string& msg, const char* where);
//==
    std::string generateHmac(const std::string& data, const std::string& key);
    Jingle(strophe::Connection& conn, const std::string& iceServers="");
    void addAudioCaps(DiscoPlugin& disco);
    void addVideoCaps(DiscoPlugin& disco);
    void registerDiscoCaps();
    void setIceServers(const std::string& servers){}
    void onConnState(const xmpp_conn_event_t status,
        const int error, xmpp_stream_error_t * const stream_error);
    static int _static_onJingle(xmpp_conn_t* const conn,
        xmpp_stanza_t* stanza, void* userdata);
    static int _static_onIncomingCallMsg(xmpp_conn_t* const conn,
        xmpp_stanza_t* stanza, void* userdata);
    bool onJingle(strophe::Stanza iq);
    /* Incoming call request with a message stanza of type 'megaCall' */
    void onIncomingCallMsg(strophe::Stanza callmsg);
    bool cancelAutoAcceptEntry(const char* sid, const char* reason,
        const char* text, char type);
    bool cancelAutoAnswerEntry(AutoAcceptMap::iterator it, const char* reason,
    const char* text, char type);
    void cancelAllAutoAcceptEntries(const char* reason, const char* text);
    void purgeOldAcceptCalls();
    void processAndDeleteInputQueue(JingleSession& sess);
    JingleSession* initiate(const char* sid, const char* peerjid, const char* myjid,
        artc::tspMediaStream sessStream, const AvFlags& mutedState,
        std::shared_ptr<StringMap> sessProps, FileTransferHandler* ftHandler);
    JingleSession* createSession(const char* me, const char* peerjid,
        const char* sid, artc::tspMediaStream, const AvFlags& mutedState,
        std::shared_ptr<StringMap> sessProps);
    void terminateAll(const char* reason, const char* text, bool nosend);
    bool terminateBySid(const char* sid, const char* reason, const char* text,
        bool nosend);
    bool terminate(JingleSession* sess, const char* reason, const char* text,
        bool nosend);
    promise::Promise<strophe::Stanza> sendTerminateNoSession(const char* sid,
        const char* to, const char* reason, const char* text);
        bool sessionIsValid(JingleSession* sess);
    std::string getFingerprintsFromJingle(strophe::Stanza j);
};
}
}
