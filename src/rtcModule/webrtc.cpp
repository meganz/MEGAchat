#ifndef KARERE_DISABLE_WEBRTC
#include <mega/types.h>
#include <mega/base64.h>
#include <rtcmPrivate.h>
#include <webrtcPrivate.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>

#include <memory>


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
    mMyPeer.reset(new sfu::Peer(karere::Id(mMegaApi.sdk.getMyUserHandleBinary()), rtc.getMySfuProtoVersion(), avflags.value()));
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

    RTCM_LOG_DEBUG("%sCall state changed. ChatId: %s, callid: %s, state: %s --> %s",
                   getLoggingName(),
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
        clearConnInitialTs(); // reset initial ts for current call connection
        clearJoinOffset();    // reset join offset, the offset is received upon ANSWER command in 't' param

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
                if (mIsReconnectingToChatd)
                {
                    // if timeout to connect to SFU has expired, and we are also connecting to chatd, we need to remove call
                    // when we finally connect to chatd, it should inform us if any call exists
                    mRtc.onDestroyCall(this, rtcModule::EndCallReason::kFailed, kUserHangup); // no need to marshall, as we are executing a lambda in a timer
                }
                else
                {
                    orderedCallDisconnect(kUserHangup, "Reconnection attempt has not succeed"); // TODO add new termcode to notify apps that reconnection attempt failed
                }
            }
        }, kConnectingTimeout * 1000, mRtc.getAppCtx());
    }

    if (newState == CallState::kStateInProgress)  // when ANSWER command is received
    {
        if (mConnectTimer) // cancel timer, as we have joined call before mConnectTimer expired
        {
            karere::cancelTimeout(mConnectTimer, mRtc.getAppCtx());
            mConnectTimer = 0;
        }

        captureConnInitialTs(); // connection initial ts, is set every time we receive ANSWER command (when we are effectively connected to call)
        captureCallInitialTs(); // call initial ts for call (will persist while call is alive)
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

mega::m_time_t Call::getCallWillEndTs() const
{
    return mCallWillEndTs;
}

int Call::getCallDurationLimitInSecs() const
{
    return mCallLimits.durationInSecs;
}

sfu::SfuInterface::CallLimits Call::getCallLimits() const
{
    return mCallLimits;
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
    if (peer != getOwnPeerId()    // check that added peer is not own peerid
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
    if (usersJoined.find(getOwnPeerId()) != usersJoined.end())
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
        if (peerid == getOwnPeerId())
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
        RTCM_LOG_WARNING("%s%s", getLoggingName(), errMsg.c_str());
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

std::set<Cid_t> Call::enableAudioLevelMonitor(const bool enable)
{
    std::set<Cid_t> cidsFailed;
    mAudioLevelMonitor = enable;
    for (auto& itSession : mSessions)
    {
        if (!itSession.second->getAudioSlot()) { continue; }
        if (!itSession.second->getAudioSlot()->enableAudioMonitor(enable))
        {
            cidsFailed.emplace(itSession.first);
        }
    }
    return cidsFailed;
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

    // release both capturer devices (in case they are in use)
    releaseCameraDevice();
    releaseScreenDevice();
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

bool Call::hasUserPendingSpeakRequest(const karere::Id& uh) const
{
    return isOnSpeakRequestsList(uh);
}

int Call::getWrJoiningState() const
{
    return static_cast<int>(mWrJoiningState);
}

bool Call::isValidWrJoiningState() const
{
    return mWrJoiningState == sfu::WrState::WR_NOT_ALLOWED || mWrJoiningState == sfu::WrState::WR_ALLOWED;
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

void Call::setWrJoiningState(const sfu::WrState status)
{
    if (!isValidWrStatus(status))
    {
        RTCM_LOG_WARNING("%supdateAsetWrJoiningState. Invalid status %d", getLoggingName(), status);
        assert(false);
        return;
    }

    mWrJoiningState = status;
}

bool Call::checkWrFlag() const
{
    if (!isWrFlagEnabled())
    {
        RTCM_LOG_ERROR("%sWaiting room should be enabled for this call", getLoggingName());
        assert(false);
        return false;
    }
    return true;
}

bool Call::isConnectedToSfu() const
{
    return mSfuConnection && mSfuConnection->isOnline();
}

bool Call::isSendingBye() const
{
    return mSfuConnection && mSfuConnection->isSendingByeCommand();
}

void Call::clearWrJoiningState()
{
    mWrJoiningState = sfu::WrState::WR_NOT_ALLOWED;
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
    return mCallerId == getOwnPeerId();
}

int64_t Call::getCallInitialTimeStamp() const
{
    return mCallInitialTs;
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
        RTCM_LOG_WARNING("%supdateAndSendLocalAvFlags: AV flags has not changed", getLoggingName());
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

bool Call::addDelSpeakRequest(const karere::Id& user, const bool add)
{
    assert(!add || !user.isValid());                            // SPEAKRQ cannot be sent on behalf of another user
    mSfuConnection->sendSpeakReqAddDel(user, add);
    return true;
}

void Call::addOrRemoveSpeaker(const karere::Id& user, const bool add)
{
    // SPEAKER_ADD cannot be sent for own user
    // moderators don't need to send SPEAKER_ADD for themselves
    assert(user.isValid() || !add);
    mSfuConnection->sendSpeakerAddDel(user, add);
}

void Call::raiseHandToSpeak(const bool add)
{
    mSfuConnection->raiseHandToSpeak(add);
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

void Call::mutePeers(const Cid_t& cid, const unsigned av) const
{
    assert(av == karere::AvFlags::kAudio);
    mSfuConnection->sendMute(cid, av);
}

void Call::setLimits(const uint32_t callDurSecs, const uint32_t numUsers, const uint32_t numClientsPerUser, const uint32_t numClients, const uint32_t divider) const
{
    mSfuConnection->sendSetLimit(callDurSecs, numUsers, numClientsPerUser, numClients, divider);
}

void Call::requestHighResolutionVideo(Cid_t cid, int quality)
{
    // If we are requesting high resolution video for a peer, session must exists
    // so we don't need to wait for PeerVerification promise
    Session *sess= getSession(cid);
    if (!sess)
    {
        RTCM_LOG_DEBUG("%srequestHighResolutionVideo: session not found for %u",
                       getLoggingName(),
                       cid);
        return;
    }

    if (quality < kCallQualityHighDef || quality > kCallQualityHighLow)
    {
        RTCM_LOG_WARNING("%srequestHighResolutionVideo: invalid resolution divider value (spatial "
                         "layer offset): %d",
                         getLoggingName(),
                         quality);
        return;
    }

    if (sess->hasHighResolutionTrack())
    {
        RTCM_LOG_WARNING("%sHigh res video requested, but already available", getLoggingName());
        sess->notifyHiResReceived();
    }
    else
    {
       /* Conditions to reuse track:
        *   1) Peer cannot be sending video from camera and screen share simultaneosly
        *   2) We must already be receiving low resolution track
        */
        const karere::AvFlags peerFlags = sess->getAvFlags();
        const bool isSendingScreenAndCamera = peerFlags.screenShare() && peerFlags.video();
        const bool reuseTrack = !isSendingScreenAndCamera && hasVideoSlot(cid, false);
        mSfuConnection->sendGetHiRes(cid, reuseTrack, quality);
    }
}

void Call::requestHiResQuality(Cid_t cid, int quality)
{
    if (!hasVideoSlot(cid, true))
    {
        RTCM_LOG_WARNING(
            "%srequestHiResQuality: Currently not receiving a hi-res stream for this peer",
            getLoggingName());
        return;
    }

    if (quality < kCallQualityHighDef || quality > kCallQualityHighLow)
    {
        RTCM_LOG_WARNING(
            "%srequestHiResQuality: invalid resolution divider value (spatial layer offset).",
            getLoggingName());
        return;
    }

    mSfuConnection->sendHiResSetLo(cid, quality);
}

void Call::stopHighResolutionVideo(std::vector<Cid_t> &cids)
{
    // If we want to stop receiving high resolution video for a peer, session must exists
    // so we don't need to wait for PeerVerification promise
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            RTCM_LOG_DEBUG("%sstopHighResolutionVideo: session not found for %u",
                           getLoggingName(),
                           *auxit);
            it = cids.erase(auxit);
        }
        else if (!sess->hasHighResolutionTrack())
        {
            RTCM_LOG_WARNING(
                "%sstopHighResolutionVideo: high resolution already not available for cid: %u",
                getLoggingName(),
                *auxit);
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
    // If we are requesting low resolution video for a peer, session must exists
    // so we don't need to wait for PeerVerification promise
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            // remove cid that has no active session
            RTCM_LOG_DEBUG("%srequestLowResolutionVideo: session not found for cid: %u",
                           getLoggingName(),
                           *auxit);
            it = cids.erase(auxit);
        }
        else if (sess->hasLowResolutionTrack())
        {
            RTCM_LOG_WARNING(
                "%srequestLowResolutionVideo: low resolution already available for cid: %u",
                getLoggingName(),
                *auxit);
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
    // If we want to stop receiving low resolution video for a peer, session must exists
    // so we don't need to wait for PeerVerification promise
    for (auto it = cids.begin(); it != cids.end();)
    {
        auto auxit = it++;
        Session *sess= getSession(*auxit);
        if (!sess)
        {
            RTCM_LOG_DEBUG("%sstopLowResolutionVideo: session not found for cid: %u",
                           getLoggingName(),
                           *auxit);
            it = cids.erase(auxit);
        }
        else if (!sess->hasLowResolutionTrack())
        {
            RTCM_LOG_WARNING(
                "%sstopLowResolutionVideo: low resolution already not available for cid: %u",
                getLoggingName(),
                *auxit);
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
        RTCM_LOG_WARNING("%supdateSvcQuality: Invalid new layer index %u",
                         getLoggingName(),
                         mSvcDriver.mCurrentSvcLayerIndex + delta);
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

const std::vector<karere::Id>& Call::getRaiseHandsList() const
{
    return mRaiseHands;
}

std::set<karere::Id> Call::getSpeakersList() const
{
    return mSpeakers;
}

const std::vector<karere::Id>& Call::getSpeakRequestsList() const
{
    return mSpeakRequests;
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
        RTCM_LOG_ERROR("%strying to connect to SFU with an Empty URL", getLoggingName());
        assert(false);
        return false;
    }

    karere::Url sfuUrl(sfuUrlStr);
    if (!sfuUrl.isValid())
    {
        RTCM_LOG_ERROR("%strying to connect to SFU with an Empty Host", getLoggingName());
        assert(sfuUrl.isValid());
        return false;
    }

    if (!mRtc.getDnsCache().getRecordByHost(sfuUrl.host) && !mRtc.getDnsCache().addSfuRecord(sfuUrl.host))
    {
        RTCM_LOG_ERROR("%sconnectSfu: can't retrieve nor add SFU record", getLoggingName());
        assert(mRtc.getDnsCache().getRecordByHost(sfuUrl.host));
        return false;
    }

    mSfuClient.addVersionToUrl(sfuUrl, mRtc.getMySfuProtoVersion());
    setState(CallState::kStateConnecting);
    mSfuConnection = mSfuClient.createSfuConnection(mChatid, std::move(sfuUrl), *this, mRtc.getDnsCache());
    return true;
}

void Call::joinSfu()
{
    if (!mMyPeer)
    {
        RTCM_LOG_WARNING("%sjoinSfu: invalid mMyPeer", getLoggingName());
        assert(false);
        return;
    }

    clearPendingPeers(); // clear pending peers (if any) before joining call
    initStatsValues();
    mRtcConn = artc::MyPeerConnection<Call>(*this, this->mRtc.getAppCtx());
    size_t hiresTrackIndex = 0;
    createTransceivers(hiresTrackIndex);
    getLocalStreams();
    setState(CallState::kStateJoining);
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
            RTCM_LOG_WARNING("%sjoinSfu: get unexpected state change at createOffer",
                             getLoggingName());
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
            RTCM_LOG_WARNING("%sjoinSfu: get unexpected state change at setLocalDescription",
                             getLoggingName());
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
            return;
        }

        mSfuConnection->joinSfu(sdp,
                                ivs,
                                ephemeralKey,
                                getLocalAvFlags().value(),
                                getPrevCid(),
                                RtcConstant::kInitialvthumbCount,
                                hasRaisedHand(mMyPeer->getPeerid()));
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
    mVThumb = std::make_unique<LocalSlot>(*this, err.MoveValue());
    mVThumb->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitHiRes;
    transceiverInitHiRes.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitHiRes);
    hiresTrackIndex = mRtcConn->GetTransceivers().size() - 1; // keep this sentence just after add transceiver for hiRes track
    mHiRes = std::make_unique<LocalSlot>(*this, err.MoveValue());
    mHiRes->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitAudio;
    transceiverInitAudio.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInitAudio);
    mAudio = std::make_unique<LocalSlot>(*this, err.MoveValue());
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

void Call::orderedCallDisconnect(TermCode termCode, const std::string &msg, const bool forceDisconnect)
{
    if (isDestroying() && !forceDisconnect)
    {
        RTCM_LOG_WARNING("%sorderedCallDisconnect: call is already being destroyed",
                         getLoggingName());
        return;
    }

    if (isSendingBye())
    {
        RTCM_LOG_DEBUG("%sorderedCallDisconnect, there's a disconnection attempt in progress (by "
                       "sending BYE command)",
                       getLoggingName());
        return;
    }

    RTCM_LOG_DEBUG("%sorderedCallDisconnect, termcode: %s, msg: %s",
                   getLoggingName(),
                   connectionTermCodeToString(termCode).c_str(),
                   msg.c_str());
    if (mIsReconnectingToChatd)
    {
        clearParticipants();
    }

    if (termCode == kUserHangup)
    {
        // clear raised hands list just if termCode is kUserHangup
        mRaiseHands.clear();
    }

    if (isConnectedToSfu())
    {
        sendStats(termCode);
        if (termCode != kSigDisconn) // kSigDisconn is mutually exclusive with BYE command
        {
            // store termcode temporarily until confirm BYE command has been sent
            mTempTermCode = termCode;

            // send BYE command as part of the protocol to inform SFU about the disconnection reason
            // once LWS confirms that BYE command has been sent (check processNextCommand) onSendByeCommand will be called
            mSfuConnection->sendBye(termCode);
            return;
        }
    }

    // if we are not connected to SFU or kSigDisconn, just perform disconnection
    immediateCallDisconnect(termCode);
}

void Call::removeCallImmediately(uint8_t reason, TermCode connectionTermCode)
{
    assert(reason != kInvalidReason);
    RTCM_LOG_DEBUG("%sRemoving call with callid: %s",
                   getLoggingName(),
                   getCallid().toString().c_str());
    // clear resources and disconnect from media channel and SFU (if required)
    immediateCallDisconnect(connectionTermCode);
    // upon kStateDestroyed state change (in call dtor) mEndCallReason will be notified through onCallStateChange
    setEndCallReason(reason);
    mRtc.deleteCall(getCallid());
}

void Call::clearResources(const TermCode& termCode)
{
    RTCM_LOG_DEBUG("%sclearResources, termcode (%u): %s",
                   getLoggingName(),
                   termCode,
                   connectionTermCodeToString(termCode).c_str());
    disableStats();
    mSessions.clear();              // session dtor will notify apps through onDestroySession callback
    clearPendingPeers();
    clearModeratorsList();
    clearSpeakRequestsList();
    clearSpeakersList();
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
            releaseCameraDevice();
        }

        if (getLocalAvFlags().screenShare())
        {
            releaseScreenDevice();
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
        case kWaitingRoomAllowTimeout:  return "Timed out waiting to be allowed from waiting room into call";
        default:                        return "invalid connection termcode";
    }
}

bool Call::isUdpDisconnected() const
{
    if (!mega::isValidTimeStamp(getConnInitialTimeStamp()))
    {
        // peerconnection establishment starts as soon ANSWER is sent to the client
        // we never have reached kStateInProgress, as mInitialTs is set when we reach kStateInProgress (upon ANSWER command is received)
        RTCM_LOG_ERROR(
            "%sonConnectionChange(kDisconnected) received but mInitialTs is not initialized",
            getLoggingName());
        assert(false);
        return true;
    }

    return (mStats.mSamples.mT.empty() && (time(nullptr) - getConnInitialTimeStamp() > sfu::SfuConnection::kNoMediaPathTimeout));
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

const karere::Id& Call::getOwnPeerId() const
{
    return mMyPeer->getPeerid();
}

bool Call::hasUserSpeakPermission(const uint64_t userid) const
{
    return !isSpeakRequestEnabled()
           || isOnSpeakersList(userid)
           || isOnModeratorsList(userid);
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
        RTCM_LOG_DEBUG("%ssendStats: stats are empty", getLoggingName());
        return;
    }

    assert(isValidConnectionTermcode(termCode));
    mStats.mDuration =
        mega::isValidTimeStamp(getConnInitialTimeStamp()) // mInitialTs
            ?
            static_cast<uint64_t>((time(nullptr) - getConnInitialTimeStamp()) * 1000) // ms
            :
            mega::mega_invalid_timestamp; // in case we have not joined SFU yet, send duration = 0
    mStats.mMaxPeers = mMaxPeers;
    mStats.mTermCode = static_cast<int32_t>(termCode);
    if (auto [statsValidation, statsJson] = mStats.getJson(); statsValidation.any())
    {
        RTCM_LOG_WARNING(
            "%ssendStats: discarding callstats due to the following error/s: %s.\nJSON: %s",
            getLoggingName(),
            Stats::statsErrToString(statsValidation).c_str(),
            statsJson.c_str());
    }
    else
    {
        mMegaApi.call(&::mega::MegaApi::sendChatStats, statsJson.c_str(), 0)
            .then(
                [lname = getLoggingName()](ReqResult result)
                {
                    if (result->getNumber() != Stats::httpErrOk)
                    {
                        RTCM_LOG_WARNING(
                            "%sReceived error %d from SFU stats server, for this stats JSON: %s",
                            lname,
                            result->getNumber(),
                            result->getName());
                        assert(false);
                    }
                });
    }

    RTCM_LOG_DEBUG("%sClear local SFU stats", getLoggingName());
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
        RTCM_LOG_WARNING("%shandleAvCommand: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    if (getOwnCid() == cid)
    {
        RTCM_LOG_WARNING("%shandleAvCommand: Received our own AV flags", getLoggingName());
        return false;
    }

    promise::Promise<void>* pms = getPeerVerificationPms(cid);
    if (!pms)
    {
        RTCM_LOG_WARNING("%shandleAvCommand: PeerVerification promise not found for cid: %u",
                         getLoggingName(),
                         cid);
        return false;
    }

    auto wptr = weakHandle();
    pms->then(
           [this, cid, av, aMid, wptr]()
           {
               if (wptr.deleted())
               {
                   return;
               }
               Session* session = getSession(cid);
               if (!session)
               {
                   RTCM_LOG_WARNING("%shandleAvCommand: Received AV flags for unknown peer cid %u",
                                    getLoggingName(),
                                    cid);
                   return;
               }

               bool oldAudioFlag = session->getAvFlags().audio();

               // update session flags
               session->setRemoteAvFlags(karere::AvFlags(static_cast<uint8_t>(av)));

               if (aMid == sfu::TrackDescriptor::invalidMid)
               {
                   if (oldAudioFlag != session->getAvFlags().audio() &&
                       session->getAvFlags().audio())
                   {
                       assert(false);
                       RTCM_LOG_WARNING("%shandleAvCommand: invalid amid received for peer cid %u",
                                        getLoggingName(),
                                        cid);
                       return;
                   }

                   if (!session->getAvFlags().audio() && session->getAudioSlot())
                   {
                       // disable slot if audio flag has been received as disabled
                       session->disableAudioSlot();
                   }
               }
               else
               {
                   assert(session->getAvFlags().audio());
                   sfu::TrackDescriptor trackDescriptor;
                   trackDescriptor.mMid = aMid;
                   trackDescriptor.mReuse = true;
                   addSpeaker(cid, aMid);
               }
           })
        .fail(
            [cid, lname = getLoggingName()](const ::promise::Error&)
            {
                RTCM_LOG_WARNING(
                    "%shandleAvCommand: PeerVerification promise was rejected for cid: %u",
                    lname,
                    cid);
                return;
            });

    return true;
}

bool Call::handleAnswerCommand(Cid_t cid, std::shared_ptr<sfu::Sdp> sdp, uint64_t callJoinOffset, std::vector<sfu::Peer>& peers,
                               const std::map<Cid_t, std::string>& keystrmap,
                               const std::map<Cid_t, sfu::TrackDescriptor>& vthumbs,
                               const std::set<karere::Id>& speakers,
                               const std::vector<karere::Id>& speakReqs,
                               const std::vector<karere::Id>& raiseHands,
                               const std::map<Cid_t, uint32_t>& amidmap)
{
    if (mState != kStateJoining)
    {
        RTCM_LOG_WARNING("%shandleAnswerCommand: get unexpected state change", getLoggingName());
        return false;
    }

    if (!getMyEphemeralKeyPair())
    {
        RTCM_LOG_ERROR("%sCan't retrieve Ephemeral key for our own user, SFU protocol version: %u",
                       getLoggingName(),
                       static_cast<unsigned int>(mRtc.getMySfuProtoVersion()));
        return false;
    }

    if (!speakReqs.empty() && !isSpeakRequestEnabled())
    {
        const std::string errMsg = "handleAnswerCommand: we shouldn't receive speak requests if that option is disabled for chatroom";
        RTCM_LOG_WARNING("%s%s", getLoggingName(), errMsg.c_str());
        assert(false);
        orderedCallDisconnect(TermCode::kUserHangup, errMsg);
        return false;
    }

    /* Store speakers list received from SFU, which contains user handles of all non-moderator
     * users that have been given speak permission (moderators not included)
     *
     * If call doesn't have speak request option enabled, this array is not included in the ANSWER command
     */
    mSpeakers = speakers;

    // store user list that raised hand, received from SFU
    mRaiseHands = raiseHands;

    // store speak requests list received from SFU
    mSpeakRequests = speakReqs;

    // clear initial backoff as this connection attempt has succeeded
    mSfuConnection->clearInitialBackoff();

    // set my own client-id (cid)
    mMyPeer->setCid(cid);

    // update max peers seen in call
    mMaxPeers = static_cast<uint8_t> (peers.size() > mMaxPeers ? peers.size() : mMaxPeers);

    // set join offset
    setJoinOffset(static_cast<int64_t>(callJoinOffset));

    if (hasUserSpeakPermission(getOwnPeerId()) && isOnSpeakRequestsList(getOwnPeerId()))
    {
        const std::string errMsg = "handleAnswerCommand: Our own user is included on speakers list but also in speak requests list";
        RTCM_LOG_WARNING("%s%s", getLoggingName(), errMsg.c_str());
        assert(false);
        orderedCallDisconnect(TermCode::kUserHangup, errMsg);
        return false;
    }

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

    auto keysVerified = std::make_shared<std::vector<bool>>();
    auto onKeyVerified = [max = peers.size(),
                          keysVerified,
                          keyDerivationPms,
                          lname = getLoggingName()](const bool verified) -> void
    {
        if (keyDerivationPms->done())
        {
            RTCM_LOG_WARNING("%shandleAnswerCommand: keyDerivationPms already resolved", lname);
            assert(keyDerivationPms->succeeded());
            return;
        }

        if (!keysVerified)
        {
            RTCM_LOG_WARNING("%shandleAnswerCommand: invalid keysVerified at onKeyVerified", lname);
            assert(false);
            return;
        }

        if (!keyDerivationPms)
        {
            RTCM_LOG_WARNING("%shandleAnswerCommand: invalid keyDerivationPms at onKeyVerified",
                             lname);
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
        if (!addPendingPeer(peer.getCid()))
        {
            RTCM_LOG_WARNING(
                "%shandleAnswerCommand: duplicated peer at mPeersVerification, with cid: %u ",
                getLoggingName(),
                cid);
            assert(false);
            continue;
        }

        // check if peerid is included in mods list received upon HELLO
        peer.setModerator(isOnModeratorsList(peer.getPeerid()));
        if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V0) // there's no ephemeral key, just add peer
        {
            addPeerWithEphemKey(peer, true, std::string());
        }
        else if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V1)
        {
            // we shouldn't receive any peer with protocol v1
            RTCM_LOG_ERROR(
                "%shandleAnswerCommand: unexpected SFU protocol version [%u] for user: %s, cid: %u",
                getLoggingName(),
                static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer.getPeerSfuVersion()),
                peer.getPeerid().toString().c_str(),
                peer.getCid());
            assert(false);
        }
        else if (peer.getPeerSfuVersion() >= sfu::SfuProtocol::SFU_PROTO_V2) // verify ephemeral key signature, derive it, and then add the peer
        {
            if (!sfu::isKnownSfuVersion(peer.getPeerSfuVersion()))
            {
                // important: upon an unkown peers's SFU protocol version, native client should act as if they are the latest known version
                RTCM_LOG_WARNING("%shandleAnswerCommand: unknown SFU protocol version [%u] for "
                                 "user: %s, cid: %u",
                                 getLoggingName(),
                                 static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                                     peer.getPeerSfuVersion()),
                                 peer.getPeerid().toString().c_str(),
                                 peer.getCid());
            }

            if (keyStr.empty())
            {
                RTCM_LOG_ERROR(
                    "%sEmpty Ephemeral key for user: %s, cid: %u, SFU protocol version: %u",
                    getLoggingName(),
                    peer.getPeerid().toString().c_str(),
                    peer.getCid(),
                    static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                        peer.getPeerSfuVersion()));
                addPeerWithEphemKey(peer, false, std::string());
                continue;
            }

            try
            {
                auto wptr = weakHandle();
                auto parsedkey = splitPubKey(keyStr);
                std::shared_ptr<sfu::Peer> auxPeer(new sfu::Peer(peer));
                verifySignature(peer.getCid(), peer.getPeerid(), parsedkey.first, parsedkey.second)
                    .then(
                        [wptr, auxPeer, addPeerWithEphemKey, parsedkey, this](bool verified)
                        {
                            wptr.throwIfDeleted();
                            const mega::ECDH* ephkeypair = getMyEphemeralKeyPair();
                            if (!ephkeypair)
                            {
                                RTCM_LOG_ERROR(
                                    "%sCan't retrieve Ephemeral key for our own user, SFU protocol "
                                    "version: %u",
                                    getLoggingName(),
                                    static_cast<unsigned int>(mRtc.getMySfuProtoVersion()));
                                addPeerWithEphemKey(*auxPeer, false, std::string());
                                return;
                            }

                            if (!verified)
                            {
                                RTCM_LOG_ERROR("%sCan't verify signature for user: %s",
                                               getLoggingName(),
                                               auxPeer->getPeerid().toString().c_str());
                                addPeerWithEphemKey(*auxPeer, false, std::string());
                                return;
                            }

                            // once peer public ephemeral key has been verified, derive it with our
                            // private ephemeral key
                            std::string out;
                            const std::string pubkeyBin = mega::Base64::atob(parsedkey.first);
                            std::vector<::mega::byte> saltBin =
                                generateEphemeralKeyIv(auxPeer->getIvs(), mMyPeer->getIvs());
                            bool derived = ephkeypair->deriveSharedKeyWithSalt(
                                reinterpret_cast<const unsigned char*>(pubkeyBin.data()),
                                saltBin.data(),
                                saltBin.size(),
                                out);
                            if (!derived)
                            {
                                RTCM_LOG_ERROR(
                                    "%sCan't derive ephemeral key for peer Cid: %u PeerId: %s",
                                    getLoggingName(),
                                    auxPeer->getCid(),
                                    auxPeer->getPeerid().toString().c_str());

                                out.clear();
                            }

                            addPeerWithEphemKey(*auxPeer, derived, out);
                        })
                    .fail(
                        [this, auxPeer, addPeerWithEphemKey](const ::promise::Error&)
                        {
                            RTCM_LOG_ERROR("%sError verifying ephemeral key signature for for "
                                           "user: %s, cid: %u",
                                           getLoggingName(),
                                           auxPeer->getPeerid().toString().c_str(),
                                           auxPeer->getCid());
                            addPeerWithEphemKey(*auxPeer, false, std::string());
                        });
            }
            catch(std::runtime_error& e)
            {
                RTCM_LOG_ERROR("%sError verifying ephemeral key signature: %s",
                               getLoggingName(),
                               e.what());
                return false; // wprt doesn't exists
            }
        }
    }

    // wait until all peers ephemeral keys have been verified and derived
    auto auxwptr = weakHandle();
    keyDerivationPms
    ->then([auxwptr, vthumbs, amidmap, sdp, keysVerified, this]
    {
        if (auxwptr.deleted())
        {
            return;
        }

        if (!keysVerified)
        {
            RTCM_LOG_WARNING(
                "%shandleAnswerCommand: invalid keysVerified at keyDerivationPms resolved",
                getLoggingName());
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
        .then([wptr, this, vthumbs, amidmap]()
        {
            if (wptr.deleted())
            {
                return;
            }

            if (mState != kStateJoining)
            {
                RTCM_LOG_WARNING(
                    "%shandleAnswerCommand: get unexpect state change at setRemoteDescription",
                    getLoggingName());
                return;
            }

            if (amidmap.size() > mSessions.size())
            {
                RTCM_LOG_WARNING("%shandleAnswerCommand: received mid list is greater than current "
                                 "list of sessions",
                                 getLoggingName());
                assert(false);
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

            for (auto& s: mSessions)
            {
                // add speaker if is currently sending audio (valid amid received)
                const auto cid = s.second->getClientid();
                const auto it = amidmap.find(cid);
                if (it != amidmap.end())
                {
                    addSpeaker(cid, it->second/*amid*/);
                }
            }

            if (hasUserSpeakPermission(getOwnPeerId()))
            {
                updateAudioTracks();
            }

            setState(CallState::kStateInProgress);
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
        RTCM_LOG_WARNING("%shandleKeyCommand: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    promise::Promise<void>* pms = getPeerVerificationPms(cid);
    if (!pms)
    {
        RTCM_LOG_WARNING("%shandleKeyCommand: PeerVerification promise not found for cid: %u",
                         getLoggingName(),
                         cid);
        return false;
    }

    auto wptr = weakHandle();
    pms->then(
           [this, keyid, cid, key, wptr]()
           {
               if (wptr.deleted())
               {
                   return;
               }
               Session* session = getSession(cid);
               if (!session)
               {
                   RTCM_LOG_WARNING("%shandleKeyCommand: session not found for Cid: %u",
                                    getLoggingName(),
                                    cid);
                   return;
               }

               const sfu::Peer& peer = session->getPeer();
               auto wptr = weakHandle();

               if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V0)
               {
                   mSfuClient.getRtcCryptoMeetings()
                       ->getCU25519PublicKey(peer.getPeerid())
                       .then(
                           [wptr, keyid, cid, key, this](Buffer*) -> void
                           {
                               if (wptr.deleted())
                               {
                                   return;
                               }

                               Session* session = getSession(cid);
                               if (!session)
                               {
                                   RTCM_LOG_WARNING(
                                       "%shandleKeyCommand: session not found for Cid: %u",
                                       getLoggingName(),
                                       cid);
                                   return;
                               }

                               // decrypt received key
                               std::string binaryKey = mega::Base64::atob(key);
                               strongvelope::SendKey encryptedKey;
                               mSfuClient.getRtcCryptoMeetings()->strToKey(binaryKey, encryptedKey);

                               strongvelope::SendKey plainKey;
                               mSfuClient.getRtcCryptoMeetings()->decryptKeyFrom(
                                   session->getPeer().getPeerid(),
                                   encryptedKey,
                                   plainKey);

                               // in case of a call in a public chatroom, XORs received key with the
                               // call key for additional authentication
                               if (hasCallKey())
                               {
                                   strongvelope::SendKey callKey;
                                   mSfuClient.getRtcCryptoMeetings()->strToKey(mCallKey, callKey);
                                   mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(callKey,
                                                                                     plainKey);
                               }

                               // add new key to peer key map
                               std::string newKey =
                                   mSfuClient.getRtcCryptoMeetings()->keyToStr(plainKey);
                               session->addKey(keyid, newKey);
                           });
               }
               else if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V1)
               {
                   // we shouldn't receive any peer with protocol v1
                   RTCM_LOG_ERROR("%shandleKeyCommand: unexpected SFU protocol version [%u] for "
                                  "user: %s, cid: %u",
                                  getLoggingName(),
                                  static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                                      peer.getPeerSfuVersion()),
                                  peer.getPeerid().toString().c_str(),
                                  peer.getCid());
                   assert(false);
                   return;
               }
               else if (peer.getPeerSfuVersion() >= sfu::SfuProtocol::SFU_PROTO_V2)
               {
                   if (!sfu::isKnownSfuVersion(peer.getPeerSfuVersion()))
                   {
                       // important: upon an unkown peers's SFU protocol version, native client
                       // should act as if they are the latest known version
                       RTCM_LOG_WARNING("%shandlePeerJoin: unknown SFU protocol version [%u] for "
                                        "user: %s, cid: %u",
                                        getLoggingName(),
                                        static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                                            peer.getPeerSfuVersion()),
                                        peer.getPeerid().toString().c_str(),
                                        peer.getCid());
                   }

                   Session* session = getSession(cid);
                   if (!session)
                   {
                       RTCM_LOG_WARNING("%shandleKeyCommand: session not found for Cid: %u",
                                        getLoggingName(),
                                        cid);
                       return;
                   }

                   const sfu::Peer& auxPeer = session->getPeer();
                   auto&& ephemeralPubKey = auxPeer.getEphemeralPubKeyDerived();
                   if (ephemeralPubKey.empty())
                   {
                       RTCM_LOG_WARNING("%sInvalid ephemeral key for peer: %s cid %u",
                                        getLoggingName(),
                                        auxPeer.getPeerid().toString().c_str(),
                                        cid);
                       assert(false);
                       return;
                   }

                   std::string result;
                   std::string recvKeyBin = mega::Base64::atob(key);
                   if (!mSymCipher.cbc_decrypt_with_key(
                           recvKeyBin,
                           result,
                           reinterpret_cast<const unsigned char*>(ephemeralPubKey.data()),
                           ephemeralPubKey.size(),
                           nullptr))
                   {
                       std::string err = "Failed cbc_decrypt received key. Cid: " +
                                         std::to_string(auxPeer.getCid()) +
                                         "PeerId: " + auxPeer.getPeerid().toString() +
                                         "KeyId: " + std::to_string(keyid);

                       mRtc.onMediaKeyDecryptionFailed(err);
                       RTCM_LOG_WARNING("%s%s", getLoggingName(), err.c_str());
                       return;
                   }

                   // in case of a call in a public chatroom, XORs received key with the call key
                   // for additional authentication
                   if (hasCallKey())
                   {
                       mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(
                           reinterpret_cast<::mega::byte*>(mCallKey.data()),
                           reinterpret_cast<::mega::byte*>(result.data()));
                   }

                   if (result.size() != kMediaKeyLen)
                   {
                       mRtc.onMediaKeyDecryptionFailed("Unexpected decrypted key size");
                       RTCM_LOG_ERROR(
                           "%sUnexpected decrypted key size expected size: %u decrypted size: %d",
                           getLoggingName(),
                           kMediaKeyLen,
                           static_cast<int>(result.size()));
                       return;
                   }
                   session->addKey(keyid, result);
               }
           })
        .fail(
            [cid, lname = getLoggingName()](const ::promise::Error&)
            {
                RTCM_LOG_WARNING(
                    "%shandleKeyCommand: PeerVerification promise was rejected for cid: %u",
                    lname,
                    cid);
                return;
            });

    return true;
}

bool Call::handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("%shandleVThumbsCommand: get unexpected state", getLoggingName());
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
        RTCM_LOG_WARNING("%shandleVThumbsStartCommand: get unexpected state", getLoggingName());
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
        RTCM_LOG_WARNING("%shandleVThumbsStopCommand: get unexpected state", getLoggingName());
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
        RTCM_LOG_WARNING("%shandleHiResCommand: get unexpected state", getLoggingName());
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
        RTCM_LOG_WARNING("%shandleHiResStartCommand: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mHiResActive = true;
    updateVideoTracks();
    return true;
}

bool Call::handleSpeakerAddDelCommand(const uint64_t userid, const bool add)
{
    if (!isSpeakRequestEnabled())
    {
        RTCM_LOG_WARNING("%shandle %s command. Speak request option is disabled for call",
                         getLoggingName(),
                         add ? "SPEAKER_ADD" : "SPEAKER_DEL");
        assert(false);
        return false;
    }

    // if userid is invalid, command is for own user
    const karere::Id uh = !karere::Id(userid).isValid() ? getOwnPeerId() : karere::Id(userid);
    const bool isOwnUser = uh == getOwnPeerId();
    if (isOwnUser && isOnModeratorsList(userid))
    {
        RTCM_LOG_WARNING(
            "%sSPEAKER_ADD/DEL command should not be received for own user with moderator role",
            getLoggingName());
        assert(false);
        return false;
    }

    if (!updateSpeakersList(uh, add))
    {
        RTCM_LOG_WARNING("%shandle %s command. cannot update speakers list",
                         getLoggingName(),
                         add ? "SPEAKER_ADD" : "SPEAKER_DEL");
    }

    if (add) // Speak permission granted for user
    {
        if (isOwnUser)
        {
            updateAudioTracks();
        }

        updateSpeakRequestsList(uh, false/*add*/);  // remove speak request (if any) for this user
    }
    // else => no need to update audio tracks for own user upon SPEAKER_DEL, MUTED command will be received
    return true;
}

bool Call::handleHiResStopCommand()
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("%shandleHiResStopCommand: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    mHiResActive = false;
    updateVideoTracks();
    return true;
}

bool Call::handleSpeakReqAddDelCommand(const uint64_t userid, const bool add)
{
    if (!isSpeakRequestEnabled())
    {
        RTCM_LOG_WARNING("%shandle %s command. Speak request option is disabled for call",
                         getLoggingName(),
                         add ? "SPEAKRQ" : "SPEAKRQ_DEL");
        assert(false);
        return false;
    }

    // userid must be always valid for SPEAKRQ and SPEAKRQ_DEL
    if (!updateSpeakRequestsList(userid, add))
    {
        RTCM_LOG_WARNING("%shandle %s command. cannot update speak requests list",
                         getLoggingName(),
                         add ? "SPEAKRQ" : "SPEAKRQ_DEL");
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
        RTCM_LOG_WARNING("%shandlePeerJoin: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    const mega::ECDH* ephkeypair = getMyEphemeralKeyPair();
    if (!ephkeypair)
    {
        RTCM_LOG_ERROR("%sCan't retrieve Ephemeral key for our own user, SFU protocol version: %u",
                       getLoggingName(),
                       static_cast<unsigned int>(mRtc.getMySfuProtoVersion()));
        orderedCallDisconnect(TermCode::kErrorCrypto, "Can't retrieve Ephemeral key for our own user");
        return false;
    }

    if (!addPendingPeer(cid))
    {
        RTCM_LOG_WARNING("%shandlePeerJoin: duplicated peer at mPeersVerification, with cid: %u ",
                         getLoggingName(),
                         cid);
        assert(false);
        return false;
    }

    std::shared_ptr<sfu::Peer> peer(new sfu::Peer(userid, sfuProtoVersion, static_cast<unsigned>(av), &ivs, cid, (mModerators.find(userid) != mModerators.end())));
    if (sfuProtoVersion == sfu::SfuProtocol::SFU_PROTO_V0)
    {
        addPeerWithEphemKey(*peer, std::string());
    }
    else if (sfuProtoVersion == sfu::SfuProtocol::SFU_PROTO_V1)
    {
        // we shouldn't receive any peer with protocol v1
        RTCM_LOG_ERROR(
            "%shandlePeerJoin: unexpected SFU protocol version [%u] for user: %s, cid: %u",
            getLoggingName(),
            static_cast<std::underlying_type<sfu::SfuProtocol>::type>(peer->getPeerSfuVersion()),
            peer->getPeerid().toString().c_str(),
            peer->getCid());
        assert(false);
        return false;
    }
    else if (sfuProtoVersion >= sfu::SfuProtocol::SFU_PROTO_V2)
    {
        if (!sfu::isKnownSfuVersion(sfuProtoVersion))
        {
            // important: upon an unkown peers's SFU protocol version, native client should act as if they are the latest known version
            RTCM_LOG_WARNING(
                "%shandlePeerJoin: unknown SFU protocol version [%u] for user: %s, cid: %u",
                getLoggingName(),
                static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                    peer->getPeerSfuVersion()),
                peer->getPeerid().toString().c_str(),
                peer->getCid());
        }

        if (keyStr.empty())
        {
            RTCM_LOG_ERROR("%shandlePeerJoin: ephemeral key not received", getLoggingName());
            assert(false);
            addPeerWithEphemKey(*peer, std::string());
            return false;
        }

        auto parsedkey = splitPubKey(keyStr);
        verifySignature(cid, userid, parsedkey.first, parsedkey.second)
            .then(
                [userid, parsedkey, peer, ephkeypair, addPeerWithEphemKey, this](bool verified)
                {
                    if (!verified)
                    {
                        RTCM_LOG_WARNING("%sCan't verify signature for user: %s",
                                         getLoggingName(),
                                         karere::Id(userid).toString().c_str());
                        assert(false);
                        addPeerWithEphemKey(*peer, std::string());
                        return;
                    }

                    // derive peer public ephemeral key with our private ephemeral key
                    std::string out;
                    const std::string pubkeyBin = mega::Base64::atob(parsedkey.first);
                    std::vector<::mega::byte> saltBin =
                        generateEphemeralKeyIv(peer->getIvs(), mMyPeer->getIvs());
                    bool derived = ephkeypair->deriveSharedKeyWithSalt(
                        reinterpret_cast<const unsigned char*>(pubkeyBin.data()),
                        saltBin.data(),
                        saltBin.size(),
                        out);
                    if (!derived)
                    {
                        RTCM_LOG_WARNING("%sCan't derive ephemeral key for peer Cid: %u PeerId: %s",
                                         getLoggingName(),
                                         peer->getCid(),
                                         peer->getPeerid().toString().c_str());

                        out.clear();
                    }
                    addPeerWithEphemKey(*peer, out);
                })
            .fail(
                [this, userid, peer, addPeerWithEphemKey](const ::promise::Error&)
                {
                    RTCM_LOG_ERROR("%sCan't retrieve public ED25519 attr for user %s",
                                   getLoggingName(),
                                   karere::Id(userid).toString().c_str());
                    addPeerWithEphemKey(*peer, std::string());
                });
    }
    return true;
}

bool Call::handlePeerLeft(Cid_t cid, unsigned termcode)
{
    if (mState != kStateInProgress && mState != kStateJoining)
    {
        RTCM_LOG_WARNING("%shandlePeerLeft: get unexpected state", getLoggingName());
        assert(false); // theoretically, it should not happen. If so, it may worth to investigate
        return false;
    }

    // reject peer promise (if still undone) and remove from map; finally check
    // if was added to sessions map, and perform required operations with that session
    removePendingPeer(cid);
    auto it = mSessions.find(cid);
    if (it == mSessions.end())
    {
        RTCM_LOG_WARNING("%shandlePeerLeft: cid: %u not found in sessions map",
                         getLoggingName(),
                         cid);
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
        RTCM_LOG_DEBUG("%shandlePeerLeft. Hangup 1on1 call, upon reception of PEERLEFT with non "
                       "recoverable termcode: %s",
                       getLoggingName(),
                       connectionTermCodeToString(peerLeftTermCode).c_str());
        hangup(); // TermCode::kUserHangup
    }
    return true;
}

bool Call::handleBye(const unsigned termCode, const bool wr, const std::string& errMsg)
{
    RTCM_LOG_WARNING("%shandleBye - termCode: %d, reason: %s",
                     getLoggingName(),
                     termCode,
                     errMsg.c_str());
    TermCode auxTermCode = static_cast<TermCode> (termCode);
    if (!isValidConnectionTermcode(auxTermCode))
    {
        RTCM_LOG_ERROR("%sInvalid termCode [%u] received at BYE command",
                       getLoggingName(),
                       termCode);
        return false;
    }

    if (isDestroying())
    {
        RTCM_LOG_WARNING("%shandleBye: call is already being destroyed", getLoggingName());
        return true;
    }

    if (wr) // we have been moved into a waiting room
    {
        assert (auxTermCode == kPushedToWaitingRoom);
        if (!isValidWrJoiningState())
        {
            RTCM_LOG_ERROR("%shandleBye: wr received but our current WrJoiningState is not valid",
                           getLoggingName());
            assert(false);
            return false;
        }
        pushIntoWr(auxTermCode);
    }
    else
    {
        auto wptr = weakHandle();
        karere::marshallCall([wptr, auxTermCode, this]()
        {
            if (wptr.deleted()) { return; }
            immediateCallDisconnect(auxTermCode);
        }, mRtc.getAppCtx());
    }
    return true;
}

bool Call::handleRaiseHandAddCommand(const uint64_t userid)
{
    const karere::Id uh = karere::Id(userid).isNull()
                            ? mMyPeer->getPeerid().val
                            : userid;

    if (std::find(mRaiseHands.begin(), mRaiseHands.end(), uh) != mRaiseHands.end())
    {
        RTCM_LOG_WARNING("%sRHANDRQ_ADD received, but user already exists at rhands list: %s ",
                         getLoggingName(),
                         karere::Id(uh).toString().c_str());
        assert(false);
        return false;
    }

    mRaiseHands.emplace_back(uh);
    mCallHandler.onRaiseHandAddedRemoved(*this, uh, true);
    return true;
}

bool Call::handleRaiseHandDelCommand(const uint64_t userid)
{
    const karere::Id uh = karere::Id(userid).isNull()
                      ? mMyPeer->getPeerid().val
                      : userid;

    auto it = std::find(mRaiseHands.begin(), mRaiseHands.end(), uh);
    if (it == mRaiseHands.end())
    {
        RTCM_LOG_WARNING("%sRHANDRQ_DEL received, but user cannot be removed from rhands list: %s ",
                         getLoggingName(),
                         karere::Id(uh).toString().c_str());
        assert(false);
        return false;
    }

    mRaiseHands.erase(it);
    mCallHandler.onRaiseHandAddedRemoved(*this, uh, false);
    return true;
}

bool Call::hasRaisedHand(const uint64_t userid) const
{
    return std::find(mRaiseHands.begin(), mRaiseHands.end(), userid) != mRaiseHands.end();
}

bool Call::handleModAdd(uint64_t userid)
{
    updateUserModeratorStatus(userid, true /*enable*/);
    if (isSpeakRequestEnabled())
    {
        // remove user from speakers and speak requests list (just if included),
        // as moderators cannot be included on speak requests nor speakers lists
        updateSpeakersList(userid, false);
        updateSpeakRequestsList(userid, false/*add*/);
    }

    // moderators have speak permission by default, and shouldn't be in speakers list
    if (!hasUserSpeakPermission(userid))
    {
        RTCM_LOG_DEBUG(
            "%sMOD_ADD received, but speak permission could not be updated for user: %s ",
            getLoggingName(),
            karere::Id(userid).toString().c_str());
        assert(false);
        return false;
    }

    if (userid == getOwnPeerId())
    {
        updateAudioTracks();
        if (isWrFlagEnabled()
            && static_cast<sfu::WrState>(getWrJoiningState()) != sfu::WrState::WR_ALLOWED
            && getState() == kInWaitingRoom)
        {
            // automatically JOIN call from WR as now we have moderator role, so we have permission
            RTCM_LOG_DEBUG("%sMOD_ADD received for our own user, and we are in waiting room. JOIN "
                           "call automatically",
                           getLoggingName());
            setWrJoiningState(sfu::WrState::WR_ALLOWED);
            mCallHandler.onWrAllow(*this);
            joinSfu();
        }
    }

    RTCM_LOG_DEBUG("%sMOD_ADD: user[%s] added in moderators list",
                   getLoggingName(),
                   karere::Id(userid).toString().c_str());
    return true;
}

bool Call::handleModDel(uint64_t userid)
{
    // Note: if command is received for own user, we don't need to call updateAudioTracks(), SFU will send us 'MUTED' command
    updateUserModeratorStatus(userid, false /*enable*/);

    if (isSpeakRequestEnabled())
    {
        if (isOnSpeakersList(userid))
        {
            // note: moderators should never be included on the speakers list
            RTCM_LOG_WARNING(
                "%sMOD_DEL received, but user: %s was already included on speakers list",
                getLoggingName(),
                karere::Id(userid).toString().c_str());

            assert(false);
            updateSpeakersList(userid, false);
        }

        if (isOnSpeakRequestsList(userid))
        {
            // note: moderators should never be included on the speak requests list
            RTCM_LOG_WARNING(
                "%sMOD_DEL received, but user: %s was already included on speak requests list",
                getLoggingName(),
                karere::Id(userid).toString().c_str());

            assert(false);
            updateSpeakRequestsList(userid, false);
        }
    }

    // Note: ex-moderators need be granted speak permission again by a moderator
    RTCM_LOG_DEBUG("%sMOD_DEL: user[%s] removed from moderators list",
                   getLoggingName(),
                   karere::Id(userid).toString().c_str());
    return true;
}

bool Call::handleHello(const Cid_t cid, const unsigned int nAudioTracks, const std::set<karere::Id>& mods,
                       const bool wr, const bool allowed, const bool speakRequest, const sfu::WrUserList& wrUsers, const CallLimits& callLimits)
{
    #ifndef NDEBUG
    // ensures that our sfu protocol version is the latest one defined in karere
    const auto sfuv = mRtc.getMySfuProtoVersion();
    assert(sfuv == sfu::SfuProtocol::SFU_PROTO_PROD
           || (sfuv == sfu::SfuProtocol::SFU_PROTO_V4 && mRtc.isSpeakRequestSupportEnabled()));
    #endif

    // mNumInputAudioTracks & mNumInputAudioTracks are used at createTransceivers after receiving HELLO command
    const auto numInputVideoTracks = mRtc.getNumInputVideoTracks();
    if (!isValidInputVideoTracksLimit(numInputVideoTracks))
    {
        RTCM_LOG_ERROR("%sInvalid number of simultaneus video tracks: %d",
                       getLoggingName(),
                       numInputVideoTracks);
        return false;
    }
    mNumInputVideoTracks = numInputVideoTracks; // Set the maximum number of simultaneous video tracks the call supports

    setSpeakRequest(speakRequest);

    // set call duration limit if any (in seconds)
    mCallLimits = callLimits;
    // Ensure no residual ending time stamp is stored. The correct one (if any) will come in WILL_END
    if (mCallWillEndTs != mega::mega_invalid_timestamp)
    {
        RTCM_LOG_DEBUG("%sResetting mCallWillEndTs to mega_invalid_timestamp upon HELLO command",
                       getLoggingName());
        mCallWillEndTs = mega::mega_invalid_timestamp;
    }

    // Set the maximum number of simultaneous audio tracks the call supports. If no received nAudioTracks or nVideoTracks set as max default
    mNumInputAudioTracks = nAudioTracks ? nAudioTracks : static_cast<uint32_t>(RtcConstant::kMaxCallAudioSenders);

    // copy moderator list, and check if our own user is moderator
    setOwnModerator(mods.find(getOwnPeerId()) != mods.end());
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
        assert(allowed || !isOwnPrivModerator());
        if (allowed)
        {
            setWrJoiningState(sfu::WrState::WR_ALLOWED);
            joinSfu();
        }
        else
        {
            // we must wait in waiting room until a moderator allow to access
            setState(CallState::kInWaitingRoom);
            setWrJoiningState(sfu::WrState::WR_NOT_ALLOWED);
            mCallHandler.onWrDeny(*this);
        }

        return dumpWrUsers(wrUsers, true/*clearCurrent*/);
    }
    return true;
}

bool Call::handleWrDump(const sfu::WrUserList& users)
{
    if (!checkWrCommandReqs("WR_DUMP", true /*mustBeModerator*/))
    {
        return false;
    }
    return dumpWrUsers(users, true/*clearCurrent*/);
}

bool Call::handleWrEnter(const sfu::WrUserList& users)
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
    std::for_each(users.begin(), users.end(), [&uhl](const auto &u) { uhl->addMegaHandle(u.mWrUserid.val); });
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
        RTCM_LOG_ERROR("%sWR_LEAVE : invalid user received", getLoggingName());
        assert(false);
        return false;
    }

    if (!mWaitingRoom)
    {
        RTCM_LOG_WARNING("%sWR_LEAVE : mWaitingRoom is null", getLoggingName());
        assert(false);
        mWaitingRoom.reset(new KarereWaitingRoom()); // instanciate in case it doesn't exists
        return false;
    }

    if (!mWaitingRoom->removeUser(user.val))
    {
        RTCM_LOG_WARNING("%sWR_LEAVE : user not found in waiting room: %s",
                         getLoggingName(),
                         user.toString().c_str());
        return false;
    }

    std::unique_ptr<mega::MegaHandleList> uhl(mega::MegaHandleList::createInstance());
    uhl->addMegaHandle(user.val);
    mCallHandler.onWrUsersLeave(*this, uhl.get());
    return true;
}

bool Call::handleWrAllow(const Cid_t& cid)
{
    if (!checkWrCommandReqs("WR_ALLOW", false /*mustBeModerator*/))
    {
        return false;
    }

    if (cid == K_INVALID_CID)
    {
        RTCM_LOG_ERROR("%sWR_ALLOW: Invalid cid received: %d", getLoggingName(), cid);
        assert(false);
    }

    if (mState != CallState::kInWaitingRoom) { return false; }
    mMyPeer->setCid(cid); // update Cid for own client from SFU
    RTCM_LOG_DEBUG("%shandleWrAllow: we have been allowed to join call, so we need to send JOIN "
                   "command to SFU",
                   getLoggingName());
    setWrJoiningState(sfu::WrState::WR_ALLOWED);
    mCallHandler.onWrAllow(*this);
    joinSfu(); // send JOIN command to SFU
    return true;
}

bool Call::handleWrDeny()
{
    if (!checkWrCommandReqs("WR_DENY", false /*mustBeModerator*/))
    {
        return false;
    }

    if (mState != CallState::kInWaitingRoom)
    {
        return false;
    }

    setWrJoiningState(sfu::WrState::WR_NOT_ALLOWED);
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

bool Call::handleWillEndCommand(const unsigned int endsIn)
{
    SFU_LOG_DEBUG("%d",endsIn);
    mCallWillEndTs = ::mega::m_time(nullptr) + endsIn;
    mCallHandler.onCallWillEndr(*this);
    return true;
}

bool Call::handleClimitsCommand(const sfu::SfuInterface::CallLimits& callLimits)
{
    auto &oldDur = mCallLimits.durationInSecs;
    auto &newDur = callLimits.durationInSecs;
    if (oldDur != ::sfu::kCallLimitDisabled && newDur == ::sfu::kCallLimitDisabled)
    {
        mCallWillEndTs = mega::mega_invalid_timestamp;
        mCallHandler.onCallWillEndr(*this);
    }
    if (mCallLimits != callLimits)
    {
        mCallLimits = callLimits;
        mCallHandler.onCallLimitsUpdated(*this);
    }
    return true;
}

bool Call::handleMutedCommand(const unsigned av, const Cid_t cidPerf)
{
    karere::AvFlags flags(static_cast<uint8_t>(av));
    if (!flags.audioMuted() && !flags.videoMuted())
    {
        SFU_LOG_WARNING("handleMuteCommand: Av flags not expected from SFU for MUTE command: %u", av);
        assert(false);
        return false;
    }
    muteMyClient(flags.audioMuted(), flags.videoMuted(), cidPerf);
    return true;
}

void Call::onSfuDisconnected()
{
    if (!isDestroying())
    {
        // Not necessary to call to orderedCallDisconnect, as we are not connected to SFU
        // disconnect from media channel and clear resources
        mediaChannelDisconnect();
        clearResources(kRtcDisconn);
        setState(CallState::kStateConnecting);
    }
    else
    {
        /* We have received a OP_DELCALLREASON, and we tried to disconnect orderly from SFU, by sending 'BYE' command before removing call,
         * but we have received a sfu socket close, before onSfuDisconnected onSendByeCommand is executed (BYE cannot be sent).
         *
         * As call was marked to be destroyed, we need to remove it.
         */
        auto wptr = weakHandle();
        karere::marshallCall([wptr, this]()
        {
            if (wptr.deleted())
            {
                return;
            }
            removeCallImmediately(mTempEndCallReason, kSigDisconn);
        }, mRtc.getAppCtx());
        return;
    }
}

void Call::immediateCallDisconnect(const TermCode& termCode)
{
    if (isTermCodeRetriable(termCode))
    {
        // if termcode is retriable, a reconnection attempt should be started automatically, so we can't destroy mSfuConnection
        RTCM_LOG_DEBUG("%ssfuDisconnect: can't disconnect from SFU as termcode is retriable %s",
                       getLoggingName(),
                       connectionTermCodeToString(termCode).c_str());
        return;
    }

    if (isDisconnecting())
    {
        // prevents multiple disconnection attempts simultaneously
        RTCM_LOG_DEBUG("%simmediateCallDisconnect call state is %s",
                       getLoggingName(),
                       mState == CallState::kStateDestroyed ? "kStateDestroyed" :
                                                              "kStateTerminatingUserParticipation");
        assert(!mSfuConnection);
        return;
    }

    const bool hadParticipants = !mSessions.empty(); // mSessions is cleared at clearResources
    mediaChannelDisconnect(true /*releaseDevices*/);
    clearResources(termCode);
    disconnectFromSfu(termCode, hadParticipants);
}

void Call::disconnectFromSfu(const TermCode& termCode, bool hadParticipants)
{
    RTCM_LOG_DEBUG("%sdisconnectFromSfu, termcode (%u): %s",
                   getLoggingName(),
                   termCode,
                   connectionTermCodeToString(termCode).c_str());
    const bool wasJoined = mSfuConnection && mSfuConnection->isJoined();
    mTermCode = termCode; // termcode is only valid at state kStateTerminatingUserParticipation

    if (mState > CallState::kStateClientNoParticipating)
    {
        setState(CallState::kStateTerminatingUserParticipation);
    }
    // else => do not set kStateTerminatingUserParticipation as we are not currently connected/connecting

    if (mSfuConnection)
    {
        mSfuClient.closeSfuConnection(mChatid);
        mSfuConnection = nullptr;
    }

    // avoid notifying kStateClientNoParticipating, as call will finally be destroyed
    bool skipNotification = (isDestroying())                    // we are destroying call
                            || (!hadParticipants && wasJoined); // no more participants but still joined to SFU
    if (!skipNotification)
    {
        mTermCode = kInvalidTermCode;
        setState(CallState::kStateClientNoParticipating);
    }
}

void Call::onByeCommandSent()
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
            RTCM_LOG_DEBUG("%sonSendByeCommand: SFU connection no longer exists", getLoggingName());
            return;
        }

        if (isDestroying())
        {
            // We have received a OP_DELCALLREASON, and we tried to disconnect orderly from SFU, by sending 'BYE' command before removing call,
            // Now we have received BYE command delivering notification, so we can remove call
            removeCallImmediately(mTempEndCallReason, mTempTermCode);
        }
        else
        {
            if (mState == CallState::kStateConnecting)
            {
                // we have sent BYE command from onConnectionChange (kDisconnected | kFailed | kClosed)
                // and now we need to force reconnect to SFU
                mSfuConnection->clearCommandsQueue();
                mSfuConnection->retryPendingConnection(true);
            }
            else
            {
                // once we have confirmed that BYE command has been sent, we can proceed with disconnect
                assert (mTempTermCode != kInvalidTermCode);

                // close sfu and media channel connection and clear some call stuff
                immediateCallDisconnect(mTempTermCode);
                mTempTermCode = kInvalidTermCode;
            }
        }
    }, mRtc.getAppCtx());
}

bool Call::processDeny(const std::string& cmd, const std::string& msg)
{
    mCallHandler.onCallDeny(*this, cmd, msg); // notify apps about the denied command

    if (cmd == "audio") // audio ummute has been denied by SFU, disable audio flag local
    {
        muteMyClient(true/*audio*/, false/*video*/);
    }
    else if (cmd == "JOIN")
    {
        if (mState != kStateJoining)
        {
            RTCM_LOG_ERROR(
                "%sDeny 'JOIN' received. Current call state: %u, expected call state: %u. %s",
                getLoggingName(),
                mState,
                kStateJoining,
                msg.c_str());
            return false;
        }
        orderedCallDisconnect(TermCode::kErrGeneral, msg);
    }
    // else => just send callback to apps to inform why their command didn't succeed

    return true;
}

bool Call::error(unsigned int code, const std::string &errMsg)
{
    TermCode connectionTermCode = static_cast<TermCode>(code);
    if (!isValidConnectionTermcode(connectionTermCode))
    {
        RTCM_LOG_ERROR("%sInvalid termCode [%u] received at error command",
                       getLoggingName(),
                       connectionTermCode);
        return false;
    }

    if (isDestroying())
    {
        RTCM_LOG_WARNING("%sSFU error command received [%u], but call is already being destroyed",
                         getLoggingName(),
                         connectionTermCode);
        return true;
    }

    const bool disconnectCall = !isTermCodeRetriable(connectionTermCode) || mParticipants.empty();
    auto wptr = weakHandle();
    karere::marshallCall([wptr, this, connectionTermCode, errMsg, disconnectCall]()
    {
        // error() is called from LibwebsocketsClient::wsCallback() for LWS_CALLBACK_CLIENT_RECEIVE.
        // If disconnect() is called here immediately, it will destroy the LWS client synchronously,
        // leave it in an invalid state (and will crash at Libwebsockets::resetMessage())

        if (wptr.deleted())
        {
            return;
        }

        // send call stats
        if (isConnectedToSfu())
        {
            sendStats(connectionTermCode);
        }

        // notify SFU error to the apps
        std::string errMsgStr = errMsg.empty() || !errMsg.compare("Unknown reason") ? connectionTermCodeToString(connectionTermCode): errMsg;
        mCallHandler.onCallError(*this, static_cast<int>(connectionTermCode), errMsgStr);

        if (disconnectCall)
        {
            // disconnect call just if there are no participants or termcode is not recoverable (we don't need to send BYE command upon SFU error reception)
            // call just can be removed upon OP_DELCALLREASON command received from chatd
            immediateCallDisconnect(connectionTermCode);
        }
    }, mRtc.getAppCtx());

    return true;
}

void Call::logError(const char *error)
{
    RTCM_LOG_ERROR("%sSFU: %s", getLoggingName(), error);
}

void Call::onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> /*stream*/)
{
    if (mState != kStateJoining)
    {
        RTCM_LOG_WARNING("%sonAddStream: get unexpected state", getLoggingName());
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
        RTCM_LOG_WARNING("%sonTrack: get unexpected state", getLoggingName());
        assert(mState != kStateInProgress); // theoretically, it should not happen. If so, it may worth to investigate
        return;
    }

    absl::optional<std::string> mid = transceiver->mid();
    if (mid.has_value())
    {
        std::string value = mid.value();
        if (transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO)
        {
            mReceiverTracks[static_cast<uint32_t>(atoi(value.c_str()))] = std::make_unique<RemoteAudioSlot>(*this, transceiver,
                                                                                                               mRtc.getAppCtx());
        }
        else
        {
            mReceiverTracks[static_cast<uint32_t>(atoi(value.c_str()))] = std::make_unique<RemoteVideoSlot>(*this, transceiver,
                                                                                                               mRtc.getAppCtx());
        }
    }
}

