#ifndef QTMEGACHATSCHEDULEDMEETINGLISTENER_H
#define QTMEGACHATSCHEDULEDMEETINGLISTENER_H

#include <QObject>
#include "megachatapi.h"

namespace megachat
{
class QTMegaChatScheduledMeetingListener : public QObject, public MegaChatScheduledMeetingListener
{
    Q_OBJECT

public:
    QTMegaChatScheduledMeetingListener(MegaChatApi* megaChatApi, MegaChatScheduledMeetingListener* parent = NULL);
    virtual ~QTMegaChatScheduledMeetingListener();
    virtual void onChatSchedMeetingUpdate(MegaChatApi* api, MegaChatScheduledMeeting* sm);

protected:
    virtual void customEvent(QEvent* event);
    MegaChatApi* megaChatApi;
    MegaChatScheduledMeetingListener* listener;
};
}

#endif // QTMEGACHATSCHEDULEDMEETINGLISTENER_H
