#include <mega/types.h>
#include <mega/base64.h>
#include <rtcmPrivate.h>
#include <webrtcPrivate.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>

namespace rtcModule
{

AvailableTracks::AvailableTracks()
{
}

AvailableTracks::~AvailableTracks()
{
}

bool AvailableTracks::hasHiresTrack(Cid_t cid)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return false;
    }
    return tracksFlags.videoHiRes(); // kHiResVideo => (camera and screen)
}

bool AvailableTracks::hasLowresTrack(Cid_t cid)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return false;
    }
    return tracksFlags.videoLowRes();  // kLowResVideo => (camera and screen)
}

bool AvailableTracks::hasVoiceTrack(Cid_t cid)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return false;
    }
    return tracksFlags.has(karere::AvFlags::kAudio);
}

void AvailableTracks::updateHiresTrack(Cid_t cid, bool add)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return;
    }
    add
        ? tracksFlags.add(karere::AvFlags::kHiResVideo)
        : tracksFlags.remove(karere::AvFlags::kHiResVideo);
}

void AvailableTracks::updateLowresTrack(Cid_t cid, bool add)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return;
    }
    add
        ? tracksFlags.add(karere::AvFlags::kLowResVideo)
        : tracksFlags.remove(karere::AvFlags::kLowResVideo);
}

void AvailableTracks::updateSpeakTrack(Cid_t cid, bool add)
{
    karere::AvFlags tracksFlags;
    if (!getTracksByCid(cid, tracksFlags))
    {
        return;
    }
    add
        ? tracksFlags.add(karere::AvFlags::kAudio)
        : tracksFlags.remove(karere::AvFlags::kAudio);
}

bool AvailableTracks::getTracksByCid(Cid_t cid, karere::AvFlags& tracksFlags)
{
    if (!hasCid(cid))
    {
        return false;
    }
    tracksFlags = mTracksFlags[cid];
    return true;
}

void AvailableTracks::addCid(Cid_t cid)
{
    if (!hasCid(cid))
    {
        mTracksFlags[cid] = 0;
    }
}

void AvailableTracks::removeCid(Cid_t cid)
{
    mTracksFlags.erase(cid);
}

bool AvailableTracks::hasCid(Cid_t cid)
{
    return (mTracksFlags.find(cid) != mTracksFlags.end());
}

void AvailableTracks::clear()
{
    mTracksFlags.clear();
}

std::map<Cid_t, karere::AvFlags>& AvailableTracks::getTracks()
{
    return mTracksFlags;
}

Call::Call(karere::Id callid, karere::Id chatid, karere::Id callerid, bool isRinging, IGlobalCallHandler &globalCallHandler, MyMegaApi& megaApi, RtcModuleSfu& rtc, bool isGroup, std::shared_ptr<std::string> callKey, karere::AvFlags avflags)
    : mCallid(callid)
    , mChatid(chatid)
    , mCallerId(callerid)
    , mIsRinging(isRinging)
    , mLocalAvFlags(avflags)
    , mIsGroup(isGroup)
    , mGlobalCallHandler(globalCallHandler)
    , mMegaApi(megaApi)
    , mSfuClient(rtc.getSfuClient())
    , mAvailableTracks(mega::make_unique<AvailableTracks>())
    , mMyPeer()
    , mCallKey(callKey ? *callKey : std::string())
    , mRtc(rtc)
{
    // notify the IGlobalCallHandler (intermediate layer), which register the listener
    // CallHandler to receive notifications about the call
    mGlobalCallHandler.onNewCall(*this);

    setState(kStateInitial); // call after onNewCall, otherwise callhandler didn't exists
}

Call::~Call()
{
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

bool Call::isAudioDetected() const
{
    return mAudioDetected;
}

void Call::setState(CallState newState)
{
    RTCM_LOG_DEBUG("Call state changed. ChatId: %s, callid: %s, state: %s --> %s",
                 karere::Id(getChatid()).toString().c_str(),
                 karere::Id(getCallid()).toString().c_str(),
                 Call::stateToStr(mState),
                 Call::stateToStr(newState));

    if (newState == CallState::kStateInProgress)
    {
        // initial ts is set when user has joined to the call
        mInitialTs = time(nullptr);
    }
    if (newState == CallState::kStateDestroyed)
    {
        mFinalTs = time(nullptr);
    }

    mState = newState;
    mCallHandler->onCallStateChange(*this);
}

CallState Call::getState() const
{
    return mState;
}

void Call::addParticipant(karere::Id peer)
{
    mParticipants.push_back(peer);
    mGlobalCallHandler.onAddPeer(*this, peer);
}


void Call::onDisconnectFromChatd()
{
    handleCallDisconnect();
    setState(CallState::kStateConnecting);
    mSfuConnection->disconnect(true);

    for (auto &it : mParticipants)
    {
        mGlobalCallHandler.onRemovePeer(*this, it);
    }
    mParticipants.clear();
}

void Call::reconnectToSfu()
{
    mSfuConnection->retryPendingConnection(true);
}

void Call::removeParticipant(karere::Id peer)
{
    for (auto itPeer = mParticipants.begin(); itPeer != mParticipants.end(); itPeer++)
    {
        if (*itPeer == peer)
        {
            mParticipants.erase(itPeer);
            mGlobalCallHandler.onRemovePeer(*this, peer);
            return;
        }
    }

    assert(false);
    return;
}

promise::Promise<void> Call::endCall()
{
    return mMegaApi.call(&::mega::MegaApi::endChatCall, mChatid, mCallid, 0)
    .then([](ReqResult /*result*/)
    {
    });
}

promise::Promise<void> Call::hangup()
{
    if (mState == kStateClientNoParticipating && mIsRinging && !mIsGroup)
    {
        // in 1on1 calls, the hangup (reject) by the user while ringing should end the call
        return endCall();
    }
    else
    {
        disconnect(TermCode::kUserHangup);
        return promise::_Void();
    }
}

promise::Promise<void> Call::join(karere::AvFlags avFlags)
{
    mLocalAvFlags = avFlags;
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::joinChatCall, mChatid.val, mCallid.val)
    .then([wptr, this](ReqResult result) -> promise::Promise<void>
    {
        if (wptr.deleted())
            return promise::Error("Join call succeed, but call has already ended");

        std::string sfuUrl = result->getText();
        connectSfu(sfuUrl);

        return promise::_Void();
    });
}

