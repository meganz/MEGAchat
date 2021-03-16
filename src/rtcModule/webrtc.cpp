#include <mega/types.h>
#include <mega/base64.h>
#include <rtcmPrivate.h>
#include <webrtcPrivate.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>

namespace rtcModule
{

Call::Call(karere::Id callid, karere::Id chatid, karere::Id callerid, bool isRinging, IGlobalCallHandler &globalCallHandler, MyMegaApi& megaApi, RtcModuleSfu& rtc, std::shared_ptr<std::string> callKey, bool moderator, karere::AvFlags avflags)
    : mCallid(callid)
    , mChatid(chatid)
    , mCallerId(callerid)
    , mState(kStateInitial)
    , mIsRinging(isRinging)
    , mLocalAvFlags(avflags)
    , mGlobalCallHandler(globalCallHandler)
    , mMegaApi(megaApi)
    , mSfuClient(rtc.getSfuClient())
    , mMyPeer()
    , mRtc(rtc)
{
    mAudioLevelMonitor.reset(new AudioLevelMonitor(*this, -1)); // -1 represent local
    mCallKey = callKey ? (*callKey.get()) : std::string();
    mMyPeer.setModerator(moderator);
    mGlobalCallHandler.onNewCall(*this);
    mSessions.clear();
}

Call::~Call()
{
    mState = kStateDestroyed;
    mGlobalCallHandler.onEndCall(*this);
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
    if (newState == CallState::kStateTerminatingUserParticipation)
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
    if (!isModerator())
    {
        return promise::_Void();
    }

    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::endChatCall, mChatid, mCallid, 0)
    .then([](ReqResult /*result*/)
    {

    });
}

promise::Promise<void> Call::hangup()
{
    if (mState == kStateClientNoParticipating && mIsRinging)
    {
        if (!isModerator())
        {
            return promise::_Void();
        }

        auto wptr = weakHandle();
        return mMegaApi.call(&::mega::MegaApi::endChatCall, mChatid, mCallid, 0)
        .then([](ReqResult /*result*/)
        {

        });
    }
    else
    {
        disconnect(TermCode::kUserHangup);
        return promise::_Void();
    }
}

promise::Promise<void> Call::join(bool moderator, karere::AvFlags avFlags)
{
    mLocalAvFlags = avFlags;
    mMyPeer.setModerator(moderator);
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::joinChatCall, mChatid, mCallid)
    .then([wptr, this](ReqResult result)
    {
        wptr.throwIfDeleted();
        std::string sfuUrl = result->getText();
        connectSfu(sfuUrl);
    });
}

bool Call::participate()
{
    return (mState > kStateClientNoParticipating && mState < kStateTerminatingUserParticipation);
}

