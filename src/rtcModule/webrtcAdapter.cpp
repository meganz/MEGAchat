#include "rtcmPrivate.h"
#include "webrtcAdapter.h"
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <modules/video_capture/video_capture_factory.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/field_trial.h>

#ifdef __ANDROID__
extern JavaVM *MEGAjvm;
extern JNIEnv *jenv;
extern jclass applicationClass;
extern jmethodID startVideoCaptureMID;
extern jmethodID stopVideoCaptureMID;
extern jmethodID deviceListMID;
extern jobject surfaceTextureHelper;
#endif

using namespace CryptoPP;

namespace artc
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext = nullptr;
std::unique_ptr<rtc::Thread> gWorkerThread = nullptr;
std::unique_ptr<rtc::Thread> gSignalingThread = nullptr;
rtc::scoped_refptr<webrtc::AudioProcessing> gAudioProcessing = nullptr;
std::string gFieldTrialStr;

static bool gIsInitialized = false;

bool isInitialized() { return gIsInitialized; }
bool init(void*)
{
    if (gIsInitialized)
        return false;

    if (gWebrtcContext == nullptr)
    {
        // Enable SVC encoding with the following configuration (3 spatial Layers | 3 temporal Layers)
        gFieldTrialStr = (webrtc::field_trial::MergeFieldTrialsStrings("WebRTC-GenericDescriptorAuth/Disabled/", "WebRTC-SupportVP9SVC/EnabledByFlag_3SL3TL/"));
        gFieldTrialStr = webrtc::field_trial::MergeFieldTrialsStrings("WebRTC-Video-DisableAutomaticResize/Enabled/", gFieldTrialStr.c_str());
        webrtc::field_trial::InitFieldTrialsFromString(gFieldTrialStr.c_str()); // trials_string must never be destroyed.

        gWorkerThread = rtc::Thread::Create();
        gWorkerThread->Start();
        gSignalingThread = rtc::Thread::Create();
        gSignalingThread->Start();

        gAudioProcessing = rtc::scoped_refptr<webrtc::AudioProcessing>(webrtc::AudioProcessingBuilder().Create());
        webrtc::AudioProcessing::Config audioConfig = gAudioProcessing->GetConfig();
        gAudioProcessing->ApplyConfig(audioConfig);

        gWebrtcContext = webrtc::CreatePeerConnectionFactory(
                    nullptr /*networThread*/, gWorkerThread.get() /*workThread*/,
                    gSignalingThread.get() /*signaledThread*/, nullptr,
                    webrtc::CreateBuiltinAudioEncoderFactory(),
                    webrtc::CreateBuiltinAudioDecoderFactory(),
                    webrtc::CreateBuiltinVideoEncoderFactory(),
                    webrtc::CreateBuiltinVideoDecoderFactory(),
                    nullptr /* audio_mixer */, gAudioProcessing);
    }

    if (!gWebrtcContext)
        throw std::runtime_error("Error creating peerconnection factory");
    gIsInitialized = true;
    return true;
}

void cleanup()
{
    if (!gIsInitialized)
        return;
    gWebrtcContext = nullptr;
    rtc::CleanupSSL();
    rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
    gIsInitialized = false;
    gWorkerThread.reset(nullptr);
    gSignalingThread.reset(nullptr);
}

/** Stream id and other ids generator */
unsigned long generateId()
{
    static unsigned long id = 0;
    return ++id;
}

CaptureModuleLinux::CaptureModuleLinux(const webrtc::VideoCaptureCapability &capabilities, bool remote)
    : mState(webrtc::MediaSourceInterface::kInitializing),
      mRemote(remote),
      mCapabilities(capabilities)
{
}

CaptureModuleLinux::~CaptureModuleLinux()
{
}

void CaptureModuleLinux::RegisterObserver(webrtc::ObserverInterface*)
{

}

void CaptureModuleLinux::UnregisterObserver(webrtc::ObserverInterface*)
{

}

webrtc::MediaSourceInterface::SourceState CaptureModuleLinux::state() const
{
    return mState;
}

bool CaptureModuleLinux::is_screencast() const
{
    return false;
}

absl::optional<bool> CaptureModuleLinux::needs_denoising() const
{
    return absl::nullopt;
}

bool CaptureModuleLinux::GetStats(webrtc::VideoTrackSourceInterface::Stats*)
{
    return false;
}

