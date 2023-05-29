#ifndef KARERE_DISABLE_WEBRTC
#pragma once
#include "base/gcmpp.h"
#include "base/promise.h"
#include "base/trackDelete.h"
#include "karereCommon.h" //only for std::string on android
#include "rtcmPrivate.h"
#include "rtcCrypto.h"
// disable warnings in webrtc headers
// the same pragma works with both GCC and Clang
#if !defined(__ANDROID__) && (!defined(_WIN32) || !defined(MSC_VER))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#include <api/peer_connection_interface.h>
#include <pc/audio_track.h>
#include <api/jsep_session_description.h>
#include <media/base/video_broadcaster.h>
#include <modules/video_capture/video_capture.h>
#include <rtc_base/ref_counter.h>
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#if !defined(__ANDROID__) && (!defined(_WIN32) || !defined(MSC_VER))
#pragma GCC diagnostic pop
#endif
#include "sfu.h"


#ifdef __OBJC__
@class AVCaptureDevice;
@class RTCCameraVideoCapturer;
#else
typedef struct objc_object AVCaptureDevice;
typedef struct objc_object RTCCameraVideoCapturer;
#endif

#ifdef __ANDROID__
#include <sdk/android/native_api/video/video_source.h>
#endif

namespace artc
{
#if USE_CRYPTOPP && (CRYPTOPP_VERSION >= 600) && (__cplusplus >= 201103L)
using byte = CryptoPP::byte;
#else
typedef unsigned char byte;
#endif

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/

extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext;
extern std::unique_ptr<rtc::Thread> gWorkerThread;
extern std::unique_ptr<rtc::Thread> gSignalingThread;
extern rtc::scoped_refptr<webrtc::AudioProcessing> gAudioProcessing;
extern std::string gFieldTrialStr;

/** Globally initializes the library */
bool init(void *appCtx);

/** De-initializes and cleans up the library and webrtc stack */
void cleanup();
bool isInitialized();
unsigned long generateId();

typedef rtc::scoped_refptr<webrtc::MediaStreamInterface> tspMediaStream;

/** The error type code that will be set when promises returned by this lib are rejected */
enum: uint32_t { ERRTYPE_RTC = 0x3e9a57c0 }; //promise error type
/** The specific error codes of rejected promises */
enum {kCreateSdpFailed = 1, kSetSdpDescriptionFailed = 2};

// meetings WebRTC frame structure lengths (in Bytes)
const static uint8_t FRAME_KEYID_LENGTH = 1;
const static uint8_t FRAME_CID_LENGTH = 3;
const static uint8_t FRAME_CTR_LENGTH = 4;
const static uint8_t FRAME_GCM_TAG_LENGTH = 4;
const static uint8_t FRAME_HEADER_LENGTH = 8;
const static uint8_t FRAME_IV_LENGTH = 12;

// max number of tracks
const static uint8_t MAX_MEDIA_TYPES = 3;

// Current webrtc version calls user callbacks directly from internal webrtc threads,
// so we needed to marshall these callbacks to our main thread. Old webrtc versions rely
// on the main thread to process internal webrtc messages (as any other webrtc thread),
// so they didn need to marshalls the calls on the main thread.
// In case future webrtc versions don't need to do that, comment the
// definition of RTCM_MARSHALL_CALLBACKS to avoid marshalling the callbacks
#define RTCM_MARSHALL_CALLBACKS
#ifdef RTCM_MARSHALL_CALLBACKS
#define RTCM_DO_CALLBACK(code,...)      \
    auto wptr = weakHandle();   \
    karere::marshallCall([wptr, __VA_ARGS__](){ \
        if (wptr.deleted())   \
            return; \
        code;                                    \
    }, this->mAppCtx)
#else
#define RTCM_DO_CALLBACK(code,...)                              \
    assert(rtc::Thread::Current() == gAsyncWaiter->guiThread()); \
    code
#endif

class SdpCreateCallbacks : public webrtc::CreateSessionDescriptionObserver, public karere::DeleteTrackable
{
public:
  // The implementation of the CreateSessionDescriptionObserver takes
  // the ownership of the |desc|.
    typedef promise::Promise<webrtc::SessionDescriptionInterface*> PromiseType;
    void* mAppCtx;
    SdpCreateCallbacks(const PromiseType& promise, void* appCtx)
        :mAppCtx(appCtx), mPromise(promise) {}
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override
    {
        RTCM_DO_CALLBACK(mPromise.resolve(desc); Release(), this, desc);
    }
    void OnFailure(webrtc::RTCError error) override
    {
        RTCM_DO_CALLBACK(
           mPromise.reject(::promise::Error(error.message(), kCreateSdpFailed, ERRTYPE_RTC));
           Release();
        , this, error);

    }

protected:
    PromiseType mPromise;
};

struct IceCandText
{
    std::string candidate;
    std::string sdpMid;
    int sdpMLineIndex;
    IceCandText(const webrtc::IceCandidateInterface* cand)
    {
        if (!cand->ToString(&candidate))
        {
            printf("ERROR: Failed to serialize candidate\n");
            return;
        }
         sdpMid = cand->sdp_mid();
         sdpMLineIndex = cand->sdp_mline_index();
    }
    inline webrtc::JsepIceCandidate* createObject()
    {
        auto cand = new webrtc::JsepIceCandidate(sdpMid, sdpMLineIndex);
        webrtc::SdpParseError err;
        if (!cand->Initialize(candidate, &err))
            throw std::runtime_error("Error parsing ICE candidate: line "+err.line+"\nError: "+err.description);
        return cand;
    }
};

class SdpSetRemoteCallbacks : public rtc::RefCountedObject<webrtc::SetRemoteDescriptionObserverInterface>
        , public karere::DeleteTrackable
{
public:
    typedef promise::Promise<void> PromiseType;
    void* mAppCtx;
    SdpSetRemoteCallbacks(const PromiseType& promise, void* appCtx) :mAppCtx(appCtx), mPromise(promise){}
    void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override
    {
        if (error.ok())
        {
            RTCM_DO_CALLBACK(mPromise.resolve(); Release(), this);
        }
        else
        {
            RTCM_DO_CALLBACK(mPromise.reject(::promise::Error(error.message(), kSetSdpDescriptionFailed, ERRTYPE_RTC)); Release();, this, error);
        }
    }
protected:
    PromiseType mPromise;
};

class SdpSetLocalCallbacks : public rtc::RefCountedObject<webrtc::SetLocalDescriptionObserverInterface>
        , public karere::DeleteTrackable
{
public:
    typedef promise::Promise<void> PromiseType;
    void* mAppCtx;
    SdpSetLocalCallbacks(const PromiseType& promise, void* appCtx) :mAppCtx(appCtx), mPromise(promise)     {}
    void OnSetLocalDescriptionComplete(webrtc::RTCError error) override
    {
        if (error.ok())
        {
            RTCM_DO_CALLBACK(mPromise.resolve(); Release(), this);
        }
        else
        {
            RTCM_DO_CALLBACK(mPromise.reject(::promise::Error(error.message(), kSetSdpDescriptionFailed, ERRTYPE_RTC)); Release();, this, error);
        }
    }
protected:
    PromiseType mPromise;
};

template <class C>
class MyPeerConnection: public rtc::scoped_refptr<webrtc::PeerConnectionInterface>
{
protected:
    //PeerConnectionObserver implementation
    struct Observer: public webrtc::PeerConnectionObserver,
                     public karere::DeleteTrackable    // required to use weakHandle() at RTCM_DO_CALLBACK()
    {
        Observer(C& handler, void* appCtx) : mAppCtx(appCtx), mHandler(handler) {}
        void OnAddStream(scoped_refptr<webrtc::MediaStreamInterface> stream) override
        {
            tspMediaStream spStream(stream);
            RTCM_DO_CALLBACK(mHandler.onAddStream(spStream), this, spStream);
        }

        void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override
        {
            RTCM_DO_CALLBACK(mHandler.onTrack(transceiver), this, transceiver);
        }

        void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override
        {
            RTCM_DO_CALLBACK(mHandler.onRemoveTrack(receiver), this, receiver);
        }

        void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override
        {
            RTCM_DO_CALLBACK(mHandler.onConnectionChange(new_state), this, new_state);
        }

        // pure virtual methods from webrtc::PeerConnectionObserver
        //
        void OnIceCandidate(const webrtc::IceCandidateInterface* /*candidate*/) override {}
        void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState /*newState*/) override {}
        void OnDataChannel(scoped_refptr<webrtc::DataChannelInterface> /*data_channel*/) override {}
        void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState /*new_state*/) override {}

