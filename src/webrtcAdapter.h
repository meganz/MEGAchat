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
#include "karereCommon.h"
#include "base/gcmpp.h"
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
    typedef promise::Promise<int> PromiseType;
    SdpSetCallbacks(const PromiseType& promise)
    :mPromise(promise)
    {}

    virtual void OnSuccess()
    {
         mega::marshallCall([this]()
         {
             mPromise.resolve(0);
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

struct StatsCallbacks: public webrtc::StatsObserver
{
    typedef promise::Promise<std::shared_ptr<std::vector<webrtc::StatsReport> > > PromiseType;
    StatsCallbacks(const PromiseType& promise):mPromise(promise){}
    virtual void OnComplete(const std::vector<webrtc::StatsReport>& reports)
    {
        PromiseType::Type stats(new std::vector<webrtc::StatsReport>(reports)); //this is a std::shared_ptr
        mega::marshallCall([this, stats]()
        {
            mPromise.resolve(stats);
            delete this;
        });
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
        C& mHandler;
        //own callback interface, always called by the GUI thread
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
  StatsCallbacks::PromiseType getStats(
    webrtc::MediaStreamTrackInterface* track, webrtc::PeerConnectionInterface::StatsOutputLevel level)
  {
      StatsCallbacks::PromiseType promise;
      get()->GetStats(new rtc::RefCountedObject<StatsCallbacks>(promise), track, level);
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

class DeviceManager: public
        std::shared_ptr<cricket::DeviceManagerInterface>
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
    rtc::scoped_refptr<webrtc::AudioTrackInterface>
        getUserAudio(const MediaGetOptions& options);
    rtc::scoped_refptr<webrtc::VideoTrackInterface>
        getUserVideo(const MediaGetOptions& options);
    rtc::scoped_refptr<webrtc::AudioTrackInterface>
        cloneAudioTrack(webrtc::AudioTrackInterface* src);
    rtc::scoped_refptr<webrtc::VideoTrackInterface>
        cloneVideoTrack(webrtc::VideoTrackInterface* src);

};

}
