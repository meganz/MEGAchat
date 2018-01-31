#include "QTMegaChatListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatListener::QTMegaChatListener(MegaChatApi *megaChatApi, MegaChatListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatListener::~QTMegaChatListener()
{ }

void QTMegaChatListener::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatListItemUpdate);
    event->setChatListItem(item->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatListener::onChatInitStateUpdate(MegaChatApi *api, int newState)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatInitStateUpdate);
    event->setStatus(newState);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatListener::onChatOnlineStatusUpdate(MegaChatApi *api, MegaChatHandle userhandle, int status, bool inProgress)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatOnlineStatusUpdate);
    event->setChatHandle(userhandle);
    event->setStatus(status);
    event->setProgress(inProgress);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatListener::onChatPresenceConfigUpdate(MegaChatApi *api, MegaChatPresenceConfig *config)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatPresenceConfigUpdate);
    event->setPresenceConfig(config->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatListener::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int newState)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatConnectionStateUpdate);
    event->setChatHandle(chatid);
    event->setStatus(newState);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnChatListItemUpdate:
            if (listener) listener->onChatListItemUpdate(event->getMegaChatApi(), event->getChatListItem());
            break;
        case QTMegaChatEvent::OnChatInitStateUpdate:
            if (listener) listener->onChatInitStateUpdate(event->getMegaChatApi(), event->getStatus());
            break;
        case QTMegaChatEvent::OnChatOnlineStatusUpdate:
            if (listener) listener->onChatOnlineStatusUpdate(event->getMegaChatApi(), event->getChatHandle(), event->getStatus(), event->getProgress());
            break;
        case QTMegaChatEvent::OnChatPresenceConfigUpdate:
            if (listener) listener->onChatPresenceConfigUpdate(event->getMegaChatApi(), event->getPresenceConfig());
            break;
        case QTMegaChatEvent::OnChatConnectionStateUpdate:
            if (listener) listener->onChatConnectionStateUpdate(event->getMegaChatApi(), event->getChatHandle(), event->getStatus());
            break;
        default:
            break;
    }
}
