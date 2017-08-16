#include "webrtc.h"
#define SUB_LOG_DEBUG(fmtString,...) RTCM_LOG_DEBUG("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_INFO(fmtString,...) RTCM_LOG_INFO("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_WARNING(fmtString,...) RTCM_LOG_WARNING("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_ERROR(fmtString,...) RTCM_LOG_DEBUG("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_EVENT(fmtString,...) RTCM_LOG_EVENT("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)

#define CONCAT(a, b) a ## b
#define FIRE_EVENT(type, evName, ...)                \
do {                                                 \
    std::string msg = logName() +" event " #evName;  \
    if (strcmp(#evName, "onDestroy") == 0) {         \
        msg += " (" + termCodeFirstArgToString(##__VA_ARGS__) + ')'; \
    }                                                \
    CONCAT(type, _LOG_DEBUG)("%s", msg.c_str());     \
    try                                              \
    {                                                \
        mHandler->##evName(##__VA_ARGS__);           \
    } catch (std::exception& e) {                    \
        RTCM_LOG_ERROR("%s event handler for'" #evName "' threw exception:\n %s", type, e.what()); \
    }                                                \
} while(0)
#define RTCM_LOG_DEBUG(
struct CallerInfo
{
    Id chatid;
    Id callid;
    chatd::Connection& shard;
    Id callerUser;
    uint32_t callerClient;
};

RtcModule::RtcModule(karere::Client& client, GlobalHandler* handler,
  ICryptoFunctions& crypto, const std::string& iceServers)
: mClient(client), mCrypto(crypto), mHandler(handler), mIceServers(iceServers),
  mOwnAnonId(client.myHandle()),
  mTurnServerProvider(new TurnServerProvider(api, "turn", iceServers, 3600))
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
    mClient.chatd->setRtcHandler(*this);
}
/* chatd.on('membersUpdated', function(event) {
    self.onUserJoinLeave(event.chatId, event.userId, event.priv);
});
*/
void RtcModule::onDisconnect(chatd::Connection& conn)
{
    // notify all relevant calls
    for (auto chatid in conn.chatIds()) {
        auto it = mCalls.find(chatid);
        if (it == mCalls.end())
                continue;
        auto& call = it.second;
        if (call.state() < Call::State::kTerminating) {
            call.destroy(Term::kErrNetSignalling, false);
        }
    }
}


void RtcModule::setIceServers(ServerList<TurnServerInfo>& servers)
{
    mIceServers = servers;
}

void RtcModule::handleMessage(chatd::Connection& conn, StaticBuffer& msg)
{
    // (opcode.1 chatid.8 userid.8 clientid.4 len.2) (type.1 data.(len-1))
    //              ^                                          ^
    //          header.hdrlen                             payload.len
    try
    {
        RtMessage packet =
        {
            .shard = conn,
            .opcode = msg.read<uint8_t>(0),
            .type = msg.read<uint8_t>(kRtmsgHdrLen),
            .chatid = msg.read<uint64_t>(1),
            .userid = msg.read<uint64_t>(9),
            .clientid = msg.read<uint32_t>(17),
            .payload = Buffer(msg.readPtr(kRtmsgHdrLen+1,
                msg.read<uint16_t>(kRtmsgHdrLen-2), msg.dataSize()-kRtMsgHdrLen-1))
        };
        auto chat = mChatd.chatFromId(chatid);
        if (!chat) {
            RTCM_LOG_ERROR(
                "Received %s for unknown chatid %s. Ignoring", msg.opcodeName(),
                msg.chatid().toString().c_str());
            return;
        }
        // this is the only command that is not handled by an existing call
        if (msg.type() == RTCMD_CALL_REQUEST) {
            assert(msg.opcode() == OP_BROADCAST);
            msgCallRequest(packet);
            return;
        }
        auto it = mCalls.find(chatid);
        if (it == mCalls.end())
        {
            RTCM_LOG_ERROR("Received %s for a chat that doesn't currently have a call, ignoring",
                msg.typeToString());
            return;
        }
        it->second.handleMsg(packet);
    }
    catch (std::exception& e) {
        RTCM_LOG_ERROR("rtMsgHandler: exception:", e.what());
    }
}

void RtcModule::onUserJoinLeave(karere::Id chatid, karere::Id userid, chatd::Priv priv)
{
}

void RtcModule::msgCallRequest(RtMessage& packet)
{
    if (packet.userid == mChatd.userId) {
        RTMSG_LOG_DEBUG("Ignoring call request from another client of our user");
        return;
    }
    packet.callid = packet.payload.read<uint64_t>(0);
    assert(packet.callid);
    if (!mCalls.empty()) {
        assert(mCalls.size() == 1);
        auto existingChatid = mCalls.begin()->first;
        auto& existingCall = mCalls.begin()->second;
        if (existingChatid == packet.chatid && existingCall.state() < Call::kStateTerminating)
        {
            bool answer = mHandler->onAnotherCall(existingCall, packet);
            if (answer) {
                existingCall.hangup();
                mCalls.erase(existingChatid);
            }  else {
                cmdEndpoint(RTCMD_CALL_REQ_DECLINE, packet, packet.callid, TermCode::kBusy);
                return;
            }
        }
    }
//    Call::Call(RtcModule& rtcModule, Id chatid, chatd::Connection& shard,
//        uint64_t callid, bool isGroup,
//        bool isJoiner, CallHandler* handler, Id callerUser, uint32_t callerClient)

    auto ret = mCalls.emplace(packet.chatid, std::make_shared<Call>(*this,
        packet.chatid, packet.shard, packet.callid,
        mHandler->isGroupChat(packet.chatid),
        true, nullptr, packet.userid, packet.clientid));
    assert(ret.second);
    Call& call = *ret.first;
    call.mHandler = mHandler->onCallIncoming(call);
    assert(call.mHandler);
    assert(call.state() == Call::kStateRingIn);
    cmdEndpoint(RTCMD_CALL_RINGING, packet, packet.callid);
    auto wcall = call.weakHandle();
    setTimeout([wcall]
    {
        if (!wcall.isValid())
            return;
        if (wcall->state() !== Call::kStateRingIn) {
            return;
        }
        wcall->destroy(TermCode::kAnswerTimeout, false);
    }, kCallAnswerTimeout+4000); // local timeout a bit longer that the caller
}
template <class... Args>
void RtcModule::cmdEndpoint(uint8_t type, const RtMessage& info, Args... args)
{
    assert(info.chatid);
    assert(info.fromUser);
    assert(info.fromClient);
    RtMessageComposer msg(info.opcode, type, info.chatid(), info.userid(), info.clientid());
    msg.payloadAppend(args...);

    if (!info.shard.sendCommand(msg)) {
        throw std::exception("cmdEndpoint: Send error trying to send command " + std::string(msg.typeToString()));
    }
}

void RtcModule::removeCall(Call& call)
{
    auto it = mCalls.find(call.chatid);
    if (it == mCalls.end()) {
        throw std::runtime_error("Call with chatid "+call.chatid.toString()+" not found");
    }
    if (&call != it->second.get() || it->second->id != call.id) {
        RTCM_LOG_DEBUG("removeCall: Call has been replaced, not removing");
        return;
    }
    mCalls.erase(call.chatid);
}
std::shared_ptr<artc::LocalStreamHandle>
RtcModule::getLocalStream(AvFlags av, std::string& errors)
{
    const auto& devices = mDeviceManager.inputDevices();
    if (devices.video.empty() || mVideoInDeviceName.empty())
    {
        mVideoInput.reset();
    }
    else if (!mVideoInput || mVideoInput.mediaOptions().device.name != mVideoInDeviceName)
    try
    {
        auto device = getDevice(mVideoInDeviceName, devices.video);
        if (!device)
        {
            device = &devices.video[0];
            errors.append("Configured video input device '").append(mVideoInDeviceName)
                  .append("' not present, using default device\n");
        }
        auto opts = std::make_shared<artc::MediaGetOptions>(*device, mediaConstraints);

        mVideoInput = deviceManager.getUserVideo(opts);
    }
    catch(exception& e)
    {
        mVideoInput.reset();
        errors.append("Error getting video device: ")
              .append(e.what()?e.what():"Unknown error")+='\n';
    }

    if (devices.audio.empty() || mAudioInDeviceName.empty())
    {
        mAudioInput.reset();
    }
    else if (!mAudioInput || mAudioInput.mediaOptions().device.name != mAudioInDeviceName)
    try
    {
        auto device = getDevice(mAudioInDeviceName, devices.audio);
        if (!device)
        {
            errors.append("Configured audio input device '").append(mAudioInDeviceName)
                  .append("' not present, using default device\n");
            device = &devices.audio[0];
        }
        mAudioInput = deviceManager.getUserAudio(
                std::make_shared<artc::MediaGetOptions>(*device, mediaConstraints));
    }
    catch(exception& e)
    {
        mAudioInput.reset();
        errors.append("Error getting audio device: ")
              .append(e.what()?e.what():"Unknown error")+='\n';
    }
    if (!mAudioInput && !mVideoInput)
        return nullptr;

    std::shared_ptr<artc::LocalStreamHandle> localStream =
        std::make_shared<artc::LocalStreamHandle>(
            mAudioInput?mAudioInput.getTrack():nullptr,
            mVideoInput?mVideoInput.getTrack():nullptr);
    localStream.setAvState(av);
    return localStream;
}

std::shared_ptr<Call> RtcModule::startOrJoinCall(karere::Id chatid, AvState av,
    CallEventHandler* handler, bool isJoin)
{
    assert(handler);
    bool isGroup = mHandler->isGroupChat(chatid);
    auto& shard = mChatd.chats(chatid).connection();
    auto callIt = mCalls.find(chatid);
    if (callIt != mCalls.end())
    {
        RTCM_LOG_WARNING("There is already a call in this chatroom, destroying it");
        callIt->second->hangup();
        mCalls.erase(chatid);
    }
/*    Call::Call(RtcModule& rtcModule, Id chatid, chatd::Connection& shard,
        uint64_t callid, bool isGroup,
        bool isJoiner, CallHandler* handler, Id callerUser, uint32_t callerClient)
*/
    auto call = std::make_shared<Call>(*this, chatid, shard,
        mCrypto.random<uint64_t>(), isGroup, isJoin, handler);

    mCalls[chatid] = call;
    call->startOrJoin(av);
    return call;
}

std::shared_ptr<Call> RtcModule::joinCall(karere::Id chatid, AvState av, CallHandler* handler)
{
    return startOrJoinCall(chatid, av, handler, true);
}
std::shared_ptr<Call> RtcModule::startCall(karere::Id chatid, AvState av, CallHandler* handler)
{
    return startOrJoinCall(chatid, av, handler, false);
}

RtcModule::onUserOffline(Id chatid, Id userid, uint32_t clientid)
{
    auto it = mCalls.find(chatid);
    if (it != mCalls.end())
    {
        it->second->onUserOffline(userid, clientid);
    }
}

void RtcModule::onShutdown()
{
    RTCM_LOG_DEBUG("Shutting down....");
    for (auto& item: mCalls) {
        auto& call = item.second;
        if (call->state() == Call::kStateRingIn)
        {
            assert(call->sessions.empty());
        }
        call->destroy(TermCode::kAppTerminating, call->state() != Call::kStateRingIn);
    }
    RTCM_LOG_DEBUG("Shutdown complete");
}

Call::Call(RtcModule& rtcModule, Id chatid, chatd::Connection& shard,
    uint64_t callid, bool isGroup,
    bool isJoiner, CallHandler* handler, Id callerUser, uint32_t callerClient)
: mManager(rtcModule), mChatid(chatid), mId(callid), mHandler(handler),
  mShard(shard), mIsGroup(isGroup), mIsJoiner(isJoiner), // the joiner is actually the answerer in case of new call
{
//    this.sessions = {};
//    this.sessRetries = {};
    if (isJoiner)
    {
        mState = kStateRingIn;
        mCallerUser = callerUser;
        mCallerClient = callerClient;
        assert(mCallerUser);
        assert(mCallerClient);
    }
    else
    {
        mState = kStateInitial;
        assert(!callerUser);
        assert(!callerClient);
    }
}

void Call::handleMsg(RtMessage& packet)
{
    switch (packet.type())
    {
        case RTCMD_CALL_TERMINATE:
            msgCallTerminate(packet);
            return;
        case RTCMD_SESSION:
            msgSession(packet);
            return;
        case RTCMD_JOIN:
            msgJoin(packet);
            return;
        case RTCMD_CALL_RINGING:
            msgRinging(packet);
            return;
        case RTCMD_CALL_REQ_DECLINE:
            msgCallReqDecline(packet);
            return;
        case RTCMD_CALL_REQ_CANCEL:
            msgCallReqCancel(packet);
            return;
    }
    auto& data = packet.payload;
    assert(data.dataLength() >= 8); // must start with sid.8
    auto sid = data.read<uint64_t>(0);
    auto sessIt = mSessions.find(sid);
    if (sessIt == mSessions.end())
    {
        SUB_LOG_WARN("Received " + msg.typeStr() +
            " for an existing call but non-existing session %s",
            base64urlencode(&sid, sizeof(sid)));
        return;
    }
    sess.handleMsg(packet);
}

void Call::setState(State newState)
{
    State oldState = mState;
    if (oldState == newState) {
        return;
    }

    assertStateChange(
        oldState,
        newState,
        CallStateAllowedStateTransitions
    );

    mState = newState;

    SUB_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(newState));
    FIRE_EVENT(CALL, onStateChange, mState);
}

void Call::getLocalStream(AvState av)
{
    setState(Call::kStateWaitLocalStream);
    // getLocalStream currently never fails - if there is error, stream is a string with the error message
    std::string errors;
    mLocalStream = mManager.getLocalStream(av, errors);
    if (!errors.empty())
    {
        SUB_LOG_WARNING("There were some error getting local stream: %s", errors.c_str());
    }
    setState(Call::kStateHasLocalStream);
}

void Call::msgCallTerminate(RtMessage& packet)
{
    if (packet.payload.dataSize() < 1) {
        SUB_LOG_ERROR("Ignoring CALL_TERMINATE without reason code");
        return;
    }
    auto code = packet.payload.read<uint8_t>(0);
    bool isCallParticipant = false;
    for (auto& item: mSessions)
    {
        auto& sess = item->second;
        if (sess.peer == packet.fromUser() && sess.peerClient == packet.fromClient())
        {
            isCallParticipant = true;
            break;
        }
    }
    if (!isCallParticipant) {
        SUB_LOG_WARNING("Received CALL_TERMINATE from a client that is not in the call, ignoring");
        return;
    }
    destroy(code | TermCode::kPeer, false);
}
void Call::msgCallReqDecline(RtMessage& packet)
{
    // callid.8 termcode.1
    assert(packet.payload.dataSize() >= 9);
    auto code = packet.readPayload<uint8_t>(8);
    if (code == TermCode::kCallRejected)
    {
        handleReject(packet);
    }
    else if (code == TermCode::kBusy)
    {
        handleBusy(packet);
    }
    else
    {
        SUB_LOG_WARNING("Ingoring CALL_REQ_DECLINE with unexpected termnation code", termCodeToStr(code));
    }
}

void Call::msgCallReqCancel(RtMessage& packet)
{
    if (mState >= Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected CALL_REQ_CANCEL while in state %s", stateToStr(mState));
        return;
    }
    assert(mFromUser);
    assert(mFromClient);
    // CALL_REQ_CANCEL callid.8 reason.1
    if (mFromUser != packet.fromUser() || mFromClient !== packet.fromClient())
    {
        SUB_LOG_WARNING("Ignoring CALL_REQ_CANCEL from a client that did not send the call request");
        return;
    }
    assert(packet.payloadLen() >= 9);
    auto callid = packet.payloadRead<uint64_t>(0);
    if (callid != mId) {
        SUB_LOG_WARNING("Ignoring CALL_REQ_CANCEL for an unknown request id");
        return;
    }
    auto term = packet.readPayload<uint8_t>(8);
    destroy(term | TermCode::kPeer, false);
}

void Call::handleReject(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ingoring unexpected CALL_REJECT while in state %s", stateToStr(mState));
        return;
    }
    if (mIsGroup || !mSessions.empty())
    {
        return;
    }
    destroy(TermCode::kCallRejected | TermCode::kPeer, false);
}

