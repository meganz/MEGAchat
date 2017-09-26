#include "webrtcAdapter.h"
#include <webrtc/rtc_base/ssladapter.h>
#include <webrtc/modules/video_capture/video_capture_factory.h>
#include <webrtc/modules/video_capture/video_capture.h>
#include <webrtc/media/engine/webrtcvideocapturerfactory.h>
#include "webrtcAsyncWaiter.h"

namespace artc
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext;

/** Local DTLS Identity */
Identity gLocalIdentity;
static bool gIsInitialized = false;
AsyncWaiter* gAsyncWaiter = nullptr;

bool isInitialized() { return gIsInitialized; }
bool init(const Identity* identity)
{
    if (gIsInitialized)
        return false;

    rtc::ThreadManager* threadMgr = rtc::ThreadManager::Instance(); //ensure the ThreadManager singleton is created
    auto t = threadMgr->CurrentThread();
    if (t) //Main thread is not wrapper if NO_MAIN_THREAD_WRAPPING is defined when building webrtc
    {
        assert(t->IsOwned());
        t->UnwrapCurrent();
        delete t;
        assert(!threadMgr->CurrentThread());
    }
// Put our custom Thread object in the main thread, so our main thread can process
// webrtc messages, in a non-blocking way, integrated with the application's message loop
    gAsyncWaiter = new AsyncWaiter;
    auto thread = new rtc::Thread(gAsyncWaiter);
    gAsyncWaiter->setThread(thread);
    thread->SetName("Main Thread", thread);
    threadMgr->SetCurrentThread(thread);

    rtc::InitializeSSL();
    if (identity)
        gLocalIdentity = *identity;
    else
        gLocalIdentity.clear();
    gWebrtcContext = webrtc::CreatePeerConnectionFactory();
    if (!gWebrtcContext)
        throw std::runtime_error("Error creating peerconnection factory");
    gIsInitialized = true;
    return true;
}

void cleanup()
{
    if (!gIsInitialized)
        return;
    gWebrtcContext = NULL;
    rtc::CleanupSSL();
    rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
    delete gAsyncWaiter->guiThread();
    delete gAsyncWaiter;
    gAsyncWaiter = nullptr;
    gIsInitialized = false;
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
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info)
        throw std::runtime_error("Can't enumerate video devices");
    int numDevices = info->NumberOfDevices();
    auto& devices = mInputDevices.video;
    for (int i = 0; i < numDevices; ++i)
    {
        const uint32_t kSize = 256;
        char name[kSize] = {0};
        char id[kSize] = {0};
        if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
        {
            devices.push_back(cricket::Device(name, id));
        }
    }
    // TODO: Implement audio device enumeration, when webrtc has it again
    // Maybe VoEHardware in src/voice_engine/main/interface/voe_hardware.h
}

template<>
void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::createSource()
{
    assert(!mSource.get());

    cricket::WebRtcVideoDeviceCapturerFactory factory;
    std::unique_ptr<cricket::VideoCapturer> capturer(
        factory.Create(cricket::Device(mOptions->device.name, 0)));
    if (!capturer)
        throw std::runtime_error("Could not create video capturer");

    mSource = gWebrtcContext->CreateVideoSource(capturer.release(),
        &(mOptions->constraints));

    if (!mSource.get())
        throw std::runtime_error("Could not create a video source");
}
template<> webrtc::VideoTrackInterface*
InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::createTrack()
{
    assert(mSource.get());
    return gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()), mSource).release();
}
template<>
void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::freeSource()
{
    if (!mSource)
        return;
//  mSource->GetVideoCapturer()->Stop(); //Seems this must not be called directly, but is called internally by the same webrtc worker thread that started the capture
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