        void* mAppCtx;

    protected:
        /** own callback interface, always called by the GUI thread */
        C& mHandler;
    };
    typedef rtc::scoped_refptr<webrtc::PeerConnectionInterface> Base;
    std::shared_ptr<Observer> mObserver;

public:
    MyPeerConnection():Base(){}
    MyPeerConnection(C& handler, void* appCtx)
        :mObserver(new Observer(handler, appCtx))
    {
        webrtc::PeerConnectionInterface::RTCConfiguration config;
        config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

        webrtc::CryptoOptions cryptoOptions;
        cryptoOptions.sframe.require_frame_encryption = true;
        config.crypto_options = cryptoOptions;

        webrtc::PeerConnectionDependencies dependencies(mObserver.get());
        webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::PeerConnectionInterface>> rtcError =
                gWebrtcContext->CreatePeerConnectionOrError(config, std::move(dependencies));

        if (!rtcError.error().ok())
        {
            RTCM_LOG_WARNING("Error at CreatePeerConnectionOrError: %s", rtcError.error().message());
        }

        Base::operator=(rtcError.value());

        if (!get())
            throw std::runtime_error("Failed to create a PeerConnection object");
    }
    using Base::operator=;
  SdpCreateCallbacks::PromiseType createOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions &options)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise, mObserver->mAppCtx);
      observer->AddRef();
      get()->CreateOffer(observer, options);
      return promise;
  }
  SdpCreateCallbacks::PromiseType createAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions &options)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise, mObserver->mAppCtx);
      observer->AddRef();
      get()->CreateAnswer(observer, options);
      return promise;
  }
  /** Takes ownership of \c desc */
  SdpSetLocalCallbacks::PromiseType setLocalDescription(std::unique_ptr<webrtc::SessionDescriptionInterface> desc)
  {
      SdpSetLocalCallbacks::PromiseType promise;
      auto observer= rtc::scoped_refptr<SdpSetLocalCallbacks>(new SdpSetLocalCallbacks(promise, mObserver->mAppCtx));
      observer->AddRef();
      get()->SetLocalDescription(move(desc), observer);
      return promise;
  }

  /** Takes ownership of \c desc */
  SdpSetRemoteCallbacks::PromiseType setRemoteDescription(std::unique_ptr<webrtc::SessionDescriptionInterface> desc)
  {
      SdpSetRemoteCallbacks::PromiseType promise;
      auto observer = rtc::scoped_refptr<SdpSetRemoteCallbacks>(new SdpSetRemoteCallbacks(promise, mObserver->mAppCtx));
      observer->AddRef();
      get()->SetRemoteDescription(move(desc), observer);
      return promise;
  }
};

