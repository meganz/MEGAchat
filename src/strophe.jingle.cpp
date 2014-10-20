
/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "webrtcAdapter.h"
#include "strophe.jingle.session.h"
#include "strophe.jingle.h"
#include "strophe.disco.h"
#include <mstrophepp.h>

using namespace std;

namespace karere
{
namespace rtcModule
{

AvFlags peerMediaToObj(const char* strPeerMedia);

void Jingle::onInternalError(const string& msg, const char* where)
{
    KR_LOG_ERROR("Internal error at %s: %s", where, msg.c_str());
};
//==

string generateHmac(const string& data, const string& key){return "";} //TODO: Implement
Jingle::Jingle(strophe::Connection& conn, const string& iceServers)
:Plugin(conn)
{
    if (!iceServers.empty())
        setIceServers(iceServers);
    mMediaConstraints.SetMandatoryReceiveAudio(true);
    mMediaConstraints.SetMandatoryReceiveVideo(true);
    artc::init(NULL);
    artc::DeviceManager devMgr;
    mInputDevices = artc::getInputDevices(devMgr);
    registerDiscoCaps();
}
void Jingle::addAudioCaps(DiscoPlugin& disco)
{
    disco.addNode("urn:xmpp:jingle:apps:rtp:audio", {});
}
void Jingle::addVideoCaps(DiscoPlugin& disco)
{
    disco.addNode("urn:xmpp:jingle:apps:rtp:video", {});
}
void Jingle::registerDiscoCaps()
{
    auto plDisco = mConn.pluginPtr<DiscoPlugin>("disco");
    if (!plDisco)
    {
        KR_LOG_WARNING("Disco plugin not found, not registering disco caps");
        return;
    }
    DiscoPlugin& disco = *plDisco;
    // http://xmpp.org/extensions/xep-0167.html#support
    // http://xmpp.org/extensions/xep-0176.html#support
    disco.addNode("urn:xmpp:jingle:1", {});
    disco.addNode("urn:xmpp:jingle:apps:rtp:1", {});
    disco.addNode("urn:xmpp:jingle:transports:ice-udp:1", {});

    disco.addNode("urn:ietf:rfc:5761", {}); // rtcp-mux
    //this.connection.disco.addNode('urn:ietf:rfc:5888', {}); // a=group, e.g. bundle
    //this.connection.disco.addNode('urn:ietf:rfc:5576', {}); // a=ssrc
    bool hasAudio = !mInputDevices->audio.empty() && !(mediaFlags & DISABLE_MIC);
    bool hasVideo = !mInputDevices->video.empty() && !(mediaFlags & DISABLE_CAM);
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
        Jingle* self = (Jingle*)userdata;
        if (status == XMPP_CONN_CONNECT)
        {
//         typedef int (*xmpp_handler)(xmpp_conn_t * const conn,
//           xmpp_stanza_t * const stanza, void * const userdata);
            xmpp_handler_add(mConn, _static_onJingle, "urn:xmpp:jingle:1", "iq", "set", userdata);
            xmpp_handler_add(mConn, _static_onIncomingCallMsg, NULL, "message", "megaCall", userdata);
        }
        self->onConnectionEvent(status, error, stream_error);
    }
    catch(exception& e)
    {
        KR_LOG_ERROR("Exception in connection state handler: %s", e.what());
    }
}
static int Jingle::_static_onJingle(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
{
    return static_cast<Jingle*>(userdata)->onJingle(stanza);
}
static int Jingle::_static_onIncomingCallMsg(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
{
    return static_cast<Jingle*>(userdata)->onIncomingCallMsg(stanza);
}
bool Jingle::onJingle(Stanza iq)
{
   try
   {
        Stanza jingle = iq.child("child");
        const char* sid = jingle.attr("sid");
        auto sess = mSessions[sid];
        if (sess.get() && sess->inputQueue.get())
        {
            sess->inputQueue->push_back(iq);
            return true;
        }
        const char* action = jingle.attr("action");
        // send ack first
        Stanza ack(mConn);
        ack.init("iq", {
            {"type", "result"},
            {"to", iq.attr("from")},
            {"id", iq.attr("id")}
        });
        bool error = false;
        KR_LOG_DEBUG("onJingle '%s' from '%s'", action, iq.attr("from"));

        if (strcmp(action, "session-initiate"))
        {

            if (!sess.get())
                error = true;
            // compare from to sess.peerjid (bare jid comparison for later compat with message-mode)
            // local jid is not checked
            else if (getBareJidFromJid(iq.attr("from")) != getBareJidFromJid(sess->peerjid))
            {
                error = true;
                KR_LOG_WARNING("onJingle: JID mismatch for session '%s': ('%s' != '%s')", iq.attr("from"), sess->peerjid.c_str());
            }
            if (error)
            {
                ack.setAttr("type", "error");
                ack.c("error", {{"type", "cancel"}})
                   .c("item-not-found", {{"xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas"}}).parent()
                   .c("unknown-session", {{"xmlns", "urn:xmpp:jingle:errors:1"}});
            }
        }
        else if (sess.get()) //action == session-initiate
        {
            // existing session with same session id
            // this might be out-of-order if the sess.peerjid is the same as from
            ack.setAttr("type", "error");
            ack.c("error", {{"type", "cancel"}})
               .c("service-unavailable", {{"xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas"}}).parent();
            KR_LOG_WARNING("onJingle: Duplicate session id '%s'", sid);
        }
        if (error)
        {
            xmpp_send(mConn, ack);
            return true;
        }

        // see http://xmpp.org/extensions/xep-0166.html#concepts-session
        if (strcmp(action, "session-initiate") == 0)
        {
            const char* peerjid = iq.attr("from");
            string barePeerJid = getBareJidFromJid(peerjid);
            KR_LOG_DEBUG("received INITIATE from %s", peerjid.c_str());
            purgeOldAcceptCalls(); //they are purged by timers, but just in case
            auto ansIter = mAcceptCallsFrom.find(peerjid);
            if (!ansIter == mAcceptCallsFrom.end())
                return true; //ignore silently - maybe there is no user on this client and some other client(resource) already accepted the call
            shared_ptr<AutoAcceptInfo> ans = ansIter->second;
            mAutoAcceptCalls.erase(ansIter);
// Verify SRTP fingerprint
            if (ans->ownNonce.empty())
                throw runtime_error("No ans.ownNonce present, there is a bug");
            if (generateHmac(getFingerprintsFromJingle(jingle), ans->ownNonce) != jingle.attr("fprmac"))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                try
                {
                    NoSessionInfo info;
                    info.peer = peerjid.c_str();
                    info.sid = jingle.attr("sid");
                    info.isInitiator = false;
                    onCallTerminated(NULL, "security", "fingerprint verification failed", &info);
                }
                catch(...){}
                return true;
            }
//===
            auto sess = createSession(iq.attr("to"), peerjid, sid,
               ans->options.localStream, ans->options.muted, NULL, ans.peerFprMacKey);

            sess.inputQueue.reset(new list<Stanza>);
            string reason, text;
            bool cont = onCallIncoming(sess, reason, text);
            if (!cont)
            {
                terminate(sess, reason, text);
                sess.inputQueue.reset();
                return true;
            }

            sess.initiate(false);

            // configure session
            sess.setRemoteDescription(jingle, "offer").then([this, &sess](int)
            {
                return sess.sendAnswer();
            })
            .then([this, &self](int)
            {
                  return sess.sendMutedState();
            })
            .then([this, &sess, &peerjid])
            {
                onCallAnswered(peerjid.c_str());
//now handle all packets queued up while we were waiting for user's accept of the call
                processAndDeleteInputQueue(sess);
            })
            .fail([this, &sess](Error& e)
             {
                  sess.inputQueue.reset();
                  terminate(sess, "error", e.msg());
             });
        }
        else if (strcmp(action, "session-accept") == 0)
        {
// Verify SRTP fingerprint
            if (sess.ownNonce.empty())
                throw runtime_error("No session.ownNonce present, there is a bug");

            if (generateHmac(getFingerprintsFromJingle(jingle), sess.ownNonce) !== jingle.attr("fprmac"))
            {
                KR_LOG_WARNING("Fingerprint verification failed, possible forge attempt, dropping call!");
                terminateBySid(sess.sid, "security", "fingerprint verification failed");
                return true;
            }
// We are likely to start receiving ice candidates before setRemoteDescription()
// has completed, esp on Firefox, so we want to queue these and feed them only
// after setRemoteDescription() completes
            sess.inputQueue.reset(new list);
            sess.setRemoteDescription(jingle, "answer", [this, &sess]()
            {
                return sess.sendAnswer();
            })
            .then([this, &sess]()
            {
                if (sess.inputQueue)
                   processAndDeleteInputQueue(sess);
            });
        }
        else if (strcmp(action, "session-terminate") == 0)
        {
            const char* reason = NULL;
            shared_ptr<AutoText> text;
            try
            {
                Stanza rsnNode = jingle.child("reason").firstChild();
                reason = rsnNode.name();
                if (strcmp(reason, "hangup") == 0)
                    reason = "peer-hangup";
                text = rsnNode.child("text").innerText();
            }
            catch(...){}
            terminate(sess, reason?reason:"peer-hangup", text.get()?text->c_str():NULL);
        }
        else if (strcmp(action, "transport-info") == 0)
        {
            sess.addIceCandidate(jingle.child("content"));
        }
        else if (strcmp(action, "session-info") == 0)
        {
            const char* affected = NULL;
            Stanza info;
            if (info = jingle.childByAttr("ringing", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true))
                onRinging(sess);
            else if (info = jingle.childByAttr("mute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1", true))
            {
                affected = info.attr("name");
                AvFlags av;
                av.audio = (strcmp(affected, "voice") == 0);
                av.video = (strcmp(affected, "video") == 0);
                AvFlags current;
                sess.getRemoteMutedState(current);
                current.audio |= av.audio;
                current.video |= av.video;
                sess.setRemoteMutedState(current);
                onMuted(sess, av);
            }
            else if (info = jingle.childByAttr("unmute", "xmlns", "urn:xmpp:jingle:apps:rtp:info:1"))
            {
                affected = info.attr("name");
                AvFlags av;
                av.audio = (strcmp(affected, "voice") == 0);
                av.video = (strcmp(affected, "video") == 0);
                AvFlags current;
                sess.getRemoteMutedState(current);
                if (av.audio)
                    current.audio = false;
                if (av.video)
                    current.video = false;
                sess.setRemoteMutedState(current);
                onUnmuted(sess, av);
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
   return true;
}
/* Incoming call request with a message stanza of type 'megaCall' */
void Jingle::onIncomingCallMsg(Stanza callmsg)
{
    const char* from = callmsg.attr("from");
    if (!from)
        throw runtime_error("No 'from' attribute in megaCall message");
    const char* sid = callmsg.attr("sid");
    if (!sid)
        throw runtime_error("No 'sid' attribute in megaCall message");
    if (mAcceptCallsFrom.find(sid) != mAcceptCallsFrom.end())
        throw runtime_error("Auto accept for call with sid '"+string(sid)+"' already exists");

    string bareJid = getBareJidFromJid(from);
    Ts tsReceived = timestampMs();
    struct State
    {
        bool handledElsewhere = false;
        xmpp_handler elsewhereHandlerId = NULL;
//        function<bool(Connection&, Stanza, int)> elsewhereHandler;
        xmpp_handler cancelHandlerId = NULL;
//        function<bool(Connection&, Stanza, int)> cancelHandler;
    };
    shared_ptr<State> state(new State);
    try
    {
    // Add a 'handled-elsewhere' handler that will invalidate the call request if a notification
    // is received that another resource answered/declined the call
        state->elsewhereHandlerId = conn.addHandler([this, state]
         (Connection& conn, Stanza stanza, int)
         {
            if (!state->cancelHandlerId)
                return false;
            state->elsewhereHandlerId = NULL;
            xmpp_handler_delete(mConn, cancelHandlerId);
            cancelHandler = cancelHandlerId = NULL;
            Stanza msg(stanza);
            const char* by = msg.attr("by");
            if (strcmp(by, xmpp_conn_get_bound_jid(mConn)))
               onCallCanceled(from, "handled-elsewhere", by,
                  strcmp(msg.attr("accepted"), "1") == 0);
            return false;
         }, NULL, "message", "megaNotifyCallHandled", from, NULL, STROPHE_MATCH_BARE_JID);

    // Add a 'cancel' handler that will ivalidate the call request if the caller sends a cancel message
        state->cancelHandlerId = conn.addHandler([this, state]
         (Connection& conn, Stanza stanza, int)
         {
            if (!state->elsewhereHandlerId)
                return false;
            state->cancelHandlerId = NULL;
            xmpp_handler_delete(mConn, state->elsewhereHandlerId);
            state->elsewhereHandlerId = NULL;

            onCallCanceled(from, "canceled", NULL, false);
            return false;
        }, NULL, "message", "megaCallCancel", from, NULL, STROPHE_MATCH_BARE_JID);

        mega::setTimeout([this, state]()
         {
            if (!state->cancelHandlerId) //cancel message was received and handler was removed
                return;
    // Call was not handled elsewhere, but may have been answered/rejected by us
            xmpp_handler_delete(mConn, state->elsewhereHandlerId);
            state->elsewhereHandlerId = NULL;
            xmpp_handler_delete(mConn, state->cancelHandlerId);
            state->cancelHandlerId = NULL;

            onCallCanceled(from, "timeout", NULL, false);
        }, callAnswerTimeout+10000);

//tsTillUser measures the time since the req was received till the user answers it
//After the timeout either the handlers will be removed (by the timer above) and the user
//will get onCallCanceled, or if the user answers at that moment, they will get
//a call-not-valid-anymore condition
        Ts tsTillUser = timestampMs() + callAnswerTimeout+10000;
        auto reqStillValid = [&tsTillUser, &state]() {
            return ((timestampMs() < tsTillUser) && state->cancelHandlerId);
        };
// Notify about incoming call
        onIncomingCallRequest(from, reqStillValid,
          [&, this](accept, obj)
        {
// If dialog was displayed for too long, the peer timed out waiting for response,
// or user was at another client and that other client answred.
// When the user returns at this client he may see the expired dialog asking to accept call,
// and will answer it, but we have to ignore it because it's no longer valid
            if (!reqStillValid()) // Call was cancelled, or request timed out and handler was removed
                return false;//the callback returning false signals to the calling user code that the call request is not valid anymore
            if (accept)
            {
                string ownNonce = generateNonce();
                string peerNonce = decryptMessage(callmsg.attr("nonce"));
                if (peerNonce.empty())
                    throw std::runtime_error("Encrypted nonce missing from call request");
// tsTillJingle measures the time since we sent megaCallAnswer till we receive jingle-initiate
                Ts tsTillJingle = timestampMs()+mJingleAutoAcceptTimeout;
                AutoAcceptCallInfo info& = acceptCallsFrom[sid];

                info.tsReceived = tsReceived;
                info.tsTill = tsTillJingle;
                info.options = obj.options; //shared_ptr
                info["peerNonce"] = peerNonce;
                info["ownNonce"] = ownNonce;

// This timer is for the period from the megaCallAnswer to the jingle-initiate stanza
                setTimeout([this, from, &tsTillJingle]()
                { //invalidate auto-answer after a timeout
                    auto callIt = acceptCallsFrom.find(from);
                    if (callIt == acceptCallsFrom.end())
                        return; //entry was removed or updated by a new call request
                    auto call = callIt->second;
                    if (call->tsTill != tsTillJingle)
                        return; //entry was updated by a new call request
                    cancelAutoAnswerEntry(from, "initiate-timeout", "timed out waiting for caller to start call");
                }, mJingleAutoAcceptTimeout);

                Stanza ans(mConn);
                ans.init("message",
                {
                    {"to", from},
                    {"type", "megaCallAnswer"},
                    {"nonce", encryptMessageForJid(ownNonce, bareJid)}
                });
                xmpp_send(mConn, ans);
         }
         else //answer == false
         {
                Stanza declMsg(mConn);
                declMsg.init("message",
                {
                    {"to", from},
                    {"type", "megaCallDecline"},
                    {"reason", obj.reason.empty()?"unknown":obj.reason}
                });
                if (!obj.text.empty())
                    declMsg.c("body", {}).t(obj.text);
                xmpp_send(declMsg);
         }
            return true;
        }); //end answer func
    }
    catch(exception& e)
    {
        KR_LOG_ERROR("Exception in onIncomingCallRequest handler: %s", e.what());
        onInternalError(e.what(), "onCallIncoming");
    }
    return true;
}
bool Jingle::cancelAutoAcceptEntry(const char* sid, const char* reason, const char* text, char type)
{
    auto it = mAutoAcceptCalls.find(sid);
    if (it == mAutoAcceptCalls.end())
        return false;
    return cancelAutoAnswerEntry(it, reason, text, type);
}

bool Jingle::cancelAutoAnswerEntry(AutoAcceptMap::iterator it, const char* reason, const char* text, char type)
{
    auto& item = it->second;
    if (item.fileTransferHandler)
    {
        if (type && (type != 'f'))
            return false;
        item.fileTransferHandler->remove(reason, text);
        mAutoAcceptCalls.erase(sid);
    }
    else
    {
        mAutoAcceptCalls.erase(sid);
        NoSessionInfo info;
        info.sid = sid;
        info.peer = item.from;
        info.isInitiator = false;
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
        cancelAutoAnswerEntry(itSave->first, reason, text);
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
        auto& call = itSave->second;
        if (call.tsTill < now)
            cancelAutoAnswerEntry(itSave->first, "initiate-timeout", "Timed out waiting for caller to start call");
    }
}
void Jingle::processAndDeleteInputQueue(JingleSession& sess)
{
    unique_ptr<StanzaQueue> queue(sess.inputQueue);
    sess.inputQueue = NULL;
    for (auto stanza: *queue)
        onJingle(stanza);
}

JingleSession* Jingle::initiate(const char* sid, const char* peerjid, const char* myjid,
  artc::tspMediaStream sessStream, const AvFlags& mutedState, shared_ptr<StringMap> sessProps,
  FileTrasnferHandler* ftHandler)
{ // initiate a new jinglesession to peerjid
    JingleSession* sess = createSession(myjid, peerjid, sid, sessStream, mutedState,
      sessProps);
    // configure session
    sess.mediaConstraints = this.mediaConstraints;
    sess.pcConstraints = this.pcConstraints;

    sess.initiate(true);
    sess.sendOffer().then([sess]()
    {
        sess.sendMutedState()
    });
    return sess;
}
JingleSession* Jingle::createSession(const char* me, const char* peerjid,
    const char* sid, myrtc::tspMediaStream, const AvFlags& mutedState,
    shared_ptr<StringMap> sessProps)
{
    KR_CHECK_NULLARG(me);
    KR_CHECK_NULLARG(peerjid);
    KR_CHECK_NULLARG(sid);
    JingleSession* sess = new JingleSession(*this, me, peerjid, sid, mConn, sessStream,
        mutedState, sessProps);
    mSessions[sid] = sess;
    return sess;
}
void Jingle::terminateAll(const char* reason, const char* text, bool nosend)
{
//terminate all existing sessions
    for (auto it: mSessions)
        terminate(it->second, reason, text, nosend);
}
bool Jingle::terminateBySid(const char* sid, const char* reason, const char* text,
    bool nosend)
{
    return terminate(mSessions[sid], reason, text, nosend);
}
bool Jingle::terminate(JingleSession* sess, const char* reason, const char* text,
    bool nosend)
{
    if ((!sess) || (mSessions.find(sess->sid) == mSession.end()))
    {
        KR_LOG_WANING("Jingle::terminate: Unknown session: %s", sess.sid.c_str());
        return false;
    }
    if (!reason)
        reason = "term";
    unique_ptr<JingleSession> autodel(sess);
    if (sess->state != SESSTATE_ENDED)
    {
        if (!nosend)
            sess->sendTerminate(reason, text);
        sess->terminate();
    }
    mSessions.erase(sess->sid);
    if (sess->fileTransferHandler)
        sess->fileTransferHandler->remove(reason, text);
    else
        try
        {
            onCallTerminated(sess, reason, text);
        }
        catch(e)
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
    return mConn.sendIqQuery(term, "set");
}

bool Jingle::sessionIsValid(JingleSession* sess)
{
    return (mSessions.find(sess->sid) != mSessions.end());
}

string Jingle::getFingerprintsFromJingle(Stanza j)
{
    vector<Stanza> nodes;
    j.forEachChild("content", [this](Stanza content)
    {
        child.forEachChild("transport", [this](Stanza transport)
        {
            transport.forEachChild("fingerprint", [this](Stanza fingerprint)
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
        fps.push_back(string(node.attr("hash"))+" "+node.text());
    }
    std::sort(fps);
    string result(256);
    for (auto& item: fps)
        result.append(item)+=';';
    if (result.size() > 0)
        result.resize(result.size()-1);
    return result;
}

AvFlags peerMediaToObj(const char* strPeerMedia)
{
    AvFlags ret;
    for (; *strPeerMedia; strPeerMedia++)
    {
        char ch = *strPeerMedia;
        if (ch == 'a')
            res.audio = true;
        else if (ch == 'v')
            res.video = true;
    }
    return ret;
}

}
