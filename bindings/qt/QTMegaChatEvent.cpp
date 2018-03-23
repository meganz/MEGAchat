#include "QTMegaChatEvent.h"

using namespace megachat;
using namespace std;

QTMegaChatEvent::QTMegaChatEvent(MegaChatApi *megaChatApi, Type type) : QEvent(type)
{
    this->megaChatApi = megaChatApi;
    request = NULL;
    error = NULL;
    item = NULL;
    handle = mega::INVALID_HANDLE;
    config = NULL;
    chat = NULL;
    msg = NULL;
    inProgress = false;
    status = 0;
}

QTMegaChatEvent::~QTMegaChatEvent()
{
    delete buffer;
    delete request;
    delete error;
    delete item;
    delete config;
    delete chat;
    delete msg;
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
    this->buffer = buffer;
}

void QTMegaChatEvent::setSize(size_t size)
{
    this->size = size;
}
