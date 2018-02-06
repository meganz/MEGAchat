#ifndef QTMEGACHATLISTENER_H
#define QTMEGACHATLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatListener : public QObject, public MegaChatListener
{
    Q_OBJECT

public:
    QTMegaChatListener(MegaChatApi *megaChatApi, MegaChatListener *parent = NULL);
    virtual ~QTMegaChatListener();

    virtual void onChatListItemUpdate(MegaChatApi* api, MegaChatListItem *item);
    virtual void onChatInitStateUpdate(MegaChatApi* api, int newState);
    virtual void onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress);
    virtual void onChatPresenceConfigUpdate(MegaChatApi* api, MegaChatPresenceConfig *config);
    virtual void onChatConnectionStateUpdate(MegaChatApi* api, MegaChatHandle chatid, int newState);

protected:
    virtual void customEvent(QEvent * event);

    MegaChatApi *megaChatApi;
    MegaChatListener *listener;
};
}

#endif // QTMEGACHATLISTENER_H