void CaptureModuleLinux::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    mBroadcaster.AddOrUpdateSink(sink, wants);
}

void CaptureModuleLinux::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    mBroadcaster.RemoveSink(sink);
}

bool CaptureModuleLinux::remote() const
{
    return mRemote;
}

void CaptureModuleLinux::OnFrame(const webrtc::VideoFrame& frame)
{
    mBroadcaster.OnFrame(frame);
}

std::set<std::pair<std::string, std::string> > CaptureModuleLinux::getVideoDevices()
{
    std::set<std::pair<std::string, std::string>> videoDevices;
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info)
    {
        RTCM_LOG_WARNING("Unable to get device info");
        return videoDevices;
    }

    uint32_t numDevices = info->NumberOfDevices();
    for (uint32_t i = 0; i < numDevices; i++)
    {
        char deviceName[256]; // Friendly name of the capture device.
        char uniqueName[256]; // Unique name of the capture device if it exist.
        info->GetDeviceName(i, deviceName, sizeof(deviceName), uniqueName, sizeof(uniqueName));
        videoDevices.insert(std::pair<std::string, std::string>(deviceName, uniqueName));
    }

    return videoDevices;
}

void CaptureModuleLinux::openDevice(const std::string &videoDevice)
{
    mCameraCapturer = webrtc::VideoCaptureFactory::Create(videoDevice.c_str());
    if (!mCameraCapturer)
    {
        RTCM_LOG_WARNING("Unable to open Device (CaptureModuleLinux)");
        return;
    }

    mCameraCapturer->RegisterCaptureDataCallback(this);

    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> deviceInfo(webrtc::VideoCaptureFactory::CreateDeviceInfo());
    webrtc::VideoCaptureCapability capabilities;
    deviceInfo->GetCapability(mCameraCapturer->CurrentDeviceName(), 0, capabilities);
    mCapabilities.interlaced = capabilities.interlaced;
    mCapabilities.videoType = webrtc::VideoType::kI420;

    if (mCameraCapturer->StartCapture(mCapabilities) != 0)
    {
        RTCM_LOG_WARNING("Unable to start capture");
        return;
    }

    RTC_CHECK(mCameraCapturer->CaptureStarted());
}

void CaptureModuleLinux::releaseDevice()
{
    if (mCameraCapturer)
    {
        mCameraCapturer->StopCapture();
        mCameraCapturer->DeRegisterCaptureDataCallback();
        mCameraCapturer = nullptr;
    }
}

webrtc::VideoTrackSourceInterface* CaptureModuleLinux::getVideoTrackSource()
{
    return this;
}

karere::AvFlags LocalStreamHandle::av()
{
    return karere::AvFlags(mAudio.get(), mVideo.get());
}

karere::AvFlags LocalStreamHandle::effectiveAv() const
{
    return karere::AvFlags(mAudio && mAudio->enabled(), mVideo && mVideo->enabled());
}

void LocalStreamHandle::setAv(karere::AvFlags av)
{
    bool audio = av.audio();
    if (mAudio && mAudio->enabled() != audio)
    {
        mAudio->set_enabled(audio);
    }

    bool video = av.camera();
    if (mVideo && mVideo->enabled() != video)
    {
        mVideo->set_enabled(video);
    }
}

LocalStreamHandle::LocalStreamHandle(const char*)
{
}

LocalStreamHandle::~LocalStreamHandle()
{ //make sure the stream is released before the tracks
}

void LocalStreamHandle::addAudioTrack(const rtc::scoped_refptr<webrtc::AudioTrackInterface> &audio)
{
    mAudio = audio;
}

