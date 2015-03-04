#include "webrtcAdapter.h"
#include <webrtc/base/ssladapter.h>
namespace artc
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
 gWebrtcContext;

/** Local DTLS Identity */
Identity gLocalIdentity;
static bool gIsInitialized = false;

bool init(const Identity* identity)
{
    if (gIsInitialized)
        return false;

    gIsInitialized = true;
    rtc::InitializeSSL();
    if (identity)
        gLocalIdentity = *identity;
    else
        gLocalIdentity.clear();
    gWebrtcContext = webrtc::CreatePeerConnectionFactory();
    if (!gWebrtcContext)
        throw std::runtime_error("Error creating peerconnection factory");
    return true;
}

void cleanup()
{
    if (!gIsInitialized)
        return;
    gWebrtcContext = NULL;
    rtc::CleanupSSL();
}


/** Stream id and other ids generator */
unsigned long generateId()
{
    static unsigned long id = 0;
    return ++id;
}

rtc::scoped_refptr<webrtc::MediaStreamInterface> cloneMediaStream(
        webrtc::MediaStreamInterface* other, const std::string& label)
{
    rtc::scoped_refptr<webrtc::MediaStreamInterface> result(
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

void DeviceManager::enumInputDevices()
{
     if (!get()->GetVideoCaptureDevices(&(mInputDevices.video)))
         throw std::runtime_error("Can't enumerate video devices");
     if (!get()->GetAudioInputDevices(&(mInputDevices.audio)))
         throw std::runtime_error("Can't enumerate audio devices");
}

std::shared_ptr<InputVideoDevice>
DeviceManager::getUserVideo(const MediaGetOptions& options)
{
    std::shared_ptr<artc::InputVideoDevice> video(new artc::InputVideoDevice);

    std::unique_ptr<cricket::VideoCapturer> capturer(
        get()->CreateVideoCapturer(options.device));
    if (!capturer)
        throw std::runtime_error("Could not create video capturer");

    auto source =
        gWebrtcContext->CreateVideoSource(capturer.release(), &(options.constraints));
    if (!source.get())
        throw std::runtime_error("Could not create a video source");

    video->mTrack =
      gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()), source);
    if (!video->mTrack)
        throw std::runtime_error("Could not create video track from video capturer");
    return video;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface>
InputVideoDevice::cloneTrack()
{
    rtc::scoped_refptr<webrtc::VideoTrackInterface> vtrack(
        gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()),
        mTrack->GetSource()));
    if (!vtrack.get())
        throw std::runtime_error("Could not create video track from video capturer");
    return vtrack;
}

InputVideoDevice::~InputVideoDevice()
{
    if (mTrack)
        mTrack->GetSource()->GetVideoCapturer()->Stop();
}

std::shared_ptr<InputAudioDevice>
    DeviceManager::getUserAudio(const MediaGetOptions& options)
{
    shared_ptr<InputAudioDevice> audio(new InputAudioDevice);
    audio->mTrack =
         gWebrtcContext->CreateAudioTrack("a"+std::to_string(generateId()),
         gWebrtcContext->CreateAudioSource(&(options.constraints)));
    if (!audio->mTrack.get())
        throw std::runtime_error("Could not create audio track");
    return audio;
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> InputAudioDevice::cloneTrack()
{
    rtc::scoped_refptr<webrtc::AudioTrackInterface> atrack(
      gWebrtcContext->CreateAudioTrack("a"+std::to_string(generateId()),
         mTrack->GetSource()));
    if (!atrack.get())
        throw std::runtime_error("Could not create audio track");
    return atrack;
}

}
