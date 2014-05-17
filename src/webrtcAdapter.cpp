#include "guiCallMarshaller.h"
#include "webrtcAdapter.h"

namespace rtcModule
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
 gWebrtcContext;

/** Local DTLS Identity */
Identity gLocalIdentity;
void init(const Identity* identity)
{
	if (gWebrtcContext.get())
		throw std::runtime_error("rtcModule already initialized");
	if (identity)
		gLocalIdentity = *identity;
	else
		gLocalIdentity.clear();
	gWebrtcContext = webrtc::CreatePeerConnectionFactory();
	if (!gWebrtcContext)
		throw std::runtime_error("Error creating peerconnection factory");
}

void cleanup()
{
	if (!gWebrtcContext.get())
		return;
	gWebrtcContext = NULL;
}


/** Stream id and other ids generator */
unsigned long generateId()
{
	static unsigned long id = 0;
	return ++id;
}

void funcCallMarshalHandler(mega::Message* msg)
{
	mega::FuncCallMessage::doCall(msg);
}

talk_base::scoped_refptr<webrtc::MediaStreamInterface> cloneMediaStream(
		webrtc::MediaStreamInterface* other, const std::string& label)
{
	talk_base::scoped_refptr<webrtc::MediaStreamInterface> result(
			webrtc::MediaStream::Create(label));
	auto audioTracks = other->GetAudioTracks();
	for (auto at: audioTracks)
	{
		auto newTrack = webrtc::AudioTrack::Create("acloned"+std::to_string(generateId()), at->GetSource());
		result->AddTrack(newTrack);
	}
	auto videoTracks = other->GetVideoTracks();
	for (auto vt: videoTracks)
	{
		auto newTrack =	webrtc::VideoTrack::Create("vcloned"+std::to_string(generateId()), vt->GetSource());
		result->AddTrack(newTrack);
	}
	return result;
}

std::shared_ptr<InputDevices> getInputDevices(DeviceManager devMgr)
{
	std::shared_ptr<InputDevices> result(new InputDevices);
	 if (!devMgr->GetVideoCaptureDevices(&(result->video)))
		 throw std::runtime_error("Can't enumerate video devices");
	 if (!devMgr->GetAudioInputDevices(&(result->audio)))
		 throw std::runtime_error("Can't enumerate audio devices");
	 return result;
}

talk_base::scoped_refptr<webrtc::MediaStreamInterface> getUserMedia(
	const GetUserMediaOptions& options, DeviceManager devMgr, const std::string& label)
{
	talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream(
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
			  gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()),
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
			gWebrtcContext->CreateAudioTrack("a"+std::to_string(generateId()),
				gWebrtcContext->CreateAudioSource(&(options.audioConstraints))));
		if (atrack.get())
			stream->AddTrack(atrack);
		else
			throw std::runtime_error("Could not create audio track");
	}
	return stream;
}
}