void LocalStreamHandle::addVideoTrack(const rtc::scoped_refptr<webrtc::VideoTrackInterface> &video)
{
    mVideo = video;
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> LocalStreamHandle::audio()
{
    return mAudio;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> LocalStreamHandle::video()
{
    return mVideo;
}

VideoManager *VideoManager::Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &
#if defined(__APPLE__) || defined(__ANDROID__)
                                   deviceName
#endif
                                   , rtc::Thread *
#ifdef __ANDROID__
                                   thread
#endif
                                   )
{
#ifdef __APPLE__
    return new OBJCCaptureModule(capabilities, deviceName);
#elif __ANDROID__
    return new CaptureModuleAndroid(capabilities, deviceName, thread);
#else
   // return new CaptureModuleLinux(capabilities);
    return new CaptureScreenModuleLinux(); // test with screen capturer instead of camera
#endif
}

std::set<std::pair<std::string, std::string>> VideoManager::getVideoDevices()
{
    #ifdef __APPLE__
        return OBJCCaptureModule::getVideoDevices();
    #elif __ANDROID__
        return CaptureModuleAndroid::getVideoDevices();
    #else
        return CaptureModuleLinux::getVideoDevices();
    #endif
}

RtcCipher::RtcCipher(const sfu::Peer &peer, std::shared_ptr<rtcModule::IRtcCryptoMeetings> cryptoMeetings, IvStatic_t iv, uint32_t mid)
    : mPeer(peer)
    , mCryptoMeetings(cryptoMeetings)
    , mIv(iv)
    , mMid(mid)
{

}

void RtcCipher::setKey(const std::string &key)
{
    const unsigned char *newKey = reinterpret_cast<const unsigned char*>(key.data());
    mSymCipher.setkey(newKey);
}

void RtcCipher::setTerminating()
{
    mTerminating = true;
}

/* frame IV format: <frameIv.12> = <frameCtr.4> <staticIv.8> */
std::unique_ptr<byte []> RtcCipher::generateFrameIV()
{
    std::unique_ptr<byte []> iv(new byte[FRAME_IV_LENGTH]);
    memcpy(iv.get(), &mCtr, FRAME_CTR_LENGTH);
    memcpy(iv.get() + FRAME_CTR_LENGTH, &mIv, FRAME_IV_LENGTH - FRAME_CTR_LENGTH);
    return iv;
}

MegaEncryptor::MegaEncryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv, uint32_t mid)
    : RtcCipher(peer, cryptoMeetings, iv, mid)
{
}

MegaEncryptor::~MegaEncryptor()
{
}

void MegaEncryptor::incrementPacketCtr()
{
    (mCtr < UINT32_MAX)
            ? mCtr++
            : mCtr = 1;   // reset packet ctr if max value has been reached
}

/*
 * Frame format: header.8: <keyId.1> <CID.3> <packetCTR.4>
 * Note: (keyId.1 senderCID.3) and packetCtr.4 are little-endian (No need byte-order swap) 32-bit integers.
 */
void MegaEncryptor::generateHeader(uint8_t *header)
{
    Keyid_t keyId = mPeer.getCurrentKeyId();
    memcpy(header, &keyId, FRAME_KEYID_LENGTH);

    Cid_t cid = mPeer.getCid();
    unsigned offset = FRAME_KEYID_LENGTH;
    memcpy(header + offset, &cid, FRAME_CID_LENGTH);

    offset += FRAME_CID_LENGTH;
    memcpy(header + offset, &mCtr, FRAME_CTR_LENGTH);
}