void Call::enableAudioLevelMonitor(bool enable)
{
    if (mAudioLevelMonitorEnabled == enable)
    {
        return;
    }

    // Todo: implement local audio level monitor management
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

bool Call::isIgnored() const
{
    return mIgnored;
}

bool Call::isAudioLevelMonitorEnabled() const
{
    return mAudioLevelMonitorEnabled;
}

void Call::setCallerId(karere::Id callerid)
{
    mCallerId  = callerid;
}

bool Call::isRinging() const
{
    return mIsRinging && mCallerId != mSfuClient.myHandle();
}

bool Call::isModerator() const
{
    return mMyPeer.getModerator();
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

const char *Call::stateToStr(uint8_t state)
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

void Call::setVideoRendererVthumb(IVideoRenderer *videoRederer)
{
    mVThumb->setVideoRender(videoRederer);
}

void Call::setVideoRendererHiRes(IVideoRenderer *videoRederer)
{
    mHiRes->setVideoRender(videoRederer);
}

karere::AvFlags Call::getLocalAvFlags() const
{
    return mLocalAvFlags;
}

void Call::updateVideoInDevice()
{
    // todo implement
    RTCM_LOG_DEBUG("updateVideoInDevice");
}

void Call::updateAndSendLocalAvFlags(karere::AvFlags flags)
{
    mLocalAvFlags = flags;
    mSfuConnection->sendAv(flags.value());
    updateAudioTracks();
    updateVideoTracks();
    mCallHandler->onLocalFlagsChanged(*this);
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
    assert(mMyPeer.getModerator());
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
        assert(mMyPeer.getModerator());
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

void Call::requestHighResolutionVideo(Cid_t cid)
{
    bool hasVthumb = false;
    for (const auto& session : mSessions)
    {
        Slot* slot = session.second->getVthumSlot();
        if (slot && slot->getCid() == cid)
        {
            hasVthumb = true;
            break;
        }
    }

    mSfuConnection->sendGetHiRes(cid, hasVthumb);
}

void Call::stopHighResolutionVideo(Cid_t cid)
{
    mSfuConnection->sendDelHiRes(cid);
}

void Call::requestLowResolutionVideo(const std::vector<Cid_t> &cids)
{
    mSfuConnection->sendGetVtumbs(cids);
}

void Call::stopLowResolutionVideo(const std::vector<Cid_t> &cids)
{
    mSfuConnection->sendDelVthumbs(cids);
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

ISession* Call::getSession(Cid_t cid) const
{
    auto it = mSessions.find(cid);
    return (it != mSessions.end())
        ? it->second.get()
        : nullptr;
}

void Call::connectSfu(const std::string &sfuUrl)
{
    setState(CallState::kStateConnecting);
    mSfuUrl = sfuUrl;
    mSfuConnection = mSfuClient.generateSfuConnection(mChatid, sfuUrl, *this);
    auto wptr = weakHandle();
    mSfuConnection->getPromiseConnection()
    .then([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        webrtc::PeerConnectionInterface::IceServers iceServer;
        mRtcConn = artc::myPeerConnection<Call>(iceServer, *this);

        createTranceiver();
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
            return mRtcConn.setLocalDescription(sdp);
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
            mSfuConnection->joinSfu(sdp, ivs, mMyPeer.getModerator(), mLocalAvFlags.value(), mSpeakerState, kInitialvthumbCount);
        })
        .fail([wptr, this](const ::promise::Error& err)
        {
            if (wptr.deleted())
                return;
            disconnect(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
        });
    });
}

void Call::createTranceiver()
{
    webrtc::RtpTransceiverInit transceiverInitVThumb;
    transceiverInitVThumb.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> err
            = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitVThumb);

    if (err.ok())
    {
        mVThumb = ::mega::make_unique<VideoSlot>(*this, err.MoveValue());
        mVThumb->generateRandomIv();
    }

    webrtc::RtpTransceiverInit transceiverInitHiRes;
    transceiverInitHiRes.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInitHiRes);
    mHiRes = ::mega::make_unique<VideoSlot>(*this, err.MoveValue());
    mHiRes->generateRandomIv();

    webrtc::RtpTransceiverInit transceiverInitAudio;
    transceiverInitAudio.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInitAudio);
    mAudio = ::mega::make_unique<Slot>(*this, err.MoveValue());
    mAudio->generateRandomIv();

    for (int i = 0; i < RtcConstant::kMaxCallAudioSenders; i++)
    {
        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInit);
    }

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
    updateVideoTracks();
}

void Call::disconnect(TermCode termCode, const std::string &msg)
{
    if (mVideoDevice)
    {
        mVideoDevice->releaseDevice();
    }

    mSessions.clear();
    mVThumb.reset(nullptr);
    mHiRes.reset(nullptr);
    mAudio.reset(nullptr);
    mReceiverTracks.clear();
    setState(CallState::kStateTerminatingUserParticipation);
    if (mSfuConnection)
    {
        mSfuClient.closeManagerProtocol(mChatid);
        mSfuConnection = nullptr;
    }
}

std::string Call::getKeyFromPeer(Cid_t cid, Keyid_t keyid)
{
    return mSessions[cid]->getPeer().getKey(keyid);
}

bool Call::hasCallKey()
{
    return !mCallKey.empty();
}

bool Call::handleAvCommand(Cid_t cid, unsigned av)
{
    mSessions[cid]->setAvFlags(karere::AvFlags(static_cast<uint8_t>(av)));
    return true;
}

