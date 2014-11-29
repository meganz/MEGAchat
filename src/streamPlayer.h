#ifndef STREAMPLAYER_H
#define STREAMPLAYER_H
#include <talk/app/webrtc/mediastreaminterface.h>
#include <IVideoRenderer.h>
//#include <karereCommon.h>
#include "base/guiCallMarshaller.h"
#include "webrtcAdapter.h"
#include <mutex>

namespace artc
{

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
    {}
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
    }

    void stop()
    {
        if(!mPlaying)
            return;
        if (mVideo.get())
            mVideo->RemoveRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
        if (mAudio.get())
        {}//TODO: Stop audio playback
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
        mRenderer->onStreamAttach();
        if (mPlaying)
            mVideo->AddRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
    }

    void detachVideo()
    {
        if (!mVideo.get())
            return;
        if (mPlaying)
            mVideo->RemoveRenderer(static_cast<webrtc::VideoRendererInterface*>(this));
        mRenderer->onStreamDetach();
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

    IVideoRenderer* preDestroy()
    {
        stop();
        detachAudio();
        detachVideo();
        IVideoRenderer* ret = mRenderer;
        mRenderer = nullptr;
        return ret;
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
        int width = frame->GetWidth();
        int height = frame->GetHeight();
        int bufSize = width*height*4;
        void* userData = NULL;
        unsigned char* frameBuf = mRenderer->getImageBuffer(bufSize, width, height, &userData);
        frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, frameBuf, bufSize, width*4);
        mRenderer->frameComplete(userData);
    }
};
}

#endif // STREAMPLAYER_H
