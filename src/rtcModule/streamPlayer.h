#ifndef STREAMPLAYER_H
#define STREAMPLAYER_H
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/video/i420_buffer.h>
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
    rtc::scoped_refptr<webrtc::AudioTrackInterface> mAudio;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> mVideo;
    IVideoRenderer* mRenderer;
    bool mPlaying = false;
    bool mMediaStartSignalled = false;
    std::function<void()> mOnMediaStart;
    std::mutex mMutex; //guards onMediaStart and other stuff that is accessed from webrtc threads
public:
    IVideoRenderer* videoRenderer() const {return mRenderer;}
    StreamPlayer(IVideoRenderer* renderer, webrtc::AudioTrackInterface* audio=nullptr,
    webrtc::VideoTrackInterface* video=nullptr)
     :mAudio(audio), mVideo(video), mRenderer(renderer)
    {
        assert(mRenderer);
    }
    StreamPlayer(IVideoRenderer *renderer, tspMediaStream stream)
        :mRenderer(renderer)
    {
        connectToStream(stream);
    }
    ~StreamPlayer()
    {
        preDestroy();
    }

    void connectToStream(tspMediaStream stream)
    {
        if (!stream)
            return;

        detachAudio();
        detachVideo();
        auto ats = stream->GetAudioTracks();
        auto vts = stream->GetVideoTracks();
        if (ats.size() > 0)
            mAudio = ats[0];
        if (vts.size() > 0)
            mVideo = vts[0];
    }

    template <class F>
    void setOnMediaStart(F&& callback)
    {
        std::unique_lock<std::mutex> locker(mMutex);
        mOnMediaStart = callback;
    }
    void start()
    {
        if (mPlaying)
            return;
        mMediaStartSignalled = false;
        if (mVideo.get())
        {
            rtc::VideoSinkWants opts;
            mVideo->AddOrUpdateSink(static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(this), opts);
        }
        if (mAudio.get())
        {}//TODO: start audio playback
        mPlaying = true;
    }

    void stop()
    {
        if(!mPlaying)
            return;
        if (mVideo.get())
            mVideo->RemoveSink(static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(this));
        if (mAudio.get())
        {}//TODO: Stop audio playback
        mPlaying = false;
        mRenderer->clearViewport();
    }

    void attachAudio(webrtc::AudioTrackInterface* audio)
    {
        assert(audio);
        detachAudio();
        mAudio = audio;
        if (mPlaying)
            {}//TODO: start audio playback
    }

    void detachAudio()
    {
        if (!mAudio.get())
            return;
        if (mPlaying)
        {} //TODO: stop audio playback
        mAudio = NULL;
    }

    void attachVideo(webrtc::VideoTrackInterface* video)
    {
        assert(video);
        detachVideo();
        mVideo = video;
        mRenderer->onVideoAttach();
        if (mPlaying)
        {
            rtc::VideoSinkWants opts;
            mVideo->AddOrUpdateSink(static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(this), opts);
        }
    }
    bool isVideoAttached() const { return mVideo.get() != nullptr; }
    bool isAudioAttached() const { return mAudio.get() != nullptr; }
    void detachVideo()
    {
        if (!mVideo.get())
            return;
        if (mPlaying)
            mVideo->RemoveSink(static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(this));
        mRenderer->onVideoDetach();
        mRenderer->clearViewport();
        mVideo = NULL;
    }
    void attachToStream(artc::tspMediaStream& stream)
    {
        detachAudio();
        auto ats = stream->GetAudioTracks();
        if (!ats.empty())
            attachAudio(ats[0]);
        detachVideo();
        auto vts = stream->GetVideoTracks();
        if (!vts.empty())
            attachVideo(vts[0]);
    }

    void changeRenderer(IVideoRenderer* newRenderer)
    {
        assert(newRenderer);
        mRenderer = newRenderer;
        mRenderer->clearViewport();
    }

    void preDestroy()
    {
        stop();
        detachAudio();
        detachVideo();
        mRenderer->released();
        mRenderer = nullptr;
    }
//rtc::VideoSinkInterface<webrtc::VideoFrame> implementation
    virtual void OnFrame(const webrtc::VideoFrame& frame)
    {
        if (!mMediaStartSignalled)
        {
            mMediaStartSignalled = true;
            std::unique_lock<std::mutex> locker(mMutex);
            if (mOnMediaStart)
            {
                auto callback = mOnMediaStart;
                karere::marshallCall([callback]()
                {
                    callback();
                });
            }
        }
        if (!mRenderer)
            return; //no renderer

        void* userData = NULL;
        rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
            frame.video_frame_buffer()->ToI420());
        if (frame.rotation() != webrtc::kVideoRotation_0)
        {
            buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
        }
        unsigned short width = buffer->width();
        unsigned short height = buffer->height();
        void* frameBuf = mRenderer->getImageBuffer(width, height, userData);
        if (!frameBuf) //image is frozen or app is minimized/covered
            return;
        libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
                           buffer->DataU(), buffer->StrideU(),
                           buffer->DataV(), buffer->StrideV(),
                           (uint8_t*)frameBuf, width * 4, width, height);
        mRenderer->frameComplete(userData);
    }
};
}

#endif // STREAMPLAYER_H