bool Call::handleAnswerCommand(Cid_t cid, sfu::Sdp& sdp, int mod,  const std::vector<sfu::Peer>&peers, const std::map<Cid_t, sfu::TrackDescriptor>&vthumbs, const std::map<Cid_t, sfu::TrackDescriptor> &speakers)
{
    mMyPeer.init(cid, mSfuClient.myHandle(), 0, mod);

    for (const sfu::Peer& peer : peers)
    {
        mSessions[peer.getCid()] = ::mega::make_unique<Session>(peer);
        mCallHandler->onNewSession(*mSessions[peer.getCid()], *this);
    }

    generateAndSendNewkey();

    std::string sdpUncompress = sdp.unCompress();
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface *sdpInterface = webrtc::CreateSessionDescription("answer", sdpUncompress, &error);
    if (!sdpInterface)
    {
        disconnect(TermCode::kErrSdp, "Error parsing peer SDP answer: line= " + error.line +"  \nError: " + error.description);
        return false;
    }

    auto wptr = weakHandle();
    mRtcConn.setRemoteDescription(sdpInterface)
    .then([wptr, this, vthumbs, speakers]()
    {
        if (wptr.deleted())
            return;

        double scale = static_cast<double>(RtcConstant::kHiResWidth) / static_cast<double>(RtcConstant::kVthumbWidth);
        webrtc::RtpParameters parameters = mVThumb->getTransceiver()->sender()->GetParameters();
        assert(parameters.encodings.size());
        parameters.encodings[0].scale_resolution_down_by = scale;
        parameters.encodings[0].max_bitrate_bps = 100 * 1024;
        mVThumb->getTransceiver()->sender()->SetParameters(parameters).ok();

        handleIncomingVideo(vthumbs);

        for(auto speak : speakers)
        {
            Cid_t cid = speak.first;
            const sfu::TrackDescriptor& speakerDecriptor = speak.second;
            addSpeaker(cid, speakerDecriptor);
        }

        setState(CallState::kStateInProgress);
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
    karere::Id peerid = mSessions[cid]->getPeer().getPeerid();
    auto wptr = weakHandle();
    mSfuClient.getRtcCryptoMeetings()->getCU25519PublicKey(peerid)
    .then([wptr, keyid, cid, key, this](Buffer*)
    {
        if (wptr.deleted())
        {
            return;
        }

        // decrypt received key
        strongvelope::SendKey plainKey;
        std::string binaryKey = mega::Base64::atob(key);


        strongvelope::SendKey encryptedKey = mSfuClient.getRtcCryptoMeetings()->strToKey(binaryKey);
        mSfuClient.getRtcCryptoMeetings()->decryptKeyFrom(mSessions[cid]->getPeer().getPeerid(), encryptedKey, plainKey);

        // in case of a call in a public chatroom, XORs received key with the call key for additional authentication
        if (hasCallKey())
        {
            strongvelope::SendKey callKey = mSfuClient.getRtcCryptoMeetings()->strToKey(mCallKey);
            mSfuClient.getRtcCryptoMeetings()->xorWithCallKey(callKey, plainKey);
        }

        // add new key to peer key map
        std::string newKey = mSfuClient.getRtcCryptoMeetings()->keyToStr(plainKey);
        assert(mSessions.find(cid) != mSessions.end());
        mSessions[cid]->addKey(keyid, newKey);
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
    handleIncomingVideo(videoTrackDescriptors, true);
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
            assert(mSessions.find(cid) != mSessions.end());
            mSessions[cid]->setSpeakRequested(true);
        }
    }

    return true;
}

bool Call::handleSpeakReqDelCommand(Cid_t cid)
{
    if (cid)
    {
        assert(mSessions.find(cid) != mSessions.end());
        mSessions[cid]->setSpeakRequested(false);
    }
    else if (mSpeakerState == SpeakerState::kPending)
    {
        mSpeakerState = SpeakerState::kNoSpeaker;
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
    sfu::Peer peer(cid, userid, av, false);

    mSessions[cid] = ::mega::make_unique<Session>(peer);
    mCallHandler->onNewSession(*mSessions[cid], *this);
    generateAndSendNewkey();
    return true;
}

bool Call::handlePeerLeft(Cid_t cid)
{
    mSessions.erase(cid);
}

bool Call::handleError(unsigned int code, const std::string reason)
{
    RTCM_LOG_ERROR("SFU error (Remove call ) -> code: %d, reason: %s", code, reason.c_str());
    disconnect(static_cast<TermCode>(code), reason);
}

bool Call::handleModerator(Cid_t cid, bool moderator)
{
    if (cid)
    {
        assert(mSessions.find(cid) != mSessions.end());
        mSessions[cid]->setModerator(moderator);
    }
    else
    {
        mIsModerator = moderator;
        mCallHandler->onModeratorChange(*this);
    }

    return true;
}

void Call::onError()
{

}

void Call::onAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    mVThumb->createDecryptor();
    mVThumb->createEncryptor(getMyPeer());

    mHiRes->createDecryptor();
    mHiRes->createEncryptor(getMyPeer());

    mAudio->createDecryptor();
    mAudio->createEncryptor(getMyPeer());
}

void Call::onRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
}

void Call::onIceCandidate(std::shared_ptr<artc::IceCandText> cand)
{

}

void Call::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    if (state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed)
    {
        // force reconnect
    }
}

