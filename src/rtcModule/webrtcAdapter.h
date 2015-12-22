#pragma once
#include <webrtc/base/common.h>
#include <webrtc/base/scoped_ref_ptr.h>
#include <talk/app/webrtc/peerconnectioninterface.h>
#include <talk/app/webrtc/jsep.h>
#include <talk/app/webrtc/peerconnectionfactory.h>
#include <talk/app/webrtc/mediastream.h>
#include <talk/app/webrtc/audiotrack.h>
#include <talk/app/webrtc/videotrack.h>
#include <talk/app/webrtc/test/fakeconstraints.h>
#include <talk/app/webrtc/jsepsessiondescription.h>
#include <talk/app/webrtc/jsep.h>
#include "base/gcmpp.h"
#include "karereCommon.h" //only for std::string on android
#include "base/promise.h"

namespace artc
{
/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/

extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
 gWebrtcContext;
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
/** Local DTLS SRTP identity */
extern Identity gLocalIdentity;

/** Globally initializes the library */
bool init(const Identity* identity);
/** De-initializes and cleans up the library and webrtc stack */
void cleanup();

unsigned long generateId();

typedef rtc::scoped_refptr<webrtc::MediaStreamInterface> tspMediaStream;
typedef rtc::scoped_refptr<webrtc::SessionDescriptionInterface> tspSdp;
typedef std::unique_ptr<webrtc::SessionDescriptionInterface> supSdp;

/** The error type code that will be set when promises returned by this lib are rejected */
enum {kRejectType = 0x17c};
/** The specific error codes of rejected promises */
enum {kCreateSdpFailed = 1, kSetSdpDescriptionFailed = 2};

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
        mega::marshallCall([this, desc]() mutable
        {
            mPromise.resolve(desc);
            Release();
        });
    }
    virtual void OnFailure(const std::string& error)
    {
        mega::marshallCall([this, error]()
        {
           mPromise.reject(promise::Error(error, kCreateSdpFailed, kRejectType));
           Release();
        });
    }
protected:
    PromiseType mPromise;
};

struct SdpText
{
    std::string sdp;
    std::string type;
    SdpText(webrtc::SessionDescriptionInterface* desc)
    {
        type = desc->type();
        desc->ToString(&sdp);
    }
    SdpText(const std::string& aSdp, const std::string& aType)
    :sdp(aSdp), type(aType)
    {}
    inline webrtc::JsepSessionDescription* createObject()
    {
        webrtc::JsepSessionDescription* jsepSdp =
            new webrtc::JsepSessionDescription(type);
        webrtc::SdpParseError error;
        if (!jsepSdp->Initialize(sdp, &error))
        {
            delete jsepSdp;
            throw std::runtime_error("Error parsing SDP: line "+error.line+"\nError: "+error.description);
        }
        return jsepSdp;
    }
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
         mega::marshallCall([this]()
         {
             mPromise.resolve();
             Release();
         });
    }
    virtual void OnFailure(const std::string& error)
    {
        mega::marshallCall([this, error]()
        {
             mPromise.reject(promise::Error(error, kSetSdpDescriptionFailed, kRejectType));
             Release();
        });
    }
protected:
    PromiseType mPromise;
};

typedef std::shared_ptr<SdpText> sspSdpText;

struct MappedStatsItem
{
    std::string id;
    std::string type;
    double timestamp;
    std::map<std::string, std::string> values;
    MappedStatsItem(const std::string& aid, const std::string& atype, const double& ats): id(aid), type(atype), timestamp(ats){}
    bool hasVal(const char* name)
    {
        return (values.find(name) != values.end());
    }
    const std::string& strVal(const std::string& name)
    {
        static const std::string empty;
        auto it = values.find(name);
        if (it == values.end())
            return empty;
        return it->second;
    }
    bool longVal(const std::string& name, long& ret)
    {
        auto it = values.find(name);
        if (it == values.end())
            return false;
        ret = std::strtol(it->second.c_str(), nullptr, 10);
        if (errno == ERANGE)
            throw std::runtime_error("MappedStatItem::longVal: Error converting '"+it->second+"' to long");
        return true;
    }
    long longVal(const std::string& name)
    {
        auto it = values.find(name);
        if (it == values.end())
            return 0;
        long ret = std::strtol(it->second.c_str(), nullptr, 10);
        if (errno == ERANGE)
            throw std::runtime_error("MappedStatItem::longVal: Error converting '"+it->second+"' to long");
        return ret;
    }
};