void Call::onRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> /*receiver*/)
{
    RTCM_LOG_DEBUG("%sonRemoveTrack received", getLoggingName());
}

void Call::onConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState)
{
    RTCM_LOG_DEBUG("%sonConnectionChange newstate: %d", getLoggingName(), newState);
    if (!mSfuConnection)
    {
        RTCM_LOG_WARNING("%sonConnectionChange: mSfuConnection no longer exists", getLoggingName());
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
                RTCM_LOG_ERROR("%sonConnectionChange: Not valid SfuConnection upon "
                               "PeerConnectionState kDisconnected received",
                               getLoggingName());
                assert(false);
                return;
            }

            if (isUdpDisconnected()) // lack of UDP connectity detected, disconnect call and don't try to reconnect
            {
                RTCM_LOG_DEBUG("%sWebRTC connection failed, there's no UDP connectivity",
                               getLoggingName());
                orderedCallDisconnect(TermCode::kNoMediaPath, connectionTermCodeToString(TermCode::kNoMediaPath).c_str());
                return;
            }

            if (!isConnectedToSfu())
            {
                setState(CallState::kStateConnecting);
                mSfuConnection->clearCommandsQueue();
                mSfuConnection->retryPendingConnection(true);
            }
            else if (!isSendingBye())    // if we are connected to SFU we need to send BYE command (if we haven't already done)
            {                                                   // don't clear commands queue here, wait for onSendByeCommand
                setState(CallState::kStateConnecting);          // just set kStateConnecting if we have not already sent a previous BYE command, or executed action upon onSendByeCommand won't match with expected one
                sendStats(TermCode::kRtcDisconn);               // send stats if we are connected to SFU regardless termcode
                mSfuConnection->sendBye(TermCode::kRtcDisconn); // once LWS confirms that BYE command has been sent (check processNextCommand) onSendByeCommand will be called
            }
        }
    }
    else if (newState == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    {
        bool reconnect = !isConnectedToSfu();
        RTCM_LOG_DEBUG("%sonConnectionChange retryPendingConnection (reconnect) : %d",
                       getLoggingName(),
                       reconnect);
        mSfuConnection->retryPendingConnection(reconnect);
    }
}

