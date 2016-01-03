#include <QWidget>
#include <QPainter>
#include <QMutexLocker>
#include "videoRenderer_Qt.h"
#include <stdexcept>

VideoRendererQt::VideoRendererQt(QWidget *parent)
    :QWidget(parent), mFrame(new QImage(size(), QImage::Format_ARGB32))
{
    clearViewport();
}

void VideoRendererQt::updateImageSlot()
{
    repaint();
}
void VideoRendererQt::showEvent(QShowEvent *)
{
    if (mFrozen == kFrozenByHide)
        mFrozen = kNotFrozen;
}

void VideoRendererQt::hideEvent(QHideEvent *)
{
    if (!mFrozen)
        mFrozen = kFrozenByHide;
}
void VideoRendererQt::showStaticImage(QImage* image)
{
    mFrame.reset(image);
    mFrozen = kFrozenForStaticImage;
}

void VideoRendererQt::resumeFromStaticImage()
{
    if (mFrozen != kFrozenForStaticImage)
        return;
    mFrozen = kNotFrozen;
}

void VideoRendererQt::paintEvent(QPaintEvent* event)
{
    if (mFrozen == kFrozenForStaticImage)
    {
        QPainter painter(this);
        painter.drawImage(QRect(0, 0, width(), height()), *mFrame);
        return;
    }
    else
    {
        QMutexLocker locker(&mMutex);
        QPainter painter(this);
        painter.drawImage(QRect(0, 0, width(), height()),
            mMirrored ? mFrame->mirrored(true, false) : *mFrame);
    }
}

//IVideoRenderer interface implementation
unsigned char* VideoRendererQt::getImageBuffer(unsigned short width, unsigned short height, void** userData)
{
    if (mFrozen)
        return nullptr; //don't overwrite if there is a static image, don't bother if invisible
    QImage* bmp = new QImage(width, height, QImage::Format_ARGB32);
    *userData = static_cast<void*>(bmp);
    return bmp->bits();
}

void VideoRendererQt::frameComplete(void* userData)
{
    QImage* bmp = static_cast<QImage*>(userData);
    {
        QMutexLocker lock(&mMutex);
        mFrame.reset(bmp);
    }
    QMetaObject::invokeMethod(this,
      "updateImageSlot", Qt::QueuedConnection);
}
void VideoRendererQt::clearViewport()
{
    mFrame->fill(0xff000000);
    repaint();
}
