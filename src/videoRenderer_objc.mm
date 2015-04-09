//
//  videoRenderer_objc.m
//  testapp
//
//  Created by Alex Vasilev on 4/6/15.
//  Copyright (c) 2015 Alex Vasilev. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "videoRenderer_objc.h"

template<class T, void(*R)(T)>
class AutoRelease
{
protected:
    T mPtr;
public:
    explicit AutoRelease(T ptr):mPtr(ptr){}
    ~AutoRelease()
    {
        if (mPtr)
            R(mPtr);
    }
    operator T() const {return mPtr;}
    T get() const {return mPtr;}
};

class VideoRendererCpp: public IVideoRenderer
{
protected:
    VideoRendererObjc* mVideoRenderer;
    bool mDisabled = false;
    unsigned char* mBuf;
    size_t mWidth;
    size_t mHeight;
    size_t mBufSize;
public:
    VideoRendererCpp(VideoRendererObjc* parent)
    :mVideoRenderer(parent){}
    virtual unsigned char* getImageBuffer(int bufSize, int width, int height, void** userData)
    {
        //Images are immutable, so we can't obtain a writable pointer to an image's raw data. Instead we return a buffer,
        //from which we create an image in frameComplete()
        if (mDisabled)
            return NULL;
        if (mBuf)
            delete[] mBuf;
        mBuf = new unsigned char[bufSize];
        mWidth = width;
        mHeight = height;
        mBufSize = bufSize;
        return mBuf;
    }
    virtual void frameComplete(void* userData)
    {
        //userData is the image buffer
        AutoRelease<CGColorSpaceRef, CGColorSpaceRelease> colorSpace(CGColorSpaceCreateDeviceRGB());
        CGContextRef context = CGBitmapContextCreateWithData(mBuf, mWidth, mHeight, 8, mWidth * 4, colorSpace,
            kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast,
            [](void* userp, void* data) {delete[] (unsigned char*)data;},
            NULL
        );
        CGImageRef cgImage = CGBitmapContextCreateImage(context);
        UIImage* image = [UIImage imageWithCGImage:cgImage];
        [mVideoRenderer performSelectorOnMainThread: @selector(setImage:) withObject: image waitUntilDone: NO];
    }
    virtual void onStreamAttach() {}
    virtual void onStreamDetach() {}
    virtual void clearViewport()
    {
        [mVideoRenderer fillWithBlack];
    }
    virtual ~VideoRendererCpp() {}
};

@implementation VideoRendererObjc
@synthesize videoRenderer = mVideoRenderer;

-(id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder: aDecoder];
    if (!self)
        return nil;
    return [self initRenderer];
}

-(id)initRenderer
{
    mVideoRenderer = new VideoRendererCpp(self);
    [self fillWithBlack];
    return self;
}
-(void)fillWithBlack
{
    CGSize size = self.frame.size;
    UIGraphicsBeginImageContext(size);
    [[UIColor blackColor] set];
    UIRectFill(CGRectMake(0, 0, size.width, size.height));
    UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    self.image = image;
}
-(void)dealloc
{
    if (mVideoRenderer)
        delete mVideoRenderer;
    //[super dealloc]; - not needed for ARC
}
-(void)setImageFunc:(UIImage*) image
{
    self.image = image;
}
@end