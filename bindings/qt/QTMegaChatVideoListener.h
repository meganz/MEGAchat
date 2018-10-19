#ifndef QTMEGACHATVIDEOLISTENER_H
#define QTMEGACHATVIDEOLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatVideoListener : public QObject, public MegaChatVideoListener
{
    Q_OBJECT

public:
    QTMegaChatVideoListener(MegaChatApi *megaChatApi, MegaChatVideoListener *parent = NULL);
    virtual ~QTMegaChatVideoListener();
    virtual void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);

protected:
    virtual void customEvent(QEvent * event);
    MegaChatApi *megaChatApi;
    MegaChatVideoListener *listener;
};
}

#endif // QTMEGACHATVIDEOLISTENER_H
