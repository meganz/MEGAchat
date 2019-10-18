
#include "OBJCCaptureModule.h"

#include <AVFoundation/AVFoundation.h>
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"
#import "sdk/objc/native/api/video_capturer.h"

#include "webrtcAdapter.h"

namespace artc
{
    
    OBJCCaptureModule::OBJCCaptureModule(const std::string &deviceName)
    {
        if (mCameraViceoCapturer)
        {
            return;
        }
        
        mCaptureDevice = nil;
        for (AVCaptureDevice *captureDevice in AVCaptureDevice.devices)
        {
            if ([captureDevice.localizedName isEqualToString:[NSString stringWithUTF8String:deviceName.c_str()]])
            {
                mCaptureDevice = captureDevice;
            }
        }
        
        assert(mCaptureDevice != nil);
        mCameraViceoCapturer = [[RTCCameraVideoCapturer alloc] init];
        [mCameraViceoCapturer startCaptureWithDevice:mCaptureDevice format:mCaptureDevice.activeFormat fps:30];
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
