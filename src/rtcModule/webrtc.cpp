#ifndef KARERE_DISABLE_WEBRTC
#include <mega/types.h>
#include <mega/base64.h>
#include <rtcmPrivate.h>
#include <webrtcPrivate.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>

namespace rtcModule
{
SvcDriver::SvcDriver ()
    : mCurrentSvcLayerIndex(kMaxQualityIndex), // by default max quality
      mPacketLostLower(14),
      mPacketLostUpper(20),
      mPacketLostCapping(10),
      mLowestRttSeen(10000),
      mRttLower(0),
      mRttUpper(0),
      mMovingAverageRtt(0),
      mMovingAveragePlost(0),
      mMovingAverageVideoTxHeight(-1),
      mTsLastSwitch(0)
{

}

bool SvcDriver::setSvcLayer(int8_t delta, int8_t& rxSpt, int8_t& rxTmp, int8_t& rxStmp, int8_t& txSpt)
{
    int8_t newSvcLayerIndex = static_cast<int8_t>(mCurrentSvcLayerIndex + delta);
    if (newSvcLayerIndex < 0 || newSvcLayerIndex > kMaxQualityIndex)
    {
        return false;
    }

    RTCM_LOG_WARNING("setSvcLayer: Switching SVC layer from %u to %d", mCurrentSvcLayerIndex, newSvcLayerIndex);
    mTsLastSwitch = time(nullptr); // update last Ts SVC switch
    mCurrentSvcLayerIndex = static_cast<uint8_t>(newSvcLayerIndex);

    // we want to provide a linear quality scale,
    // layers are defined for each of the 7 "quality" steps
    // layer: rxSpatial (resolution), rxTemporal (FPS), rxScreenTemporal (for screen video), txSpatial (resolution)
    switch (mCurrentSvcLayerIndex)
    {
        case 0: { rxSpt = 0; rxTmp = 0; rxStmp = 0; txSpt = 0; return true; }
        case 1: { rxSpt = 0; rxTmp = 1; rxStmp = 0; txSpt = 0; return true; }
        case 2: { rxSpt = 0; rxTmp = 2; rxStmp = 0; txSpt = 1; return true; }
        case 3: { rxSpt = 1; rxTmp = 1; rxStmp = 0; txSpt = 1; return true; }
        case 4: { rxSpt = 1; rxTmp = 2; rxStmp = 1; txSpt = 1; return true; }
        case 5: { rxSpt = 2; rxTmp = 1; rxStmp = 1; txSpt = 2; return true; }
        case 6: { rxSpt = 2; rxTmp = 2; rxStmp = 2; txSpt = 2; return true; }
        default: return false;
    }
}

Call::Call(const karere::Id& callid, const karere::Id& chatid, const karere::Id& callerid, bool isRinging, CallHandler& callHandler, MyMegaApi& megaApi, RtcModuleSfu& rtc, bool isGroup, std::shared_ptr<std::string> callKey, karere::AvFlags avflags, bool caller)
    : mCallid(callid)
    , mChatid(chatid)
    , mCallerId(callerid)
    , mIsRinging(isRinging)
    , mIsOutgoingRinging (caller && !isGroup) // If I have started a 1on1 call outgoing ringing is true
    , mIsOwnClientCaller(caller)
    , mIsGroup(isGroup)
    , mCallHandler(callHandler) // CallHandler to receive notifications about the call
    , mMegaApi(megaApi)
    , mSfuClient(rtc.getSfuClient())
    , mCallKey(callKey ? *callKey : std::string())
    , mIsJoining(false)
    , mRtc(rtc)
{
    mMyPeer.reset(new sfu::Peer(karere::Id(mMegaApi.sdk.getMyUserHandleBinary()), sfu::SfuProtocol::SFU_PROTO_INVAL, avflags.value()));
    setState(kStateInitial); // call after onNewCall, otherwise callhandler didn't exists
}

Call::~Call()
{
    disableStats();

    if (mTermCode == kInvalidTermCode)
    {
        mTermCode = kUnKnownTermCode;
    }

    setState(CallState::kStateDestroyed);
}

karere::Id Call::getCallid() const
{
    return mCallid;
}

karere::Id Call::getChatid() const
{
    return mChatid;
}

karere::Id Call::getCallerid() const
{
    return mCallerId;
}

void Call::setState(CallState newState)
{
    if (newState == mState)
    {
        return;
    }

    RTCM_LOG_DEBUG("Call state changed. ChatId: %s, callid: %s, state: %s --> %s",
                 mChatid.toString().c_str(),
                 mCallid.toString().c_str(),
                 Call::stateToStr(mState),
                 Call::stateToStr(newState));

    if (mConnectTimer && (newState == CallState::kInWaitingRoom
                          || newState >= CallState::kStateTerminatingUserParticipation))
    {
        karere::cancelTimeout(mConnectTimer, mRtc.getAppCtx());
        mConnectTimer = 0;
    }

    if (newState == CallState::kStateConnecting && !mConnectTimer) // if are we trying to reconnect, and no previous timer was set
    {
        auto wptr = weakHandle();
        mConnectTimer = karere::setTimeout([this, wptr]()
        {
            if (wptr.deleted())
                return;

            assert(mState <= CallState::kInWaitingRoom || !mConnectTimer); // if call state >= kStateInProgress mConnectTimer must be 0
            if (mState < CallState::kInWaitingRoom)
            {
                mConnectTimer = 0;
                SFU_LOG_DEBUG("Reconnection attempt has not succeed after %d seconds. Automatically hang up call", kConnectingTimeout);
                mIsReconnectingToChatd
                    ? mRtc.orderedDisconnectAndCallRemove(this, rtcModule::EndCallReason::kFailed, kUserHangup) // no need to marshall, as we are executing a lambda in a timer
                    : orderedCallDisconnect(kUserHangup, "Reconnection attempt has not succeed"); // TODO add new termcode to notify apps that reconnection attempt failed
            }
        }, kConnectingTimeout * 1000, mRtc.getAppCtx());
    }

    if (newState == CallState::kStateInProgress)
    {
        if (mConnectTimer) // cancel timer, as we have joined call before mConnectTimer expired
        {
            karere::cancelTimeout(mConnectTimer, mRtc.getAppCtx());
            mConnectTimer = 0;
        }

        // initial ts is set when user has joined to the call
        mInitialTs = time(nullptr);
    }
    else if (newState == CallState::kStateDestroyed)
    {
        mFinalTs = time(nullptr);
    }

    mState = newState;
    mCallHandler.onCallStateChange(*this);
}

CallState Call::getState() const
{
    return mState;
}

bool Call::isOwnClientCaller() const
{
    return mIsOwnClientCaller;
}

bool Call::isJoined() const
{
    return mSfuConnection && mSfuConnection->isJoined();
}

bool Call::isOwnPrivModerator() const
{
   return mMyPeer->isModerator();
}

void Call::addParticipant(const karere::Id &peer)
{
    mParticipants.insert(peer);
    mCallHandler.onAddPeer(*this, peer);
    if (peer != mMyPeer->getPeerid()    // check that added peer is not own peerid
            && !mIsGroup
            && mIsOwnClientCaller
            && mIsOutgoingRinging)
    {
        // notify that 1on1 call has stopped ringing, in order stop outgoing ringing sound, if we started the call and a peer have joined
        stopOutgoingRinging();
    }
}

void Call::joinedCallUpdateParticipants(const std::set<karere::Id> &usersJoined)
{
    if (usersJoined.find(mMyPeer->getPeerid()) != usersJoined.end())
    {
        setRinging(false);
    }

    if (!mIsReconnectingToChatd)
    {
        for (const karere::Id &peer : usersJoined)
        {
            // if we haven't experimented a chatd connection lost (mIsConnectedToChatd == true) just add received peers
            addParticipant(peer);
        }
    }
    else
    {
        for (const karere::Id &recvPeer : usersJoined)
        {
            if (mParticipants.find(recvPeer) == mParticipants.end())
            {
                // add new participant received at OP_JOINEDCALL
                addParticipant(recvPeer);
            }
        }

        for (auto it = mParticipants.begin(); it != mParticipants.end();)
        {
            auto auxit = it++;
            karere::Id peer = *auxit;
            if (usersJoined.find(peer) == usersJoined.end())
            {
                // remove participant from mParticipants, not present at list received at OP_JOINEDCALL
                mParticipants.erase(auxit);
                mCallHandler.onRemovePeer(*this, peer);
            }
        }

        mIsReconnectingToChatd = false; // we can assume that we are connected to chatd, and our participants list is up to date
    }
}

void Call::onDisconnectFromChatd()
{
    if (!participate())
    {
        // if we don't participate in a meeting, and we are disconnected from chatd, we need to clear participants
        clearParticipants();
    }

    mIsReconnectingToChatd = true;
}

void Call::reconnectToSfu()
{
    mSfuConnection->retryPendingConnection(true);
}

void Call::removeParticipant(const karere::Id& peer)
{
    for (auto itPeer = mParticipants.begin(); itPeer != mParticipants.end(); itPeer++)
    {
        if (*itPeer == peer)
        {
            mParticipants.erase(itPeer);
            mCallHandler.onRemovePeer(*this, peer);
            return;
        }
    }

    assert(false);
    return;
}

bool Call::alreadyParticipating()
{
    for (auto& peerid : mParticipants)
    {
        if (peerid == mMyPeer->getPeerid())
        {
            return true;
        }
    }

    return false;
}

// for the moment just chatd::kRejected is a valid reason (only for rejecting 1on1 call while ringing)
promise::Promise<void> Call::endCall()
{
    int endCallReason = mIsGroup
            ? chatd::kEndedByModerator            // reject 1on1 call ringing (not answered yet)
            : chatd::kRejected;                   // end group/meeting call by moderator

    return mMegaApi.call(&::mega::MegaApi::endChatCall, mChatid, mCallid, endCallReason)
    .then([](ReqResult /*result*/)
    {
    });
}

promise::Promise<void> Call::hangup()
{
    if (!alreadyParticipating() && mState == kStateClientNoParticipating && mIsRinging && !mIsGroup)
    {
        // in 1on1 calls, the hangup (reject) by the user while ringing should end the call
        return endCall(); // reject 1on1 call while ringing
    }
    else
    {
        orderedCallDisconnect(TermCode::kUserHangup, "normal user hangup");
        return promise::_Void();
    }
}

promise::Promise<void> Call::join(karere::AvFlags avFlags)
{
    if (!isValidInputVideoTracksLimit(mRtc.getNumInputVideoTracks()))
    {
        const std::string errMsg = "join: Invalid value for simultaneous input video tracks";
        RTCM_LOG_WARNING("%s", errMsg.c_str());
        assert(false);
        return promise::Error(errMsg);
    }

    mIsJoining = true; // set flag true to avoid multiple join call attempts
    mMyPeer->setAvFlags(avFlags);
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::joinChatCall, mChatid.val, mCallid.val)
    .then([wptr, this](ReqResult result) -> promise::Promise<void>
    {
        if (wptr.deleted())
            return promise::Error("Join call succeed, but call has already ended");

        std::string sfuUrlStr = result->getText();
        mIsJoining = false;

        if (!connectSfu(sfuUrlStr))
        {
           return promise::Error("connectSfu error, invalid or empty sfu URL");
        }
        else
        {
           return promise::_Void();
        }
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (!wptr.deleted())
            return promise::Error("Join call failed, and call has already ended");

        mIsJoining = false;
        return err;
    });
}

bool Call::participate()
{
    return (mState > kStateClientNoParticipating && mState < kStateTerminatingUserParticipation);
}

bool Call::isJoining() const
{
    return mIsJoining;
}

void Call::enableAudioLevelMonitor(bool enable)
{
    mAudioLevelMonitor = enable;
    for (auto& itSession : mSessions)
    {
        if (!itSession.second->getAudioSlot()) { continue; }
        itSession.second->getAudioSlot()->enableAudioMonitor(enable);
    }
}

void Call::ignoreCall()
{
    mIgnored = true;
}

void Call::setRinging(bool ringing)
{
    if (mIsRinging != ringing)
    {
        mIsRinging = ringing;
        mCallHandler.onCallRinging(*this);
    }
}

void Call::stopOutgoingRinging()
{
    if (!mIsOutgoingRinging)
    {
        return;
    }

    if (!mIsOwnClientCaller || mIsGroup)
    {
        assert(false);
        return;
    }

    // this event must notified just once per call (only for 1on1 calls)
    mIsOutgoingRinging = false;
    mCallHandler.onStopOutgoingRinging(*this);
}

void Call::setOnHold()
{
    // disable audio track
    if (mAudio && mAudio->getTransceiver()->sender()->track())
    {
        mAudio->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // disable hi-res track
    if (mHiRes && mHiRes->getTransceiver()->sender()->track())
    {
        mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // disable low-res track
    if (mVThumb && mVThumb->getTransceiver()->sender()->track())
    {
        mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // release video device
    releaseVideoDevice();
}

void Call::releaseOnHold()
{
    updateAudioTracks();
    updateVideoTracks();
}

bool Call::isIgnored() const
{
    return mIgnored;
}

bool Call::isAudioLevelMonitorEnabled() const
{
    return mAudioLevelMonitor;
}

bool Call::hasVideoSlot(Cid_t cid, bool highRes) const
{
    for (const auto& session : mSessions)
    {
        RemoteSlot *slot = highRes
                ? session.second->getHiResSlot()
                : session.second->getVthumSlot();

        if (slot && slot->getCid() == cid)
        {
            return true;
        }
    }
    return false;
}

int Call::getNetworkQuality() const
{
    return mNetworkQuality;
}

bool Call::hasRequestSpeak() const
{
    return mSpeakerState == SpeakerState::kPending;
}

int Call::getWrJoiningState() const
{
    return static_cast<int>(mWrJoiningState);
}

bool Call::isValidWrJoiningState() const
{
    return mWrJoiningState == WrState::WR_NOT_ALLOWED || mWrJoiningState == WrState::WR_ALLOWED;
}

TermCode Call::getTermCode() const
{
    return mTermCode;
}

uint8_t Call::getEndCallReason() const
{
    return mEndCallReason;
}

void Call::setCallerId(const karere::Id& callerid)
{
    mCallerId  = callerid;
}

void Call::setWrJoiningState(WrState status)
{
    if (!isValidWrStatus(status))
    {
        RTCM_LOG_WARNING("updateAsetWrJoiningState. Invalid status %d", status);
        assert(false);
        return;
    }

    mWrJoiningState = status;
}

bool Call::checkWrFlag() const
{
    if (!isWrFlagEnabled())
    {
        RTCM_LOG_ERROR("Waiting room should be enabled for this call");
        assert(false);
        return false;
    }
    return true;
}

void Call::clearWrJoiningState()
{
    mWrJoiningState = WrState::WR_NOT_ALLOWED;
}

void Call::setPrevCid(Cid_t prevcid)
{
    mPrevCid = prevcid;
}

Cid_t Call::getPrevCid() const
{
    return mPrevCid;
}

bool Call::isRinging() const
{
    return mIsRinging;
}

bool Call::isOutgoingRinging() const
{
    return mIsOutgoingRinging;
}

bool Call::isOutgoing() const
{
    return mCallerId == mMyPeer->getPeerid();
}

int64_t Call::getInitialTimeStamp() const
{
    return mInitialTs;
}

int64_t Call::getFinalTimeStamp() const
{
    return mFinalTs;
}

const char *Call::stateToStr(CallState state)
{
    switch(state)
    {
        RET_ENUM_RTC_NAME(kStateInitial);
        RET_ENUM_RTC_NAME(kStateClientNoParticipating);
        RET_ENUM_RTC_NAME(kStateConnecting);
        RET_ENUM_RTC_NAME(kStateJoining);    // < Joining a call
        RET_ENUM_RTC_NAME(kInWaitingRoom);
        RET_ENUM_RTC_NAME(kStateInProgress);
        RET_ENUM_RTC_NAME(kStateTerminatingUserParticipation);
        RET_ENUM_RTC_NAME(kStateDestroyed);
        default: return "(invalid call state)";
    }
}

karere::AvFlags Call::getLocalAvFlags() const
{
    return mMyPeer->getAvFlags();
}

void Call::updateAndSendLocalAvFlags(karere::AvFlags flags)
{
    if (flags == getLocalAvFlags())
    {
        RTCM_LOG_WARNING("updateAndSendLocalAvFlags: AV flags has not changed");
        return;
    }

    // update and send local AV flags
    karere::AvFlags oldFlags = getLocalAvFlags();
    mMyPeer->setAvFlags(flags);
    mSfuConnection->sendAv(flags.value());

    if (oldFlags.isOnHold() != flags.isOnHold())
    {
        // kOnHold flag has changed
        (flags.isOnHold())
                ? setOnHold()
                : releaseOnHold();

        mCallHandler.onOnHold(*this); // notify app onHold Change
    }
    else
    {
        updateAudioTracks();
        updateVideoTracks();
        mCallHandler.onLocalFlagsChanged(*this);  // notify app local AvFlags Change
    }
}

const KarereWaitingRoom* Call::getWaitingRoom() const
{
    return mWaitingRoom.get();
}

bool Call::isAllowSpeak() const
{
    return mSpeakerState == SpeakerState::kActive;
}

void Call::requestSpeak(bool add)
{
    if (mSpeakerState == SpeakerState::kNoSpeaker && add)
    {
        mSpeakerState = SpeakerState::kPending;
        mSfuConnection->sendSpeakReq();
        return;
    }

    if (mSpeakerState == SpeakerState::kPending && !add)
    {
        mSpeakerState = SpeakerState::kNoSpeaker;
        mSfuConnection->sendSpeakReqDel();
        return;
    }
}

bool Call::isSpeakAllow() const
{
    return mSpeakerState == SpeakerState::kActive && getLocalAvFlags().audio();
}

void Call::approveSpeakRequest(Cid_t cid, bool allow)
{
    if (allow)
    {
        mSfuConnection->sendSpeakReq(cid);
    }
    else
    {
        mSfuConnection->sendSpeakReqDel(cid);
    }
}

void Call::stopSpeak(Cid_t cid)
{
    if (cid)
    {
        assert(mSessions.find(cid) != mSessions.end());
        mSfuConnection->sendSpeakDel(cid);
        return;
    }

    mSfuConnection->sendSpeakDel();
}

void Call::pushUsersIntoWaitingRoom(const std::set<karere::Id>& users, const bool all) const
{
    assert(all || !users.empty());
    mSfuConnection->sendWrPush(users, all);
}

void Call::allowUsersJoinCall(const std::set<karere::Id>& users, const bool all) const
{
    assert(all || !users.empty());
    mSfuConnection->sendWrAllow(users, all);
}

void Call::kickUsersFromCall(const std::set<karere::Id>& users) const
{
    assert(!users.empty());
    mSfuConnection->sendWrKick(users);
}

std::vector<Cid_t> Call::getSpeakerRequested()
{
    std::vector<Cid_t> speakerRequested;

    for (const auto& session : mSessions)
    {
        if (session.second->hasRequestSpeak())
        {
            speakerRequested.push_back(session.first);
        }
    }

    return speakerRequested;
}

void Call::requestHighResolutionVideo(Cid_t cid, int quality)
{
    Session *sess= getSession(cid);
    if (!sess)
    {
        RTCM_LOG_DEBUG("requestHighResolutionVideo: session not found for %u", cid);
        return;
    }

    if (quality < kCallQualityHighDef || quality > kCallQualityHighLow)
    {
        RTCM_LOG_WARNING("requestHighResolutionVideo: invalid resolution divider value (spatial layer offset): %d", quality);
        return;
    }

    if (sess->hasHighResolutionTrack())
    {
        RTCM_LOG_WARNING("High res video requested, but already available");
        sess->notifyHiResReceived();
    }
    else
    {
        mSfuConnection->sendGetHiRes(cid, hasVideoSlot(cid, false) ? 1 : 0, quality);
    }
}

void Call::requestHiResQuality(Cid_t cid, int quality)
{
    if (!hasVideoSlot(cid, true))
    {
        RTCM_LOG_WARNING("requestHiResQuality: Currently not receiving a hi-res stream for this peer");
        return;
    }

    if (quality < kCallQualityHighDef || quality > kCallQualityHighLow)
    {
        RTCM_LOG_WARNING("requestHiResQuality: invalid resolution divider value (spatial layer offset).");
        return;
    }

    mSfuConnection->sendHiResSetLo(cid, quality);
}

void Call::stopHighResolutionVideo(std::vector<Cid_t> &cids)
{
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            RTCM_LOG_DEBUG("stopHighResolutionVideo: session not found for %u", *auxit);
            it = cids.erase(auxit);
        }
        else if (!sess->hasHighResolutionTrack())
        {
            RTCM_LOG_WARNING("stopHighResolutionVideo: high resolution already not available for cid: %u", *auxit);
            it = cids.erase(auxit);
            sess->notifyHiResReceived();    // also used to notify there's no video anymore
        }
    }
    if (!cids.empty())
    {
        for (auto cid: cids)
        {
            Session *sess= getSession(cid);
            sess->disableVideoSlot(kHiRes);
        }

        mSfuConnection->sendDelHiRes(cids);
    }
}

void Call::requestLowResolutionVideo(std::vector<Cid_t> &cids)
{
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            // remove cid that has no active session
            RTCM_LOG_DEBUG("requestLowResolutionVideo: session not found for cid: %u", *auxit);
            it = cids.erase(auxit);
        }
        else if (sess->hasLowResolutionTrack())
        {
            RTCM_LOG_WARNING("requestLowResolutionVideo: low resolution already available for cid: %u", *auxit);
            it = cids.erase(auxit);
            sess->notifyLowResReceived();
        }
    }
    if (!cids.empty())
    {
        mSfuConnection->sendGetVtumbs(cids);
    }
}

