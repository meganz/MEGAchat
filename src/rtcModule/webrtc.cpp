#include <mega/types.h>
#include <mega/base64.h>
#include <rtcmPrivate.h>
#include <webrtcPrivate.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>

namespace rtcModule
{

Call::Call(karere::Id callid, karere::Id chatid, karere::Id callerid, bool isRinging, IGlobalCallHandler &globalCallHandler, MyMegaApi& megaApi, sfu::SfuClient &sfuClient, bool moderator, unsigned avflags)
    : mCallid(callid)
    , mChatid(chatid)
    , mCallerId(callerid)
    , mState(kStateInitial)
    , mIsRinging(isRinging)
    , mLocalAvFlags(static_cast<uint8_t>(avflags))
    , mGlobalCallHandler(globalCallHandler)
    , mMegaApi(megaApi)
    , mSfuClient(sfuClient)
    , mMyPeer()
{
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

void Call::setState(CallState state)
{
    mState = state;
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

void Call::hangup()
{
    disconnect(TermCode::kUserHangup);
}

promise::Promise<void> Call::join(bool moderator)
{
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
    return (mState > kStateUserNoParticipating && mState < kStateTerminatingUserParticipation);
}

void Call::enableAudioLevelMonitor(bool enable)
{

}

void Call::ignoreCall()
{

}

void Call::setRinging(bool ringing)
{
    if (mIsRinging != ringing)
    {
        mIsRinging = ringing;
        mCallHandler->onCallRinging(*this);
    }

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

void Call::updateAndSendLocalAvFlags(karere::AvFlags flags)
{
    mLocalAvFlags = flags;
    mSfuConnection->sendAv(flags.value());
}

void Call::requestSpeaker(bool add)
{
    if (!mSpeakerRequested && add)
    {
        mSpeakerRequested = true;
        mSfuConnection->sendSpeakReq();
        return;
    }

    if (mSpeakerRequested && !add)
    {
        mSpeakerRequested = false;
        mSfuConnection->sendSpeakReqDel();
        return;
    }
}

bool Call::isSpeakAllow()
{
    assert(false);
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
        if (session.second->getVthumSlot()->getCid() == cid)
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

void Call::requestLowResolutionVideo(const std::vector<karere::Id> &cids)
{
    mSfuConnection->sendGetVtumbs(cids);
}

void Call::stopLowResolutionVideo(const std::vector<karere::Id> &cids)
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
    return mSessions.at(cid).get();
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
            int avFlags = 6;
            mSfuConnection->joinSfu(sdp, ivs, mMyPeer.getModerator(), avFlags, true, 10);
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
    webrtc::RtpTransceiverInit transceiverInit;
    transceiverInit.direction = webrtc::RtpTransceiverDirection::kSendRecv;
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> err
            = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInit);

    if (err.ok())
    {
        mVThumb = ::mega::make_unique<VideoSlot>(*this, err.MoveValue());
        mVThumb->generateRandomIv();
    }

    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, transceiverInit);
    mHiRes = ::mega::make_unique<VideoSlot>(*this, err.MoveValue());
    mHiRes->generateRandomIv();

    err = mRtcConn->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, transceiverInit);
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
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack =
            artc::gWebrtcContext->CreateAudioTrack("a"+std::to_string(artc::generateId()), artc::gWebrtcContext->CreateAudioSource(cricket::AudioOptions()));

    mAudio->getTransceiver()->sender()->SetTrack(audioTrack);
    audioTrack->set_enabled(mLocalAvFlags.audio());

    rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
    webrtc::VideoCaptureCapability capabilities;
    capabilities.width = RtcConstant::kHiResWidth;
    capabilities.height = RtcConstant::kHiResHeight;
    capabilities.maxFPS = RtcConstant::kHiResMaxFPS;
    std::set<std::pair<std::string, std::string>> videoDevices = artc::VideoManager::getVideoDevices();
    mVideoDevice = artc::VideoManager::Create(capabilities, videoDevices.begin()->second, artc::gAsyncWaiter->guiThread());
    videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mVideoDevice->getVideoTrackSource());

    if (mLocalAvFlags.video())
    {
        mVideoDevice->openDevice(videoDevices.begin()->second);
    }

    videoTrack->set_enabled(mLocalAvFlags.video());

    mHiRes->getTransceiver()->sender()->SetTrack(videoTrack);
    rtc::VideoSinkWants wants;
    static_cast<webrtc::VideoTrackInterface*>(mHiRes->getTransceiver()->sender()->track().get())->AddOrUpdateSink(mHiRes.get(), wants);