bool Call::participate()
{
    return (mState > kStateClientNoParticipating && mState < kStateTerminatingUserParticipation);
}

void Call::enableAudioLevelMonitor(bool enable)
{
    if ( (enable && mVoiceDetectionTimer != 0)          // already enabled
        || (!enable && mVoiceDetectionTimer == 0) )     // already disabled
    {
        return;
    }

    RTCM_LOG_DEBUG("Audio level monitor %s", enable ? "enabled" : "disabled");

    if (enable)
    {
        mAudioDetected = false;
        auto wptr = weakHandle();
        mVoiceDetectionTimer = karere::setInterval([this, wptr]()
        {
            if (wptr.deleted())
                return;

            webrtc::AudioProcessingStats audioStats = artc::gAudioProcessing->GetStatistics(false);

            if (audioStats.voice_detected && mAudioDetected != audioStats.voice_detected.value())
            {
                setAudioDetected(audioStats.voice_detected.value());
            }
        }, kAudioMonitorTimeout, mRtc.getAppCtx());
    }
    else
    {
        setAudioDetected(false);
        karere::cancelInterval(mVoiceDetectionTimer, mRtc.getAppCtx());
        mVoiceDetectionTimer = 0;
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
        mCallHandler->onCallRinging(*this);
    }
}

void Call::setOnHold()
{
    // disable audio track
    if (mAudio->getTransceiver()->sender()->track())
    {
        mAudio->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // disable hi-res track
    if (mHiRes->getTransceiver()->sender()->track())
    {
        mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
    }

    // disable low-res track
    if (mVThumb->getTransceiver()->sender()->track())
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
    return mVoiceDetectionTimer;
}

bool Call::hasVideoSlot(Cid_t cid, bool highRes) const
{
    for (const auto& session : mSessions)
    {
        Slot *slot = highRes
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

TermCode Call::getTermCode() const
{
    return mTermCode;
}

void Call::setCallerId(karere::Id callerid)
{
    mCallerId  = callerid;
}

bool Call::isRinging() const
{
    return mIsRinging && mCallerId != mSfuClient.myHandle();
}

bool Call::isOutgoing() const
{
    return mCallerId == mSfuClient.myHandle();
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
        RET_ENUM_NAME(kStateInitial);
        RET_ENUM_NAME(kStateClientNoParticipating);
        RET_ENUM_NAME(kStateConnecting);
        RET_ENUM_NAME(kStateJoining);    // < Joining a call
        RET_ENUM_NAME(kStateInProgress);
        RET_ENUM_NAME(kStateTerminatingUserParticipation);
        RET_ENUM_NAME(kStateDestroyed);
        default: return "(invalid call state)";
    }
}

void Call::setCallHandler(CallHandler* callHanlder)
{
    mCallHandler = std::unique_ptr<CallHandler>(callHanlder);
}

karere::AvFlags Call::getLocalAvFlags() const
{
    return mLocalAvFlags;
}

void Call::updateAndSendLocalAvFlags(karere::AvFlags flags)
{
    if (flags == mLocalAvFlags)
    {
        RTCM_LOG_WARNING("updateAndSendLocalAvFlags: AV flags has not changed");
        return;
    }

    // update and send local AV flags
    karere::AvFlags olFlags = mLocalAvFlags;
    mLocalAvFlags = flags;
    mSfuConnection->sendAv(flags.value());

    if (olFlags.isOnHold() != flags.isOnHold())
    {
        // kOnHold flag has changed
        (flags.isOnHold())
                ? setOnHold()
                : releaseOnHold();

        mCallHandler->onOnHold(*this); // notify app onHold Change
    }
    else
    {
        updateAudioTracks();
        updateVideoTracks();
        mCallHandler->onLocalFlagsChanged(*this);  // notify app local AvFlags Change
    }
}

void Call::setAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    mCallHandler->onLocalAudioDetected(*this);
}

void Call::requestSpeaker(bool add)
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
    return mSpeakerState == SpeakerState::kActive && mLocalAvFlags.audio();
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
        RTCM_LOG_ERROR("requestHighResolutionVideo: session not found for %d", cid);
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
        mSfuConnection->sendGetHiRes(cid, hasVideoSlot(cid, false), quality);
    }
}

void Call::requestHiResQuality(Cid_t cid, int quality)
{
    if (!hasVideoSlot(cid, true))
    {
        RTCM_LOG_WARNING("setHighResolutionDivider: Currently not receiving a hi-res stream for this peer");
        return;
    }

    if (quality < kCallQualityHighDef || quality > kCallQualityHighLow)
    {
        RTCM_LOG_WARNING("setHiResDivider: invalid resolution divider value (spatial layer offset).");
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
            it = cids.erase(auxit);
        }
        else if (!sess->hasHighResolutionTrack())
        {
            it = cids.erase(auxit);
            sess->notifyHiResReceived();    // also used to notify there's no video anymore
        }
    }
    if (!cids.empty())
    {
        for (auto cid: cids)
        {
            Session *sess= getSession(cid);
            assert(mAvailableTracks->hasCid(cid));
            mAvailableTracks->updateHiresTrack(cid, false);
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
            it = cids.erase(auxit);
        }
        else if (sess->hasLowResolutionTrack())
        {
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
            it = cids.erase(auxit);
        }
        else if (!sess->hasLowResolutionTrack())
        {
            it = cids.erase(auxit);
            sess->notifyLowResReceived();
        }
    }
    if (!cids.empty())
    {
        for (auto cid: cids)
        {
            Session *sess= getSession(cid);
            assert(mAvailableTracks->hasCid(cid));
            mAvailableTracks->updateLowresTrack(cid, false);
            sess->disableVideoSlot(kLowRes);
        }

        mSfuConnection->sendDelVthumbs(cids);
    }
}