void Call::stopLowResolutionVideo(std::vector<Cid_t> &cids)
{
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            RTCM_LOG_DEBUG("stopLowResolutionVideo: session not found for cid: %u", *auxit);
            it = cids.erase(auxit);
        }
        else if (!sess->hasLowResolutionTrack())
        {
            RTCM_LOG_WARNING("stopLowResolutionVideo: low resolution already not available for cid: %u", *auxit);
            it = cids.erase(auxit);
            sess->notifyLowResReceived();
        }
    }
    if (!cids.empty())
    {
        for (auto cid: cids)
        {
            Session *sess= getSession(cid);
            sess->disableVideoSlot(kLowRes);
        }

        mSfuConnection->sendDelVthumbs(cids);
    }
}

void Call::updateSvcQuality(int8_t delta)
{
    // layer: rxSpatial (resolution), rxTemporal (FPS), rxScreenTemporal (for screen video), txSpatial (resolution)
    int8_t rxSpt = 0;
    int8_t rxTmp = 0;
    int8_t rxStmp = 0;
    int8_t txSpt = 0;

    // calculate new layer index from delta and retrieve layer components separately
    if (!mSvcDriver.setSvcLayer(delta, rxSpt, rxTmp, rxStmp, txSpt))
    {
        RTCM_LOG_WARNING("updateSvcQuality: Invalid new layer index %u", mSvcDriver.mCurrentSvcLayerIndex + delta);
        return;
    }

    // adjust Received SVC quality by sending LAYER command
    mSfuConnection->sendLayer(rxSpt, rxTmp, rxStmp);
}

std::set<karere::Id> Call::getParticipants() const
{
    return mParticipants;
}

std::set<karere::Id> Call::getModerators() const
{
    return mModerators;
}

std::vector<Cid_t> Call::getSessionsCids() const
{
    std::vector<Cid_t> returnedValue;

    for (const auto& sessionIt : mSessions)
    {
        returnedValue.push_back(sessionIt.first);
    }

    return returnedValue;
}

ISession* Call::getIsession(Cid_t cid) const
{
    auto it = mSessions.find(cid);
    return (it != mSessions.end())
        ? it->second.get()
        : nullptr;
}

Session* Call::getSession(Cid_t cid)
{
    auto it = mSessions.find(cid);
    return (it != mSessions.end())
        ? it->second.get()
        : nullptr;
}

std::set<Cid_t> Call::getSessionsCidsByUserHandle(const karere::Id& id)
{
    std::set<Cid_t> peers;
    for (const auto& session : mSessions)
    {
        if (session.second->getPeerid() == id)
        {
            peers.insert(session.first);
        }
    }
    return peers;
}

bool Call::connectSfu(const std::string& sfuUrlStr)
{
    if (sfuUrlStr.empty()) // if URL by param is empty, we must ensure that we already have a valid URL
    {
        RTCM_LOG_ERROR("trying to connect to SFU with an Empty URL");
        assert(false);
        return false;
    }

    karere::Url sfuUrl(sfuUrlStr);
    if (!sfuUrl.isValid())
    {
        RTCM_LOG_ERROR("trying to connect to SFU with an Empty Host");
        assert(sfuUrl.isValid());
        return false;
    }

    if (!mRtc.getDnsCache().getRecordByHost(sfuUrl.host) && !mRtc.getDnsCache().addSfuRecord(sfuUrl.host))
    {
        RTCM_LOG_ERROR("connectSfu: can't retrieve nor add SFU record");
        assert(mRtc.getDnsCache().getRecordByHost(sfuUrl.host));
        return false;
    }

    mSfuClient.addVersionToUrl(sfuUrl);
    setState(CallState::kStateConnecting);
    mSfuConnection = mSfuClient.createSfuConnection(mChatid, std::move(sfuUrl), *this, mRtc.getDnsCache());
    return true;
}

void Call::joinSfu()
{
    initStatsValues();
    mRtcConn = artc::MyPeerConnection<Call>(*this, this->mRtc.getAppCtx());
    size_t hiresTrackIndex = 0;
    createTransceivers(hiresTrackIndex);
    mSpeakerState = SpeakerState::kPending;
    getLocalStreams();
    setState(CallState::kStateJoining);

    // even if you are moderator, currently you need to send SPEAK_RQ (which is immediately approved by SFU)
    mSpeakerState = isOwnPrivModerator() && isSpeakRequestEnabled() ? SpeakerState::kPending : SpeakerState::kNoSpeaker;
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    options.offer_to_receive_video = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    auto wptr = weakHandle();
    mRtcConn.createOffer(options)
    .then([wptr, this, hiresTrackIndex](webrtc::SessionDescriptionInterface* sdp) -> promise::Promise<void>
    {
        if (wptr.deleted())
        {
            return ::promise::_Void();
        }

        if (mState != kStateJoining)
        {
            RTCM_LOG_WARNING("joinSfu: get unexpected state change at createOffer");
            assert(false); // theoretically, it should not happen. If so, it may worth to investigate
            return ::promise::_Void();
        }

        if (!mRtcConn)
        {
            assert(mState == kStateClientNoParticipating
                   || mState == kStateTerminatingUserParticipation);
            return ::promise::Error("Failure at initialization. Call destroyed or disconnect");
        }

        KR_THROW_IF_FALSE(sdp->ToString(&mSdpStr));
        sfu::Sdp mungedSdp(mSdpStr, static_cast<int64_t>(hiresTrackIndex)); // Create a Sdp instance from String and modify it to enable SVC
        std::string sdpUncompress = mungedSdp.unCompress(); // get string from modified Sdp instance

        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::SessionDescriptionInterface> sdpInterface(webrtc::CreateSessionDescription(sdp->GetType(), sdpUncompress, &error));
        if (!sdpInterface)
        {
            orderedCallDisconnect(TermCode::kErrSdp, "Error parsing SDP offer: line= " + error.line +"  \nError: " + error.description);
        }

        // update mSdpStr with modified SDP
        KR_THROW_IF_FALSE(sdpInterface->ToString(&mSdpStr));
        return mRtcConn.setLocalDescription(std::move(sdpInterface));   // takes onwership of sdp
    })
    .then([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (mState != kStateJoining)
        {
            RTCM_LOG_WARNING("joinSfu: get unexpected state change at setLocalDescription");
            return;
        }

        sfu::Sdp sdp(mSdpStr);
        std::map<std::string, std::string> ivs;
        ivs[std::to_string(kVthumbTrack)] = sfu::Command::binaryToHex(mVThumb->getIv());
        ivs[std::to_string(kHiResTrack)] = sfu::Command::binaryToHex(mHiRes->getIv());
        ivs[std::to_string(kAudioTrack)] = sfu::Command::binaryToHex(mAudio->getIv());

        // store ivs in MyPeer
        mMyPeer->setIvs(std::vector<std::string> { ivs[std::to_string(kVthumbTrack)],
                                                   ivs[std::to_string(kHiResTrack)],
                                                   ivs[std::to_string(kAudioTrack)] });

        // when reconnecting, send to the SFU the CID of the previous connection, so it can kill it instantly
        setPrevCid(getOwnCid());

        std::string ephemeralKey = generateSessionKeyPair();
        if (ephemeralKey.empty())
        {
            orderedCallDisconnect(TermCode::kErrorCrypto, std::string("Error generating ephemeral keypair"));
        }
        mSfuConnection->joinSfu(sdp, ivs, ephemeralKey, getLocalAvFlags().value(), getPrevCid(), mSpeakerState, kInitialvthumbCount);
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
            return;

        orderedCallDisconnect(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
    });
}

void Call::createTransceivers(size_t &hiresTrackIndex)
{
    assert(mRtcConn);

    // create your transceivers for sending (and receiving)
    webrtc::RtpTransceiverInit transceiverInitVThumb;
    transceiverInitVThumb.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> err
            = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitVThumb);
    mVThumb = ::mega::make_unique<LocalSlot>(*this, err.MoveValue());
    mVThumb->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitHiRes;
    transceiverInitHiRes.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitHiRes);
    hiresTrackIndex = mRtcConn->GetTransceivers().size() - 1; // keep this sentence just after add transceiver for hiRes track
    mHiRes = ::mega::make_unique<LocalSlot>(*this, err.MoveValue());
    mHiRes->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitAudio;
    transceiverInitAudio.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInitAudio);
    mAudio = ::mega::make_unique<LocalSlot>(*this, err.MoveValue());
    mAudio->generateRandomIv();

    // create transceivers for receiving audio from peers
    for (uint32_t i = 1; i < mNumInputAudioTracks; ++i)
    {
        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInit);
    }

    // create transceivers for receiving video from peers
    for (uint32_t i = 2; i < mNumInputVideoTracks; ++i)
    {
        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInit);
    }
}

std::string Call::generateSessionKeyPair()
{
    // generate ephemeral ECDH X25519 keypair
    generateEphemeralKeyPair();
    std::string X25519PubKeyStr(reinterpret_cast<const char*>(getMyEphemeralKeyPair()->getPubKey()), mega::ECDH::PUBLIC_KEY_LENGTH);
    std::string X25519PubKeyB64 = mega::Base64::btoa(X25519PubKeyStr);

    // Generate public key signature (using Ed25519), on the string: sesskey|<callId>|<clientId>|<pubkey>
    std::string signature = "sesskey|" + mCallid.toString() + "|" + std::to_string(mMyPeer->getCid()) + "|" + X25519PubKeyB64;
    return X25519PubKeyB64 + ":" + mSfuClient.getRtcCryptoMeetings()->signEphemeralKey(signature); // -> publicKey:signature
}

void Call::getLocalStreams()
{
    updateAudioTracks();
    if (getLocalAvFlags().camera())
    {
        updateVideoTracks();
    }
}

void Call::orderedCallDisconnect(TermCode termCode, const std::string &msg)
{
    // When the client initiates a disconnect we need to send BYE command to inform SFU about the reason
    RTCM_LOG_DEBUG("orderedCallDisconnect, termcode: %s, msg: %s", connectionTermCodeToString(termCode).c_str(), msg.c_str());
    if (mSfuConnection && mSfuConnection->isOnline())
    {
        sendStats(termCode); // send stats if we are connected to SFU regardless termcode
    }

    if (mIsReconnectingToChatd)
    {
        clearParticipants();
    }

    if (!mSfuConnection || !mSfuConnection->isOnline()
            || termCode == kSigDisconn)  // kSigDisconn is mutually exclusive with the BYE command
    {
        isDestroying()
            ? mRtc.immediateRemoveCall(this, mTempEndCallReason, termCode) // destroy call immediately
            : immediateCallDisconnect(termCode); // we don't need to send BYE command, just perform disconnection

        return;
    }

    // send BYE command as part of the protocol to inform SFU about the disconnection reason
    if (mSfuConnection->isSendingByeCommand())
    {
        RTCM_LOG_DEBUG("orderedCallDisconnect, already sending BYE command");
        return;
    }

    // we need to store termcode temporarily until confirm BYE command has been sent
    mTempTermCode = termCode;

    // once LWS confirms that BYE command has been sent (check processNextCommand) onSendByeCommand will be called
    mSfuConnection->sendBye(termCode);
}

void Call::clearResources(const TermCode& termCode)
{
    RTCM_LOG_DEBUG("clearResources, termcode (%u): %s", termCode, connectionTermCodeToString(termCode).c_str());
    disableStats();
    mSessions.clear();              // session dtor will notify apps through onDestroySession callback

    mModerators.clear();            // clear moderators list and ownModerator
    mMyPeer->setModerator(false);

    mVThumb.reset();
    mHiRes.reset();
    mAudio.reset();
    mReceiverTracks.clear();        // clear receiver tracks after free sessions and audio/video local tracks
    clearWrJoiningState();
    if (!isDisconnectionTermcode(termCode))
    {
        resetLocalAvFlags();        // reset local AvFlags: Audio | Video | OnHold => disabled
    }
}

void Call::mediaChannelDisconnect(bool releaseDevices)
{
    if (releaseDevices)
    {
        if (getLocalAvFlags().camera())
        {
            releaseVideoDevice();
        }

        for (const auto& session : mSessions)
        {
            session.second->disableAudioSlot();
        }
    }

    if (mRtcConn)
    {
        mRtcConn->Close();
        mRtcConn = nullptr;
    }
}

void Call::resetLocalAvFlags()
{
    mMyPeer->setAvFlags(karere::AvFlags::kEmpty);
}

void Call::setTempEndCallReason(uint8_t reason)
{
    mTempEndCallReason = reason;
}

void Call::setEndCallReason(uint8_t reason)
{
    mEndCallReason = reason;
}

std::string Call::endCallReasonToString(const EndCallReason &reason) const
{
    switch (reason)
    {
        case kEnded:            return "normal hangup of on-going call";
        case kRejected:         return "incoming call was rejected by callee";
        case kNoAnswer:         return "outgoing call didn't receive any answer from the callee";
        case kFailed:           return "on-going call failed";
        case kCancelled:        return "outgoing call was cancelled by caller before receiving any answer from the callee";
        case kEndedByMod:       return "ended by moderator";
        case kInvalidReason:    return "invalid endcall reason";
    }
    return "invalid endcall reason";
}

std::string Call::connectionTermCodeToString(const TermCode &termcode) const
{
    switch (termcode)
    {
        case kUserHangup:               return "normal user hangup";
        case kTooManyParticipants:      return "there are too many participants";
        case kLeavingRoom:              return "user has been removed from chatroom";
        case kCallEndedByModerator:     return "group or meeting call has been ended by moderator";
        case kApiEndCall:               return "API/chatd ended call";
        case kPeerJoinTimeout:          return "Nobody joined call";
        case kPushedToWaitingRoom:      return "Our client has been removed from the call and pushed back into the waiting room";
        case kKickedFromWaitingRoom:    return "User has been kicked from call regardless of whether is in the call or in the waiting room";
        case kTooManyUserClients:       return "Too many clients of same user connected";
        case kRtcDisconn:               return "SFU connection failed";
        case kSigDisconn:               return "socket error on the signalling connection";
        case kSfuShuttingDown:          return "SFU server is shutting down";
        case kChatDisconn:              return "chatd connection is broken";
        case kNoMediaPath:              return "webRTC connection failed, no UDP connectivity";
        case kErrSignaling:             return "signalling error";
        case kErrNoCall:                return "attempted to join non-existing call";
        case kErrAuth:                  return "authentication error";
        case kErrApiTimeout:            return "ping timeout between SFU and API";
        case kErrSdp:                   return "error generating or setting SDP description";
        case kErrorProtocolVersion:     return "SFU protocol version not supported";
        case kErrorCrypto:              return "Cryptographic error";
        case kErrClientGeneral:         return "Client general error";
        case kErrGeneral:               return "SFU general error";
        case kUnKnownTermCode:          return "unknown error";
        default:                        return "invalid connection termcode";
    }
}

