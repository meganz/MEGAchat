#pragma once
#include <api/peer_connection_interface.h>
#include <pc/audio_track.h>
#include <api/jsep_session_description.h>
#include <media/base/video_broadcaster.h>
#include <modules/video_capture/video_capture.h>
#include "base/gcmpp.h"
#include "karereCommon.h" //only for std::string on android
#include "base/promise.h"
#include "webrtcAsyncWaiter.h"
#include "rtcmPrivate.h"
#include <rtc_base/ref_counter.h>

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


namespace std
{
    template< bool B, class T = void >
    using enable_if_t = typename enable_if<B,T>::type;
}

namespace artc
{
/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/

extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext;
extern AsyncWaiter* gAsyncWaiter;

struct Identity
{
    std::string derCert;
    std::string derPrivateKey;
    inline void clear()
    {
        derCert.clear();
        derPrivateKey.clear();
    }
    inline bool isValid() {return !derCert.empty();}
};

/** Globally initializes the library */
bool init(void *appCtx);
/** De-initializes and cleans up the library and webrtc stack */
void cleanup();
bool isInitialized();
unsigned long generateId();

typedef rtc::scoped_refptr<webrtc::MediaStreamInterface> tspMediaStream;
typedef rtc::scoped_refptr<webrtc::SessionDescriptionInterface> tspSdp;
typedef std::unique_ptr<webrtc::SessionDescriptionInterface> supSdp;

/** The error type code that will be set when promises returned by this lib are rejected */
enum: uint32_t { ERRTYPE_RTC = 0x3e9a57c0 }; //promise error type
/** The specific error codes of rejected promises */
enum {kCreateSdpFailed = 1, kSetSdpDescriptionFailed = 2};

// Old webrtc versions called user callbacks directly from internal webrtc threads,
// so we needed to marshall these callbacks to our GUI thread. New webrtc relies
// on the main thread to process internal webrtc messages (as any other webrtc thread),
// and using that mechanism webrtc marshalls the calls on the main/GUI thread by itself,
// thus we don't need to do that. Define RTCM_MARSHALL_CALLBACKS if you want the callbacks
// marshalled by Karere. This should not be needed.

#ifdef RTCM_MARSHALL_CALLBACKS
#define RTCM_DO_CALLBACK(code,...)      \
    ::mega::marshallCall([__VA_ARGS__]() mutable { \
        code;                                    \
    })
#else
#define RTCM_DO_CALLBACK(code,...)                              \
    assert(rtc::Thread::Current() == gAsyncWaiter->guiThread()); \
    code
#endif

class SdpCreateCallbacks : public webrtc::CreateSessionDescriptionObserver
{
public:
  // The implementation of the CreateSessionDescriptionObserver takes
  // the ownership of the |desc|.
    typedef promise::Promise<webrtc::SessionDescriptionInterface*> PromiseType;
    SdpCreateCallbacks(const PromiseType& promise)
        :mPromise(promise){}
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
    {
        RTCM_DO_CALLBACK(mPromise.resolve(desc); Release(), this, desc);
    }
    virtual void OnFailure(const std::string& error)
    {
        RTCM_DO_CALLBACK(
           mPromise.reject(::promise::Error(error, kCreateSdpFailed, ERRTYPE_RTC));
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

class SdpSetCallbacks : public webrtc::SetSessionDescriptionObserver
{
public:
    typedef promise::Promise<void> PromiseType;
    SdpSetCallbacks(const PromiseType& promise)
    :mPromise(promise)
    {}

    virtual void OnSuccess()
    {
        RTCM_DO_CALLBACK(mPromise.resolve(); Release(), this);
    }
    virtual void OnFailure(const std::string& error)
    {
        RTCM_DO_CALLBACK(
             mPromise.reject(::promise::Error(error, kSetSdpDescriptionFailed, ERRTYPE_RTC));
             Release();
        , this, error);
    }

protected:
    PromiseType mPromise;
};

template <class C>
class myPeerConnection: public
        rtc::scoped_refptr<webrtc::PeerConnectionInterface>
{
protected:
    //PeerConnectionObserver implementation
    struct Observer: public webrtc::PeerConnectionObserver
    {
        Observer(C& handler):mHandler(handler){}
        virtual void OnError()
        {
            RTCM_DO_CALLBACK(mHandler.onError(), this);
        }
        virtual void OnAddStream(scoped_refptr<webrtc::MediaStreamInterface> stream)
        {
            tspMediaStream spStream(stream);
            RTCM_DO_CALLBACK(mHandler.onAddStream(spStream), this, spStream);
        }
        virtual void OnRemoveStream(scoped_refptr<webrtc::MediaStreamInterface> stream)
        {
            tspMediaStream spStream(stream);
            RTCM_DO_CALLBACK(mHandler.onRemoveStream(spStream), this, spStream);
        }
        virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
        {
            std::shared_ptr<IceCandText> spCand(new IceCandText(candidate));
            RTCM_DO_CALLBACK(mHandler.onIceCandidate(spCand), this, spCand);
        }
        virtual void OnIceComplete()
        {
            RTCM_DO_CALLBACK(mHandler.onIceComplete(), this);
        }
        virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
        {
            RTCM_DO_CALLBACK(mHandler.onSignalingChange(newState), this, newState);
        }
        virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
        {
            RTCM_DO_CALLBACK(mHandler.onIceConnectionChange(newState), this, newState);
        }
        virtual void OnRenegotiationNeeded()
        {
            RTCM_DO_CALLBACK(mHandler.onRenegotiationNeeded(), this);
        }
        virtual void OnDataChannel(scoped_refptr<webrtc::DataChannelInterface> data_channel)
        {
            rtc::scoped_refptr<webrtc::DataChannelInterface> chan(data_channel);
            RTCM_DO_CALLBACK(mHandler.onDataChannel(chan), this, chan);
        }
        virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
        {
            //TODO: Forward on GUI thread
        }

        virtual void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
        {
            RTCM_DO_CALLBACK(mHandler.onTrack(transceiver));
        }

    protected:
        /** own callback interface, always called by the GUI thread */
        C& mHandler;
    };
    typedef rtc::scoped_refptr<webrtc::PeerConnectionInterface> Base;
    std::shared_ptr<Observer> mObserver;
public:
    myPeerConnection():Base(){}
    myPeerConnection(const webrtc::PeerConnectionInterface::IceServers& servers, C& handler)
        :mObserver(new Observer(handler))
    {
        webrtc::PeerConnectionInterface::RTCConfiguration config;
        config.servers = servers;
        config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
        Base::operator=(gWebrtcContext->CreatePeerConnection(config, NULL, NULL /*DTLS stuff*/, mObserver.get()));
        if (!get())
            throw std::runtime_error("Failed to create a PeerConnection object");
    }
    using Base::operator=;
  SdpCreateCallbacks::PromiseType createOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions &options)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise);
      observer->AddRef();
      get()->CreateOffer(observer, options);
      return promise;
  }
  SdpCreateCallbacks::PromiseType createAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions &options)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise);
      observer->AddRef();
      get()->CreateAnswer(observer, options);
      return promise;
  }
  /** Takes ownership of \c desc */
  SdpSetCallbacks::PromiseType setLocalDescription(webrtc::SessionDescriptionInterface* desc)
  {
      SdpSetCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpSetCallbacks>(promise);
      observer->AddRef();
      get()->SetLocalDescription(observer, desc);
      return promise;
  }
  SdpSetCallbacks::PromiseType setRemoteDescription(webrtc::SessionDescriptionInterface* desc)
  {
      SdpSetCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpSetCallbacks>(promise);
      observer->AddRef();
      get()->SetRemoteDescription(observer, desc);
      return promise;
  }
};

