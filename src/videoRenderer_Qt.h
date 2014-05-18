#ifndef VIDEORENDERER_QT_H
#define VIDEORENDERER_QT_H

#include <talk/app/webrtc/mediastreaminterface.h>
#include <QWidget>
#include <QMutex>
#include <memory>

class VideoRendererQt: public QWidget, webrtc::VideoRendererInterface
{
protected:
    Q_OBJECT
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> mTrack;
    bool mIsActive;
    std::auto_ptr<QImage> mFrame;
    QMutex mMutex;
protected slots:
    void setSizeSlot(int w, int h);
    void updateImageSlot();
    void paintEvent(QPaintEvent * event);
public:
    VideoRendererQt(QWidget* parent);
    void attach(webrtc::VideoTrackInterface* track, bool autoplay = true);
    void detach();
    void start();
    void stop();

    virtual void SetSize(int width, int height);
    virtual void RenderFrame(const cricket::VideoFrame* frame);
    virtual ~VideoRendererQt()
    {
        stop();
    }
};

#endif // VIDEORENDERER_QT_H
