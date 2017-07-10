#ifndef STROPHE_JINGLE_H
#define STROPHE_JINGLE_H

/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "webrtcAdapter.h"
#include "IRtcModule.h"
#include <mstrophepp.h>
#include "karereCommon.h"
#include <serverListProviderForwards.h>

namespace disco
{
    class DiscoPlugin;
}


namespace rtcModule
{
class JingleSession;
class RtcModule;
class IEventHandler;
class IGlobalEventHandler;
//TODO: Implement
class FileTransferHandler;

struct AnonId
{
    uint64_t u64;
    uint32_t u32;
    AnonId(const AnonId& other): u64(other.u64), u32(other.u32){}
    bool operator==(const AnonId& other) const = default;
    bool operator<(const AnonId& other) const
    {
        if (u64 == other.u64)
            return u32 < other.u32;
        else
            return u64 < other.u64;
    }
};

struct SessionKey
{
    karere::Id sid;
    AnonId caller;
    AnonId answerer;
    SessionKey(karere::Id aSid, const AnonId& clr, const AnonId& ans)
        :sid(aSid), caller(clr), answerer(ans){}
    bool operator<(const SessionKey& other) const
    {
        if (sid != other.sid)
            return sid < other.sid;
        if (caller != other.caller)
            return caller < other.caller;
        return answerer < other.answerer;
    }
};

struct JingleCall: public ICall
{
protected:
    typedef std::function<bool(TermCode, const std::string&)> HangupFunc;
    AvFlags mLocalAv;
    std::shared_ptr<JingleSession> mSess;
    std::string mOwnFprMacKey;
    std::string mPeerFprMacKey;
    std::unique_ptr<webrtc::FakeConstraints> pcConstraints;

    JingleCall(RtcModule& aRtc, bool isCaller, CallState aState, IEventHandler* aHandler,
         const std::string& aSid, HangupFunc&& hangupFunc, const std::string& aPeerJid,
         AvFlags localAv, bool aIsFt, const std::string& aOwnJid);
    void initiate();
    void createPcConstraints();
    friend class Jingle;
    friend class JingleSession;
    friend class RtcModule;
public:
    virtual ~JingleCall(){ KR_LOG_DEBUG("Call %s destroy", mSid.c_str()); } //we shouldn't need it virtual, as the map type is of the actual class Call, but just in case
    void setState(CallState newState) { mHangupFunc = nullptr; mState = newState; }
    virtual void setPcConstraint(const std::string& name, const std::string& value, bool optional=false);
};

class ScopedCallRemover
{
protected:
    RtcModule& rtc;
    std::string sid;
    TermCode code;
    ScopedCallRemover(RtcModule& aRtc, const std::string& aSid,
            TermCode aCode=JingleCall::kUserHangup)
            : rtc(aRtc), sid(aSid), code(aCode){}
    ScopedCallRemover(const JingleCall& call, TermCode aCode);
public:
    std::string text;
};
class ScopedCallDestroy: public ScopedCallRemover
{
    ScopedCallDestroy(ScopedCallDestroy&) = delete;
public:
    ScopedCallDestroy(RtcModule& aRtc, const std::string& aSid,
        TermCode aCode=JingleCall::kUserHangup): ScopedCallRemover(aRtc,aSid,aCode){}
    ScopedCallDestroy(const JingleCall& call, TermCode aCode=JingleCall::kUserHangup)
        :ScopedCallRemover(call, aCode){}
    ~ScopedCallDestroy(); //has to be defined in .cpp file as we can't load rtcModule.h here
};
class ScopedCallHangup: public ScopedCallRemover
{
    ScopedCallHangup(const ScopedCallHangup&) = delete;
public:
    ScopedCallHangup(RtcModule& aRtc, const std::string& aSid,
        TermCode aCode=JingleCall::kUserHangup)
        : ScopedCallRemover(aRtc, aSid, aCode){}
    ScopedCallHangup(const JingleCall& call, TermCode aCode=JingleCall::kUserHangup)
    :ScopedCallRemover(call, aCode){}
    ~ScopedCallHangup(); //has to be defined in .cpp file as we can't load rtcModule.h here
};

enum ErrorType
{
    ERR_UNKNOWN = 0,
    ERR_INTERNAL = 1,
    ERR_API = 2,
    ERR_PROTOCOL = 3
};

class Call;
class ICryptoFunctions;

class Jingle: public std::map<std::string, std::shared_ptr<Call>>, public IRtcModule
{
protected:
/** Contains all info about an incoming call that has been accepted at the message level and needs to be autoaccepted at the jingle level */
    strophe::Connection mConn;
    IGlobalEventHandler* mGlobalHandler;
    bool mHandlersInitialized = false; //used in conn state handler to initialize only on first connect
/** Timeout after which if an iq response is not received, an error is generated */
    int mJingleTimeout = 50000;
/** The period, during which an accepted call request will be valid
* and the actual call answered. The period starts at the time the
* request is received from the network */
    int mJingleAutoAcceptTimeout = 15000;
/** The period within which an outgoing call can be answered by peer */
    int callAnswerTimeout = 50000;
    unsigned mIceFastGetTimeout = 2000;
    std::unique_ptr<ICryptoFunctions> mCrypto;
    std::string mOwnAnonId;
    typedef karere::FallbackServerProvider<karere::TurnServerInfo> TurnServerProvider;
    std::unique_ptr<TurnServerProvider> mTurnServerProvider;
    void discoAddFeature(const char* feature);
    bool destroyBySid(const std::string& sid, TermCode termcode, const std::string& text="");
    bool hangupBySid(const std::string& sid, TermCode termcode, const std::string& text="");
    std::shared_ptr<ICall> getCallBySid(const std::string& sid);
    friend class ScopedCallDestroy; friend class ScopedCallHangup;
    friend class JingleSession;
public:
    enum MediaOptions {DISABLE_MIC = 1, DISABLE_CAM = 2};
    int mediaOptions = 0;