bool Call::isUdpDisconnected() const
{
    if (!mInitialTs)
    {
        // peerconnection establishment starts as soon ANSWER is sent to the client
        // we never have reached kStateInProgress, as mInitialTs is set when we reach kStateInProgress (upon ANSWER command is received)
        RTCM_LOG_ERROR("onConnectionChange(kDisconnected) received but mInitialTs is not initialized");
        assert(false);
        return true;
    }

    return (mStats.mSamples.mT.empty() && (time(nullptr) - mInitialTs > sfu::SfuConnection::kNoMediaPathTimeout));
}

bool Call::isTermCodeRetriable(const TermCode& termCode) const
{
    return termCode == kRtcDisconn || termCode == kSigDisconn;
}

bool Call::isDisconnectionTermcode(const TermCode& termCode) const
{
    return termCode & kFlagDisconn;
}

Cid_t Call::getOwnCid() const
{
    return mMyPeer->getCid();
}


void Call::setSessionModByUserId(uint64_t userid, bool isMod)
{
    for (const auto& session : mSessions)
    {
        if (session.second->getPeerid() == userid)
        {
            session.second->setModerator(isMod);
        }
    }
}

void Call::setOwnModerator(bool isModerator)
{
    if (mMyPeer->isModerator() == isModerator) { return; }
    mMyPeer->setModerator(isModerator);
    mCallHandler.onPermissionsChanged(*this);
}

bool Call::isValidConnectionTermcode(TermCode termCode) const
{
    return termCode >= kUserHangup && termCode <= kFlagMaxValid;
}

void Call::sendStats(const TermCode& termCode)
{
    if (mStats.isEmptyStats())
    {
        // avoid sending stats more than once upon disconnect
        RTCM_LOG_DEBUG("sendStats: stats are empty");
        return;
    }

    assert(isValidConnectionTermcode(termCode));
    mStats.mDuration = mInitialTs
            ? static_cast<uint64_t>((time(nullptr) - mInitialTs) * 1000)  // ms
            : 0; // in case we have not joined SFU yet, send duration = 0
    mStats.mMaxPeers = mMaxPeers;
    mStats.mTermCode = static_cast<int32_t>(termCode);
    mMegaApi.sdk.sendChatStats(mStats.getJson().c_str());
    RTCM_LOG_DEBUG("Clear local SFU stats");
    mStats.clear();
}

EndCallReason Call::getEndCallReasonFromTermcode(const TermCode& termCode)
{
    if (termCode == kUserHangup)                    { return kEnded; }
    if (termCode == kTooManyParticipants)           { return kFailed; }
    if (termCode == kLeavingRoom)                   { return kEnded; }
    if (termCode == kCallEndedByModerator)          { return kEndedByMod; }
    if (termCode == kApiEndCall)                    { return kFailed; }
    if (termCode == kPeerJoinTimeout)               { return kFailed; }
    if (termCode == kKickedFromWaitingRoom)         { return kEnded; }
    if (termCode & kFlagDisconn)                    { return kFailed; }
    if (termCode & kFlagError)                      { return kFailed; }

    return kInvalidReason;
}

void Call::clearParticipants()
{
    for (auto &it : mParticipants)
    {
        mCallHandler.onRemovePeer(*this, it);
    }
    mParticipants.clear();
}

std::string Call::getKeyFromPeer(Cid_t cid, Keyid_t keyid)
{
    Session *session = getSession(cid);
    return session
            ? session->getPeer().getKey(keyid)
            : std::string();
}

std::vector<mega::byte> Call::generateEphemeralKeyIv(const std::vector<std::string>& peerIvs, const std::vector<std::string>& myIvs) const
{
    std::string salt;
    std::vector<std::string> v { peerIvs[kHiResTrack], peerIvs[kAudioTrack], myIvs[kHiResTrack], myIvs[kAudioTrack] };
    sort(v.begin(), v.end());
    std::for_each(v.begin(), v.end(), [&salt](std::string &s){ salt += s; });
    return sfu::Command::hexToByteArray(salt);
}

bool Call::hasCallKey()
{
    return !mCallKey.empty();
}

bool Call::handleAvCommand(Cid_t cid, unsigned av, uint32_t aMid)
{
    if (mState != kStateJoining && mState != kStateInProgress)
    {
        RTCM_LOG_WARNING("handleAvCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    if (getOwnCid() == cid)
    {
        RTCM_LOG_WARNING("handleAvCommand: Received our own AV flags");
        return false;
    }

    Session *session = getSession(cid);
    if (!session)
    {
        RTCM_LOG_WARNING("handleAvCommand: Received AV flags for unknown peer cid %u", cid);
        return false;
    }

    bool oldAudioFlag = session->getAvFlags().audio();

    // update session flags
    session->setAvFlags(karere::AvFlags(static_cast<uint8_t>(av)));

    if (aMid == sfu::TrackDescriptor::invalidMid)
    {
        if (oldAudioFlag != session->getAvFlags().audio() && session->getAvFlags().audio())
        {
            assert(false);
            RTCM_LOG_WARNING("handleAvCommand: invalid amid received for peer cid: %u with audio flag enabled", cid);
            return false;
        }

        if (oldAudioFlag && !session->getAvFlags().audio())
        {
            removeSpeaker(cid);
        }
    }
    else
    {
        assert(session->getAvFlags().audio());
        sfu::TrackDescriptor trackDescriptor;
        trackDescriptor.mMid = aMid;
        trackDescriptor.mReuse = true;
        addSpeaker(cid, trackDescriptor);
    }

    return true;
}

bool Call::handleAnswerCommand(Cid_t cid, std::shared_ptr<sfu::Sdp> sdp, uint64_t duration, std::vector<sfu::Peer>& peers,
                               const std::map<Cid_t, std::string>& keystrmap,
                               const std::map<Cid_t, sfu::TrackDescriptor>& vthumbs, const std::map<Cid_t, sfu::TrackDescriptor>& speakers
                               , std::set<karere::Id>& moderators, bool ownMod)
{
    if (mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleAnswerCommand: get unexpected state change");
        return false;
    }

    if (!getMyEphemeralKeyPair())
    {
        RTCM_LOG_ERROR("Can't retrieve Ephemeral key for our own user, SFU protocol version: %u", static_cast<unsigned int>(sfu::MY_SFU_PROTOCOL_VERSION));
        return false;
    }

    // clear initial backoff as this connection attempt has succeeded
    mSfuConnection->clearInitialBackoff();

    // set my own client-id (cid)
    mMyPeer->setCid(cid);

    // update max peers seen in call
    mMaxPeers = static_cast<uint8_t> (peers.size() > mMaxPeers ? peers.size() : mMaxPeers);

    // set moderator list and ownModerator value
    setOwnModerator(ownMod);
    mModerators = moderators;

    // this promise will be resolved when all ephemeral keys (for users with SFU > V0) have been verified and derived
    // in case of any of the keys can't be verified or derived, the peer will be added anyway.
    // the promise won't be resolved until all ephemeral keys have been processed (without taking account if the
    // verification or derivation fails)

    // we want to continue with call unless all ephemeral keys verification fails
    // for those peers without a valid derived ephemeral key, our client won't be able to encrypt/decrypt any media key sent or received by that client
    auto keyDerivationPms = std::make_shared<::promise::Promise<void>>();
    if (peers.empty())
    {
        keyDerivationPms->resolve();
    }

    if (isSpeakRequestEnabled() && isOwnPrivModerator())
    {
        if (mSpeakerState == SpeakerState::kActive)
        {
            RTCM_LOG_WARNING("handleAnswerCommand: unexpected mSpeakerState: %d", mSpeakerState);
            assert(false); // this should not happen with current SFU behavior.
                           // If happens it's not a problem but better to be aware of this.
        }
        else
        {
            if (mSpeakerState == SpeakerState::kNoSpeaker)
            {
                RTCM_LOG_WARNING("handleAnswerCommand: unexpected mSpeakerState: %d", mSpeakerState);
                assert(false);
                mSpeakerState = SpeakerState::kPending; // set speaker state to kPending
            }
             requestSpeak(true); // request to speak
        }
    }

    auto keysVerified = std::make_shared<std::vector<bool>>();
    auto onKeyVerified = [max = peers.size(), keysVerified, keyDerivationPms](const bool verified) -> void
    {
        if (keyDerivationPms->done())
        {
            RTCM_LOG_WARNING("handleAnswerCommand: keyDerivationPms already resolved");
            assert(keyDerivationPms->succeeded());
            return;
        }

        if (!keysVerified)
        {
            RTCM_LOG_WARNING("handleAnswerCommand: invalid keysVerified at onKeyVerified");
            assert(false);
            return;
        }

        if (!keyDerivationPms)
        {
            RTCM_LOG_WARNING("handleAnswerCommand: invalid keyDerivationPms at onKeyVerified");
            assert(false);
            return;
        }

        keysVerified->emplace_back(verified);
        if (keysVerified->size() == max)
        {
            keyDerivationPms->resolve();
        }
    };

    auto addPeerWithEphemKey = [this, onKeyVerified](sfu::Peer& peer, const bool keyVerified, const std::string& ephemeralPubKeyDerived) -> void
    {
        addPeer(peer, ephemeralPubKeyDerived);
        onKeyVerified(keyVerified);
    };

    for (sfu::Peer& peer : peers) // does not include own cid
    {
        const auto& it = keystrmap.find(peer.getCid());
        const auto& keyStr = it != keystrmap.end() ? it->second : std::string();

        if (sfu::isInitialSfuVersion(peer.getPeerSfuVersion())) // there's no ephemeral key, just add peer
        {
            addPeerWithEphemKey(peer, true, std::string());
        }
        else if (sfu::isCurrentSfuVersion(peer.getPeerSfuVersion())) // verify ephemeral key signature, derive it, and then add the peer
        {
            if (keyStr.empty())
            {
                RTCM_LOG_ERROR("Empty Ephemeral key for user: %s, cid: %u, SFU protocol version: %u",
                               peer.getPeerid().toString().c_str(), peer.getCid(),
                               static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer.getPeerSfuVersion()));
                addPeerWithEphemKey(peer, false, std::string());
                continue;
            }

            try
            {
                auto wptr = weakHandle();
                auto parsedkey = splitPubKey(keyStr);
                std::shared_ptr<sfu::Peer> auxPeer(new sfu::Peer(peer));
                verifySignature(peer.getCid(), peer.getPeerid(), parsedkey.first, parsedkey.second)
                .then([wptr, auxPeer, addPeerWithEphemKey, parsedkey, this](bool verified)
                {
                    wptr.throwIfDeleted();
                    const mega::ECDH* ephkeypair = getMyEphemeralKeyPair();
                    if (!ephkeypair)
                    {
                        RTCM_LOG_ERROR("Can't retrieve Ephemeral key for our own user, SFU protocol version: %u", static_cast<unsigned int>(sfu::MY_SFU_PROTOCOL_VERSION));
                        addPeerWithEphemKey(*auxPeer, false, std::string());
                        return;
                    }

                    if (!verified)
                    {
                        RTCM_LOG_ERROR("Can't verify signature for user: %s", auxPeer->getPeerid().toString().c_str());
                        addPeerWithEphemKey(*auxPeer, false, std::string());
                        return;
                    }

                    // once peer public ephemeral key has been verified, derive it with our private ephemeral key
                    std::string out;
                    const std::string pubkeyBin = mega::Base64::atob(parsedkey.first);
                    std::vector<::mega::byte> saltBin = generateEphemeralKeyIv(auxPeer->getIvs(), mMyPeer->getIvs());
                    bool derived = ephkeypair->deriveSharedKeyWithSalt(reinterpret_cast<const unsigned char *>(pubkeyBin.data()), saltBin.data(), saltBin.size(), out);
                    if (!derived)
                    {
                        RTCM_LOG_ERROR("Can't derive ephemeral key for peer Cid: %u PeerId: %s",
                                       auxPeer->getCid(), auxPeer->getPeerid().toString().c_str());

                        out.clear();
                    }

                    addPeerWithEphemKey(*auxPeer, derived, out);
                })
                .fail([this, auxPeer, addPeerWithEphemKey](const ::promise::Error&)
                {
                    RTCM_LOG_ERROR("Error verifying ephemeral key signature for for user: %s, cid: %u", auxPeer->getPeerid().toString().c_str(), auxPeer->getCid());
                    addPeerWithEphemKey(*auxPeer, false, std::string());
                });
            }
            catch(std::runtime_error& e)
            {
                RTCM_LOG_ERROR("Error verifying ephemeral key signature: %s", e.what());
                return false; // wprt doesn't exists
            }
        }
        else
        {
            assert(false);
            RTCM_LOG_ERROR("handleAnswerCommand: unknown SFU protocol version [%u] for user: %s, cid: %u",
                           static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer.getPeerSfuVersion()),
                           peer.getPeerid().toString().c_str(), peer.getCid());
            addPeerWithEphemKey(peer, false, std::string());
        }
    }

    // wait until all peers ephemeral keys have been verified and derived
    auto auxwptr = weakHandle();
    keyDerivationPms
    ->then([auxwptr, vthumbs, speakers, duration, sdp, keysVerified, this]
    {
        if (auxwptr.deleted())
        {
            return;
        }

        if (!keysVerified)
        {
            RTCM_LOG_WARNING("handleAnswerCommand: invalid keysVerified at keyDerivationPms resolved");
            assert(false);
            return;
        }

        bool anyVerified = std::any_of(keysVerified->begin(), keysVerified->end(), [](const auto& kv) { return kv; });
        if (!keysVerified->empty() && !anyVerified)
        {
            orderedCallDisconnect(TermCode::kErrorCrypto, "Can't verify any of the ephemeral keys on any peer received in ANSWER command");
            return;
        }

        generateAndSendNewMediakey(true);
        std::string sdpUncompress = sdp->unCompress();
        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::SessionDescriptionInterface> sdpInterface(webrtc::CreateSessionDescription("answer", sdpUncompress, &error));
        if (!sdpInterface)
        {
            orderedCallDisconnect(TermCode::kErrSdp, "Error parsing peer SDP answer: line= " + error.line +"  \nError: " + error.description);
            return;
        }

        assert(mRtcConn);
        auto wptr = weakHandle();
        mRtcConn.setRemoteDescription(std::move(sdpInterface))
        .then([wptr, this, vthumbs, speakers, duration]()
        {
            if (wptr.deleted())
            {
                return;
            }

            if (mState != kStateJoining)
            {
                RTCM_LOG_WARNING("handleAnswerCommand: get unexpect state change at setRemoteDescription");
                return;
            }

            // prepare parameters for low resolution video
            double scale = static_cast<double>(RtcConstant::kHiResWidth) / static_cast<double>(RtcConstant::kVthumbWidth);
            webrtc::RtpParameters parameters = mVThumb->getTransceiver()->sender()->GetParameters();
            if (!parameters.encodings.size())
            {
                orderedCallDisconnect(TermCode::kErrClientGeneral, "Error getting encodings parameters");
                assert(false);
                return;
            }

            parameters.encodings[0].scale_resolution_down_by = scale;
            parameters.encodings[0].max_bitrate_bps = kmax_bitrate_kbps;   // 100 Kbps
            mVThumb->getTransceiver()->sender()->SetParameters(parameters).ok();
            handleIncomingVideo(vthumbs, kLowRes);

            for (const auto& speak : speakers)  // current speakers in the call
            {
                Cid_t cid = speak.first;
                const sfu::TrackDescriptor& speakerDecriptor = speak.second;
                Session* sess = getSession(cid);
                if (!sess)
                {
                    RTCM_LOG_WARNING("handleAnswerCommand: unknown cid: %u in speakers field", cid);
                    continue;
                }
                sess->setSpeakPermission(true); // set speak permission true
                addSpeaker(cid, speakerDecriptor);
            }

            setState(CallState::kStateInProgress);

            mOffset = static_cast<int64_t>(duration);
            enableStats();
        })
        .fail([wptr, this](const ::promise::Error& err)
        {
            if (wptr.deleted()) return;

            std::string msg = "Error setting SDP answer: " + err.msg();
            orderedCallDisconnect(TermCode::kErrSdp, msg);
        });
    });

    return true;
}

