#include "QTMegaChatNotificationListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatNotificationListener::QTMegaChatNotificationListener(MegaChatApi *megaChatApi, MegaChatNotificationListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatNotificationListener::~QTMegaChatNotificationListener()
{ }

void QTMegaChatNotificationListener::onChatNotification(MegaChatApi *api, MegaChatHandle chatid, MegaChatMessage *msg)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatNotification);
    event->setChatHandle(chatid);
    event->setChatMessage(msg->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatNotificationListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnChatNotification:
            if (listener) listener->onChatNotification(event->getMegaChatApi(), event->getChatHandle(), event->getChatMessage());
            break;
        default:
            break;
    }
}
