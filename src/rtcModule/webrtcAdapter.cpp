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
std::unique_ptr<rtc::Thread> gThread = nullptr;

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
    gThread = std::unique_ptr<rtc::Thread>(new rtc::Thread(gAsyncWaiter));
    gAsyncWaiter->setThread(gThread.get());
    gThread->SetName("Main Thread", gThread.get());
    threadMgr->SetCurrentThread(gThread.get());

    rtc::InitializeSSL();
    if (gWebrtcContext == nullptr)
    {
        gWebrtcContext = webrtc::CreatePeerConnectionFactory(
                    nullptr /* network_thread */, gThread.get() /* worker_thread */,
                    gThread.get() /* signaling_thread */, nullptr /* default_adm */,
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

CaptureModuleLinux::CaptureModuleLinux(const webrtc::VideoCaptureCapability &capabilities, bool remote)
    : mState(webrtc::MediaSourceInterface::kInitializing), mRemote(remote), mCapabilities(capabilities)
{
    mWorkerThreadChecker.Detach();
}

CaptureModuleLinux::~CaptureModuleLinux()
{
}

void CaptureModuleLinux::SetState(webrtc::MediaSourceInterface::SourceState new_state)
{
    if (mState != new_state) {
        mState = new_state;
        FireOnChanged();
    }
}

void CaptureModuleLinux::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
    mBroadcaster.AddOrUpdateSink(sink, wants);
}

void CaptureModuleLinux::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    RTC_DCHECK(mWorkerThreadChecker.IsCurrent());
    mBroadcaster.RemoveSink(sink);
}

bool CaptureModuleLinux::remote() const
{
    return mRemote;
}

void CaptureModuleLinux::OnFrame(const webrtc::VideoFrame& frame)
{
    mBroadcaster.OnFrame(frame);
}

std::set<std::pair<std::string, std::string> > CaptureModuleLinux::getVideoDevices()
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

void CaptureModuleLinux::openDevice(const std::string &videoDevice)
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
        return;
    }

    RTC_CHECK(mCameraCapturer->CaptureStarted());
}

void CaptureModuleLinux::releaseDevice()
{
    if (mCameraCapturer)
    {
        mCameraCapturer->StopCapture();
        mCameraCapturer->DeRegisterCaptureDataCallback();
        mCameraCapturer = nullptr;
    }
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CaptureModuleLinux::getVideoTrackSource()
{
    return this;
}

CapturerTrackSource* CapturerTrackSource::Create(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName)
{
    return new rtc::RefCountedObject<CapturerTrackSource>(capabilities, deviceName);
}

CapturerTrackSource::~CapturerTrackSource()
{
    releaseDevice();
}

std::set<std::pair<std::string, std::string>> CapturerTrackSource::getVideoDevices()
{

#ifdef __APPLE__
    return OBJCCaptureModule::getVideoDevices();
#else
    return CaptureModuleLinux::getVideoDevices();
#endif
}

void CapturerTrackSource::openDevice(const std::string &videoDevice)
{
    VideoManager *videoManager = dynamic_cast<VideoManager *>(mCaptureModule.get());
    assert(videoManager);
    videoManager->openDevice(videoDevice);
}

void CapturerTrackSource::releaseDevice()
{
    VideoManager *videoManager = dynamic_cast<VideoManager *>(mCaptureModule.get());
    assert(videoManager);
    videoManager->releaseDevice();
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>CapturerTrackSource::getVideoTrackSource()
{
    return mCaptureModule;
}

CapturerTrackSource::CapturerTrackSource(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName)
{

#ifdef __APPLE__
    mCaptureModule = rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>(new OBJCCaptureModule(capabilities, deviceName));
#else
    mCaptureModule = rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>(new CaptureModuleLinux(capabilities));
#endif
}

}