// encrypted_frame: <header.8> <encrypted.data.varlen> <GCM_Tag.4>
int MegaEncryptor::Encrypt(cricket::MediaType media_type, uint32_t /*ssrc*/, rtc::ArrayView<const uint8_t> /*additional_data*/, rtc::ArrayView<const uint8_t> frame, rtc::ArrayView<uint8_t> encrypted_frame, size_t *bytes_written)
{
    if (mTerminating)
    {
        // kRecoverable should be returned if the failure was due to something other than a encryption failure
        return kRecoverable;
    }

    if (!frame.size())
    {
        RTCM_LOG_WARNING("Encrypt: given frame to be encrypted is empty");
        return kRecoverable;
    }

    // get keyId for peer
    Keyid_t currentKeyId = mPeer.getCurrentKeyId();
    if (currentKeyId != mKeyId || !mInitialized)
    {
        // If there's no key armed in SymCipher or keyId doesn't match with current one
        mKeyId = currentKeyId;
        std::string encryptionKey = mPeer.getKey(currentKeyId);
        if (encryptionKey.empty())
        {

            RTCM_LOG_WARNING("Encrypt: key doesn't found with keyId: %u, MyCid %u, MyPeerid: %s, frameCtr: %u",
                             currentKeyId, mPeer.getCid(), mPeer.getPeerid().toString().c_str(), mCtr);
            return kRecoverable;
        }
        setKey(encryptionKey);

        if (!mInitialized)
        {
            mInitialized = true;
        }
    }

    // generate frame iv
    mega::unique_ptr<byte []> iv = generateFrameIV();

    // generate header and store in encrypted_frame
    generateHeader(encrypted_frame.data());

    // encrypt frame and store it in encrypted_frame
    std::string encryptedFrame;
    size_t encSize = encrypted_frame.size()-FRAME_HEADER_LENGTH;
    bool result = mSymCipher.gcm_encrypt_add(frame.data(), frame.size(),
                                           encrypted_frame.data(), FRAME_HEADER_LENGTH,
                                           iv.get(), FRAME_IV_LENGTH,
                                           FRAME_GCM_TAG_LENGTH, encryptedFrame, encSize);

    if (!result || encryptedFrame.size() != encSize)
    {
        RTCM_LOG_WARNING("Failed gcm_encrypt_aad encryption with additional authenticated data: MyCid: %u, MyPeerId: %s, KeyId: %u, frameCtr: %u",
                         mPeer.getCid(), mPeer.getPeerid().toString().c_str(), mKeyId, mCtr - 1);
        return kRecoverable;
    }
    memcpy(encrypted_frame.data() + FRAME_HEADER_LENGTH, encryptedFrame.data(), encSize);

    // set bytes_written to the number of bytes, written in encrypted_frame
    assert(GetMaxCiphertextByteSize(media_type, frame.size()) == encrypted_frame.size());
    *bytes_written = encrypted_frame.size();
    size_t expectedSize = GetMaxCiphertextByteSize(media_type, frame.size());
    if (expectedSize != *bytes_written)
    {
        RTCM_LOG_WARNING("Encrypt: Frame size: %lu doesn't match with expected size: %lu MyCid: %u, MyPeerId: %s, KeyId: %u, frameCtr: %u",
                         *bytes_written, expectedSize, mPeer.getCid(), mPeer.getPeerid().toString().c_str(), mKeyId, mCtr - 1);
        return kRecoverable;
    }

    // increment packetCtr, if encryption process has succeeded
    incrementPacketCtr();
    return kOk;
}

size_t MegaEncryptor::GetMaxCiphertextByteSize(cricket::MediaType /*media_type*/, size_t frame_size)
{
    // header size + frame size + GCM authentication tag size
    return FRAME_HEADER_LENGTH + frame_size + FRAME_GCM_TAG_LENGTH;
}

MegaDecryptor::MegaDecryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv,  uint32_t mid)
    : RtcCipher(peer, cryptoMeetings, iv, mid)
{
}

MegaDecryptor::~MegaDecryptor()
{
}


/*
 * header format: <header.8> = <keyId.1> <cid.3> <packetCTR.4>
 * Note: (keyId.1 senderCID.3) and packetCtr.4 are little-endian (No need byte-order swap) 32-bit integers.
 */
int MegaDecryptor::validateAndProcessHeader(rtc::ArrayView<const uint8_t> header)
{
    assert(header.size() == FRAME_HEADER_LENGTH);
    const uint8_t *headerData = header.data();

    // extract keyId from header
    Keyid_t auxKeyId = 0;
    memcpy(&auxKeyId, headerData, FRAME_KEYID_LENGTH);

    // extract CID from header, and check if matches with expected one
    unsigned offset = FRAME_KEYID_LENGTH;
    Cid_t peerCid = 0;
    memcpy(&peerCid, headerData + offset, FRAME_CID_LENGTH);

    // extract packet ctr from header, and update mCtr (ctr will be used to generate an IV to decrypt the frame)
    offset += FRAME_CID_LENGTH;
    memcpy(&mCtr, headerData + offset, FRAME_CTR_LENGTH);

    if (peerCid != mPeer.getCid())
    {
        RTCM_LOG_WARNING("validateAndProcessHeader: Frame CID doesn't match with expected one. expected: %u, received: %u, "
                         "mid: %u peerid: %s, keyid: %u, frameCtr: %u", mPeer.getCid(), peerCid,
                         mMid, mPeer.getPeerid().toString().c_str(), auxKeyId, mCtr);
        return static_cast<int>(Status::kRecoverable); // recoverable error
    }

    if (auxKeyId != mKeyId || !mInitialized)
    {
        // If there's no key armed in SymCipher or keyId doesn't match with current one
        std::string decryptionKey = mPeer.getKey(auxKeyId);
        if (decryptionKey.empty())
        {
            RTCM_LOG_WARNING("validateAndProcessHeader: key doesn't found with Frame keyId: %u, mid: %u, peercid: %u, peerid: %s, frameCtr: %u",
                             auxKeyId, mMid, peerCid, mPeer.getPeerid().toString().c_str(), mCtr);
            return static_cast<int>(Status::kRecoverable); // decryption error
        }

        mKeyId = auxKeyId;
        setKey(decryptionKey);

        if (!mInitialized)
        {
            mInitialized = true;
        }
    }

    return static_cast<int>(Status::kOk);
}

