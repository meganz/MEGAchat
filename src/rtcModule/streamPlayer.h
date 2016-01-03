#ifndef STREAMPLAYER_H
#define STREAMPLAYER_H
#include <talk/app/webrtc/mediastreaminterface.h>
#include <IVideoRenderer.h>
#include "base/gcm.h"
#include "webrtcAdapter.h"
#include <mutex>

namespace artc
{
typedef rtcModule::IVideoRenderer IVideoRenderer;

class StreamPlayer: public webrtc::VideoRendererInterface
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
            mVideo->AddRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
        if (mAudio.get())
        {}//TODO: start audio playback
        mPlaying = true;
    }

    void stop()
    {
        if(!mPlaying)
            return;
        if (mVideo.get())
            mVideo->RemoveRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
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
            mVideo->AddRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
    }
    bool isVideoAttached() const { return mVideo.get() != nullptr; }
    bool isAudioAttached() const { return mAudio.get() != nullptr; }
    void detachVideo()
    {
        if (!mVideo.get())
            return;
        if (mPlaying)
            mVideo->RemoveRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
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
        auto renderer = mRenderer;
        mega::marshallCall([renderer]()
        {
            renderer->released();
        });
        mRenderer = nullptr;
    }
//VideoRendererInterface implementation
    virtual void SetSize(int width, int height)
    {
        //IRenderer must check size every time getImageBuffer() is called
    }

    void RenderFrame(const cricket::VideoFrame* frame)
    {
        if (!mRenderer)
            return; //maybe about to be destroyed
        if (!mMediaStartSignalled)
        {
            mMediaStartSignalled = true;
            std::unique_lock<std::mutex> locker(mMutex);
            if (mOnMediaStart)
            {
                auto callback = mOnMediaStart;
                mega::marshallCall([callback]()
                {
                    callback();
                });
            }
        }
        unsigned short width = frame->GetWidth();
        unsigned short height = frame->GetHeight();
        int bufSize = width*height*4;
        void* userData = NULL;
        unsigned char* frameBuf = mRenderer->getImageBuffer(width, height, &userData);
        if (!frameBuf)
            return; //used by null renderer
        frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, frameBuf, bufSize, width*4);
        mRenderer->frameComplete(userData);
    }
};
}

#endif // STREAMPLAYER_H