bool Call::addPendingPeer(const Cid_t cid)
{
    if (mPeersVerification.find(cid) != mPeersVerification.end())
    {
        return false;
    }
    mPeersVerification[cid] = promise::Promise<void>();
    return true;
}

void Call::clearPendingPeers()
{
    std::for_each(mPeersVerification.begin(), mPeersVerification.end(), [](auto &it)
    {
        promise::Promise<void>& pms = it.second;
        if (!pms.done()) { pms.reject("Rejecting peer pms upon pms clear"); }
    });
    mPeersVerification.clear();
}

bool Call::removePendingPeer(const Cid_t cid)
{
    auto it = mPeersVerification.find(cid);
    if (it == mPeersVerification.end())
    {
        RTCM_LOG_WARNING(
            "%shandlePeerLeft: peer with cid: %u, is still pending to verify it's ephemeral key",
            getLoggingName());
        return false;
    }

    if (!it->second.done()) { it->second.reject("Rejecting peer pms upon removePendingPeer"); }
    mPeersVerification.erase(it);
    return true;
}

bool Call::isPeerPendingToAdd(const Cid_t cid) const
{
    auto it = mPeersVerification.find(cid);
    return it != mPeersVerification.end() && !it->second.done();
}

bool Call::peerExists(const Cid_t cid) const
{
    return mPeersVerification.find(cid) != mPeersVerification.end();
}

