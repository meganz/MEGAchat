#include "QTMegaChatEvent.h"

using namespace megachat;
using namespace std;

QTMegaChatEvent::QTMegaChatEvent(MegaChatApi *megaChatApi, Type type) : QEvent(type)
{
    this->megaChatApi = megaChatApi;
    request = nullptr;
    error = nullptr;
    item = nullptr;
    handle = ::mega::INVALID_HANDLE;
    config = nullptr;
    chat = nullptr;
    msg = nullptr;
    buffer = nullptr;
    inProgress = false;
    status = 0;
    call = nullptr;
    callid = MEGACHAT_INVALID_HANDLE;
    session = nullptr;
    schedMeeting = nullptr;
    schedMeetingOccurr = nullptr;
}

QTMegaChatEvent::~QTMegaChatEvent()
{
    delete [] buffer;
    delete request;
    delete error;
    delete item;
    delete config;
    delete chat;
    delete msg;
    delete call;
    delete session;
    delete schedMeeting;
    delete schedMeetingOccurr;
}

MegaChatApi *QTMegaChatEvent::getMegaChatApi()
{
    return megaChatApi;
}

MegaChatRequest *QTMegaChatEvent::getChatRequest()
{
    return request;
}

MegaChatError *QTMegaChatEvent::getChatError()
{
    return error;
}

MegaChatListItem *QTMegaChatEvent::getChatListItem()
{
    return item;
}

MegaChatHandle QTMegaChatEvent::getChatHandle()
{
    return handle;
}

MegaChatPresenceConfig *QTMegaChatEvent::getPresenceConfig()
{
    return config;
}

MegaChatRoom *QTMegaChatEvent::getChatRoom()
{
    return chat;
}

MegaChatMessage *QTMegaChatEvent::getChatMessage()
{
    return msg;
}

MegaChatCall *QTMegaChatEvent::getChatCall()
{
    return call;
}

bool QTMegaChatEvent::getProgress()
{
    return inProgress;
}

int QTMegaChatEvent::getStatus()
{
    return status;
}

int QTMegaChatEvent::getWidth()
{
    return width;
}

int QTMegaChatEvent::getHeight()
{
    return height;
}

char *QTMegaChatEvent::getBuffer()
{
    return buffer;
}

size_t QTMegaChatEvent::getSize()
{
    return size;
}

MegaChatSession *QTMegaChatEvent::getChatSession()
{
    return session;
}

MegaChatScheduledMeeting *QTMegaChatEvent::getSchedMeeting()
{
    return schedMeeting;
}

MegaChatScheduledMeetingList* QTMegaChatEvent::getSchedMeetingOccurr()
{
    return schedMeetingOccurr;
}

MegaChatHandle QTMegaChatEvent::getChatCallid()
{
    return callid;
}

void QTMegaChatEvent::setChatRequest(MegaChatRequest *request)
{
    this->request = request;
}

void QTMegaChatEvent::setChatError(MegaChatError *error)
{
    this->error = error;
}

void QTMegaChatEvent::setChatListItem(MegaChatListItem *item)
{
    this->item = item;
}

void QTMegaChatEvent::setChatHandle(MegaChatHandle handle)
{
    this->handle = handle;
}

void QTMegaChatEvent::setPresenceConfig(MegaChatPresenceConfig *config)
{
    this->config = config;
}

void QTMegaChatEvent::setChatRoom(MegaChatRoom *chat)
{
    this->chat = chat;
}

void QTMegaChatEvent::setChatMessage(MegaChatMessage *msg)
{
    this->msg = msg;
}

void QTMegaChatEvent::setChatCall(MegaChatCall *call)
{
    this->call = call;
}

void QTMegaChatEvent::setProgress(bool progress)
{
    this->inProgress = progress;
}

void QTMegaChatEvent::setStatus(int status)
{
    this->status = status;
}

void QTMegaChatEvent::setWidth(int width)
{
    this->width = width;
}

void QTMegaChatEvent::setHeight(int height)
{
    this->height = height;
}

void QTMegaChatEvent::setBuffer(char *buffer)
{
    if (this->buffer)
    {
        delete [] this->buffer;
    }

    this->buffer = buffer;
}

void QTMegaChatEvent::setSize(size_t size)
{
    this->size = size;
}

void QTMegaChatEvent::setChatSession(MegaChatSession *session)
{
    this->session = session;
}

void QTMegaChatEvent::setSchedMeeting(MegaChatScheduledMeeting* sm)
{
    this->schedMeeting = sm;
}

void QTMegaChatEvent::setSchedMeetingOccurr(MegaChatScheduledMeetingList* l)
{
    schedMeetingOccurr = l;
}

void QTMegaChatEvent::setChatCallid(MegaChatHandle callid)
{
    this->callid = callid;
}