void Call::msgRinging(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected RINGING");
        return;
    }
    if (!mRingOutUsers)
    {
        mRingOutUsers.reset(new std::set<Id>());
        mRingOutUsers->insert(fromUser);
        clearCallOutTimer();
        auto wptr = weakHandle();
        mCallOutTimer = setTimeout([wptr, this] {
            if (wptr.deleted() || mState != Call::kStateReqSent)
            {
                return;
            }
            hangup(TermCode::kAnswerTimeout); // TODO: differentiate whether peer has sent us RINGING or not
        }, RtcModule::kCallAnswerTimeout);
    }
    else
    {
        self.ringOutUsers->insert(fromUser);
    }
    FIRE_EVENT(CALL, onRingOut, fromUser);
}

void Call::clearCallOutTimer()
{
    if (!mCallOutTimer) {
        return;
    }
    clearTimeout(mCallOutTimer);
    mCallOutTimer = 0;
}

void Call::handleBusy(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected BUSY when in state %s", stateToStr(mState));
        return;
    }
    if (!this.isGroup && Object.keys(this.sessions).length < 1) {
        this._destroy(Term.kBusy | Term.kPeer, false);
    } else {
        this.logger.warn("Ignoring incoming BUSY for a group call or one with already existing session(s)");
    }
}

