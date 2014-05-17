#include "guiCallMarshaller.h"
#include "webrtcAdapter.h"

namespace rtcModule
{
/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/

talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
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

Identity gLocalIdentity;

unsigned long generateId()
{
	static unsigned long id = 0;
	return ++id;
}

void init(const Identity* identity)
{
	if (gWebrtcContext.get())
		throw std::runtime_error("rtcModule already initialized");
	if (identity)
		gLocalIdentity = *identity;
	else
		gLocalIdentity.clear();
	gWebrtcContext.reset(webrtc::CreatePeerConnectionFactory());
	if (!gWebrtcContext)
		throw std::runtime_error("Error creating peerconnection factory");
}

void cleanup()
{
	if (!gWebrtcContext.get())
		return;
	gWebrtcContext.reset(NULL);
}

void funcCallMarshalHandler(karere::Message* msg)
{
	FuncCallMessage::doCall(msg);
}

inline void marshalCall(Message::Lambda&& lambda)
{
	mega::marshalCall(::rtcModule::funcCallMarshalHandler, std::forward(Message::Lambda>(lambda)));
}

class SdpCreateCallbacks: public webrtc::CreateSessionDescriptionObserver
{
public:
  // The implementation of the CreateSessionDescriptionObserver takes
  // the ownership of the |desc|.
	typedef promise::Promise<std::shared_ptr<webrtc::SessionDescriptionInterface> > PromiseType;
	SdpCreateCallbacks(const PromiseType& promise)
		:mPromise(promise){}
	virtual void OnSuccess(SessionDescriptionInterface* desc)
	{
		::rtcModule::marshalCall([this, desc]()
		{
			mPromise.resolve(std::shared_pointer<webrtc::SessionDescriptionInterface>(desc));
			delete this;
		});
	}
	virtual void OnFailure(const std::string& error)
	{
		::rtcModule::marshalCall([this]()
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
	typedef Promise<int> PromiseType;
	SdpSetCallbacks(const PromiseType& promise):mPromise(promise){}
	virtual void OnSuccess()
	{
		 ::rtcModule::marshalCall([this]()
		 {
			 mPromise.resolve(0);
			 delete this;
		 });
	}
	virtual void OnError(std::string& error)
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

class StatsCallbacks: public webrtc::StatsObserver
{
	typedef Promise<std::shared_ptr<std::vector<webrtc::StatsReport> > > PromiseType;
	StatsCallbacks(const PromiseType& promise):mPromise(promise){}
	virtual void OnComplete(const std::vector<webrtc::StatsReport> &reports)
	{
		PromiseType::Type stats(reports); //this is a std::shared_ptr
		marshalCall([this, stats]()
		{
			mPromise.resolve(stats);
			delete this;
		});
	}
};

class myPeerConnection: public
		talk_base::scoped_refptr<webrtc::PeerConnectionInterface>,
		webrtc::PeerConnectionObserver
{
protected:
	typedef talk_base::scoped_refptr<webrtc::PeerConnectionInterface> Base;
	std::shared_ptr<Observer> mObserver;
public:
	myPeerConnection(const webrtc::PeerConnectionInterface::IceServers& servers,
		webrtc::MediaConstraintsInterface* options=NULL)
	{
		mObserver = new Observer(*this);
		if (gLocalIdentity.isValid())
		{
//TODO: give dtls identity to webrtc
		}
		Base(mWebrtcContext->CreatePeerConnection(servers, options, NULL, dfgdfg, mObserver));
	}

  SdpCreateCallbacks::PromiseType createOffer(const MediaConstraintsInterface* constraints)
  {
	  SdpCreateCallbacks::PromiseType promise;
	  get()->CreateOffer(new SdpCreateCallbacks(promise), constraints);
	  return promise;
  }
  SdpCreateCallbacks::PromiseType createAnswer(const MediaConstraintsInterface* constraints)
  {
	  SdpCreateCallbacks::PromiseType promise;
	  get()->CreateAnswer(new SdpCreateCallbacks(promise), constraints);
	  return promise;
  }
  SdpSetCallbacks::PromiseType setLocalDescription(SessionDescriptionInterface* desc)
  {
	  SdpSetCallbacks::PromiseType promise;
	  get()->SetLocalDescription(new SdpSetCallbacks(promise), desc);
	  return promise;
  }
  SdpSetCallbacks::PromiseType setRemoteDescription(SessionDescriptionInterface* desc)
  {
	  SdpSetCallbacks::PromiseType promise;
	  get()->SetRemoteDescription(new SdpSetCallbacks(promise), desc);
	  return promise;
  }
  StatsCallbacks::PromiseType getStats(
	webrtc::MediaStreamTrackInterface* track, StatsOutputLevel level)
  {
	  StatsCallbacks::PromiseType promise;
	  get()->GetStats(new StatsCallbacks(promise), track, level);
	  return promise;
  }
protected:
//PeerConnectionObserver implementation
  struct Observer
  {
	  Observer(myPeerConnection& peerConn):mPeerConn(peerConn){}
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
			 LOG_ERROR("Failed to serialize candidate");
			 return;
		 }
		 std::shared_ptr<std::string> strCand = new std::string;
		 (*strCand)
		  .append("candidate: ").append(sdp).append("\r\n")
		  .append("sdpMid: ").append(candidate->sdp_mid()).append('\r\n')
		  .append("sdpMLineIndex: ").append(candidate->sdp_mline_index()).append('\r\n');

		 marshalCall([this, strCand](){ mPeerConn.onIceCandidate(*strCand); });
	 }
	 virtual void OnInceComplete()
	 {
		 marshalCall([this]() { mPeerConn.onIceComplete(); });
	 }
	 virtual void OnSignalingChange(PeerConnectionInterface::SignalingState newState)
	 {
		 marshalCall([this, newState]() { mPeerConn.onSignalingChange(newState); });
	 }
	 virtual void OnIceConnectionChange(PeerConnectionInterface::IceConnectionState newState)
	 {
		 marshalCall([this, newState]() { mPeerConn.onIceConnectionChange(newState);	});
	 }
	 virtual void OnRenegotiationNeeded()
	 {
		 marshalCall([this]() { mPeerConn.onRenegotiationNeeded();});
	 }
  protected:
	myPeerConnection& mPeerConn;
  };
};

talk_base::scoped_refptr<MediaStreamInterface> cloneMediaStream(MediaStreamInterface* other, const string& label)
{
	talk_base::scoped_refptr<MediaStreamInterface> result =
			webrtc::MediaStream::Create(label);
	auto audioTracks = other.getAudioTracks();
	for (auto at: audioTracks)
	{
		auto newTrack = AudioTrack::Create("acloned"+std::to_string(generateId()), at->GetSource());
		resul->AddTrack(newTrack);
	}
	auto videoTracks = other.getVideoTracks();
	for (auto vt: videoTracks)
	{
		auto newTrack = VideoTrack::Create("vcloned"+std::to_string(generateId()), vt->GetSource());
		resul->AddTrack(newTrack);
	}
	return result;
}

typedef std::vector<cricket::Device> DeviceList;
struct DeviceManager: public talk_base::scoped_ptr<cricket::DeviceManagerInterface>
{
	typedef talk_base::scoped_ptr<cricket::DeviceManagerInterface> Base;
	DeviceManager()
	{
		reset(cricket::DeviceManagerFactory::Create());
		if (!devMgr->Init())
		{
			reset(NULL);
			throw std::runtime_error("Can't create device manager");
		}
	}
	DeviceManager(const DeviceManager& other)
	:Base(other){}
};

void getInputDevices(DeviceList& audio, DeviceList& video,
		DeviceManager devMgr)
{
	 if (!dev_manager->GetVideoCaptureDevices(&video))
		 throw std::runtime_error("Can't enumerate video devices");
	 if (!devMgr->GetAudioInputDevices(&audio))
		 throw std::runtime_error("Can't enumerate audio devices");
}

struct GetUserMediaOptions
{
	cricket::Device* audio;
	cricket::Device* video;
	webrtc::FakeConstraints audioConstraints;
	webrtc::FakeConstraints videoConstraints;
};

talk_base::scoped_refptr<MediaStreamInterface> getUserMedia(
	const GetUserMediaOptions& options, DeviceManager devMgr, const std::string& label)
{
	talk_base::scoped_refptr<MediaStreamInterface> stream(
		gWebrtcContext->CreateLocalMediaStream(label));
	if (!stream.get())
		throw std::runtime_error("Error creating local media stream object");
	if (options.video)
	{
		cricket::VideoCapturer* capturer =
			devMgr->CreateVideoCapturer(*(options.video));
		if (capturer)
		{
			talk_base::scoped_refptr<webrtc::VideoTrackInterface> vtrack(
			  gPeerConFactory->CreateVideoTrack("v"+std::to_string(generateId()),
				gWebrtcContext->CreateVideoSource(capturer, &(options.videoConstraints))));
			if (vtrack.get())
				stream->AddTrack(vtrack);
			else
				throw std::runtime_error("Could not create video track from video capturer");
		}
	}
	if (options.audio)
	{
		talk_base::scoped_refptr<webrtc::AudioTrackInterface> atrack(
			gPeerConneFactory->CreateAudioTrack("a"+std::to_string(generateId()),
				gWebrtcContext->CreateAudioSource(&(options.audioConstraints))));
		if (atrack.get())
			stream->AddTrack(atrack);
		else
			throw std::runtime_errror("Could not create audio track");
	}
	return stream;
}