bool Call::fullfilPeerPms(const Cid_t cid, const bool ephemKeyVerified)
{
    auto it = mPeersVerification.find(cid);
    if (it != mPeersVerification.end() && !(it->second.done()))
    {
        if (ephemKeyVerified)
        {
            it->second.resolve();
        }
        else
        {
            it->second.reject("Rejecting peer pms upon fullfilPeerPms");
        }
        return true;
    }
    return false;
}

promise::Promise<void>* Call::getPeerVerificationPms(const Cid_t cid)
{
    auto it = mPeersVerification.find(cid);
    if (it != mPeersVerification.end())
    {
        return &it->second;
    }

    return nullptr;
}

bool Call::addWrUsers(const sfu::WrUserList& users, const bool clearCurrent)
{
    if (!isOwnPrivModerator() && !users.empty())
    {
        RTCM_LOG_ERROR("%saddWrUsers : SFU has sent wr users list to a non-moderator user",
                       getLoggingName());
        mWaitingRoom.reset();
        assert(false);
        return false;
    }

    if (clearCurrent && mWaitingRoom)   { mWaitingRoom->clear(); }
    else if (!mWaitingRoom)             { mWaitingRoom.reset(new KarereWaitingRoom()); }

    std::for_each(users.begin(), users.end(), [this](const auto &u)
    {
        mWaitingRoom->addOrUpdateUserStatus(u.mWrUserid, u.mWrState);
    });
    return true;
}

