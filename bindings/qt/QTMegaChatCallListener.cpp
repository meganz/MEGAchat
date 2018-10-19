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


void QTMegaChatCallListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnChatCallUpdate:
            if (listener) listener->onChatCallUpdate(event->getMegaChatApi(), event->getChatCall());
            break;
        default:
            break;
    }
}