void Call::requestSvcLayers(Cid_t cid, int layerIndex)
{
    if (!hasVideoSlot(cid, true))
    {
        RTCM_LOG_WARNING("setLayerSettings: Currently not receiving a hi-res stream for this peer");
        return;
    }

    // layer: spatial, temporal, screen-temporal
    int spt = 0;
    int tmp = 0;
    int stmp = 0;
    if (!getLayerByIndex(layerIndex, spt, tmp, stmp))
    {
        RTCM_LOG_WARNING("setLayerSettings: Invalid layer index");
        return;
    }

    mCurrentSvcLayerIndex = layerIndex;
    mSfuConnection->sendLayer(spt, tmp, stmp);
}

std::vector<karere::Id> Call::getParticipants() const
{
    return mParticipants;
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

void Call::connectSfu(const std::string& sfuUrl)
{
    if (!sfuUrl.empty())
    {
        mSfuUrl = sfuUrl;
    }
    else if (mSfuUrl.empty()) // if URL by param is empty, we must ensure that we already have a valid URL
    {
        RTCM_LOG_DEBUG("trying to connect to SFU with an Empty URL");
        assert(false);
        return;
    }
    setState(CallState::kStateConnecting);
    mSfuConnection = mSfuClient.createSfuConnection(mChatid, mSfuUrl, *this);
}

void Call::joinSfu()
{
    mRtcConn = artc::myPeerConnection<Call>(*this);

    createTransceivers();
    mSpeakerState = SpeakerState::kPending;
    getLocalStreams();
    setState(CallState::kStateJoining);

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    options.offer_to_receive_video = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    auto wptr = weakHandle();
    mRtcConn.createOffer(options)
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> promise::Promise<void>
    {
        if (wptr.deleted())
            return ::promise::_Void();

        KR_THROW_IF_FALSE(sdp->ToString(&mSdp));
        return mRtcConn.setLocalDescription(std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp));   // takes onwership of sdp
    })
    .then([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        sfu::Sdp sdp(mSdp);

        std::map<std::string, std::string> ivs;
        ivs["0"] = sfu::Command::binaryToHex(mVThumb->getIv());
        ivs["1"] = sfu::Command::binaryToHex(mHiRes->getIv());
        ivs["2"] = sfu::Command::binaryToHex(mAudio->getIv());
        mSfuConnection->joinSfu(sdp, ivs, mLocalAvFlags.value(), mSpeakerState, kInitialvthumbCount);
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
            return;
        disconnect(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
    });
}

void Call::createTransceivers()
{
    // create your transceivers for sending (and receiving)

    webrtc::RtpTransceiverInit transceiverInitVThumb;
    transceiverInitVThumb.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> err
            = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitVThumb);
    mVThumb = ::mega::make_unique<RemoteVideoSlot>(*this, err.MoveValue());
    mVThumb->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitHiRes;
    transceiverInitHiRes.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitHiRes);
    mHiRes = ::mega::make_unique<RemoteVideoSlot>(*this, err.MoveValue());
    mHiRes->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitAudio;
    transceiverInitAudio.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInitAudio);
    mAudio = ::mega::make_unique<Slot>(*this, err.MoveValue());
    mAudio->generateRandomIv();

    // create transceivers for receiving audio from peers
    for (int i = 0; i < RtcConstant::kMaxCallAudioSenders; i++)
    {
        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInit);
    }

    // create transceivers for receiving video from peers
    for (int i = 0; i < RtcConstant::kMaxCallVideoSenders; i++)
    {
        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInit);
    }
}

void Call::getLocalStreams()
{
    updateAudioTracks();
    if (mLocalAvFlags.videoCam())
    {
        updateVideoTracks();
    }
}

void Call::handleCallDisconnect()
{
    enableAudioLevelMonitor(false); // disable local audio level monitor
    mSessions.clear();              // session dtor will notify apps through onDestroySession callback
    freeVideoTracks(true);          // free local video tracks and release slots
    freeAudioTrack(true);           // free local audio track and release slot
    mReceiverTracks.clear();        // clear receiver tracks after free sessions and audio/video tracks
}

void Call::disconnect(TermCode termCode, const std::string &)
{
    if (mLocalAvFlags.videoCam())
    {
        releaseVideoDevice();
    }

    for (const auto& session : mSessions)
    {
        session.second->disableAudioSlot();
    }

    handleCallDisconnect();
    mAvailableTracks->clear();
    mTermCode = termCode;
    setState(CallState::kStateTerminatingUserParticipation);
    if (mSfuConnection)
    {
        mSfuClient.closeSfuConnection(mChatid);
        mSfuConnection = nullptr;
    }

    if (mRtcConn)
    {
        mRtcConn->Close();
        mRtcConn = nullptr;
    }

    // I'm the last one participant, it isn't necessary set kStateClientNoParticipating
    if (mParticipants.size() == 0 ||  (mParticipants.size() == 1 && mParticipants.at(0) == mMyPeer.getPeerid()))
    {
        return;
    }

    setState(CallState::kStateClientNoParticipating);
}

std::string Call::getKeyFromPeer(Cid_t cid, Keyid_t keyid)
{
    Session *session = getSession(cid);
    return session
            ? session->getPeer().getKey(keyid)
            : std::string();
}

bool Call::hasCallKey()
{
    return !mCallKey.empty();
}

bool Call::handleAvCommand(Cid_t cid, unsigned av)
{
    if (mMyPeer.getCid() == cid)
    {
        RTCM_LOG_WARNING("handleAvCommand: Received our own AV flags");
        return false;
    }

    Session *session = getSession(cid);
    if (!session)
    {
        RTCM_LOG_WARNING("handleAvCommand: Received AV flags for unknown peer cid %d", cid);
        return false;
    }

    // update session flags
    session->setAvFlags(karere::AvFlags(static_cast<uint8_t>(av)));
    return true;
}

