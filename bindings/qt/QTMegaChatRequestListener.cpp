#include "QTMegaChatRequestListener.h"
#include "QTMegaChatEvent.h"
#include <QCoreApplication>

using namespace megachat;

QTMegaChatRequestListener::QTMegaChatRequestListener(MegaChatApi *megaChatApi, MegaChatRequestListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatRequestListener::~QTMegaChatRequestListener()
{
    if (megaChatApi)
    {
        megaChatApi->removeChatRequestListener(this);
    }
}

void QTMegaChatRequestListener::onRequestStart(MegaChatApi *api, MegaChatRequest *request)
{
    if (request->getType() == MegaChatRequest::TYPE_DELETE)
    {
        megaChatApi = NULL;
    }

    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnRequestStart);
    event->setChatRequest(request->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatRequestListener::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnRequestFinish);
    event->setChatRequest(request->copy());
    event->setChatError(e->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatRequestListener::onRequestUpdate(MegaChatApi *api, MegaChatRequest *request)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnRequestUpdate);
    event->setChatRequest(request->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatRequestListener::onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError *error)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnRequestTemporaryError);
    event->setChatRequest(request->copy());
    event->setChatError(error->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatRequestListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnRequestStart:
            if (listener) listener->onRequestStart(event->getMegaChatApi(), event->getChatRequest());
            break;
        case QTMegaChatEvent::OnRequestUpdate:
            if (listener) listener->onRequestUpdate(event->getMegaChatApi(), event->getChatRequest());
            break;
        case QTMegaChatEvent::OnRequestFinish:
            if (listener) listener->onRequestFinish(event->getMegaChatApi(), event->getChatRequest(), event->getChatError());
            break;
        case QTMegaChatEvent::OnRequestTemporaryError:
            if (listener) listener->onRequestTemporaryError(event->getMegaChatApi(), event->getChatRequest(), event->getChatError());
            break;
        default:
            break;
    }
}