bool Call::handleKeyCommand(const Keyid_t& keyid, const Cid_t& cid, const std::string& key)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleKeyCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    Session* session = getSession(cid);
    if (!session)
    {
        RTCM_LOG_WARNING("handleKeyCommand: session not found for Cid: %u", cid);
        return false;
    }

    const sfu::Peer& peer = session->getPeer();
    auto wptr = weakHandle();

    if (sfu::isInitialSfuVersion(peer.getPeerSfuVersion()))
    {
        mSfuClient.getRtcCryptoMeetings()->getCU25519PublicKey(peer.getPeerid())
        .then([wptr, keyid, cid, key, this](Buffer*) -> void
        {
            if (wptr.deleted())
            {
                return;
            }

            Session* session = getSession(cid);
            if (!session)
            {
                RTCM_LOG_WARNING("handleKeyCommand: session not found for Cid: %u", cid);
                return;
            }

            // decrypt received key
            std::string binaryKey = mega::Base64::atob(key);
            strongvelope::SendKey encryptedKey;
            mSfuClient.getRtcCryptoMeetings()->strToKey(binaryKey, encryptedKey);

            strongvelope::SendKey plainKey;
            mSfuClient.getRtcCryptoMeetings()->decryptKeyFrom(session->getPeer().getPeerid(), encryptedKey, plainKey);

            // in case of a call in a public chatroom, XORs received key with the call key for additional authentication
            if (hasCallKey())
            {
                strongvelope::SendKey callKey;
                mSfuClient.getRtcCryptoMeetings()->strToKey(mCallKey, callKey);
                mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(callKey, plainKey);
            }

            // add new key to peer key map
            std::string newKey = mSfuClient.getRtcCryptoMeetings()->keyToStr(plainKey);
            session->addKey(keyid, newKey);
        });
    }
    else if (sfu::isCurrentSfuVersion(peer.getPeerSfuVersion()))
    {
        auto pms = peer.getEphemeralPubKeyPms();
        pms.then([wptr, cid, key, keyid, this]() -> void
        {
            if (wptr.deleted())
            {
                return;
            }

            Session* session = getSession(cid);
            if (!session)
            {
                RTCM_LOG_WARNING("handleKeyCommand: session not found for Cid: %u", cid);
                return;
            }

            const sfu::Peer& auxPeer = session->getPeer();
            auto&& ephemeralPubKey = auxPeer.getEphemeralPubKeyDerived();
            if (ephemeralPubKey.empty())
            {
                RTCM_LOG_WARNING("Invalid ephemeral key for peer: %s cid %u", auxPeer.getPeerid().toString().c_str(), cid);
                assert(false);
                return;
            }

            std::string result;
            std::string recvKeyBin = mega::Base64::atob(key);
            if (!mSymCipher.cbc_decrypt_with_key(recvKeyBin, result, reinterpret_cast<const unsigned char*>(ephemeralPubKey.data())
                                                 , ephemeralPubKey.size(), nullptr))
            {
                std::string err = "Failed cbc_decrypt received key. Cid: "
                        + std::to_string(auxPeer.getCid())
                        + "PeerId: " + auxPeer.getPeerid().toString()
                        + "KeyId: " + std::to_string(keyid);

                mRtc.onMediaKeyDecryptionFailed(err);
                RTCM_LOG_WARNING("%s", err.c_str());
                return;
            }

            // in case of a call in a public chatroom, XORs received key with the call key for additional authentication
            if (hasCallKey())
            {
                mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(reinterpret_cast<::mega::byte*>(mCallKey.data()), reinterpret_cast<::mega::byte*>(result.data()));
            }

            if (result.size() != kMediaKeyLen)
            {
                mRtc.onMediaKeyDecryptionFailed("Unexpected decrypted key size");
                RTCM_LOG_ERROR("Unexpected decrypted key size expected size: %u decrypted size: %d", kMediaKeyLen, static_cast<int>(result.size()));
                return;
            }
            session->addKey(keyid, result);
        });
        pms.fail([peerId = peer.getPeerid(), peerCid = peer.getCid()](const ::promise::Error&)
        {
            RTCM_LOG_DEBUG("Can't get ephemeral public key for peer: %s cid: %u", karere::Id(peerId).toString().c_str(),peerCid);
        });
    }
    else
    {
        RTCM_LOG_ERROR("handleKeyCommand: unknown SFU protocol version [%u] for user: %s, cid: %u",
                       static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer.getPeerSfuVersion()),
                       peer.getPeerid().toString().c_str(), peer.getCid());
        return false;
    }

    return true;
}

bool Call::handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleVThumbsCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    handleIncomingVideo(videoTrackDescriptors, kLowRes);
    return true;
}

bool Call::handleVThumbsStartCommand()
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleVThumbsStartCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mVThumbActive = true;
    updateVideoTracks();
    return true;
}

bool Call::handleVThumbsStopCommand()
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleVThumbsStopCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mVThumbActive = false;
    updateVideoTracks();
    return true;
}

bool Call::handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor>& videoTrackDescriptors)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleHiResCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    handleIncomingVideo(videoTrackDescriptors, kHiRes);
    return true;
}

bool Call::handleHiResStartCommand()
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleHiResStartCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mHiResActive = true;
    updateVideoTracks();
    return true;
}

bool Call::handleHiResStopCommand()
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleHiResStopCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mHiResActive = false;
    updateVideoTracks();
    return true;
}

bool Call::handleSpeakReqsCommand(const std::vector<Cid_t> &speakRequests)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleSpeakReqsCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    for (Cid_t cid : speakRequests)
    {
        if (cid != getOwnCid())
        {
            Session *session = getSession(cid);
            assert(session);
            if (!session)
            {
                RTCM_LOG_ERROR("handleSpeakReqsCommand: Received speakRequest for unknown peer cid %u", cid);
                continue;
            }
            session->setSpeakRequested(true);
        }
    }

    return true;
}

bool Call::handleSpeakReqDelCommand(Cid_t cid)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleSpeakReqDelCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    if (getOwnCid() != cid) // remote peer
    {
        Session *session = getSession(cid);
        assert(session);
        if (!session)
        {
            RTCM_LOG_ERROR("handleSpeakReqDelCommand: Received delSpeakRequest for unknown peer cid %u", cid);
            return false;
        }
        session->setSpeakRequested(false);
    }
    else // own peer
    {
        // our current speaker state is kActive, so no need to do anything with SPEAK_RQ_DEL
        if (mSpeakerState == SpeakerState::kActive) { return true; }

        mSpeakerState = SpeakerState::kNoSpeaker;
        updateAudioTracks();
    }
    return true;
}

bool Call::handleSpeakOnCommand(Cid_t cid)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleSpeakOnCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    if (cid) // SPEAK_ON received for another peer
    {
        Session* session = getSession(cid);
        if (!session)
        {
            RTCM_LOG_WARNING("handleSpeakOnCommand: session not found for Cid: %u", cid);
            return false;
        }

        session->setSpeakPermission(true);
    }
    else if (mSpeakerState != SpeakerState::kActive)
    {
        if (isSpeakRequestEnabled() && mSpeakerState != SpeakerState::kPending)
        {
            // This could happen if moderator sends SPEAK_RQ but this client didn't sent any previous request (check if this could happen)
            RTCM_LOG_WARNING("handleSpeakOnCommand: Received speak on for own cid %u without a pending requests", cid);
        }

        mSpeakerState = SpeakerState::kActive;
        updateAudioTracks();
    }
    return true;
}

bool Call::handleSpeakOffCommand(Cid_t cid)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handleSpeakOffCommand: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    if (cid)
    {
        Session* session = getSession(cid);
        if (!session)
        {
            RTCM_LOG_WARNING("handleSpeakOffCommand: session not found for Cid: %u", cid);
            return false;
        }

        session->setSpeakPermission(false);
    }
    else if (mSpeakerState == SpeakerState::kActive)
    {
        // SPEAK_OFF received from SFU requires to mute our client (audio flag is already unset from the SFU's viewpoint)
        mSpeakerState = SpeakerState::kNoSpeaker;
        muteMyClientFromSfu();
    }
    else // SPEAK_OFF received own cid, but SpeakerState is not kActive
    {
        RTCM_LOG_ERROR("handleSpeakOffCommand: Received speak off for own cid %u without being active", cid);
        assert(false);
        return false;
    }

    return true;
}


bool Call::handlePeerJoin(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string>& ivs)
{
    auto addPeerWithEphemKey = [this](sfu::Peer& peer, const std::string& ephemeralPubKeyDerived) -> void
    {
        addPeer(peer, ephemeralPubKeyDerived);
        // update max peers seen in call
        mMaxPeers = std::max(mMaxPeers, static_cast<uint8_t>(mSessions.size()));
        generateAndSendNewMediakey();

        if (mIsReconnectingToChatd && mParticipants.find(peer.getPeerid()) == mParticipants.end())
        {
            // if we are disconnected from chatd, but still connected to SFU and participating in a call
            // we need to update participants list with SFU information
            addParticipant(peer.getPeerid());
        }
    };

    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handlePeerJoin: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    const mega::ECDH* ephkeypair = getMyEphemeralKeyPair();
    if (!ephkeypair)
    {
        RTCM_LOG_ERROR("Can't retrieve Ephemeral key for our own user, SFU protocol version: %u", static_cast<unsigned int>(sfu::MY_SFU_PROTOCOL_VERSION));
        orderedCallDisconnect(TermCode::kErrorCrypto, "Can't retrieve Ephemeral key for our own user");
        return false;
    }

    std::shared_ptr<sfu::Peer> peer(new sfu::Peer(userid, sfuProtoVersion, static_cast<unsigned>(av), &ivs, cid, (mModerators.find(userid) != mModerators.end())));
    if (sfu::isInitialSfuVersion(sfuProtoVersion))
    {
        addPeerWithEphemKey(*peer, std::string());
    }
    else if (sfu::isCurrentSfuVersion(sfuProtoVersion))
    {
        if (keyStr.empty())
        {
            RTCM_LOG_ERROR("handlePeerJoin: ephemeral key not received");
            assert(false);
            addPeerWithEphemKey(*peer, std::string());
            return false;
        }

        auto parsedkey = splitPubKey(keyStr);
        verifySignature(cid, userid, parsedkey.first, parsedkey.second)
        .then([userid, parsedkey, peer, ephkeypair, addPeerWithEphemKey, this](bool verified)
        {
            if (!verified)
            {
                RTCM_LOG_WARNING("Can't verify signature for user: %s", karere::Id(userid).toString().c_str());
                assert(false);
                addPeerWithEphemKey(*peer, std::string());
                return;
            }

            // derive peer public ephemeral key with our private ephemeral key
            std::string out;
            const std::string pubkeyBin = mega::Base64::atob(parsedkey.first);
            std::vector<::mega::byte> saltBin = generateEphemeralKeyIv(peer->getIvs(), mMyPeer->getIvs());
            bool derived = ephkeypair->deriveSharedKeyWithSalt(reinterpret_cast<const unsigned char *>(pubkeyBin.data()), saltBin.data(), saltBin.size(), out);
            if (!derived)
            {
                RTCM_LOG_WARNING("Can't derive ephemeral key for peer Cid: %u PeerId: %s",
                               peer->getCid(), peer->getPeerid().toString().c_str());

                out.clear();
            }
            addPeerWithEphemKey(*peer, out);
        })
        .fail([this, userid, peer, addPeerWithEphemKey](const ::promise::Error&)
        {
            RTCM_LOG_ERROR("Can't retrieve public ED25519 attr for user %s", karere::Id(userid).toString().c_str());
            addPeerWithEphemKey(*peer, std::string());
        });
    }
    else
    {
        RTCM_LOG_ERROR("handlePeerJoin: unknown SFU protocol version [%u] for user: %s, cid: %u",
                       static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer->getPeerSfuVersion()),
                       peer->getPeerid().toString().c_str(), peer->getCid());
        assert(false);
        addPeerWithEphemKey(*peer, std::string());
        return false;
    }
    return true;
}

bool Call::handlePeerLeft(Cid_t cid, unsigned termcode)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("handlePeerLeft: get unexpected state");
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    auto it = mSessions.find(cid);
    if (it == mSessions.end())
    {
        RTCM_LOG_ERROR("handlePeerLeft: unknown cid");
        return false;
    }

    if (mIsReconnectingToChatd && mParticipants.find(it->second->getPeerid()) != mParticipants.end()
                && getSessionsCidsByUserHandle(it->second->getPeerid()).size() == 1)
    {
        // Check that received peer left is not participating in meeting with more than one client

        // if we are disconnected from chatd but still connected to SFU, and participating in a call
        // we need to update participants list with SFU information
        mParticipants.erase(it->second->getPeerid());
        mCallHandler.onRemovePeer(*this, it->second->getPeerid());
    }

    // set session termcode before destroying it (in order to app can be notified through OnChatSessionUpdate)
    TermCode peerLeftTermCode = static_cast<TermCode>(termcode);
    assert(isValidConnectionTermcode(peerLeftTermCode));
    it->second->setTermcode(peerLeftTermCode);
    mSessions.erase(cid);

    if (!mIsGroup && !isTermCodeRetriable(peerLeftTermCode))
    {
        RTCM_LOG_DEBUG("handlePeerLeft. Hangup 1on1 call, upon reception of PEERLEFT with non recoverable termcode: %s", connectionTermCodeToString(peerLeftTermCode).c_str());
        hangup(); // TermCode::kUserHangup
    }
    return true;
}

bool Call::handleBye(const unsigned termCode, const bool wr, const std::string& errMsg)
{
    RTCM_LOG_WARNING("handleBye - termCode: %d, reason: %s", termCode, errMsg.c_str());
    TermCode auxTermCode = static_cast<TermCode> (termCode);
    if (!isValidConnectionTermcode(auxTermCode))
    {
        RTCM_LOG_ERROR("Invalid termCode [%u] received at BYE command", termCode);
        return false;
    }

    if (wr) // we have been moved into a waiting room
    {
        assert (auxTermCode == kPushedToWaitingRoom);
        if (!isValidWrJoiningState())
        {
            RTCM_LOG_ERROR("handleBye: wr received but our current WrJoiningState is not valid");
            assert(false);
            return false;
        }
        pushIntoWr(auxTermCode);
    }
    else
    {
        if (auxTermCode == kKickedFromWaitingRoom)
        {
            auto wptr = weakHandle();
            karere::marshallCall([wptr, auxTermCode, this]()
            {
                // need to marshall this, otherwise there could be memory issues when we remove Sfuconnection
                if (wptr.deleted())
                {
                    return;
                }

                RTCM_LOG_DEBUG("handleBye: immediate call disconnect due to BYE [%u] command received from SFU (kKickedFromWaitingRoom)", auxTermCode);
                immediateCallDisconnect(auxTermCode); // we don't need to send BYE command, just perform disconnection
            }, mRtc.getAppCtx());
        }
        else
        {
            EndCallReason reason = getEndCallReasonFromTermcode(auxTermCode);
            if (reason == kInvalidReason)
            {
                RTCM_LOG_ERROR("Invalid end call reason for termcode [%u]", termCode);
                assert(false); // we don't need to fail, just log a msg and assert => check getEndCallReasonFromTermcode
            }

            auto wptr = weakHandle();
            karere::marshallCall([wptr, auxTermCode, reason, this]()
            {
                RTCM_LOG_DEBUG("Immediate removing call due to BYE [%u] command received from SFU", auxTermCode);
                setDestroying(true); // we need to set destroying true to avoid notifying (kStateClientNoParticipating) when sfuDisconnect is called, and we are going to finally remove call
                mRtc.immediateRemoveCall(this, reason, auxTermCode);
            }, mRtc.getAppCtx());
        }
    }
    return true;
}

bool Call::handleModAdd(uint64_t userid)
{
    if (userid == mMyPeer->getPeerid())
    {
        setOwnModerator(true);
    }

    // update moderator privilege for all sessions that mached with received userid
    setSessionModByUserId(userid, true);

    if (!mModerators.emplace(userid).second)
    {
        RTCM_LOG_WARNING("MOD_ADD: user[%s] already added in moderators list", karere::Id(userid).toString().c_str());
        return false;
    }

    RTCM_LOG_DEBUG("MOD_ADD: user[%s] added in moderators list", karere::Id(userid).toString().c_str());
    return true;
}

bool Call::handleModDel(uint64_t userid)
{
    if (userid == mMyPeer->getPeerid())
    {
        setOwnModerator(false);
    }

    // update moderator privilege for all sessions that mached with received userid
    setSessionModByUserId(userid, false);

    if (!mModerators.erase(userid))
    {
        RTCM_LOG_WARNING("MOD_DEL: user[%s] not found in moderators list", karere::Id(userid).toString().c_str());
        return false;
    }

    RTCM_LOG_DEBUG("MOD_DEL: user[%s] removed from moderators list", karere::Id(userid).toString().c_str());
    return true;
}

bool Call::handleHello(const Cid_t cid, const unsigned int nAudioTracks,
                                   const std::set<karere::Id>& mods, const bool wr, const bool speakRequest, const bool allowed,
                                   const std::map<karere::Id, bool>& wrUsers)
{
    // mNumInputAudioTracks & mNumInputAudioTracks are used at createTransceivers after receiving HELLO command
    const auto numInputVideoTracks = mRtc.getNumInputVideoTracks();
    if (!isValidInputVideoTracksLimit(numInputVideoTracks))
    {
        RTCM_LOG_ERROR("Invalid number of simultaneus video tracks: %d", numInputVideoTracks);
        return false;
    }
    mNumInputVideoTracks = numInputVideoTracks; // Set the maximum number of simultaneous video tracks the call supports

    setSpeakRequest(speakRequest);

    // Set the maximum number of simultaneous audio tracks the call supports. If no received nAudioTracks or nVideoTracks set as max default
    mNumInputAudioTracks = nAudioTracks ? nAudioTracks : static_cast<uint32_t>(RtcConstant::kMaxCallAudioSenders);

    // set moderator list and ownModerator value
    setOwnModerator(mods.find(mMyPeer->getPeerid()) != mods.end());
    mModerators = mods;

    // set my own client-id (cid)
    mMyPeer->setCid(cid);
    mSfuConnection->setMyCid(cid);

    // set flag to check if wr is enabled or not for this call
    setWrFlag(wr);

    if (!wr) // if waiting room is disabled => send JOIN command to SFU
    {
        joinSfu();
    }
    else
    {
        // set kInWaitingRoom state, even if we are allowed to JOIN. Just if we are not allowed,
        // we must wait in waiting room until a moderator allow to access, otherwise we can continue with JOIN
        assert(allowed || !isOwnPrivModerator());
        setState(CallState::kInWaitingRoom);
        setWrJoiningState(allowed ? WrState::WR_ALLOWED : WrState::WR_NOT_ALLOWED);
        if (allowed)
        {
            joinSfu();
        }

        return dumpWrUsers(wrUsers, true/*clearCurrent*/);
    }
    return true;
}

bool Call::handleWrDump(const std::map<karere::Id, bool>& users)
{
    if (!checkWrCommandReqs("WR_DUMP", true /*mustBeModerator*/))
    {
        return false;
    }
    return dumpWrUsers(users, true/*clearCurrent*/);
}

bool Call::handleWrEnter(const std::map<karere::Id, bool>& users)
{
    if (!checkWrCommandReqs("WR_ENTER", true /*mustBeModerator*/))
    {
        return false;
    }

    assert(!users.empty());
    if (!addWrUsers(users, false/*clearCurrent*/))
    {
        return false;
    }

    std::unique_ptr<mega::MegaHandleList> uhl(mega::MegaHandleList::createInstance());
    std::for_each(users.begin(), users.end(), [&uhl](const auto &u) { uhl->addMegaHandle(u.first.val); });
    mCallHandler.onWrUsersEntered(*this, uhl.get());
    return true;
}

