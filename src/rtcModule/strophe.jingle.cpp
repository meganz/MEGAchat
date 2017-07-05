
/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "karereCommon.h"
#include "base/services.h"
#include "webrtcAdapter.h"
#include "strophe.jingle.session.h"
#include "strophe.jingle.h"
#include "stringUtils.h"
#include <mstrophepp.h>
#include <serverListProvider.h>
#include "rtcModule.h"
#include "IRtcModule.h"
#include "ICryptoFunctions.h"
#include "rtcmPrivate.h"

using namespace std;
using namespace promise;
using namespace std::placeholders;

namespace rtcModule
{
using namespace std;
using namespace strophe;
using namespace karere;

AvFlags peerMediaToObj(const char* strPeerMedia);
//==

Jingle::Jingle(karere::Client *karereClient, xmpp_conn_t* conn, IGlobalEventHandler* globalHandler,
               ICryptoFunctions* crypto, const char* iceServers)
:mConn(conn), mGlobalHandler(globalHandler), mCrypto(crypto),
  mTurnServerProvider(
    new TurnServerProvider(karereClient, "turn", iceServers, 3600)),
  mIceServers(new webrtc::PeerConnectionInterface::IceServers)
{
    pcConstraints.SetMandatoryReceiveAudio(true);
    pcConstraints.SetMandatoryReceiveVideo(true);
    pcConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);
    registerDiscoCaps();
    mConn.addHandler(std::bind(&Jingle::onJingle, this, _1),
                     "urn:xmpp:jingle:1", "iq", "set");
    mConn.addHandler([this](Stanza stanza, void* user, bool& keep)
    {
        onIncomingCallMsg(stanza);
    },
    nullptr, "message", "megaCall");
    //preload ice servers to make calls faster
    mTurnServerProvider->getServers()
    .then([this](ServerList<TurnServerInfo>* servers)
    {
        setIceServers(*servers);
    });
}

void Jingle::discoAddFeature(const char* feature)
{
    mGlobalHandler->discoAddFeature(feature);
}

