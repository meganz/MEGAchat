#pragma once
//#include <p2p/base/common.h>
#include <api/peerconnectioninterface.h>
#include <api/jsep.h>
#include <media/base/device.h>
#include <media/base/videosourceinterface.h>
#include <pc/peerconnectionfactory.h>
#include <api/mediastream.h>
#include <api/mediastreaminterface.h>
#include <pc/audiotrack.h>
#include <pc/videotrack.h>
#include <api/test/fakeconstraints.h>
#include <api/jsepsessiondescription.h>
#include "base/gcmpp.h"
#include "karereCommon.h" //only for std::string on android
#include "base/promise.h"
#include "webrtcAsyncWaiter.h"
#include "rtcmPrivate.h"

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

class SdpCreateCallbacks: public webrtc::CreateSessionDescriptionObserver
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
           mPromise.reject(promise::Error(error, kCreateSdpFailed, ERRTYPE_RTC));
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

class SdpSetCallbacks: public webrtc::SetSessionDescriptionObserver
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
             mPromise.reject(promise::Error(error, kSetSdpDescriptionFailed, ERRTYPE_RTC));
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

    protected:
        /** own callback interface, always called by the GUI thread */
        C& mHandler;
    };
    typedef rtc::scoped_refptr<webrtc::PeerConnectionInterface> Base;
    std::shared_ptr<Observer> mObserver;