bool Call::handleWrLeave(const karere::Id& user)
{
    if (!checkWrCommandReqs("WR_LEAVE", true /*mustBeModerator*/))
    {
        return false;
    }

    if (!user.isValid())
    {
        RTCM_LOG_ERROR("WR_LEAVE : invalid user received");
        assert(false);
        return false;
    }

    if (!mWaitingRoom)
    {
        RTCM_LOG_WARNING("WR_LEAVE : mWaitingRoom is null");
        assert(false);
        mWaitingRoom.reset(new KarereWaitingRoom()); // instanciate in case it doesn't exists
        return false;
    }

    if (!mWaitingRoom->removeUser(user.val))
    {
        RTCM_LOG_WARNING("WR_LEAVE : user not found in waiting room: %s", user.toString().c_str());
        return false;
    }

    std::unique_ptr<mega::MegaHandleList> uhl(mega::MegaHandleList::createInstance());
    uhl->addMegaHandle(user.val);
    mCallHandler.onWrUsersLeave(*this, uhl.get());
    return true;
}

bool Call::handleWrAllow(const Cid_t& cid, const std::set<karere::Id>& mods)
{
    if (!checkWrCommandReqs("WR_ALLOW", false /*mustBeModerator*/))
    {
        return false;
    }

    if (cid == K_INVALID_CID)
    {
        RTCM_LOG_ERROR("WR_ALLOW: Invalid cid received: %d", cid);
        assert(false);
    }

    if (mState != CallState::kInWaitingRoom) { return false; }
    mMyPeer->setCid(cid); // update Cid for own client from SFU
    mModerators = mods;
    setWrJoiningState(WrState::WR_ALLOWED);
    RTCM_LOG_DEBUG("handleWrAllow: we have been allowed to join call, so we need to send JOIN command to SFU");
    joinSfu(); // send JOIN command to SFU
    mCallHandler.onWrAllow(*this);
    return true;
}

bool Call::handleWrDeny(const std::set<karere::Id>& mods)
{
    if (!checkWrCommandReqs("WR_DENY", false /*mustBeModerator*/))
    {
        return false;
    }

    if (mState != CallState::kInWaitingRoom)
    {
        return false;
    }

    mModerators = mods;
    setWrJoiningState(WrState::WR_NOT_ALLOWED);
    mCallHandler.onWrDeny(*this);
    return true;
}

bool Call::handleWrUsersAllow(const std::set<karere::Id>& users)
{
    return manageAllowedDeniedWrUSers(users, true /*allow*/, "WR_USERS_ALLOW");
}

bool Call::handleWrUsersDeny(const std::set<karere::Id>& users)
{
    return manageAllowedDeniedWrUSers(users, false /*allow*/, "WR_USERS_DENY");
}

void Call::onSfuDisconnected()
{
    if (isDestroying()) // we was trying to destroy call but we have received a sfu socket close (before processing BYE command)
    {
        if (!mSfuConnection->isSendingByeCommand())
        {
            // if we called orderedDisconnectAndCallRemove (call state not between kStateConnecting and kStateInProgress) immediateRemoveCall would have been called, and call wouldn't exists at this point
            RTCM_LOG_ERROR("onSfuDisconnected: call is being destroyed but we are not sending BYE command, current call shouldn't exist at this point");
            assert(mSfuConnection->isSendingByeCommand()); // in prod fallback to mediaChannelDisconnect and clearResources
        }
        else
        {
            auto wptr = weakHandle();
            karere::marshallCall([wptr, this]()
            {
                if (wptr.deleted())
                {
                    return;
                }
                /* if we called orderedDisconnectAndCallRemove (call state between kStateConnecting and kStateInProgress),
                 * but socket has been closed before BYE command is delivered, we need to remove call */
                mRtc.immediateRemoveCall(this, mTempEndCallReason, kSigDisconn);
            }, mRtc.getAppCtx());
            return;
        }
    }

    // Not necessary to call to orderedCallDisconnect, as we are not connected to SFU
    // disconnect from media channel and clear resources
    mediaChannelDisconnect();
    clearResources(kRtcDisconn);
    setState(CallState::kStateConnecting);
}

void Call::immediateCallDisconnect(const TermCode& termCode)
{
    bool hadParticipants = !mSessions.empty();
    mediaChannelDisconnect(true /*releaseDevices*/);
    clearResources(termCode);
    sfuDisconnect(termCode, hadParticipants);
}

void Call::sfuDisconnect(const TermCode& termCode, bool hadParticipants)
{
    if (isTermCodeRetriable(termCode))
    {
        // if termcode is retriable, a reconnection attempt should be started automatically, so we can't destroy mSfuConnection
        RTCM_LOG_DEBUG("sfuDisconnect: can't disconnect from SFU as termcode is retriable %s", connectionTermCodeToString(termCode).c_str());
        return;
    }

    if (mState > CallState::kStateInProgress)
    {
        RTCM_LOG_DEBUG("sfuDisconnect, current call state is %s", mState == CallState::kStateDestroyed ? "kStateDestroyed": "kStateTerminatingUserParticipation");
        assert(!mSfuConnection);
        return;
    }

    RTCM_LOG_DEBUG("callDisconnect, termcode (%u): %s", termCode, connectionTermCodeToString(termCode).c_str());
    mTermCode = termCode; // termcode is only valid at state kStateTerminatingUserParticipation
    setState(CallState::kStateTerminatingUserParticipation);

    // skip kStateClientNoParticipating notification if:
    bool skipClientNoParticipating = (isDestroying())             // we are destroying call
            || (!hadParticipants && mSfuConnection && mSfuConnection->isJoined());  // no more participants but still joined to SFU

    if (mSfuConnection)
    {
        mSfuClient.closeSfuConnection(mChatid);
        mSfuConnection = nullptr;
    }

    if (!skipClientNoParticipating)
    {
        mTermCode = kInvalidTermCode;
        setState(CallState::kStateClientNoParticipating);
    }
}

void Call::onSendByeCommand()
{
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this]()
    {
        // need to marshall this, otherwise there could be memory issues when we remove Sfuconnection
        if (wptr.deleted())
        {
            return;
        }

        if (!mSfuConnection)
        {
            RTCM_LOG_DEBUG("onSendByeCommand: SFU connection no longer exists");
            return;
        }

        if (mState == CallState::kStateConnecting)
        {
            // we have sent BYE command from onConnectionChange (kDisconnected | kFailed | kClosed)
            // and now we need to force reconnect to SFU
            mSfuConnection->clearCommandsQueue();
            mSfuConnection->retryPendingConnection(true);
            return;
        }

        if (isDestroying()) // we was trying to destroy call, and we have received BYE command delivering notification
        {
            mRtc.immediateRemoveCall(this, mTempEndCallReason, mTempTermCode);
        }
        else
        {
            // once we have confirmed that BYE command has been sent, we can proceed with disconnect
            assert (mTempTermCode != kInvalidTermCode);

            // close sfu and media channel connection and clear some call stuff
            immediateCallDisconnect(mTempTermCode);
            mTempTermCode = kInvalidTermCode;
        }
    }, mRtc.getAppCtx());
}

bool Call::processDeny(const std::string& cmd, const std::string& msg)
{
    mCallHandler.onCallDeny(*this, cmd, msg); // notify apps about the denied command

    if (cmd == "audio") // audio ummute has been denied by SFU
    {
        muteMyClientFromSfu();
    }
    else if (cmd == "JOIN")
    {
        if (mState != kStateJoining)
        {
            RTCM_LOG_ERROR("Deny 'JOIN' received. Current call state: %u, expected call state: %u. %s",
                           mState, kStateJoining, msg.c_str());
            return false;
        }
        orderedCallDisconnect(TermCode::kErrorProtocolVersion, "Client doesn't supports waiting rooms");
    }
    else
    {
        assert(false);
        RTCM_LOG_ERROR("Deny cmd received for unexpected command: %s", msg.c_str());
        return false;
    }
    return true;
}

bool Call::error(unsigned int code, const std::string &errMsg)
{
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this, code, errMsg]()
    {
        // error() is called from LibwebsocketsClient::wsCallback() for LWS_CALLBACK_CLIENT_RECEIVE.
        // If disconnect() is called here immediately, it will destroy the LWS client synchronously,
        // leave it in an invalid state (and will crash at Libwebsockets::resetMessage())

        if (wptr.deleted())
        {
            return;
        }

        TermCode connectionTermCode = static_cast<TermCode>(code);

        // send call stats
        if (mSfuConnection && mSfuConnection->isOnline())
        {
            sendStats(connectionTermCode);
        }

        // notify SFU error to the apps
        std::string errMsgStr = errMsg.empty() || !errMsg.compare("Unknown reason") ? connectionTermCodeToString(static_cast<TermCode>(code)): errMsg;
        mCallHandler.onCallError(*this, static_cast<int>(code), errMsgStr);

        // remove call just if there are no participants or termcode is not recoverable (we don't need to send BYE command upon SFU error reception)
        assert(!isTermCodeRetriable(connectionTermCode));
        if (!isTermCodeRetriable(connectionTermCode) || mParticipants.empty())
        {
            //immediateCallDisconnect will be called inside immediateRemoveCall
            setDestroying(true); // we need to set destroying true to avoid notifying (kStateClientNoParticipating) when sfuDisconnect is called, and we are going to finally remove call
            mRtc.immediateRemoveCall(this, EndCallReason::kFailed, connectionTermCode);
        }
    }, mRtc.getAppCtx());

    return true;
}

void Call::logError(const char *error)
{
    RTCM_LOG_ERROR("SFU: %s", error);
}

void Call::onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> /*stream*/)
{
    if (mState != kStateJoining)
    {
        RTCM_LOG_WARNING("onAddStream: get unexpected state");
        assert(mState != kStateInProgress); // theoretically, it should not happen. If so, it may worth to investigate
        return;
    }

    assert(mVThumb && mHiRes && mAudio);
    mVThumb->createEncryptor();
    mHiRes->createEncryptor();
    mAudio->createEncryptor();
}

void Call::onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    if (mState != kStateJoining)
    {
        RTCM_LOG_WARNING("onTrack: get unexpected state");
        assert(mState != kStateInProgress); // theoretically, it should not happen. If so, it may worth to investigate
        return;
    }

    absl::optional<std::string> mid = transceiver->mid();
    if (mid.has_value())
    {
        std::string value = mid.value();
        if (transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO)
        {
            mReceiverTracks[static_cast<uint32_t>(atoi(value.c_str()))] = ::mega::make_unique<RemoteAudioSlot>(*this, transceiver,
                                                                                                               mRtc.getAppCtx());
        }
        else
        {
            mReceiverTracks[static_cast<uint32_t>(atoi(value.c_str()))] = ::mega::make_unique<RemoteVideoSlot>(*this, transceiver,
                                                                                                               mRtc.getAppCtx());
        }
    }
}

void Call::onRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> /*receiver*/)
{
    RTCM_LOG_DEBUG("onRemoveTrack received");
}

void Call::onConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState)
{
    RTCM_LOG_DEBUG("onConnectionChange newstate: %d", newState);
    if (!mSfuConnection)
    {
        RTCM_LOG_WARNING("onConnectionChange: mSfuConnection no longer exists");
        return;
    }

    if (newState >= webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected)
    {

        if (isDestroying()) // we was trying to destroy call, but we have received onConnectionChange. Don't do anything else (wait for BYE command delivering)
        {
            return;
        }

        if (mState == CallState::kStateJoining ||  mState == CallState::kStateInProgress) //  kStateConnecting isn't included to avoid interrupting a reconnection in progress
        {
            if (!mSfuConnection)
            {
                RTCM_LOG_ERROR("onConnectionChange: Not valid SfuConnection upon PeerConnectionState kDisconnected received");
                assert(false);
                return;
            }

            if (isUdpDisconnected()) // lack of UDP connectity detected, disconnect call and don't try to reconnect
            {
                RTCM_LOG_DEBUG("WebRTC connection failed, there's no UDP connectivity");
                orderedCallDisconnect(TermCode::kNoMediaPath, connectionTermCodeToString(TermCode::kNoMediaPath).c_str());
                return;
            }

            if (!mSfuConnection->isOnline())
            {
                setState(CallState::kStateConnecting);
                mSfuConnection->clearCommandsQueue();
                mSfuConnection->retryPendingConnection(true);
            }
            else if (!mSfuConnection->isSendingByeCommand())    // if we are connected to SFU we need to send BYE command (if we haven't already done)
            {                                                   // don't clear commands queue here, wait for onSendByeCommand
                setState(CallState::kStateConnecting);          // just set kStateConnecting if we have not already sent a previous BYE command, or executed action upon onSendByeCommand won't match with expected one
                sendStats(TermCode::kRtcDisconn);               // send stats if we are connected to SFU regardless termcode
                mSfuConnection->sendBye(TermCode::kRtcDisconn); // once LWS confirms that BYE command has been sent (check processNextCommand) onSendByeCommand will be called
            }
        }
    }
    else if (newState == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    {
        bool reconnect = !mSfuConnection->isOnline();
        RTCM_LOG_DEBUG("onConnectionChange retryPendingConnection (reconnect) : %d", reconnect);
        mSfuConnection->retryPendingConnection(reconnect);
    }
}

bool Call::addWrUsers(const std::map<karere::Id, bool>& users, const bool clearCurrent)
{
    if (!isOwnPrivModerator() && !users.empty())
    {
        RTCM_LOG_ERROR("addWrUsers : SFU has sent wr users list to a non-moderator user");
        mWaitingRoom.reset();
        assert(false);
        return false;
    }

    if (clearCurrent && mWaitingRoom)   { mWaitingRoom->clear(); }
    else if (!mWaitingRoom)             { mWaitingRoom.reset(new KarereWaitingRoom()); }

    std::for_each(users.begin(), users.end(), [this](const auto &u)
    {
        mWaitingRoom->addOrUpdateUserStatus(u.first, u.second);
    });
    return true;
}

void Call::pushIntoWr(const TermCode& termCode)
{
    if (mSfuConnection && mSfuConnection->isOnline())
    {
        sendStats(termCode); //send stats
    }

    // keep mSfuConnection intact just disconnect from media channel
    mediaChannelDisconnect(true /*releaseDevices*/);
    clearResources(termCode);
    mTermCode = termCode; // termcode is only valid at state kStateTerminatingUserParticipation
    setState(CallState::kInWaitingRoom);
    mCallHandler.onWrPushedFromCall(*this);
}

bool Call::dumpWrUsers(const std::map<karere::Id, bool>& wrUsers, bool clearCurrent)
{
    if (!addWrUsers(wrUsers, clearCurrent))
    {
        return false;
    }
    mCallHandler.onWrUserDump(*this); // notify app about users in wr
    return true;
}

bool Call::checkWrCommandReqs(std::string && commandStr, bool mustBeModerator)
{
    if (mustBeModerator && !isOwnPrivModerator())
    {
        RTCM_LOG_ERROR("%s. Waiting room command received for our client with non moderator permissions for this call: %s",
                       commandStr.c_str(), getCallid().toString().c_str());
        assert(false);
        return false;
    }

    if (!checkWrFlag())
    {
        RTCM_LOG_ERROR("%s. Waiting room should be enabled for this call: %s", commandStr.c_str(), getCallid().toString().c_str());
        assert(false);
        return false;
    }
    return true;
}

bool Call::manageAllowedDeniedWrUSers(const std::set<karere::Id>& users, bool allow, std::string && commandStr)
{
    if (!checkWrCommandReqs(commandStr.c_str(), true /*mustBeModerator*/))
    {
        return false;
    }

    if (users.empty())
    {
        RTCM_LOG_ERROR("%s : empty user list received", commandStr.c_str());
        assert(false);
        return false;
    }

    if (!mWaitingRoom)
    {
        RTCM_LOG_WARNING("%s : mWaitingRoom is null", commandStr.c_str());
        assert(false);
        mWaitingRoom.reset(new KarereWaitingRoom()); // instanciate in case it doesn't exists
    }

    if (!mWaitingRoom->updateUsers(users, allow ? WrState::WR_ALLOWED : WrState::WR_NOT_ALLOWED))
    {
        RTCM_LOG_WARNING("%s : could not update users status in waiting room", commandStr.c_str());
        return false;
    }

    std::unique_ptr<mega::MegaHandleList> uhl(mega::MegaHandleList::createInstance());
    std::for_each(users.begin(), users.end(), [&uhl](const auto &u) { uhl->addMegaHandle(u.val); });
    allow
        ? mCallHandler.onWrUsersAllow(*this, uhl.get())
        : mCallHandler.onWrUsersDeny(*this, uhl.get());

    return true;
}

Keyid_t Call::generateNextKeyId()
{
    if (mMyPeer->getCurrentKeyId() >= 255
            || (!mMyPeer->getCurrentKeyId() && !mMyPeer->hasAnyKey()))
    {
        // if we have exceeded max keyid => reset keyid to zero
        // if current keyId is zero and we don't have stored any key => first keyId (zero)
        return 0;
    }
    else
    {
        return static_cast<Keyid_t>(mMyPeer->getCurrentKeyId() + 1);
    }
}

void Call::generateAndSendNewMediakey(bool reset)
{
    if (reset)
    {
        // when you leave a meeting or you experiment a reconnect, we should reset keyId to zero and clear keys map
        mMyPeer->resetKeys();
    }

    // generate a new plain key
    std::shared_ptr<strongvelope::SendKey> newPlainKey = mSfuClient.getRtcCryptoMeetings()->generateSendKey();

    // add new key to own peer key map and update currentKeyId
    Keyid_t newKeyId = generateNextKeyId();
    std::string plainKeyStr = mSfuClient.getRtcCryptoMeetings()->keyToStr(*newPlainKey.get());

    // in case of a call in a public chatroom, XORs new key with the call key for additional authentication
    if (hasCallKey())
    {
        strongvelope::SendKey callKey;
        mSfuClient.getRtcCryptoMeetings()->strToKey(mCallKey, callKey);
        mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(callKey, *newPlainKey.get());
    }

    std::vector<promise::Promise<Buffer*>> promises;
    for (const auto& session : mSessions) // encrypt key to all participants
    {
        promises.push_back(mSfuClient.getRtcCryptoMeetings()->getCU25519PublicKey(session.second->getPeer().getPeerid()));
    }

    auto wptr = weakHandle();
    promise::when(promises)
    .then([wptr, newKeyId, plainKeyStr, newPlainKey, this]
    {
        if (wptr.deleted())
        {
            return;
        }

        auto keys = std::make_shared<std::map<Cid_t, std::string>>();

        for (const auto& session : mSessions) // encrypt key to all participants
        {
            // get peer Cid
            Cid_t sessionCid = session.first;
            const sfu::Peer& peer = session.second->getPeer();
            if (sfu::isInitialSfuVersion(peer.getPeerSfuVersion()))
            {
                // encrypt key to participant
                strongvelope::SendKey encryptedKey;
                mSfuClient.getRtcCryptoMeetings()->encryptKeyTo(peer.getPeerid(), *newPlainKey.get(), encryptedKey);
                (*keys)[sessionCid] = mega::Base64::btoa(std::string(encryptedKey.buf(), encryptedKey.size()));
            }
            else if (sfu::isCurrentSfuVersion(peer.getPeerSfuVersion()))
            {
                auto pms = peer.getEphemeralPubKeyPms();
                pms.then([this, newPlainKey, keys, sessionCid, &peer]()
                {
                    auto&& ephemeralPubKey = peer.getEphemeralPubKeyDerived();
                    if (ephemeralPubKey.empty())
                    {
                        RTCM_LOG_WARNING("Invalid ephemeral key for peer: %s cid %u", peer.getPeerid().toString().c_str(), sessionCid);
                        assert(false);
                        return;
                    }

                    // Encrypt key for participant with its public ephemeral key
                    std::string encryptedKey;
                    std::string plainKey (newPlainKey->buf(), newPlainKey->bufSize());
                    if (!mSymCipher.cbc_encrypt_with_key(plainKey, encryptedKey, reinterpret_cast<const unsigned char *>(ephemeralPubKey.data()), ephemeralPubKey.size(), nullptr))
                    {
                        RTCM_LOG_ERROR("Failed Media key cbc_encrypt for peerId %s Cid %u",
                                         peer.getPeerid().toString().c_str(), peer.getCid());
                        return;
                    }

                    (*keys)[sessionCid] = mega::Base64::btoa(encryptedKey);
                 });
                 pms.fail([peerId = peer.getPeerid(), peerCid = peer.getCid()](const ::promise::Error&)
                 {
                    RTCM_LOG_DEBUG("Can't get ephemeral public key for peer: %s cid: %u", karere::Id(peerId).toString().c_str(), peerCid);
                 });
            }
            else
            {
                RTCM_LOG_ERROR("generateAndSendNewMediakey: unknown SFU protocol version [%u] for user: %s, cid: %u",
                               static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer.getPeerSfuVersion()),
                               peer.getPeerid().toString().c_str(), peer.getCid());
                assert(false);
                return;
            }
        }

        mSfuConnection->sendKey(newKeyId, *keys);

        // set a small delay after broadcasting the new key, and before starting to use it,
        // to minimize the chance that the key hasn't yet been received over the signaling channel
        karere::setTimeout([this, newKeyId, plainKeyStr, wptr]()
        {
            if (wptr.deleted())
            {
                return;
            }

            // add key to peer's key map, although is not encrypted for any other participant,
            // as we need to start sending audio frames as soon as we receive SPEAK_ON command
            // and we could receive it even if there's no more participants in the meeting
            mMyPeer->addKey(newKeyId, plainKeyStr);
        }, RtcConstant::kRotateKeyUseDelay, mRtc.getAppCtx());

    });
}

