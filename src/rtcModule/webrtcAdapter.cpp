#include "rtcmPrivate.h"
#include "webrtcAdapter.h"
#include "webrtcAsyncWaiter.h"
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <modules/video_capture/video_capture_factory.h>
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

namespace artc
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext = nullptr;

static bool gIsInitialized = false;
AsyncWaiter* gAsyncWaiter = nullptr;

bool isInitialized() { return gIsInitialized; }
bool init(void *appCtx)
{
    if (gIsInitialized)
        return false;

    rtc::ThreadManager* threadMgr = rtc::ThreadManager::Instance(); //ensure the ThreadManager singleton is created
    auto t = threadMgr->CurrentThread();
    if (t) //Main thread is not wrapper if NO_MAIN_THREAD_WRAPPING is defined when building webrtc
    {
        assert(t->IsOwned());
        t->UnwrapCurrent();
        delete t;
        assert(!threadMgr->CurrentThread());
    }
// Put our custom Thread object in the main thread, so our main thread can process
// webrtc messages, in a non-blocking way, integrated with the application's message loop
    gAsyncWaiter = new AsyncWaiter(appCtx);
    auto thread = new rtc::Thread(gAsyncWaiter);
    gAsyncWaiter->setThread(thread);
    thread->SetName("Main Thread", thread);
    threadMgr->SetCurrentThread(thread);

    if (gWebrtcContext == nullptr)
    {
        webrtc::field_trial::InitFieldTrialsFromString("WebRTC-GenericDescriptorAdvertised/Enabled/");
        gWebrtcContext = webrtc::CreatePeerConnectionFactory(
                    nullptr /* network_thread */, thread /* worker_thread */,
                    thread, nullptr /* default_adm */,
                    webrtc::CreateBuiltinAudioEncoderFactory(),
                    webrtc::CreateBuiltinAudioDecoderFactory(),
                    webrtc::CreateBuiltinVideoEncoderFactory(),
                    webrtc::CreateBuiltinVideoDecoderFactory(),
                    nullptr /* audio_mixer */, nullptr /* audio_processing */);

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
    gWebrtcContext.release();
    gWebrtcContext = NULL;
    rtc::CleanupSSL();
    rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
    delete gAsyncWaiter->guiThread();
    delete gAsyncWaiter;
    gAsyncWaiter = nullptr;
    gIsInitialized = false;
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
    mWorkerThreadChecker.Detach();
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

bool CaptureModuleLinux::GetStats(webrtc::VideoTrackSourceInterface::Stats *stats)
{
    return false;
}

void CaptureModuleLinux::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
    mBroadcaster.AddOrUpdateSink(sink, wants);
}

void CaptureModuleLinux::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
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
        char deviceName[256];
        char uniqueName[256];
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

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CaptureModuleLinux::getVideoTrackSource()
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

    bool video = av.video();
    if (mVideo && mVideo->enabled() != video)
    {
        mVideo->set_enabled(video);
    }
}

LocalStreamHandle::LocalStreamHandle(const char *name)
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

VideoManager *VideoManager::Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread)
{
#ifdef __APPLE__
    return new OBJCCaptureModule(capabilities, deviceName);
#elif __ANDROID__
    return new CaptureModuleAndroid(capabilities, deviceName, thread);
#else
    return new CaptureModuleLinux(capabilities);
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

void VideoManager::AddRef() const
{
    mRefCount.IncRef();
}

rtc::RefCountReleaseStatus VideoManager::Release() const
{
    const auto status = mRefCount.DecRef();
    if (status == rtc::RefCountReleaseStatus::kDroppedLastRef)
    {
        delete this;
    }

    return status;
}

MegaEncryptor::MegaEncryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv)
    : mMyPeer(peer)
    , mCryptoMeetings(cryptoMeetings)
    , mIv(iv)
{
}

MegaEncryptor::~MegaEncryptor()
{
}

void MegaEncryptor::setEncryptionKey(const std::string& encryptKey)
{
    const unsigned char *encKey = reinterpret_cast<const unsigned char*>(encryptKey.data());
    mSymCipher
            ? mSymCipher->setkey(encKey)
            : mSymCipher.reset(new mega::SymmCipher(encKey));
}

void MegaEncryptor::incrementPacketCtr()
{
    (mCtr < UINT32_MAX)
            ? mCtr++
            : mCtr = 1;   // reset packet ctr if max value has been reached
}

byte *MegaEncryptor::generateHeader()
{
    // header (8 Bytes): <senderCid.3> <keyId.1> <packetCtr.4>
    Cid_t cid = mMyPeer.getCid();
    byte *header = new byte[FRAME_HEADER_LENGTH];
    memcpy(header, &cid, FRAME_CID_LENGTH);

    uint8_t offset = 0;
    Keyid_t keyId = mMyPeer.getCurrentKeyId();
    offset += FRAME_CID_LENGTH;
    memcpy(header + offset, &keyId, FRAME_KEYID_LENGTH);

    offset += FRAME_KEYID_LENGTH;
    memcpy(header + offset, &mCtr, FRAME_CTR_LENGTH);
    return header;
}

