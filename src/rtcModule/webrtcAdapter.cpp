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
/* Disable free cloning as we want to keep track of all active streams, so obtain
 * any streams from the corresponding InputAudio/VideoDevice objects

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
*/

void DeviceManager::enumInputDevices()
{
     if (!get()->GetVideoCaptureDevices(&(mInputDevices.video)))
         throw std::runtime_error("Can't enumerate video devices");
     get()->GetAudioInputDevices(&(mInputDevices.audio)); //normal to fail on iOS and other platforms that don't use device manager for audio devices
//         throw std::runtime_error("Can't enumerate audio devices");
}

template<>
void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoSourceInterface>::createSource()
{
    assert(!mSource.get());

    std::unique_ptr<cricket::VideoCapturer> capturer(
        mManager.get()->CreateVideoCapturer(mOptions->device));

    if (!capturer)
        throw std::runtime_error("Could not create video capturer");

    mSource = gWebrtcContext->CreateVideoSource(capturer.release(),
        &(mOptions->constraints));

    if (!mSource.get())
        throw std::runtime_error("Could not create a video source");
}
template<> webrtc::VideoTrackInterface*
InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoSourceInterface>::createTrack()
{
    assert(mSource.get());
    return gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()), mSource).release();
}
template<>
void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoSourceInterface>::freeSource()
{
    if (!mSource)
        return;
    mSource->GetVideoCapturer()->Stop();
    mSource = nullptr;
}

template<>
void InputDeviceShared<webrtc::AudioTrackInterface, webrtc::AudioSourceInterface>::createSource()
{
    assert(!mSource.get());
    mSource = gWebrtcContext->CreateAudioSource(&(mOptions->constraints));
}
template<> webrtc::AudioTrackInterface*
InputDeviceShared<webrtc::AudioTrackInterface, webrtc::AudioSourceInterface>::createTrack()
{
    assert(mSource.get());
    return gWebrtcContext->CreateAudioTrack("a"+std::to_string(generateId()), mSource).release();
}
template<>
void InputDeviceShared<webrtc::AudioTrackInterface, webrtc::AudioSourceInterface>::freeSource()
{
    mSource = nullptr;
}
}
