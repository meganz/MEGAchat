/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <cocoa/cocoa.h>

#import "device_info_mac_objc.h"
#import "rtc_video_capture_mac_objc.h"

#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;
using namespace webrtc::videocapturemodule;

@interface RTCVideoCaptureIosObjC (hidden)
- (int)changeCaptureInputWithName:(NSString*)captureDeviceName;
@end

@implementation RTCVideoCaptureIosObjC {
  webrtc::videocapturemodule::VideoCaptureIos* _owner;
  webrtc::VideoCaptureCapability _capability;
  AVCaptureSession* _captureSession;
  int _captureId;
  AVCaptureConnection* _connection;
  BOOL _captureChanging;  // Guarded by _captureChangingCondition.
  NSCondition* _captureChangingCondition;
}

@synthesize frameRotation = _framRotation;

- (id)initWithOwner:(VideoCaptureIos*)owner captureId:(int)captureId {
  if (self == [super init]) {
    _owner = owner;
    _captureId = captureId;
    _captureSession = [[AVCaptureSession alloc] init];
#if defined(__IPHONE_7_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_7_0
    NSString* version = [[UIDevice currentDevice] systemVersion];
    if ([version integerValue] >= 7) {
      _captureSession.usesApplicationAudioSession = NO;
    }
#endif
    _captureChanging = NO;
    _captureChangingCondition = [[NSCondition alloc] init];

    if (!_captureSession || !_captureChangingCondition) {
      return nil;
    }

    // create and configure a new output (using callbacks)
    AVCaptureVideoDataOutput* captureOutput =
        [[AVCaptureVideoDataOutput alloc] init];
    NSString* key = (NSString*)kCVPixelBufferPixelFormatTypeKey;

    NSNumber* val = [NSNumber
        numberWithUnsignedInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange];
    NSDictionary* videoSettings =
        [NSDictionary dictionaryWithObject:val forKey:key];
    captureOutput.videoSettings = videoSettings;

    // add new output
    if ([_captureSession canAddOutput:captureOutput]) {
      [_captureSession addOutput:captureOutput];
    } else {
      WEBRTC_TRACE(kTraceError,
                   kTraceVideoCapture,
                   _captureId,
                   "%s:%s:%d Could not add output to AVCaptureSession ",
                   __FILE__,
                   __FUNCTION__,
                   __LINE__);
    }

    NSNotificationCenter* notify = [NSNotificationCenter defaultCenter];
    [notify addObserver:self
               selector:@selector(onVideoError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
    [notify addObserver:self
               selector:@selector(statusBarOrientationDidChange:)
                   name:@"StatusBarOrientationDidChange"
                 object:nil];
  }

  return self;
}

- (void)directOutputToSelf {
  [[self currentOutput]
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(
                                  DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
}

- (void)directOutputToNil {
  [[self currentOutput] setSampleBufferDelegate:nil queue:NULL];
}

- (void)statusBarOrientationDidChange:(NSNotification*)notification {
  [self setRelativeVideoOrientation];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  // check to see if the camera is already set
  if (_captureSession) {
    NSArray* currentInputs = [NSArray arrayWithArray:[_captureSession inputs]];
    if ([currentInputs count] > 0) {
      AVCaptureDeviceInput* currentInput = [currentInputs objectAtIndex:0];
      if ([uniqueId isEqualToString:[currentInput.device localizedName]]) {
        return YES;
      }
    }
  }

  return [self changeCaptureInputByUniqueId:uniqueId];
}

- (BOOL)startCaptureWithCapability:(const VideoCaptureCapability&)capability {
  [self waitForCaptureChangeToFinish];
  if (!_captureSession) {
      return NO;
  }
  // check limits of the resolution
  if (capability.maxFPS < 0 || capability.maxFPS > 60) {
      return NO;
  }
  int32_t width = capability.width;
  int32_t height = capability.height;
  if ((capability.width >= 1920 || capability.height >= 1080)
      && ([_captureSession canSetSessionPreset:@"AVCaptureSessionPreset1920x1080"])) {
          width = 1920;
          height = 1080;
  } else if ((capability.width >= 1280 || capability.height >= 720)
      && ([_captureSession canSetSessionPreset:AVCaptureSessionPreset1280x720])) {
         width = 1280;
         height = 720;
  } else if ((capability.width >= 640 || capability.height >= 480)
      && ([_captureSession canSetSessionPreset:AVCaptureSessionPreset640x480])) {
          width = 640;
          height = 480;
  } else if ((capability.width >= 352 || capability.height >= 288)
      && ([_captureSession canSetSessionPreset:AVCaptureSessionPreset352x288])) {
          width = 352;
          height = 288;
  } else if ((capability.width >= 320 || capability.height >= 240)
      && ([_captureSession canSetSessionPreset:AVCaptureSessionPreset320x240])) {
          width = 320;
          height = 240;
  } else {
      printf("Video Capturer: Could not find a supported capture resolution (requested: %dx%d) at %s:%d\n",
        capability.width, capability.height, __FILE__, __LINE__);
      return NO;
  }

  _capability = capability;
  _capability.width = width;
  _capability.height = height;
 //printf("===== decided w = %d; h = %d\n", width, height);
  AVCaptureVideoDataOutput* currentOutput = [self currentOutput];
  if (!currentOutput)
    return NO;

  [self directOutputToSelf];

  _captureChanging = YES;
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
      ^(void) { [self startCaptureInBackgroundWithOutput:currentOutput]; });
  return YES;
}

- (AVCaptureVideoDataOutput*)currentOutput {
  return [[_captureSession outputs] firstObject];
}

- (void)startCaptureInBackgroundWithOutput: (AVCaptureVideoDataOutput*)currentOutput {
  NSString* preset = [NSString stringWithFormat:
      @"AVCaptureSessionPreset%dx%d", _capability.width, _capability.height];

  // begin configuration for the AVCaptureSession
  [_captureSession beginConfiguration];

  // picture resolution
 [_captureSession setSessionPreset:preset];

  // take care of capture framerate now
  do {
      NSArray* sessionInputs = _captureSession.inputs;
      if ([sessionInputs count] <= 0)
          break;
      AVCaptureDevice* inputDevice = [sessionInputs[0] device];
      if (!inputDevice) {
          break;
      }
      NSArray* supportedRanges = inputDevice.activeFormat.videoSupportedFrameRateRanges;
      if ([supportedRanges count] <= 0) {
        break;
      }
      AVFrameRateRange* targetRange = supportedRanges[0];
      // Find the largest supported framerate, but lower than capability.maxFPS.
      for (AVFrameRateRange* range in supportedRanges) {
          if ((range.maxFrameRate <= _capability.maxFPS) &&
                  (range.maxFrameRate >= targetRange.maxFrameRate)) {
              targetRange = range;
          }
      }
      if (targetRange && [inputDevice lockForConfiguration:NULL]) {
          inputDevice.activeVideoMinFrameDuration = targetRange.minFrameDuration;
          inputDevice.activeVideoMaxFrameDuration = targetRange.minFrameDuration;
          [inputDevice unlockForConfiguration];
          //printf("fps = %llu\n", targetRange.minFrameDuration.timescale/targetRange.minFrameDuration.value);
      }
  } while(false);

  _connection = [currentOutput connectionWithMediaType:AVMediaTypeVideo];
  [self setRelativeVideoOrientation];
  // finished configuring, commit settings to AVCaptureSession.
  [_captureSession commitConfiguration];

  [_captureSession startRunning];
  [self signalCaptureChangeEnd];
}

- (void)setRelativeVideoOrientation {
    return;
}

- (void)onVideoError:(NSNotification*)notification {
  NSLog(@"onVideoError: %@", notification);
  // TODO(sjlee): make the specific error handling with this notification.
  WEBRTC_TRACE(kTraceError,
               kTraceVideoCapture,
               _captureId,
               "%s:%s:%d [AVCaptureSession startRunning] error.",
               __FILE__,
               __FUNCTION__,
               __LINE__);
}

- (BOOL)stopCapture {
  [self waitForCaptureChangeToFinish];
  [self directOutputToNil];

  if (!_captureSession) {
    return NO;
  }

  _captureChanging = YES;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                 ^(void) { [self stopCaptureInBackground]; });
  return YES;
}

- (void)stopCaptureInBackground {
  [_captureSession stopRunning];
  [self signalCaptureChangeEnd];
}

- (BOOL)changeCaptureInputByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  NSArray* currentInputs = [_captureSession inputs];
  // remove current input
  if ([currentInputs count] > 0) {
    AVCaptureInput* currentInput =
        (AVCaptureInput*)[currentInputs objectAtIndex:0];

    [_captureSession removeInput:currentInput];
  }

  // Look for input device with the name requested (as our input param)
  // get list of available capture devices
  int captureDeviceCount = [DeviceInfoIosObjC captureDeviceCount];
  if (captureDeviceCount <= 0) {
    return NO;
  }

  AVCaptureDevice* captureDevice =
      [DeviceInfoIosObjC captureDeviceForUniqueId:uniqueId];

  if (!captureDevice) {
    return NO;
  }

  // now create capture session input out of AVCaptureDevice
  NSError* deviceError = nil;
  AVCaptureDeviceInput* newCaptureInput =
      [AVCaptureDeviceInput deviceInputWithDevice:captureDevice
                                            error:&deviceError];

  if (!newCaptureInput) {
    const char* errorMessage = [[deviceError localizedDescription] UTF8String];

    WEBRTC_TRACE(kTraceError,
                 kTraceVideoCapture,
                 _captureId,
                 "%s:%s:%d deviceInputWithDevice error:%s",
                 __FILE__,
                 __FUNCTION__,
                 __LINE__,
                 errorMessage);

    return NO;
  }

  // try to add our new capture device to the capture session
  [_captureSession beginConfiguration];

  BOOL addedCaptureInput = NO;
  if ([_captureSession canAddInput:newCaptureInput]) {
    [_captureSession addInput:newCaptureInput];
    addedCaptureInput = YES;
  } else {
    addedCaptureInput = NO;
  }

  [_captureSession commitConfiguration];

  return addedCaptureInput;
}

- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  const int kFlags = 0;
  CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);

  if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) != kCVReturnSuccess) {
    return;
  }

  const int kYPlaneIndex = 0;
  const int kUVPlaneIndex = 1;

  uint8_t* baseAddress =
      (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(videoFrame, kYPlaneIndex);
  size_t yPlaneBytesPerRow =
      CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kYPlaneIndex);
  size_t yPlaneHeight = CVPixelBufferGetHeightOfPlane(videoFrame, kYPlaneIndex);
  size_t uvPlaneBytesPerRow =
      CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kUVPlaneIndex);
  size_t uvPlaneHeight = CVPixelBufferGetHeightOfPlane(videoFrame, kUVPlaneIndex);
  size_t frameSize =
      yPlaneBytesPerRow * yPlaneHeight + uvPlaneBytesPerRow * uvPlaneHeight;

  VideoCaptureCapability tempCaptureCapability;
  tempCaptureCapability.width = CVPixelBufferGetWidth(videoFrame);
  tempCaptureCapability.height = CVPixelBufferGetHeight(videoFrame);
  tempCaptureCapability.maxFPS = _capability.maxFPS;
  tempCaptureCapability.rawType = kVideoNV12;

  _owner->IncomingFrame(baseAddress, frameSize, tempCaptureCapability, 0);

  CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);
}

- (void)signalCaptureChangeEnd {
  [_captureChangingCondition lock];
  _captureChanging = NO;
  [_captureChangingCondition signal];
  [_captureChangingCondition unlock];
}

- (void)waitForCaptureChangeToFinish {
  [_captureChangingCondition lock];
  while (_captureChanging) {
    [_captureChangingCondition wait];
  }
  [_captureChangingCondition unlock];
}
@end