    mVThumb->getTransceiver()->sender()->SetTrack(videoTrack);
    webrtc::RtpParameters parameters;
    webrtc::RtpEncodingParameters encoding;
    double scale = static_cast<double>(RtcConstant::kHiResWidth) / static_cast<double>(RtcConstant::kVthumbWidth);
    encoding.scale_resolution_down_by = scale;
    encoding.max_bitrate_bps = 100 * 1024;
    parameters.encodings.push_back(encoding);
    mVThumb->getTransceiver()->sender()->SetParameters(parameters);
    static_cast<webrtc::VideoTrackInterface*>(mVThumb->getTransceiver()->sender()->track().get())->AddOrUpdateSink(mVThumb.get(), wants);
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
    if (mMyPeer.getModerator())
    {
        mSpeakAllow = true;
    }

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

        //TODO setThumbVtrackResScale()

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
    mVThumb->enableTrack(true);
    return true;
}

bool Call::handleVThumbsStopCommand()
{
    mVThumb->enableTrack(false);
    return true;
}

bool Call::handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor>& videoTrackDescriptors)
{
    handleIncomingVideo(videoTrackDescriptors, true);
    return true;
}

bool Call::handleHiResStartCommand()
{
    mHiRes->enableTrack(true);
    return true;
}

bool Call::handleHiResStopCommand()
{
    mHiRes->enableTrack(false);
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
        else
        {
            mSpeakerRequested = true;
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
    else
    {
        mSpeakAllow = false;
        mSpeakerRequested = false;
    }

    return true;
}

bool Call::handleSpeakOnCommand(Cid_t cid, sfu::TrackDescriptor speaker)
{
    if (cid)
    {
        addSpeaker(cid, speaker);
    }
    else
    {
        mSpeakAllow = true;
        mAudio->enableTrack(true);
    }

    return true;
}

bool Call::handleSpeakOffCommand(Cid_t cid)
{
    if (cid)
    {
        removeSpeaker(cid);
    }
    else
    {
        mSpeakAllow = false;
        mAudio->enableTrack(false);
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
    it->second->setAudioSlot(nullptr);
}

sfu::Peer& Call::getMyPeer()
{
    return mMyPeer;
}

const std::string& Call::getCallKey() const
{
    return mCallKey;
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

void RtcModuleSfu::loadDeviceList()
{

}

bool RtcModuleSfu::selectVideoInDevice(const std::string &device)
{
    return false;
}

void RtcModuleSfu::getVideoInDevices(std::set<std::string> &devicesVector)
{

}

std::string RtcModuleSfu::getVideoDeviceSelected()
{
    return "";
}

promise::Promise<void> RtcModuleSfu::startCall(karere::Id chatid)
{
    auto wptr = weakHandle();
    return mMegaApi.call(&::mega::MegaApi::startChatCall, chatid)
    .then([wptr, this, chatid](ReqResult result)
    {
        wptr.throwIfDeleted();
        karere::Id callid = result->getParentHandle();
        std::string sfuUrl = result->getText();
        if (mCalls.find(callid) == mCalls.end()) // it can be created by JOINEDCALL command
        {
            mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, mSfuClient->myHandle(), false, mCallHandler, mMegaApi, *mSfuClient.get(), true);
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

void RtcModuleSfu::handleNewCall(karere::Id chatid, karere::Id callerid, karere::Id callid, bool isRinging)
{
    mCalls[callid] = ::mega::make_unique<Call>(callid, chatid, callerid, isRinging, mCallHandler, mMegaApi, *mSfuClient.get());
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

}

void Slot::createEncryptor(const sfu::Peer& peer)
{
    mTransceiver->sender()->SetFrameEncryptor(new artc::MegaEncryptor(mCall.getMyPeer(),
                                                                      mCall.mSfuClient.getRtcCryptoMeetings(),
                                                                      mIv));
}

void Slot::createDecryptor()
{
    auto it = mCall.mSessions.find(mCid);
    if (it == mCall.mSessions.end())
    {
        RTCM_LOG_ERROR("createDecryptor: unknown cid");
        return;
    }

    mTransceiver->receiver()->SetFrameDecryptor(new artc::MegaDecryptor(it->second->getPeer(),
                                                                      mCall.mSfuClient.getRtcCryptoMeetings(),
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
    mSessionHandler = sessionHandler;
}

void Session::setVideoRendererVthumb(IVideoRenderer *videoRederer)
{
    mVthumSlot->setVideoRender(videoRederer);
}

void Session::setVideoRendererHiRes(IVideoRenderer *videoRederer)
{
    mHiresSlot->setVideoRender(videoRederer);
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

bool Session::hasRequestSpeak() const
{
    return mHasRequestSpeak;
}
}