public:
    myPeerConnection():Base(){}
    myPeerConnection(const webrtc::PeerConnectionInterface::IceServers& servers,
     C& handler, webrtc::MediaConstraintsInterface* options)
        :mObserver(new Observer(handler))
    {
        webrtc::PeerConnectionInterface::RTCConfiguration config;
        config.servers = servers;
        Base::operator=(gWebrtcContext->CreatePeerConnection(
            config, options, NULL, NULL /*DTLS stuff*/, mObserver.get()));
        if (!get())
            throw std::runtime_error("Failed to create a PeerConnection object");
    }
    using Base::operator=;
  SdpCreateCallbacks::PromiseType createOffer(const webrtc::MediaConstraintsInterface* constraints)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise);
      observer->AddRef();
      get()->CreateOffer(observer, constraints);
      return promise;
  }
  SdpCreateCallbacks::PromiseType createAnswer(const webrtc::MediaConstraintsInterface* constraints)
  {
      SdpCreateCallbacks::PromiseType promise;
      auto observer = new rtc::RefCountedObject<SdpCreateCallbacks>(promise);
      observer->AddRef();
      get()->CreateAnswer(observer, constraints);
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

rtc::scoped_refptr<webrtc::MediaStreamInterface> cloneMediaStream(
        webrtc::MediaStreamInterface* other, const std::string& label);

typedef std::vector<cricket::Device> DeviceList;

struct MediaGetOptions
{
    const cricket::Device& device;
    webrtc::FakeConstraints& constraints;
    MediaGetOptions(const cricket::Device& aDevice, webrtc::FakeConstraints& constr)
    :device(aDevice), constraints(constr){}
};

class DeviceManager;

template<class T>
class TrackHandle
{
    T mDevice; //shared_ptr
    typename T::Shared::Track* mTrack;
public:
    TrackHandle(const T& device, typename T::Shared::Track* track);
    ~TrackHandle();
    typename T::Shared::Track* track() const { return mTrack; }
    operator typename T::Shared::Track*() { return mTrack; }
    operator const typename T::Shared::Track*() const { return mTrack; }
};
template <class T, class S>
class InputDevice;
template <class T, class S>
class InputDeviceShared
{
private:
    rtc::scoped_refptr<S> mSource;
    cricket::VideoCapturer *mCapturer;
    std::shared_ptr<MediaGetOptions> mOptions;
    int mRefCount = 0;
    void createSource();
    void freeSource();
    friend class InputDevice<T,S>;
    friend class TrackHandle<InputDevice<T,S>>;
protected:
    typedef InputDeviceShared<T,S> This;
    typedef T Track;
    DeviceManager& mManager;
    void refSource() { mRefCount++; }
    void unrefSource()
    {
        if (--mRefCount <= 0)
        {
            assert(mRefCount == 0);
            freeSource();
        }
    }
    Track* createTrack();
    friend class TrackHandle<This>;
public:
    InputDeviceShared(DeviceManager& manager, const std::shared_ptr<MediaGetOptions>& options)
    : mCapturer(NULL), mOptions(options), mManager(manager) { assert(mOptions); }
    ~InputDeviceShared()
    {
        if (mRefCount)
        {
            fprintf(stderr, "ERROR: artc::InputDevice: Media track is still being used");
            freeSource();
        }
    }
};

template <class T, class S>
class InputDevice: public std::shared_ptr<InputDeviceShared<T,S>>
{
protected:
    typedef InputDeviceShared<T,S> Shared;
    typedef std::shared_ptr<Shared> Base;
    typedef TrackHandle<InputDevice<T,S>> Handle;
    friend Handle;
public:
    const MediaGetOptions& mediaOptions() const { return *Base::get()->mOptions; }
    InputDevice(): Base(nullptr){}
    InputDevice(DeviceManager& manager, const std::shared_ptr<MediaGetOptions>& options)
        : Base(std::make_shared<InputDeviceShared<T,S>>(manager, options)){}

    std::shared_ptr<Handle> getTrack()
    {
        auto shared = Base::get();
        if (!shared->mSource)
        {
            shared->createSource();
            if (!shared->mSource)
            {
                RTCM_LOG_WARNING("getTrack: Cannot create video source");
                return nullptr;
            }
        }
        return std::make_shared<Handle>(*this, shared->createTrack());
    }
};
typedef InputDevice<webrtc::AudioTrackInterface, webrtc::AudioSourceInterface> InputAudioDevice;
typedef InputDevice<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface> InputVideoDevice;

template<class T>
inline TrackHandle<T>::TrackHandle(const T& device, typename T::Shared::Track* track)
:mDevice(device), mTrack(track)
{
    assert(track);
    mDevice->refSource();
}
template<class T>
inline TrackHandle<T>::~TrackHandle()
{
    mTrack->Release();
    mDevice->unrefSource();
}

typedef TrackHandle<InputAudioDevice> LocalAudioTrackHandle;
typedef TrackHandle<InputVideoDevice> LocalVideoTrackHandle;

class LocalStreamHandle
{
protected:
    std::shared_ptr<LocalAudioTrackHandle> mAudio;
    std::shared_ptr<LocalVideoTrackHandle> mVideo;
    tspMediaStream mStream;
public:
    karere::AvFlags av() { return karere::AvFlags(mAudio.get(), mVideo.get()); }
    karere::AvFlags effectiveAv()
    { return karere::AvFlags(mAudio && mAudio->track()->enabled(), mVideo && mVideo->track()->enabled()); }
    void setAv(karere::AvFlags av)
    {
        bool audio = av.audio();
        bool video = av.video();
        if (mAudio && mAudio->track()->enabled() != audio)
            mAudio->track()->set_enabled(audio);
        if (mVideo && mVideo->track()->enabled() != video)
            mVideo->track()->set_enabled(video);
    }
    LocalStreamHandle(const std::shared_ptr<LocalAudioTrackHandle>& aAudio,
        const std::shared_ptr<LocalVideoTrackHandle>& aVideo, const char* name="localStream")
    :mAudio(aAudio), mVideo(aVideo), mStream(gWebrtcContext->CreateLocalMediaStream(name))
    {
        if (!mStream.get())
            throw std::runtime_error("MyStream: Error creating stream object");
        bool ok = true;
        if (aAudio)
            ok &= mStream->AddTrack(*aAudio);
        if (aVideo)
            ok &= mStream->AddTrack(*aVideo);
        if (!ok)
            throw std::runtime_error("Error adding track to media stream");
    }
    ~LocalStreamHandle()
    { //make sure the stream is released before the tracks
        mStream = nullptr;
    }
    webrtc::AudioTrackInterface* audio() { return mAudio?*mAudio:(webrtc::AudioTrackInterface*)nullptr; }
    webrtc::VideoTrackInterface* video() { return mVideo?*mVideo:(webrtc::VideoTrackInterface*)nullptr; }
    tspMediaStream& stream() { return mStream; }
    operator webrtc::MediaStreamInterface*() { return mStream; }
    operator const webrtc::MediaStreamInterface*() const { return mStream; }
};

class DeviceManager
{
public:
    struct InputDevices
    {
        DeviceList audio;
        DeviceList video;
    };
protected:
    InputDevices mInputDevices;
public:
    DeviceManager()
    {
        enumInputDevices();
    }
    const InputDevices& inputDevices() const {return mInputDevices;}
    void enumInputDevices();
    InputAudioDevice getUserAudio(const std::shared_ptr<MediaGetOptions>& options)
        { return InputAudioDevice(*this, options); }

    InputVideoDevice getUserVideo(const std::shared_ptr<MediaGetOptions>& options)
        { return InputVideoDevice(*this, options); }
};

}
