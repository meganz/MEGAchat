
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

#define VERIFY_SID(msg)         \
    Sid sid;                    \
    msg->payloadRead(sid, 16);  \
    if (sid != mSid) return;

namespace rtcModule
{
using namespace std;
using namespace karere;
using namespace promise;
using namespace std::placeholders;

AvFlags peerMediaToObj(const char* strPeerMedia);
//==

RtcHandler::RtcHandler(chatd::Chat& chat, IGlobalEventHandler* globalHandler,
               ICryptoFunctions* crypto, const char* iceServers)
:mChat(chat), mGlobalHandler(globalHandler), mCrypto(crypto),
  mTurnServerProvider(
    new TurnServerProvider("https://" KARERE_GELB_HOST, "turn", iceServers, 3600)),
  mIceServers(new webrtc::PeerConnectionInterface::IceServers)
{
    pcConstraints.SetMandatoryReceiveAudio(true);
    pcConstraints.SetMandatoryReceiveVideo(true);
    pcConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);

//preload ice servers to make calls faster
    mTurnServerProvider->getServers()
    .then([this](ServerList<TurnServerInfo>* servers)
    {
        setIceServers(*servers);
    });

    //install handlers
    mChat.rtAddHandler(RTCMSG_CallRequest,
    [this](const chatd::RtMessage& msg, bool& keep)
    {
        onIncomingCallMsg(msg);
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

void JingleCall::handleSdpAnswer(const std::shared_ptr<RtMesageWithEndpoint>& msg)
{
    /*
    if (sess->inputQueue.get())
    {
        sess->inputQueue->push_back(iq);
        return;
    }
    */

// sid.8 encFprMac.32 sdpLen.2 sdp.sdpLen
    if (msgGetSid(*msg) != sid)
    {
        RTCM_LOG_WARNING("sdpAnswer received for unknown session, ignoring");
        return;
    }

    if (state() != kCallStateInReqWaitJingle)
        throw std::runtime_error("Unexptected session-initiate received");
    if (mTimer)
    {
        cancelTimeout(mTimer);
        mTimer = 0;
    }
    RTCM_LOG_DEBUG("received rtcAnswer from %s[0x%04x]", peer.toString().c_str(), peerClient);

    // Verify SRTP fingerprint
    const char* encFprMac = msg->payloadReadPtr(8, 32);
    auto sdpLen = msg->readPayload<uint16_t>(40);
    std::string sdp(msg->payloadReadPtr(42, sdpLen), sdpLen);

    if (!verifyMac(getFingerprintsSdp(sdp), mOwnFprMacKey, encFprMac))
    {
/*    handleSdpAnswer(sdp)
    sess->answer(jingle)
    .then([this, call]()
    {
//now handle all packets queued up while we were waiting for user's accept of the call
        processAndDeleteInputQueue(*call->mSess);
    })
*/
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
/* Call initiation sequence for group calls:
 *  1) Initiator broadcasts a call request
 *  2) Answerers broadcast a join request to everybody, with a nonce
 *  3) Clients already in the call respond to the join request with
 *    sessionOffer, containing an SDP offer, their own nonce, and a hash
 *    of the webrtc fingerprint, keyed with the joiner's nonce, and encrypted
 *    to the joiner's pubkey.
 *  4) the joiner sends an RTCMSG_sessionAnswer with an SDP answer, with a
 * webrtc fingerprint hash keyed with the session-offerer's nonce, and encrypted
 * to their public rsa key.
 *
 *  For joinin an existing call, the sequence starts from 2)
 */
void GroupHandler::handleSessionOffer(const std::shared_ptr<RtMessage>& callmsg)
{
}

struct IncomingCallRequest: public karere::WeakReferenceable<IncomingCallRequest>
{
    RtMsgHandler mElsewhereHandler;
    std::shared_ptr<RtMessage> mCallMsg;
    bool mUserAnswered = false; //set to true when user answers or rejects the call
//    Call& call;
    karere::Id mRid; //request id
    IncomingCallRequest(Call& aCall, const std::shared_ptr<RtMsg_PeerCallRequest>& aCallmsg);
    ~IncomingCallRequest() { mElsewhereHandler.remove(); }
};

class CallAnswer: public ICallAnswer, protected IncomingCallRequest::WeakRefHandle
{
protected:
    using IncomingCallRequest::WeakRefHandle::WeakRefHandle;
public:
    //ICallAnswer interface
    AvFlags peerMedia() const
    {
        if (!isValid())
            throw std::runtime_error("Incoming call request is no longer valid");
        return get()->peerMedia();
    } //TODO: implement peer media parsing
    bool reqStillValid() const
    {
        return (isValid() && !get()->userResponded());
    }
    bool answer(bool accept, AvFlags av)
    {
        return isValid() ? get()->answer(accept, av) : false;
    }
};

IncomingCallRequest::IncomingCallRequest(Call& aCall, const std::shared_ptr<RtMsg_PeerCallRequest>& aCallMsg)
    :call(aCall), mRid(aCallMsg->readPayload<uint32_t>(0)), callmsg(aCallMsg)
{
// When a client answers a call request, it broadcasts a 'handledElsewhere'
// message to all (other) clients of that user, so they destroy the incoming the call
// request.
// This handler will be removed by the destructor of this object, so it's not possible
// to have the object deleted when the handler is called
    mElsewhereHandler = manager.chat.addHandler(RTMSG_notifyCallHandled, manager.myUserid,
    [this](const std::shared_ptr<RtMessage>& msg, void*, bool& keep)
    {
        //rid.4 caller.8 callerClientId.4 answered.1
        if ((msg->readPayload<uint32_t>(0) != mRid) ||
         || (msg->readPayload<uint64_t>(4) != callmsg->userid())
         || (msg->readPayload<uint32_t>(12) != callmsg->clientid()))
            return;

        keep = false;
        auto by = msg->clientid();
        if (by == call.manager.chat.clientId()) //if it was us, ignore the message. We should have already removed this handler though.
            return;
        auto accepted = msg->readPayload<uint8_t>(8);
        call.manager.destroyIncomingReq(accepted ? Call::kRejectedElsewhere : Call::kAnsweredElsewhere);
    });
// The timer will be canceled when the request is destroyed, so we don't need
// to check for valid 'this'
    mTimer = setTimeout([this]()
    {
        call.manager.destroyIncomingReq(kUserAnswerTimeout);
    }, userAnswerTimeout+10000);

// pmsGelb = mTurnServerProvider->getServers(mIceFastGetTimeout)
    manager.updateTurnServersFromGelb(true);
}

bool IncomingCallRequest::answer(bool accept, AvFlags av)
{
    //If user answer times out or the call request is cancelled or handled by someone else,
    //the call will be immediately destroyed, so we will never reach here

    if (mUserAnswered)
        return false;

    mUserAnswered = true;
    if (!accept)
        return manager.destroyIncomingReq(kUserDeclined);

//join call
    auto wptr = getWeakHandle();
    manager.initGelbAndCrypto()
    .then([wptr, this, av]()
    {
        if (!wptr.isValid())
            return;
        joinCall(av);
        manager.destroyIncomingReq(kUserAnswered);
    })
    .fail([this, wptr](const Error& err)
    {
        if (!wptr.isValid())
            return;
        RTMGS_LOG_ERROR("Error while sending call answer response: %s", err.what());
        manager.destroyIncomingReq(kInternalError);
    });
    return true;
}

bool Call::join()
{
    assert(mState == kStateNew || mState == kStateIncomingCallReq);
    generateOwnFprNonce();
    //nonce.16 anonid.12
    RtMessage msg(RTMSG_join, 32);
    static_assert(sizeof(mOwnNonce) == 16, "own nonce size");
    msg.append(mOwnNonce, 16)
       .append(manager.rtcShared.ownAnonId, 12);

    if (!manager.chat.rtSendMessage(msg))
    {
        hangup(kErrSignallingDisconnected);
        return false;
    }

    setupIncomingOffersHandler();
    setupIncomingJoinsHandler();

    mState = kStateJoining;
    return true;
    //TODO: Cansel the 'joining' state when the first session starts media.
}

void Call::setupIncomingOffersHandler()
{
    assert(!mIncomingOffersHandler);
    mIncomingOffersHandler = chat.rtAddHandler(RTMSG_sdpOffer,
    [this](const std::shared_ptr<RtMessageWithEndpoint>& msg, bool& keep)
    {
        if (mState != kStateJoining)
            return;
        Sid sid;
        memcpy(msg->payloadReadPtr(0, 16), sid, 16);
        auto& sess = at(sid);
        if (sess)
            throw std::runtime_error("Incoming session offer: Session with sid '"+sid.toString()+"' already exists");
        sess = std::make_shared<Session>(*this, *msg, sid);
    });
}

void Call::setupIncomingJoinsHandler()
{
    assert(!mIncomingJoinsHandler);
    mIncomingJoinsHandler = chat.rtAddHandler(RTMSG_join,
    [this](const std::shared_ptr<RtMessageWithEndpoint>& msg, bool& keep)
    {
        if (mState != kStateJoining)
            return;
        auto sess = std::make_shared<Session>(*this, *msg);
        assert(sess->sid);
        auto ret = insert(sess->sid, sess);
        assert(ret.second); //clash
    });
}

// Constructor for incoming offer (as a result of a JOIN sent by us)
// sdpOffer: sid.8 avFlags.1 nonce.16 fprHash.32 anonId.12 sdpLen.2 sdp.sdpLen
void Call::Session::Session(Call& aCall, const RtMessageWithEndpoint& msg, const Sid& aSid)
    : call(aCall), isCaller(false), kStateSdpHandshake,
    sid(aSid), peer(msg.userid()), peerClient(msg.clientid()),
    mRemoteAv(msg.readPayload<uint8_t>(17))
{
    auto sdpLen = msg.readPayload<uint16_t>(77);
    if (!sdpLen)
        throw std::runtime_error("Incoming sdp offer has zero-size sdp string");
    std::string sdpOffer(msg.payloadReadPtr(79, sdpLen));
    mSdpOffer.parse(sdpOffer);

    memcpy(mPeerAnonId, msg.payloadReadPtr(65, 12), 12);
    memcpy(mPeerNonce, msg.payloadReadPtr(17, 16), 16);
    const char* encFprHash = msg.payloadReadPtr(33, 32);

    setupHangupHandler();
    createPeerConnAndAnswer(sdpOffer, encFprHash)
    .then([this]()
    {
        //sid.8 encFprHash.32 sentAv.1 sdpLen.2 sdp.sdpLen
        RtMessageWithEndpoint ans(RTMSG_megaCallAnswer, peer, peerClient, 64);
        ans.append(&sid, 16);
        ans.append(call.manager.rtcShared.ownAnonId, 16); //anonid, 16 bytes
        auto& crypto = *call.manager.rtcShared.crypto;
        char fprHash[32];
        crypto.hashMessage(mLocalSdp.fingerprints(), mPeerNonce, fprHash);
        char encFprHash[32];
        crypto.encryptHashForUser(fprHash, peer, encFprHash);

        ans.append(mSid, 16)
           .append(encFprHash, 32)
           .append<uint8_t>(call.mLocalAv.toByte()) //"media"
           .append<uint16_t>(mSdpAnswer.raw().size());
           .append(sdpAnswer.raw());
        send(ans);
        startMediaTimeoutTimer();
    })
    .fail([this](const promise::Error& err)
    {
        int code = (err.type() == PROMISE_ERR_RTCMODULE) ? err.code() : kErrorSdpHandshake;
        hangup(code, err.what());
    });
}

void Call::Session::startMediaTimeoutTimer()
{
    assert(!mTimer);
    // This timer is for the period from the sdp answer we send to receiving any media
    mTimer = setTimeout([this]()
    {
        //invalidate call after a timeout
        if (mState != hasExchangedMedia())
            return; //media timeout did not occur.
        hangup(kMediaTimeoutError);
    }, kMediaTimeout);
}

void CallManager::handleCallRequest(const std::shared_ptr<RtMessageWithEndpoint>& callmsg)
{
    if (mCall)
    {
        RTCM_LOG_WARNING("handleCallRequest: Already has call, ignoring");
        //TODO: Maybe busy signal
        return;
    }
    try
    {
        mCall.reset(new Call(*this, callmsg));
    }
    catch(std::exception& e)
    {
        RTCM_LOG_ERROR("handleCallRequest exception: %s", e.what());
        return;
    }
    assert(mCall && mCall->mIncomingRequest);
    RTCM_LOG_EVENT("global->onIncomingCallRequest");

    mCall->mHandler = mGlobalHandler->onIncomingCallRequest(
        std::make_shared<CallAnswer>(mCall->getWeakHandle()));
    if (!mCall->mHandler)
        throw std::runtime_error("onIncomingCallRequest: Application did not provide call event handler");
}

void Call::Session::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{

}
void Call::Session::onIceCandidate(std::shared_ptr<artc::IceCandText> candidate)
{
    if (mState != kStateConnecting)
        return;
    RTCM_LOG_DEBUG("%s: onIceCandidate", mSid.toString().c_str());
    //sid.8 mlineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    RtMessageWithEndpoint cand(RTCM_iceCand, peer, peerClient, candidate->candidate.size()+16);
    cand.append(mSid, RTCM_SID_LEN)
        .append<uint8_t>(candidate->sdpMLineIndex())
        .append<uint8_t>(candidate->sdpMid.size())
        .append(candidate->sdpMid)
        .append<uint16_t>(candidate->candidate.size())
        .append(candidate.candidate);
    send(cand);
}
void Call::Session::setupIceCandidateHandler()
{
    mIceCandHandler = call.manager.char.addHandler(RTCM_iceCand, peer, peerClient,
    [this](const std::shared_ptr<RtMessageWithEndpoint>& cand)
    {
      try
      {
        //sid.8 sdpMlineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
        VERIFY_SID(cand);
        auto mLineIdx = cand->payloadRead<uint8_t>(16);
        auto midLen = cand->readPlayload<uint8_t>(17);
        if (!midLen)
            throw std::runtime_error("No 'mid' present in ice candidate");
        std::string mid(cand->payloadReadPtr(18, midLen), midLen);
        auto candLen = cand->readPayload<uint16_t>(18+midLen);
        if (!candLen)
            throw std::runtime_error("Candidate string is empty");

        string text(cand->payloadReadPtr(20+midLen, candLen), candLen);
        unique_ptr<webrtc::JsepIceCandidate> cand(
          new webrtc::JsepIceCandidate(mid, mLineIdx));
        webrtc::SdpParseError err;
        if (!cand->Initialize(text, &err))
            throw runtime_error("Error parsing ICE candidate:\nline: '"+err.line+"'\nError:" +err.description);

        mPeerConn->AddIceCandidate(cand.release());
      }
      catch(std::exception& e)
      {
          RTCM_LOG_ERROR("Error handling received ICE candidate: %s", e.what());
          hangup(kErrorProtocol, "Error handling received ICE candidate");
      }
    });
}

void Jingle::processAndDeleteInputQueue(JingleSession& sess)
{
    unique_ptr<StanzaQueue> queue(sess.inputQueue.release());
    for (auto& stanza: *queue)
        onJingle(stanza);
}

// Constructor for incoming join (i.e. we should send sdp offer)
Call::Session::Session(Call& aCall, const RtMessageWithEndpoint& joinMsg)
: call(aCall), isCaller(true), peer(joinMsg.userid()), peerClient(joinMsg.clientid())
{
    assert(joinMsg.type() == RTCM_join);
    generateSid();
    memcpy(mPeerNonce, joinMsg->payloadReadPtr(0, 16), 16);
    memcpy(mPeerAnonId, joinMsg->payloadReadPtr(16, 12), 12);
    setupHangupHandler();
    setupSdpAnsHandler();
    createPeerConnAndOffer()
    .then([this]()
    {
        //sid.8 nonce.16 anonId.12 av.1 sdpLen.2 sdpOffer.sdpLen
        RtMessageWithEndpoint offer(RTCM_sdpOffer, peer, peerClient, 19+mSdpOffer.raw.size());
        offer.append(mSid, 16)
             .append(mOwnNonce, 16)
             .append(call.manager.rtcShared.ownAnonId, 12)
             .append(mLocalStream.av().toByte())
             .append<uint16_t>(mSdpOffer.raw().size())
             .append(mLocalSdp.raw());
        send(offer);
        startMediaTimeoutTimer();
    })
    .fail([this](const promise::Error& err)
    {
        int code = (err.type() == PROMISE_ERR_RTCMMODULE)
                ? err.code()
                : kErrSdpHandshake;
        hangup(code, err.msg());
    });
}

promise::Promise<void> Call::Session::createPeerConnAndOffer()
{
    std::string errors;
    if (!createPeerConnWithLocalStream(errors))
        return promise::Error(errors, kNoMediaError, PROMISE_ERR_RTCMODULE);

    return mPeerConn.createOffer(mPcConstraints
        ? mPcConstraints.get()
        : &call.manager.rtcShared.pcConstraints)
    .then([this](webrtc::SessionDescriptionInterface* sdp)
    {
        string strSdp;
        if (!sdp->ToString(&strSdp))
            return promise::Error("sdp->ToString() returned false", kInternalError, PROMISE_ERR_RTCMODULE);
        mLocalSdp.parse(strSdp);
        if (tweakEncoding(mLocalSdp))
        {
            webrtc::SessionDescriptionInterface* newSdp = parsedSdpToWebrtcSdp(mLocalSdp, sdp->type());
            delete sdp;
            sdp = newSdp;
        }
        return mPeerConn.setLocalDescription(sdp);
    });
}

void Call::Session::setupSdpAnsHandler()
{
    mSdpAnsHandler = call.manager.char.rtAddHandler(RTCM_sdpAnswer, peer, peerClient,
    [this](const std::shared_ptr<RtMessageWithEndpoint>& ans, bool& keep)
    {
        //sid.8 av.1 fprHash.32 sdpLen.2 sdpAns.sdpLen
        assert(mState == kWaitSdpAnswer);
        VERIFY_SID(ans);

        keep = false;
        const char* encFprHash = ans->payloadReadPtr(17, 32);
        auto sdpLen = ans->readPayload<uint16_t>(17);
        std::string sdp(ans->payloadReadPtr(19, sdpLen));

        setRemoteDescription("answer", sdp, encFprHash)
        .fail([this](const promise::Error& err)
        {
            int code = (err.type() == PROMISE_ERR_RTCMODULE) ? err.code() : kErrSdpHandshake;
            hangup(code, err.msg());
        });
    });
}
// we received an offer
promise::Promise<void>
Call::Session::createPeerConnAndAnswer(const std::string& sdpOffer, const char* encFprHash)
{
    std::string errors;
    if (!createPeerConnWithLocalStream(errors))
        return promise::Error(errors, kNoMediaError, PROMISE_ERR_RTCMODULE);
    setRemoteDescription("offer", sdpOffer, encFprHash)
    .then([this]()
    {
        return mPeerConn.createAnswer(call.pcConstraints
            ? call.pcConstraints.get()
            : &call.manager.rtcShared.pcConstraints)
    })
    .then([this](webrtc::SessionDescriptionInterface* sdp) mutable
    {
        checkActive("created SDP answer");
        string strSdp;
        if (!sdp->ToString(&strSdp))
            return promise::Error("sdp->ToString() returned false", kErrSdpHandshake, PROMISE_ERR_RTCMODULE);

        mLocalSdp.parse(strSdp);
        if (tweakEncoding(mLocalSdp))
        {
            auto newSdp = parsedSdpToWebrtcSdp(mLocalSdp, sdp->type());
            delete sdp;
            sdp = newSdp;
        }
        return mPeerConn.setLocalDescription(sdp)
    });
}

promise::Promise<void>
Call::Session::setRemoteDescription(const char* type, const std::string& sdp,
    const char* encFingerprintHash)
{
    try
    {
        mRemoteSdp.parse(sdp);
    }
    catch(std::runtime_error& e)
    {
        return promise::Error(e.what(), kErrSdpParse, PROMISE_ERR_RTCMODULE);
    }
    call.manager.rtcShared.crypto->decryptMessage(encFingerprintHash, fprHash, 32);
    if (!verifyHash(mRemoteSdp.fingerprints(), mOwnNonce, fprHash))
        return promise::Error("fingerprint verification failed", kErrFprVerifFail, PROMISE_ERR_RTCMODULE);

    unique_ptr<webrtc::JsepSessionDescription> jsepSdp(
        new webrtc::JsepSessionDescription(type));
    webrtc::SdpParseError error;
    if (!jsepSdp->Initialize(mRemoteSdp.raw(), &error))
    {
        std::string msg = "Error parsing SDP: line='"+error.line+"'\nError: "+error.description;
        return promise::Error(msg, kErrSdpParse, PROMISE_ERR_RTCMODULE);
    }
    return mPeerConn.setRemoteDescription(jsepSdp.release());
}

bool Call::Session::createPeerConnWithLocalStream(std::string& errors)
{
    if (!startLocalStream(true, errors))
        return false;

    mPeerConn = artc::myPeerConnection<Call::Session>(
        call.manager.rtcShared.mIceServers,
        *this, call.pcConstraints
            ? call.pcConstraints.get()
            : &manager.rtcShared.pcConstraints);
    if (!mPeerConn->AddStream(*mLocalStream))
    {
        errors = "mPeerConn->AddStream() returned false";
        return false;
    }
    return true;
}
void Call::Session::setupHangupHandler()
{
    //sid.8 code.1 reasonLen.2 reason.reasonLen
    mHangupHandler = call.manager.chat.rtAddHandler(RTMSG_hangup, peer, peerClient,
    [this](const std::shared_ptr<RtMessageWithEndpoint>& msg, bool& keep)
    {
        VERIFY_SID(msg);
        auto termcode = msg->readPayload<uint8_t>(16);
        auto reasonLen = msg->readPayload<uint16_t>(17);
        std::string reason = reasonLen
            ? std::string(msg->payloadReadPtr(19, reasonLen))
            : "unknown";
        destroy(termcode | kPeer, reason);
    });
}
//incoming
Call::Call(CallManager& aManager, const std::shared_ptr<RtMessageWithEndpoint>& callmsg)
    :manager(aManager), mState(kStateInReq), mIncominRequest(new IncomingCallRequest(*this, callmsg))
{}

bool PeerCallManager::hangup(TermCode termcode, const std::string& reason)
{
    if (!mCall)
        return false;
    mCall->maybeSendHangup(termcode, reason);
    mCall.reset(); //deletes this
}

bool JingleCall::maybSendHangup(TermCode termcode, const std::string& reason)
{
    if (mIncomingRequest && !mIncomingRequest->userAnswered)
    //we haven't answered the call, dont sent hangup and let other device pick it
        return false;

    RtMessageWithEndpoint hup(RTMSG_hangup, mPeer, mPeerClient, 3+reason.size());
    hup.append<uint8_t>(termcode);
    hup.append<uint16_t>(reason.size());
    if (!reason.empty())
        hup.append(reason);
    mChat.rtSendMessage(hup);
    return true;
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
