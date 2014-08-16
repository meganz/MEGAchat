
/* This code is based on strophe.jingle.js by ESTOS */
#include <string>
#include <map>
#include <memory>
#include "webrtcAdapter.h"

namespace strophe
{
    class Stanza;
    class Connection;
}
namespace karere
{
struct JingleEventHandler
{
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
    struct AutoAcceptInfo
    {

    };

    std::map<std::string, std::shared_ptr<JingleSession> > mSessions;
    std::map<std::string, std::shared_ptr<JingleSession> > mJid2Session;
    webrtc::FaceConstraints mMediaConstraints;
/** Timeout after which if an iq response is not received, an error is generated */
    int mJingleTimeout = 50000;
/** The period, during which an accepted call request will be valid
* and the actual call answered. The period starts at the time the
* request is received from the network */
    int mJingleAutoAcceptTimeout = 15000;
/** The period within which an outgoing call can be answered by peer */
    int callAnswerTimeout = 50000;
    std::map<std::string, std::shared_ptr<AutoAcceptInfo> >mAcceptCallsFrom;
    webrtc::PeerConnectionInterface::IceServers mIceServers;
    std::shared_ptr<rtc::InputDevices> mInputDevices;
public:
    enum {DISABLE_MIC = 1, DISABLE_CAM = 2, HAS_MIC = 4, HAS_CAM = 8};
    int mediaFlags = 0;
//event handler interface
    virtual void onRemoteStreamAdded(JingleSession& sess, rtc::tspMediaStream stream){}
    virtual void onRemoteStreamRemoved(JingleSession& sess, rtc::tspMediaStream stream){}
    virtual void onJingleError(JingleSession& sess, const std::string& err, strophe::Stanza stanza, strophe::Stanza orig){}
    virtual void onJingleTimeout(JingleSession& sess, const std::string& err, strophe::Stanza orig){}
//    virtual void onIceConnStateChange(JingleSession& sess, event){}
    virtual void onIceComplete(JingleSession& sess, event){}
//    virtual void onNoStunCandidates(JingleSession& sess){}

//rtcHandler callback interface, called by the connection.jingle object
    virtual void onIncomingCallRequest(const char* from,
     bool(*reqStillValid)(), void(*ansFunc)(bool)){}
    virtual void onCallCanceled(const char* peer, const char* event,
     const char* by, bool accepted){}
    virtual void onCallRequestTimeout(const char* peer) {}
    virtual void onCallAnswered(const char* peer) {}
    virtual void onCallTerminated(JingleSession* sess, const char* reason,
      const char* text, const NoSessionInfo* info=NULL){}
    virtual void onCallIncoming(JingleSession& sess){return true;}
    virtual void onRinging(JingleSession& sess){}
    virtual void onMuted(JingleSession& sess, const AvFlags& affected){}
    virtual void onUnmuted(JingleSession& sess, const AvFlags& affected){}
    virtual void onInternalError(const char& msg, const char* where)
    {
        KR_LOG_ERROR("Internal error at %s: %s", where, msg.c_str());
    };
//==