void Call::msgSession(RtMessage& packet)
{
    if (mState != Call::kStateJoining && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected SESSION");
        return;
    }
    setState(Call::kStateInProgress);
    auto sid = packet.readPayload<uint64_t>(8);
    if (mSessions.find(sid) != mSessions.end())
    {
        SUB_LOG_ERROR("Received SESSION with sid of an existing session (%s), ignoring", base64urlencode(&sid, sizeof(sid)));
        return;
    }
    auto sess = std::make_shared<Session>(*this, packet);
    mSessions[sid] = sess;
    notifyNewSession(*sess);
    sess->sendOffer();
}

void Call::notifyNewSession(Session& sess)
{
    sess.mHandler = mHandler->onNewSession(sess);
    if (!mCallStartSignalled) {
        mCallStartSignalled = true;
        FIRE_EVENT(CALL, onCallStarting);
    }
}

void Call::msgJoin(RtMessage& packet)
{
    if (mState == CallkStateRingIn && packet.fromUser() == mManager.chatd.userId)
    {
        destroy(TermCode::kAnsElsewhere, false);
    }
    else if (mState == Call::kStateInProgress || mState == Call::kStateReqSent)
    {
        packet.callid = packet.payloadRead<uint64_t>(0);
        assert(packet.callid);
        if (mState == Call::kStateReqSent)
        {
            setState(Call::kStateInProgress);
        }
        // create session to this peer
        auto sess = std::make_shared<Session>(*this, packet);
        mSessions[sess->sid] = sess;
        notifyNewSession(*sess);
        sess->createRtcConn();
        sess->sendCmdSession(packet);
    }
    else
    {
        SUB_LOG_WANRING("Ingoring unexpected JOIN");
        return;
    }
}
promise::Promise<void> Call::gracefullyTerminateAllSessions(TermCode code)
{
    std::vector<Promise<void>> promises;
    for (auto& item: mSessions)
    {
        promises.push(item->second->terminateAndDestroy(code));
    }
    return Promise::when(promises)
    .fail([](promise::Error& err)
    {
        assert(false); // terminateAndDestroy() should never fail
    });
}

Promise<void> Call::waitAllSessionsTerminated(TermCode code)
{
    // if the peer initiated the call termination, we must wait for
    // all sessions to go away and remove the call
    for (auto& item: mSessions) {
        item->second->setState(Session::kStateTerminating);
    }
    auto wptr = weakHandle();
    struct Ctx
    {
        int count = 0;
        megaHandle timer;
        Promise<void> pms;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->timer = setInterval([wptr, this, ctx, code]() {
        if (wptr.deleted())
            return;
        if (++ctx->count > 7)
        {
            cancelTimer(ctx->timer);
            SUB_LOG_ERROR("Timed out waiting for all sessions to terminate, force closing them");
            for (auto& item: mSessions) {
                item->second->destroy(code);
            }
            ctx->pms.resolve();
            return;
        }
        if (!mSessions.empty())
        return;
        cancelTimer(ctx->timer);
        ctx->pms.resolve();
    });
    return ctx->pms;
}

Promise<void> Call::destroy(TermCode code, bool weTerminate, const string& msg)
{
    if (mState == Call::kStateDestroyed)
    {
        return promise::_Void();
    }
    else if (mState == Call::kStateTerminating)
    {
        assert(!mDestroyPromise.done());
        return mDestroyPromise;
    }
    if (!msg.empty())
    {
        SUB_LOG_DEBUG("Destroying call due to: %s", msg.c_str());
    }

    setState(Call::kStateTerminating);
    clearCallOutTimer();

    Promise<void> pms(promise::_Empty);
    if (weTerminate) {
        if (!mIsGroup) //TODO: Maybe do it also for group calls
        {
            cmdBroadcast(RTCMD_CALL_TERMINATE, code);
        }
        // if we initiate the call termination, we must initiate the
        // session termination handshake
        pms = gracefullyTerminateAllSessions(code);
    } else {
        pms = waitAllSessionsTerminated(code);
    }
    auto wptr = weakHandle();
    mDestroyPromise = pms.then([wptr, this]()
    {
        if (wptr.deleted())
            return;
        assert(mSessions.empty());
        stopIncallPingTimer();
        setState(Call::kStateDestroyed);
        FIRE_EVENT(Call, onDestroy, code & 0x7f, !!(code & 0x80), msg);// jscs:ignore disallowImplicitTypeConversion
        mManager.removeCall(*this);
    });
    return mDestroyPromise;
}
template <class... Args>
bool Call::cmdBroadcast(uint8_t type, Args... args)
{
    RtMessageComposer msg(RTMSG_BROADCAST, type, chatid, 0, 0);
    msg.payloadAppend(args...);
    if (mShard.sendCommand(msg))
    {
        return true;
    }
    auto wptr = weakHandle();
    marshallCall([wptr, this]()
    {
        destroy(TermCode::kErrNetSignalling, true);
    });
    return false;
}

void Call::broadcastCallReq()
{
    if (mState >= Call::kStateTerminating)
    {
        SUB_LOG_WARNING("broadcastCallReq: Call terminating/destroyed");
        return;
    }
    assert(mState == Call::kStateHasLocalStream);
    if (!cmdBroadcast(RTCMD_CALL_REQUEST, mId))
    {
        return false;
    }

    setState(Call::kStateReqSent);
    startIncallPingTimer();
    auto wptr = weakHandle();
    mCallOutTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted() || mState != Call::kStateReqSent)
            return;

        destroy(TermCode::kRingOutTimeout, true);
    }, RtcModule::kRingOutTimeout);
    return true;
}