void Call::pushIntoWr(const TermCode& termCode)
{
    if (isConnectedToSfu())
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

bool Call::dumpWrUsers(const sfu::WrUserList& wrUsers, const bool clearCurrent)
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
        RTCM_LOG_ERROR("%s%s. Waiting room command received for our client with non moderator "
                       "permissions for this call: %s",
                       getLoggingName(),
                       commandStr.c_str(),
                       getCallid().toString().c_str());
        assert(false);
        return false;
    }

    if (!checkWrFlag())
    {
        RTCM_LOG_ERROR("%s%s. Waiting room should be enabled for this call: %s",
                       getLoggingName(),
                       commandStr.c_str(),
                       getCallid().toString().c_str());
        assert(false);
        return false;
    }
    return true;
}

bool Call::manageAllowedDeniedWrUSers(const std::set<karere::Id>& users, bool allow, std::string && commandStr)
{
    // Non-host Users (with standard permissions) can send WR_ALLOW if open invite is enabled, so they can process WR_USERS_ALLOW
    const bool mustBeModerator = !allow;
    if (!checkWrCommandReqs(commandStr.c_str(), mustBeModerator))
    {
        return false;
    }

    if (users.empty())
    {
        RTCM_LOG_ERROR("%s%s : empty user list received", getLoggingName(), commandStr.c_str());
        assert(false);
        return false;
    }

    if (isOwnPrivModerator())
    {
        if (!mWaitingRoom)
        {
            RTCM_LOG_WARNING("%s%s : mWaitingRoom is null", getLoggingName(), commandStr.c_str());
            assert(false);
            mWaitingRoom.reset(new KarereWaitingRoom()); // instanciate in case it doesn't exists
        }

        if (!mWaitingRoom->updateUsers(users, allow ? sfu::WrState::WR_ALLOWED : sfu::WrState::WR_NOT_ALLOWED))
        {
            RTCM_LOG_WARNING("%s%s : could not update users status in waiting room",
                             getLoggingName(),
                             commandStr.c_str());
            return false;
        }
    }

    std::unique_ptr<mega::MegaHandleList> uhl(mega::MegaHandleList::createInstance());
    std::for_each(users.begin(), users.end(), [&uhl](const auto &u) { uhl->addMegaHandle(u.val); });
    allow
        ? mCallHandler.onWrUsersAllow(*this, uhl.get())
        : mCallHandler.onWrUsersDeny(*this, uhl.get());

    return true;
}

