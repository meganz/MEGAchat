#include "QTMegaChatNodeHistoryListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatNodeHistoryListener::QTMegaChatNodeHistoryListener(MegaChatApi *megaChatApi, MegaChatNodeHistoryListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatNodeHistoryListener::~QTMegaChatNodeHistoryListener()
{ }

void QTMegaChatNodeHistoryListener::onAttachmentLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnAttachmentLoaded);
    event->setChatMessage(msg ? msg->copy() : NULL);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatNodeHistoryListener::onAttachmentReceived(MegaChatApi *api, MegaChatMessage *msg)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnAttachmentReceived);
    event->setChatMessage(msg ? msg->copy() : NULL);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatNodeHistoryListener::onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnAttachmentDeleted);
    event->setChatHandle(msgid);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatNodeHistoryListener::onTruncate(MegaChatApi *api, MegaChatHandle msgid)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnAttachmentTruncated);
    event->setChatHandle(msgid);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatNodeHistoryListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnAttachmentLoaded:
            if (listener) listener->onAttachmentLoaded(event->getMegaChatApi(), event->getChatMessage());
            break;
        case QTMegaChatEvent::OnAttachmentReceived:
            if (listener) listener->onAttachmentReceived(event->getMegaChatApi(), event->getChatMessage());
            break;
        case QTMegaChatEvent::OnAttachmentDeleted:
            if (listener) listener->onAttachmentDeleted(event->getMegaChatApi(), event->getChatHandle());
            break;
        case QTMegaChatEvent::OnAttachmentTruncated:
            if (listener) listener->onTruncate(event->getMegaChatApi(), event->getChatHandle());
            break;
        default:
            break;
    }
}