void Call::startIncallPingTimer()
{
    assert(!mInCallPingTimer);
    auto wptr = weakHandle();
    mInCallPingTimer = setInterval([this, wptr]()
    {
        if (!mShard.sendCommand(Command(OP_INCALL) + chatid + mManager.chatd.userId + self.shard.clientId))
        {
            destroy(TermCode::kErrNetSignalling, true);
        }
    }, RtcModule.kIncallPingInterval);
}

void Call::stopIncallPingTimer()
{
    if (mInCallPingTimer)
    {
        clearInterval(mInCallPingTimer);
        mInCallPingTimer = 0;
    }
    mShard.sendCommand(OP_ENDCALL, mChatid + mManager.chatd.userId + mShard.clientId);
}

void Call::removeSession(Session& sess, TermCode reason)
{
    mSessions.erase(sess.sid);
    if (mState == Call::kStateTerminating)
        return;

    // if no more sessions left, destroy call even if group
    if (mSessions.empty())
    {
        destroy(reason, false);
        return;
    }
    // upon session failure, we swap the offerer and answerer and retry
    if (mState != Call::kStateTerminating && mIsGroup && isTermError(reason)
         && !sess.mIsJoiner)
    {
        EndpointId endpointId(sess.peer, sess.peerClient);
        if (mSessRetries.find(endpointId) == mSessRetries.end())
        {
            mSessRetries[endpointId] = 1;
        } else
        {
            mSessRetries[endpointId]++;
        }
        auto wptr = weakHandle();
        auto peer = sess.peer;
        setTimeout([this, wptr, peer]() {

            if (wptr.deleted())
                return;
            join(peer);
        }, 500);
    }
}
void Call::startOrJoin(AvFlags av)
{
    std::string errors;
    mLocalStream = getLocalStream(av, errors);
    if (!errors.empty())
    {
        SUB_LOG_ERROR("Error(s) getting local stream: %s", errors.c_str());
    }
    if (mIsJoiner) {
        join();
    } else {
        broadcastCallReq();
    }
}
template <class... Args>
bool Call::cmd(uint8_t type, Id userid, uint32_t clientid, Args... args)
{
    assert(userid);
    uint8_t opcode = clientid ? OP_RTMSG_ENDPOINT : OP_RTMSG_USER;
    RtMessageComposer msg(opcode, type, mChatId, userid, clientid);
    msg.payloadAppend(args...);
    return mShard.sendCommand(msg);
}

bool Call::join(Id userid)
{
    assert(mState == Call::kStateHasLocalStream);
    // JOIN:
    // chatid.8 userid.8 clientid.4 dataLen.2 type.1 callid.8 anonId.8
    // if userid is not specified, join all clients in the chat, otherwise
    // join a specific user (used when a session gets broken)
    setState(Call::kStateJoining);
    bool sent = userid
            ? cmd(RTCMD_JOIN, userid, 0, mId, mManager.ownAnonId)
            : cmdBroadcast(RTCMD_JOIN, mId, mManager.ownAnonId);
    if (!sent)
    {
        marshallCall([this]()
        {
            destroy(TermCode::kErrNetSignalling, true);
        });
        return false;
    }
    startIncallPingTimer();
    // we have session setup timeout timer, but in case we don't even reach a session creation,
    // we need another timer as well
    setTimeout([wptr, this]()
    {
        if (mState <= Call::kStateJoining)
        {
            destroy(TermCode::kErrProtoTimeout, true);
        }
    }, RtcModule::kSessSetupTimeout);
    return true;
}

bool Call::answer(AvFlags av)
{
    if (mState != Call::kStateRingIn)
    {
        SUB_LOG_WARNING("answer: Not in kRingIn state, nothing to answer");
        return false;
    }
    assert(mIsJoiner);
    return startOrJoin(av);
}

void Call::hangup(TermCode reason)
{
    TermCode term;
    switch (mState)
    {
    case kStateReqSent:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kCallReqCancel;
        }
        else
        {
            assert(reason == TermCode::kCallReqCancel || reason == TermCode::kAnswerTimeout);
        }
        cmd(RTCMD_CALL_REQ_CANCEL, 0, 0, mId, reason);
        destroy(reason, false);
        return;
    case kStateRingIn:
        if (reason == TermCode::kInvalid)
        {
            term = TermCode::kCallRejected;
        }
        else if (reason == TermCode::kBusy)
        {
            term = TermCode::kBusy;
        }
        else
        {
            assert(false && "Hangup reason can only be undefined or kBusy when hanging up call in state kRingIn");
        }
        assert(mSessions.empty());
        cmd(RTCMD_CALL_REQ_DECLINE, mCallerUser, mCallerClient, mId, term);
        destroy(term, false);
        return;
    case kStateJoining:
    case kStateInProgress:
    case kStateWaitLocalStream:
        // TODO: Check if the sender is the call host and only then destroy the call
        term = TermCode::kUserHangup;
        break;
    case kStateTerminating:
    case kStateDestroyed:
        SUB_LOG_DEBUG("hangup: Call already terminating/terminated");
        return;
    default:
        term = TermCode::kUserHangup;
        SUB_LOG_WARNING("Don't know what term code to send in state %s", stateString(mState));
        break;
    }
    // in any state, we just have to send CALL_TERMINATE and that's all
    destroy(term, true);
}

void Call::hangupAllCalls()
{
    for (auto& item: mCalls)
    {
        item->second->hangup();
    }
}