class LocalStreamHandle
{
public:
    LocalStreamHandle(const char* name="localStream");
    ~LocalStreamHandle();

    karere::AvFlags av();
    karere::AvFlags effectiveAv();
    void setAv(karere::AvFlags av);

    void addAudioTrack(const rtc::scoped_refptr<webrtc::AudioTrackInterface>& audio);
    void addVideoTrack(const rtc::scoped_refptr<webrtc::VideoTrackInterface>& video);

    webrtc::AudioTrackInterface* audio();
    webrtc::VideoTrackInterface* video();

protected:
    rtc::scoped_refptr<webrtc::AudioTrackInterface> mAudio;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> mVideo;
};


class VideoManager : public webrtc::VideoTrackSourceInterface
{
public:
    virtual ~VideoManager(){}
    static VideoManager* Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName, rtc::Thread *thread);
    virtual void openDevice(const std::string &videoDevice) = 0;
    virtual void releaseDevice() = 0;
    virtual rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> getVideoTrackSource() = 0;
    static std::set<std::pair<std::string, std::string>> getVideoDevices();

    void AddRef() const override;
    rtc::RefCountReleaseStatus Release() const override;
};

class CaptureModuleLinux : public rtc::VideoSinkInterface<webrtc::VideoFrame>, public VideoManager
{
public:
    explicit CaptureModuleLinux(const webrtc::VideoCaptureCapability &capabilities, bool remote = false);
    virtual ~CaptureModuleLinux() override;

    static std::set<std::pair<std::string, std::string>> getVideoDevices();
    void openDevice(const std::string &videoDevice) override;
    void releaseDevice() override;
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool GetStats(webrtc::VideoTrackSourceInterface::Stats* stats) override;

    webrtc::MediaSourceInterface::SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void OnFrame(const webrtc::VideoFrame& frame) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;

protected:
    rtc::ThreadChecker mWorkerThreadChecker;
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
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool GetStats(Stats* stats) override;

    SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;
    
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
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> getVideoTrackSource() override;

    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;

    bool GetStats(Stats* stats) override;

    SourceState state() const override;
    bool remote() const override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    void RegisterObserver(webrtc::ObserverInterface* observer) override;
    void UnregisterObserver(webrtc::ObserverInterface* observer) override;

private:
    bool mRunning = false;
    rtc::scoped_refptr<webrtc::JavaVideoTrackSourceInterface> mVideoSource;
    webrtc::VideoCaptureCapability mCapabilities;
    JNIEnv* mEnv;
};
#endif

}
