#ifndef QTMEGACHATNOTIFICATIONLISTENER_H
#define QTMEGACHATNOTIFICATIONLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatNotificationListener: public QObject, public MegaChatNotificationListener
{
    Q_OBJECT

public:
    explicit QTMegaChatNotificationListener(MegaChatApi *megaChatApi, MegaChatNotificationListener *listener = NULL);
    virtual ~QTMegaChatNotificationListener();

    virtual void onChatNotification(MegaChatApi *api, MegaChatHandle chatid, MegaChatMessage *msg);

protected:
    virtual void customEvent(QEvent * event);

    MegaChatApi *megaChatApi;
    MegaChatNotificationListener *listener;
};
}

#endif // QTMEGACHATNOTIFICATIONLISTENER_H
