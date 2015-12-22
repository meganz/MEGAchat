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
/** Frame buffer. Written by the webrtc thread, read and painted on the widget by the
*  GUI thread, on receipt of updateImageSlot()
*/
    std::auto_ptr<QImage> mFrame;
/** Serializes access to frame buffer image from webrtc and GUI thread */
    QMutex mMutex;
    int mAspectRatio = 0;

/** Flag whether to mirror the image horizontally when rendering.
 *  Can be changed at any time, will take effect at the next frame.
 */
    bool mMirrored = false;
protected slots:
    void updateImageSlot();
    void paintEvent(QPaintEvent * event);
public:
    VideoRendererQt(QWidget* parent);

    /** Sets the aspect ratio to be kept constant when resizing the Qt widget */
    void setAspectRatio(double ar)
    {
        mAspectRatio = ar*10;
    }
    /** Set to true to mirror the image horizontally. But default the image is not flipped.
    * Must be done for local video only
    */
    void setMirrored(bool mirrored) { mMirrored = mirrored; }

    /** QT-specific callback method used by QT to resize image proportionally */
    virtual int heightForWidth(int w) const
    {
        if (mAspectRatio)
            return (w*mAspectRatio)/10;
        else
            return -1;
    }
//IVideoRenderer interface
    virtual unsigned char* getImageBuffer(int size, int width, int height, void** userData);
    virtual void frameComplete(void* userData);
    virtual void clearViewport();
    virtual void onStreamDetach();
//==
};

#endif // VIDEORENDERER_QT_H