bool Call::updateUserModeratorStatus(const karere::Id& userid, const bool enable)
{
    // update call's moderators list
    enable
        ? addToModeratorsList(userid)
        : removeFromModeratorsList(userid);

    if (userid == getOwnPeerId())
    {
        setOwnModerator(enable);
    }

    // for all sessions whose userid matches with received one, update moderator status
    for (auto& it : mPeersVerification)
    {
        promise::Promise<void>* pms = &it.second;
        auto wptr = weakHandle();
        pms->then(
               [this, &cid = it.first, userid, enable, wptr]()
               {
                   if (wptr.deleted())
                   {
                       return;
                   }
                   Session* session = getSession(cid);
                   if (!session || session->getPeerid() != userid)
                   {
                       return;
                   }
                   session->setModerator(enable);
               })
            .fail(
                [&cid = it.first, lname = getLoggingName()](const ::promise::Error&)
                {
                    RTCM_LOG_WARNING("%supdateUserModeratorStatus: PeerVerification promise was "
                                     "rejected for cid: %u",
                                     lname,
                                     cid);
                    return;
                });
    }
    return true;
}

bool Call::updateSpeakersList(const karere::Id& userid, const bool add)
{
    const bool updated = add ? addToSpeakersList(userid) : removeFromSpeakersList(userid);
    if (updated)
    {
        mCallHandler.onUserSpeakStatusUpdate(*this, userid, add);
        return true;
    }
    return false;
}

bool Call::updateSpeakRequestsList(const karere::Id& userid, const bool add)
{
    const bool updated = add ? addToSpeakRequestsList(userid) : removeFromSpeakRequestsList(userid);
    if (updated)
    {
        mCallHandler.onSpeakRequest(*this, userid, add);
        return true;
    }
    return false;
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
            if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V0)
            {
                // encrypt key to participant
                strongvelope::SendKey encryptedKey;
                mSfuClient.getRtcCryptoMeetings()->encryptKeyTo(peer.getPeerid(), *newPlainKey.get(), encryptedKey);
                (*keys)[sessionCid] = mega::Base64::btoa(std::string(encryptedKey.buf(), encryptedKey.size()));
            }
            else if (peer.getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V1)
            {
                // we shouldn't receive any peer with protocol v1
                RTCM_LOG_ERROR("%sgenerateAndSendNewMediaKey: unexpected SFU protocol version [%u] "
                               "for user: %s, cid: %u",
                               getLoggingName(),
                               static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                                   peer.getPeerSfuVersion()),
                               peer.getPeerid().toString().c_str(),
                               peer.getCid());
                assert(false);
                continue;
            }
            else if (peer.getPeerSfuVersion() >= sfu::SfuProtocol::SFU_PROTO_V2)
            {
                if (!sfu::isKnownSfuVersion(peer.getPeerSfuVersion()))
                {
                    // important: upon an unkown peers's SFU protocol version, native client should act as if they are the latest known version
                    RTCM_LOG_WARNING("%sgenerateAndSendNewMediaKey: unknown SFU protocol version "
                                     "[%u] for user: %s, cid: %u",
                                     getLoggingName(),
                                     static_cast<std::underlying_type<sfu::SfuProtocol>::type>(
                                         peer.getPeerSfuVersion()),
                                     peer.getPeerid().toString().c_str(),
                                     peer.getCid());
                }

                auto&& ephemeralPubKey = peer.getEphemeralPubKeyDerived();
                if (ephemeralPubKey.empty())
                {
                    RTCM_LOG_WARNING("%sInvalid ephemeral key for peer: %s cid %u",
                                     getLoggingName(),
                                     peer.getPeerid().toString().c_str(),
                                     sessionCid);
                    assert(false);
                    continue;
                }

                // Encrypt key for participant with its public ephemeral key
                std::string encryptedKey;
                std::string plainKey (newPlainKey->buf(), newPlainKey->bufSize());
                if (!mSymCipher.cbc_encrypt_with_key(plainKey, encryptedKey, reinterpret_cast<const unsigned char *>(ephemeralPubKey.data()), ephemeralPubKey.size(), nullptr))
                {
                    RTCM_LOG_ERROR("%sFailed Media key cbc_encrypt for peerId %s Cid %u",
                                   getLoggingName(),
                                   peer.getPeerid().toString().c_str(),
                                   peer.getCid());
                    continue;
                }

                (*keys)[sessionCid] = mega::Base64::btoa(encryptedKey);
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

            // add key to peer's key map, although is not encrypted for any other participant
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
            RTCM_LOG_ERROR("%sUnknown vtrack mid %u",
                           getLoggingName(),
                           trackDescriptor.second.mMid);
            continue;
        }

        Cid_t cid = trackDescriptor.first;
        uint32_t mid = trackDescriptor.second.mMid;
        RemoteVideoSlot *slot = static_cast<RemoteVideoSlot*>(it->second.get());
        if (slot->getCid() == cid && slot->getVideoResolution() == videoResolution)
        {
            RTCM_LOG_WARNING("%sFollow same cid with same resolution over same track",
                             getLoggingName());
            continue;
        }

        if (slot->getCid() != 0)    // the slot is already in use, need to release first and notify
        {
            if (trackDescriptor.second.mReuse && slot->getCid() != cid)
            {
                RTCM_LOG_ERROR("%sattachSlotToSession: trying to reuse slot, but cid has changed",
                               getLoggingName());
                assert(false && "Possible error at SFU: slot with CID not found");
            }

            RTCM_LOG_DEBUG("%sreassign slot with mid: %u from cid: %u to newcid: %u, reuse: %d ",
                           getLoggingName(),
                           mid,
                           slot->getCid(),
                           cid,
                           trackDescriptor.second.mReuse);

            Session* oldSess = getSession(slot->getCid());
            if (oldSess)
            {
                // In case of Slot reassign for another peer (CID) or same peer (CID) slot reusing, we need to notify app about that
                oldSess->disableVideoSlot(slot->getVideoResolution());
            }
        }

        promise::Promise<void>* pms = getPeerVerificationPms(cid);
        if (!pms)
        {
            RTCM_LOG_WARNING(
                "%shandleIncomingVideo: PeerVerification promise not found for cid: %u",
                getLoggingName(),
                cid);
            return;
        }

        auto wptr = weakHandle();
        slot->setAuxCid(cid);
        pms->then(
               [this, slot, cid, videoResolution, wptr]()
               {
                   if (wptr.deleted())
                   {
                       return;
                   }
                   Session* sess = getSession(cid);
                   if (!sess)
                   {
                       RTCM_LOG_ERROR("%shandleIncomingVideo: session with CID %u not found",
                                      getLoggingName(),
                                      cid);
                       assert(false && "Possible error at SFU: session with CID not found");
                       return;
                   }

                   if (slot->getAuxCid() != K_INVALID_CID && slot->getAuxCid() != cid)
                   {
                       // Race condition, more than one session (still pending to be verified) tried
                       // to attach slot When Pms for this session has been resolved, another
                       // session (still pending to be verified) was trying to attach same slot so
                       // getAuxCid doesn't match with this cid
                       RTCM_LOG_WARNING("%sTemp CID %u doesn't match with this CID: %u",
                                        getLoggingName(),
                                        slot->getAuxCid(),
                                        cid);
                       return;
                   }

                   const std::vector<std::string> ivs = sess->getPeer().getIvs();
                   slot->assignVideoSlot(
                       cid,
                       sfu::Command::hexToBinary(ivs[static_cast<size_t>(videoResolution)]),
                       videoResolution);
                   attachSlotToSession(*sess, slot, false, videoResolution);
               })
            .fail(
                [cid, slot, wptr, lname = getLoggingName()](const ::promise::Error&)
                {
                    if (wptr.deleted())
                    {
                        return;
                    }
                    if (slot->getAuxCid() == cid)
                    {
                        slot->setAuxCid(K_INVALID_CID);
                    }
                    RTCM_LOG_WARNING(
                        "%shandleAvCommand: PeerVerification promise was rejected for cid: %u",
                        lname,
                        cid);
                    return;
                });
    }
}

void Call::attachSlotToSession (Session& session, RemoteSlot* slot, const bool audio, const VideoResolution hiRes)
{
    if (audio)
    {
        session.setAudioSlot(static_cast<RemoteAudioSlot *>(slot));
    }
    else
    {
        hiRes
            ? session.setHiResSlot(static_cast<RemoteVideoSlot *>(slot))
            : session.setVThumSlot(static_cast<RemoteVideoSlot *>(slot));
    }
}

void Call::addSpeaker(const Cid_t cid, const uint32_t amid)
{
    if (cid == K_INVALID_CID)
    {
        RTCM_LOG_WARNING("%sAddSpeaker: invalid Cid received as param", getLoggingName());
        assert(false);
    }

    if (amid == sfu::TrackDescriptor::invalidMid)
    {
        // peer notified as speaker from SFU, but track not provided yet (this happens if peer is muted)
        return;
    }

    auto it = mReceiverTracks.find(amid);
    if (it == mReceiverTracks.end())
    {
        RTCM_LOG_WARNING("%sAddSpeaker: unknown track mid %u", getLoggingName(), amid);
        return;
    }

    RemoteAudioSlot* slot = static_cast<RemoteAudioSlot*>(it->second.get());
    if (slot->getCid() != cid)
    {
        Session* oldSess = getSession(slot->getCid());
        if (oldSess)
        {
            // In case of Slot reassign for another peer (CID) we need to notify app about that
            oldSess->disableAudioSlot();
        }
    }

    promise::Promise<void>* auxpms = getPeerVerificationPms(cid);
    if (!auxpms)
    {
        RTCM_LOG_WARNING("%sAddSpeaker: PeerVerification promise not found for cid: %u",
                         getLoggingName(),
                         cid);
        return;
    }

    auto wptr = weakHandle();
    slot->setAuxCid(cid);
    auxpms
        ->then(
            [this, slot, cid, wptr]()
            {
                if (wptr.deleted())
                {
                    return;
                }
                Session* sess = getSession(cid);
                if (!sess)
                {
                    RTCM_LOG_WARNING("%sAddSpeaker: unknown cid", getLoggingName());
                    return;
                }

                if (slot->getAuxCid() != K_INVALID_CID && slot->getAuxCid() != cid)
                {
                    // Race condition, more than one session (still pending to be verified) tried to
                    // attach slot When Pms for this session has been resolved, another session
                    // (still pending to be verified) was trying to attach same slot so getAuxCid
                    // doesn't match with this cid
                    RTCM_LOG_WARNING("%sTemp CID %u doesn't match with this CID: %u",
                                     getLoggingName(),
                                     slot->getAuxCid(),
                                     cid);
                    return;
                }

                const std::vector<std::string> ivs = sess->getPeer().getIvs();
                assert(ivs.size() >= kAudioTrack);
                slot->assignAudioSlot(
                    cid,
                    sfu::Command::hexToBinary(ivs[static_cast<size_t>(kAudioTrack)]));
                attachSlotToSession(*sess, slot, true, kUndefined);
            })
        .fail(
            [cid, slot, wptr, lname = getLoggingName()](const ::promise::Error&)
            {
                if (wptr.deleted())
                {
                    return;
                }
                if (slot->getAuxCid() == cid)
                {
                    slot->setAuxCid(K_INVALID_CID);
                }
                RTCM_LOG_WARNING(
                    "%shandleKeyCommand: PeerVerification promise was rejected for cid: %u",
                    lname,
                    cid);
                return;
            });
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

void Call::takeCameraDevice()
{
    if (!mCameraManager)
    {
        mRtc.takeCameraDevice();
        mCameraManager = mRtc.getCameraDevice();
    }
}

void Call::releaseCameraDevice()
{
    if (mCameraManager)
    {
        mRtc.releaseCameraDevice();
        mCameraManager = nullptr;
    }
}

bool Call::hasCameraDevice()
{
    return mCameraManager ? true : false;
}

void Call::takeScreenDevice()
{
    if (!mScreenManager)
    {
        mRtc.takeScreenDevice();
        mScreenManager = mRtc.getScreenDevice();
    }
}

void Call::releaseScreenDevice()
{
    if (mScreenManager)
    {
        mRtc.releaseScreenDevice();
        mScreenManager = nullptr;
    }
}

bool Call::hasScreenDevice()
{
    return mScreenManager ? true : false;
}

void Call::freeLocalVideoTracks()
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
    mStats.mSamples.mQ.push_back(static_cast<int32_t>(mSvcDriver.mCurrentSvcLayerIndex) |
                                 static_cast<int32_t>(RtcConstant::kTxSpatialLayerCount) << 8);
    mStats.mSamples.mNrxa.push_back(audioSession);
    mStats.mSamples.mNrxl.push_back(vThumbSession);
    mStats.mSamples.mNrxh.push_back(hiResSession);
    mStats.mSamples.mAv.push_back(getLocalAvFlags().value());
}

void Call::initStatsValues()
{
    mStats.mPeerId = getOwnPeerId();
    mStats.mCallid = mCallid;
    mStats.mIsGroup = mIsGroup;
    mStats.mDevice = mRtc.getDeviceInfo();
    mStats.mSfuHost = mSfuConnection->getSfuUrl().host;
}