void Call::requestPeerTracks(const std::set<Cid_t>& cids)
{
    std::vector<Cid_t> lowResCids;

    // compare stored cids with received ones upon ANSWER command
    std::map<Cid_t, karere::AvFlags> &availableTracks = mAvailableTracks->getTracks();
    for (auto it = availableTracks.begin(); it != availableTracks.end();)
    {
        auto auxit = it++;
        Cid_t auxCid = auxit->first;
        if (cids.find(auxCid) == cids.end()) // peer(CID) doesn't exists anymore
        {
            mAvailableTracks->removeCid(auxCid);
        }
        else // peer(CID) exists
        {
            if (mAvailableTracks->hasHiresTrack(auxCid)) // request HIRES video for that peer
            {
                requestHighResolutionVideo(auxCid, kCallQualityHighDef); // request default resolution quality
            }
            if (mAvailableTracks->hasLowresTrack(auxCid)) // add peer(CID) to lowResCids vector
            {
                lowResCids.emplace_back(auxCid);
            }
        }
    }

    // add new peers(CID) and request LowRes video by default
    for (auto cid: cids)
    {
        if (!mAvailableTracks->hasCid(cid))
        {
            mAvailableTracks->addCid(cid);
            lowResCids.emplace_back(cid);   // add peer(CID) to lowResCids vector
        }
    }

    requestLowResolutionVideo(lowResCids);
}

bool Call::getLayerByIndex(int index, int& stp, int& tmp, int& stmp)
{
    // we want to provide a linear quality scale,
    // layers are defined for each of the 7 "quality" steps
    // layer: spatial (resolution), temporal (FPS), screen-temporal (temporal layer for screen video)
    switch (index)
    {
        case 0: { stp = 0; tmp = 0; stmp = 0; return true; }
        case 1: { stp = 0; tmp = 1; stmp = 0; return true; }
        case 2: { stp = 0; tmp = 2; stmp = 0; return true; }
        case 3: { stp = 1; tmp = 1; stmp = 0; return true; }
        case 4: { stp = 1; tmp = 2; stmp = 1; return true; }
        case 5: { stp = 2; tmp = 1; stmp = 1; return true; }
        case 6: { stp = 2; tmp = 2; stmp = 2; return true; }
        default: return false;
    }
}

bool Call::handleAnswerCommand(Cid_t cid, sfu::Sdp& sdp, uint64_t ts, const std::vector<sfu::Peer>&peers,
                               const std::map<Cid_t, sfu::TrackDescriptor>&vthumbs, const std::map<Cid_t, sfu::TrackDescriptor> &speakers)
{
    // set my own client-id (cid)
    mMyPeer.init(cid, mSfuClient.myHandle(), 0);

    std::set<Cid_t> cids;
    for (const sfu::Peer& peer : peers) // does not include own cid
    {
        cids.insert(peer.getCid());
        mSessions[peer.getCid()] = ::mega::make_unique<Session>(peer);
        mCallHandler->onNewSession(*mSessions[peer.getCid()], *this);
    }

    generateAndSendNewkey();

    std::string sdpUncompress = sdp.unCompress();
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sdpInterface(webrtc::CreateSessionDescription("answer", sdpUncompress, &error));
    if (!sdpInterface)
    {
        disconnect(TermCode::kErrSdp, "Error parsing peer SDP answer: line= " + error.line +"  \nError: " + error.description);
        return false;
    }

    auto wptr = weakHandle();
    mRtcConn.setRemoteDescription(move(sdpInterface))
    .then([wptr, this, vthumbs, speakers, ts, cids]()
    {
        if (wptr.deleted())
            return;

        // prepare parameters for low resolution video
        double scale = static_cast<double>(RtcConstant::kHiResWidth) / static_cast<double>(RtcConstant::kVthumbWidth);
        webrtc::RtpParameters parameters = mVThumb->getTransceiver()->sender()->GetParameters();
        assert(parameters.encodings.size());
        parameters.encodings[0].scale_resolution_down_by = scale;
        parameters.encodings[0].max_bitrate_bps = 100 * 1024;
        mVThumb->getTransceiver()->sender()->SetParameters(parameters).ok();

        // annotate the available tracks upon connection, for further reconnects (to request the same)
        for (auto const vthumb : vthumbs)
        {
            mAvailableTracks->addCid(vthumb.first);
        }

        handleIncomingVideo(vthumbs);
        requestPeerTracks(cids);    // the ones previously available before reconnection

        for(auto speak : speakers)
        {
            Cid_t cid = speak.first;
            const sfu::TrackDescriptor& speakerDecriptor = speak.second;
            addSpeaker(cid, speakerDecriptor);
        }

        setState(CallState::kStateInProgress);
        mInitialTs -= (ts / 1000); // subtract ts(ms) received in ANSWER command, from ts captured upon setState kStateInProgress
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
            return;

        std::string msg = "Error setting SDP answer: " + err.msg();
        disconnect(TermCode::kErrSdp, msg);
    });

    return true;
}

bool Call::handleKeyCommand(Keyid_t keyid, Cid_t cid, const std::string &key)
{
    Session *session = getSession(cid);
    if (!session)
    {
        RTCM_LOG_WARNING("handleKeyCommand: Received key for unknown peer cid %d", cid);
        return false;
    }

    karere::Id peerid = session->getPeer().getPeerid();
    auto wptr = weakHandle();
    mSfuClient.getRtcCryptoMeetings()->getCU25519PublicKey(peerid)
    .then([wptr, keyid, cid, key, this](Buffer*)
    {
        if (wptr.deleted())
        {
            return;
        }

        Session *session = getSession(cid);
        if (!session)
        {
            RTCM_LOG_WARNING("handleKeyCommand: Received key for unknown peer cid %d", cid);
            return;
        }

        // decrypt received key
        strongvelope::SendKey plainKey;
        std::string binaryKey = mega::Base64::atob(key);


        strongvelope::SendKey encryptedKey;
        mSfuClient.getRtcCryptoMeetings()->strToKey(binaryKey, encryptedKey);
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

    return true;
}

bool Call::handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &videoTrackDescriptors)
{
    handleIncomingVideo(videoTrackDescriptors);
    return true;
}

bool Call::handleVThumbsStartCommand()
{
    mVThumbActive = true;
    updateVideoTracks();
    return true;
}

bool Call::handleVThumbsStopCommand()
{
    mVThumbActive = false;
    updateVideoTracks();
    return true;
}

bool Call::handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor>& videoTrackDescriptors)
{
    handleIncomingVideo(videoTrackDescriptors, kHiRes);
    return true;
}