void Call::onIceComplete()
{

}

void Call::onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
{

}

void Call::onDataChannel(webrtc::DataChannelInterface *data_channel)
{

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
            mReceiverTracks[atoi(value.c_str())] = ::mega::make_unique<VideoSlot>(*this, transceiver);
        }
    }
}

void Call::onRenegotiationNeeded()
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
        strongvelope::SendKey callKey = mSfuClient.getRtcCryptoMeetings()->strToKey(mCallKey);
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

        mSfuConnection->sendKey(currentKeyId, keys);
    });
}

void Call::handleIncomingVideo(const std::map<Cid_t, sfu::TrackDescriptor> &videotrackDescriptors, bool hiRes)
{
    for (auto trackDescriptor : videotrackDescriptors)
    {
        auto it = mReceiverTracks.find(trackDescriptor.second.mMid);
        if (it == mReceiverTracks.end())
        {
            RTCM_LOG_ERROR("Unknown vtrack mid %d", trackDescriptor.second.mMid);
            return;
        }

        rtc::VideoSinkWants opts;
        VideoSlot* slot = static_cast<VideoSlot*>(it->second.get());
        Cid_t cid = trackDescriptor.first;
        slot->createDecryptor(cid, trackDescriptor.second.mIv);
        slot->enableTrack(true);
        slot->setTrackSink();

        if (hiRes)
        {
            mSessions[cid]->setHiResSlot(slot);
        }
        else
        {
            mSessions[cid]->setVThumSlot(slot);
        }
    }
}

void Call::addSpeaker(Cid_t cid, const sfu::TrackDescriptor &speaker)
{
    if (mSessions.find(cid) == mSessions.end())
    {
        RTCM_LOG_ERROR("AddSpeaker: unknown cid");
        return;
    }

    auto it = mReceiverTracks.find(speaker.mMid);
    if (it == mReceiverTracks.end())
    {
        RTCM_LOG_ERROR("AddSpeaker: unknown mid");
        return;
    }

    Slot* slot = it->second.get();
    slot->enableTrack(true);
    slot->createDecryptor(cid, speaker.mIv);
    slot->enableAudioMonitor(true);

    mSessions[cid]->setAudioSlot(slot);
}

void Call::removeSpeaker(Cid_t cid)
{
    auto it = mSessions.find(cid);
    if (it == mSessions.end())
    {
        RTCM_LOG_ERROR("removeSpeaker: unknown cid");
        return;
    }

    Slot* slot = it->second->getAudioSlot();
    slot->enableTrack(false);
    slot->enableAudioMonitor(false);
    it->second->setAudioSlot(nullptr);
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

const std::string& Call::getCallKey() const
{
    return mCallKey;
}

void Call::updateAudioTracks()
{
    bool audio = mSpeakerState > SpeakerState::kNoSpeaker && mLocalAvFlags.audio();
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = mAudio->getTransceiver()->sender()->track();
    if (audio)
    {
        if (!track)
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
    else if (track)
    {
        track->set_enabled(false);
        mAudio->getTransceiver()->sender()->SetTrack(nullptr);
    }
}

void Call::updateVideoTracks()
{
    if (mLocalAvFlags.video())
    {
        if (!mVideoDevice)
        {
            std::string videoDevice = mRtc.getVideoDeviceSelected(); // get default video device
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
            mVideoDevice = artc::VideoManager::Create(capabilities, videoDevice, artc::gAsyncWaiter->guiThread());
            mVideoDevice->openDevice(videoDevice);
            // Our local slot connect directly to video device to keep showing video althoug no one wants our video
            rtc::VideoSinkWants wants;
            mVideoDevice->AddOrUpdateSink(mVThumb.get(), wants);
            mVideoDevice->AddOrUpdateSink(mHiRes.get(), wants);

        }

        if (mHiResActive && !mHiRes->getTransceiver()->sender()->track())
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mVideoDevice->getVideoTrackSource());
            mHiRes->getTransceiver()->sender()->SetTrack(videoTrack);
        }
        else if (!mHiResActive)
        {
            mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
        }

        if (!mVThumb->getTransceiver()->sender()->track())
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
            videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mVideoDevice->getVideoTrackSource());
            webrtc::RtpParameters parameters = mVThumb->getTransceiver()->sender()->GetParameters();
            mVThumb->getTransceiver()->sender()->SetTrack(videoTrack);

        }
        else if (!mVThumbActive)
        {
            mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
        }
    }
    else
    {
        if (mHiRes->getTransceiver()->sender()->track())
        {
            mHiRes->getTransceiver()->sender()->SetTrack(nullptr);
        }

        if (mVThumb->getTransceiver()->sender()->track())
        {
            mVThumb->getTransceiver()->sender()->SetTrack(nullptr);
        }

        if (mVideoDevice)
        {
            mVideoDevice->releaseDevice();
            mVideoDevice = nullptr;
        }
    }
}