void Call::enableStats()
{
    mStats.mSfuProtoVersion = static_cast<uint32_t>(mRtc.getMySfuProtoVersion());
    mStats.mCid = getOwnCid();
    mStats.mTimeOffset = getJoinOffset();
    auto wptr = weakHandle();
    mStatsTimer = karere::setInterval([this, wptr]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (!mSfuConnection || !mSfuConnection->isJoined())
        {
            RTCM_LOG_WARNING("%sCannot collect stats until reach kJoined state", getLoggingName());
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
    mIsDestroyingCall = isDestroying;
    if (mSfuConnection)
    {
        mSfuConnection->setAvoidReconnect(isDestroying);
    }
}

bool Call::isDestroying()
{
    return mIsDestroyingCall;
}

bool Call::isDisconnecting()
{
    return mState > CallState::kStateInProgress;
}

void Call::generateEphemeralKeyPair()
{
    mEphemeralKeyPair.reset(new mega::ECDH());
}

const mega::ECDH* Call::getMyEphemeralKeyPair() const
{
    return mEphemeralKeyPair.get();
}

void Call::muteMyClient(const bool audio, const bool video, const Cid_t cidPerf)
{
    karere::AvFlags currentFlags = getLocalAvFlags();
    if (audio)
    {
        currentFlags.remove(karere::AvFlags::kAudio);
        mMyPeer->setAvFlags(currentFlags);
        updateAudioTracks();
    }

    if (video)
    {
        currentFlags.remove(karere::AvFlags::kVideo);
        mMyPeer->setAvFlags(currentFlags);
        updateVideoTracks();
    }

    mCallHandler.onLocalFlagsChanged(*this, cidPerf);  // notify app local AvFlags Change
}

void Call::addPeer(sfu::Peer& peer, const std::string& ephemeralPubKeyDerived)
{
    if (!isPeerPendingToAdd(peer.getCid()))
    {
        // we could have received a PEERLEFT before peer's ephemeral key is verified
        RTCM_LOG_WARNING("%saddPeer: Unexpected peer state at mPeersVerification. Cid: %u",
                         getLoggingName(),
                         peer.getCid());
        return;
    }
    const bool ephemKeyVerified = peer.setEphemeralPubKeyDerived(ephemeralPubKeyDerived);
    mSessions[peer.getCid()] = std::make_unique<Session>(peer);

    /* We need to call onNewSession (as we set setSessionHandler there) before calling verifyPeer,
     * otherwise after calling verifyPeer, threads waiting in then blocks, will execute code in lambdas
     * and any call to mSessionHandler will fail as it's still unregistered
     */
    mCallHandler.onNewSession(*mSessions[peer.getCid()], *this);

    // Peer Verification pms must exists at this point. If ephemeral key could be verified and it's valid,
    // we'll resolve pms, otherwise we'll reject it.
    const bool res = fullfilPeerPms(peer.getCid(), ephemKeyVerified);
    RTCM_LOG_WARNING("%saddPeer: peer verification finished %s. Cid: %u",
                     getLoggingName(),
                     ephemKeyVerified && res ? "ok" : "with error",
                     peer.getCid());
    assert(res);
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
    const bool isOnHold = getLocalAvFlags().isOnHold();
    const bool hasCameraFlags = getLocalAvFlags().camera();
    const bool hasScreenFlags = getLocalAvFlags().screenShare();

    if ((!hasCameraFlags && !hasScreenFlags) || isOnHold)
    {
        freeLocalVideoTracks();
        releaseCameraDevice();
        releaseScreenDevice();
        return;
    }

    hasCameraFlags
        ? takeCameraDevice()
        : releaseCameraDevice();

    hasScreenFlags
        ? takeScreenDevice()
        : releaseScreenDevice();

    if (mHiRes)
    {
        if (!mHiResActive)
        {
            // if there is a track, but none in the call has requested hi res video, disable the track
            mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
        }
        else
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            if (hasScreenFlags) // not matter if also camera is enabled (screen = hi-res | camera = lowres)
            {
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getScreenDevice()->getVideoTrackSource());
            }
            else if (hasCameraFlags) // only camera flags enabled
            {
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getCameraDevice()->getVideoTrackSource());
            }
            else
            {
                RTCM_LOG_WARNING("%supdateVideoTracks(mHiRes): unexpected video flags: %u",
                                 getLoggingName(),
                                 getLocalAvFlags().value());
                assert(false);
            }
            mHiRes->getTransceiver()->sender()->SetTrack(videoTrack.get());
        }
    }

    if (mVThumb)
    {
        if (!mVThumbActive)
        {
            // if there is a track, but none in the call has requested low res video, disable the track
            mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
        }
        else
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            if (hasCameraFlags)  // not matter if also screen is enabled (screen = hi-res | camera = lowres)
            {
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getCameraDevice()->getVideoTrackSource());
            }
            else if (hasScreenFlags) // only screen flags enabled
            {
                videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getScreenDevice()->getVideoTrackSource());
            }
            else
            {
                RTCM_LOG_WARNING("%supdateVideoTracks(vThumb): unexpected video flags: %u",
                                 getLoggingName(),
                                 getLocalAvFlags().value());
                assert(false);
            }
            mVThumb->getTransceiver()->sender()->SetTrack(videoTrack.get());
        }
    }
}

void Call::updateNetworkQuality(int networkQuality)
{
    if (networkQuality == mNetworkQuality)
    {
        return;
    }

    RTCM_LOG_WARNING("%supdateNetworkQuality: %s network quality detected",
                     getLoggingName(),
                     networkQuality == kNetworkQualityBad ? "Bad" : "Good");
    mNetworkQuality = networkQuality;
    mCallHandler.onNetworkQualityChanged(*this);
}

void Call::adjustSvcByStats()
{
    if (mStats.mSamples.mRoundTripTime.empty())
    {
        RTCM_LOG_WARNING("%sadjustSvcBystats: not enough collected data", getLoggingName());
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

    bool audio = hasUserSpeakPermission(mMyPeer->getPeerid()) && getLocalAvFlags().audio();
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
    : mCallHandler(callhandler)
    , mMegaApi(megaApi)
    , mDnsCache(dnsCache)
    , mCameraVideoSink(appCtx, *this)
    , mScreenVideoSink(appCtx, *this)
{
    mAppCtx = appCtx;

    mSfuClient = std::make_unique<sfu::SfuClient>(websocketIO, appCtx, rRtcCryptoMeetings);
    if (!artc::isInitialized())
    {
        //rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
        artc::init(appCtx);
        RTCM_LOG_DEBUG("%sWebRTC stack initialized before first use", getLoggingName());
    }

    mSelectedCameraDeviceId = getDefaultCameraDeviceId();
    mSelectedScreenDeviceId = getDefaultScreenDeviceId();
}

std::optional<std::string> RtcModuleSfu::getDefaultCameraDeviceId()
{
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoCapturerManager::getCameraDevices();
    if (videoDevices.empty())
    {
        return std::nullopt;
    }
    return videoDevices.begin()->second;
}

std::optional<long int> RtcModuleSfu::getDefaultScreenDeviceId()
{
    auto screenDevices = artc::VideoCapturerManager::getScreenDevices();
    if (screenDevices.empty())
    {
        return std::nullopt;
    }
    return screenDevices.begin()->second;
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


template<typename T>
static std::optional<T> findAndSetDeviceId(const std::set<std::pair<std::string, T>>& devices,
                               const std::string& deviceName)
{
    if (auto it = std::find_if(devices.begin(),
                               devices.end(),
                               [&deviceName](const auto& p)
                               {
                                   return p.first == deviceName;
                               });
        it != devices.end())
    {
        return it->second;
    }
    return std::nullopt;
}

bool RtcModuleSfu::setVideoCapturerInDevice(const std::string &device, const int type)
{
    if (!checkValidDevType(type))
    {
        RTCM_LOG_WARNING("%ssetVideoCapturerInDevice: invalid input type: %d",
                         getLoggingName(),
                         type);
        assert(false);
        return false;
    }

    std::vector<Call*> calls;
    bool shouldOpen = false;
    RtcDevType t = static_cast<RtcDevType>(type);
    if (t == RtcDevType::TYPE_CAPTURER_CAMERA)
    {
        auto deviceId = findAndSetDeviceId(artc::VideoCapturerManager::getCameraDevices(), device);
        if (!deviceId)
        {
            RTCM_LOG_WARNING(
                "%ssetVideoCapturerInDevice: device: %s of type camera could not be found",
                getLoggingName(),
                device.c_str());
            assert(false);
            return false;
        }

        for (auto& [callId, callPtr] : mCalls)
        {
            if (callPtr->hasCameraDevice())
            {
                calls.push_back(callPtr.get());
                callPtr->freeLocalVideoTracks();
                callPtr->releaseCameraDevice();
                shouldOpen = true;
            }
        }
        changeCameraDevice(*deviceId, shouldOpen);
    }
    else if (t == RtcDevType::TYPE_CAPTURER_SCREEN)
    {
        auto deviceId = findAndSetDeviceId(artc::VideoCapturerManager::getScreenDevices(), device);
        if (!deviceId)
        {
            RTCM_LOG_WARNING(
                "%ssetVideoCapturerInDevice: device: %s of type screen could not be found",
                getLoggingName(),
                device.c_str());
            assert(false);
            return false;
        }

        for (auto& [callId, callPtr] : mCalls)
        {
            if (callPtr->hasScreenDevice())
            {
                calls.push_back(callPtr.get());
                callPtr->freeLocalVideoTracks();
                callPtr->releaseScreenDevice();
                shouldOpen = true;
            }
        }
        changeScreenDevice(*deviceId, shouldOpen);
    }

    for (auto& call : calls)
    {
        call->updateVideoTracks();
    }

    return true;
}

std::string RtcModuleSfu::getVideoDeviceNameById(const std::string& id)
{
    const std::set<std::pair<std::string, std::string>> d =
        artc::VideoCapturerManager::getCameraDevices();
    auto it = std::find_if(d.begin(),
                           d.end(),
                           [&id](const auto& pair)
                           {
                               return pair.second == id;
                           });
    return it != d.end() ? it->first : std::string();
}

std::string RtcModuleSfu::getScreenDeviceNameById(const long int id)
{
    std::set<std::pair<std::string, long int>> d = mCameraCapturerDevice->getScreenDevices();
    auto it = std::find_if(d.begin(),
                           d.end(),
                           [&id](const auto& pair)
                           {
                               return pair.second == id;
                           });
    return it != d.end() ? it->first : std::string();
}

void RtcModuleSfu::getVideoInDevices(std::set<std::string> &devicesVector)
{
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoCapturerManager::getCameraDevices();
    for (auto it = videoDevices.begin(); it != videoDevices.end(); it++)
    {
        devicesVector.insert(it->first);
    }
}

std::set<std::pair<std::string, long int>> RtcModuleSfu::getScreenDevices()
{
    return mCameraCapturerDevice->getScreenDevices();
}

promise::Promise<void> RtcModuleSfu::startCall(const karere::Id &chatid, karere::AvFlags avFlags, bool isGroup, const bool notRinging, std::shared_ptr<std::string> unifiedKey)
{
    if (!isValidInputVideoTracksLimit(mRtcNumInputVideoTracks))
    {
        const std::string errMsg = "startCall: Invalid value for simultaneous input video tracks";
        RTCM_LOG_WARNING("%s%s", getLoggingName(), errMsg.c_str());
        assert(false);
        return promise::Error(errMsg);
    }

    // add chatid to CallsAttempts to avoid multiple start call attempts
    mCallStartAttempts.insert(chatid);

    // we need a temp string to avoid issues with lambda shared pointer capture
    std::string auxCallKey = unifiedKey ? (*unifiedKey.get()) : std::string();
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::startChatCall, chatid, notRinging)
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
            mCalls[callid] = std::make_unique<Call>(callid, chatid, myUserHandle, false, mCallHandler, mMegaApi, (*this), isGroup, sharedUnifiedKey, avFlags, true);

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

void RtcModuleSfu::takeCameraDevice()
{
    if (!mCameraDeviceTakenCount)
    {
        openCameraDevice();
        mCameraDeviceTakenCount++;
    }
}

void RtcModuleSfu::releaseCameraDevice()
{
    if (mCameraDeviceTakenCount > 0)
    {
        mCameraDeviceTakenCount--;
        if (mCameraDeviceTakenCount == 0)
        {
            assert(mCameraCapturerDevice);
            closeCameraDevice();
        }
    }
}

void RtcModuleSfu::takeScreenDevice()
{
    if (!mScreenDeviceTakenCount)
    {
        openScreenDevice();
        mScreenDeviceTakenCount++;
    }
}

void RtcModuleSfu::releaseScreenDevice()
{
    if (mScreenDeviceTakenCount > 0)
    {
        mScreenDeviceTakenCount--;
        if (mScreenDeviceTakenCount == 0)
        {
            assert(mScreenCapturerDevice);
            closeScreenDevice();
        }
    }
}

bool RtcModuleSfu::hasLocalCameraRenderer(const karere::Id &chatid) const
{
    auto it = mCameraVideoSink.mRenderers.find(chatid);
    return it != mCameraVideoSink.mRenderers.end() && it->second;
}

bool RtcModuleSfu::hasLocalScreenRenderer(const karere::Id &chatid) const
{
    auto it = mScreenVideoSink.mRenderers.find(chatid);
    return it != mScreenVideoSink.mRenderers.end() && it->second;
}

void RtcModuleSfu::addLocalCameraRenderer(const karere::Id &chatid, IVideoRenderer *videoRederer)
{
    mCameraVideoSink.mRenderers[chatid] = std::unique_ptr<IVideoRenderer>(videoRederer);
}

void RtcModuleSfu::removeLocalCameraRenderer(const karere::Id &chatid)
{
    mCameraVideoSink.mRenderers.erase(chatid);
}

void RtcModuleSfu::addLocalScreenRenderer(const karere::Id &chatid, IVideoRenderer *videoRederer)
{
    mScreenVideoSink.mRenderers[chatid] = std::unique_ptr<IVideoRenderer>(videoRederer);
}

void RtcModuleSfu::removeLocalScreenRenderer(const karere::Id &chatid)
{
    mScreenVideoSink.mRenderers.erase(chatid);
}

void RtcModuleSfu::onMediaKeyDecryptionFailed(const std::string& err)
{
    mMegaApi.callIgnoreResult(&::mega::MegaApi::sendEvent,
                              99017,
                              err.c_str(),
                              false,
                              static_cast<const char*>(nullptr));
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

const std::optional<std::string>& RtcModuleSfu::getCameraDeviceIdSelected() const
{
    return mSelectedCameraDeviceId;
}

const std::optional<long int>& RtcModuleSfu::getScreenDeviceIdSelected() const
{
    return mSelectedScreenDeviceId;
}

sfu::SfuClient& RtcModuleSfu::getSfuClient()
{
    return (*mSfuClient.get());
}

DNScache& RtcModuleSfu::getDnsCache()
{
    return mDnsCache;
}

void RtcModuleSfu::rtcOrderedCallDisconnect(rtcModule::ICall* iCall, TermCode connectionTermCode)
{
    Call* call = static_cast<Call*>(iCall);
    if (!call)
    {
        RTCM_LOG_WARNING("%srtcOrderedCallDisconnect: call no longer exists", getLoggingName());
        return;
    }

    RTCM_LOG_DEBUG("%srtcOrderedCallDisconnect: Ordered removing call with callid: %s",
                   getLoggingName(),
                   call->getCallid().toString().c_str());
    call->orderedCallDisconnect(connectionTermCode, call->connectionTermCodeToString(connectionTermCode).c_str());
}

void RtcModuleSfu::onDestroyCall(rtcModule::ICall* iCall, EndCallReason reason, TermCode connectionTermCode)
{
    Call* call = static_cast<Call*>(iCall);
    if (!call)
    {
        RTCM_LOG_WARNING("%sorderedDisconnectAndCallRemove: call no longer exists",
                         getLoggingName());
        return;
    }

    if (call->isDestroying())
    {
        RTCM_LOG_WARNING("%sorderedDisconnectAndCallRemove: call is already being destroyed",
                         getLoggingName());
        return;
    }

    // by setting this flag true, we prevent more than once call destruction attempt
    call->setDestroying(true);

    // set temporary endCall reason in case immediateRemoveCall is not called immediately (i.e if we first need to send BYE command)
    call->setTempEndCallReason(reason);

    // we are going to destroy call, but in case we are still connected to SFU we need to send BYE command
    call->isConnectedToSfu()
        ? call->orderedCallDisconnect(connectionTermCode, call->connectionTermCodeToString(connectionTermCode).c_str(), true /*forceDisconnect*/)
        : call->removeCallImmediately(reason, connectionTermCode);
}

void RtcModuleSfu::deleteCall(const karere::Id& callId)
{
    mCalls.erase(callId);
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
    mCalls[callid] = std::make_unique<Call>(callid, chatid, callerid, isRinging, mCallHandler, mMegaApi, (*this), isGroup, callKey);
    mCalls[callid]->setState(kStateClientNoParticipating);
}

bool KarereWaitingRoom::updateUsers(const std::set<karere::Id>& users, const sfu::WrState status)
{
    if (!isValidWrStatus(status) || users.empty())
    {
        return false;
    }

    std::for_each(users.begin(), users.end(), [this, &status](const auto &u)
    {
        bool found = false;
        for (auto it = mWaitingRoomUsers.begin(); it != mWaitingRoomUsers.end(); ++it)
        {
            if (it->mWrUserid == u.val)
            {
                it->mWrState = status;
                found = true;
                break;
            }
        }

        if (!found)
        {
            RTCM_LOG_WARNING("KarereWaitingRoom::updateUsers - skip user(%s) status update, as it has not been found in waiting room. "
                             "Note: a moderator could send WR_ALLOW/DENY for an user that dind't answered call yet.",
                             karere::Id(u.val).toString().c_str());
        }
    });

    return true;
}

int KarereWaitingRoom::getUserStatus(const uint64_t& userid) const
{
    for (auto it = mWaitingRoomUsers.begin(); it != mWaitingRoomUsers.end(); ++it)
    {
        if (it->mWrUserid == userid)
        {
            return static_cast<int>(it->mWrState);
        }
    }

    return static_cast<int>(sfu::WrState::WR_UNKNOWN);
}

std::vector<uint64_t> KarereWaitingRoom::getUsers() const
{
    std::vector<uint64_t> users;
    users.reserve(size());
    std::for_each(mWaitingRoomUsers.begin(), mWaitingRoomUsers.end(), [&users](const auto &u)
    {
        users.emplace_back(u.mWrUserid);
    });
    return users;
}

void RtcCameraVideoSink::OnFrame(const webrtc::VideoFrame &frame)
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
            ICall* call = mModuleSfu.findCallByChatid(render.first);
            if ((call && call->getLocalAvFlags().camera() && !call->getLocalAvFlags().has(karere::AvFlags::kOnHold)) || !call)
            {
                processFrame(frame, render.second, VideoSink::Rtc_Type_Video_source_Local_Camera);
            }
        }
    }, mAppCtx);

}

