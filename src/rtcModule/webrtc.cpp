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
        CALL_LOG_WARN("Received " + msg.typeStr() +
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

    CALL_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(newState));
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
        CALL_LOG_WARNING("There were some error getting local stream: %s", errors.c_str());
    }
    setState(Call::kStateHasLocalStream);
}

void Call::msgCallTerminate(RtMessage& packet)
{
    if (packet.payload.dataSize() < 1) {
        CALL_LOG_ERROR("Ignoring CALL_TERMINATE without reason code");
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
        CALL_LOG_WARNING("Received CALL_TERMINATE from a client that is not in the call, ignoring");
        return;
    }
    destroy(code | TermCode::kPeer, false);
}
void Call::msgCallReqDecline(RtMessage& packet)
{
    // callid.8 termcode.1
    assert(packet.payload.dataSize() >= 9);
    auto code = packet.readPayload<uint8_t>(8);
    if (code == TermCode::kCallRejected) {
        handleReject(packet);
    } else if (code == TermCode::kBusy) {
        handleBusy(packet);
    } else {
        CALL_LOG_WARNING("Ingoring CALL_REQ_DECLINE with unexpected termnation code", termCodeToStr(code));
    }
}
void Call::msgCallReqCancel(RtMessage& packet)
{
    if (mState >= Call::kStateInProgress)
    {
        CALL_LOG_WARNING("Ignoring unexpected CALL_REQ_CANCEL while in state %s", stateToStr(mState));
        return;
    }
    assert(mFromUser);
    assert(mFromClient);
    // CALL_REQ_CANCEL callid.8 reason.1
    if (mFromUser != packet.fromUser() || mFromClient !== packet.fromClient())
    {
        CALL_LOG_WARNING("Ignoring CALL_REQ_CANCEL from a client that did not send the call request");
        return;
    }
    assert(packet.payloadLen() >= 9);
    auto callid = packet.payloadRead<uint64_t>(0);
    if (callid != mId) {
        CALL_LOG_WARNING("Ignoring CALL_REQ_CANCEL for an unknown request id");
        return;
    }
    auto term = packet.readPayload<uint8_t>(8);
    destroy(term | TermCode::kPeer, false);
}

void Call::handleReject(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        CALL_LOG_WARNING("Ingoring unexpected CALL_REJECT while in state %s", stateToStr(mState));
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
        CALL_LOG_WARNING("Ignoring unexpected RINGING");
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
        CALL_LOG_WARNING("Ignoring unexpected BUSY when in state %s", stateToStr(mState));
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
        CALL_LOG_WARNING("Ignoring unexpected SESSION");
        return;
    }
    setState(Call::kStateInProgress);
    auto sid = packet.readPayload<uint64_t>(8);
    if (mSessions.find(sid) != mSessions.end())
    {
        CALL_LOG_ERROR("Received SESSION with sid of an existing session (%s), ignoring", base64urlencode(&sid, sizeof(sid)));
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
        CALL_LOG_WANRING("Ingoring unexpected JOIN");
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
            CALL_LOG_ERROR("Timed out waiting for all sessions to terminate, force closing them");
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
        CALL_LOG_DEBUG("Destroying call due to: %s", msg.c_str());
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
        CALL_LOG_WARNING("broadcastCallReq: Call terminating/destroyed");
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
         && !sess.isJoiner)
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
        CALL_LOG_ERROR("Error(s) getting local stream: %s", errors.c_str());
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
        CALL_LOG_WARNING("answer: Not in kRingIn state, nothing to answer");
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
        CALL_LOG_DEBUG("hangup: Call already terminating/terminated");
        return;
    default:
        term = TermCode::kUserHangup;
        CALL_LOG_WARNING("Don't know what term code to send in state %s", stateString(mState));
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
    call.manager.crypto.random(&mOwnHashKey);
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

    SESS_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(mState));
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
            SESS_LOG_WARNING("Don't know how to handle", packet.typeStr());
            return;
    }
}

