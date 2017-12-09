#ifndef QTMEGACHATREQUESTLISTENER_H
#define QTMEGACHATREQUESTLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatRequestListener : public QObject, public MegaChatRequestListener
{
    Q_OBJECT

public:
    QTMegaChatRequestListener(MegaChatApi *megaChatApi, MegaChatRequestListener *listener = NULL);
    virtual ~QTMegaChatRequestListener();

    virtual void onRequestStart(MegaChatApi* api, MegaChatRequest *request);
    virtual void onRequestFinish(MegaChatApi* api, MegaChatRequest *request, MegaChatError* e);
    virtual void onRequestUpdate(MegaChatApi*api, MegaChatRequest *request);
    virtual void onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError* error);

protected:
    virtual void customEvent(QEvent * event);

    MegaChatApi *megaChatApi;
    MegaChatRequestListener *listener;
};
}

#endif // QTMEGACHATREQUESTLISTENER_H
