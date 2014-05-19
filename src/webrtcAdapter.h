#pragma once
#include <talk/base/common.h>
#include <talk/base/scoped_ref_ptr.h>
#include <talk/app/webrtc/peerconnectioninterface.h>
#include <talk/app/webrtc/jsep.h>
#include <talk/app/webrtc/peerconnectionfactory.h>
#include <talk/app/webrtc/mediastream.h>
#include <talk/app/webrtc/audiotrack.h>
#include <talk/app/webrtc/videotrack.h>
#include <talk/app/webrtc/test/fakeconstraints.h>
#include "guiCallMarshaller.h"
#include "promise.h"

namespace rtcModule
{
/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/

extern talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
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

void init(const Identity* identity);
void cleanup();

unsigned long generateId();
/** Globally initializes the library */
/** The library's local handler for all function call marshal messages.
 * This handler is given for all marshal requests done from this
 * library(via rtcModule::marshalCall()
 * The code of this function must always be in the same dynamic library that
 * marshals requests, i.e. we need this local handler for this library
 * This function can't be inlined
 */
void funcCallMarshalHandler(mega::Message* msg);
static inline void marshalCall(std::function<void()>&& lambda)
{
	mega::marshalCall(::rtcModule::funcCallMarshalHandler,
		std::forward<std::function<void()> >(lambda));
}

class SdpCreateCallbacks: public webrtc::CreateSessionDescriptionObserver
{
public:
  // The implementation of the CreateSessionDescriptionObserver takes
  // the ownership of the |desc|.
	typedef promise::Promise<std::shared_ptr<webrtc::SessionDescriptionInterface> > PromiseType;
	SdpCreateCallbacks(const PromiseType& promise)
		:mPromise(promise){}
	virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
	{
		::rtcModule::marshalCall([this, desc]()
		{
			mPromise.resolve(std::shared_ptr<webrtc::SessionDescriptionInterface>(desc));
			delete this;
		});
	}
	virtual void OnFailure(const std::string& error)
	{
		::rtcModule::marshalCall([this, error]()
		{
		   mPromise.reject(error);
		   delete this;
		});
	}
protected:
	PromiseType mPromise;
};

class SdpSetCallbacks: public webrtc::SetSessionDescriptionObserver
{
public:
	typedef promise::Promise<int> PromiseType;
	SdpSetCallbacks(const PromiseType& promise):mPromise(promise){}
	virtual void OnSuccess()
	{
		 ::rtcModule::marshalCall([this]()
		 {
			 mPromise.resolve(0);
			 delete this;
		 });
	}
	virtual void OnFailure(const std::string& error)
	{
		::rtcModule::marshalCall([this, error]()
		{
			 mPromise.reject(error);
			 delete this;
		});
	}
protected:
	PromiseType mPromise;
};

struct StatsCallbacks: public webrtc::StatsObserver
{
	typedef promise::Promise<std::shared_ptr<std::vector<webrtc::StatsReport> > > PromiseType;
	StatsCallbacks(const PromiseType& promise):mPromise(promise){}
	virtual void OnComplete(const std::vector<webrtc::StatsReport>& reports)
	{
		PromiseType::Type stats(new std::vector<webrtc::StatsReport>(reports)); //this is a std::shared_ptr
		marshalCall([this, stats]()
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
		talk_base::scoped_refptr<webrtc::PeerConnectionInterface>,
		webrtc::PeerConnectionObserver
{
protected:
	struct Observer;
	typedef talk_base::scoped_refptr<webrtc::PeerConnectionInterface> Base;
	std::shared_ptr<Observer> mObserver;
public:
	myPeerConnection(const webrtc::PeerConnectionInterface::IceServers& servers,
	 webrtc::MediaConstraintsInterface* options=NULL)
		:mObserver(new Observer(*this))
	{

		if (gLocalIdentity.isValid())
		{
//TODO: give dtls identity to webrtc
		}
		Base(gWebrtcContext->CreatePeerConnection(
			servers, options, NULL, NULL /*DTLS stuff*/, mObserver.get()));
	}

  SdpCreateCallbacks::PromiseType createOffer(const webrtc::MediaConstraintsInterface* constraints)
  {
	  SdpCreateCallbacks::PromiseType promise;
	  get()->CreateOffer(new talk_base::RefCountedObject<SdpCreateCallbacks>(promise), constraints);
	  return promise;
  }
  SdpCreateCallbacks::PromiseType createAnswer(const webrtc::MediaConstraintsInterface* constraints)
  {
	  SdpCreateCallbacks::PromiseType promise;
	  get()->CreateAnswer(new talk_base::RefCountedObject<SdpCreateCallbacks>(promise), constraints);
	  return promise;
  }
  SdpSetCallbacks::PromiseType setLocalDescription(webrtc::SessionDescriptionInterface* desc)
  {
	  SdpSetCallbacks::PromiseType promise;
	  get()->SetLocalDescription(new talk_base::RefCountedObject<SdpSetCallbacks>(promise), desc);
	  return promise;
  }
  SdpSetCallbacks::PromiseType setRemoteDescription(webrtc::SessionDescriptionInterface* desc)
  {
	  SdpSetCallbacks::PromiseType promise;
	  get()->SetRemoteDescription(new talk_base::RefCountedObject<SdpSetCallbacks>(promise), desc);
	  return promise;
  }
  StatsCallbacks::PromiseType getStats(
	webrtc::MediaStreamTrackInterface* track, webrtc::PeerConnectionInterface::StatsOutputLevel level)
  {
	  StatsCallbacks::PromiseType promise;
	  get()->GetStats(new talk_base::RefCountedObject<StatsCallbacks>(promise), track, level);
	  return promise;
  }
protected:
//PeerConnectionObserver implementation
  struct Observer: public webrtc::PeerConnectionObserver
  {
	  Observer(C& peerConn):mPeerConn(peerConn){}
	  virtual void OnError()
	  { marshalCall([this](){mPeerConn.onError();}); }
	  virtual void OnAddStream(webrtc::MediaStreamInterface* stream)
	  {
		  talk_base::scoped_refptr<webrtc::MediaStreamInterface> spStream(stream);
		  marshalCall([this, spStream] {mPeerConn.onAddStream(spStream);} );
	  }
	  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream)
	  {
		  talk_base::scoped_refptr<webrtc::MediaStreamInterface> spStream(stream);
		  marshalCall([this, spStream] {mPeerConn.OnRemoveStream(spStream);} );
	  }
	  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
	  {
		 std::string sdp;
		 if (!candidate->ToString(&sdp))
		 {
			 printf("ERROR: Failed to serialize candidate\n");
			 return;
		 }
		 std::shared_ptr<std::string> strCand = new std::string;
		 (*strCand)
		  .append("candidate: ").append(sdp).append("\r\n")
		  .append("sdpMid: ").append(candidate->sdp_mid()).append("\r\n")
		  .append("sdpMLineIndex: ").append(std::to_string(candidate->sdp_mline_index())).append("\r\n");

		 marshalCall([this, strCand](){ mPeerConn.onIceCandidate(*strCand); });
	 }
	 virtual void OnInceComplete()
	 {
		 marshalCall([this]() { mPeerConn.onIceComplete(); });
	 }
	 virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
	 {
		 marshalCall([this, newState]() { mPeerConn.onSignalingChange(newState); });
	 }
	 virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
	 {
		 marshalCall([this, newState]() { mPeerConn.onIceConnectionChange(newState);	});
	 }
	 virtual void OnRenegotiationNeeded()
	 {
		 marshalCall([this]() { mPeerConn.onRenegotiationNeeded();});
	 }
  protected:
	C& mPeerConn;
//own callback interface, always called by the GUI thread

  };
};

talk_base::scoped_refptr<webrtc::MediaStreamInterface> cloneMediaStream(
		webrtc::MediaStreamInterface* other, const std::string& label);

struct DeviceManager: public
		std::shared_ptr<cricket::DeviceManagerInterface>
{
	typedef std::shared_ptr<cricket::DeviceManagerInterface> Base;
	DeviceManager()
		:Base(cricket::DeviceManagerFactory::Create())
	{
		if (!get()->Init())
		{
			reset();
			throw std::runtime_error("Can't create device manager");
		}
	}
	DeviceManager(const DeviceManager& other)
	:Base(other){}
};

typedef std::vector<cricket::Device> DeviceList;

struct InputDevices
{
	DeviceList audio;
	DeviceList video;
};

std::shared_ptr<InputDevices> getInputDevices(DeviceManager devMgr);
struct MediaGetOptions
{
	cricket::Device* device;
	webrtc::FakeConstraints constraints;
	MediaGetOptions(cricket::Device* aDevice)
	:device(aDevice){}
};

talk_base::scoped_refptr<webrtc::AudioTrackInterface>
	getUserAudio(const MediaGetOptions& options, DeviceManager& devMgr);

talk_base::scoped_refptr<webrtc::VideoTrackInterface>
	getUserVideo(const MediaGetOptions& options, DeviceManager& devMgr);

}