class RtcCipher
{
public:
    RtcCipher(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings> cryptoMeetings, IvStatic_t iv, uint32_t mid);
    virtual ~RtcCipher() {}

    // set a key for SymmCipher
    void setKey(const std::string &key);

    // generates an IV for a new frame
    std::unique_ptr<byte []> generateFrameIV();

    void setTerminating();

protected:
    // symmetric cipher
    mega::SymmCipher mSymCipher;

    // sequential number of the packet
    Ctr_t mCtr = 0;

    // keyId of current key armed in SymCipher
    Keyid_t mKeyId = 0;

    // own peer for encryption, any peer for decryption (ownership belongs to Call, whose lifetime is longer than this object)
    const sfu::Peer& mPeer;

    // shared ptr to crypto module for meetings
    std::shared_ptr<::rtcModule::IRtcCryptoMeetings> mCryptoMeetings;

    // static part (8 Bytes) of IV
    IvStatic_t mIv;

    // track mid for log purposes
    uint32_t mMid = 0;

    bool mTerminating = false;
    bool mInitialized = false; // this flag will be set true upon first key is set in cipher
};

class MegaEncryptor
        : public RtcCipher
        , public rtc::RefCountedObject<webrtc::FrameEncryptorInterface>
{
public:
    enum Status { kOk, kRecoverable, kFailedToEncrypt};

    MegaEncryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv, uint32_t mid);
    ~MegaEncryptor() override;

    // increments sequential number of the packet, for each sent frame of that media track.
    void incrementPacketCtr();

    // generates a header for a new frame
    void generateHeader(uint8_t *header);

    // FrameEncryptorInterface
    //
    // encrypts a received frame
    int Encrypt(cricket::MediaType media_type,
                        uint32_t ssrc,
                        rtc::ArrayView<const uint8_t> additional_data,
                        rtc::ArrayView<const uint8_t> frame,
                        rtc::ArrayView<uint8_t> encrypted_frame,
                        size_t* bytes_written) override;
    // returns the encrypted_frame size for a given frame
    size_t GetMaxCiphertextByteSize(cricket::MediaType media_type, size_t frame_size) override;
};