bool Call::handleHiResStartCommand()
{
    mHiResActive = true;
    updateVideoTracks();
    return true;
}

bool Call::handleHiResStopCommand()
{
    mHiResActive = false;
    updateVideoTracks();
    return true;
}

bool Call::handleSpeakReqsCommand(const std::vector<Cid_t> &speakRequests)
{
    for (Cid_t cid : speakRequests)
    {
        if (cid != mMyPeer.getCid())
        {
            Session *session = getSession(cid);
            assert(session);
            if (!session)
            {
                RTCM_LOG_WARNING("handleSpeakReqsCommand: Received speakRequest for unknown peer cid %d", cid);
                return false;
            }
            session->setSpeakRequested(true);
        }
    }

    return true;
}

bool Call::handleSpeakReqDelCommand(Cid_t cid)
{
    if (mMyPeer.getCid() != cid) // remote peer
    {
        Session *session = getSession(cid);
        assert(session);
        if (!session)
        {
            RTCM_LOG_WARNING("handleSpeakReqDelCommand: Received delSpeakRequest for unknown peer cid %d", cid);
            return false;
        }
        session->setSpeakRequested(false);
    }
    else if (mSpeakerState == SpeakerState::kPending)
    {
        // only update audio tracks if mSpeakerState is pending to be accepted
        mSpeakerState = SpeakerState::kNoSpeaker;
        updateAudioTracks();
    }

    return true;
}

bool Call::handleSpeakOnCommand(Cid_t cid, sfu::TrackDescriptor speaker)
{
    if (cid)
    {
        addSpeaker(cid, speaker);
    }
    else if (mSpeakerState == SpeakerState::kPending)
    {
        mSpeakerState = SpeakerState::kActive;
        updateAudioTracks();
    }

    return true;
}

bool Call::handleSpeakOffCommand(Cid_t cid)
{
    if (cid)
    {
        removeSpeaker(cid);
    }
    else if (mSpeakerState == SpeakerState::kActive)
    {
        mSpeakerState = SpeakerState::kNoSpeaker;
        updateAudioTracks();
    }

    return true;
}

bool Call::handleStatCommand()
{
    return true;
}

bool Call::handlePeerJoin(Cid_t cid, uint64_t userid, int av)
{
    sfu::Peer peer(cid, userid, av);
    mSessions[cid] = ::mega::make_unique<Session>(peer);
    mCallHandler->onNewSession(*mSessions[cid], *this);
    generateAndSendNewkey();

    // We shouldn't receive a handlePeerJoin with an existing CID in mAvailableTracks
    // Upon reconnect SFU assign a new CID to the peer.
    assert(!mAvailableTracks->hasCid(cid));
    mAvailableTracks->addCid(cid);

    ISession *sess = getSession(cid);
    if (sess && sess->getAvFlags().videoLowRes())
    {
        // request low-res video by default for a new peer joined
        std::vector<Cid_t> cids;
        cids.emplace_back(cid);
        requestLowResolutionVideo(cids);
    }

    return true;
}

bool Call::handlePeerLeft(Cid_t cid)
{
    auto it = mSessions.find(cid);
    if (it == mSessions.end())
    {
        RTCM_LOG_ERROR("handlePeerLeft: unknown cid");
        return false;
    }

    mAvailableTracks->removeCid(cid);
    it->second->disableAudioSlot();
    it->second->disableVideoSlot(kHiRes);
    it->second->disableVideoSlot(kLowRes);
    mSessions.erase(cid);
    return true;
}

bool Call::handleError(unsigned int code, const std::string reason)
{
    RTCM_LOG_ERROR("SFU error (Remove call ) -> code: %d, reason: %s", code, reason.c_str());
    disconnect(static_cast<TermCode>(code), reason);
    return true;
}

bool Call::handleModerator(Cid_t cid, bool moderator)
{
    return true;
}

void Call::onSfuConnected()
{
    joinSfu();
}

void Call::onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    mVThumb->createEncryptor(getMyPeer());
    mHiRes->createEncryptor(getMyPeer());
    mAudio->createEncryptor(getMyPeer());
}

void Call::onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    absl::optional<std::string> mid = transceiver->mid();
    if (mid.has_value())
    {
        std::string value = mid.value();
        if (transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO)
        {
            mReceiverTracks[atoi(value.c_str())] = ::mega::make_unique<Slot>(*this, transceiver);
        }
        else
        {
            mReceiverTracks[atoi(value.c_str())] = ::mega::make_unique<RemoteVideoSlot>(*this, transceiver);
        }
    }
}

void Call::onRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
    RTCM_LOG_DEBUG("onRemoveTrack received");
}

void Call::onConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState)
{
    RTCM_LOG_DEBUG("onConnectionChange newstate: %d", newState);
    if ((newState == webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected)
        || (newState == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed))
    {
        if (mState != CallState::kStateConnecting) // avoid interrupting a reconnection in progress
        {
            if (mState == CallState::kStateInProgress
                    && newState == webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected)
            {
                handleCallDisconnect();
            }

            setState(CallState::kStateConnecting);
            mSfuConnection->retryPendingConnection(true);
            mSfuConnection->clearCommandsQueue();
        }
    }
    else if (newState == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    {
        bool reconnect = !mSfuConnection->isOnline();
        RTCM_LOG_DEBUG("onConnectionChange retryPendingConnection (reconnect) : %d", reconnect);
        mSfuConnection->retryPendingConnection(reconnect);
    }
}

void Call::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
}

void Call::onError()
{
}

void Call::onIceComplete()
{
}

void Call::onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
{
}

void Call::onRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
}

