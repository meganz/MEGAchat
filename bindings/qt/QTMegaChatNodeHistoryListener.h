#ifndef QTMEGACHATNODEHISTORYLISTENER_H
#define QTMEGACHATNODEHISTORYLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatNodeHistoryListener : public QObject, public MegaChatNodeHistoryListener
{
    Q_OBJECT

public:
    QTMegaChatNodeHistoryListener(MegaChatApi *megaChatApi, MegaChatNodeHistoryListener *parent = NULL);
    virtual ~QTMegaChatNodeHistoryListener();

    virtual void onAttachmentLoaded(MegaChatApi *api, MegaChatMessage *msg);
    virtual void onAttachmentReceived(MegaChatApi *api, MegaChatMessage *msg);
    virtual void onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid);
    virtual void onTruncate(MegaChatApi *api, MegaChatHandle msgid);

protected:
    virtual void customEvent(QEvent * event);

    MegaChatApi *megaChatApi;
    MegaChatNodeHistoryListener *listener;
};

}
#endif // QTMEGACHATNODEHISTORYLISTENER_H
