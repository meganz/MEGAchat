#include "webrtcAdapter.h"
#include <rtc_base/ssl_adapter.h>
#include <modules/video_capture/video_capture_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
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
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> gWebrtcContext = nullptr;

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
    if (gWebrtcContext == nullptr)
    {
        gWebrtcContext = webrtc::CreatePeerConnectionFactory(
                    nullptr /* network_thread */, thread /* worker_thread */,
                    nullptr /* signaling_thread */, nullptr /* default_adm */,
                    webrtc::CreateBuiltinAudioEncoderFactory(),
                    webrtc::CreateBuiltinAudioDecoderFactory(),
                    webrtc::CreateBuiltinVideoEncoderFactory(),
                    webrtc::CreateBuiltinVideoDecoderFactory(),
                    nullptr /* audio_mixer */, nullptr /* audio_processing */);
    }

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

//template<>
//void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::createSource()
//{
////    assert(!mSource.get());
////    mSource = CapturerTrackSource::Create();

//#ifndef __ANDROID__

//#else
//    JNIEnv *env;
//    MEGAjvm->AttachCurrentThread(&env, NULL);
//    rtc::Thread *currentThread = rtc::ThreadManager::Instance()->CurrentThread();
//    mSource = new rtc::RefCountedObject<webrtc::AndroidVideoTrackSource>(currentThread, env, surfaceTextureHelper, false);
//    rtc::scoped_refptr<webrtc::VideoTrackSourceProxy> proxySource = webrtc::VideoTrackSourceProxy::Create(currentThread, currentThread, mSource);
//    if (startVideoCaptureWithParametersMID)
//    {
//        std::string widthString;
//        mOptions->constraints.GetMandatory().FindFirst(webrtc::MediaConstraintsInterface::kMinWidth, &widthString);
//        int width = std::atoi(widthString.c_str());
//        std::string heightString;
//        mOptions->constraints.GetMandatory().FindFirst(webrtc::MediaConstraintsInterface::kMinHeight, &heightString);
//        int height = std::atoi(heightString.c_str());
//        int fps = 15;
//        env->CallStaticVoidMethod(applicationClass, startVideoCaptureWithParametersMID, (jint)width, (jint)height, (jint)fps, (jlong)proxySource.release(), surfaceTextureHelper);
//    }
//    else
//    {
//        env->CallStaticVoidMethod(applicationClass, startVideoCaptureMID, (jlong)proxySource.release(), surfaceTextureHelper);
//    }

//    MEGAjvm->DetachCurrentThread();
//#endif

//    if (!mSource.get())
//    {
//        RTCM_LOG_WARNING("Could not create a video source for device");
//    }
//}
//template<> webrtc::VideoTrackInterface*
//InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::createTrack()
//{
//    assert(mSource.get());
//    return gWebrtcContext->CreateVideoTrack("v"+std::to_string(generateId()), mSource).release();
//}
//template<>
//void InputDeviceShared<webrtc::VideoTrackInterface, webrtc::VideoTrackSourceInterface>::freeSource()
//{
//    if (!mSource)
//        return;

//#ifdef __ANDROID__
//    JNIEnv *env;
//    MEGAjvm->AttachCurrentThread(&env, NULL);
//    env->CallStaticVoidMethod(applicationClass, stopVideoCaptureMID);
//    MEGAjvm->DetachCurrentThread();
//#else
////    if (mCapturer)
////    {
////        // This seems to be needed on iOS
////        // TODO: Check if this breaks desktop builds
////        mCapturer->Stop();
////        mCapturer = NULL;
////    }
//#endif

//    mSource = nullptr;
//}


VideoTrackSource::VideoTrackSource(bool remote)
    : mState(webrtc::MediaSourceInterface::kInitializing), mRemote(remote)
{
    mWorkerThreadChecker.Detach();
}

void VideoTrackSource::SetState(webrtc::MediaSourceInterface::SourceState new_state)
{
    if (mState != new_state) {
        mState = new_state;
        FireOnChanged();
    }
}

void VideoTrackSource::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
    mBroadcaster.AddOrUpdateSink(sink, wants);
}

void VideoTrackSource::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
    mBroadcaster.RemoveSink(sink);
}

bool VideoTrackSource::remote() const
{
    return mRemote;
}

CapturerTrackSource* CapturerTrackSource::Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &videoDevice)
{
    return new rtc::RefCountedObject<CapturerTrackSource>(capabilities, videoDevice);
}

void CapturerTrackSource::OnFrame(const webrtc::VideoFrame& frame)
{
    mBroadcaster.OnFrame(frame);
}

CapturerTrackSource::~CapturerTrackSource()
{
    destroy();
}

std::set<std::pair<std::string, std::string>> CapturerTrackSource::getVideoDevices()
{
    std::set<std::pair<std::string, std::string>> videoDevices;
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
        return videoDevices;
    }
    uint32_t numDevices = info->NumberOfDevices();
    for (uint32_t i = 0; i < numDevices; i++)
    {
        char deviceName[256];
        char uniqueName[256];
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> deviceInfo(webrtc::VideoCaptureFactory::CreateDeviceInfo());
        deviceInfo->GetDeviceName(i, deviceName, sizeof(deviceName), uniqueName, sizeof(uniqueName));
        videoDevices.insert(std::pair<std::string, std::string>(deviceName, uniqueName));
    }

    return videoDevices;
}

void CapturerTrackSource::openDevice(const std::string &videoDevice)
{
    mCameraCapturer = webrtc::VideoCaptureFactory::Create(videoDevice.c_str());
    if (!mCameraCapturer)
    {
        return;
    }

    mCameraCapturer->RegisterCaptureDataCallback(this);

    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> deviceInfo(webrtc::VideoCaptureFactory::CreateDeviceInfo());
    webrtc::VideoCaptureCapability capabilities;
    deviceInfo->GetCapability(mCameraCapturer->CurrentDeviceName(), 0, capabilities);
    mCapabilities.interlaced = capabilities.interlaced;
    mCapabilities.videoType = webrtc::VideoType::kI420;

    if (mCameraCapturer->StartCapture(mCapabilities) != 0)
    {
        destroy();
        return;
    }

    RTC_CHECK(mCameraCapturer->CaptureStarted());
}

void CapturerTrackSource::releaseDevice()
{
    destroy();
}

CapturerTrackSource::CapturerTrackSource(const webrtc::VideoCaptureCapability &capabilities, const std::string &videoDevice)
    : VideoTrackSource(false)
{
    mCapabilities = capabilities;
}

void CapturerTrackSource::destroy()
{
    if (!mCameraCapturer)
        return;

    mCameraCapturer->StopCapture();
    mCameraCapturer->DeRegisterCaptureDataCallback();
    mCameraCapturer = nullptr;
}

rtc::VideoSourceInterface<webrtc::VideoFrame>* CapturerTrackSource::source()
{
    return this;
}

}