void RtcScreenVideoSink::OnFrame(const webrtc::VideoFrame &frame)
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
            ICall* call = mModuleSfu.findCallByChatid(render.first);
            if ((call && call->getLocalAvFlags().screenShare() && !call->getLocalAvFlags().has(karere::AvFlags::kOnHold)) || !call)
            {
               processFrame(frame, render.second, VideoSink::Rtc_Type_Video_source_Local_Screen);
            }
        }
    }, mAppCtx);
}

artc::VideoCapturerManager *RtcModuleSfu::getCameraDevice()
{
    return mCameraCapturerDevice.get();
}

artc::VideoCapturerManager *RtcModuleSfu::getScreenDevice()
{
    return mScreenCapturerDevice.get();
}

void RtcModuleSfu::changeCameraDevice(const std::string& deviceId, bool shouldOpen)
{
    mSelectedCameraDeviceId = deviceId;
    if (mCameraCapturerDevice)
    {
        shouldOpen = true;
        closeCameraDevice();
    }
    if (shouldOpen)
    {
        openCameraDevice();
        mCameraDeviceTakenCount++;
    }
}

void RtcModuleSfu::changeScreenDevice(const long int deviceId, bool shouldOpen)
{
    mSelectedScreenDeviceId = deviceId;
    if (mScreenCapturerDevice)
    {
        shouldOpen = true;
        closeScreenDevice();
    }
    if (shouldOpen)
    {
        openScreenDevice();
        mScreenDeviceTakenCount++;
    }
}

void RtcModuleSfu::openCameraDevice()
{
    if (!mSelectedCameraDeviceId)
    {
        RTCM_LOG_WARNING("%sopenCameraDevice: default camera in device is not set",
                         getLoggingName());
#ifndef TARGET_OS_SIMULATOR
        // it's expected to not have a camera device in the simulator but we do not want to crash here so that automated tests do not stop
        assert(false);
#endif
        // Try to get default (it already set in the constructor but just in case)
        mSelectedCameraDeviceId = getDefaultCameraDeviceId();
        // If not present now, return
        if (!mSelectedCameraDeviceId)
        {
            RTCM_LOG_WARNING("%sopenCameraDevice: no camera devices available", getLoggingName());
            return;
        }
    }

    webrtc::VideoCaptureCapability capabilities;
    capabilities.width = RtcConstant::kHiResWidth;
    capabilities.height = RtcConstant::kHiResHeight;
    capabilities.maxFPS = RtcConstant::kHiResMaxFPS;

    mCameraCapturerDevice =
        artc::VideoCapturerManager::createCameraCapturer(capabilities,
                                                         *mSelectedCameraDeviceId,
                                                         artc::gWorkerThread.get());
    mCameraCapturerDevice->openDevice(*mSelectedCameraDeviceId);
    mCameraCapturerDevice->AddOrUpdateSink(&mCameraVideoSink, {});
}

void RtcModuleSfu::closeCameraDevice()
{
    if (mCameraCapturerDevice)
    {
        mCameraCapturerDevice->RemoveSink(&mCameraVideoSink);
        mCameraCapturerDevice->releaseDevice();
        mCameraCapturerDevice = nullptr;
    }
}

void RtcModuleSfu::openScreenDevice()
{
    if (!mSelectedScreenDeviceId)
    {
        RTCM_LOG_WARNING("%sopenScreenDevice: default screen in device is not set",
                         getLoggingName());
        // Try to get default (it already set in the constructor but just in case)
        mSelectedScreenDeviceId = getDefaultScreenDeviceId();
        // If not present now, return
        if (!mSelectedScreenDeviceId)
        {
            RTCM_LOG_WARNING("%sopenScreenDevice: no screen devices available", getLoggingName());
            return;
        }
    }

    webrtc::VideoCaptureCapability capabilities;
    capabilities.width = RtcConstant::kHiResWidth;
    capabilities.height = RtcConstant::kHiResHeight;
    capabilities.maxFPS = RtcConstant::kHiResMaxFPS;

    mScreenCapturerDevice =
        artc::VideoCapturerManager::createScreenCapturer(capabilities,
                                                         *mSelectedScreenDeviceId,
                                                         artc::gWorkerThread.get());
    mScreenCapturerDevice->openDevice(std::string());
    mScreenCapturerDevice->AddOrUpdateSink(&mScreenVideoSink, {});
}

void RtcModuleSfu::closeScreenDevice()
{
    if (mScreenCapturerDevice)
    {
        mScreenCapturerDevice->RemoveSink(&mScreenVideoSink);
        mScreenCapturerDevice->releaseDevice();
        mScreenCapturerDevice = nullptr;
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

sfu::SfuProtocol RtcModuleSfu::getMySfuProtoVersion() const
{
    return mMySfuProtoVersion;
}

bool RtcModuleSfu::isSpeakRequestSupportEnabled() const
{
    return mIsSpeakRequestEnabled;
}

void RtcModuleSfu::enableSpeakRequestSupportForCalls(const bool enable)
{
    mIsSpeakRequestEnabled = enable;
    mMySfuProtoVersion = enable
                            ? sfu::SfuProtocol::SFU_PROTO_V4
                            : sfu::SfuProtocol::SFU_PROTO_PROD;
}

void RtcModuleSfu::setNumInputVideoTracks(const unsigned int numInputVideoTracks)
{
    if (!isValidInputVideoTracksLimit(mRtcNumInputVideoTracks))
    {
        RTCM_LOG_WARNING(
            "%ssetNumInputVideoTracks: Invalid value for simultaneous input video tracks",
            getLoggingName());
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
        RTCM_LOG_WARNING("%sWe have received a transceiver without 'mid'", mCall.getLoggingName());
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
    setAuxCid(K_INVALID_CID);
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

void VideoSink::processFrame(const webrtc::VideoFrame& frame,
                             const std::unique_ptr<IVideoRenderer>& render,
                             const int sourceType)
{
    if (!render)
    {
        RTCM_LOG_WARNING("processFrame: Invalid input params");
        return;
    }

    assert(render != nullptr);
    void* userData = NULL;
    auto buffer = frame.video_frame_buffer()->ToI420(); // smart ptr type changed
    if (frame.rotation() != webrtc::kVideoRotation_0)
    {
        buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
    }
    unsigned short width = (unsigned short)buffer->width();
    unsigned short height = (unsigned short)buffer->height();
    void* frameBuf = render->getImageBuffer(width, height, sourceType, userData);
    if (!frameBuf) // image is frozen or app is minimized/covered
    {
        return;
    }
    libyuv::I420ToABGR(buffer->DataY(),
                       buffer->StrideY(),
                       buffer->DataU(),
                       buffer->StrideU(),
                       buffer->DataV(),
                       buffer->StrideV(),
                       (uint8_t*)frameBuf,
                       width * 4,
                       width,
                       height);

    render->frameComplete(userData);
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
            processFrame(frame, mRenderer, VideoSink::Rtc_Type_Video_source_Remote);
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

bool RemoteAudioSlot::enableAudioMonitor(const bool enable)
{
    if (enable == mAudioLevelMonitorEnabled)
    {
        RTCM_LOG_DEBUG("%senableAudioMonitor: audio level monitor already %s",
                       mCall.getLoggingName(),
                       enable ? " enabled " : "disabled");
        return true;
    }

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> mediaTrack = mTransceiver->receiver()->track();
    webrtc::AudioTrackInterface* audioTrack = static_cast<webrtc::AudioTrackInterface*>(mediaTrack.get());
    if (!audioTrack)
    {
        RTCM_LOG_WARNING("%senableAudioMonitor: non valid audiotrack", mCall.getLoggingName());
        assert(false);
        return false;
    }

    if (!mAudioLevelMonitor)
    {
        RTCM_LOG_WARNING("%senableAudioMonitor: AudioMonitor is null", mCall.getLoggingName());
        assert(false);
        return false;
    }

    if (enable)
    {
        mAudioLevelMonitorEnabled = true;
        mAudioLevelMonitor->onAudioDetected(false);
        audioTrack->AddSink(mAudioLevelMonitor.get());     // enable AudioLevelMonitor for remote audio detection
    }
    else
    {
        mAudioLevelMonitorEnabled = false;
        mAudioLevelMonitor->onAudioDetected(false);
        audioTrack->RemoveSink(mAudioLevelMonitor.get()); // disable AudioLevelMonitor
    }
    return true;
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
    if (mAudioDetected == audioDetected)
    {
        return;
    }
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
}

void Session::addKey(Keyid_t keyid, const std::string &key)
{
    mPeer.addKey(keyid, key);
}

void Session::setRemoteAvFlags(karere::AvFlags flags)
{
    assert(mSessionHandler);
    if (flags == mPeer.getAvFlags())
    {
        RTCM_LOG_WARNING("setAvFlags: remote AV flags has not changed");
        return;
    }

    const karere::AvFlags oldFlags = mPeer.getAvFlags();
    karere::AvFlags auxFlags = flags;
    mPeer.setAvFlags(flags); // store received flags

    const bool recordingChanged = oldFlags.isRecording() != flags.isRecording();
    const bool onHoldChanged = oldFlags.isOnHold() != flags.isOnHold();

    // Remove onHold and isRecording flags to check if Av flags have changed
    auxFlags.setOnHold(oldFlags.isOnHold());
    auxFlags.setRecording(oldFlags.isRecording());
    const bool onAvChanged = auxFlags != oldFlags;

    if (recordingChanged)   { mSessionHandler->onRecordingChanged(*this); }
    if (onHoldChanged)      { mSessionHandler->onOnHold(*this); }
    if (onAvChanged)        { mSessionHandler->onRemoteFlagsChanged(*this); }
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
    if (mPeer.isModerator() == isModerator) { return; }
    mPeer.setModerator(isModerator);
    mSessionHandler->onPermissionsChanged(*this);
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
    // Not required to wait for PeerVerification promise.
    // To enable audio level monitor for a remote audio slot, the session must exist.
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

    // Not required to wait for PeerVerification promise.
    // To enable audio level monitor for a remote audio slot, the session must exist.
    Session* sess = mCall.getSession(static_cast<Cid_t>(mCid));
    if (sess)
    {
        sess->setAudioDetected(mAudioDetected);
    }
    else
    {
        RTCM_LOG_WARNING("%sAudioLevelMonitor::onAudioDetected: session with Cid: %u not found",
                         mCall.getLoggingName(),
                         mCid);
    }
}
}
#endif
