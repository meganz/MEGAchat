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
protected slots:
    void updateImageSlot();
    void paintEvent(QPaintEvent * event);
public:
    VideoRendererQt(QWidget* parent);
//IVideoRenderer interface
    virtual unsigned char* getImageBuffer(int size, int width, int height, void** userData);
    virtual void frameComplete(void* userData);
//==
};

#endif // VIDEORENDERER_QT_H
