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
void VideoRendererQt::setStaticImage(QImage* image)
{
    if (mFrozen == kFrozenForStaticImage)
    {
        if (!image)
            throw std::runtime_error("Can't delete static image while showing it");
        mStaticImage.reset(image);
        drawStaticImageOnFrame();
    }
    else
    {
        mStaticImage.reset(image);
    }
}
void VideoRendererQt::showStaticImage()
{
    if (!mStaticImage)
        throw std::runtime_error("No static image has been set");
    mFrozen = kFrozenForStaticImage;
}

void VideoRendererQt::resumeFromStaticImage()
{
    if (mFrozen != kFrozenForStaticImage)
        return;
    mFrozen = kNotFrozen;
}
void VideoRendererQt::drawStaticImageOnFrame()
{
    mFrame.reset(new QImage(size(), QImage::Format_ARGB32));
    QPainter painter(mFrame.get());
    QRect ir;
    ir.setX((width() - mStaticImage->width()) / 2);
    ir.setY((height() - mStaticImage->height()) / 2);
    if (ir.x() < 0)
    {
        ir.setX(0);
        ir.setWidth(width());
    }
    else
    {
        ir.setWidth(mStaticImage->width());
    }
    if (ir.y() < 0)
    {
        ir.setY(0);
        ir.setHeight(height());
    }
    else
    {
        ir.setHeight(mStaticImage->height());
    }
    if (ir.x() || ir.y())
        painter.fillRect(rect(), Qt::black);

    painter.drawImage(ir, *mStaticImage);
}

void VideoRendererQt::paintEvent(QPaintEvent* event)
{
    if (mFrozen == kFrozenForStaticImage)
    {
        if (mFrame->size() != size())
            drawStaticImageOnFrame();

        QPainter painter(this);
        painter.drawImage(rect(), *mFrame);
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
