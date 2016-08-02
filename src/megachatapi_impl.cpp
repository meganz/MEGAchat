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

void megachat::MegaChatApiImpl::init(megachat::MegaChatApi *chatApi, megachat::MegaApi *megaApi)
{
    this->chatApi = chatApi;
    this->megaApi = megaApi;

    services_init(MegaChatApiImpl::megaApiPostMessage, SVC_STROPHE_LOG);

    mClient = new Client(*this, Presence::kOnline);
    mClient->init()
    .then([]()
    {
        KR_LOG_DEBUG("Client initialized");
    })
    .fail([](const promise::Error& error)
    {
        QMessageBox::critical(this, "rtctestapp", QString::fromLatin1("Client startup failed with error:\n")+QString::fromStdString(error.msg()));
//        this->close();
        exit(1);
    });
}

void MegaChatApiImpl::megaApiPostMessage(void* msg)
{
    // create event with "msg"
}

void MegaChatApiImpl::postMessage(void *msg)
{
    eventQueue.push(msg);
    // notify waiter to trigger a call to sendPendingEvents()
}

void MegaChatApiImpl::sendPendingEvents()
{
    void *msg;
    while((msg = eventQueue.pop()))
    {
//        sdkMutex.lock();
        megaProcessMessage(msg);
//        sdkMutex.unlock();
    }
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