RtcModuleSfu::RtcModuleSfu(MyMegaApi &megaApi, IGlobalCallHandler &callhandler, IRtcCrypto *crypto, const char *iceServers)
    : mCallHandler(callhandler)
    , mMegaApi(megaApi)
{
}

void RtcModuleSfu::init(WebsocketsIO& websocketIO, void *appCtx, rtcModule::RtcCryptoMeetings* rRtcCryptoMeetings, const karere::Id& myHandle)
{
    mSfuClient = ::mega::make_unique<sfu::SfuClient>(websocketIO, appCtx, rRtcCryptoMeetings, myHandle);
    if (!artc::isInitialized())
    {
        artc::init(appCtx);
        RTCM_LOG_DEBUG("WebRTC stack initialized before first use");
    }

    // set default video in device
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
    mVideoDeviceSelected = videoDevices.begin()->second;
}

void RtcModuleSfu::hangupAll()
{

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

ICall *RtcModuleSfu::findCallByChatid(karere::Id chatid)
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
    for (auto it = videoDevices.begin(); it != videoDevices.end(); it++)
    {
        if (!it->first.compare(device))
        {
            mVideoDeviceSelected = it->second;
            for (const auto& call : mCalls)
            {
                call.second->updateVideoInDevice();
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

promise::Promise<void> RtcModuleSfu::startCall(karere::Id chatid, karere::AvFlags avFlags, std::shared_ptr<std::string> unifiedKey)
{
    // we need a temp string to avoid issues with lambda shared pointer capture
    std::string auxCallKey = unifiedKey ? (*unifiedKey.get()) : std::string();
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::startChatCall, chatid)
    .then([wptr, this, chatid, avFlags, auxCallKey](ReqResult result)
    {
        std::shared_ptr<std::string> sharedUnifiedKey = !auxCallKey.empty()
                ? std::make_shared<std::string>(auxCallKey)
                : nullptr;

        wptr.throwIfDeleted();
        karere::Id callid = result->getParentHandle();
        std::string sfuUrl = result->getText();
        if (mCalls.find(callid) == mCalls.end()) // it can be created by JOINEDCALL command
        {
            mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, mSfuClient->myHandle(), false, mCallHandler, mMegaApi, (*this), sharedUnifiedKey, true, avFlags);
            mCalls[callid]->connectSfu(sfuUrl);
        }
    });
}

std::vector<karere::Id> RtcModuleSfu::chatsWithCall()
{
    std::vector<karere::Id> v;
    return v;
}

unsigned int RtcModuleSfu::getNumCalls()
{
    return 0;
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
        if (call->getState() <= CallState::kStateInProgress)
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

void RtcModuleSfu::handleLefCall(karere::Id chatid, karere::Id callid, const std::vector<karere::Id> &usersLeft)
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

void RtcModuleSfu::handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging, std::shared_ptr<std::string> callKey)
{
    mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, callerid, isRinging, mCallHandler, mMegaApi, (*this), callKey);
    mCalls[callid]->setState(kStateClientNoParticipating);
}

RtcModule* createRtcModule(MyMegaApi &megaApi, IGlobalCallHandler& callhandler, IRtcCrypto* crypto, const char* iceServers)
{
    return new RtcModuleSfu(megaApi, callhandler, crypto, iceServers);
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

void Slot::createDecryptor(Cid_t cid, IvStatic_t iv)
{
    mCid = cid;
    mIv = iv;
    createDecryptor();
    mAudioLevelMonitor.reset(new AudioLevelMonitor(mCall, mCid));
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

void Slot::enableTrack(bool enable)
{
    if (mTransceiver->direction() == webrtc::RtpTransceiverDirection::kRecvOnly)
    {
        mTransceiver->receiver()->track()->set_enabled(enable);
    }
    else if(mTransceiver->direction() == webrtc::RtpTransceiverDirection::kSendRecv)
    {
        mTransceiver->receiver()->track()->set_enabled(enable);
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

VideoSlot::VideoSlot(Call& call, rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    : Slot(call, transceiver)
{
}

VideoSlot::~VideoSlot()
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());
    videoTrack->set_enabled(true);

    if (videoTrack)
    {
        videoTrack->RemoveSink(this);
    }
}

void VideoSlot::setVideoRender(IVideoRenderer *videoRenderer)
{
    mRenderer = std::unique_ptr<IVideoRenderer>(videoRenderer);
}

void VideoSlot::OnFrame(const webrtc::VideoFrame &frame)
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

void VideoSlot::setTrackSink()
{
    webrtc::VideoTrackInterface* videoTrack =
            static_cast<webrtc::VideoTrackInterface*>(mTransceiver->receiver()->track().get());
    videoTrack->set_enabled(true);

    if (videoTrack)
    {
        rtc::VideoSinkWants wants;
        videoTrack->AddOrUpdateSink(this, wants);
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
    mState = kSessStateDestroyed;
    mSessionHandler->onDestroySession(*this);
}

void Session::setSessionHandler(SessionHandler* sessionHandler)
{
    mSessionHandler = std::unique_ptr<SessionHandler>(sessionHandler);
}

void Session::setVideoRendererVthumb(IVideoRenderer *videoRederer)
{
    mVthumSlot->setVideoRender(videoRederer);
}

void Session::setVideoRendererHiRes(IVideoRenderer *videoRederer)
{
    mHiresSlot->setVideoRender(videoRederer);
}

void Session::setAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    mSessionHandler->onRemoteAudioDetected(*this);
}

const sfu::Peer& Session::getPeer() const
{
    return mPeer;
}

void Session::setVThumSlot(VideoSlot *slot)
{
    mVthumSlot = slot;
    mSessionHandler->onVThumbReceived(*this);
}

void Session::setHiResSlot(VideoSlot *slot)
{
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
    mPeer.setAvFlags(flags);
    assert(mSessionHandler);
    mSessionHandler->onAudioVideoFlagsChanged(*this);
}

Slot *Session::getAudioSlot()
{
    return mAudioSlot;
}

VideoSlot *Session::getVthumSlot()
{
    return mVthumSlot;
}

VideoSlot *Session::betHiResSlot()
{
    return mHiresSlot;
}

void Session::setSpeakRequested(bool requested)
{
    mHasRequestSpeak = requested;
    mSessionHandler->onAudioRequested(*this);
}

bool Session::setModerator(bool requested)
{
    mIsModerator = requested;
    mSessionHandler->onModeratorChange(*this);
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

bool Session::isModerator() const
{
    return mIsModerator;
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
    if (mCid < 0)
    {
        return mCall.getLocalAvFlags().audio();
    }
    else
    {
        ISession *sess = mCall.getSession(mCid);
        if (sess)
        {
            return sess->getAvFlags().audio();
        }
        return false;
    }
}

void AudioLevelMonitor::onAudioDetected(bool audioDetected)
{
    mAudioDetected = audioDetected;
    if (mCid < 0) // local
    {
        mCall.setAudioDetected(mAudioDetected);
    }
    else // remote
    {
        assert(mCall.getSession(mCid));
        ISession *sess = mCall.getSession(mCid);
        sess->setAudioDetected(mAudioDetected);
    }
}
}