void Call::onUserOffline(Id userid, uint32_t clientid)
{
    if (mState == kStateRingIn)
    {
        destroy(TermCode::kCallReqCancel, false);
        return;
    }
    for (auto& item: mSessions)
    {
        auto sess = item->second;
        if (sess.peer == userid && sess.peerClient == clientid)
        {
            marshallCall([sess]()
            {
                sess.terminateAndDestroy(TermCode::kErrUserOffline | TermCode::kPeer);
            });
            return;
        }
    }
}

void Call::notifySessionConnected(Session& sess)
{
    if (mHasConnectedSession) {
        return;
    }
    mHasConnectedSession = true;
    FIRE_EVENT(CALL, onCallStarted);
}

void Call::muteUnmute(AvFlags av)
{
    if (!mLocalStream)
    {
        return;
    }
    var oldAv = mLocalStream.effectiveAv();
    mLocalStream->setAvState(av);
    av = mLocalStream.effectiveAv();
    if (oldAv == av)
    {
        return;
    }
    for (auto& item: mSessions)
    {
        item.second->sendAv(av);
    }
}

AvFlags Call::localAv()
{
    return mLocalStream ? mLocalStream.effectiveAv() : AvFlags(0);
}

/** Protocol flow:
    C(aller): broadcast RTCMD.CALL_REQUEST callid.8 avflags.1
       => state: CallState.kReqSent
    A(nswerer): send RINGING
       => state: CallState.kRingIn
    C: may send RTCMD.CALL_REQ_CANCEL callid.8 reason.1 if caller aborts the call request.
       The reason is normally Term.kCallReqCancel or Term.kAnswerTimeout
    A: may send RTCMD.CALL_REQ_DECLINE callid.8 reason.1 if answerer rejects the call
    == (from here on we can join an already ongoing group call) ==
    A: broadcast JOIN callid.8 anonId.8
        => state: CallState.kJoining
        => isJoiner = true
        Note: In case of joining an ongoing call, the callid is generated locally
          and does not match the callid if the ongoing call, as the protocol
          has no way of conveying the callid of the ongoing call. The responders
          to the JOIN will use the callid of the JOIN, and not the original callid.
          The callid is just used to match command/responses in the call setup handshake.
          Once that handshake is complete, only session ids are used for the actual 1on1 sessions.
        In case of 1on1 call other clients of user A will receive the answering client's JOIN (if call
        was answered), or TERMINATE (in case the call was rejected), and will know that the call has
        been handled by another client of that user. They will then dismiss the "incoming call"
        dialog.
          A: broadcast RTCMD.CALL_REQ_HANDLED callid.8 ans.1 to all other devices of own userid
    C: send SESSION callid.8 sid.8 anonId.8 encHashKey.32 actualCallId.8
        => call state: CallState.kInProgress
        => sess state: SessState.kWaitSdpOffer
    A: send SDP_OFFER sid.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        => call state: CallState.kInProress
        => sess state: SessState.kWaitSdpAnswer
    C: send SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
        => state: SessState.kWaitMedia
    A and C: exchange ICE_CANDIDATE sid.8 midLen.1 mid.midLen mLineIdx.1 candLen.2 iceCand.candLen
    A or C: may send MUTE sid.8 avState.1 (if user mutes/unmutes audio/video).
        Webrtc does not have an in-band notification of stream muting
        (needed to update the GUI), so we do it out-of-band
    A or C: send SESS_TERMINATE sid.8 reason.1
        => state: SessState.kTerminating
    C or A: send SESS_TERMINATE_ACK sid.8
        => state: Sess.kDestroyed
    A or C: close webrtc connection upont receipt of terminate ack,
        or after a timeout of 1-2 seconds. The terminate ack mechanism prevents
        the peer from thinking that the webrtc connection was closed
        due to error
        => state: Sess.kDestroyed
@note avflags of caller are duplicated in RTCMD.CALL_REQUEST and RTCMD.SDP_OFFER
The first is purely informative, to make the callee aware what type of
call the caller is requesting - audio or video.
This is not available when joining an existing call.
*/
Session::Session(Call& call, RtMessage& packet)
:mCall(call), mPeer(packet.userid), mPeerClient(packet.clientid)
{
    // Packet can be RTCMD_JOIN or RTCMD_SESSION
    call.manager.crypto.random(&mOwnSdpKey);
    if (packet.type == RTCMD_JOIN)
    {
        // peer will send offer
        // JOIN callid.8 anonId.8
        mIsJoiner = false;
        mSid = call.manager.crypto.random<uint64_t>();
        mState = kStateWaitSdpOffer;
        mPeerAnonId = packet.payloadRead<uint64_t>(8);
    }
    else
    {
        // SESSION callid.8 sid.8 anonId.8 encHashKey.32
        assert(packet.type == RTCMD_SESSION);
        mIsJoiner = true;
        mSid = packet.payloadRead<uint64_t>(8);
        mState = kStateWaitSdpAnswer;
        assert(packet.payloadSize() >= 56);
        mPeerAnonId = packet.payloadRead<uint64_t>(16);
        uint8_t encKey[32];
        packet.payloadRead(24, 32, encKey);
        call.manager.crypto.decryptNonceFrom(mPeer, encKey, mPeerHashKey);
    }
    mName = "sess[" + base64urlencode(mSid) + "]";
    auto wptr = weakHandle();
    mSetupTimer = setTimeout([wptr, this] {
        if (wptr.deleted())
            return;
        if (mState < kStateInProgress) {
            terminateAndDestroy(TermCode::kErrProtoTimeout);
        }
    }, RtcModule::kSessSetupTimeout);
}

bool Session::sendCmdSession(RtMessage& joinPacket)
{
    HashKey encKey;
    mCall.manager.crypto.encryptKeyTo(mPeer, mOwnHashKey, encKey);
    // SESSION callid.8 sid.8 anonId.8 encHashKey.32
   return mCall.manager.cmdEndpoint(RTCMD_SESSION, joinPacket,
        joinPacket.callid,
        mSid,
        mCall.manager.ownAnonId,
        encKey,
        mCall.id()
    );
}

void Session::setState(State newState)
{
    auto oldState = mState;
    if (oldState == newState) {
        return false;
    }

    assertStateChange(
        oldState,
        newState,
        AllowedSessStateTransitions
    );

    mState = newState;

    SUB_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(mState));
    FIRE_EVENT(SESSION, onStateChange, mState);
}

webrtc::FakeConstraints& Session::pcConstraints()
{
    return mCall.manager.pcConstraints;
}

void Session::handleMsg(RtMessage& packet)
{
    switch (packet.type)
    {
        case RTCMD_SDP_OFFER:
            msgSdpOfferSendAnswer(packet);
            return;
        case RTCMD_SDP_ANSWER:
            msgSdpAnswer(packet);
            return;
        case RTCMD_ICE_CANDIDATE:
            msgIceCandidate(packet);
            return;
        case RTCMD_SESS_TERMINATE:
            msgSessTerminate(packet);
            return;
        case RTCMD_SESS_TERMINATE_ACK:
            msgSessTerminateAck(packet);
            return;
        case RTCMD_MUTE:
            msgMute(packet);
            return;
        default:
            SUB_LOG_WARNING("Don't know how to handle", packet.typeStr());
            return;
    }
}

