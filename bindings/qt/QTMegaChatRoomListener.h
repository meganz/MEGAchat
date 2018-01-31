#ifndef QTMEGACHATROOMLISTENER_H
#define QTMEGACHATROOMLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatRoomListener : public QObject, public MegaChatRoomListener
{
    Q_OBJECT

public:
    explicit QTMegaChatRoomListener(MegaChatApi *megaApi, MegaChatRoomListener *listener = NULL);
    virtual ~QTMegaChatRoomListener();

    virtual void onChatRoomUpdate(MegaChatApi* api, MegaChatRoom *chat);
    virtual void onMessageLoaded(MegaChatApi* api, MegaChatMessage *msg);
    virtual void onMessageReceived(MegaChatApi* api, MegaChatMessage *msg);
    virtual void onMessageUpdate(MegaChatApi* api, MegaChatMessage *msg);

protected:
    virtual void customEvent(QEvent * event);

    MegaChatApi *megaChatApi;
    MegaChatRoomListener *listener;
};
}

#endif // QTMEGACHATROOMLISTENER_H