void Session::createRtcConn()
{
    var conn = self.rtcConn = new RTCPeerConnection({
        iceServers: RTC.fixupIceServers(self.call.manager.iceServers)
    });
    if (self.call.manager.gLocalStream) {
        conn.addStream(self.call.manager.gLocalStream);
    }
    conn.onicecandidate = function(event) {
        // mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
        var cand = event.candidate;
        if (!cand) {
            return;
        }
        var idx = cand.sdpMLineIndex;
        var mid = cand.sdpMid;
        var data = String.fromCharCode(idx);
        if (mid) {
            data += String.fromCharCode(mid.length);
            data += mid;
        } else {
            data += '\0';
        }
        data += Chatd.pack16le(cand.candidate.length);
        data += cand.candidate;
        self.cmd(RTCMD.ICE_CANDIDATE, data);
    };
    conn.onaddstream = function(event) {
        self.remoteStream = event.stream;
        self._setState(SessState.kInProgress);
        self._remoteStream = self.remoteStream;
        self._fire("onRemoteStreamAdded", self.remoteStream);
        var player = self.mediaWaitPlayer = document.createElement('video');
        RTC.attachMediaStream(player, self.remoteStream);
        // self.waitForRemoteMedia();
    };
    conn.onremovestream = function(event) {
        self.self.remoteStream = null;
        // TODO: Do we need to generate event from this?
    };
    conn.onsignalingstatechange = function(event) {
        var state = conn.signalingState;
        self.logger.log('Signaling state change to', state);
    };
    conn.oniceconnectionstatechange = function (event) {
        var state = conn.iceConnectionState;
        self.logger.debug('ICE connstate changed to', state);
        if (self.state >= SessState.kTerminating) { // use double equals because state might be some internal enum that
            return;                                 // converts to string?
        }
        if (state === 'disconnected') { // double equal is on purpose, state may not be exactly a string
            self.terminateAndDestroy(Term.kErrIceDisconn);
        } else if (state === 'failed') {
            self.terminateAndDestroy(Term.kErrIceFail);
        } else if (state === 'connected') {
            self._tsIceConn = Date.now();
            self.call._notifySessionConnected(self);
        }
    };
    // RTC.Stats will be set to 'false' if stats are not available for this browser
    if ((typeof RTC.Stats === 'undefined') && (typeof statsGlobalInit === 'function')) {
        statsGlobalInit(conn);
    }
    if (RTC.Stats) {
        self.statRecorder = new RTC.Stats.Recorder(conn, 1, 5, self);
        self.statRecorder.start();
    }
};

// stats interface
Session.prototype.onCommonInfo = function(info) {
}
Session.prototype.onSample = function(sample) {
}
//====
Session.prototype._sendAv = function(av) {
    this.cmd(RTCMD.MUTE, String.fromCharCode(av));
};
/*
Session.prototype.waitForRemoteMedia = function() {
    if (this.state !== SessState.kInProgress) {
        return;
    }
    var self = this;
    // Under Firefox < 4x, currentTime seems to stay at 0 forever,
    // despite that there is playback
    if ((self.mediaWaitPlayer.currentTime > 0) || (RTC.browser === "firefox")) {
//      self.mediaWaitPlayer = undefined;
        self.logger.log('Actual media received');
        // Use this to debug remote media players:
        if (false) {
            $('.messages.content-area:visible').append(self.mediaWaitPlayer);
        }
        self.mediaWaitPlayer.play();
    } else {
        setTimeout(function () { self.waitForRemoteMedia(); }, 200);
    }
};
*/
Session.prototype.sendOffer = function() {
    var self = this;
    assert(self.isJoiner); // the joiner sends the SDP offer
    assert(self.peerAnonId);
    self._createRtcConn();
    self.rtcConn.createOffer(self.pcConstraints())
    .then(function (sdp) {
    /*  if (self.state !== SessState.kWaitSdpAnswer) {
            return;
        }
    */

        self.ownSdpOffer = sdp.sdp;
        return self.rtcConn.setLocalDescription(sdp);
    })
    .then(function() {
        // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        self.cmd(RTCMD.SDP_OFFER,
            self.call.manager.ownAnonId +
            self.crypto.encryptNonceTo(self.peer, self.ownHashKey) +
            self.crypto.mac(self.ownSdpOffer, self.peerHashKey) +
            String.fromCharCode(Av.fromStream(self.call.manager.gLocalStream)) +
            Chatd.pack16le(self.ownSdpOffer.length) +
            self.ownSdpOffer
        );
        assert(self.state === SessState.kWaitSdpAnswer);
    })
    .catch(function(err) {
        if (err.stack) {
            err = err.stack;
        } else if (err.err) {
            err = err.err;
        }
        self.terminateAndDestroy(Term.kErrSdp, "Error creating SDP offer: " + err);
    });
};

