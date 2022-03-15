#include "QTMegaChatCallListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatCallListener::QTMegaChatCallListener(MegaChatApi *megaChatApi, MegaChatCallListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatCallListener::~QTMegaChatCallListener()
{ }

void QTMegaChatCallListener::onChatCallUpdate(MegaChatApi *api, MegaChatCall *call)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatCallUpdate);
    event->setChatCall(call->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatCallListener::onChatSessionUpdate(MegaChatApi *api, MegaChatHandle chatid, MegaChatHandle callid, MegaChatSession *session)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::onChatSessionUpdate);
    event->setChatSession(session->copy());
    event->setChatHandle(chatid);
    event->setChatCallid(callid);
    QCoreApplication::postEvent(this, event, INT_MIN);
}


void QTMegaChatCallListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(static_cast<QTMegaChatEvent::MegaType>(event->type()))
    {
        case QTMegaChatEvent::OnChatCallUpdate:
            if (listener) listener->onChatCallUpdate(event->getMegaChatApi(), event->getChatCall());
            break;
        case QTMegaChatEvent::onChatSessionUpdate:
            if (listener) listener->onChatSessionUpdate(event->getMegaChatApi(), event->getChatHandle(), event->getChatCallid(), event->getChatSession());
            break;
        default:
            break;
    }
}