byte *MegaEncryptor::generateFrameIV()
{
    // frame iv (12 Bytes): <ctr.4> <randombytes.8> (randombytes = static part)
    byte *iv = new byte[FRAME_IV_LENGTH];
    memcpy(iv, &mCtr, FRAME_CTR_LENGTH);
    memcpy(iv, &mIv, FRAME_IV_LENGTH - FRAME_CTR_LENGTH);
}

// encrypted_frame: <header.8> <encrypted.data.varlen> <GCM_Tag.4>
int MegaEncryptor::Encrypt(cricket::MediaType media_type, uint32_t ssrc, rtc::ArrayView<const uint8_t> additional_data, rtc::ArrayView<const uint8_t> frame, rtc::ArrayView<uint8_t> encrypted_frame, size_t *bytes_written)
{
    if (!frame.size())
    {
        // TODO: manage errors and define error codes
        return 1;
    }

    // set current encryption key in symCipher
    Keyid_t keyId = mMyPeer.getCurrentKeyId();
    std::string encryptionKey = mMyPeer.getKey(keyId);
    if (encryptionKey.empty())
    {
        RTCM_LOG_WARNING("Encrypt: key doesn't found with keyId: %d", keyId);
        //return ERRCODE
    }
    setEncryptionKey(encryptionKey);

    // generate frame iv
    mega::unique_ptr<byte []> iv(generateFrameIV());

    // generate header
    mega::unique_ptr<byte []> header(generateHeader());

    // increment PacketCtr after we have generated header
    incrementPacketCtr();

    // encrypt frame
    std::string encFrame;
    std::string plainFrame;
    plainFrame.resize(frame.size());
    for (size_t i = 0; i < frame.size(); i++)
    {
        plainFrame[i] = static_cast<char>(frame[i]);
    }
    mSymCipher->gcm_encrypt(&plainFrame, iv.get(), FRAME_IV_LENGTH, FRAME_GCM_TAG_LENGTH, &encFrame);

    // add header to the output
    const CryptoPP::byte *headerPtr= header.get();
    for (size_t i = 0; i < FRAME_HEADER_LENGTH; i++)
    {
        encrypted_frame[i] = static_cast<uint8_t>(headerPtr[i]);
    }

    // add encrypted frame to the output
    for (size_t i = 0; i < encFrame.size(); i++)
    {
        // add encrypted frame to the output
        encrypted_frame[i + FRAME_HEADER_LENGTH] = static_cast<uint8_t> (encFrame[i]);
    }

    // set bytes_written to the number of bytes, written in encrypted_frame
    assert(GetMaxCiphertextByteSize(media_type, frame.size()) == encrypted_frame.size());
    *bytes_written = encrypted_frame.size();
    if (GetMaxCiphertextByteSize(media_type, frame.size()) != *bytes_written)
    {
        RTCM_LOG_WARNING("Encrypt: Frame size doesn't match with expected size");
    }
    return 0;
}

size_t MegaEncryptor::GetMaxCiphertextByteSize(cricket::MediaType media_type, size_t frame_size)
{
    // header size + frame size + GCM authentication tag size
    return FRAME_HEADER_LENGTH + frame_size + FRAME_GCM_TAG_LENGTH;
}

MegaDecryptor::MegaDecryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv)
    : mPeer(peer)
    , mCryptoMeetings(cryptoMeetings)
    , mIv(iv)
{
}

MegaDecryptor::~MegaDecryptor()
{
}

void MegaDecryptor::setDecryptionKey(const std::string &decryptKey)
{
    const unsigned char *decKey = reinterpret_cast<const unsigned char*>(decryptKey.data());
    mSymCipher
            ? mSymCipher->setkey(decKey)
            : mSymCipher.reset(new mega::SymmCipher(decKey));

}

/*
 * Frame format: header.8: <keyId.1> <CID.3> <packetCTR.4>
 * Note: (keyId.1 senderCID.3) and packetCtr.4 are little-endian (No need byte-order swap) 32-bit integers.
 */