void Call::handleIncomingVideo(const std::map<Cid_t, sfu::TrackDescriptor> &videotrackDescriptors, VideoResolution videoResolution)
{
    for (auto trackDescriptor : videotrackDescriptors)
    {
        auto it = mReceiverTracks.find(trackDescriptor.second.mMid);
        if (it == mReceiverTracks.end())
        {
            RTCM_LOG_ERROR("Unknown vtrack mid %u", trackDescriptor.second.mMid);
            continue;
        }

        Cid_t cid = trackDescriptor.first;
        uint32_t mid = trackDescriptor.second.mMid;
        RemoteVideoSlot *slot = static_cast<RemoteVideoSlot*>(it->second.get());
        if (slot->getCid() == cid && slot->getVideoResolution() == videoResolution)
        {
            RTCM_LOG_WARNING("Follow same cid with same resolution over same track");
            continue;
        }

        if (slot->getCid() != 0)    // the slot is already in use, need to release first and notify
        {
            if (trackDescriptor.second.mReuse && slot->getCid() != cid)
            {
                RTCM_LOG_ERROR("attachSlotToSession: trying to reuse slot, but cid has changed");
                assert(false && "Possible error at SFU: slot with CID not found");
            }

            RTCM_LOG_DEBUG("reassign slot with mid: %u from cid: %u to newcid: %u, reuse: %d ", mid, slot->getCid(), cid, trackDescriptor.second.mReuse);

            Session *oldSess = getSession(slot->getCid());
            if (oldSess)
            {
                // In case of Slot reassign for another peer (CID) or same peer (CID) slot reusing, we need to notify app about that
                oldSess->disableVideoSlot(slot->getVideoResolution());
            }
        }

        Session* sess = getSession(cid);
        if (!sess)
        {
            RTCM_LOG_ERROR("handleIncomingVideo: session with CID %u not found", cid);
            assert(false && "Possible error at SFU: session with CID not found");
            continue;
        }

        const std::vector<std::string> ivs = sess->getPeer().getIvs();
        slot->assignVideoSlot(cid, sfu::Command::hexToBinary(ivs[static_cast<size_t>(videoResolution)]), videoResolution);
        attachSlotToSession(cid, slot, false, videoResolution);
    }
}

void Call::attachSlotToSession (Cid_t cid, RemoteSlot* slot, bool audio, VideoResolution hiRes)
{
    Session *session = getSession(cid);
    assert(session);
    if (!session)
    {
        RTCM_LOG_WARNING("attachSlotToSession: unknown peer cid %u", cid);
        return;
    }

    if (audio)
    {
        session->setAudioSlot(static_cast<RemoteAudioSlot *>(slot));
    }
    else
    {
        if (hiRes)
        {
            session->setHiResSlot(static_cast<RemoteVideoSlot *>(slot));
        }
        else
        {
            session->setVThumSlot(static_cast<RemoteVideoSlot *>(slot));
        }
    }
}

void Call::addSpeaker(Cid_t cid, const sfu::TrackDescriptor &speaker)
{
    if (speaker.mMid == sfu::TrackDescriptor::invalidMid)
    {
        // peer notified as speaker from SFU, but track not provided yet (this happens if peer is muted)
        // TODO: check when we fully support raise-to-speak requests (to avoid sending an unnecessary speak request)
        return;
    }

    auto it = mReceiverTracks.find(speaker.mMid);
    if (it == mReceiverTracks.end())
    {
        RTCM_LOG_WARNING("AddSpeaker: unknown track mid %u", speaker.mMid);
        return;
    }

    RemoteAudioSlot* slot = static_cast<RemoteAudioSlot*>(it->second.get());
    if (slot->getCid() != cid)
    {
        Session *oldSess = getSession(slot->getCid());
        if (oldSess)
        {
            // In case of Slot reassign for another peer (CID) we need to notify app about that
            oldSess->disableAudioSlot();
        }
    }

    Session *sess = getSession(cid);
    if (!sess)
    {
        RTCM_LOG_WARNING("AddSpeaker: unknown cid");
        return;
    }

    if (!sess->hasSpeakPermission())
    {
        RTCM_LOG_ERROR("AddSpeaker: should not receive amid for peer without speak permission,"
                       " callid: %s, cid: %u", getCallid().toString().c_str(), cid);
        assert(false);
        return;
    }

    const std::vector<std::string> ivs = sess->getPeer().getIvs();
    assert(ivs.size() >= kAudioTrack);
    slot->assignAudioSlot(cid, sfu::Command::hexToBinary(ivs[static_cast<size_t>(kAudioTrack)]));
    attachSlotToSession(cid, slot, true, kUndefined);
}

void Call::removeSpeaker(Cid_t cid)
{
    Session* sess = getSession(cid);
    if (!sess)
    {
        RTCM_LOG_WARNING("removeSpeaker: unknown cid: %u", cid);
        return;
    }

    if (sess->getAudioSlot())
    {
        if (!sess->hasSpeakPermission())
        {
            RTCM_LOG_ERROR("removeSpeaker: trying to remove a speaker whose permission to speak was disabled"
                           " callid: %s, cid: %u", getCallid().toString().c_str(), cid);
            assert(false);
            return;
        }
        sess->disableAudioSlot();
    }
}

sfu::Peer& Call::getMyPeer()
{
    return *mMyPeer;
}

sfu::SfuClient &Call::getSfuClient()
{
    return mSfuClient;
}

std::map<Cid_t, std::unique_ptr<Session> > &Call::getSessions()
{
    return mSessions;
}

void Call::takeVideoDevice()
{
    if (!mVideoManager)
    {
        mRtc.takeDevice();
        mVideoManager = mRtc.getVideoDevice();
    }
}

void Call::releaseVideoDevice()
{
    if (mVideoManager)
    {
        mRtc.releaseDevice();
        mVideoManager = nullptr;
    }
}

bool Call::hasVideoDevice()
{
    return mVideoManager ? true : false;
}

void Call::freeVideoTracks(bool releaseSlots)
{
    // disable hi-res track
    if (mHiRes && mHiRes->getTransceiver()->sender()->track())
    {
        mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // disable low-res track
    if (mVThumb && mVThumb->getTransceiver()->sender()->track())
    {
        mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
    }

    if (releaseSlots) // release slots in case flag is true
    {
        mVThumb.reset();
        mHiRes.reset();
    }
}

void Call::freeAudioTrack(bool releaseSlot)
{
    // disable audio track
    if (mAudio && mAudio->getTransceiver()->sender()->track())
    {
        mAudio->getTransceiver()->sender()->SetTrack(nullptr);
    }

    if (releaseSlot) // release slot in case flag is true
    {
        mAudio.reset();
    }
}

void Call::collectNonRTCStats()
{
    int audioSession = 0;
    int vThumbSession = 0;
    int hiResSession = 0;
    for (const auto& session : mSessions)
    {
        if (session.second->getAudioSlot())
        {
            audioSession++;
        }

        if (session.second->getVthumSlot())
        {
            vThumbSession++;
        }

        if (session.second->getHiResSlot())
        {
            hiResSession++;
        }
    }

    // TODO: pending to implement disabledTxLayers in future if needed
    mStats.mSamples.mQ.push_back(static_cast<int32_t>(mSvcDriver.mCurrentSvcLayerIndex) | static_cast<int32_t>(kTxSpatialLayerCount) << 8);
    mStats.mSamples.mNrxa.push_back(audioSession);
    mStats.mSamples.mNrxl.push_back(vThumbSession);
    mStats.mSamples.mNrxh.push_back(hiResSession);
    mStats.mSamples.mAv.push_back(getLocalAvFlags().value());
}

void Call::initStatsValues()
{
    mStats.mPeerId = mMyPeer->getPeerid();
    mStats.mCallid = mCallid;
    mStats.mIsGroup = mIsGroup;
    mStats.mDevice = mRtc.getDeviceInfo();
    mStats.mSfuHost = mSfuConnection->getSfuUrl().host;
}

void Call::enableStats()
{
    mStats.mCid = getOwnCid();
    mStats.mTimeOffset = static_cast<uint64_t>(mOffset);
    auto wptr = weakHandle();
    mStatsTimer = karere::setInterval([this, wptr]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (!mSfuConnection || !mSfuConnection->isJoined())
        {
            RTCM_LOG_WARNING("Cannot collect stats until reach kJoined state");
            return;
        }

        // poll TxVideoStats
        assert(mVThumb && mHiRes);
        uint32_t hiResId = 0;
        if (mHiResActive)
        {
            hiResId = mHiRes->getTransceiver()->sender()->ssrc();
        }

        uint32_t lowResId = 0;
        if (mVThumbActive)
        {
            lowResId = mVThumb->getTransceiver()->sender()->ssrc();
        }

        // poll non-rtc stats
        collectNonRTCStats();

        // Keep mStats ownership
        mStatConnCallback = rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>(new ConnStatsCallBack(&mStats, hiResId, lowResId, mRtc.getAppCtx()));
        assert(mRtcConn);
        mRtcConn->GetStats(mStatConnCallback.get());

        // adjust SVC driver based on collected stats
        // TODO: I can be done in ConnStatsCallBack to take into account latest stats
        adjustSvcByStats();
    }, RtcConstant::kStatsInterval, mRtc.getAppCtx());
}

void Call::disableStats()
{
    if (mStatsTimer != 0)
    {
        karere::cancelInterval(mStatsTimer, mRtc.getAppCtx());
        mStatsTimer = 0;
        if (mStatConnCallback)
        {
            static_cast<ConnStatsCallBack*>(mStatConnCallback.get())->removeStats();
        }

        mStatConnCallback = nullptr;
    }
}

void Call::setDestroying(bool isDestroying)
{
    mIsDestroying = isDestroying;
}

bool Call::isDestroying()
{
    return mIsDestroying;
}

void Call::generateEphemeralKeyPair()
{
    mEphemeralKeyPair.reset(new mega::ECDH());
}

const mega::ECDH* Call::getMyEphemeralKeyPair() const
{
    return mEphemeralKeyPair.get();
}

void Call::muteMyClientFromSfu()
{
    if (!getLocalAvFlags().audio())
    {
        return;
    }

    karere::AvFlags currentFlags = getLocalAvFlags();
    currentFlags.remove(karere::AvFlags::kAudio);
    mMyPeer->setAvFlags(currentFlags);
    mCallHandler.onLocalFlagsChanged(*this);  // notify app local AvFlags Change
    updateAudioTracks();
}

void Call::addPeer(sfu::Peer& peer, const std::string& ephemeralPubKeyDerived)
{
    peer.setEphemeralPubKeyDerived(ephemeralPubKeyDerived);
    mSessions[peer.getCid()] = std::make_unique<Session>(peer);
    mCallHandler.onNewSession(*mSessions[peer.getCid()], *this);
}

std::pair<std::string, std::string> Call::splitPubKey(const std::string& keyStr) const
{
    auto pos = keyStr.find(":");
    if (pos == std::string::npos)
    {
        return std::make_pair(std::string(), std::string());
    }

    std::string pubkey = keyStr.substr(0, pos);
    std::string signature = keyStr.substr(pos + 1, keyStr.size());
    return std::make_pair(pubkey, signature);
}

promise::Promise<bool>
Call::verifySignature(const Cid_t cid, const uint64_t userid, const std::string& pubkey, const std::string& signature)
{
    promise::Promise<bool> pms;
    if (pubkey.empty() || signature.empty())
    {
        pms.resolve(true);
    }
    else
    {
        // verify received ephemeral public key signature for joined user
        std::string msg = "sesskey|" + mCallid.toString() + "|" + std::to_string(cid) + "|" + pubkey;
        pms = mSfuClient.getRtcCryptoMeetings()->verifyKeySignature(msg, signature, getChatid(), karere::Id(userid));
    }
    return pms;
}

void Call::updateVideoTracks()
{
    bool isOnHold = getLocalAvFlags().isOnHold();
    if (getLocalAvFlags().camera() && !isOnHold)
    {
        takeVideoDevice();

        // hi-res track
        if (mHiRes)
        {
            if (mHiResActive && !mHiRes->getTransceiver()->sender()->track())
            {
                rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getVideoDevice()->getVideoTrackSource());
                mHiRes->getTransceiver()->sender()->SetTrack(videoTrack.get());
            }
            else if (!mHiResActive)
            {
                // if there is a track, but none in the call has requested hi res video, disable the track
                mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
            }
        }

        // low-res track
        if (mVThumb)
        {
            if (mVThumbActive && !mVThumb->getTransceiver()->sender()->track())
            {
                rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getVideoDevice()->getVideoTrackSource());
                mVThumb->getTransceiver()->sender()->SetTrack(videoTrack.get());
            }
            else if (!mVThumbActive)
            {
                // if there is a track, but none in the call has requested low res video, disable the track
                mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
            }
        }
    }
    else    // no video from camera (muted or not available), or call on-hold
    {
        freeVideoTracks();
        releaseVideoDevice();
    }
}

void Call::updateNetworkQuality(int networkQuality)
{
    if (networkQuality == mNetworkQuality)
    {
        return;
    }

    RTCM_LOG_WARNING("updateNetworkQuality: %s network quality detected", networkQuality == kNetworkQualityBad  ? "Bad" : "Good");
    mNetworkQuality = networkQuality;
    mCallHandler.onNetworkQualityChanged(*this);
}

