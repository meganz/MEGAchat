#include "webrtcAdapter.h"
#include <rtc_base/ssladapter.h>
#include <modules/video_capture/video_capture_factory.h>
#include <modules/video_capture/video_capture.h>
#include <media/engine/webrtcvideocapturerfactory.h>
#include "webrtcAsyncWaiter.h"

#ifdef __ANDROID__
#include <jni.h>
#include <webrtc/api/videosourceproxy.h>
#include <webrtc/sdk/android/src/jni/androidvideotracksource.h>

extern JavaVM *MEGAjvm;
extern jclass applicationClass;
extern jmethodID startVideoCaptureMID;
extern jmethodID startVideoCaptureWithParametersMID;
extern jmethodID stopVideoCaptureMID;
extern jobject surfaceTextureHelper;
#endif

namespace artc
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext;

static bool gIsInitialized = false;
AsyncWaiter* gAsyncWaiter = nullptr;

bool isInitialized() { return gIsInitialized; }
bool init(void *appCtx)
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
    gAsyncWaiter = new AsyncWaiter(appCtx);
    auto thread = new rtc::Thread(gAsyncWaiter);
    gAsyncWaiter->setThread(thread);
    thread->SetName("Main Thread", thread);
    threadMgr->SetCurrentThread(thread);

    rtc::InitializeSSL();
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
    gWebrtcContext.release();
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
    mInputDevices.audio.clear();
    mInputDevices.video.clear();

    // TODO: Implement audio device enumeration, when webrtc has it again
    // Maybe VoEHardware in src/voice_engine/main/interface/voe_hardware.h
    mInputDevices.audio.push_back(cricket::Device("default", "0"));

    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info)
    {
        RTCM_LOG_WARNING("Could not enumerate video devices. Using the default device.");
        mInputDevices.video.push_back(cricket::Device("default", "0"));
        return;
    }

    int numDevices = info->NumberOfDevices();
    RTCM_LOG_INFO("Enumerate input devices: %d found.", numDevices);
    auto& devices = mInputDevices.video;
    for (int i = 0; i < numDevices; ++i)
    {
        const uint32_t kSize = 256;
        char name[kSize] = {0};
        char id[kSize] = {0};
        if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
        {
            std::string sId = id;
            if (sId.find(":1") == std::string::npos)
            {
                devices.push_back(cricket::Device(name, id));
            }
            else
            {
                devices.insert(devices.begin(), cricket::Device(name, id));
            }
            RTCM_LOG_INFO("Video input device %d: '%s'", i+1, name);
        }
    }

    if (devices.size() == 0)
    {
        RTCM_LOG_WARNING("Not suitable input video devices were found. Default device added.");
        devices.push_back(cricket::Device("default", "0"));
    }
}

template<>
void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::createSource()
{
    assert(!mSource.get());

#ifndef __ANDROID__
    cricket::WebRtcVideoDeviceCapturerFactory factory;
    std::unique_ptr<cricket::VideoCapturer> capturer(
        factory.Create(cricket::Device(mOptions->device.name, 0)));
    if (!capturer)
    {
        RTCM_LOG_WARNING("Could not create video capturer for device '%s'", mOptions->device.name.c_str());
        return;
    }

    mCapturer = capturer.get();
    mSource = gWebrtcContext->CreateVideoSource(capturer.release(),
        &(mOptions->constraints));
#else
    JNIEnv *env;
    MEGAjvm->AttachCurrentThread(&env, NULL);
    rtc::Thread *currentThread = rtc::ThreadManager::Instance()->CurrentThread();
    mSource = new rtc::RefCountedObject<webrtc::AndroidVideoTrackSource>(currentThread, env, surfaceTextureHelper, false);
    rtc::scoped_refptr<webrtc::VideoTrackSourceProxy> proxySource = webrtc::VideoTrackSourceProxy::Create(currentThread, currentThread, mSource);
    if (startVideoCaptureWithParametersMID)
    {
        std::string widthString;
        mOptions->constraints.GetMandatory().FindFirst(webrtc::MediaConstraintsInterface::kMinWidth, &widthString);
        int width = std::atoi(widthString.c_str());
        std::string heightString;
        mOptions->constraints.GetMandatory().FindFirst(webrtc::MediaConstraintsInterface::kMinHeight, &heightString);
        int height = std::atoi(heightString.c_str());
        int fps = 15;
        env->CallStaticVoidMethod(applicationClass, startVideoCaptureWithParametersMID, (jint)width, (jint)height, (jint)fps, (jlong)proxySource.release(), surfaceTextureHelper);
    }
    else
    {
        env->CallStaticVoidMethod(applicationClass, startVideoCaptureMID, (jlong)proxySource.release(), surfaceTextureHelper);
    }

    MEGAjvm->DetachCurrentThread();
#endif

    if (!mSource.get())
    {
        RTCM_LOG_WARNING("Could not create a video source for device '%s'", mOptions->device.name.c_str());
    }
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

#ifdef __ANDROID__
    JNIEnv *env;
    MEGAjvm->AttachCurrentThread(&env, NULL);
    env->CallStaticVoidMethod(applicationClass, stopVideoCaptureMID);
    MEGAjvm->DetachCurrentThread();
#else
    if (mCapturer)
    {
        // This seems to be needed on iOS
        // TODO: Check if this breaks desktop builds
        mCapturer->Stop();
        mCapturer = NULL;
    }
#endif

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