/* frame format: <receivedFrame.N> = <header.8> <encframeData.M> <gcmTag.4>
 * Note: encframeData length (M) = receivedFrame_length(N) - header_length(8) - gcmTag_length(4) */
webrtc::FrameDecryptorInterface::Result MegaDecryptor::Decrypt(cricket::MediaType media_type, const std::vector<uint32_t> &/*csrcs*/, rtc::ArrayView<const uint8_t> /*additional_data*/, rtc::ArrayView<const uint8_t> encrypted_frame, rtc::ArrayView<uint8_t> frame)
{
    if (mTerminating)
    {
        // kRecoverable should be returned if the failure was due to something other than a decryption failure
        return Result(Status::kRecoverable, 0);
    }

    if (encrypted_frame.empty())
    {
        // error with the given frame, don't pass to the decoder, but the receive stream is still decryptable
        RTCM_LOG_WARNING("Decrypt: received frame to be decrypted is empty");
        return Result(Status::kRecoverable, 0);
    }

    // extract header, encrypted frame data, and gcmTag(message hash or MAC) from encrypted_frame
    rtc::ArrayView<const byte> header = encrypted_frame.subview(0, FRAME_HEADER_LENGTH);
    rtc::ArrayView<const byte> data   = encrypted_frame.subview(FRAME_HEADER_LENGTH, encrypted_frame.size() - FRAME_HEADER_LENGTH - FRAME_GCM_TAG_LENGTH);
    rtc::ArrayView<const byte> gcmTag = encrypted_frame.subview(encrypted_frame.size() - FRAME_GCM_TAG_LENGTH);
    assert(encrypted_frame.size() == (header.size() + data.size()+ gcmTag.size()));

    // validate header and extract keyid, cid and Ctr
    int checkHeader = validateAndProcessHeader(header);
    if (checkHeader != static_cast<int>(Status::kOk))
    {
        // error with the given frame, don't pass to the decoder, return error returned by validateAndProcessHeader
        return Result(static_cast<Status>(checkHeader), 0);
    }

    // re-build frame iv with staticIv and frame CTR
    std::unique_ptr<byte []> iv = generateFrameIV();

    // decrypt frame and store it in frame
    if (!mSymCipher.gcm_decrypt_aad(data.data(), static_cast<unsigned int>(data.size()),
                                     header.data(), FRAME_HEADER_LENGTH,
                                     gcmTag.data(), FRAME_GCM_TAG_LENGTH,
                                     iv.get(), FRAME_IV_LENGTH,
                                     frame.data(), frame.size()))
    {
        RTCM_LOG_WARNING("Failed gcm_decrypt_aad decryption with additional authenticated data: mid: %u Cid: %u, PeerId: %s, KeyId: %u, frameCtr: %u",
                         mMid, mPeer.getCid(), mPeer.getPeerid().toString().c_str(), mKeyId, mCtr);
        return Result(Status::kRecoverable, 0); // decryption error, don't pass to the decoder
    }

    // check if decrypted frame size is the expected one
    assert(GetMaxPlaintextByteSize(media_type, encrypted_frame.size()) == frame.size());
    size_t expectedFrameSize = GetMaxPlaintextByteSize(media_type, encrypted_frame.size());
    if (expectedFrameSize != frame.size())
    {
        RTCM_LOG_WARNING("Decrypt: Decrypted frame size: %lu doesn't match with expected size: %lu Cid: %u, PeerId: %s, KeyId: %u, frameCtr: %u",
                               frame.size(), expectedFrameSize, mPeer.getCid(), mPeer.getPeerid().toString().c_str(), mKeyId, mCtr);
        return Result(Status::kRecoverable, 0); // decryption error, don't pass to the decoder
    }
    return Result(Status::kOk, frame.size());
}