void Session::createRtcConn()
{
    mRtcConn = artc::myPeerConnection<Call::Session>(
        mCall.manager.iceServers, *this, pcConstraints());
    if (mCall.mLocalStream)
    {
        if (!mRtcConn->AddStream(*mCall.mLocalStream))
            throw std::runtime_error("mRtcConn->AddStream() returned false");
    }
    mStatRecorder.reset(new Stats::Recorder(mRtcConn, 1, 5, *this));
    mStatRecorder->start();
}
//PeerConnection events
void Session::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    setState(kStateInProgress);
    if (mRemotePlayer)
    {
        SUB_LOG_ERROR("onRemoteStreamAdded: Session already has a remote player, ignoring event");
        return;
    }

    IVideoRenderer* renderer = NULL;
    FIRE_EVENT(SESSION, onRemoteStreamAdded, renderer);
    assert(renderer);
    mRemotePlayer.reset(new artc::StreamPlayer(renderer));
    mRemotePlayer->attachToStream(stream);
    mRemotePlayer->start();
}
void Session::onRemoveStream(artc::tspMediaStream stream)
{
    if (stream != mRemoteStream) //we can't throw here because we are in a callback
    {
        KR_LOG_ERROR("onRemoveStream: Stream is not the remote stream that we have");
        return;
    }
    if(mRemotePlayer)
    {
        mRemotePlayer->stop();
        mRemotePlayer.reset();
    }
    mRemoteStream.reset();
    FIRE_EVENT(SESSION, onRemoteStreamRemoved);
}

void Session::onIceCandidate(std::shared_ptr<artc::IceCandText> cand)
{
    // mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    if (!cand)
        return;
    Buffer data(10+cand->candidate.size());
    data.append<uint8_t>(cand->sdpMLineIndex);
    auto& mid = cand->sdpMid;
    if (!mid.empty())
    {
        data.append<uint8_t>(mid.size());
        data.append(mid);
    }
    else
    {
        data.append<uint8_t>(0);
    }
    data.append<uint16_t>(cand->candidate.size())
        .append(cand->candidate);
    cmd(RTCMD_ICE_CANDIDATE, data);
}

void Session::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    SUB_LOG_DEBUG("ICE connstate changed to %s", iceStateToStr(state));
    if (mState >= Session::kStateTerminating)
        return;
/*  kIceNew, kIceGathering, kIceWaiting, kIceChecking, kIceConnected,
    kIceCompleted, kIceFailed, kIceClosed
 */

    if (mState == PeerConnectionInterface::kIceClosed)
    {
        terminateAndDestroy(TermCode::kErrIceDisconn);
    }
    else if (mState == PeerConnectionInterface::kIceFailed)
    {
        self.terminateAndDestroy(TermCode::kErrIceFail);
    }
    else if (state == PeerConnectionInterface::kIceConnected)
    {
        mTsIceConn = time(NULL);
        mCall.notifySessionConnected(*this);
    }
}

void Session::onIceComplete()
{
    SUB_LOG_DEBUG("onIceComplete");
}
void Session::onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
{
    SUB_LOG_DEBUG("onSignalingStateChange: %d", newState);
}
//end of event handlers

// stats interface
//====
Session::sendAv(AvFlags av)
{
    cmd(RTCMD_MUTE, av.value());
}
Promise<void> Session::sendOffer()
{
    assert(mIsJoiner); // the joiner sends the SDP offer
    assert(mPeerAnonId);
    createRtcConn();
    auto wptr = weakHandle();
    mRtcConn.createOffer(pcConstraints())
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp)
    {
        if (wptr.deleted())
            return;
    /*  if (self.state !== SessState.kWaitSdpAnswer) {
            return;
        }
    */
        KR_THROW_IF_FALSE(sdp->ToString(&mOwnSdp));
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, this]()
    {
        if (wptr.deleted())
            return;
        Key<32> encKey;
        mCall.manager.crypto.encryptNonceTo(mPeer, mOwnSdpKey, encKey);
        Key<32> hash;
        mCall.manager.crypto.mac(mOwnSdp, mPeerSdpKey, hash);

        // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        cmd(RTCMD_SDP_OFFER,
            mCall.manager.ownAnonId,
            encKey,
            hash,
            mCall.mLocalStream.effectiveAv(),
            static_cast<uint16_t>(mOwnSdp.size()),
            mOwnSdp
        );
        assert(mState == Session::kStateWaitSdpAnswer);
    })
    .fail([wptr, this](const promise::Error& err)
    {
        if (!wptr.deleted())
            return;
        terminateAndDestroy(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
    });
}

void Session::msgSdpOfferSendAnswer(RtMessage& packet)
{
    // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
    if (mState != Session::kStateWaitSdpOffer)
    {
        SUB_LOG_WARNING("Ingoring unexpected SDP offer while in state %s", stateStr());
        return;
    }
    assert(!mIsJoiner);
    // The peer is likely to send ICE candidates immediately after the offer,
    // but we can't process them until setRemoteDescription is ready, so
    // we have to store them in a queue
    setState(Session::kStateWaitLocalSdpAnswer);
    mPeerAnonId = packet.payloadRead<uint64_t>(8);
    SdpKey encKey;
    packet.payloadRead(16, 32, encKey);
    mCall.manager.crypto.decryptKeyFrom(mPeer, encKey, mPeerSdpKey);
    mPeerAv = packet.payloadRead<uint8_t>(80);
    uint16_t sdpLen = data.payloadRead<uint16_t>(81);
    assert(packet.payloadSize() >= 83 + sdpLen);
    packet.payloadRead(83, sdpLen, mPeerSdpOffer);
    if (!verifySdpFingerprints(mPeerSdpOffer, packet.payloadReadPtr<SdpKey>(48)))
    {
        SUB_LOG_ERROR("Fingerprint verification error, immediately terminating session");
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forge attempt");
        return;
    }
    mungeSdp(mPeerSdpOffer);
    unique_ptr<webrtc::JsepSessionDescription> jsepSdp(new webrtc::JsepSessionDescription("offer"));
    webrtc::SdpParseError error;
    if (!jsepSdp->Initialize(mPeerSdpOffer, &error))
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP offer: line="+error.line+"\nError: "+error.description);
        return;
    }
    auto wptr = weakHandle();
    mRtcConn.setRemoteDescription(sdp)
    .fail([this](const promise::Error& err)
    {
        return Promise::Error(err.msg(), 1, ERRTYPE_RTC); //we signal 'remote' (i.e. protocol) error with errCode == 1
    })
    .then([this, wptr]() -> Promise<webrtc::SessionDescriptionInterface*>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return promise::Error("Session killed");
        return mRtcConn.createAnswer(pcConstraints());
    })
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> Promise<void>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return promise::Error("Session killed");

        sdp->toString(mOwnSdp);
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, this]()
    {
        SdpKey ownFprHash;
        // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
        mCall.manager.crypto.mac(mOwnSdp, mPeerSdpKey, ownFprHash);
        cmd(
            RTCMD_SDP_ANSWER,
            ownFprHash,
            mCall.mLocalStream.effectiveAv().value(),
            static_cast<uint16_t>(mOwnSdp.size()),
            mOwnSdp
        );
    })
    .fail([wptr, this](const promise::Error& err)
    {
        if (wptr.deleted())
            return;
        // cmd() doesn't throw, so we are here because of other error
        std::string msg;
        if (err.type() == ERRTYPE_RTC && err.code() == 1) {
            msg = "Error accepting remote SDP offer: " + err.msg();
        } else {
            msg = "Error creating SDP answer: " + err.msg();
        }
        terminateAndDestroy(TermCode::kErrSdp, msg);
    });
}

