#ifndef VIDEORENDERER_QT_H
#define VIDEORENDERER_QT_H

#include "IVideoRenderer.h"
#include <QWidget>
#include <QMutex>
#include <memory>

class VideoRendererQt: public QWidget, public IVideoRenderer
{
protected:
    Q_OBJECT
    std::auto_ptr<QImage> mFrame;
    QMutex mMutex;
    int mAspectRatio = 0;
protected slots:
    void updateImageSlot();
    void paintEvent(QPaintEvent * event);
public:
    VideoRendererQt(QWidget* parent);
    void setAspectRatio(double ar)
    {
        mAspectRatio = ar*10;
    }
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