    string generateHmac(const string& data, const string& key);
    Jingle(strophe::Connection& conn, const string& iceServers)
        :Plugin(mConn)
    {
        mConn.add
        setIceServers(iceServers);
        mMediaConstraints.setMandatoryReceiveAudio(true);
        mMediaConstraints.setMandatoryReceiveVideo(true);
        rtc::init(NULL);
        rtc::DeviceManager devMgr;
        mInputDevices = rtc::getInputDevices(devMgr);
        registerDiscoCaps();
    }
    void addAudioCaps(DiscoPlugin& disco)
    {
        disco.addNode("urn:xmpp:jingle:apps:rtp:audio", {});
    }
    function addVideoCaps(DiscoPlugin& disco)
    {
        disco.addNode("urn:xmpp:jingle:apps:rtp:video", {});
    }
    void registerDiscoCaps()
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
//        if (localStorage.megaDisableMediaInputs === undefined)
//            RTC.queryMediaInputPermissions();
        bool hasAudio = !mInputDevices->audio.empty() && !(mediaFlags & DISABLE_MIC);
        bool hasVideo = !mInputDevices->video.empty() && !(mediaFlags & DISABLE_CAM);
        if (hasAudio)
            addAudioCaps();
        if (hasVideo)
            addVideoCaps();
    }
    void onConnState(const xmpp_conn_event_t status,
                const int error, xmpp_stream_error_t * const stream_error)
    {
      try
      {
        Jingle* self = (Jingle*)userdata;
        if (status === XMPP_CONN_CONNECT)
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
    static int _static_onJingle(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
    {
        return static_cast<Jingle*>(userdata)->onJingle(stanza);
    }
    static int _static_onIncomingCallMsg(xmpp_conn_t* const conn, xmpp_stanza_t* stanza, void* userdata)
    {
        return static_cast<Jingle*>(userdata)->onIncomingCallMsg(stanza);
    }
bool onJingle(Stanza iq)
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
            mAutoAcceptCallsFrom.erase(ansIter);
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
                AvFlags& av;
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
            KR_LOG_WARNING('Jingle action '%s' not implemented', action);
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
void onIncomingCallMsg(Stanza callmsg)
{
    const char* from = callmsg.attr("from");
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
     conn.addHandler([this, state]
      (Connection& conn, Stanza stanza, int)
     {
          if (!state->cancelHandlerId)
              return false;
          state->elsewhereHandler = NULL;
          xmpp_handler_delete(mConn, cancelHandlerId);
          cancelHandler = cancelHandlerId = NULL;
            Stanza msg(stanza);
            const char* by = msg.attr("by");
            if (strcmp(by, xmpp_conn_get_bound_jid(mConn)))
               onCallCanceled(msg.attr("from"), "handled-elsewhere", by,
                  strcmp(msg.attr("accepted"), "1") == 0);
    }, "message", NULL, "megaNotifyCallHandled", from, {matchBare:true});

    // Add a 'cancel' handler that will ivalidate the call request if the caller sends a cancel message
        cancelHandler = self.connection.addHandler(function(msg) {
            if (!elsewhereHandler)
                return;
            cancelHandler = null;
            self.connection.deleteHandler(elsewhereHandler);
            elsewhereHandler = null;

            self.onCallCanceled.call(self.eventHandler, $(msg).attr('from'), {event: 'canceled'});
        }, null, 'message', 'megaCallCancel', null, from, {matchBare:true});

        setTimeout(function() {
            if (!cancelHandler) //cancel message was received and handler was removed
                return;
    // Call was not handled elsewhere, but may have been answered/rejected by us
            self.connection.deleteHandler(elsewhereHandler);
            elsewhereHandler = null;
            self.connection.deleteHandler(cancelHandler);
            cancelHandler = null;

            self.onCallCanceled.call(self.eventHandler, from, {event:'timeout'});
        }, self.callAnswerTimeout+10000);

//tsTillUser measures the time since the req was received till the user answers it
//After the timeout either the handlers will be removed (by the timer above) and the user
//will get onCallCanceled, or if the user answers at that moment, they will get
//a call-not-valid-anymore condition
        var tsTillUser = Date.now() + self.callAnswerTimeout+10000;
        var reqStillValid = function() {
            return ((tsTillUser > Date.now()) && (cancelHandler != null));
        };
// Notify about incoming call
        self.onIncomingCallRequest.call(self.eventHandler, from, reqStillValid,
          function(accept, obj) {
// If dialog was displayed for too long, the peer timed out waiting for response,
// or user was at another client and that other client answred.
// When the user returns at this client he may see the expired dialog asking to accept call,
// and will answer it, but we have to ignore it because it's no longer valid
            if (!reqStillValid()) // Call was cancelled, or request timed out and handler was removed
                return false;//the callback returning false signals to the calling user code that the call request is not valid anymore
            if (accept) {
                var ownNonce = self.generateNonce();
                var peerNonce = self.decryptMessage($(callmsg).attr('nonce'));
                if (!peerNonce)
                    throw new Error("Encrypted nonce missing from call request");
// tsTillJingle measures the time since we sent megaCallAnswer till we receive jingle-initiate
                var tsTillJingle = Date.now()+self.jingleAutoAcceptTimeout;
                self.acceptCallsFrom[from] = {
                    tsReceived: tsReceived,
                    tsTill: tsTillJingle,
                    options: obj.options,
                    peerNonce: peerNonce,
                    ownNonce: ownNonce
                };
// This timer is for the period from the megaCallAnswer to the jingle-initiate stanza
                setTimeout(function() { //invalidate auto-answer after a timeout
                    var call = self.acceptCallsFrom[from];
                    if (!call || (call.tsTill != tsTillJingle))
                        return; //entry was removed or updated by a new call request
                    self.cancelAutoAnswerEntry(from, 'initiate-timeout', 'timed out waiting for caller to start call');
                }, self.jingleAutoAcceptTimeout);

                self.connection.send($msg({
                    to: from,
                    type: 'megaCallAnswer',
                    nonce: self.encryptMessageForJid(ownNonce, bareJid)
                }));
            }
            else {
// We don't want to answer calls from that user, and this includes a potential previous
// request - we want to cancel that too
                delete self.acceptCallsFrom[bareJid];
                var msg = $msg({to: from, type: 'megaCallDecline', reason: obj.reason?obj.reason:'unknown'});
                if (obj.text)
                    msg.c('body').t(obj.text);
                self.connection.send(msg);
            }
            return true;
        });
      } catch(e) {
            console.error('Exception in onIncomingCallRequest handler:', e);
            self.onInternalError.call(self.eventHandler, {type:'jingle'} , e);
      }
      return true;
    },
    cancelAutoAnswerEntry: function(from, reason, text) {
        delete this.acceptCallsFrom[from];
        this.onCallTerminated.call(this.eventHandler, {fake: true, peerjid: from, isInitiator: false},
            reason, text);
    },
    cancelAllAutoAnswerEntries: function(reason, text) {
        var save = this.acceptCallsFrom;
        this.acceptCallsFrom = {};
        for (var k in save)
            this.onCallTerminated.call(this.eventHandler, {fake: true, peerjid: k, isInitiator: false},
                reason, text);
    },
    purgeOldAcceptCalls: function() {
        var self = this;
        var now = Date.now();
        for (var k in self.acceptCallsFrom) {
            var call = self.acceptCallsFrom[k];
            if (call.tsTill < now)
                this.cancelAutoAnswerEntry(k, 'initiate-timeout', 'timed out waiting for caller to start call');
        }
    },
    processAndDeleteInputQueue: function(sess) {
        var queue = sess.inputQueue;
        delete sess.inputQueue;
        for (var i=0; i<queue.length; i++)
            this.onJingle(queue[i]);
    },
    initiate: function (peerjid, myjid, sessStream, mutedState, sessProps) { // initiate a new jinglesession to peerjid
        var sess = this.createSession(myjid, peerjid,
            Math.random().toString(36).substr(2, 12), // random string
            sessStream, mutedState, sessProps);
        // configure session
        sess.media_constraints = this.media_constraints;
        sess.pc_constraints = this.pc_constraints;
        sess.ice_config = this.ice_config;

        sess.initiate(true);
        sess.sendOffer(function() {sess.sendMutedState()});
        return sess;
    },
    createSession: function(me, peerjid, sid, sessStream, mutedState, sessProps) {
        var sess = new JingleSession(me, peerjid, sid, this.connection, sessStream, mutedState);
        this.sessions[sess.sid] = sess;
        this.jid2session[sess.peerjid] = sess;
        if (sessProps) {
            for (var k in sessProps)
                if (sessProps.hasOwnProperty(k)) {
                    if (sess[k] === undefined)
                        sess[k] = sessProps[k];
                      else
                        console.warn('Jingle.initiate: a property in sessProps overlaps with an existing property of the create session object - not setting');
                }
        }

        return sess;
    },
    terminateAll: function(reason, text, nosend)
    {
    //terminate all existing sessions
        for (sid in this.sessions)
            this.terminate(this.sessions[sid], reason, text, nosend);
    },
    terminateBySid: function(sid, reason, text, nosend)
    {
        return this.terminate(this.sessions[sid], reason, text, nosend);
    },
    terminate: function(sess, reason, text, nosend)
    {
        if ((!sess) || (!this.sessions[sess.sid]))
            return false; //throw new Error("Unknown session: " + sid);
        if (sess.state != 'ended')
        {
            if (!nosend)
                sess.sendTerminate(reason||'term', text);
            sess.terminate();
        }
        delete this.jid2session[sess.peerjid];
        delete this.sessions[sess.sid];
        try {
            this.onCallTerminated.call(this.eventHandler, sess, reason||'term', text);
        } catch(e) {
            console.error('Jingle.onCallTerminated() threw an exception:', e.stack);
        }

        return true;
    },
    terminateByJid: function (jid)
    {
        var sess = this.jid2session[jid];
        if (!sess)
            return false;
        return this.terminate(sess, null, null);
    },
    sessionIsValid: function(sess)
    {
        return (this.sessions[sess.sid] != undefined);
    },
    getFingerprintsFromJingle: function(j) {
        var fpNodes = j.find('>content>transport>fingerprint');
        if (fpNodes.length < 1)
            throw new Error("Could not extract SRTP fingerprint from jingle packet");
        var fps = [];
        for (var i=0; i<fpNodes.length; i++) {
            var node = fpNodes[i];
            fps.push(node.getAttribute('hash')+' '+node.textContent);
        }
        fps.sort();
        return fps.join(';');
    },
    getStunAndTurnCredentials: function () {
        // get stun and turn configuration from server via xep-0215
        // uses time-limited credentials as described in
        // http://tools.ietf.org/html/draft-uberti-behave-turn-rest-00
        //
        // see https://code.google.com/p/prosody-modules/source/browse/mod_turncredentials/mod_turncredentials.lua
        // for a prosody module which implements this
        //
        // currently, this doesn't work with updateIce and therefore credentials with a long
        // validity have to be fetched before creating the peerconnection
        // TODO: implement refresh via updateIce as described in
        //      https://code.google.com/p/webrtc/issues/detail?id=1650
        this.connection.send(
            $iq({type: 'get', to: this.connection.domain})
                .c('services', {xmlns: 'urn:xmpp:extdisco:1'}).c('service', {host: 'turn.' + this.connection.domain}),
            function (res) {
                var iceservers = [];
                $(res).find('>services>service').each(function (idx, el) {
                    el = $(el);
                    var dict = {};
                    switch (el.attr('type')) {
                    case 'stun':
                        dict.url = 'stun:' + el.attr('host');
                        if (el.attr('port')) {
                            dict.url += ':' + el.attr('port');
                        }
                        iceservers.push(dict);
                        break;
                    case 'turn':
                        dict.url = 'turn:';
                        if (el.attr('username')) { // https://code.google.com/p/webrtc/issues/detail?id=1508
                            if (navigator.userAgent.match(/Chrom(e|ium)\/([0-9]+)\./) && parseInt(navigator.userAgent.match(/Chrom(e|ium)\/([0-9]+)\./)[2], 10) < 28) {
                                dict.url += el.attr('username') + '@';
                            } else {
                                dict.username = el.attr('username'); // only works in M28
                            }
                        }
                        dict.url += el.attr('host');
                        if (el.attr('port') && el.attr('port') != '3478') {
                            dict.url += ':' + el.attr('port');
                        }
                        if (el.attr('transport') && el.attr('transport') != 'udp') {
                            dict.url += '?transport=' + el.attr('transport');
                        }
                        if (el.attr('password')) {
                            dict.credential = el.attr('password');
                        }
                        iceservers.push(dict);
                        break;
                    }
                });
                this.ice_config.iceServers = iceservers;
            },
            function (err) {
                console.warn('getting turn credentials failed', err);
                console.warn('is mod_turncredentials or similar installed?');
            }
        );
        // implement push?
 },
 jsonStringifyOneLevel: function(obj) {
    var str = '{';
    for (var k in obj) {
        if (!obj.hasOwnProperty(k))
            continue;
        str+=(k+':');
        var prop = obj[k];
        switch(typeof prop) {
          case 'string': str+=('"'+prop+'"'); break;
          case 'number': str+=prop; break;
          case 'function': str+='(func)'; break;
          case 'object':
            if (prop instanceof Array)
                str+='(array)';
            else
                str+='(object)';
            break;
          default: str+='(unk)'; break;
        }
        str+=', ';
    }
    if (str.length > 1)
        str = str.substr(0, str.length-2)+'}';
    return str;
 }
};

function MuteInfo(affected) {
    if (affected.match(/voice/i))
        this.audio = true;
    if (affected.match(/video/i))
        this.video = true;
}

Strophe.addConnectionPlugin('jingle', JinglePlugin);