void Session::msgSdpAnswer(RtMessage& packet)
{
    if (mState != Session::kStateWaitSdpAnswer)
    {
        SUB_LOG_WARNING("Ingoring unexpected SDP_ANSWER");
        return;
    }
    // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
    mPeerAv.set(packet.payloadRead<uint8_t>(40));
    auto sdpLen = packet.payloadRead<uint16_t>(41);
    assert(packet.payloadSize() >= sdpLen + 43);
    packet.payloadRead(43, sdpLen, mPeerSdp);
    SdpKey encKey;
    packet.payloadRead(8, encKey);
    if (!verifySdpFingerprints(mPeerSdp, encKey))
    {
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forgery");
        return;
    }
    mungeSdp(mPeerSdp);
    unique_ptr<webrtc::JsepSessionDescription> sdp(new webrtc::JsepSessionDescription("answer"));
    webrtc::SdpParseError error;
    if (!jsepSdp->Initialize(mPeerSdp, &error))
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP answer: line="+error.line+"\nError: "+error.description);
        return;
    }

    mRtcConn.setRemoteDescription(sdp)
    .then([this, wptr](webrtc::SessionDescriptionInterface*)
    {
        if (mState > Session::kStateInProgress)
            return promise::Error("Session killed");
        setState(Session::kStateInProgress);
    })
    .fail([wptr, this](const promise::Error& err)
    {
        std::string msg = "Error setting SDP answer: " + err.msg();
        terminateAndDestroy(TermCode::kErrSdp, msg);
    });
}

template<class... Args>
bool Session::cmd(uint8_t type, Args... args)
{
    RtMessageComposer msg(OP_RTMSG_ENDPOINT, type, mCall.chatid, mPeer, mPeerClient);
    msg.payloadAppend(mSid, args...);
    if (!mCall.shard.sendCommand(msg))
    {
        if (mState < SessState.kTerminating)
        {
            destroy(TermCode::kErrNetSignalling);
        }
        return false;
    }
    return true;
}

Promise<void> Session::terminateAndDestroy(TermCode code, const std::string& msg)
{
    if (mState == Session::kStateTerminating)
    {
        if (!mTerminatePromise)
        {
            // state was set to Terminating, but promise was not created - this is done
            // only by waitAllSessionsTerminated(), which will eventually time out and destroy us
            SUB_LOG_WARNING("terminateAndDestroy: Already waiting for termination");
            return Promise<void>(); //this promise never resolves
        }
        else
        {
            return *mTerminatePromise;
        }
    }
    else if (mState == Session::kStateTerminated)
    {
        return promise::_Void();
    }

    if (!msg.empty())
    {
        SUB_LOG_ERROR("Terminating due to: %s", msg.c_str());
    }
    assert(!mTerminatePromise);
    setState(kStateTerminating);
    mTerminatePromise.reset(new Promise<void>());
    if (!cmd(RTCMD_SESS_TERMINATE, code))
    {
        destroy(code, msg);
        if (!mTerminatePromise->done())
        {
            mTerminatePromise.resolve();
        }
        return;
    }
    auto wptr = weakHandle();
    setTimeout([wptr, this, code]()
    {
        if (wptr.deleted() || mState != Session::kStateTerminating)
            return;
        destroy(code, msg);
        if (mTerminatePromise && !mTerminatePromise->done())
        {
            mTerminatePromise.resolve();
        }
    }, 1000);
    return *mTerminatePromise;
}

void Session::msgSessTerminateAck(RtMessage& packet)
{
    if (mState != kStateTerminating)
    {
        SUB_LOG_WARNING("Ignoring unexpected TERMINATE_ACK");
        return;
    }
    assert(mTerminatePromise);
    if (!mTerminatePromise->done())
        mTerminatePromise->resolve();
}

void Session::msgSessTerminate(RtMessage& packet)
{
    // sid.8 termcode.1
    assert(packet.payloadSize() >= 1);
    cmd(RTCMD_SESS_TERMINATE_ACK);

    if (mState == kStateTerminating && mTerminatePromise) {
        // handle terminate as if it were an ack - in both cases the peer is terminating
        msgSessTerminateAck(packet);
    }
    setState(kStateTerminating);
    destroy(packet.payloadRead<uint8_t>(8) | TermCode::kPeer);
}

/** Terminates a session without the signalling terminate handshake.
  * This should normally not be called directly, but via terminate(),
  * unless there is a network error
  */
void Session::destroy(TermCode code, std::string& msg)
{
    assert(code != TermCode::kInvalid);
    if (mState >= kStateDestroyed)
    {
        SUB_LOG_ERROR("Session::destroy(): Already destroyed");
        return;
    }
    if (!msg.empty()) {
        SUB_LOG_DEBUG("Destroying session due to:", msg.c_str());
    }

    submitStats(code, msg);

    if (mRtcConn)
    {
        if (mRtcConn->signaling_state() != PeerConnectionInterface::kClosed)
        {
            mRtcConn->close();
        }
        mRtcConn.reset();
    }
    setState(kStateDestroyed);
    FIRE_EVENT(SESS, onDestroy, code & (~TermCode::kPeer), !!(code & TermCode::kPeer), msg);
    mCall.removeSession(*this, code);
}

void Session::submitStats(TermCode termCode, const std::string& errInfo)
{
    StatSessInfo info(mSid, termCode, errInfo);
    if (mIsJoiner)
    { // isJoiner means answerer
        info.isCaller = false;
        info.caid = mPeerAnonId;
        info.aaid = mCall.manager.ownAnonId;
    }
    else
    {
        info.isCaller = true;
        info.caid = mCall.manager.ownAnonId;
        info.aaid = mPeerAnonId;
    }
    std::string stats = mStatRecorder->getStats(info);
    mCall.manager.client.api->sendChatStats(stats.c_str());
}

// we actually verify the whole SDP, not just the fingerprints
bool Session::verifySdpFingerprints(const std::string& sdp, const SdpKey& peerHash)
{
    SdpKey hash;
    mCall.manager.crypto.mac(sdp, mOwnSdpKey, hash);
    bool match = true; // constant time compare
    for (var i = 0; i < sizeof(SdpKey); i++) {
        match &= (hash[i] == peerHash[i]);
    }
    return match;
}

void Session::msgIceCandidate(RtMessage& packet)
{
    // sid.8 mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    auto mLineIdx = packet.payloadRead<uint8_t>(8);
    auto midLen = packet.payloadRead<uint8_t>(9);
    if (midLen > packet.payloadSize() - 11)
        throw new Error("Invalid ice candidate packet: midLen spans beyond data length");

    std::string mid;
    if (midLen)
        packet.payloadRead(10, midLen, mid);
    auto candLen = packet.payloadRead<uint16_t>(10 + midLen);
    assert(packet.payloadSize() >= 12 + midLen + candLen);
    std::string strCand;
    packet.payloadRead(midLen + 12, candLen, strCand);

    unique_ptr<webrtc::JsepIceCandidate> cand(new webrtc::JsepIceCandidate(mid, mLineIdx));
    webrtc::SdpParseError err;
    if (!cand->Initialize(strCand, &err))
        throw runtime_error("Error parsing ICE candidate:\nline: '"+err.line+"'\nError:" +err.description);

    //KR_LOG_COLOR(34, "cand: mid=%s, %s\n", mid.c_str(), line.c_str());

    if (!mPeerConn->AddIceCandidate(cand.release()))
    {
        terminateAndDestroy(TermCode::kErrProtocol);
    }
}

