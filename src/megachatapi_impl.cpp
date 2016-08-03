/**
 * @file megachatapi_impl.cpp
 * @brief Private implementation of the intermediate layer for the MEGA C++ SDK.
 *
 * (c) 2013-2016 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#define _POSIX_SOURCE
#define _LARGE_FILES

#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

#define __DARWIN_C_LEVEL 199506L

#define USE_VARARGS
#define PREFER_STDARG

#include <megaapi_impl.h>

#include "megachatapi_impl.h"
#include <base/cservices.h>
#include <base/logger.h>

#ifndef _WIN32
#include <signal.h>
#endif

using namespace karere;
using namespace megachat;
using namespace mega;

MegaChatApiImpl::MegaChatApiImpl(MegaChatApi *chatApi, MegaApi *megaApi)
{
    init(chatApi, megaApi);
}

MegaChatApiImpl::MegaChatApiImpl(MegaChatApi *chatApi, const char *appKey, const char *appDir)
{
    init(chatApi, (MegaApi*) new MyMegaApi(appKey, appDir));
}

MegaChatApiImpl::~MegaChatApiImpl()
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DELETE);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::init(megachat::MegaChatApi *chatApi, megachat::MegaApi *megaApi)
{
    this->chatApi = chatApi;
    this->megaApi = megaApi;

    this->waiter = new MegaWaiter();
    this->mClient = NULL;   // created at loop()

    //Start blocking thread
    threadExit = 0;
    thread.start(threadEntryPoint, this);
}

//Entry point for the blocking thread
void *MegaChatApiImpl::threadEntryPoint(void *param)
{
#ifndef _WIN32
    struct sigaction noaction;
    memset(&noaction, 0, sizeof(noaction));
    noaction.sa_handler = SIG_IGN;
    ::sigaction(SIGPIPE, &noaction, 0);
#endif

    MegaChatApiImpl *chatApiImpl = (MegaChatApiImpl *)param;
    chatApiImpl->loop();
    return 0;
}

void MegaChatApiImpl::loop()
{
    // karere initialization
    services_init(MegaChatApiImpl::megaApiPostMessage, SVC_STROPHE_LOG);

    this->mClient = new Client(*this, Presence::kOnline);
    this->mClient->init()
    .then([]()
    {
        KR_LOG_DEBUG("Client initialized");
    })
    .fail([](const promise::Error& error)
    {
        KR_LOG_ERROR("Client startup failed with error: %s\n", error.msg());
        exit(-1);
    });

    while (true)
    {
        waiter->init(NEVER);
        waiter->wait();         // waken up directly by Waiter::notify()

        sendPendingRequests();
        sendPendingEvents();

        if(threadExit)
            break;
    }

    delete mClient;
}

void MegaChatApiImpl::megaApiPostMessage(void* msg)
{
    // Add the message to the queue of events
    // TODO: decide if a singleton is suitable to retrieve instance of MegaChatApi
//    MegaChatApiImpl *chatApi = MegaChatApiImpl::getMegaChatApi();
//    chatApi->postMessage(msg);
}

void MegaChatApiImpl::postMessage(void *msg)
{
    eventQueue.push(msg);
    waiter->notify();
}

void MegaChatApiImpl::sendPendingRequests()
{
    MegaChatRequestPrivate *request;
    error e;
    int nextTag = 0;

    while((request = requestQueue.pop()))
    {
        nextTag = ++reqtag;
        request->setTag(nextTag);
        requestMap[nextTag]=request;
        e = API_OK;

        fireOnRequestStart(request);

        switch (request->getType())
        {
        case MegaChatRequest::TYPE_SET_CHAT_STATUS:
        {

            break;
        }
        case MegaChatRequest::TYPE_START_CHAT_CALL:
        {
            break;
        }
        case MegaChatRequest::TYPE_ANSWER_CHAT_CALL:
        {
            break;
        }
        default:
        {
            e = API_EINTERNAL;
        }
        }   // end switch(request->getType())


        if(e)
        {
            MegaError err(e);
            KR_LOG_WARNING("Error starting request: %s", err.getErrorString());
            fireOnRequestFinish(request, err);
        }
    }
}

void MegaChatApiImpl::sendPendingEvents()
{
    void *msg;
    while((msg = eventQueue.pop()))
    {
        megaProcessMessage(msg);
    }
}


ChatRequestQueue::ChatRequestQueue()
{
    mutex.init(false);
}

void ChatRequestQueue::push(MegaChatRequestPrivate *request)
{
    mutex.lock();
    requests.push_back(request);
    mutex.unlock();
}

void ChatRequestQueue::push_front(MegaChatRequestPrivate *request)
{
    mutex.lock();
    requests.push_front(request);
    mutex.unlock();
}

MegaChatRequestPrivate *ChatRequestQueue::pop()
{
    mutex.lock();
    if(requests.empty())
    {
        mutex.unlock();
        return NULL;
    }
    MegaChatRequestPrivate *request = requests.front();
    requests.pop_front();
    mutex.unlock();
    return request;
}

void ChatRequestQueue::removeListener(MegaChatRequestListener *listener)
{
    mutex.lock();

    std::deque<MegaChatRequestPrivate *>::iterator it = requests.begin();
    while(it != requests.end())
    {
        MegaChatRequestPrivate *request = (*it);
        if(request->getListener()==listener)
            request->setListener(NULL);
        it++;
    }

    mutex.unlock();
}

EventQueue::EventQueue()
{
    mutex.init(false);
}

void EventQueue::push(void *transfer)
{
    mutex.lock();
    events.push_back(transfer);
    mutex.unlock();
}

void EventQueue::push_front(void *event)
{
    mutex.lock();
    events.push_front(event);
    mutex.unlock();
}

void* EventQueue::pop()
{
    mutex.lock();
    if(events.empty())
    {
        mutex.unlock();
        return NULL;
    }
    void* event = events.front();
    events.pop_front();
    mutex.unlock();
    return event;
}

MegaChatRequestPrivate::MegaChatRequestPrivate(int type, MegaChatRequestListener *listener)
{
    this->type = type;
    this->tag = 0;
    this->listener = listener;

    this->number = 0;
}

MegaChatRequestPrivate::MegaChatRequestPrivate(MegaChatRequestPrivate &request)
{
    this->type = request.getType();
    this->listener = request.getListener();
    this->setTag(request.getTag());

    this->setNumber(request.getNumber());
}

MegaChatRequestPrivate::~MegaChatRequestPrivate()
{
}

MegaChatRequest *MegaChatRequestPrivate::copy()
{
    return new MegaChatRequestPrivate(*this);
}

const char *MegaChatRequestPrivate::getRequestString() const
{
    switch(type)
    {
        case TYPE_DELETE: return "DELETE";
        case TYPE_SET_CHAT_STATUS: return "SET_CHAT_STATUS";
        case TYPE_START_CHAT_CALL: return "START_CHAT_CALL";
        case TYPE_ANSWER_CHAT_CALL: return "ANSWER_CHAT_CALL";
    }
    return "UNKNOWN";
}

MegaChatRequestListener *MegaChatRequestPrivate::getListener() const
{
    return listener;
}

int MegaChatRequestPrivate::getType() const
{
    return type;
}

long long MegaChatRequestPrivate::getNumber() const
{
    return number;
}

int MegaChatRequestPrivate::getTag() const
{
    return tag;
}

void MegaChatRequestPrivate::setListener(MegaChatRequestListener *listener)
{
    this->listener = listener;
}

void MegaChatRequestPrivate::setTag(int tag)
{
    this->tag = tag;
}

void MegaChatRequestPrivate::setNumber(long long number)
{
    this->number = number;
}