webrtc::FrameDecryptorInterface::Status MegaDecryptor::validateAndProcessHeader(rtc::ArrayView<const uint8_t> encrypted_frame)
{
    if (!encrypted_frame.size())
    {
        // return error
    }

    uint8_t offset = 0;
    const uint8_t *data = encrypted_frame.data();

    // extract keyId and if it's valid, set key into SymCipher
    Keyid_t keyId = 0;
    memcpy(&keyId, data, FRAME_KEYID_LENGTH);
    std::string decryptionKey = mPeer.getKey(keyId);
    if (decryptionKey.empty())
    {
        RTCM_LOG_WARNING("validateAndProcessHeader: key doesn't found with keyId: %d", keyId);
        //return ERRCODE
    }
    setDecryptionKey(decryptionKey);

    // check if frame CID matches with expected one
    offset += FRAME_KEYID_LENGTH;
    Cid_t peerCid = 0;
    memcpy(&peerCid, data + offset, FRAME_CID_LENGTH);
    if (peerCid != mPeer.getCid())
    {
        RTCM_LOG_WARNING("validateAndProcessHeader: Frame CID doesn't match with expected one. expected: %d, received: %d", mPeer.getCid(), peerCid);
        //return error
    }

    // extract packet ctr and update mCtr (ctr will be used to generate an IV to decrypt the frame)
    offset += FRAME_KEYID_LENGTH;
    memcpy(&mCtr, data + offset, FRAME_CTR_LENGTH);
    return Status::kOk;
}

std::shared_ptr<byte []> MegaDecryptor::generateFrameIV()
{
    std::shared_ptr<byte []>iv(new byte[FRAME_IV_LENGTH]);
    memcpy(iv.get(), &mCtr, FRAME_CTR_LENGTH);
    memcpy(iv.get(), &mIv, FRAME_IV_LENGTH - FRAME_CTR_LENGTH);
    return iv;
}

webrtc::FrameDecryptorInterface::Result MegaDecryptor::Decrypt(cricket::MediaType media_type, const std::vector<uint32_t> &csrcs, rtc::ArrayView<const uint8_t> additional_data, rtc::ArrayView<const uint8_t> encrypted_frame, rtc::ArrayView<uint8_t> frame)
{
    // validate header
    if (validateAndProcessHeader(encrypted_frame) != Status::kOk)
    {
        // return error
    }

    // generate iv using packet CRT received in frame header
    std::shared_ptr<byte[]> iv = generateFrameIV();

    // copy encrypted_frame content into a string
    std::string encFrame;
    std::string plainFrame;
    const char *auxData = reinterpret_cast<const char *>(encrypted_frame.data());
    size_t auxSize = encrypted_frame.size();
    for (unsigned int i = 0; i < auxSize; i++)
    {
        encFrame.push_back(auxData[i]);
    }

    // Remove header
    encFrame.erase(0, FRAME_HEADER_LENGTH);

    // decrypt frame
    mSymCipher->gcm_decrypt_v2(&encFrame, iv.get(), FRAME_IV_LENGTH, FRAME_GCM_TAG_LENGTH, &plainFrame);

    size_t plainFrameSize = plainFrame.size();
    for (unsigned int i = 0; i < plainFrameSize; i++)
    {
        // add decrypted frame to the output
        frame[i] = static_cast<uint8_t> (plainFrame[i]);
    }

    // check if decrypted frame size is the expected one
    assert(GetMaxPlaintextByteSize(media_type, encrypted_frame.size()) == plainFrameSize);
    if (GetMaxPlaintextByteSize(media_type, encrypted_frame.size()) != plainFrameSize)
    {
        RTCM_LOG_WARNING("Plain frame size doesn't match with expected size");
    }
    return Result(Status::kOk, plainFrameSize);
}

size_t MegaDecryptor::GetMaxPlaintextByteSize(cricket::MediaType media_type, size_t encrypted_frame_size)
{
    return encrypted_frame_size - FRAME_HEADER_LENGTH - FRAME_GCM_TAG_LENGTH;
}

#ifdef __ANDROID__
CaptureModuleAndroid::CaptureModuleAndroid(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread)
    : mCapabilities(capabilities)
{
    JNIEnv* env;
    MEGAjvm->GetEnv((void**)&env, JNI_VERSION_1_6);
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
    JNIEnv* env;
    MEGAjvm->GetEnv((void**)&env, JNI_VERSION_1_6);
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
    JNIEnv* env;
    MEGAjvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (startVideoCaptureMID)
    {
        jstring javaDevice = env->NewStringUTF(videoDevice.c_str());
        env->CallStaticVoidMethod(applicationClass, startVideoCaptureMID, (jint)mCapabilities.width, (jint)mCapabilities.height, (jint)mCapabilities.maxFPS, surfaceTextureHelper, mVideoSource->GetJavaVideoCapturerObserver(env).Release(), javaDevice);
    }
}

void CaptureModuleAndroid::releaseDevice()
{
    JNIEnv* env;
    MEGAjvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (stopVideoCaptureMID)
    {
        env->CallStaticVoidMethod(applicationClass, stopVideoCaptureMID);
    }
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CaptureModuleAndroid::getVideoTrackSource()
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