class MappedStatsData: public std::vector<MappedStatsItem>
{
public:
    MappedStatsData(const std::vector<webrtc::StatsReport>& data)
    {
        for (const auto& item: data)
        {
            emplace_back(item.id, item.type, item.timestamp);
            auto& m = back().values;
            for (const auto& val: item.values)
            {
                assert(m.find(val.name) == m.end());
                m[val.name] = val.value;
            }
        }
    }
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
          mega::marshallCall([this](){mHandler.onError();});
      }
      virtual void OnAddStream(webrtc::MediaStreamInterface* stream)
      {
          tspMediaStream spStream(stream);
          mega::marshallCall([this, spStream] {mHandler.onAddStream(spStream);} );
      }
      virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream)
      {
          tspMediaStream spStream(stream);
          mega::marshallCall([this, spStream] {mHandler.onRemoveStream(spStream);} );
      }
      virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
      {
        std::shared_ptr<IceCandText> spCand(new IceCandText(candidate));
        mega::marshallCall([this, spCand]()
        {
            mHandler.onIceCandidate(spCand);
        });
      }
      virtual void OnIceComplete()
      {
          mega::marshallCall([this]() { mHandler.onIceComplete(); });
      }
      virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
      {
          mega::marshallCall([this, newState]() { mHandler.onSignalingChange(newState); });
      }
      virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
      {
          mega::marshallCall([this, newState]() { mHandler.onIceConnectionChange(newState);	});
      }
      virtual void OnRenegotiationNeeded()
      {
          mega::marshallCall([this]() { mHandler.onRenegotiationNeeded();});
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

        if (gLocalIdentity.isValid())
        {
//TODO: give dtls identity to webrtc
        }
        Base::operator=(gWebrtcContext->CreatePeerConnection(
            servers, options, NULL, NULL /*DTLS stuff*/, mObserver.get()));
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
    webrtc::FakeConstraints constraints;
    MediaGetOptions(const cricket::Device& aDevice)
    :device(aDevice){}
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
    std::shared_ptr<MediaGetOptions> mOptions;
    int mRefCount = 0;
    void createSource();
    void freeSource();
    friend class InputDevice<T,S>;
    friend class TrackHandle<InputDevice<T,S>>;
protected:
    typedef InputDeviceShared<T,S> This;
    typedef T Track;
    std::shared_ptr<cricket::DeviceManagerInterface> mManager;
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
    InputDeviceShared(const std::shared_ptr<cricket::DeviceManagerInterface>& manager,
            const std::shared_ptr<MediaGetOptions>& options)
    : mOptions(options), mManager(manager) { assert(mManager && mOptions); }
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
    InputDevice(const std::shared_ptr<cricket::DeviceManagerInterface>& manager,
        const std::shared_ptr<MediaGetOptions>& options)
        : Base(std::make_shared<InputDeviceShared<T,S>>(manager, options)){}

    std::shared_ptr<Handle> getTrack()
    {
        auto shared = Base::get();
        if (!shared->mSource)
            shared->createSource();
        return std::make_shared<Handle>(*this, shared->createTrack());
    }
};
typedef InputDevice<webrtc::AudioTrackInterface, webrtc::AudioSourceInterface> InputAudioDevice;
typedef InputDevice<webrtc::VideoTrackInterface, webrtc::VideoSourceInterface> InputVideoDevice;

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
    void setAvState(karere::AvFlags av)
    {
        if (mAudio && mAudio->track()->enabled() != av.audio)
                mAudio->track()->set_enabled(av.audio);
        if (mVideo && mVideo->track()->enabled() != av.video)
            mVideo->track()->set_enabled(av.video);
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
    webrtc::AudioTrackInterface* audio() { return mAudio?*mAudio:nullptr; }
    webrtc::VideoTrackInterface* video() { return mVideo?*mVideo:nullptr; }
    operator webrtc::MediaStreamInterface*() { return mStream; }
    operator const webrtc::MediaStreamInterface*() const { return mStream; }
};

class DeviceManager: public std::shared_ptr<cricket::DeviceManagerInterface>
{
public:
    struct InputDevices
    {
        DeviceList audio;
        DeviceList video;
    };
protected:
    typedef std::shared_ptr<cricket::DeviceManagerInterface> Base;
    InputDevices mInputDevices;
public:
    DeviceManager()
        :Base(cricket::DeviceManagerFactory::Create())
    {
        if (!get()->Init())
        {
            reset();
            throw std::runtime_error("Can't create device manager");
        }
        enumInputDevices();
    }
    DeviceManager(const DeviceManager& other)
    :Base(other){}
    const InputDevices& inputDevices() const {return mInputDevices;}
    void enumInputDevices();
    InputAudioDevice getUserAudio(const std::shared_ptr<MediaGetOptions>& options)
        { return InputAudioDevice(*this, options); }

    InputVideoDevice getUserVideo(const std::shared_ptr<MediaGetOptions>& options)
        { return InputVideoDevice(*this, options); }
};

}