Session.prototype.msgSdpOfferSendAnswer = function(packet) {
    // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
    var self = this;
    if (self.state !== SessState.kWaitSdpOffer) {
        self.logger.warn("Ingoring unexpected SDP offer");
        return;
    }
    assert(!self.isJoiner);
    // The peer is likely to send ICE candidates immediately after the offer,
    // but we can't process them until setRemoteDescription is ready, so
    // we have to store them in a queue
    self._setState(SessState.kWaitLocalSdpAnswer);
    var data = packet.data;
    self.peerAnonId = data.substr(8, 8);
    self.peerHashKey = self.crypto.decryptNonceFrom(self.peer, data.substr(16, 32));
    self.peerAv = data.charCodeAt(80);
    var sdpLen = Chatd.unpack16le(data.substr(81, 2));
    assert(data.length >= 83 + sdpLen);
    self.peerSdpOffer = data.substr(83, sdpLen);
    if (!self.verifySdpFingerprints(self.peerSdpOffer, data.substr(48, 32))) {
        self.logger.warn("Fingerprint verification error, immediately terminating session");
        self.terminateAndDestroy(Term.kErrFprVerifFailed, "Fingerprint verification failed, possible forge attempt");
        return;
    }
    var sdp = new RTCSessionDescription({type: 'offer', sdp: self.peerSdpOffer});
    self._mungeSdp(sdp);
    self.rtcConn.setRemoteDescription(sdp)
    .catch(function(err) {
        return Promise.reject({err: err, remote: true});
    })
    .then(function() {
        if (self.state > SessState.kInProgress) {
            return Promise.reject({err: "Session killed"});
        }
        return self.rtcConn.createAnswer(self.pcConstraints());
    })
    .then(function(sdp) {
        if (self.state > SessState.kInProgress) {
            return Promise.reject({err: "Session killed"});
        }
        self.ownSdpAnswer = sdp.sdp;
        return self.rtcConn.setLocalDescription(sdp);
    })
    .then(function() {
        // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
        self.ownFprHash = self.crypto.mac(self.ownSdpAnswer, self.peerHashKey);
        var success = self.cmd(
            RTCMD.SDP_ANSWER,
            self.ownFprHash +
            String.fromCharCode(Av.fromStream(self.call.manager.gLocalStream)) +
            Chatd.pack16le(self.ownSdpAnswer.length) +
            self.ownSdpAnswer
        );
        if (success) {
            self.logger.log("Successfully generated and sent SDP answer");
        }
    })
    .catch(function(err) {
        // self.cmd() doesn't throw, so we are here because of other error
        var msg;
        if (err.remote) {
            msg = "Error accepting remote SDP offer: " + err.err;
        } else {
            if (err.stack) {
                err = err.stack;
            } else if (err.err) {
                err = err.err;
            }
            msg = "Error creating SDP answer: " + err;
        }
        self.terminateAndDestroy(Term.kErrSdp, msg);
    });
};

Session.prototype.msgSdpAnswer = function(packet) {
    var self = this;
    if (self.state !== SessState.kWaitSdpAnswer) {
        self.logger.warn("Ingoring unexpected SDP_ANSWER");
        return;
    }
    // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
    var data = packet.data;
    this.peerAv = data.substr(40, 1).charCodeAt(0);
    var sdpLen = Chatd.unpack16le(data.substr(41, 2));
    assert(data.length >= sdpLen + 43);
    self.peerSdpAnswer = data.substr(43, sdpLen);

    if (!self.verifySdpFingerprints(self.peerSdpAnswer, data.substr(8, 32))) {
        self.terminateAndDestroy(Term.kErrFprVerifFailed, "Fingerprint verification failed, possible forgery");
        return;
    }

    var sdp = new RTCSessionDescription({type: 'answer', sdp: self.peerSdpAnswer});
    self._mungeSdp(sdp);
    self.rtcConn.setRemoteDescription(sdp)
    .then(function() {
        if (self.state > SessState.kInProgress) {
            return Promise.reject("Session killed");
        }
        self._setState(SessState.kInProgress);
    })
    .catch(function(err) {
        var msg = "Error setting SDP answer: " + err;
        self.terminateAndDestroy(Term.kErrSdp, msg);
    });
};

