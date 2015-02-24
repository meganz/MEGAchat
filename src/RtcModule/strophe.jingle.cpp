
/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "KarereCommon.h"
#include "base/services.h"
#include "WebrtcAdapter.h"
#include "strophe.jingle.session.h"
#include "strophe.jingle.h"
#include "StringUtils.h"
#include <mstrophepp.h>

using namespace std;
using namespace promise;
using namespace std::placeholders;

namespace rtcModule
{
using namespace std;
using namespace strophe;
using namespace karere;
using namespace mega; //In clang there is a class named mega, so using the mega:: qualifier causes a compile error about ambiguous identifier

AvFlags peerMediaToObj(const char* strPeerMedia);

void Jingle::onInternalError(const string& msg, const char* where)
{
    KR_LOG_ERROR("Internal error at %s: %s", where, msg.c_str());
}
//==

Jingle::Jingle(xmpp_conn_t* conn, ICryptoFunctions* crypto, const char* iceServers)
:mConn(conn), mCrypto(crypto)
{
    setIceServers(iceServers);
    mMediaConstraints.SetMandatoryReceiveAudio(true);
    mMediaConstraints.SetMandatoryReceiveVideo(true);
    mMediaConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);
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
    bool hasAudio = !devices.audio.empty() && !(mediaFlags & DISABLE_MIC);
    bool hasVideo = !devices.video.empty() && !(mediaFlags & DISABLE_CAM);
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
        if (status == XMPP_CONN_CONNECT)
        {
            registerDiscoCaps();
            mConn.addHandler(std::bind(&Jingle::onJingle, this, _1),
                             "urn:xmpp:jingle:1", "iq", "set");
//            mConn.addHandler(std::bind(&Jingle::onIncomingCallMsg, this, _1),
            mConn.addHandler([this](Stanza stanza, void* user, bool& keep)
            {
                onIncomingCallMsg(stanza);
            },
            nullptr, "message", "megaCall");
        }
        else if (status == XMPP_CONN_FAIL)
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
/*
int Jingle::_static_onJingle(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
{
    return static_cast<Jingle*>(userdata)->onJingle(stanza);
}
static int Jingle::_static_onIncomingCallMsg(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
{
    return static_cast<Jingle*>(userdata)->onIncomingCallMsg(stanza);
}
*/
void Jingle::onJingle(Stanza iq)
{
   try
   {
        Stanza jingle = iq.child("jingle");
        const char* sid = jingle.attr("sid");
        auto sit = mSessions.find(sid);
        shared_ptr<JingleSession> sess((sit == mSessions.end())
            ? nullptr
            : sit->second);

        if (sess && sess->inputQueue.get())
        {
            sess->inputQueue->push_back(iq);
            return;
        }
        const char* action = jingle.attr("action");
        // send ack first
        Stanza ack(mConn);
        ack.setName("iq") //type will be set before sending, depending on the error flag
           .setAttr("from", mConn.fullJid())
           .setAttr("to", iq.attr("from"))
           .setAttr("id", iq.attr("id"));

        bool error = false;
        KR_LOG_COLOR(34, "onJingle '%s' from '%s'", action, iq.attr("from"));

        if (strcmp(action, "session-initiate"))
        {

            if (!sess)
                error = true;
            // compare from to sess.peerjid (bare jid comparison for later compat with message-mode)
            // local jid is not checked
            else if (getBareJidFromJid(iq.attr("from")) != getBareJidFromJid(sess->peerJid()))
            {
                error = true;
                KR_LOG_WARNING("onJingle: JID mismatch for session '%s': ('%s' != '%s')", sess->sid().c_str(), iq.attr("from"), sess->peerJid().c_str());
            }
            if (error)
            {
                ack.c("error").setAttr("type", "cancel")
                   .c("item-not-found")
                        .setAttr("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas")
                        .parent()
                   .c("unknown-session")
                        .setAttr("xmlns", "urn:xmpp:jingle:errors:1");
            }
        }
        else if (sess) //action == session-initiate
        {
            error = true;
            // existing session with same session id
            // this might be out-of-order if the sess.peerjid is the same as from
            ack.c("error").setAttr("type", "cancel")
               .c("service-unavailable")
                    .setAttr("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
            KR_LOG_WARNING("onJingle: session-initiate for existing session [id='%s']", sid);
        }

        if (error)
        {
            ack.setAttr("type", "result");
            mConn.send(ack);
            return;
        }
        else
        {
            ack.setAttr("type", "result");
            mConn.send(ack);
        }

        // see http://xmpp.org/extensions/xep-0166.html#concepts-session
        if (strcmp(action, "session-initiate") == 0)
        {
            const char* peerjid = iq.attr("from");
            KR_LOG_DEBUG("received INITIATE from %s", peerjid);
            purgeOldAcceptCalls(); //they are purged by timers, but just in case
            auto ansIter = mAutoAcceptCalls.find(sid);
            if (ansIter == mAutoAcceptCalls.end())
            {
                KR_LOG_DEBUG("session-initiate received, but there is no autoaccept entry for this sid");
                return; //ignore silently - maybe there is no user on this client and some other client(resource) already accepted the call
            }
            shared_ptr<AutoAcceptCallInfo> spAns(ansIter->second);
            mAutoAcceptCalls.erase(ansIter);
            AutoAcceptCallInfo& ans = *spAns;
// Verify SRTP fingerprint
            const string& ownFprMacKey = ans["ownFprMacKey"];
            if (ownFprMacKey.empty())
                throw runtime_error("No ans.ownFrpMacKey present, there is a bug");
            if (!verifyMac(getFingerprintsFromJingle(jingle), ownFprMacKey, jingle.attr("fprmac")))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                try
                {
                    FakeSessionInfo info(jingle.attr("sid"), peerjid, mConn.fullJid(), false, ans["peerAnonId"]);
                    onCallTerminated(NULL, "security", "fingerprint verification failed", &info);
                }
                catch(...){}
                return;
            }
//===
            sess = createSession(iq.attr("to"), peerjid, sid,
               ans.options->localStream, ans.options->av, ans);

            sess->inputQueue.reset(new StanzaQueue());
            string reason, text;
            bool cont = onCallIncoming(*sess, reason, text);
            if (!cont)
            {
                terminate(sess.get(), reason.c_str(), text.c_str());
                sess->inputQueue.reset();
                return;
            }

            sess->initiate(false);

            // configure session
            sess->setRemoteDescription(jingle, "offer").then([sess](int)
            {
                return sess->sendAnswer();
            })
            .then([sess](int)
            {
                  return sess->sendAvState();
            })
            .then([this, sess](int)
            {
                onCallAnswered(*sess);
//now handle all packets queued up while we were waiting for user's accept of the call
                processAndDeleteInputQueue(*sess);
                return 0;
            })
            .fail([this, sess](const promise::Error& e) mutable
             {
                  sess->inputQueue.reset();
                  terminate(sess.get(), "error", e.msg().c_str());
                  return 0;
             });
        }
        else if (strcmp(action, "session-accept") == 0)
        {
// Verify SRTP fingerprint
            const string& ownFprMacKey = (*sess)["ownFprMacKey"];
            if (ownFprMacKey.empty())
                throw runtime_error("No session.ownFprMacKey present, there is a bug");

            if (!verifyMac(getFingerprintsFromJingle(jingle), ownFprMacKey, jingle.attr("fprmac")))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                terminateBySid(sess->sid().c_str(), "security", "fingerprint verification failed");
                return;
            }
// We are likely to start receiving ice candidates before setRemoteDescription()
// has completed, esp on Firefox, so we want to queue these and feed them only
// after setRemoteDescription() completes
            sess->inputQueue.reset(new StanzaQueue());
            sess->setRemoteDescription(jingle, "answer")
            .then([this, sess](int)
            {
                if (sess->inputQueue)
                   processAndDeleteInputQueue(*sess);
                return 0;
            });
        }
        else if (strcmp(action, "session-terminate") == 0)
        {
            const char* reason = NULL;
            Stanza::AutoText text;
            try
            {
                Stanza rsnNode = jingle.child("reason").firstChild();
                reason = rsnNode.name();
                if (strcmp(reason, "hangup") == 0)
                    reason = "peer-hangup";
                text = rsnNode.child("text").text();
            }
            catch(...){}
            terminate(sess.get(), reason?reason:"peer-hangup", text?text.c_str():NULL);
        }
        else if (strcmp(action, "transport-info") == 0)
        {
            sess->addIceCandidates(jingle);
        }
        else if (strcmp(action, "session-info") == 0)
        {
            const char* affected = NULL;
            Stanza info;
            if ((info = jingle.childByAttr("ringing", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true)))
                onRinging(*sess);
            else if ((info = jingle.childByAttr("mute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true)))
            {
                affected = info.attr("name");
                AvFlags av;
                av.audio = (strcmp(affected, "voice") == 0);
                av.video = (strcmp(affected, "video") == 0);
                AvFlags& current = sess->mRemoteAvState;
                if (av.audio)
                    current.audio = false;
                if (av.video)
                    current.video = false;
                onMuted(*sess, av);
            }
            else if ((info = jingle.childByAttr("unmute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1")))
            {
                affected = info.attr("name");
                AvFlags av;
                av.audio = (strcmp(affected, "voice") == 0);
                av.video = (strcmp(affected, "video") == 0);
                auto& current = sess->mRemoteAvState;
                current.audio |= av.audio;
                current.video |= av.video;
                onUnmuted(*sess, av);
            }
        }
        else
            KR_LOG_WARNING("Jingle action '%s' not implemented", action);
   }
   catch(exception& e)
   {
        const char* msg = e.what();
        if (!msg)
            msg = "(no message)";
        KR_LOG_ERROR("Exception in onJingle handler: '%s'", msg);
        onInternalError(msg, "onJingle");
   }
}
/* Incoming call request with a message stanza of type 'megaCall' */
void Jingle::onIncomingCallMsg(Stanza callmsg)
{
    KR_LOG_COLOR(34;1, "megaCall handler called");

    struct State
    {
        bool handledElsewhere = false;
        xmpp_handler elsewhereHandlerId = nullptr;
        xmpp_handler cancelHandlerId = nullptr;
        string sid;
        string from;
        Ts tsReceived = -1;
        string bareJid;
        string ownFprMacKey;
        void* userp = nullptr;
    };
    shared_ptr<State> state(new State);

    state->from = callmsg.attr("from");
    state->sid = callmsg.attr("sid");
    if (mAutoAcceptCalls.find(state->sid) != mAutoAcceptCalls.end())
        throw runtime_error("Auto accept for call with sid '"+string(state->sid)+"' already exists");
    state->bareJid = getBareJidFromJid(state->from);
    state->tsReceived = timestampMs();
    try
    {
    // Add a 'handled-elsewhere' handler that will invalidate the call request if a notification
    // is received that another resource answered/declined the call
        state->elsewhereHandlerId = mConn.addHandler([this, state]
         (Stanza msg, void*, bool& keep)
         {
            keep = false;
            if (!state->cancelHandlerId)
                return;
            state->elsewhereHandlerId = nullptr;
            mConn.removeHandler(state->cancelHandlerId);
            state->cancelHandlerId = nullptr;
            const char* by = msg.attr("by");
            if (strcmp(by, mConn.fullJid()))
               onCallCanceled(state->sid.c_str(), "handled-elsewhere", by,
                  strcmp(msg.attr("accepted"), "1") == 0, &(state->userp));
         },
         NULL, "message", "megaNotifyCallHandled", state->from.c_str(), nullptr, STROPHE_MATCH_BAREJID);

    // Add a 'cancel' handler that will invalidate the call request if the caller sends a cancel message
        state->cancelHandlerId = mConn.addHandler([this, state]
         (Stanza stanza, void*, bool& keep)
         {
            keep = false;
            if (!state->elsewhereHandlerId)
                return;
            state->cancelHandlerId = nullptr;
            mConn.removeHandler(state->elsewhereHandlerId);
            state->elsewhereHandlerId = nullptr;
            const char* reason = stanza.attrOrNull("reason");
            if (!reason)
                reason = "unknown";
            onCallCanceled(state->sid.c_str(), reason, nullptr, false, &(state->userp));
        },
        NULL, "message", "megaCallCancel", state->from.c_str(), nullptr, STROPHE_MATCH_BAREJID);

        setTimeout([this, state]()
         {
            if (!state->cancelHandlerId) //cancel message was received and handler was removed
                return;
    // Call was not handled elsewhere, but may have been answered/rejected by us
            mConn.removeHandler(state->elsewhereHandlerId);
            state->elsewhereHandlerId = nullptr;
            mConn.removeHandler(state->cancelHandlerId);
            state->cancelHandlerId = nullptr;

            onCallCanceled(state->sid.c_str(), "timeout", NULL, false, &(state->userp));
        },
        callAnswerTimeout+10000);

//tsTillUser measures the time since the req was received till the user answers it
//After the timeout either the handlers will be removed (by the timer above) and the user
//will get onCallCanceled, or if the user answers at that moment, they will get
//a call-not-valid-anymore condition

        shared_ptr<function<bool()> > reqStillValid(new function<bool()>(
         [this, state]()
         {
              Ts tsTillUser = state->tsReceived + callAnswerTimeout+10000;
              return ((timestampMs() < tsTillUser) && state->cancelHandlerId);
         }));
        shared_ptr<set<string> > files; //TODO: implement file transfers
        AvFlags peerMedia; //TODO: Implement peerMedia parsing
// Notify about incoming call
        shared_ptr<CallAnswerFunc> ansFunc(new CallAnswerFunc(
         [this, state, callmsg, reqStillValid](bool accept, shared_ptr<AnswerOptions> options, const char* reason, const char* text)
         {
// If dialog was displayed for too long, the peer timed out waiting for response,
// or user was at another client and that other client answred.
// When the user returns at this client he may see the expired dialog asking to accept call,
// and will answer it, but we have to ignore it because it's no longer valid
            if (!(*reqStillValid)()) // Call was cancelled, or request timed out and handler was removed
                return false;//the callback returning false signals to the calling user code that the call request is not valid anymore
            if (accept)
            {
                state->ownFprMacKey = VString(mCrypto->generateFprMacKey()).c_str();
                VString peerFprMacKey = mCrypto->decryptMessage(callmsg.attr("fprmackey"));
                if (peerFprMacKey.empty())
                    throw std::runtime_error("Faield to verify peer's fprmackey from call request");
// tsTillJingle measures the time since we sent megaCallAnswer till we receive jingle-initiate
                Ts tsTillJingle = timestampMs()+mJingleAutoAcceptTimeout;
                auto pInfo = new AutoAcceptCallInfo;
                mAutoAcceptCalls.emplace(state->sid, shared_ptr<AutoAcceptCallInfo>(pInfo));
                AutoAcceptCallInfo& info = *pInfo;

                info["from"] = state->from;
                info.tsReceived = state->tsReceived;
                info.tsTillJingle = tsTillJingle;
                info.options = options; //shared_ptr
                info["peerFprMacKey"] = peerFprMacKey.c_str();
                info["ownFprMacKey"] = state->ownFprMacKey.c_str();
                info["peerAnonId"] = callmsg.attr("anonid");
//TODO: Handle file transfer
// This timer is for the period from the megaCallAnswer to the jingle-initiate stanza
                setTimeout([this, state, tsTillJingle]()
                { //invalidate auto-answer after a timeout
                    AutoAcceptMap::iterator callIt = mAutoAcceptCalls.find(state->sid);
                    if (callIt == mAutoAcceptCalls.end())
                        return; //entry was removed meanwhile
                    auto& call = callIt->second;
                    if (call->tsTillJingle != tsTillJingle)
                        KR_LOG_WARNING("autoaccept timeout: tsTillJingle does not match the one that was set");
                    cancelAutoAcceptEntry(callIt, "initiate-timeout", "timed out waiting for caller to start call", 0);
                }, mJingleAutoAcceptTimeout);

                auto answerFunc = new function<void(const CString&)>
                ([this, state](const CString& errMsg)
                {
                    if (errMsg)
                    {
                        onInternalError("Failed to preload peer's public key. Won't answer call", "preloadCryptoForJid");
                        return;
                    }

                    Stanza ans(mConn);
                    ans.setName("message")
                        .setAttr("sid", state->sid.c_str())
                        .setAttr("to", state->from.c_str())
                        .setAttr("type", "megaCallAnswer")
                        .setAttr("fprmackey", VString(
                            mCrypto->encryptMessageForJid(
                                CString(state->ownFprMacKey),
                                CString(state->bareJid)))
                        )
                        .setAttr("anonid", mOwnAnonId.c_str());
                    mConn.send(ans);
                });
                mCrypto->preloadCryptoForJid(CString(state->bareJid), answerFunc,
                  [](void* userp, const CString& errMsg)
                {
                    unique_ptr<function<void(const CString&)> >
                      fcall(static_cast<function<void(const CString&)>*>(userp));
                    (*fcall)(errMsg);
                });
         }
         else //answer == false
         {
                Stanza declMsg(mConn);
                declMsg.setName("message")
                .setAttr("sid", state->sid.c_str())
                .setAttr("to", state->from.c_str())
                .setAttr("type", "megaCallDecline")
                .setAttr("reason", reason?reason:"reject");

                if (text)
                    declMsg.c("body").t(text);
                mConn.send(declMsg);
         }
         return true;
        })); //end answer func

        onIncomingCallRequest(state->from.c_str(), state->sid.c_str(),
          ansFunc, reqStillValid, peerMedia, files, &(state->userp));
    }
    catch(exception& e)
    {
        KR_LOG_ERROR("Exception in onIncomingCallRequest handler: %s", e.what());
        onInternalError(e.what(), "onCallIncoming");
    }
}

bool Jingle::cancelAutoAcceptEntry(const char* sid, const char* reason, const char* text,
                                   char type)
{
    auto it = mAutoAcceptCalls.find(sid);
    if (it == mAutoAcceptCalls.end())
        return false;
    return cancelAutoAcceptEntry(it, reason, text, type);
}

bool Jingle::cancelAutoAcceptEntry(AutoAcceptMap::iterator it, const char* reason,
                                   const char* text, char type)
{
    auto& item = *(it->second);
    if (item.ftHandler)
    {
        if (type && (type != 'f'))
            return false;
        item.ftHandler->remove(reason, text);
        mAutoAcceptCalls.erase(it);
    }
    else
    {
        AutoAcceptCallInfo& ans = *(it->second);
        FakeSessionInfo info(it->first, ans["from"], mConn.fullJid(), false, ans["peerAnonId"]);
        mAutoAcceptCalls.erase(it);
        onCallTerminated(NULL, reason, text, &info);
    }
    return true;
}
void Jingle::cancelAllAutoAcceptEntries(const char* reason, const char* text)
{
    for (auto it=mAutoAcceptCalls.begin(); it!=mAutoAcceptCalls.end();)
    {
        auto itSave = it;
        it++;
        cancelAutoAcceptEntry(itSave, reason, text, 0);
    }
    mAutoAcceptCalls.clear();
}
void Jingle::purgeOldAcceptCalls()
{
    Ts now = timestampMs();
    for (auto it=mAutoAcceptCalls.begin(); it!=mAutoAcceptCalls.end();)
    {
        auto itSave = it;
        it++;
        auto& call = *itSave->second;
        if (now > call.tsTillJingle)
        {
            KR_LOG_DEBUG("Deleting expired auto-accept entry for session %s", itSave->first.c_str());
            cancelAutoAcceptEntry(itSave, "initiate-timeout", "Timed out waiting for caller to start call");
        }
    }
}
void Jingle::processAndDeleteInputQueue(JingleSession& sess)
{
    unique_ptr<StanzaQueue> queue(sess.inputQueue.release());
    for (auto& stanza: *queue)
        onJingle(stanza);
}
//called by startMediaCall() in rtcModule.cpp
Promise<shared_ptr<JingleSession> >
Jingle::initiate(const char* sid, const char* peerjid, const char* myjid,
  artc::tspMediaStream sessStream, const AvFlags& avState, StringMap&& sessProps,
  FileTransferHandler* ftHandler)
{
// create and initiate a new jinglesession to peerjid
    auto sess = createSession(myjid, peerjid, sid, sessStream, avState,
      std::forward<StringMap>(sessProps), ftHandler);

    sess->initiate(true);
    return sess->sendOffer()
      .then([sess](Stanza)
      {
        return sess->sendAvState();
      })
      .then([sess](int)
      {
        return sess;
      });
}

shared_ptr<JingleSession> Jingle::createSession(const char* me, const char* peerjid,
    const char* sid, artc::tspMediaStream sessStream, const AvFlags& avState,
    const StringMap& sessProps, FileTransferHandler *ftHandler)
{
    KR_CHECK_NULLARG(me);
    KR_CHECK_NULLARG(peerjid);
    KR_CHECK_NULLARG(sid);
    shared_ptr<JingleSession> sess(new JingleSession(*this, me, peerjid, sid, mConn, sessStream,
        avState, sessProps, ftHandler));
    mSessions.emplace(sid, sess);
    return sess;
}
void Jingle::terminateAll(const char* reason, const char* text, bool nosend)
{
//terminate all existing sessions
    for (auto it = mSessions.begin(); it!=mSessions.end();)
    {
        auto erased = it++;
        terminate(erased->second.get(), reason, text, nosend);
    }
}
bool Jingle::terminateBySid(const char* sid, const char* reason, const char* text,
    bool nosend)
{
    return terminate(mSessions[sid].get(), reason, text, nosend);
}
bool Jingle::terminate(JingleSession* sess, const char* reason, const char* text,
    bool nosend)
{
    if (!sess)
    {
        KR_LOG_WARNING("terminate: requested to terminate a NULL session");
        return false;
    }
    auto sessIt = mSessions.find(sess->sid());
    if (sessIt == mSessions.end())
    {
        KR_LOG_WARNING("Jingle::terminate: Unknown session: %s", sess->sid().c_str());
        return false;
    }
    if (!reason)
        reason = "term";
    bool isFt = !!sess->ftHandler();
    if (sess->state() != JingleSession::SESSTATE_ENDED)
    {
        if (!nosend)
            sess->sendTerminate(reason, text);
        sess->terminate(reason, text); //handles ftHandler, updates mState
    }
    auto sessKeepalive = sessIt->second; //prevent the erase() below from destroying the session until we exit this function
    mSessions.erase(sessIt);
    if (!isFt)
        try
        {
            onCallTerminated(sess, reason, text);
        }
        catch(exception& e)
        {
            KR_LOG_ERROR("Jingle::onCallTerminated() threw an exception: %s", e.what());
        }
    return true;
}
Promise<Stanza> Jingle::sendTerminateNoSession(const char* sid, const char* to, const char* reason,
    const char* text)
{
    Stanza term(mConn);
    auto last = term.init("iq", {{"to", to}})
      .c("jingle",
        {
             {"xmlns", "urn:xmpp:jingle:1"},
             {"action", "session-terminate"},
             {"sid", sid}
        })
      .c("reason")
      .c(reason);
    if (text)
        last.parent().c("text").t(text);
    return sendIq(term, "term-no-sess");
}

bool Jingle::sessionIsValid(const JingleSession& sess)
{
    return (mSessions.find(sess.sid()) != mSessions.end());
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

Promise<Stanza> Jingle::sendIq(Stanza iq, const string& origin)
{
    return mConn.sendIqQuery(iq)
        .fail([this, origin, iq](const promise::Error& err)
        {
            onJingleError(nullptr, origin, err.msg(), iq);
            return err;
        });
}

bool Jingle::verifyMac(const std::string& msg, const std::string& key, const std::string& actualMac)
{
    if (actualMac.empty())
        return false;
    string expectedMac;
    try
    {
        expectedMac = VString(crypto().generateMac(msg, key)).c_str();
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

int Jingle::setIceServers(const char* iceServers)
{
    if (!iceServers || !iceServers[0])
    {
        mIceServers.reset(new webrtc::PeerConnectionInterface::IceServers);
        return 0;
    }
    webrtc::PeerConnectionInterface::IceServers servers;

    vector<string> strServers;
    tokenize(iceServers, ";", strServers);
    for (string& strServer: strServers)
    {
        map<string, string> props;
        parseNameValues(strServer.c_str(), ",", '=', props);
        webrtc::PeerConnectionInterface::IceServer server;

        for (auto& p: props)
        {
            const string& name = p.first;
            if (name == "url")
                server.uri = p.second;
            else if (name == "user")
                server.username = p.second;
            else if (name == "pass")
                server.password = p.second;
            else
                KR_LOG_WARNING("setIceServers: Unknown server property '%s'", p.second.c_str());
        }
        servers.push_back(server);
    }
    mIceServers->swap(servers);
    return (int)(mIceServers->size());
}

AvFlags peerMediaToObj(const char* strPeerMedia)
{
    AvFlags ret;
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

}