class MegaDecryptor
        : public RtcCipher
        , public rtc::RefCountedObject<webrtc::FrameDecryptorInterface>
{
public:
     MegaDecryptor(const sfu::Peer& peer, std::shared_ptr<::rtcModule::IRtcCryptoMeetings>cryptoMeetings, IvStatic_t iv, uint32_t mid);
    ~MegaDecryptor() override;

    // validates header by checking if CID matches with expected one, also extracts keyId and packet CTR */
    int validateAndProcessHeader(rtc::ArrayView<const uint8_t> header);

    // FrameDecryptorInterface
    //
    // decrypts a received frame
    Result Decrypt(cricket::MediaType media_type,
                   const std::vector<uint32_t>& csrcs,
                   rtc::ArrayView<const uint8_t> additional_data,
                   rtc::ArrayView<const uint8_t> encrypted_frame,
                   rtc::ArrayView<uint8_t> frame) override;
    // returns the plain_frame size for a given encrypted frame
    size_t GetMaxPlaintextByteSize(cricket::MediaType media_type, size_t encrypted_frame_size) override;
};

class LocalStreamHandle
{
public:
    LocalStreamHandle(const char* name="localStream");
    ~LocalStreamHandle();

    karere::AvFlags av();
    karere::AvFlags effectiveAv() const;
    void setAv(karere::AvFlags av);

    void addAudioTrack(const rtc::scoped_refptr<webrtc::AudioTrackInterface>& audio);
    void addVideoTrack(const rtc::scoped_refptr<webrtc::VideoTrackInterface>& video);

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio();
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video();

protected:
    rtc::scoped_refptr<webrtc::AudioTrackInterface> mAudio;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> mVideo;
};


class VideoManager : public rtc::RefCountedObject<webrtc::VideoTrackSourceInterface>
{
public:
    virtual ~VideoManager(){}
    static VideoManager* Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread);
    virtual void openDevice(const std::string &videoDevice) = 0;
    virtual void releaseDevice() = 0;
    virtual webrtc::VideoTrackSourceInterface* getVideoTrackSource() = 0;
    static std::set<std::pair<std::string, std::string>> getVideoDevices();
};

class CaptureScreenModuleLinux : public webrtc::DesktopCapturer::Callback, public VideoManager
{
public:
    CaptureScreenModuleLinux() {};
    ~CaptureScreenModuleLinux() override {};