Session.prototype.cmd = function(op, data) {
    var self = this;
    var payload = String.fromCharCode(op) + self.sid;
    if (data) {
        payload += data;
    }
    if (!self.call.shard.cmd(Chatd.Opcode.RTMSG_ENDPOINT,
        self.call.chatid + self.peer + self.peerClient +
        Chatd.pack16le(payload.length) + payload)) {
        if (self.state < SessState.kTerminating) {
            this._destroy(Term.kErrNetSignalling);
        }
        return false;
    }
    return true;
};

Session.prototype.terminateAndDestroy = function(code, msg) {
    var self = this;
    if (self.state === SessState.kTerminating) {
        if (!self.terminatePromise) { //we are waiting for session terminate by peer
            self.logger.warn("terminateAndDestroy: Already waiting for termination");
            return new Promise(function(){}); //this promise never resolves, the actual termination is done by waitAllSessionsTerminated
        } else {
            return self.terminatePromise;
        }
    } else if (self.state === SessState.kTerminated) {
        return Promise.resolve();
    }

    if (msg) {
        self.logger.warn("Terminating due to:", msg);
    }
    self._setState(SessState.kTerminating);
    self.terminatePromise = new Promise(function(resolve, reject) {
        setTimeout(function() { //execute function async so that we have the promise object before that
            // destroy() sets state to kDestroyed
            if (!self.cmd(RTCMD.SESS_TERMINATE, String.fromCharCode(code))) {
                self._destroy(code, msg);
                resolve(null);
            } else {
                var done = function(packet) {
                    if (self.state !== SessState.kTerminating) {
                        return;
                    }
                    self._destroy(code, msg);
                    resolve(packet);
                };
                self.terminateAckCallback = done;
                setTimeout(function() {
                    done(null);
                }, 1000);
            }
        });
    }, 0);
    return self.terminatePromise;
};

Session.prototype.msgSessTerminateAck = function(packet) {
    if (this.state !== SessState.kTerminating) {
        this.logger.warn("Ignoring unexpected TERMINATE_ACK");
    }
    assert(this.terminateAckCallback);
    var cb = this.terminateAckCallback;
    delete this.terminateAckCallback;
    cb(packet);
};

Session.prototype.msgSessTerminate = function(packet) {
    // sid.8 termcode.1
    var self = this;
    assert(packet.data.length >= 1);
    self.cmd(RTCMD.SESS_TERMINATE_ACK);

    if (self.state === SessState.kTerminating && this.terminateAckCallback) {
        // handle terminate as if it were an ack - in both cases the peer is terminating
        self.msgSessTerminateAck(packet);
    }
    self._setState(SessState.kTerminating);
    self._destroy(packet.data.charCodeAt(8) | Term.kPeer);
};

/** Terminates a session without the signalling terminate handshake.
  * This should normally not be called directly, but via terminate(),
  * unless there is a network error
  */
