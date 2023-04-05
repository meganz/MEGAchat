#include "QTMegaChatSchedMeetListener.h"
#include "QTMegaChatEvent.h"

#include <QCoreApplication>

using namespace megachat;

QTMegaChatScheduledMeetingListener::QTMegaChatScheduledMeetingListener(MegaChatApi* megaChatApi, MegaChatScheduledMeetingListener* listener) : QObject()
{
    this->megaChatApi = megaChatApi;
    this->listener = listener;
}

QTMegaChatScheduledMeetingListener::~QTMegaChatScheduledMeetingListener()
{

}

void QTMegaChatScheduledMeetingListener::onChatSchedMeetingUpdate(MegaChatApi* api, MegaChatScheduledMeeting* sm)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::onChatSchedMeetingUpdate);
    event->setSchedMeeting(sm->copy());
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatScheduledMeetingListener::onSchedMeetingOccurrencesUpdate(MegaChatApi* api, MegaChatHandle chatid, bool append)
{
    QTMegaChatEvent *event = new QTMegaChatEvent(api, (QEvent::Type)QTMegaChatEvent::onSchedMeetingOccurrencesChange);
    event->setChatHandle(chatid);
    event->setProgress(append);
    QCoreApplication::postEvent(this, event, INT_MIN);
}

void QTMegaChatScheduledMeetingListener::customEvent(QEvent *e)
{
    QTMegaChatEvent *event = (QTMegaChatEvent *)e;
    switch(static_cast<QTMegaChatEvent::MegaType>(event->type()))
    {
        case QTMegaChatEvent::onChatSchedMeetingUpdate:
            if (listener) listener->onChatSchedMeetingUpdate(event->getMegaChatApi(), event->getSchedMeeting());
            break;
        case QTMegaChatEvent::onSchedMeetingOccurrencesChange:
            if (listener) listener->onSchedMeetingOccurrencesUpdate(event->getMegaChatApi(), event->getChatHandle(), event->getProgress());
            break;
        default:
            break;
    }
}