void Call::onIceCandidate(std::shared_ptr<artc::IceCandText> cand)
{
}

void Call::onRenegotiationNeeded()
{
}

void Call::onDataChannel(webrtc::DataChannelInterface *)
{
}

void Call::generateAndSendNewkey()
{
    // generate a new plain key
    std::shared_ptr<strongvelope::SendKey> newPlainKey = mSfuClient.getRtcCryptoMeetings()->generateSendKey();

    // add new key to own peer key map and update currentKeyId
    Keyid_t currentKeyId = mMyPeer.getCurrentKeyId() + 1;
    std::string plainkey = mSfuClient.getRtcCryptoMeetings()->keyToStr(*newPlainKey.get());
    mMyPeer.addKey(currentKeyId, plainkey);

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
    .then([wptr, currentKeyId, newPlainKey, this]
    {
        if (wptr.deleted())
        {
            return;
        }

        std::map<Cid_t, std::string> keys;

        for (const auto& session : mSessions) // encrypt key to all participants
        {
            // get peer Cid
            Cid_t sessionCid = session.first;

            // get peer id
            karere::Id peerId = session.second->getPeer().getPeerid();

            // encrypt key to participant
            strongvelope::SendKey encryptedKey;
            mSfuClient.getRtcCryptoMeetings()->encryptKeyTo(peerId, *newPlainKey.get(), encryptedKey);

            keys[sessionCid] = mega::Base64::btoa(std::string(encryptedKey.buf(), encryptedKey.size()));
        }

        if (keys.size())
        {
            mSfuConnection->sendKey(currentKeyId, keys);
        }
    });
}

void Call::handleIncomingVideo(const std::map<Cid_t, sfu::TrackDescriptor> &videotrackDescriptors, VideoResolution videoResolution)
{
    for (auto trackDescriptor : videotrackDescriptors)
    {
        auto it = mReceiverTracks.find(trackDescriptor.second.mMid);
        if (it == mReceiverTracks.end())
        {
            RTCM_LOG_WARNING("Unknown vtrack mid %d", trackDescriptor.second.mMid);
            return;
        }

        Cid_t cid = trackDescriptor.first;
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

            Session *oldSess = getSession(slot->getCid());
            if (oldSess)
            {
                // In case of Slot reassign for another peer (CID) we need to notify app about that
                (videoResolution == kHiRes)
                        ? mAvailableTracks->updateHiresTrack(slot->getCid(), false)
                        : mAvailableTracks->updateLowresTrack(slot->getCid(), false);

                oldSess->disableVideoSlot(slot->getVideoResolution());
            }
        }

        Session *sess = getSession(cid);
        if (!sess)
        {
            RTCM_LOG_ERROR("handleIncomingVideo: session with CID %d not found", cid);
            assert(false && "Possible error at SFU: session with CID not found");
            continue;
        }

        slot->assignVideoSlot(cid, trackDescriptor.second.mIv, videoResolution);
        attachSlotToSession(cid, slot, false, videoResolution, trackDescriptor.second.mReuse);
    }
}

void Call::attachSlotToSession (Cid_t cid, Slot* slot, bool audio, VideoResolution hiRes, bool reuse)
{
    Session *session = getSession(cid);
    assert(session);
    if (!session)
    {
        RTCM_LOG_WARNING("attachSlotToSession: unknown peer cid %d", cid);
        return;
    }

    assert(mAvailableTracks->hasCid(cid));
    if (audio)
    {
        mAvailableTracks->updateSpeakTrack(cid, true);
        session->setAudioSlot(slot);
    }
    else
    {
        if (hiRes)
        {
            mAvailableTracks->updateHiresTrack(cid, true);
            if (reuse)
            {
                mAvailableTracks->updateLowresTrack(cid, false);
            }

            session->setHiResSlot(static_cast<RemoteVideoSlot *>(slot));
        }
        else
        {
            mAvailableTracks->updateLowresTrack(cid, true);
            if (reuse)
            {
                mAvailableTracks->updateHiresTrack(cid, false);
            }

            session->setVThumSlot(static_cast<RemoteVideoSlot *>(slot));
        }
    }
}

void Call::addSpeaker(Cid_t cid, const sfu::TrackDescriptor &speaker)
{
    auto it = mReceiverTracks.find(speaker.mMid);
    if (it == mReceiverTracks.end())
    {
        RTCM_LOG_WARNING("AddSpeaker: unknown track mid %d", speaker.mMid);
        return;
    }

    Slot *slot = it->second.get();
    if (slot->getCid() != cid)
    {
        Session *oldSess = getSession(slot->getCid());
        if (oldSess)
        {
            // In case of Slot reassign for another peer (CID) we need to notify app about that
            mAvailableTracks->updateSpeakTrack(slot->getCid(), false);
            oldSess->disableAudioSlot();
        }
    }

    Session *sess = getSession(cid);
    if (!sess)
    {
        RTCM_LOG_WARNING("AddSpeaker: unknown cid");
        return;
    }
    slot->assign(cid, speaker.mIv);
    attachSlotToSession(cid, slot, true, kUndefined, false);
}

void Call::removeSpeaker(Cid_t cid)
{
    auto it = mSessions.find(cid);
    if (it == mSessions.end())
    {
        RTCM_LOG_ERROR("removeSpeaker: unknown cid");
        return;
    }

    assert(mAvailableTracks->hasCid(cid));
    mAvailableTracks->updateSpeakTrack(cid, false);
    it->second->disableAudioSlot();
}