void Jingle::addAudioCaps()
{
    discoAddFeature("urn:xmpp:jingle:apps:rtp:audio");
}
void Jingle::addVideoCaps()
{
    discoAddFeature("urn:xmpp:jingle:apps:rtp:video");
}
void Jingle::registerDiscoCaps()
{
    // http://xmpp.org/extensions/xep-0167.html#support
    // http://xmpp.org/extensions/xep-0176.html#support
    discoAddFeature("urn:xmpp:jingle:1");
    discoAddFeature("urn:xmpp:jingle:apps:rtp:1");
    discoAddFeature("urn:xmpp:jingle:transports:ice-udp:1");
    discoAddFeature("urn:ietf:rfc:5761"); // rtcp-mux

    //this.connection.disco.addNode('urn:ietf:rfc:5888', {}); // a=group, e.g. bundle
    //this.connection.disco.addNode('urn:ietf:rfc:5576', {}); // a=ssrc
    auto& devices = deviceManager.inputDevices();
    bool hasAudio = !devices.audio.empty() && !(mediaOptions & DISABLE_MIC);
    bool hasVideo = !devices.video.empty() && !(mediaOptions & DISABLE_CAM);
    if (hasAudio)
        addAudioCaps();
    if (hasVideo)
        addVideoCaps();
}
void Jingle::onConnState(const xmpp_conn_event_t status,
    const int error, xmpp_stream_error_t * const stream_error)
{
    try
    {
        if (status == XMPP_CONN_FAIL)
        {
            string msg("error code: "+to_string(error));
            if (stream_error)
            {
                if (stream_error->stanza)
                    msg.append(", Stanza:\n").append(strophe::Stanza(stream_error->stanza).dump().c_str());
                else if (stream_error->type)
                    msg.append(", Type: ").append(to_string(stream_error->type));
            }
            KR_LOG_DEBUG("XMPP disconnected: %s", msg.c_str());
        }
    }
    catch(exception& e)
    {
        KR_LOG_ERROR("Exception in connection state handler: %s", e.what());
    }
}
void Jingle::onJingle(Stanza iq)
{
   shared_ptr<Call> call; //declare it here because we need it in teh catch() handler
   try
   {
        const char* from = iq.attr("from");
        Stanza jingle = iq.child("jingle");
        const char* sid = jingle.attr("sid");
        auto callIt = find(sid);
        if (callIt == end())
            throw std::runtime_error("Could not find call for incoming jingle stanza with sid "+ std::string(sid));

        call = callIt->second;
        if (call->mPeerJid != from)
            throw std::runtime_error("onJingle: sid and sender full JID mismatch: expected jid: '"
                +call->mPeerJid+"', actual: '"+from+"'");

        auto sess = call->mSess;
        if (!sess)
            throw std::runtime_error("Jingle packet received, but the call has no session created");
        if (sess->inputQueue.get())
        {
            sess->inputQueue->push_back(iq);
            return;
        }
        std::string action = jingle.attr("action");
        JINGLE_LOG_INFO("onJingle '%s' from '%s'", action.c_str(), from);

        // send ack first
        Stanza ack(mConn);
        ack.setName("iq") //type will be set before sending, depending on the error flag
           .setAttr("from", mConn.fullJid())
           .setAttr("to", from)
           .setAttr("id", iq.attr("id"))
           .setAttr("type", "result");
        mConn.send(ack);

        // see http://xmpp.org/extensions/xep-0166.html#concepts-session
        if (action == "session-initiate")
        {
            if (call->state() != kCallStateInReqWaitJingle)
                throw std::runtime_error("Unexptected session-initiate received");
            KR_LOG_DEBUG("received INITIATE from %s", from);
// Verify SRTP fingerprint
            if (call->mOwnFprMacKey.empty())
                throw runtime_error("No ans.ownFrpMacKey present, there is a bug");
            if (!verifyMac(getFingerprintsFromJingle(jingle), call->mOwnFprMacKey, jingle.attr("fprmac")))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                call->hangup(Call::kFprVerifFail);
                return;
            }
//===
            call->setState(kCallStateSession);
            RTCM_EVENT(call, onSession);

            sess->inputQueue.reset(new StanzaQueue());
            sess->answer(jingle)
            .then([this, call]()
            {
//now handle all packets queued up while we were waiting for user's accept of the call
                processAndDeleteInputQueue(*call->mSess);
            })
            .fail([this, call](const promise::Error& e) mutable
             {//TODO: Can be also a protocol error
                  call->hangup(Call::kInternalError, e.msg().c_str());
             });
        }
        else if (action == "session-accept")
        {
            if (call->state() != kCallStateOutReq)
                throw std::runtime_error(std::string("Call is not in outgoing request state (state=")
                    +std::to_string(call->state())+"), but session-accept received");
            call->setState(kCallStateSession); //we should not do it before we send the jingle handshake

// Verify SRTP fingerprint
            const string& ownFprMacKey = call->mOwnFprMacKey;
            if (ownFprMacKey.empty())
                throw runtime_error("No session.ownFprMacKey present, there is a bug");

            if (!verifyMac(getFingerprintsFromJingle(jingle), ownFprMacKey, jingle.attr("fprmac")))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                call->hangup(Call::kFprVerifFail);
                return;
            }
// We are likely to start receiving ice candidates before setRemoteDescription()
// has completed, esp on Firefox, so we want to queue these and feed them only
// after setRemoteDescription() completes
            sess->inputQueue.reset(new StanzaQueue());
            sess->setRemoteDescription(jingle, "answer")
            .then([this, sess]()
            {
                if (sess->inputQueue)
                   processAndDeleteInputQueue(*sess);
            });
        }
        else if (action == "session-terminate")
        {
            TermCode code = Call::kUserHangup;
            Stanza::AutoText text;
            try
            {
                Stanza rsnNode = jingle.child("reason").firstChild();
                code = Call::strToTermcode(rsnNode.name());
                text = rsnNode.child("text").text();
            }
            catch(...){}
            code |= Call::kPeer;
            call->destroy(code, text?text.c_str():"", true);
        }
        else if (action == "transport-info")
        {
            sess->addIceCandidates(jingle);
        }
        else if (action == "session-info")
        {
            const char* affected = NULL;
            Stanza info;
//            if ((info = jingle.childByAttr("ringing", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true)))
//                RTCM_EVENT(call, onRinging);
            if ((info = jingle.childByAttr("mute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true)))
            {
                affected = info.attr("name");
                AvFlags av((strcmp(affected, "voice") == 0),
                           (strcmp(affected, "video") == 0));
                AvFlags& current = sess->mRemoteAvState;
                if (av.audio)
                    current.audio = false;
                if (av.video)
                    current.video = false;
                call->onPeerMute(av);
            }
            else if ((info = jingle.childByAttr("unmute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1")))
            {
                affected = info.attr("name");
                AvFlags av((strcmp(affected, "voice") == 0),
                           (strcmp(affected, "video") == 0));
                auto& current = sess->mRemoteAvState;
                current.audio |= av.audio;
                current.video |= av.video;
                call->onPeerUnmute(av);
            }
        }
        else
            KR_LOG_WARNING("Jingle action '%s' not implemented", action.c_str());
   }
   catch(exception& e)
   {
        const char* msg = e.what();
        if (!msg)
            msg = "(no message)";
        KR_LOG_ERROR("Exception in onJingle handler: %s", msg);
        if (call)
            call->hangup(Call::kProtoError, msg);
   }
}
/* Incoming call request with a message stanza of type 'megaCall' */
void Jingle::onIncomingCallMsg(Stanza callmsg)
{
    struct State: public ICallAnswer
    {
        Jingle& self;
        bool handledElsewhere = false;
        xmpp_uid elsewhereHandlerId = 0;
        xmpp_uid cancelHandlerId = 0;
        string sid;
        string from;
        string ownJid;
        int64_t tsReceived = -1;
        string fromBare;
        string ownFprMacKey;
        void* userp = nullptr;
        Stanza callmsg;
        Promise<void> pmsCrypto;
        Promise<void> pmsGelb;
        bool handlersFreed = false; //set to true when the stanza handlers have been freed
        bool userResponded = false; //set to true when user answers or rejects the call
        std::shared_ptr<Call> mCall;
        AvFlags mPeerMedia;
        std::shared_ptr<CallAnswerFunc> ansFunc;
        //ICallAnswer interface
        AvFlags peerMedia() const { return mPeerMedia; } //TODO: implement peer media parsing
        bool answeredReqStillValid() const
        {
            if (mCall->state() != kCallStateInReq)
                return false;
            int64_t tsTillUser = tsReceived + self.callAnswerTimeout+10000;
            return (timestampMs() < tsTillUser);
        }
        bool reqStillValid() const
        {
            return (!userResponded && answeredReqStillValid());
        }
        bool answer(bool accept, AvFlags av)
        {
            return (*ansFunc)(accept, av);
        }
        std::set<std::string>* files() const { return nullptr; }
        std::shared_ptr<ICall> call() const { return std::static_pointer_cast<ICall>(mCall); }
        //==
        bool freeHandlers(xmpp_uid* handler=nullptr)
        {
            if (handlersFreed)
                return false;
            handlersFreed = true;
            if (handler)
                *handler = 0;
            if (cancelHandlerId)
            {
                self.mConn.removeHandler(cancelHandlerId);
                cancelHandlerId = 0;
            }
            if (elsewhereHandlerId)
            {
                self.mConn.removeHandler(elsewhereHandlerId);
                elsewhereHandlerId = 0;
            }
            return true;
        }
        State(Jingle& aSelf): self(aSelf){}
    };
    shared_ptr<State> state = make_shared<State>(*this);

    state->callmsg = callmsg;
    state->sid = callmsg.attr("sid");
    if (find(state->sid) != end())
        throw runtime_error("onIncomingCallMsg: Call with sid '"+string(state->sid)+"' already exists");
    state->from = callmsg.attr("from");
    state->ownJid = callmsg.attr("to");
    if (state->ownJid.find('/') == std::string::npos)
    {
        assert(state->ownJid == getBareJidFromJid(mConn.fullJid()));
        state->ownJid = mConn.fullJid();
    }
    state->fromBare = getBareJidFromJid(state->from);
    state->tsReceived = timestampMs();

    try
    {
    // Add a 'handled-elsewhere' handler that will invalidate the call request if a notification
    // is received that another resource answered/declined the call
        state->elsewhereHandlerId = mConn.addHandler([this, state]
         (Stanza msg, void*, bool& keep)
         {
            keep = false;
            if (!state->freeHandlers(&state->elsewhereHandlerId))
                return;
            const char* by = msg.attr("by");
            if (strcmp(by, mConn.fullJid())) //if it wasn't us
            {
                destroyBySid(state->sid, strcmp(msg.attr("accepted"), "1")
                    ? Call::kRejectedElsewhere : Call::kAnsweredElsewhere, by);
            }
         },
         NULL, "message", "megaNotifyCallHandled", state->from.c_str(), nullptr, STROPHE_MATCH_BAREJID);

    // Add a 'cancel' handler that will invalidate the call request if the caller sends a cancel message
        state->cancelHandlerId = mConn.addHandler([this, state]
         (Stanza stanza, void*, bool& keep)
         {
            keep = false;
            if (!state->freeHandlers(&state->cancelHandlerId))
                return;
            const char* reason = stanza.attrOrNull("reason");
            if (!reason)
                reason = "unknown";
            destroyBySid(state->sid, Call::kCallReqCancel|Call::kPeer);
        },
        NULL, "message", "megaCallCancel", state->from.c_str(), nullptr, STROPHE_MATCH_BAREJID);

        setTimeout([this, state]()
        {
            if (!state->freeHandlers()) //cancel message was received and handler was removed
                return;
            destroyBySid(state->sid, Call::kAnswerTimeout);
        },
        callAnswerTimeout+10000);

//tsTillUser measures the time since the req was received till the user answers it
//After the timeout either the handlers will be removed (by the timer above) and the user
//will get onCallCanceled, or if the user answers at that moment, they will get
//a call-not-valid-anymore condition

        state->mPeerMedia = AvFlags(false,false); //TODO: Implement peerMedia parsing
        auto hangupFunc = [this, state](TermCode termcode, const std::string& text)->bool
        {
            if (!state->reqStillValid()) // Call was cancelled, or request timed out and handler was removed
                return false;
            state->userResponded = true;
            state->freeHandlers();
            Stanza declMsg(mConn);
            declMsg.setName("message")
                   .setAttr("sid", state->sid.c_str())
                   .setAttr("to", state->from.c_str())
                   .setAttr("type", "megaCallDecline")
                   .setAttr("reason", Call::termcodeToReason(termcode).c_str());

            if (!text.empty())
                declMsg.c("body").t(text.c_str());
            mConn.send(declMsg);
            return true;
        };

// Notify about incoming call
        state->ansFunc = std::make_shared<CallAnswerFunc>(
        [this, state](bool accept, AvFlags av)->bool
        {
// If dialog was displayed for too long, the peer timed out waiting for response,
// or user was at another client and that other client answred.
// When the user returns at this client he may see the expired dialog asking to accept call,
// and will answer it, but we have to ignore it because it's no longer valid
            if (!accept)
                return state->mCall->hangup();
//accept call
            if (!state->reqStillValid()) // Call was cancelled, or request timed out and handler was removed
                return false; //the callback returning false signals to the calling user code that the call request is not valid anymore

            state->userResponded = true;
            when(state->pmsCrypto, state->pmsGelb)
            .then([this, state, av]()
            {
                if (!state->answeredReqStillValid())
                    return;
                auto it = find(state->sid);
                if (it == end())
                    return; //call was deleted meanwhile
                auto& call = it->second;
                if (call->state() != kCallStateInReq)
                    throw std::runtime_error("BUG: Call transitioned to another state while waiting for gelb and crypto");

                state->ownFprMacKey = mCrypto->generateFprMacKey();
                auto peerFprMacKey = mCrypto->decryptMessage(state->callmsg.attr("fprmackey"));
                if (peerFprMacKey.empty())
                    throw std::runtime_error("Faield to verify peer's fprmackey from call request");
// tsTillJingle measures the time since we sent megaCallAnswer till we receive jingle-initiate
                int64_t tsTillJingle = timestampMs()+mJingleAutoAcceptTimeout;
                call->mPeerFprMacKey = peerFprMacKey;
                call->mOwnFprMacKey = state->ownFprMacKey;
                call->mPeerAnonId = state->callmsg.attr("anonid");
                call->mLocalAv = av;
                if (!call->startLocalStream(true))
                    return; //startLocalStream() hangs up the call
//TODO: Handle file transfer
// This timer is for the period from the megaCallAnswer to the jingle-initiate stanza
                setTimeout([this, state, tsTillJingle]()
                { //invalidate auto-answer after a timeout
                    GET_CALL(state->sid, kCallStateInReqWaitJingle, return); //initiate was sent and we answered the call
                    //It's possible that we have received or will receive a cancel from the
                    //peer, and since we remove the call asynchronously,
                    //in that case the call will receive both cancel and hangup.
                    call->destroy(Call::kInitiateTimeout);
                }, mJingleAutoAcceptTimeout);

                Stanza ans(mConn);
                ans.setName("message")
                   .setAttr("sid", state->sid.c_str())
                   .setAttr("to", state->from.c_str())
                   .setAttr("type", "megaCallAnswer")
                   .setAttr("fprmackey",
                       mCrypto->encryptMessageForJid(
                                state->ownFprMacKey, state->fromBare))
                   .setAttr("anonid", mOwnAnonId.c_str())
                   .setAttr("media", call->mLocalAv.toString().c_str());
                mConn.send(ans);

                call->setState(kCallStateInReqWaitJingle);
                //the peer must create a session immediately after receiving our answer
                //and before reading any other data from network, otherwise it may not have
                //as session when we send the below terminate
                call->createSession(state->from, state->ownJid, nullptr);
            })
            .fail([this, state](const Error& err)
            {
                auto msg = "Error during pre-jingle call answer response: "+err.msg();
                if (state->userResponded)
                    hangupBySid(state->sid, Call::kInternalError, msg);
                else //we don't want to reject the call from this client if user hasn't interacted
                    destroyBySid(state->sid, Call::kInternalError, msg);
            });
            return true;
        }); //end answer func

        state->pmsGelb = mTurnServerProvider->getServers(mIceFastGetTimeout)
        .then([this, state](ServerList<TurnServerInfo>* servers)
        {
            setIceServers(*servers);
        });
        state->pmsCrypto = mCrypto->preloadCryptoForJid(state->fromBare);

        auto& call = state->mCall = addCall(kCallStateInReq, false, nullptr, state->sid,
            std::move(hangupFunc), state->from, AvFlags(true, true));
        RTCM_LOG_EVENT("global(%s)->onIncomingCallRequest", state->sid.c_str());
        call->mHandler = mGlobalHandler->onIncomingCallRequest(
            static_pointer_cast<ICallAnswer>(state));
        if (!call->mHandler)
            throw std::runtime_error("onIncomingCallRequest: Application did not provide call event handler");
    }
    catch(exception& e)
    {
        KR_LOG_ERROR("Exception in onIncomingCallMsg handler: %s", e.what());
        auto it = find(state->sid);
        if (it != end())
            it->second->hangup(Call::kInternalError, (std::string("Exception during call accept: ")+e.what()).c_str());
    }
}

bool Jingle::hangupBySid(const std::string& sid, TermCode termcode, const std::string& text)
{
    auto it = find(sid);
    if (it == end())
        return false;
    it->second->hangup(termcode, text);
    return true;
}

bool Jingle::destroyBySid(const std::string& sid, TermCode termcode, const std::string& text)
{
    auto it = find(sid);
    if (it == end())
        return false;
    it->second->destroy(termcode, text);
    return true;
}

bool Jingle::cancelIncomingReqCall(iterator it, TermCode termcode, const std::string& text)
{
    auto& call = it->second;
//    if (call->mIsFt && type && (type != 'f'))
//        return false;
    return call->hangup(termcode, text);
}
void Jingle::processAndDeleteInputQueue(JingleSession& sess)
{
    unique_ptr<StanzaQueue> queue(sess.inputQueue.release());
    for (auto& stanza: *queue)
        onJingle(stanza);
}

JingleCall::JingleCall(RtcModule& aRtc, bool isCaller, CallState aState,
    IEventHandler* aHandler, const std::string& aSid, HangupFunc &&hangupFunc,
    const std::string& aPeerJid, AvFlags localAv, bool aIsFt,
    const std::string& aOwnJid)
: ICall(aRtc, isCaller, aState, aHandler, aSid, aPeerJid, aIsFt, aOwnJid),
  mHangupFunc(std::forward<HangupFunc>(hangupFunc)), mLocalAv(localAv)
{
    if (!mHandler && mIsCaller) //handler is set after creation when we answer
        throw std::runtime_error("Call::Call: NULL user handler passed");
}

//called by startMediaCall() in rtcModule.cpp
void JingleCall::initiate()
{
    if (!mSess)
        throw std::runtime_error("Call::initiate: Call does not have a session, call createSession() first");
// create and initiate a new jinglesession to peerjid
    mSess->initiate(true);
    mSess->sendOffer()
    .then([this](Stanza)
    {
        mSess->sendMuteDelta(AvFlags(true,true), mLocalAv);
    })
    .fail([](const promise::Error& err)
    {
        RTCM_LOG_ERROR("Error sending offer: %s", err.what());
    });

    RTCM_EVENT(this, onSession);
}

void JingleCall::createPcConstraints()
{
    if (pcConstraints)
        return;
    pcConstraints.reset(new webrtc::FakeConstraints);
    for (auto& constr: rtc().pcConstraints.GetMandatory())
    {
        pcConstraints->AddMandatory(constr.key, constr.value);
    }
    for (auto& constr: rtc().pcConstraints.GetOptional())
    {
        pcConstraints->AddOptional(constr.key, constr.value);
    }
}

void setConstraint(webrtc::FakeConstraints& constr, const string &name, const std::string& value,
    bool optional)
{
    if (optional)
    {
        //TODO: why webrtc has no SetOptional?
        auto& optional = (webrtc::MediaConstraintsInterface::Constraints&)(constr.GetOptional());
        auto it = optional.begin();
        for (; it != optional.end(); it++)
        {
            if (it->key == name)
            {
                it->value = value;
                break;
            }
        }
        if (it == optional.end())
        {
            constr.AddOptional(name, value);
        }
    }
    else
    {
        constr.SetMandatory(name, value);
    }
}

void JingleCall::setPcConstraint(const string &name, const string &value, bool optional)
{
    if (!pcConstraints)
    {
        createPcConstraints();
    }
    rtcModule::setConstraint(*pcConstraints, name, value, optional);
}

void Jingle::setMediaConstraint(const string& name, const string &value, bool optional)
{
    rtcModule::setConstraint(mediaConstraints, name, value, optional);
}
void Jingle::setPcConstraint(const string& name, const string &value, bool optional)
{
    rtcModule::setConstraint(pcConstraints, name, value, optional);
}


void Jingle::hangupAll(TermCode termcode, const std::string& text)
{
    while(begin() != end())
    {
        begin()->second->hangup(termcode, text, false);
    }
}
void Jingle::destroyAll(TermCode termcode, const std::string& text)
{
    while(begin() != end())
        begin()->second->destroy(termcode, text);
}
std::shared_ptr<ICall> Jingle::getCallBySid(const string& sid)
{
    auto it = find(sid);
    if (it == end())
        return nullptr;
    else
        return static_pointer_cast<ICall>(it->second);
}

string Jingle::getFingerprintsFromJingle(Stanza j)
{
    vector<Stanza> nodes;
    j.forEachChild("content", [this, &nodes](Stanza content)
    {
        content.forEachChild("transport", [this, &nodes](Stanza transport)
        {
            transport.forEachChild("fingerprint", [this, &nodes](Stanza fingerprint)
            {
                nodes.push_back(fingerprint);
            });
        });
    });
    if (nodes.size() < 1)
        throw runtime_error("Could not extract SRTP fingerprint from jingle packet");
    vector<string> fps;
    for (Stanza node: nodes)
    {
        fps.push_back(string(node.attr("hash"))+" "+node.text().c_str());
    }
    std::sort(fps.begin(), fps.end());
    string result;
    result.reserve(256);
    for (auto& item: fps)
        result.append(item)+=';';
    if (result.size() > 0)
        result.resize(result.size()-1);
    return result;
}


bool Jingle::verifyMac(const std::string& msg, const std::string& key, const std::string& actualMac)
{
    if (actualMac.empty())
        return false;
    string expectedMac;
    try
    {
        expectedMac = crypto().generateMac(msg, key).c_str();
    }
    catch(...)
    {
        return false;
    }
    // constant-time compare
    auto aLen = actualMac.size();
    auto eLen = expectedMac.size();
    auto len = (aLen<eLen)?aLen:eLen;
    bool match = (aLen == eLen);
    for (size_t i=0; i < len; i++)
        match &= (expectedMac[i] == actualMac[i]);

    return match;
}

int Jingle::setIceServers(const ServerList<TurnServerInfo>& servers)
{
    webrtc::PeerConnectionInterface::IceServers rtcServers;
    webrtc::PeerConnectionInterface::IceServer rtcServer;
    for (auto& server: servers)
    {
        rtcServer.uri = server->url;
        if (!server->user.empty())
            rtcServer.username = server->user;
        else
            rtcServer.username = KARERE_TURN_USERNAME;
        if (!server->pass.empty())
            rtcServer.password = server->pass;
        else
            rtcServer.password = KARERE_TURN_PASSWORD;
        KR_LOG_DEBUG("Adding ICE server: '%s'", rtcServer.uri.c_str());
        rtcServers.push_back(rtcServer);
    }
    mIceServers->swap(rtcServers);
    return (int)(mIceServers->size());
}

AvFlags peerMediaToObj(const char* strPeerMedia)
{
    AvFlags ret(false,false);
    for (; *strPeerMedia; strPeerMedia++)
    {
        char ch = *strPeerMedia;
        if (ch == 'a')
            ret.audio = true;
        else if (ch == 'v')
            ret.video = true;
    }
    return ret;
}

RTCM_API IEventHandler* ICall::changeEventHandler(IEventHandler *handler)
{
    auto save = mHandler;
    mHandler = handler;
    return save;
}
const std::string& ICall::ownAnonId() const
{
    return mRtc.ownAnonId();
}
static const char* sTermcodeMsgs[Call::kTermLast+1] =
{
    "User hangup",
    "Call request was canceled",
    "Call was answered on another device",
    "Call was rejected on another device",
    "Timed out waiting for answer",
    "Reserved1", "Reserved2",
    "Timed out waiting for peer to initiate call",
    "Api server response timeout",
    "Identity fingerprint verification error, possible forge attempt",
    "Protocol timeout",
    "Protocol error",
    "Internal error",
    "No camera and/or audio access",
    "Disconnected from XMPP server"
};

static const char* sTermcodeReasons[Call::kTermLast+1] =
{
    "hangup", "call-canceled", "answered-elewhere", "rejected-elsewhere",
    "answer-timeout", "reserved1", "reserved2",
    "initiate-timeout", "api-timeout", "jingle-timeout",
    "jingle-error", "internal-error", "security", "nomedia", "xmpp-disconnect"
};

static std::map<std::string, TermCode> sReasonNames =
{
  {"hangup", Call::kUserHangup}, {"call-canceled", Call::kCallReqCancel},
  {"handled-elsewhere", Call::kAnsweredElsewhere},
  {"rejected-elsewhere", Call::kRejectedElsewhere},
  {"answer-timeout", Call::kAnswerTimeout},
  {"app-terminating", Call::kAppTerminating},
  {"initiate-timeout", Call::kInitiateTimeout},  {"api-timeout", Call::kApiTimeout},
  {"jingle-timeout", Call::kProtoTimeout}, {"jingle-error", Call::kProtoError},
  {"internal-error", Call::kInternalError}, {"security", Call::kFprVerifFail},
  {"nomedia", Call::kNoMediaError}, {"xmpp-disconnect", Call::kXmppDisconnError}
};

RTCM_API TermCode ICall::strToTermcode(std::string event)
{
    TermCode isPeer;
    if ((event.size() > 5) && (event.compare(0, 5, "peer-") == 0))
    {
        event.erase(0, 5);
        isPeer = 128;
    }
    else
    {
        isPeer = 0;
    }
    auto it = sReasonNames.find(event);
    if (it == sReasonNames.end())
    {
        KR_LOG_ERROR("Unknown term reason '%s'", event.c_str());
        return kProtoError | isPeer;
    }
    return it->second | isPeer;
}

RTCM_API std::string ICall::termcodeToReason(TermCode event)
{
    std::string result;
    if (event & kPeer)
    {
        result.append("peer-", 5);
        event &= ~kPeer;
    }
    if (event > kTermLast)
        return result.append("error");
    else
        return result.append(sTermcodeReasons[event]);
}

RTCM_API const char* ICall::termcodeToMsg(TermCode event)
{
    event &= ~kPeer;
    if (event > kTermLast)
        return "(Unknown)";
    else
        return sTermcodeMsgs[event];
}

ScopedCallRemover::ScopedCallRemover(const JingleCall &call, TermCode aCode)
    :rtc(call.rtc()), sid(call.sid()), code(aCode){}
ScopedCallHangup::~ScopedCallHangup()
{
    rtc.hangupBySid(sid, code, text);
}
ScopedCallDestroy::~ScopedCallDestroy()
{
    rtc.destroyBySid(sid, code, text);
}
}