void Call::adjustSvcByStats()
{
    if (mStats.mSamples.mRoundTripTime.empty())
    {
        RTCM_LOG_WARNING("adjustSvcBystats: not enough collected data");
        return;
    }

    double roundTripTime = mStats.mSamples.mRoundTripTime.back();
    double packetLost = 0;
    if (mStats.mSamples.mPacketLost.size() >= 2)
    {
        // get last lost packets
        int lastpl =  mStats.mSamples.mPacketLost.back();
        // use mPacketLostCapping to limit the influence of a large momentary peak on the moving average.
        lastpl = lastpl < mSvcDriver.mPacketLostCapping ? lastpl : static_cast<int>(mSvcDriver.mPacketLostCapping);

        // get (pre) last lost packets
        int prelastpl= mStats.mSamples.mPacketLost.at(mStats.mSamples.mPacketLost.size()-2);
        // use mPacketLostCapping to limit the influence of a large momentary peak on the moving average.
        prelastpl = prelastpl < mSvcDriver.mPacketLostCapping ? prelastpl : static_cast<int>(mSvcDriver.mPacketLostCapping);

        // get periods
        int lastT = mStats.mSamples.mT.back();
        int prelastT = mStats.mSamples.mT.at(mStats.mSamples.mT.size() - 2);
        packetLost = static_cast<double>(abs(lastpl - prelastpl)) / (static_cast<double>(abs(lastT - prelastT)) / 1000.0);
    }

    if (std::fabs(mSvcDriver.mMovingAverageRtt) <= std::numeric_limits<double>::epsilon())
    {
         // if mMovingAverageRtt has not value yet
         mSvcDriver.mMovingAverageRtt = roundTripTime;
         mSvcDriver.mMovingAveragePlost = packetLost;
         return; // intentionally skip first sample for lower/upper range calculation
    }

    if (roundTripTime < mSvcDriver.mLowestRttSeen)
    {
        // rttLower and rttUpper define the window inside which layer is not switched.
        //  - if rtt falls below that window, layer is switched to higher quality,
        //  - if rtt is higher, layer is switched to lower quality.
        // the window is defined/redefined relative to the lowest rtt seen.
        mSvcDriver.mLowestRttSeen = roundTripTime;
        mSvcDriver.mRttLower = roundTripTime + mSvcDriver.kRttLowerHeadroom;
        mSvcDriver.mRttUpper = roundTripTime + mSvcDriver.kRttUpperHeadroom;
    }

    roundTripTime = mSvcDriver.mMovingAverageRtt = (mSvcDriver.mMovingAverageRtt * 3 + roundTripTime) / 4;
    packetLost  = mSvcDriver.mMovingAveragePlost = (mSvcDriver.mMovingAveragePlost * 3 + packetLost) / 4;

    time_t tsNow = time(nullptr);
    if (mSvcDriver.mTsLastSwitch
            && (tsNow - mSvcDriver.mTsLastSwitch < mSvcDriver.kMinTimeBetweenSwitches))
    {
        return; // too early
    }

    if (mSvcDriver.mCurrentSvcLayerIndex > 0
            && (roundTripTime > mSvcDriver.mRttUpper || packetLost > mSvcDriver.mPacketLostUpper))
    {
        // if retrieved rtt OR packetLost have increased respect current values decrement 1 layer
        // we want to decrease layer when references values (mRttUpper and mPacketLostUpper)
        // have been exceeded.
        updateSvcQuality(-1);
    }
    else if (mSvcDriver.mCurrentSvcLayerIndex < mSvcDriver.kMaxQualityIndex
             && roundTripTime < mSvcDriver.mRttLower
             && packetLost < mSvcDriver.mPacketLostLower)
    {
        // if retrieved rtt AND packetLost have decreased respect current values increment 1 layer
        // we only want to increase layer when the improvement is bigger enough to represents a
        // faithfully improvement in network quality, we take mRttLower and mPacketLostLower as references
        updateSvcQuality(+1);
    }

    if (mStats.mSamples.mVtxHiResh.size() < 2
            || mStats.mSamples.mPacketSent.size() < 2
            || mStats.mSamples.mTotalPacketSendDelay.size() < 2
            || mStats.mSamples.mVtxHiResh.back() == 0
            || mStats.mSamples.mVtxHiResh.at(mStats.mSamples.mVtxHiResh.size() -2) == 0)
    {
        // notify about a change in network quality if received quality is very low
        mSvcDriver.mCurrentSvcLayerIndex < 1
                ? updateNetworkQuality(kNetworkQualityBad)
                : updateNetworkQuality(kNetworkQualityGood);
        return;
    }

    mSvcDriver.mMovingAverageVideoTxHeight = mSvcDriver.mMovingAverageVideoTxHeight > 0
            ? ((mSvcDriver.mMovingAverageVideoTxHeight * 3) + static_cast<double>(mStats.mSamples.mVtxHiResh.back())) / 4
            : mStats.mSamples.mVtxHiResh.back();

    bool txBad = mSvcDriver.mMovingAverageVideoTxHeight < 360;
    uint32_t pktSent =  mStats.mSamples.mPacketSent.back() - mStats.mSamples.mPacketSent.at(mStats.mSamples.mPacketSent.size() - 2);
    double totalPacketSendDelay = mStats.mSamples.mTotalPacketSendDelay.back() - mStats.mSamples.mTotalPacketSendDelay.at(mStats.mSamples.mTotalPacketSendDelay.size() - 2);
    double vtxDelay = pktSent ? round(totalPacketSendDelay * 1000 / pktSent) : -1;

    // notify about a change in network quality if necessary
    (txBad || mSvcDriver.mCurrentSvcLayerIndex < 1 || roundTripTime > mSvcDriver.mRttUpper || vtxDelay > 1500)
            ? updateNetworkQuality(kNetworkQualityBad)
            : updateNetworkQuality(kNetworkQualityGood);
}

const std::string& Call::getCallKey() const
{
    return mCallKey;
}

void Call::updateAudioTracks()
{
    if (!mAudio)
    {
        return;
    }

    bool audio = mSpeakerState > SpeakerState::kNoSpeaker && getLocalAvFlags().audio();
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = mAudio->getTransceiver()->sender()->track();
    if (audio && !getLocalAvFlags().isOnHold())
    {
        if (!track) // create audio track only if not exists
        {
            rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack =
                    artc::gWebrtcContext->CreateAudioTrack("a"+std::to_string(artc::generateId()), artc::gWebrtcContext->CreateAudioSource(cricket::AudioOptions()).get());

            mAudio->getTransceiver()->sender()->SetTrack(audioTrack.get());
            audioTrack->set_enabled(true);
        }
        else
        {
            track->set_enabled(true);
        }
    }
    else if (track) // if no audio flags active, no speaker allowed, or call is onHold
    {
        track->set_enabled(false);
        mAudio->getTransceiver()->sender()->SetTrack(nullptr);
    }
}

RtcModuleSfu::RtcModuleSfu(MyMegaApi &megaApi, CallHandler &callhandler, DNScache &dnsCache,
                           WebsocketsIO& websocketIO, void *appCtx,
                           rtcModule::RtcCryptoMeetings* rRtcCryptoMeetings)
    : VideoSink(appCtx)
    , mCallHandler(callhandler)
    , mMegaApi(megaApi)
    , mDnsCache(dnsCache)
{
    mAppCtx = appCtx;

    mSfuClient = ::mega::make_unique<sfu::SfuClient>(websocketIO, appCtx, rRtcCryptoMeetings);
    if (!artc::isInitialized())
    {
        //rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
        artc::init(appCtx);
        RTCM_LOG_DEBUG("WebRTC stack initialized before first use");
    }

    // set default video in device
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
    if (videoDevices.size())
    {
        mVideoDeviceSelected = videoDevices.begin()->second;
    }

    mDeviceTakenCount = 0;
}

ICall *RtcModuleSfu::findCall(const karere::Id& callid) const
{
    auto it = mCalls.find(callid);
    if (it != mCalls.end())
    {
        return it->second.get();
    }

    return nullptr;
}

ICall *RtcModuleSfu::findCallByChatid(const karere::Id &chatid) const
{
    for (const auto& call : mCalls)
    {
        if (call.second->getChatid() == chatid)
        {
            return call.second.get();
        }
    }

    return nullptr;
}

bool RtcModuleSfu::isCallStartInProgress(const karere::Id &chatid) const
{
    return mCallStartAttempts.find(chatid) != mCallStartAttempts.end();
}

bool RtcModuleSfu::selectVideoInDevice(const std::string &device)
{
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
    bool shouldOpen = false;
    for (auto it = videoDevices.begin(); it != videoDevices.end(); it++)
    {
        if (!it->first.compare(device))
        {
            std::vector<Call*> calls;
            for (auto& callIt : mCalls)
            {
                if (callIt.second->hasVideoDevice())
                {
                    calls.push_back(callIt.second.get());
                    callIt.second->freeVideoTracks();
                    callIt.second->releaseVideoDevice();
                    shouldOpen = true;
                }
            }

            changeDevice(it->second, shouldOpen);

            for (auto& call : calls)
            {
                call->updateVideoTracks();
            }

            return true;
        }
    }
    return false;
}

void RtcModuleSfu::getVideoInDevices(std::set<std::string> &devicesVector)
{
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
    for (auto it = videoDevices.begin(); it != videoDevices.end(); it++)
    {
        devicesVector.insert(it->first);
    }
}

promise::Promise<void> RtcModuleSfu::startCall(const karere::Id &chatid, karere::AvFlags avFlags, bool isGroup, const karere::Id &schedId, std::shared_ptr<std::string> unifiedKey)
{
    if (!isValidInputVideoTracksLimit(mRtcNumInputVideoTracks))
    {
        const std::string errMsg = "startCall: Invalid value for simultaneous input video tracks";
        RTCM_LOG_WARNING("%s", errMsg.c_str());
        assert(false);
        return promise::Error(errMsg);
    }

    // add chatid to CallsAttempts to avoid multiple start call attempts
    mCallStartAttempts.insert(chatid);

    // we need a temp string to avoid issues with lambda shared pointer capture
    std::string auxCallKey = unifiedKey ? (*unifiedKey.get()) : std::string();
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::startChatCall, chatid, schedId)
    .then([wptr, this, chatid, avFlags, isGroup, auxCallKey](ReqResult result) -> promise::Promise<void>
    {
        if (wptr.deleted())
        {
            return promise::Error("call started successfully, but RtcModuleSfu instance was removed");
        }

        std::shared_ptr<std::string> sharedUnifiedKey = !auxCallKey.empty()
                ? std::make_shared<std::string>(auxCallKey)
                : nullptr;

        karere::Id callid = result->getParentHandle();
        std::string sfuUrlStr = result->getText();
        mCallStartAttempts.erase(chatid); // remove chatid from CallsAttempts
        if (mCalls.find(callid) == mCalls.end()) // it can be created by JOINEDCALL command
        {
            std::unique_ptr<char []> userHandle(mMegaApi.sdk.getMyUserHandle());
            karere::Id myUserHandle(userHandle.get());
            mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, myUserHandle, false, mCallHandler, mMegaApi, (*this), isGroup, sharedUnifiedKey, avFlags, true);

            if (!mCalls[callid]->connectSfu(sfuUrlStr))
            {
               return promise::Error("connectSfu error, invalid or empty URL");
            }
        }
        return promise::_Void();
    })
    .fail([wptr, this, chatid](const ::promise::Error& err)
    {
        if (wptr.deleted())
        {
            return err;
        }

        mCallStartAttempts.erase(chatid); // remove chatid from CallsAttempts
        return err;
    });
}

void RtcModuleSfu::takeDevice()
{
    if (!mDeviceTakenCount)
    {
        openDevice();
    }

    mDeviceTakenCount++;
}

void RtcModuleSfu::releaseDevice()
{
    if (mDeviceTakenCount > 0)
    {
        mDeviceTakenCount--;
        if (mDeviceTakenCount == 0)
        {
            assert(mVideoDevice);
            closeDevice();
        }
    }
}

void RtcModuleSfu::addLocalVideoRenderer(const karere::Id &chatid, IVideoRenderer *videoRederer)
{
    mRenderers[chatid] = std::unique_ptr<IVideoRenderer>(videoRederer);
}

void RtcModuleSfu::removeLocalVideoRenderer(const karere::Id &chatid)
{
    mRenderers.erase(chatid);
}

void RtcModuleSfu::onMediaKeyDecryptionFailed(const std::string& err)
{
    mMegaApi.callIgnoreResult(&::mega::MegaApi::sendEvent, 99017, err.c_str());
}

std::vector<karere::Id> RtcModuleSfu::chatsWithCall()
{
    std::vector<karere::Id> chats;
    for (const auto& call : mCalls)
    {
        chats.push_back(call.second->getChatid());
    }

    return chats;
}

unsigned int RtcModuleSfu::getNumCalls()
{
    return static_cast<unsigned int>(mCalls.size());
}

const std::string& RtcModuleSfu::getVideoDeviceSelected() const
{
    return mVideoDeviceSelected;
}

sfu::SfuClient& RtcModuleSfu::getSfuClient()
{
    return (*mSfuClient.get());
}

DNScache& RtcModuleSfu::getDnsCache()
{
    return mDnsCache;
}


void RtcModuleSfu::orderedDisconnectAndCallRemove(rtcModule::ICall* iCall, EndCallReason reason, TermCode connectionTermCode)
{
    Call *call = static_cast<Call*>(iCall);
    if (!call)
    {
        RTCM_LOG_WARNING("orderedDisconnectAndCallRemove: call no longer exists");
        return;
    }

    if (call->isDestroying())
    {
        RTCM_LOG_WARNING("orderedDisconnectAndCallRemove: call is already being destroyed");
        return;
    }

    // set temporary endCall reason in case immediateRemoveCall is not called immediately (i.e if we first need to send BYE command)
    call->setTempEndCallReason(reason);

    RTCM_LOG_DEBUG("Ordered removing call with callid: %s", call->getCallid().toString().c_str());
    call->setDestroying(true);
    (call->getState() > kStateClientNoParticipating && call->getState() <= kStateInProgress)
            ? call->orderedCallDisconnect(connectionTermCode, call->connectionTermCodeToString(connectionTermCode).c_str())
            : immediateRemoveCall(call, reason, connectionTermCode);
}


void RtcModuleSfu::immediateRemoveCall(Call* call, uint8_t reason, TermCode connectionTermCode)
{
    assert(reason != kInvalidReason);
    if (!call)
    {
        RTCM_LOG_WARNING("removeCall: call no longer exists");
        return;
    }

    RTCM_LOG_DEBUG("Removing call with callid: %s", call->getCallid().toString().c_str());
    if (call->getState() > kStateClientNoParticipating && call->getState() <= kStateInProgress)
    {
        call->immediateCallDisconnect(connectionTermCode);
    }

    // upon kStateDestroyed state change (in call dtor) mEndCallReason will be notified through onCallStateChange
    call->setEndCallReason(reason);
    mCalls.erase(call->getCallid());
}

void RtcModuleSfu::handleJoinedCall(const karere::Id &/*chatid*/, const karere::Id &callid, const std::set<karere::Id> &usersJoined)
{
    mCalls[callid]->joinedCallUpdateParticipants(usersJoined);
}

void RtcModuleSfu::handleLeftCall(const karere::Id &/*chatid*/, const karere::Id &callid, const std::set<karere::Id> &usersLeft)
{
    for (const karere::Id &peer : usersLeft)
    {
        mCalls[callid]->removeParticipant(peer);
    }
}

void RtcModuleSfu::handleNewCall(const karere::Id &chatid, const karere::Id &callerid, const karere::Id &callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey)
{
    mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, callerid, isRinging, mCallHandler, mMegaApi, (*this), isGroup, callKey);
    mCalls[callid]->setState(kStateClientNoParticipating);
}

bool KarereWaitingRoom::updateUsers(const std::set<karere::Id>& users, const WrState& status)
{
    if (!isValidWrStatus(status) || users.empty())
    {
        return false;
    }

    std::for_each(users.begin(), users.end(), [this, &status](const auto &u)
                  {
                      mWaitingRoomUsers[u.val] = status;
                  });

    return true;
}

int KarereWaitingRoom::getPeerStatus(const uint64_t& peerid) const
{
    const auto& it = mWaitingRoomUsers.find(peerid);
    if (it == mWaitingRoomUsers.end())
    {
        return static_cast<int>(WrState::WR_UNKNOWN);
    }

    return static_cast<int>(it->second);
}

std::vector<uint64_t> KarereWaitingRoom::getPeers() const
{
    std::vector<uint64_t> keys;
    keys.reserve(mWaitingRoomUsers.size());
    std::transform(mWaitingRoomUsers.begin(), mWaitingRoomUsers.end(),
                   std::back_inserter(keys), [](const auto& pair) { return pair.first; });

    return keys;
}

void RtcModuleSfu::OnFrame(const webrtc::VideoFrame &frame)
{
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this, frame]()
    {
        if (wptr.deleted())
        {
            return;
        }

        for (auto& render : mRenderers)
        {
            ICall* call = findCallByChatid(render.first);
            if ((call && call->getLocalAvFlags().camera() && !call->getLocalAvFlags().has(karere::AvFlags::kOnHold)) || !call)
            {
                assert(render.second != nullptr);
                void* userData = NULL;
                auto buffer = frame.video_frame_buffer()->ToI420();   // smart ptr type changed
                if (frame.rotation() != webrtc::kVideoRotation_0)
                {
                    buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
                }
                unsigned short width = (unsigned short)buffer->width();
                unsigned short height = (unsigned short)buffer->height();
                void* frameBuf = render.second->getImageBuffer(width, height, userData);
                if (!frameBuf) //image is frozen or app is minimized/covered
                    return;
                libyuv::I420ToABGR(buffer->DataY(), buffer->StrideY(),
                                   buffer->DataU(), buffer->StrideU(),
                                   buffer->DataV(), buffer->StrideV(),
                                   (uint8_t*)frameBuf, width * 4, width, height);

                render.second->frameComplete(userData);
            }
        }
    }, mAppCtx);

}

artc::VideoManager *RtcModuleSfu::getVideoDevice()
{
    return mVideoDevice.get();
}

void RtcModuleSfu::changeDevice(const std::string &device, bool shouldOpen)
{
    if (mVideoDevice)
    {
        shouldOpen = true;
        closeDevice();
    }

    mVideoDeviceSelected = device;
    if (shouldOpen)
    {
        openDevice();
    }
}

void RtcModuleSfu::openDevice()
{
    std::string videoDevice = mVideoDeviceSelected; // get default video device
    if (videoDevice.empty())
    {
        RTCM_LOG_WARNING("Default video in device is not set");
        assert(false);
        std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
        if (videoDevices.empty())
        {
            RTCM_LOG_WARNING("openDevice(): no video devices available");
            return;
        }

        videoDevice = videoDevices.begin()->second;
    }

    webrtc::VideoCaptureCapability capabilities;
    capabilities.width = RtcConstant::kHiResWidth;
    capabilities.height = RtcConstant::kHiResHeight;
    capabilities.maxFPS = RtcConstant::kHiResMaxFPS;

    mVideoDevice = artc::VideoManager::Create(capabilities, videoDevice, artc::gWorkerThread.get());
    mVideoDevice->openDevice(videoDevice);
    rtc::VideoSinkWants wants;
    mVideoDevice->AddOrUpdateSink(this, wants);
}

void RtcModuleSfu::closeDevice()
{
    if (mVideoDevice)
    {
        mVideoDevice->RemoveSink(this);
        mVideoDevice->releaseDevice();
        mVideoDevice = nullptr;
    }
}