void Session::msgMute(RtMessage& packet)
{
    mPeerAv.set(packet.payloadRead<uint8_t>(8));
    FIRE_EVENT(SESS, onRemoteMute, mPeerAv);
}

void Session::mungeSdp(std::string& sdp)
{
    try
    {
        auto& maxbr = mCall.manager.maxbr;
        if (maxbr)
        {
            SUB_LOG_WARNING("mungeSdp: Limiting peer's send video send bitrate to %d kbps", maxbr);
            sdpSetVideoBw(sdp, maxbr);
        }
    }
    catch(std::exception& e)
    {
        SUB_LOG_ERROR("mungeSdp: Exception: %s", e.what());
        throw;
    }
}
const char* Call::stateToStr(uint8_t state)
{
    switch(state)
    {
        RET_ENUM_NAME(kStateInitial);
        RET_ENUM_NAME(kStateHasLocalStream);
        RET_ENUM_NAME(kStateReqSent);
        RET_ENUM_NAME(kStateRingIn);
        RET_ENUM_NAME(kStateJoining);
        RET_ENUM_NAME(kStateInProgress);
        RET_ENUM_NAME(kStateTerminating);
        RET_ENUM_NAME(kStateDestroyed);
        default: return "(invalid call state)";
    }
}

const char* Session::stateToStr(uint8_t state)
{
    switch(state)
    {
        RET_ENUM_NAME(kStateWaitSdpOffer);
        RET_ENUM_NAME(kStateWaitSdpAnswer);
        RET_ENUM_NAME(kStateWaitLocalSdpAnswer);
        RET_ENUM_NAME(kStateInProgress);
        RET_ENUM_NAME(kStateTerminating);
        RET_ENUM_NAME(kStateDestroyed);
        default: return "(invalid session state)";
    }
}

void assertStateChange(uint8_t oldState, uin8_t newState, const StateDesc& desc)
{
    if (oldState >= transMap.size())
        throw std::runtime_error(std::string("assertStateChange: Invalid old state ")+desc.toStrFunc(oldState));
    auto allowed = transMap[oldState];
    if (newState >= allowed.size())
        throw std::runtime_error(std::string("assertStateChange: Invalid new state ")+desc.toStrFunc(newState));
    for (auto a: allowed)
    {
        if (newState == a)
            return;
    }
    throw std::runtime_error(std::string("assertStateChange: Invalid state transition ")+desc.toStrFunc(oldState)+" -> "+desc.toStrFunc(newState));
}

StateDesc Call::sStateDesc = {
    .transMap = {
        { kStateReqSent, kStateTerminating },                //for kStateInitial
        { kStateJoining, kStateReqSent, kStateTerminating }, //for kStateHasLocalStream
        { kStateInProgress, kStateTerminating },             //for kStateReqSent
        { kStateInProgress, kStateTerminating },             //for kStateRingIn
        { kStateInProgress, kStateTerminating },             //for kStateJoining
        { kStateTerminating },                               //for kStateInProgress,
        { kStateDestroyed },                                 //for kStateTerminating,
        {}                                                   //for kStateDestroyed
    },
    .toStrFunc = Call::stateToStr
};

StateDesc Session::sStateDesc = {
    .transMap = {
        { kStateWaitLocalSdpAnswer, kStateTerminating }, //for kSWaitSdpOffer
        { kStateInProgress, kStateTerminating },         //for kStateWaitLocalSdpAnswer
        { kStateInPgoress, kTerminating },               //for kStateWaitSdpAnswer
        { kStateTerminating },                           //for kStateInProgress
        { kStateDestroyed },                             //for kStateTerminating
        {}                                               //for kStateDestroyed
    },
    .toStrFunc = Session::stateToStr
};

#define RET_ENUM_NAME(name) case name: return #name

const char* RtMessage::typeToString(uint8_t type)
{
    switch(type)
    {
        RET_ENUM_NAME(RTCMD_CALL_REQUEST);
        RET_ENUM_NAME(RTCMD_CALL_RINGING);
        RET_ENUM_NAME(RTCMD_CALL_REQ_DECLINE);
        RET_ENUM_NAME(RTCMD_CALL_REQ_CANCEL);
        RET_ENUM_NAME(RTCMD_CALL_TERMINATE); // hangup existing call, cancel call request. Works on an existing call
        RET_ENUM_NAME(RTCMD_JOIN); // join an existing/just initiated call. There is no call yet, so the command identifies a call request
        RET_ENUM_NAME(RTCMD_SESSION); // join was accepter and the receiver created a session to joiner
        RET_ENUM_NAME(RTCMD_SDP_OFFER); // joiner sends an SDP offer
        RET_ENUM_NAME(RTCMD_SDP_ANSWER); // joinee answers with SDP answer
        RET_ENUM_NAME(RTCMD_ICE_CANDIDATE); // both parties exchange ICE candidates
        RET_ENUM_NAME(RTCMD_SESS_TERMINATE); // initiate termination of a session
        RET_ENUM_NAME(RTCMD_SESS_TERMINATE_ACK); // acknowledge the receipt of SESS_TERMINATE, so the sender can safely stop the stream and
        // it will not be detected as an error by the receiver
        RET_ENUM_NAME(RTCMD_MUTE);
        default: return "(invalid RTCMD)";
    }
}
const char* termCodeToStr(uint8_t code)
{
    switch(code)
    {
        RET_ENUM_NAME(kUserHangup);
        RET_ENUM_NAME(kCallReqCancel);
        RET_ENUM_NAME(kCallRejected);
        RET_ENUM_NAME(kAnsElsewhere);
        RET_ENUM_NAME(kAnswerTimeout);
        RET_ENUM_NAME(kRingOutTimeout);
        RET_ENUM_NAME(kAppTerminating);
        RET_ENUM_NAME(kCallGone);
        RET_ENUM_NAME(kBusy);
        RET_ENUM_NAME(kNormalHangupLast);
        RET_ENUM_NAME(kErrApiTimeout);
        RET_ENUM_NAME(kErrFprVerifFailed);
        RET_ENUM_NAME(kErrProtoTimeout);
        RET_ENUM_NAME(kErrProtocol);
        RET_ENUM_NAME(kErrInternal);
        RET_ENUM_NAME(kErrLocalMedia);
        RET_ENUM_NAME(kErrNoMedia);
        RET_ENUM_NAME(kErrNetSignalling);
        RET_ENUM_NAME(kErrIceDisconn);
        RET_ENUM_NAME(kErrIceFail);
        RET_ENUM_NAME(kErrSdp);
        RET_ENUM_NAME(kErrUserOffline);
        RET_ENUM_NAME(kInvalid);
        default: return "(invalid term code)";
    }
}