    // ---- DesktopCapturer::Callback methods ----
    void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) override
    {
        if (result != webrtc::DesktopCapturer::Result::SUCCESS)
        {
            RTCM_LOG_WARNING("OnCaptureResult: error capturing frame");
            return;
        }

        RTCM_LOG_WARNING("OnCaptureResult: Frame captured. With: %d  Height: %d", frame->size().width(), frame->size().height());
    }

    // ---- VideoManager methods ----
    void openDevice(const std::string &) override
    {
        const webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();
        mScreenCapturer = webrtc::DesktopCapturer::CreateScreenCapturer(options);

        webrtc::DesktopCapturer::SourceList sourceList;
        if (mScreenCapturer->GetSourceList(&sourceList))
        {
            // Test with first element in the list
            mScreenCapturer->SelectSource(sourceList.at(0).id);
        }

        if (!mScreenCapturer)
        {
            RTCM_LOG_WARNING("openDevice: error creating DesktopCapturer instance");
            return;
        }

        mScreenCapturer->Start(this);
    }

    void releaseDevice() override
    {
        mScreenCapturer.release();
        mScreenCapturer = nullptr;
    }

    webrtc::VideoTrackSourceInterface* getVideoTrackSource() override
    {
        return this;
    }

    // ---- VideoTrackSourceInterface methods ----
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override
    {
        mBroadcaster.AddOrUpdateSink(sink, wants);
    }

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override
    {
        mBroadcaster.RemoveSink(sink);
    }

    bool is_screencast() const override                                                             { return false; }
    bool SupportsEncodedOutput() const override                                                     { return false; }
    bool GetStats(webrtc::VideoTrackSourceInterface::Stats*) override                               { return false; }
    bool remote() const override                                                                    { return false; }
    absl::optional<bool> needs_denoising() const override                                           { return absl::nullopt; }
    webrtc::MediaSourceInterface::SourceState state() const override                                { return MediaSourceInterface::kLive;}
    void GenerateKeyFrame() override                                                                {}
    void AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*) override          {}
    void RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*) override       {}
    void RegisterObserver(webrtc::ObserverInterface* ) override                                     {}
    void UnregisterObserver(webrtc::ObserverInterface* ) override                                   {}

private:
    std::unique_ptr<webrtc::DesktopCapturer> mScreenCapturer;
    rtc::VideoBroadcaster mBroadcaster;
};

class CaptureModuleLinux : public rtc::VideoSinkInterface<webrtc::VideoFrame>, public VideoManager
{
public:
    explicit CaptureModuleLinux(const webrtc::VideoCaptureCapability &capabilities, bool remote = false);
    virtual ~CaptureModuleLinux() override;

    static std::set<std::pair<std::string, std::string>> getVideoDevices();
    void openDevice(const std::string &videoDevice) override;
    void releaseDevice() override;
    VideoTrackSourceInterface* getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool SupportsEncodedOutput() const override { return  false; }
    void GenerateKeyFrame() override {}

    bool GetStats(webrtc::VideoTrackSourceInterface::Stats* stats) override;

    webrtc::MediaSourceInterface::SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*) override {}

    void RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*) override {}

    void OnFrame(const webrtc::VideoFrame& frame) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;

protected:
    rtc::VideoBroadcaster mBroadcaster;
    webrtc::MediaSourceInterface::SourceState mState;
    bool mRemote;
    rtc::scoped_refptr<webrtc::VideoCaptureModule> mCameraCapturer;
    webrtc::VideoCaptureCapability mCapabilities;
};

#ifdef __APPLE__
class OBJCCaptureModule : public VideoManager
{
public:
    explicit OBJCCaptureModule(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName);
    virtual ~OBJCCaptureModule() {}

    static std::set<std::pair<std::string, std::string>> getVideoDevices();
    void openDevice(const std::string &videoDevice) override;
    void releaseDevice() override;
    webrtc::VideoTrackSourceInterface* getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool GetStats(Stats* stats) override;

    SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;

    bool SupportsEncodedOutput() const override;
    void GenerateKeyFrame() override;

    void AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override;
    void RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override;
    
private:
    bool mRunning = false;
    AVCaptureDevice *mCaptureDevice = nullptr;
    RTCCameraVideoCapturer *mCameraVideoCapturer = nullptr;
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> mVideoSource;
};
#endif

#ifdef __ANDROID__
class CaptureModuleAndroid : public VideoManager
{
public:
    explicit CaptureModuleAndroid(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread);
    virtual ~CaptureModuleAndroid();

    static std::set<std::pair<std::string, std::string>> getVideoDevices();
    void openDevice(const std::string &videoDevice) override;
    void releaseDevice() override;
    webrtc::VideoTrackSourceInterface* getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool GetStats(Stats* stats) override;

    SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;

    bool SupportsEncodedOutput() const override { return false; }
    void GenerateKeyFrame() override {}

    void AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {}
    void RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {}

private:
    bool mRunning = false;
    rtc::scoped_refptr<webrtc::JavaVideoTrackSourceInterface> mVideoSource;
    webrtc::VideoCaptureCapability mCapabilities;
    JNIEnv* mEnv;
};

#endif

}
#endif