    std::shared_ptr<webrtc::PeerConnectionInterface::IceServers> mIceServers;
    artc::DeviceManager deviceManager;

    webrtc::FakeConstraints mediaConstraints;
    webrtc::FakeConstraints pcConstraints;
    ICryptoFunctions& crypto() {return *mCrypto;}
    const std::string& ownAnonId() const { return mOwnAnonId; }
//==
/** @param iceServers the static fallback server list in json form:
 *  {"host":"turn:host:port?protocol=tcp/udp", "user":<username>,"pass": <password>}
 */
    Jingle(MyMegaApi *api, xmpp_conn_t* conn, IGlobalEventHandler* globalHandler, ICryptoFunctions* crypto,
           const char* iceServers="");
    strophe::Connection& conn() { return mConn; }
    virtual std::shared_ptr<Call>& addCall(CallState aState,
        bool aIsCaller, IEventHandler* aHandler, karere::Id aSid,
        karere::Id peer, chatd::ClientId peerClient, AvFlags localAv, AvFlags removeAv) = 0;
    virtual std::shared_ptr<Call>& addIncomingCall(const RtMessageWithEndpoint& sdpOffer);

    void addAudioCaps();
    void addVideoCaps();
    void registerDiscoCaps();
    int setIceServers(const karere::ServerList<karere::TurnServerInfo>& servers);
 //plugin connection state handler
    void onConnState(const xmpp_conn_event_t status,
        const int error, xmpp_stream_error_t * const stream_error);
    void onJingle(strophe::Stanza iq);
/** Incoming call request with a message stanza of type 'megaCall' */
    void onIncomingCallMsg(strophe::Stanza callmsg);
    bool cancelIncomingReqCall(iterator it, TermCode code, const std::string& text="");
    void processAndDeleteInputQueue(JingleSession& sess);
    promise::Promise<std::shared_ptr<JingleSession> > initiate();
    void hangupAll(TermCode code, const std::string& text="");
    void destroyAll(TermCode code, const std::string& text="");
    promise::Promise<strophe::Stanza> sendIq(strophe::Stanza iq, const std::string& origin,
                                             const char* sid=nullptr);
    std::string getFingerprintsFromJingle(strophe::Stanza j);
    bool verifyMac(const std::string& msg, const std::string& key, const std::string& actualMac);
    virtual void setMediaConstraint(const std::string& key, const std::string& value, bool optional=false);
    virtual void setPcConstraint(const std::string& key, const std::string& value, bool optional=false);
};

}

#endif
