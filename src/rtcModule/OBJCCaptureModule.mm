
#include <AVFoundation/AVFoundation.h>
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"
#import "sdk/objc/native/api/video_capturer.h"

#include "webrtcAdapter.h"

namespace artc
{
    
    OBJCCaptureModule::OBJCCaptureModule(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName)
    {
        if (mCameraVideoCapturer)
        {
            return;
        }
        
        mCaptureDevice = nil;
        
        AVCaptureDeviceDiscoverySession *captureDeviceDiscoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera]
                                                                                                                                mediaType:AVMediaTypeVideo
                                                                                                                                 position:AVCaptureDevicePositionUnspecified];
        
        for (AVCaptureDevice *captureDevice in captureDeviceDiscoverySession.devices)
        {
            if ([captureDevice.localizedName isEqualToString:[NSString stringWithUTF8String:deviceName.c_str()]])
            {
                mCaptureDevice = captureDevice;
            }
        }
        
        assert(mCaptureDevice != nil);
        mCameraVideoCapturer = [[RTCCameraVideoCapturer alloc] init];
        
        AVCaptureDeviceFormat *selectedFormat = nil;
        int currentDiff = INT_MAX;
        int targetWidth = capabilities.width;
        int targetHeight = capabilities.height;
        for (AVCaptureDeviceFormat *format in mCaptureDevice.formats)
        {
            CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
            FourCharCode pixelFormat = CMFormatDescriptionGetMediaSubType(format.formatDescription);
            int diff = abs(targetWidth - dimension.width) + abs(targetHeight - dimension.height);
            if (diff < currentDiff)
            {
                selectedFormat = format;
                currentDiff = diff;
            }
            else if (diff == currentDiff && pixelFormat == [mCameraVideoCapturer preferredOutputPixelFormat])
            {
                selectedFormat = format;
            }
        }
        
        if (!selectedFormat)
        {
            selectedFormat = mCaptureDevice.activeFormat;
        }
        
        [mCameraVideoCapturer startCaptureWithDevice:mCaptureDevice format:selectedFormat fps:capabilities.maxFPS];
        mRunning = true;
        mVideoSource = webrtc::ObjCToNativeVideoCapturer(mCameraVideoCapturer, gSignalingThread.get(), gWorkerThread.get());
    }

    std::set<std::pair<std::string, std::string>> OBJCCaptureModule::getVideoDevices()
    {
        std::set<std::pair<std::string, std::string>> devices;
        
        AVCaptureDeviceDiscoverySession *captureDeviceDiscoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera]
                                                                                                                                mediaType:AVMediaTypeVideo
                                                                                                                                 position:AVCaptureDevicePositionUnspecified];
        
        for (AVCaptureDevice *captureDevice in captureDeviceDiscoverySession.devices)
        {
            std::string deviceName = captureDevice.localizedName.UTF8String;
            devices.insert(std::pair<std::string, std::string>(deviceName, deviceName));
        }
        
        return devices;
    }
    
    void OBJCCaptureModule::openDevice(const std::string &videoDevice)
    {
        if (mRunning)
        {
            return;
        }
        
        [mCameraVideoCapturer startCaptureWithDevice:mCaptureDevice format:mCaptureDevice.activeFormat fps:30];
        mRunning = true;
    }
    
    void OBJCCaptureModule::releaseDevice()
    {
        [mCameraVideoCapturer stopCapture];
        mRunning = false;
    }

    webrtc::VideoTrackSourceInterface* OBJCCaptureModule::getVideoTrackSource()
    {
        return this;
    }

    bool OBJCCaptureModule::is_screencast() const
    {
        return mVideoSource->is_screencast();
    }

    absl::optional<bool> OBJCCaptureModule::needs_denoising() const
    {
        return mVideoSource->needs_denoising();
    }

    bool OBJCCaptureModule::GetStats(Stats* stats)
    {
        return mVideoSource->GetStats(stats);
    }

    webrtc::MediaSourceInterface::SourceState OBJCCaptureModule::state() const
    {
        return mVideoSource->state();
    }

    bool OBJCCaptureModule::remote() const
    {
        return mVideoSource->remote();
    }

    void OBJCCaptureModule::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
    {
        mVideoSource->AddOrUpdateSink(sink, wants);
    }

    void OBJCCaptureModule::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
    {
        mVideoSource->RemoveSink(sink);
    }

    void OBJCCaptureModule::RegisterObserver(webrtc::ObserverInterface* observer)
    {
        mVideoSource->RegisterObserver(observer);
    }

    void OBJCCaptureModule::UnregisterObserver(webrtc::ObserverInterface* observer)
    {
        mVideoSource->UnregisterObserver(observer);
    }

    bool OBJCCaptureModule::SupportsEncodedOutput() const
    {
        return false;
    }

    void OBJCCaptureModule::GenerateKeyFrame()
    {

    }

    void OBJCCaptureModule::AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink)
    {

    }

    void OBJCCaptureModule::RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink)
    {

    }
}