size_t MegaDecryptor::GetMaxPlaintextByteSize(cricket::MediaType /*media_type*/, size_t encrypted_frame_size)
{
    return encrypted_frame_size - FRAME_HEADER_LENGTH - FRAME_GCM_TAG_LENGTH;
}

#ifdef __ANDROID__
CaptureModuleAndroid::CaptureModuleAndroid(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread)
    : mCapabilities(capabilities)
{
    JNIEnv* env = webrtc::AttachCurrentThreadIfNeeded();
    startVideoCaptureMID = env->GetStaticMethodID(applicationClass, "startVideoCapture", "(IIILorg/webrtc/SurfaceTextureHelper;Lorg/webrtc/CapturerObserver;Ljava/lang/String;)V");
    if (!startVideoCaptureMID)
    {
        RTCM_LOG_WARNING("Unable to get static method startVideoCapture");
        env->ExceptionClear();
    }

    stopVideoCaptureMID = env->GetStaticMethodID(applicationClass, "stopVideoCapture", "()V");
    if (!stopVideoCaptureMID)
    {
        RTCM_LOG_WARNING("Unable to get static method stopVideoCapture");
        env->ExceptionClear();
    }

    mVideoSource = webrtc::CreateJavaVideoSource(env, thread, false, true);
}

CaptureModuleAndroid::~CaptureModuleAndroid()
{
}

std::set<std::pair<std::string, std::string>> CaptureModuleAndroid::getVideoDevices()
{
    std::set<std::pair<std::string, std::string>> devices;
    JNIEnv* env = webrtc::AttachCurrentThreadIfNeeded();
    if (deviceListMID)
    {
        jobject object = env->CallStaticObjectMethod(applicationClass, deviceListMID);
        jobjectArray* array = reinterpret_cast<jobjectArray*>(&object);
        jsize size = env->GetArrayLength(*array);
        for (jsize i = 0; i < size; i++)
        {
            jstring device = (jstring)env->GetObjectArrayElement(*array, i);
            const char *characters = env->GetStringUTFChars(device, NULL);
            std::string deviceStr = std::string(characters);
            env->ReleaseStringUTFChars(device, characters);
            devices.insert(std::pair<std::string, std::string>(deviceStr, deviceStr));
        }
    }

    return devices;
}

void CaptureModuleAndroid::openDevice(const std::string &videoDevice)
{
    JNIEnv* env = webrtc::AttachCurrentThreadIfNeeded();
    if (startVideoCaptureMID)
    {
        jstring javaDevice = env->NewStringUTF(videoDevice.c_str());
        env->CallStaticVoidMethod(applicationClass, startVideoCaptureMID, (jint)mCapabilities.width, (jint)mCapabilities.height, (jint)mCapabilities.maxFPS, surfaceTextureHelper, mVideoSource->GetJavaVideoCapturerObserver(env).Release(), javaDevice);
    }
}

void CaptureModuleAndroid::releaseDevice()
{
    JNIEnv* env = webrtc::AttachCurrentThreadIfNeeded();
    if (stopVideoCaptureMID)
    {
        env->CallStaticVoidMethod(applicationClass, stopVideoCaptureMID);
    }
}

webrtc::VideoTrackSourceInterface* CaptureModuleAndroid::getVideoTrackSource()
{
    return this;
}

bool CaptureModuleAndroid::is_screencast() const
{
    return mVideoSource->is_screencast();
}

absl::optional<bool> CaptureModuleAndroid::needs_denoising() const
{
    return mVideoSource->needs_denoising();
}

bool CaptureModuleAndroid::GetStats(Stats* stats)
{
    return mVideoSource->GetStats(stats);
}

webrtc::MediaSourceInterface::SourceState CaptureModuleAndroid::state() const
{
    return mVideoSource->state();
}

bool CaptureModuleAndroid::remote() const
{
    return mVideoSource->remote();
}

void CaptureModuleAndroid::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    mVideoSource->AddOrUpdateSink(sink, wants);
}

void CaptureModuleAndroid::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    mVideoSource->RemoveSink(sink);
}

void CaptureModuleAndroid::RegisterObserver(webrtc::ObserverInterface* observer)
{
}

void CaptureModuleAndroid::UnregisterObserver(webrtc::ObserverInterface* observer)
{
}

#endif

}
