#include "QTMegaChatVideoListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatVideoListener::QTMegaChatVideoListener(MegaChatApi *megaChatApi, MegaChatVideoListener *listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatVideoListener::~QTMegaChatVideoListener()
{ }

void QTMegaChatVideoListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::OnChatVideoData);
    event->setChatHandle(chatid);
    event->setWidth(width);
    event->setHeight(height);
    event->setSize(size);

    char * auxBuff = new char[size];
    memcpy(auxBuff, buffer, size);
    event->setBuffer(auxBuff);
    event->setSize(size);
    event->setBuffer(auxBuff);
    QCoreApplication::postEvent(this, event, INT_MIN);
}


void QTMegaChatVideoListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(event->type())
    {
        case QTMegaChatEvent::OnChatVideoData:
            if (listener)
            {
                char * auxBuff = event->getBuffer();
                listener->onChatVideoData(event->getMegaChatApi(), event->getChatHandle(), event->getWidth(), event->getHeight(), auxBuff, event->getSize());
                delete auxBuff;
            }
            break;
        default:
            break;
    }
}