sfu::Peer& Call::getMyPeer()
{
    return mMyPeer;
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

void Call::updateVideoTracks()
{
    bool isOnHold = mLocalAvFlags.isOnHold();
    if (mLocalAvFlags.videoCam() && !isOnHold)
    {
        takeVideoDevice();

        // hi-res track
        if (mHiResActive && !mHiRes->getTransceiver()->sender()->track())
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getVideoDevice()->getVideoTrackSource());
            mHiRes->getTransceiver()->sender()->SetTrack(videoTrack);
        }
        else if (!mHiResActive)
        {
            // if there is a track, but none in the call has requested hi res video, disable the track
            mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
        }

        // low-res track
        if (!mVThumb->getTransceiver()->sender()->track())
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mRtc.getVideoDevice()->getVideoTrackSource());
            webrtc::RtpParameters parameters = mVThumb->getTransceiver()->sender()->GetParameters();
            mVThumb->getTransceiver()->sender()->SetTrack(videoTrack);

        }
        else if (!mVThumbActive)
        {
            // if there is a track, but none in the call has requested low res video, disable the track
            mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
        }
    }
    else    // no video from camera (muted or not available), or call on-hold
    {
        freeVideoTracks();
        releaseVideoDevice();
    }
}

const std::string& Call::getCallKey() const
{
    return mCallKey;
}

void Call::updateAudioTracks()
{
    bool audio = mSpeakerState > SpeakerState::kNoSpeaker && mLocalAvFlags.audio();
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = mAudio->getTransceiver()->sender()->track();
    if (audio && !mLocalAvFlags.isOnHold())
    {
        if (!track) // create audio track only if not exists
        {
            rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack =
                    artc::gWebrtcContext->CreateAudioTrack("a"+std::to_string(artc::generateId()), artc::gWebrtcContext->CreateAudioSource(cricket::AudioOptions()));

            mAudio->getTransceiver()->sender()->SetTrack(audioTrack);
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

RtcModuleSfu::RtcModuleSfu(MyMegaApi &megaApi, IGlobalCallHandler &callhandler)
    : mCallHandler(callhandler)
    , mMegaApi(megaApi)
{
}

void RtcModuleSfu::init(WebsocketsIO& websocketIO, void *appCtx, rtcModule::RtcCryptoMeetings* rtcCryptoMeetings, const karere::Id& myHandle)
{
    mAppCtx = appCtx;
    mSfuClient = ::mega::make_unique<sfu::SfuClient>(websocketIO, appCtx, rtcCryptoMeetings, myHandle);
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

ICall *RtcModuleSfu::findCall(karere::Id callid)
{
    auto it = mCalls.find(callid);
    if (it != mCalls.end())
    {
        return it->second.get();
    }

    return nullptr;
}

ICall *RtcModuleSfu::findCallByChatid(const karere::Id &chatid)
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

promise::Promise<void> RtcModuleSfu::startCall(karere::Id chatid, karere::AvFlags avFlags, bool isGroup, std::shared_ptr<std::string> unifiedKey)
{
    // we need a temp string to avoid issues with lambda shared pointer capture
    std::string auxCallKey = unifiedKey ? (*unifiedKey.get()) : std::string();
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::startChatCall, chatid)
    .then([wptr, this, chatid, avFlags, isGroup, auxCallKey](ReqResult result)
    {
        std::shared_ptr<std::string> sharedUnifiedKey = !auxCallKey.empty()
                ? std::make_shared<std::string>(auxCallKey)
                : nullptr;

        wptr.throwIfDeleted();
        karere::Id callid = result->getParentHandle();
        std::string sfuUrl = result->getText();
        if (mCalls.find(callid) == mCalls.end()) // it can be created by JOINEDCALL command
        {
            mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, mSfuClient->myHandle(), false, mCallHandler, mMegaApi, (*this), isGroup, sharedUnifiedKey, avFlags);
            mCalls[callid]->connectSfu(sfuUrl);
        }
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

void RtcModuleSfu::addLocalVideoRenderer(karere::Id chatid, IVideoRenderer *videoRederer)
{
    mRenderers[chatid] = std::unique_ptr<IVideoRenderer>(videoRederer);
}

void RtcModuleSfu::removeLocalVideoRenderer(karere::Id chatid)
{
    mRenderers.erase(chatid);
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
    return mCalls.size();
}

const std::string& RtcModuleSfu::getVideoDeviceSelected() const
{
    return mVideoDeviceSelected;
}

sfu::SfuClient& RtcModuleSfu::getSfuClient()
{
    return (*mSfuClient.get());
}

void RtcModuleSfu::removeCall(karere::Id chatid, TermCode termCode)
{
    Call* call = static_cast<Call*>(findCallByChatid(chatid));
    if (call)
    {
        if (call->getState() > kStateClientNoParticipating && call->getState() <= kStateInProgress)
        {
            call->disconnect(termCode);
        }

        mCalls.erase(call->getCallid());
    }
}

void RtcModuleSfu::handleJoinedCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id> &usersJoined)
{
    for (karere::Id peer : usersJoined)
    {
        mCalls[callid]->addParticipant(peer);
    }
}

void RtcModuleSfu::handleLeftCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id> &usersLeft)
{
    for (karere::Id peer : usersLeft)
    {
        mCalls[callid]->removeParticipant(peer);
    }
}

void RtcModuleSfu::handleCallEnd(karere::Id chatid, karere::Id callid, uint8_t reason)
{
    mCalls.erase(callid);
}

void RtcModuleSfu::handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging, bool isGroup, std::shared_ptr<std::string> callKey)
{
    mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, callerid, isRinging, mCallHandler, mMegaApi, (*this), isGroup, callKey);
    mCalls[callid]->setState(kStateClientNoParticipating);
}

void RtcModuleSfu::OnFrame(const webrtc::VideoFrame &frame)
{
    for (auto& render : mRenderers)
    {
        ICall* call = findCallByChatid(render.first);
        if ((call && call->getLocalAvFlags().videoCam() && !call->getLocalAvFlags().has(karere::AvFlags::kOnHold)) || !call)
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
}

artc::VideoManager *RtcModuleSfu::getVideoDevice()
{
    return mVideoDevice;
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

RtcModule* createRtcModule(MyMegaApi &megaApi, IGlobalCallHandler& callhandler)
{
    return new RtcModuleSfu(megaApi, callhandler);
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
        rtc::scoped_refptr<webrtc::FrameEncryptorInterface> encrytor = mTransceiver->sender()->GetFrameEncryptor();
        if (encrytor)
        {
            static_cast<artc::MegaEncryptor*>(encrytor.get())->setTerminating();
        }
    }
}

void Slot::createEncryptor(const sfu::Peer& peer)
{
    mTransceiver->sender()->SetFrameEncryptor(new artc::MegaEncryptor(mCall.getMyPeer(),
                                                                      mCall.getSfuClient().getRtcCryptoMeetings(),
                                                                      mIv));
}

void Slot::createDecryptor()
{
    auto it = mCall.getSessions().find(mCid);
    if (it == mCall.getSessions().end())
    {
        RTCM_LOG_ERROR("createDecryptor: unknown cid");
        return;
    }

    mTransceiver->receiver()->SetFrameDecryptor(new artc::MegaDecryptor(it->second->getPeer(),
                                                                      mCall.getSfuClient().getRtcCryptoMeetings(),
                                                                      mIv));
}

webrtc::RtpTransceiverInterface *Slot::getTransceiver()
{
    return mTransceiver.get();
}

Cid_t Slot::getCid() const
{
    return mCid;
}

void Slot::assign(Cid_t cid, IvStatic_t iv)
{
    assert(!mCid);
    createDecryptor(cid, iv);
    enableTrack(true, kRecv);
    if (mTransceiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO)
    {
        enableAudioMonitor(true); // enable audio monitor
    }
}

bool Slot::hasTrack(bool send)
{
    assert(mTransceiver);

    if ((send && (mTransceiver->direction() == webrtc::RtpTransceiverDirection::kRecvOnly))  ||
     (!send && (mTransceiver->direction() == webrtc::RtpTransceiverDirection::kSendOnly)))
    {
        return false;
    }

    return send
            ? mTransceiver->sender()->track()
            : mTransceiver->receiver()->track();
}

void Slot::createDecryptor(Cid_t cid, IvStatic_t iv)
{
    mCid = cid;
    mIv = iv;
    createDecryptor();

    if (mTransceiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO)
    {
        mAudioLevelMonitor.reset(new AudioLevelMonitor(mCall, mCid));
    }
}

void Slot::enableAudioMonitor(bool enable)
{
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> mediaTrack = mTransceiver->receiver()->track();
    webrtc::AudioTrackInterface *audioTrack = static_cast<webrtc::AudioTrackInterface*>(mediaTrack.get());
    assert(audioTrack);
    if (enable && !mAudioLevelMonitorEnabled)
    {
        mAudioLevelMonitorEnabled = true;
        audioTrack->AddSink(mAudioLevelMonitor.get());     // enable AudioLevelMonitor for remote audio detection
    }
    else if (!enable && mAudioLevelMonitorEnabled)
    {
        mAudioLevelMonitorEnabled = false;
        audioTrack->RemoveSink(mAudioLevelMonitor.get()); // disable AudioLevelMonitor
    }
}

void Slot::enableTrack(bool enable, TrackDirection direction)
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

IvStatic_t Slot::getIv() const
{
    return mIv;
}

void Slot::generateRandomIv()
{
    randombytes_buf(&mIv, sizeof(mIv));
}

void Slot::release()
{
    if (!mCid)
    {
        return;
    }

    mIv = 0;
    mCid = 0;

    if (mAudioLevelMonitor)
    {
        enableAudioMonitor(false);
        mAudioLevelMonitor = nullptr;
        mAudioLevelMonitorEnabled = false;
    }

    enableTrack(false, kRecv);
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> decryptor = getTransceiver()->receiver()->GetFrameDecryptor();
    static_cast<artc::MegaDecryptor*>(decryptor.get())->setTerminating();
    getTransceiver()->receiver()->SetFrameDecryptor(nullptr);
}

RemoteVideoSlot::RemoteVideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    : Slot(call, transceiver)
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());

    assert(videoTrack);
    rtc::VideoSinkWants wants;
    videoTrack->AddOrUpdateSink(this, wants);
}

