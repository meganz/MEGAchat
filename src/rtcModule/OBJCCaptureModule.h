
#ifndef ARTC_OBJCCAPTUREMODULE_H_
#define ARTC_OBJCCAPTUREMODULE_H_

#ifdef __APPLE__

#include <api/peer_connection_interface.h>
#include <api/scoped_refptr.h>
#include <modules/video_capture/video_capture_defines.h>

#ifdef __OBJC__
@class AVCaptureDevice;
@class RTCCameraVideoCapturer;
#else
typedef struct objc_object AVCaptureDevice;
typedef struct objc_object RTCCameraVideoCapturer;
#endif

namespace artc
{
class OBJCCaptureModule
{
    
public:
    OBJCCaptureModule(const webrtc::VideoCaptureCapability &capabilities, const std::string &deviceName);
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> getVideoSource();
    static std::set<std::pair<std::string, std::string>> getVideoDevices();
    virtual void openDevice(const std::string &videoDevice);
    virtual void releaseDevice();
    virtual webrtc::VideoTrackSourceInterface *getVideoTrackSource();

private:
    AVCaptureDevice *mCaptureDevice = nullptr;
    RTCCameraVideoCapturer *mCameraViceoCapturer = nullptr;
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> mVideoSource;
    bool mRunning = false;
};
}

#endif

#endif
