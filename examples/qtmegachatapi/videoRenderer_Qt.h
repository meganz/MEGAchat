#ifndef VIDEORENDERER_QT_H
#define VIDEORENDERER_QT_H

#include "rtcModule/IVideoRenderer.h"
#include <QWidget>
#include <QMutex>
#include <memory>

class VideoRendererQt: public QWidget, public rtcModule::IVideoRenderer
{
protected:
    Q_OBJECT
    enum FrozenReasons { kNotFrozen = 0, kFrozenByHide = 1, kFrozenForStaticImage = 2 };
/** Frame buffer. Written by the webrtc thread, read and painted on the widget by the
*  GUI thread, on receipt of \c updateImageSlot()
*/
    std::unique_ptr<QImage> mFrame;
    std::unique_ptr<QImage> mStaticImage;

/** Serializes access to frame buffer image from webrtc and GUI thread */
    QMutex mMutex;
    int mAspectRatio = 0;

/** Flag whether to mirror the image horizontally when rendering.
 *  Can be changed at any time, will take effect at the next frame.
 */
    bool mMirrored = false;
/** Determines whether the video is rendered or for what reason it is not */
    unsigned char mFrozen = 0;
    virtual void showEvent(QShowEvent *);
    virtual void hideEvent(QHideEvent *);
    void drawStaticImageOnFrame();
protected slots:
    void updateImageSlot();
    void paintEvent(QPaintEvent * event);
public:
    VideoRendererQt(QWidget* parent);
    /** Sets the aspect ratio to be kept constant when resizing the Qt widget */
    void setAspectRatio(double ar)
    {
        mAspectRatio = static_cast<int>(ar*10);
    }
    void setStaticImage(QImage* image);
    void clearStaticImage();
    void enableStaticImage();
    void disableStaticImage();
    /** Set to true to mirror the image horizontally. But default the image is not flipped.
    * Must be done for local video only
    */
    void setMirrored(bool mirrored){ mMirrored = mirrored; }

    /** QT-specific callback method used by QT to resize image proportionally */
    virtual int heightForWidth(int w) const
    {
        if (mAspectRatio)
            return (w*mAspectRatio)/10;
        else
            return -1;
    }
    virtual ~VideoRendererQt() {}
//IVideoRenderer interface
    virtual void* getImageBuffer(unsigned short width, unsigned short height, void*& userData);
    virtual void frameComplete(void* userData);
    virtual void clearViewport() { doClearViewport(); }
    virtual void onStreamDetach() { disableStaticImage(); }
//==

private:
    void doClearViewport();
};

#endif // VIDEORENDERER_QT_H
