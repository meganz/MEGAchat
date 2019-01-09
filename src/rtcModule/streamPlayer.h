#ifndef STREAMPLAYER_H
#define STREAMPLAYER_H
#include <api/mediastreaminterface.h>
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>
#include <IVideoRenderer.h>
#include "base/gcm.h"
#include "webrtcAdapter.h"
#include <mutex>

namespace artc
{
typedef rtcModule::IVideoRenderer IVideoRenderer;

class StreamPlayer: public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
protected:
    void *appCtx;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> mAudio;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> mVideo;
    IVideoRenderer* mRenderer;
    bool mMediaStartSignalled = false;
    std::function<void()> mOnMediaStart;
    std::mutex mMutex; //guards onMediaStart and mRenderer (stuff that is accessed by public API and by webrtc threads)
    bool mVideoEnable = true;
public:
    IVideoRenderer* videoRenderer() const {return mRenderer;}
    StreamPlayer(IVideoRenderer* renderer, void *ctx, webrtc::AudioTrackInterface* audio=nullptr,
    webrtc::VideoTrackInterface* video=nullptr)
     :mAudio(audio), mVideo(video), mRenderer(renderer)
    {
        appCtx = ctx;
    }
    StreamPlayer(IVideoRenderer *renderer, tspMediaStream stream, void *ctx)
     :mRenderer(renderer)
    {
        appCtx = ctx;
        attachToStream(stream);
    }
    ~StreamPlayer()
    {
        preDestroy();
    }
    template <class F>
    void setOnMediaStart(F&& callback)
    {
        std::unique_lock<std::mutex> locker(mMutex);
        mOnMediaStart = callback;
    }

    void detachFromStream()
    {
        detachAudio();
        detachVideo();
    }
    void attachAudio(webrtc::AudioTrackInterface* audio)
    {
        assert(audio);
        detachAudio();
        mAudio = audio;
        //TODO: start audio playback
    }

    void detachAudio()
    {
        if (!mAudio.get())
            return;
        //TODO: stop audio playback
        mAudio = NULL;
    }

    void attachVideo(webrtc::VideoTrackInterface* video)
    {
        assert(video);
        detachVideo();
        mVideo = video;
        if (mRenderer)
        {
            mRenderer->onVideoAttach();
        }
        rtc::VideoSinkWants opts;
        mVideo->AddOrUpdateSink(this, opts);
    }
    bool isVideoAttached() const { return mVideo.get() != nullptr; }
    bool isAudioAttached() const { return mAudio.get() != nullptr; }
    void detachVideo()
    {
        if (!mVideo.get())
            return;
        mVideo->RemoveSink(this);
        mVideo = NULL;
        if (mRenderer)
        {
            mRenderer->onVideoDetach();
            mRenderer->clearViewport();
        }
    }
    void attachToStream(artc::tspMediaStream& stream)
    {
        detachAudio();
        auto ats = stream->GetAudioTracks();
        if (!ats.empty())
        {
            attachAudio(ats[0]);
        }
        detachVideo();
        auto vts = stream->GetVideoTracks();
        if (!vts.empty())
        {
            attachVideo(vts[0]);
        }
    }

    void enableVideo(bool enable)
    {
        mVideoEnable = enable;
    }

    void changeRenderer(IVideoRenderer* newRenderer)
    {
        std::unique_lock<std::mutex> locker(mMutex);
        mRenderer = newRenderer;
        if (mRenderer)
        {
            mRenderer->clearViewport();
        }
    }

    void preDestroy()
    {
        detachFromStream();
        std::unique_lock<std::mutex> locker(mMutex);
        if (mRenderer)
        {
            mRenderer->released();
            mRenderer = nullptr;
        }
    }
//rtc::VideoSinkInterface<webrtc::VideoFrame> implementation
    virtual void OnFrame(const webrtc::VideoFrame& frame)
    {
        std::unique_lock<std::mutex> locker(mMutex);
        if (!mMediaStartSignalled)
        {
            mMediaStartSignalled = true;
            if (mOnMediaStart)
            {
                auto callback = mOnMediaStart;
                karere::marshallCall([callback]()
                {
                    callback();
                }, appCtx);
            }
        }
        if (!mRenderer)
            return; //no renderer


        if (mVideoEnable)
        {
            void* userData = NULL;
            auto buffer = frame.video_frame_buffer()->ToI420();   // smart ptr type changed
            if (frame.rotation() != webrtc::kVideoRotation_0)
            {
                buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
            }
            unsigned short width = (unsigned short)buffer->width();
            unsigned short height = (unsigned short)buffer->height();
            void* frameBuf = mRenderer->getImageBuffer(width, height, userData);
            if (!frameBuf) //image is frozen or app is minimized/covered
                return;
            libyuv::I420ToABGR(buffer->DataY(), buffer->StrideY(),
                               buffer->DataU(), buffer->StrideU(),
                               buffer->DataV(), buffer->StrideV(),
                               (uint8_t*)frameBuf, width * 4, width, height);
            mRenderer->frameComplete(userData);
        }
    }

    webrtc::AudioTrackInterface *getAudioTrack()
    {
        return mAudio.get();
    }
};
}

#endif // STREAMPLAYER_H