RemoteVideoSlot::~RemoteVideoSlot()
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());
    videoTrack->set_enabled(false);

    if (videoTrack)
    {
        videoTrack->RemoveSink(this);
    }
}

VideoSink::VideoSink()
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
}

void RemoteVideoSlot::assignVideoSlot(Cid_t cid, IvStatic_t iv, VideoResolution videoResolution)
{
    assert(mVideoResolution == kUndefined);
    assign(cid, iv);
    mVideoResolution = videoResolution;
}

void RemoteVideoSlot::release()
{
    Slot::release();
    mVideoResolution = VideoResolution::kUndefined;
}

VideoResolution RemoteVideoSlot::getVideoResolution() const
{
    return mVideoResolution;
}

void RemoteVideoSlot::enableTrack()
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());
    videoTrack->set_enabled(true);
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
    return mHiresSlot && mHiresSlot->hasTrack(false);
}

bool Session::hasLowResolutionTrack() const
{
    return mVthumSlot && mVthumSlot->hasTrack(false);
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

void Session::setAudioSlot(Slot *slot)
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

Slot *Session::getAudioSlot()
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

void Session::setSpeakRequested(bool requested)
{
    mHasRequestSpeak = requested;
    mSessionHandler->onAudioRequested(*this);
}

karere::Id Session::getPeerid() const
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

AudioLevelMonitor::AudioLevelMonitor(Call &call, int32_t cid)
    : mCall(call), mCid(cid)
{
}

void AudioLevelMonitor::OnData(const void *audio_data, int bits_per_sample, int /*sample_rate*/, size_t number_of_channels, size_t number_of_frames)
{
    if (!hasAudio())
    {
        if (mAudioDetected)
        {
            onAudioDetected(false);
        }

        return;
    }

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
        if (audioDetected != mAudioDetected)
        {
            onAudioDetected(mAudioDetected);
        }
    }
}

bool AudioLevelMonitor::hasAudio()
{
    Session *sess = mCall.getSession(mCid);
    if (sess)
    {
        return sess->getAvFlags().audio();
    }
    return false;
}

void AudioLevelMonitor::onAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    assert(mCall.getSession(mCid));
    Session *sess = mCall.getSession(mCid);
    sess->setAudioDetected(mAudioDetected);
}
}
