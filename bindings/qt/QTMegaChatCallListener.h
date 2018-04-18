#ifndef QTMEGACHATCALLLISTENER_H
#define QTMEGACHATCALLLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatCallListener : public QObject, public MegaChatCallListener
{
    Q_OBJECT

public:
    QTMegaChatCallListener(MegaChatApi *megaChatApi, MegaChatCallListener *parent = NULL);
    virtual ~QTMegaChatCallListener();
    virtual void onChatCallUpdate(MegaChatApi *api, MegaChatCall *call);

protected:
    virtual void customEvent(QEvent * event);
    MegaChatApi *megaChatApi;
    MegaChatCallListener *listener;
};
}

#endif // QTMEGACHATCALLLISTENER_H