Session.prototype._destroy = function(code, msg) {
    assert(typeof code !== 'undefined');
    if (this.state >= SessState.kDestroyed) {
        this.logger.error("Session.destroy(): Already destroyed");
        return;
    }
    if (msg) {
        this.logger.log("Destroying session due to:", msg);
    }

    this.submitStats(code, msg);

    if (this.rtcConn) {
        if (this.rtcConn.signallingState !== 'closed') {
            this.rtcConn.close();
        }
        this.rtcConn = null;
    }
    this._setState(SessState.kDestroyed);
    this._fire("onDestroy", code & 0x7f, !!(code & 0x80), msg);// jscs:ignore disallowImplicitTypeConversion
    this.call._removeSession(this, code);
};
Session.prototype.submitStats = function(termCode, errInfo) {
    var stats;
    if (this.statRecorder) {
        stats = this.statRecorder.getStats(base64urlencode(this.sid));
        delete this.statRecorder;
    } else { //no stats, but will still provide callId and duration
        stats = {
            cid: base64urlencode(this.sid),
            bws: RTC.getBrowserVersion()
        };

        if (this._tsIceConn) {
            stats.ts = Math.round(this._tsIceConn/1000);
            stats.dur = Math.ceil((Date.now()-this._tsIceConn)/1000);
        } else {
            stats.ts = Date.now();
            stats.dur = 0;
        }
    }
    if (this.isJoiner) { // isJoiner means answerer
        stats.isCaller = 0;
        stats.caid = base64urlencode(this.peerAnonId);
        stats.aaid = base64urlencode(this.call.manager.ownAnonId);
    } else {
        stats.isCaller = 1;
        stats.caid = base64urlencode(this.call.manager.ownAnonId);
        stats.aaid = base64urlencode(this.peerAnonId);
    }

    if (termCode & Term.kPeer) {
        stats.termRsn = 'peer-'+constStateToText(Term, termCode & ~Term.kPeer);
    } else {
        stats.termRsn = constStateToText(Term, termCode);
    }
    if (errInfo) {
        stats.errInfo = errInfo;
    }
    var url = this.call.manager.statsUrl;
    if (url) {
        jQuery.ajax(url, {
                        type: 'POST',
                        data: JSON.stringify(stats)
                    });
    }
}

// we actually verify the whole SDP, not just the fingerprints
Session.prototype.verifySdpFingerprints = function(sdp, peerHash) {
    var hash = this.crypto.mac(sdp, this.ownHashKey);
    var len = hash.length;
    var match = true; // constant time compare
    for (var i = 0; i < len; i++) {
        match &= hash[i] === peerHash[i];
    }
    return match;
};

Session.prototype.msgIceCandidate = function(packet) {
    // sid.8 mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    var self = this;
    var data = packet.data;
    var mLineIdx = data.charCodeAt(8);
    var midLen = data.charCodeAt(9);
    if (midLen > data.length - 11) {
        throw new Error("Invalid ice candidate packet: midLen spans beyond data length");
    }
    var mid = midLen ? data.substr(10, midLen) : undefined;
    var candLen = Chatd.unpack16le(data.substr(10 + midLen, 2));
    assert(data.length >= 12 + midLen + candLen);
    var cand = new RTCIceCandidate({
        sdpMLineIndex: mLineIdx,
        sdpMid: mid,
        candidate: data.substr(midLen + 12, candLen)
    });
    return self.rtcConn.addIceCandidate(cand)
        .catch(function(err) {
            self.terminateAndDestroy(Term.kErrProtocol, err);
            return err;
        });
};

Session.prototype.msgMute = function(packet) {
    var av = packet.data.charCodeAt(8);
    this.peerAv = av;
    this._fire('onRemoteMute', av);
};

Session.prototype._fire = function(evName) {
    var func = this.handler[evName];
    var logger = this.logger;
    if (logger.isEnabled()) {
        var msg = "fire sess event " + evName;
        if (evName === 'onDestroy') {
            msg += '(' + constStateToText(Term, arguments[1]) + ')';
        }
        if (!func) {
            msg += " ...unhandled";
        }
        logger.log(msg);
    }
    if (!func) {
        return;
    }
    try {
        // Don't mess with slicing the arguments array - this will be slower than
        // manually indexing arguments 1, 2 and 3. Currently we don't have an event with
        // more than 3 arguments. If such appears, we need to update this method
        func.call(this.handler, arguments[1], arguments[2], arguments[3]);
    } catch (e) {
        logger.error("Event handler '" + evName + "' threw exception:\n" + e, "\n", e.stack);
    }
};
Session.prototype._mungeSdp = function(sdp) {
    try {
        var maxbr = localStorage.webrtcMaxBr;
        if (maxbr) {
            maxbr = parseInt(maxbr);
            assert(!isNaN(maxbr));
            this.logger.warn("mungeSdp: Limiting peer's send video send bitrate to", maxbr, "kbps");
            RTC.sdpSetVideoBw(sdp, maxbr);
        }
    } catch(e) {
        this.logger.error("mungeSdp: Exception:", e.stack);
        throw e;
    }
}

