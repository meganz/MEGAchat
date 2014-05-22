#ifndef STREAMPLAYER_H
#define STREAMPLAYER_H
#include <talk/app/webrtc/mediastreaminterface.h>
#include <IVideoRenderer.h>
#include <karereCommon.h>

namespace MEGA_RTCADAPTER_NS
{

class StreamPlayer: public webrtc::VideoRendererInterface
{
protected:
    talk_base::scoped_refptr<webrtc::AudioTrackInterface> mAudio;
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> mVideo;
    IVideoRenderer* mRenderer;
    bool mPlaying;
public:
    StreamPlayer(IVideoRenderer* renderer, webrtc::AudioTrackInterface* audio,
    webrtc::VideoTrackInterface* video)
     :mAudio(audio), mVideo(video), mRenderer(renderer), mPlaying(false)
    {}
    void start()
    {
        if (mPlaying)
            return;
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
        if (mAudio.get())
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
        assert(video != NULL);
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
//VideoRendererInterface implementation
    virtual void SetSize(int width, int height)
    {
        //IRenderer must check size every time getImageBuffer() is called
    }

    void RenderFrame(const cricket::VideoFrame* frame)
    {
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
