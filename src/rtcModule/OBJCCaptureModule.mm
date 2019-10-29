
#include "OBJCCaptureModule.h"

#include <AVFoundation/AVFoundation.h>
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"
#import "sdk/objc/native/api/video_capturer.h"

#include "webrtcAdapter.h"

namespace artc
{
    
    OBJCCaptureModule::OBJCCaptureModule(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName)
    {
        if (mCameraViceoCapturer)
        {
            return;
        }
        
        mCaptureDevice = nil;
        for (AVCaptureDevice *captureDevice in AVCaptureDevice.devices)
        {
            // TODO: Choose captureDevice by its localizedName
            //if ([captureDevice.localizedName isEqualToString:[NSString stringWithUTF8String:deviceName.c_str()]])
            if (captureDevice.position == AVCaptureDevicePositionFront)
            {
                mCaptureDevice = captureDevice;
            }
        }
        
        assert(mCaptureDevice != nil);
        mCameraViceoCapturer = [[RTCCameraVideoCapturer alloc] init];
        
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
            else if (diff == currentDiff && pixelFormat == [mCameraViceoCapturer preferredOutputPixelFormat])
            {
                selectedFormat = format;
            }
        }
        
        if (!selectedFormat)
        {
            selectedFormat = mCaptureDevice.activeFormat;
        }
        
        [mCameraViceoCapturer startCaptureWithDevice:mCaptureDevice format:selectedFormat fps:capabilities.maxFPS];
        mRunning = true;
        mVideoSource = webrtc::ObjCToNativeVideoCapturer(mCameraViceoCapturer, gAsyncWaiter->guiThread(), gAsyncWaiter->guiThread());
    }
    
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> OBJCCaptureModule::getVideoSource()
    {
        return mVideoSource;
    }
    
    std::set<std::pair<std::string, std::string>> OBJCCaptureModule::getVideoDevices()
    {
        std::set<std::pair<std::string, std::string>> devices;
        for (AVCaptureDevice *captureDevice in AVCaptureDevice.devices)
        {
            if ([captureDevice hasMediaType:AVMediaTypeVideo])
            {
                std::string deviceName = captureDevice.localizedName.UTF8String;
                devices.insert(std::pair<std::string, std::string>(deviceName, deviceName));
            }
        }
        
        return devices;
    }
    
    void OBJCCaptureModule::openDevice(const std::string &videoDevice)
    {
        if (mRunning)
        {
            return;
        }
        
        [mCameraViceoCapturer startCaptureWithDevice:mCaptureDevice format:mCaptureDevice.activeFormat fps:30];
        mRunning = true;
    }
    
    void OBJCCaptureModule::releaseDevice()
    {
        [mCameraViceoCapturer stopCapture];
        mRunning = false;
    }
    
    webrtc::VideoTrackSourceInterface *OBJCCaptureModule::getVideoTrackSource()
    {
        return mVideoSource;
    }
    
}