var Av = { Audio: 1, Video: 2 };
Av.fromStream = function(stream) {
    if (!stream) {
        return 0;
    }
    var av = 0;
    var at = stream.getAudioTracks();
    var i;
    for (i = 0; i < at.length; i++) {
        if (at[i].enabled) {
            av |= Av.Audio;
        }
    }
    var vt = stream.getVideoTracks();
    for (i = 0; i < vt.length; i++) {
        if (vt[i].enabled) {
            av |= Av.Video;
        }
    }
    return av;
};
Av.fromMediaOptions = function(opts) {
    var result = opts.audio ? Av.Audio : 0;
    if (opts.video) {
        result |= Av.Video;
    }
    return result;
};
Av.applyToStream = function(stream, av) {
    if (!stream) {
        return;
    }
    var result = 0;
    var at = stream.getAudioTracks();
    var i;
    for (i = 0; i < at.length; i++) {
        if (av & Av.Audio) {
            at[i].enabled = true;
            result |= Av.Audio;
        } else {
            at[i].enabled = false;
        }
    }
    var vt = stream.getVideoTracks();
    for (i = 0; i < vt.length; i++) {
        if (av & Av.Video) {
            vt[i].enabled = true;
            result |= Av.Video;
        } else {
            vt[i].enabled = false;
        }
    }
    return result;
};
var RTCMD = Object.freeze({
    CALL_REQUEST: 0, // initiate new call, receivers start ringing
    CALL_RINGING: 1, // notifies caller that there is a receiver and it is ringing
    CALL_REQ_DECLINE: 2, // decline incoming call request, with specified Term code
    // (can be only kBusy and kCallRejected)
    CALL_REQ_CANCEL: 3,  // caller cancels the call requests, specifies the request id
    CALL_TERMINATE: 4, // hangup existing call, cancel call request. Works on an existing call
    JOIN: 5, // join an existing/just initiated call. There is no call yet, so the command identifies a call request
    SESSION: 6, // join was accepter and the receiver created a session to joiner
    SDP_OFFER: 7, // joiner sends an SDP offer
    SDP_ANSWER: 8, // joinee answers with SDP answer
    ICE_CANDIDATE: 9, // both parties exchange ICE candidates
    SESS_TERMINATE: 10, // initiate termination of a session
    SESS_TERMINATE_ACK: 11, // acknowledge the receipt of SESS_TERMINATE, so the sender can safely stop the stream and
    // it will not be detected as an error by the receiver
    MUTE: 12
});

var Term = Object.freeze({
    kUserHangup: 0,         // < Normal user hangup
    kCallReqCancel: 1,      // < Call request was canceled before call was answered
    kCallRejected: 2,       // < Outgoing call has been rejected by the peer OR incoming call has been rejected by
    // <another client of our user
    kAnsElsewhere: 3,       // < Call was answered on another device of ours
    kAnswerTimeout: 5,      // < Call was not answered in a timely manner
    kRingOutTimeout: 6,     // < We have sent a call request but no RINGING received within this timeout - no other
    // < users are online
    kAppTerminating: 7,     // < The application is terminating
    kCallGone: 8,
    kBusy: 9,               // < Peer is in another call
    kNormalHangupLast: 20,  // < Last enum specifying a normal call termination
    kErrorFirst: 21,        // < First enum specifying call termination due to error
    kErrApiTimeout: 22,     // < Mega API timed out on some request (usually for RSA keys)
    kErrFprVerifFailed: 23, // < Peer DTLS-SRTP fingerprint verification failed, posible MiTM attack
    kErrProtoTimeout: 24,   // < Protocol timeout - one if the peers did not send something that was expected,
    // < in a timely manner
    kErrProtocol: 25,       // < General protocol error
    kErrInternal: 26,       // < Internal error in the client
    kErrLocalMedia: 27,     // < Error getting media from mic/camera
    kErrNoMedia: 28,        // < There is no media to be exchanged - both sides don't have audio/video to send
    kErrNetSignalling: 29,  // < chatd shard was disconnected
    kErrIceDisconn: 30,     // < ice-disconnect condition on webrtc connection
    kErrIceFail: 31,        // <ice-fail condition on webrtc connection
    kErrSdp: 32,            // < error generating or setting SDP description
    kErrUserOffline: 33,    // < we received a notification that that user went offline
    kErrorLast: 33,         // < Last enum indicating call termination due to error
    kLast: 33,              // < Last call terminate enum value
    kPeer: 128              // < If this flag is set, the condition specified by the code happened at the peer,
    // < not at our side
});