void *RtcModuleSfu::getAppCtx()
{
    return mAppCtx;
}

std::string RtcModuleSfu::getDeviceInfo() const
{
    // UserAgent Format
    // MEGA<app>/<version> (platform) Megaclient/<version>
    std::string userAgent = mMegaApi.sdk.getUserAgent();

    std::string androidId = "MEGAAndroid";
    std::string iosId = "MEGAiOS";
    std::string testChatId = "MEGAChatTest";
    std::string syncId = "MEGAsync";
    std::string qtAppId = "MEGAChatQtApp";
    std::string megaClcId = "MEGAclc";

    std::string deviceType = "n";
    std::string version = "0";

    size_t endTypePosition = std::string::npos;
    size_t idPosition;
    if ((idPosition = userAgent.find(androidId)) != std::string::npos)
    {
        deviceType = "na";
        endTypePosition = idPosition + androidId.size() + 1; // remove '/'
    }
    else if ((idPosition = userAgent.find(iosId)) != std::string::npos)
    {
        deviceType = "ni";
        endTypePosition = idPosition + iosId.size() + 1;  // remove '/'
    }
    else if (userAgent.find(testChatId) != std::string::npos)
    {
        deviceType = "nct";
    }
    else if ((idPosition = userAgent.find(syncId)) != std::string::npos)
    {
        deviceType = "nsync";
        endTypePosition = idPosition + syncId.size() + 1;  // remove '/'
    }
    else if (userAgent.find(qtAppId) != std::string::npos)
    {
        deviceType = "nqtApp";
    }
    else if (userAgent.find(megaClcId) != std::string::npos)
    {
        deviceType = "nclc";
    }

    size_t endVersionPosition = userAgent.find(" (");
    if (endVersionPosition != std::string::npos &&
            endTypePosition != std::string::npos &&
            endVersionPosition > endTypePosition)
    {
        version = userAgent.substr(endTypePosition, endVersionPosition - endTypePosition);
    }

    return deviceType + ":" + version;
}

unsigned int RtcModuleSfu::getNumInputVideoTracks() const
{
    return mRtcNumInputVideoTracks;
}

void RtcModuleSfu::setNumInputVideoTracks(const unsigned int numInputVideoTracks)
{
    if (!isValidInputVideoTracksLimit(mRtcNumInputVideoTracks))
    {
        RTCM_LOG_WARNING("setNumInputVideoTracks: Invalid value for simultaneous input video tracks");
        assert(false);
        return;
    }
    mRtcNumInputVideoTracks = numInputVideoTracks;
}

RtcModule* createRtcModule(MyMegaApi &megaApi, rtcModule::CallHandler &callHandler,
                           DNScache &dnsCache, WebsocketsIO& websocketIO, void *appCtx,
                           rtcModule::RtcCryptoMeetings* rRtcCryptoMeetings)
{
    return new RtcModuleSfu(megaApi, callHandler, dnsCache, websocketIO, appCtx,
                            rRtcCryptoMeetings);
}

Slot::Slot(Call &call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    : mCall(call)
    , mTransceiver(transceiver)
{
}

Slot::~Slot()
{
    if (mTransceiver->receiver())
    {
       rtc::scoped_refptr<webrtc::FrameDecryptorInterface> decryptor = mTransceiver->receiver()->GetFrameDecryptor();
       if (decryptor)
       {
           static_cast<artc::MegaDecryptor*>(decryptor.get())->setTerminating();
       }
    }

    if (mTransceiver->sender())
    {
        rtc::scoped_refptr<webrtc::FrameEncryptorInterface> encryptor = mTransceiver->sender()->GetFrameEncryptor();
        if (encryptor)
        {
            static_cast<artc::MegaEncryptor*>(encryptor.get())->setTerminating();
        }
    }
}

uint32_t Slot::getTransceiverMid() const
{
    if (!mTransceiver->mid())
    {
        assert(false);
        RTCM_LOG_WARNING("We have received a transceiver without 'mid'");
        return 0;
    }

    return static_cast<uint32_t>(atoi(mTransceiver->mid()->c_str()));
}

void RemoteSlot::release()
{
    if (!mCid)
    {
        return;
    }

    mIv = 0;
    mCid = 0;

    enableTrack(false, kRecv);
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> decryptor = getTransceiver()->receiver()->GetFrameDecryptor();
    static_cast<artc::MegaDecryptor*>(decryptor.get())->setTerminating();
    getTransceiver()->receiver()->SetFrameDecryptor(nullptr);
}

void RemoteSlot::assign(Cid_t cid, IvStatic_t iv)
{
    assert(!mCid);
    createDecryptor(cid, iv);
    enableTrack(true, kRecv);
}

void RemoteSlot::createDecryptor(Cid_t cid, IvStatic_t iv)
{
    mCid = cid;
    mIv = iv;

    auto it = mCall.getSessions().find(mCid);
    if (it == mCall.getSessions().end())
    {
        mCall.logError("createDecryptor: unknown cid");
        return;
    }

    mTransceiver->receiver()->SetFrameDecryptor(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>(new artc::MegaDecryptor(it->second->getPeer(),
                                                                      mCall.getSfuClient().getRtcCryptoMeetings(),
                                                                      mIv, getTransceiverMid())));
}

RemoteSlot::RemoteSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx)
    : Slot(call, transceiver), mAppCtx(appCtx)
{
}

void RemoteSlot::enableTrack(bool enable, TrackDirection direction)
{
    assert(mTransceiver);
    if (direction == kRecv)
    {
        mTransceiver->receiver()->track()->set_enabled(enable);
    }
    else if (direction == kSend)
    {
        mTransceiver->sender()->track()->set_enabled(enable);
    }
}

LocalSlot::LocalSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    : Slot(call, transceiver)
{
}

void LocalSlot::createEncryptor()
{
    mTransceiver->sender()->SetFrameEncryptor(rtc::scoped_refptr<webrtc::FrameEncryptorInterface>(new artc::MegaEncryptor(mCall.getMyPeer(),
                                                                      mCall.getSfuClient().getRtcCryptoMeetings(),
                                                                      mIv, getTransceiverMid())));
}

void LocalSlot::generateRandomIv()
{
    randombytes_buf(&mIv, sizeof(mIv));
}

RemoteVideoSlot::RemoteVideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx)
    : RemoteSlot(call, transceiver, appCtx)
    , VideoSink(appCtx)
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());

    assert(videoTrack);
    rtc::VideoSinkWants wants;
    videoTrack->AddOrUpdateSink(this, wants);
}

RemoteVideoSlot::~RemoteVideoSlot()
{
}

VideoSink::VideoSink(void* appCtx) :mAppCtx(appCtx)
{

}

VideoSink::~VideoSink()
{

}

void VideoSink::setVideoRender(IVideoRenderer *videoRenderer)
{
    mRenderer = std::unique_ptr<IVideoRenderer>(videoRenderer);
}

void VideoSink::OnFrame(const webrtc::VideoFrame &frame)
{
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this, frame]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (mRenderer)
        {
            void* userData = NULL;
            auto buffer = frame.video_frame_buffer()->ToI420();   // smart ptr type changed
            if (frame.rotation() != webrtc::kVideoRotation_0)
            {
                buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
            }
            unsigned short width = (unsigned short)buffer->width();
            unsigned short height = (unsigned short)buffer->height();
            void* frameBuf = mRenderer->getImageBuffer(width, height, userData);
            if (!frameBuf) //image is frozen or app is minimized/covered
                return;
            libyuv::I420ToABGR(buffer->DataY(), buffer->StrideY(),
                               buffer->DataU(), buffer->StrideU(),
                               buffer->DataV(), buffer->StrideV(),
                               (uint8_t*)frameBuf, width * 4, width, height);
            mRenderer->frameComplete(userData);
        }
    }, mAppCtx);
}

void RemoteVideoSlot::assignVideoSlot(Cid_t cid, IvStatic_t iv, VideoResolution videoResolution)
{
    assert(mVideoResolution == kUndefined);
    assign(cid, iv);
    mVideoResolution = videoResolution;
}

void RemoteVideoSlot::release()
{
    RemoteSlot::release();
    mVideoResolution = VideoResolution::kUndefined;
}

VideoResolution RemoteVideoSlot::getVideoResolution() const
{
    return mVideoResolution;
}

bool RemoteVideoSlot::hasTrack()
{
    assert(mTransceiver);

    if (mTransceiver->receiver())
    {
        return  mTransceiver->receiver()->track().get() != nullptr;
    }

    return false;

}

void RemoteVideoSlot::enableTrack()
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());
    videoTrack->set_enabled(true);
}

RemoteAudioSlot::RemoteAudioSlot(Call &call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver, void* appCtx)
    : RemoteSlot(call, transceiver, appCtx)
{
}

void RemoteAudioSlot::assignAudioSlot(Cid_t cid, IvStatic_t iv)
{
    assign(cid, iv);
    if (mCall.isAudioLevelMonitorEnabled())
    {
        enableAudioMonitor(true);   // Enable audio monitor
    }
}

void RemoteAudioSlot::enableAudioMonitor(bool enable)
{
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> mediaTrack = mTransceiver->receiver()->track();
    webrtc::AudioTrackInterface* audioTrack = static_cast<webrtc::AudioTrackInterface*>(mediaTrack.get());
    if (!audioTrack)
    {
        RTCM_LOG_WARNING("enableAudioMonitor: non valid audiotrack");
        assert(false);
        return;
    }

    if (enable && !mAudioLevelMonitorEnabled)
    {
        mAudioLevelMonitorEnabled = true;
        mAudioLevelMonitor->onAudioDetected(false);
        audioTrack->AddSink(mAudioLevelMonitor.get());     // enable AudioLevelMonitor for remote audio detection
    }
    else if (!enable && mAudioLevelMonitorEnabled)
    {
        mAudioLevelMonitorEnabled = false;
        mAudioLevelMonitor->onAudioDetected(false);
        audioTrack->RemoveSink(mAudioLevelMonitor.get()); // disable AudioLevelMonitor
    }
}

void RemoteAudioSlot::createDecryptor(Cid_t cid, IvStatic_t iv)
{
    RemoteSlot::createDecryptor(cid, iv);
    mAudioLevelMonitor.reset(new AudioLevelMonitor(mCall, mAppCtx, static_cast<int32_t>(mCid)));
}

void RemoteAudioSlot::release()
{
    RemoteSlot::release();
    if (mAudioLevelMonitor)
    {
        enableAudioMonitor(false);
        mAudioLevelMonitor = nullptr;
        mAudioLevelMonitorEnabled = false;
    }
}

void globalCleanup()
{
    if (!artc::isInitialized())
        return;
    artc::cleanup();
}

Session::Session(const sfu::Peer& peer)
    : mPeer(peer)
{

}

Session::~Session()
{
    disableAudioSlot();
    disableVideoSlot(kHiRes);
    disableVideoSlot(kLowRes);
    mState = kSessStateDestroyed;
    mSessionHandler->onDestroySession(*this);
}

TermCode Session::getTermcode() const
{
    return mTermCode;
}

void Session::setTermcode(TermCode termcode)
{
    mTermCode = termcode;
}

void Session::setSessionHandler(SessionHandler* sessionHandler)
{
    mSessionHandler = std::unique_ptr<SessionHandler>(sessionHandler);
}

void Session::setVideoRendererVthumb(IVideoRenderer *videoRenderer)
{
    if (!mVthumSlot)
    {
        RTCM_LOG_WARNING("setVideoRendererVthumb: There's no low-res slot associated to this session");
        return;
    }

    mVthumSlot->setVideoRender(videoRenderer);
}

void Session::setVideoRendererHiRes(IVideoRenderer *videoRenderer)
{
    if (!mHiresSlot)
    {
        RTCM_LOG_WARNING("setVideoRendererHiRes: There's no hi-res slot associated to this session");
        return;
    }

    mHiresSlot->setVideoRender(videoRenderer);
}

void Session::setAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    mSessionHandler->onRemoteAudioDetected(*this);
}

bool Session::hasHighResolutionTrack() const
{
    return mHiresSlot && mHiresSlot->hasTrack();
}

bool Session::hasLowResolutionTrack() const
{
    return mVthumSlot && mVthumSlot->hasTrack();
}

bool Session::isModerator() const
{
    return mPeer.isModerator();
}

void Session::notifyHiResReceived()
{
    mSessionHandler->onHiResReceived(*this);
}

void Session::notifyLowResReceived()
{
    mSessionHandler->onVThumbReceived(*this);
}

const sfu::Peer& Session::getPeer() const
{
    return mPeer;
}

void Session::setVThumSlot(RemoteVideoSlot *slot)
{
    assert(slot);
    mVthumSlot = slot;
    mSessionHandler->onVThumbReceived(*this);
}

void Session::setHiResSlot(RemoteVideoSlot *slot)
{
    assert(slot);
    mHiresSlot = slot;
    mSessionHandler->onHiResReceived(*this);
}

void Session::setAudioSlot(RemoteAudioSlot *slot)
{
    mAudioSlot = slot;
    setSpeakRequested(false);
}

void Session::addKey(Keyid_t keyid, const std::string &key)
{
    mPeer.addKey(keyid, key);
}

void Session::setAvFlags(karere::AvFlags flags)
{
    assert(mSessionHandler);
    if (flags == mPeer.getAvFlags())
    {
        RTCM_LOG_WARNING("setAvFlags: remote AV flags has not changed");
        return;
    }

    bool onHoldChanged = mPeer.getAvFlags().isOnHold() != flags.isOnHold();
    mPeer.setAvFlags(flags);
    onHoldChanged
        ? mSessionHandler->onOnHold(*this)              // notify session onHold Change
        : mSessionHandler->onRemoteFlagsChanged(*this); // notify remote AvFlags Change
}

void Session::setSpeakPermission(const bool hasSpeakPermission)
{
    if (hasSpeakPermission == mPeer.hasSpeakPermission())
    {
        RTCM_LOG_WARNING("setSpeakPermission: peer with Cid %d already had speakPermission: %s"
                         , mPeer.getCid()
                         , hasSpeakPermission ? "enabled" : "disabled");
        return;
    }
    mPeer.setSpeakPermission(hasSpeakPermission);
}

bool Session::hasSpeakPermission() const
{
    return mPeer.hasSpeakPermission();
}

RemoteAudioSlot *Session::getAudioSlot()
{
    return mAudioSlot;
}

RemoteVideoSlot *Session::getVthumSlot()
{
    return mVthumSlot;
}

RemoteVideoSlot *Session::getHiResSlot()
{
    return mHiresSlot;
}

void Session::disableAudioSlot()
{
    if (mAudioSlot)
    {
        mAudioSlot->release();
        setAudioSlot(nullptr);
    }
}

void Session::disableVideoSlot(VideoResolution videoResolution)
{
    if ((videoResolution == kHiRes && !mHiresSlot) || (videoResolution == kLowRes && !mVthumSlot))
    {
        return;
    }

    if (videoResolution == kHiRes)
    {
        mHiresSlot->release();
        mHiresSlot = nullptr;
        mSessionHandler->onHiResReceived(*this);
    }
    else
    {
        mVthumSlot->release();
        mVthumSlot = nullptr;
        mSessionHandler->onVThumbReceived(*this);
    }
}

void Session::setModerator(bool isModerator)
{
    mPeer.setModerator(isModerator);
    mSessionHandler->onPermissionsChanged(*this);
}

void Session::setSpeakRequested(bool requested)
{
    mHasRequestSpeak = requested;
    mSessionHandler->onAudioRequested(*this);
}

const karere::Id& Session::getPeerid() const
{
    return mPeer.getPeerid();
}

Cid_t Session::getClientid() const
{
    return mPeer.getCid();
}

SessionState Session::getState() const
{
    return mState;
}

karere::AvFlags Session::getAvFlags() const
{
    return mPeer.getAvFlags();
}

bool Session::isAudioDetected() const
{
    return mAudioDetected;
}

bool Session::hasRequestSpeak() const
{
    return mHasRequestSpeak;
}

AudioLevelMonitor::AudioLevelMonitor(Call &call, void* appCtx, int32_t cid)
    : mCall(call), mCid(cid), mAppCtx(appCtx)
{
}

void AudioLevelMonitor::OnData(const void *audio_data, int bits_per_sample, int /*sample_rate*/, size_t number_of_channels, size_t number_of_frames, absl::optional<int64_t> /*absolute_capture_timestamp_ms*/)
{
    assert(bits_per_sample == 16);
    time_t nowTime = time(NULL);
    if (nowTime - mPreviousTime > 2) // Two seconds between samples
    {
        mPreviousTime = nowTime;
        size_t valueCount = number_of_channels * number_of_frames;
        int16_t *data = (int16_t*)audio_data;
        int16_t audioMaxValue = data[0];
        int16_t audioMinValue = data[0];
        for (size_t i = 1; i < valueCount; i++)
        {
            if (data[i] > audioMaxValue)
            {
                audioMaxValue = data[i];
            }

            if (data[i] < audioMinValue)
            {
                audioMinValue = data[i];
            }
        }

        bool audioDetected = (abs(audioMaxValue) + abs(audioMinValue) > kAudioThreshold);

        auto wptr = weakHandle();
        karere::marshallCall([wptr, this, audioDetected]()
        {
            if (wptr.deleted())
            {
                return;
            }

            if (!hasAudio())
            {
                if (mAudioDetected)
                {
                    onAudioDetected(false);
                }

                return;
            }

            if (audioDetected != mAudioDetected)
            {
                onAudioDetected(audioDetected);
            }

        }, mAppCtx);
    }
}

bool AudioLevelMonitor::hasAudio()
{
    Session *sess = mCall.getSession(static_cast<Cid_t>(mCid));
    if (sess)
    {
        return sess->getAvFlags().audio();
    }
    return false;
}

void AudioLevelMonitor::onAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    Session* sess = mCall.getSession(static_cast<Cid_t>(mCid));
    if (sess)
    {
        sess->setAudioDetected(mAudioDetected);
    }
    else
    {
        RTCM_LOG_WARNING("AudioLevelMonitor::onAudioDetected: session with Cid: %u not found", mCid);
    }
}
}
#endif
