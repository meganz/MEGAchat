#include <talk/app/webrtc/mediastreaminterface.h>
#include <talk/media/base/videocommon.h>
#include <talk/media/base/videoframe.h>
#include <QWidget>
#include <QPainter>
#include <QMutexLocker>
#include "videoRenderer_Qt.h"
#include <stdexcept>

VideoRendererQt::VideoRendererQt(QWidget *parent)
    :QWidget(parent), mFrame(new QImage(size(), QImage::Format_ARGB32))
{
    mFrame->fill(0);
}

void VideoRendererQt::attach(webrtc::VideoTrackInterface* track, bool autoplay)
{
    detach();
    mTrack = track;
    if (autoplay)
        start();

}
void VideoRendererQt::detach()
{
    stop();
    mTrack = NULL;
}
void VideoRendererQt::updateImageSlot()
{
    update();
}

void VideoRendererQt::paintEvent(QPaintEvent* event)
{
    QMutexLocker locker(&mMutex);
    QPainter painter(this);
    painter.drawImage(QRect(0,0,width(), height()),*mFrame);
}

void VideoRendererQt::setSizeSlot(int width, int height)
{
    printf("setSize(%d,%d)\n", width, height);
}

void VideoRendererQt::SetSize(int width, int height)
{
    QMetaObject::invokeMethod(this,
      "setSizeSlot", Qt::BlockingQueuedConnection,
       Q_ARG(int, width), Q_ARG(int, height));
}

void VideoRendererQt::RenderFrame(const cricket::VideoFrame* frame)
{
    static int ctr = 0;
    printf("frame %d\n", ++ctr);

    QImage* bmp = new QImage(frame->GetWidth(), frame->GetHeight(), QImage::Format_ARGB32);
    frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, bmp->bits(), bmp->byteCount(), bmp->width()*bmp->depth()/8);
    QMutexLocker lock(&mMutex);
    mFrame.reset(bmp);

    QMetaObject::invokeMethod(this,
      "updateImageSlot", Qt::QueuedConnection);
}

void VideoRendererQt::start()
{
    if (mIsActive)
        return;
    if (!mTrack.get())
        throw std::runtime_error("No track is attached to renderer, use attach() first");
    mTrack->AddRenderer(static_cast<VideoRendererInterface*>(this));
    mIsActive = true;
}

void VideoRendererQt::stop()
{
    if (!mIsActive)
        return;
    mTrack->RemoveRenderer(static_cast<VideoRendererInterface*>(this));
    mIsActive = false;
}