function isTermError(code) {
    return (code & 0x7f) >= Term.kErrorFirst;
}

var CallState = Object.freeze({
    kInitial: 0, // < Call object was initialised
    kWaitLocalStream: 1,
    kHasLocalStream: 2,
    kReqSent: 3, // < Call request sent
    kRingIn: 4, // < Call request received, ringing
    kJoining: 5, // < Joining a call
    kInProgress: 6,
    kTerminating: 7, // < Call is waiting for sessions to terminate
    kDestroyed: 8 // < Call object is not valid anymore
});

var CallStateAllowedStateTransitions = {};
CallStateAllowedStateTransitions[CallState.kInitial] = [
    CallState.kWaitLocalStream,
    CallState.kReqSent
];
CallStateAllowedStateTransitions[CallState.kWaitLocalStream] = [
    CallState.kHasLocalStream,
    CallState.kTerminating
];
CallStateAllowedStateTransitions[CallState.kHasLocalStream] = [
    CallState.kJoining,
    CallState.kReqSent,
    CallState.kTerminating
];
CallStateAllowedStateTransitions[CallState.kReqSent] = [
    CallState.kInProgress,
    CallState.kTerminating
];

CallStateAllowedStateTransitions[CallState.kRingIn] = [
    CallState.kWaitLocalStream,
    CallState.kInProgress,
    CallState.kTerminating
];

CallStateAllowedStateTransitions[CallState.kJoining] = [
    CallState.kInProgress,
    CallState.kTerminating
];

CallStateAllowedStateTransitions[CallState.kInProgress] = [
    CallState.kTerminating
];

CallStateAllowedStateTransitions[CallState.kTerminating] = [
    CallState.kDestroyed
];

CallStateAllowedStateTransitions[CallState.kDestroyed] = [];

var SessState = Object.freeze({
    kWaitSdpOffer: 1, // < Session just created, waiting for SDP offer from initiator
    kWaitSdpAnswer: 2, // < SDP offer has been sent by initiator, waniting for SDP answer
    kWaitLocalSdpAnswer: 3, // < Remote SDP offer has been set, and we are generating SDP answer
    //    kWaitRemoteMedia: 4, // < We have completed the SDP handshake and waiting for actual media frames from the
    // remote
    kInProgress: 5,
    kTerminating: 6, // < Session is in terminate handshake
    kDestroyed: 7 // < Session object is not valid anymore
});


var SessStateAllowedStateTransitions = {};
SessStateAllowedStateTransitions[SessState.kWaitSdpOffer] = [
    SessState.kWaitLocalSdpAnswer,
    SessState.kTerminating
];
SessStateAllowedStateTransitions[SessState.kWaitLocalSdpAnswer] = [
    SessState.kInProgress,
    SessState.kTerminating
];
SessStateAllowedStateTransitions[SessState.kWaitSdpAnswer] = [
    SessState.kInProgress,
    SessState.kTerminating
];
SessStateAllowedStateTransitions[SessState.kInProgress] = [
    SessState.kTerminating
];
SessStateAllowedStateTransitions[SessState.kTerminating] = [
    SessState.kDestroyed
];
SessStateAllowedStateTransitions[SessState.kDestroyed] = [];

scope.RtcModule = RtcModule;
scope.Call = Call;
scope.Session = Session;
scope.Av = Av;
scope.Term = Term;
scope.RTCMD = RTCMD;
scope.SessState = SessState;
scope.CallState = CallState;
}(window));
