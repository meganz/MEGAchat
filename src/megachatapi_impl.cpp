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
#include <IGui.h>

#ifndef _WIN32
#include <signal.h>
#endif

using namespace std;
using namespace megachat;
using namespace mega;
using namespace karere;

MegaChatApiImpl *MegaChatApiImpl::megaChatApiRef = NULL;

MegaChatApiImpl::MegaChatApiImpl(MegaChatApi *chatApi, MegaApi *megaApi)
{
    init(chatApi, megaApi);

    MegaChatApiImpl::megaChatApiRef = this;
}

//MegaChatApiImpl::MegaChatApiImpl(MegaChatApi *chatApi, const char *appKey, const char *appDir)
//{
//    init(chatApi, (MegaApi*) new MyMegaApi(appKey, appDir));
//}

MegaChatApiImpl::~MegaChatApiImpl()
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DELETE);
    requestQueue.push(request);
    waiter->notify();
    thread.join();
}

void MegaChatApiImpl::init(megachat::MegaChatApi *chatApi, mega::MegaApi *megaApi)
{
    this->chatApi = chatApi;
    this->megaApi = megaApi;

    this->waiter = new MegaWaiter();
    this->mClient = NULL;   // created at loop()

    this->status = MegaChatApi::STATUS_OFFLINE;

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

    this->mClient = new karere::Client(*this->megaApi, *this, karere::Presence::kOnline);

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

    rtcModule::globalCleanup();
    services_shutdown();
}

void MegaChatApiImpl::megaApiPostMessage(void* msg)
{
    // Add the message to the queue of events
    // TODO: decide if a singleton is suitable to retrieve instance of MegaChatApi,
    // or it's better to change karere to pass the instance of the MegaChatApi as parameter
//    MegaChatApiImpl *chatApi = MegaChatApiImpl::getMegaChatApi();
    megaChatApiRef->postMessage(msg);
}

void MegaChatApiImpl::postMessage(void *msg)
{
    eventQueue.push(msg);
    waiter->notify();
}

void MegaChatApiImpl::sendPendingRequests()
{
    MegaChatRequestPrivate *request;
    int errorCode = MegaChatError::ERROR_OK;
    int nextTag = 0;

    while((request = requestQueue.pop()))
    {
//        sdkMutex.lock();
        nextTag = ++reqtag;
        request->setTag(nextTag);
        requestMap[nextTag]=request;
        errorCode = MegaChatError::ERROR_OK;

        fireOnChatRequestStart(request);

        switch (request->getType())
        {
        case MegaChatRequest::TYPE_CONNECT:
        {
            mClient->connect()
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& e)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(e.msg(), e.code(), e.type());
                fireOnChatRequestFinish(request, megaChatError);
            });

            break;
        }
        case MegaChatRequest::TYPE_DELETE:
        {
            mClient->terminate()
            .then([this]()
            {
                KR_LOG_ERROR("Chat engine closed!");
                threadExit = 1;
            })
            .fail([](const promise::Error& err)
            {
                KR_LOG_ERROR("Error closing chat engine: ", err.what());
            });
            break;
        }
        case MegaChatRequest::TYPE_SET_ONLINE_STATUS:
        {
            MegaChatApi::Status status = (MegaChatApi::Status) request->getNumber();
            if (status < MegaChatApi::STATUS_OFFLINE || status > MegaChatApi::STATUS_CHATTY)
            {
                fireOnChatRequestFinish(request, new MegaChatErrorPrivate("Invalid online status", MegaChatError::ERROR_ARGS));
                break;
            }

            mClient->setPresence(request->getNumber(), true)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                KR_LOG_ERROR("Error setting online status: ", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }

        case MegaChatRequest::TYPE_CREATE_CHATROOM:
        {
            MegaChatPeerList *peersList = request->getMegaChatPeerList();
            if (!peersList)   // refuse to create chats without participants
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            bool group = request->getFlag();
            const userpriv_vector *userpriv = ((MegaChatPeerListPrivate*)peersList)->getList();
            if (!userpriv || (!group && peersList->size() > 1))
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            mega::MegaTextChatPeerListPrivate peers;
            peers.addPeer(mClient->myHandle(), MegaChatRoom::PRIV_STANDARD);
            mClient->api.call(&mega::MegaApi::createChat, group, &peers)
            .then([request, this](ReqResult result)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                KR_LOG_ERROR("Error creating chatroom: ", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_INVITE_TO_CHATROOM:
        {
            handle chatid = request->getChatHandle();
            handle uh = request->getUserHandle();
            chatd::Priv privilege = (chatd::Priv) request->getPrivilege();

            if (chatid == INVALID_HANDLE || uh == INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            karere::ChatRoomList::iterator it = mClient->chats->find(chatid);
            if (it == mClient->chats->end())
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            karere::ChatRoom *chatroom = it->second;
            if (chatroom->ownPriv() != (chatd::Priv) MegaChatPeerList::PRIV_MODERATOR)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }
            if (!chatroom->isGroup())   // invite only for group chats
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ((karere::GroupChatRoom *)chatroom)->invite(uh, privilege)
//            mClient->api.call(&::mega::MegaApi::inviteToChat, chatid, uid, privilege)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                KR_LOG_ERROR("Error adding user to group chat: ", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });

            break;
        }
        default:
        {
            errorCode = MegaChatError::ERROR_UNKNOWN;
        }
        }   // end of switch(request->getType())


        if(errorCode)
        {
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(errorCode);
            KR_LOG_WARNING("Error starting request: %s", megaChatError->getErrorString());
            fireOnChatRequestFinish(request, megaChatError);
        }

//        sdkMutex.unlock();
    }
}

void MegaChatApiImpl::sendPendingEvents()
{
    void *msg;
    while((msg = eventQueue.pop()))
    {
//        sdkMutex.lock();
        megaProcessMessage(msg);
//		sdkMutex.unlock();
    }
}

void MegaChatApiImpl::fireOnChatRequestStart(MegaChatRequestPrivate *request)
{
    KR_LOG_INFO("Request (%s) starting", request->getRequestString());

    for (set<MegaChatRequestListener *>::iterator it = requestListeners.begin(); it != requestListeners.end() ; it++)
    {
        (*it)->onRequestStart(chatApi, request);
    }

    MegaChatRequestListener* listener = request->getListener();
    if (listener)
    {
        listener->onRequestStart(chatApi, request);
    }
}

void MegaChatApiImpl::fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaChatError *e)
{
    if(e->getErrorCode())
    {
        KR_LOG_INFO("Request (%s) finished with error: %s", request->getRequestString(), e->getErrorString());
    }
    else
    {
        KR_LOG_INFO("Request (%s) finished", request->getRequestString());
    }

    for (set<MegaChatRequestListener *>::iterator it = requestListeners.begin(); it != requestListeners.end() ; it++)
    {
        (*it)->onRequestFinish(chatApi, request, e);
    }

    MegaChatRequestListener* listener = request->getListener();
    if (listener)
    {
        listener->onRequestFinish(chatApi, request, e);
    }

    requestMap.erase(request->getTag());

    delete request;
    delete e;
}

void MegaChatApiImpl::fireOnChatRequestUpdate(MegaChatRequestPrivate *request)
{
    for (set<MegaChatRequestListener *>::iterator it = requestListeners.begin(); it != requestListeners.end() ; it++)
    {
        (*it)->onRequestUpdate(chatApi, request);
    }

    MegaChatRequestListener* listener = request->getListener();
    if (listener)
    {
        listener->onRequestUpdate(chatApi, request);
    }
}

void MegaChatApiImpl::fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaChatError *e)
{
    request->setNumRetry(request->getNumRetry() + 1);

    for (set<MegaChatRequestListener *>::iterator it = requestListeners.begin(); it != requestListeners.end() ; it++)
    {
        (*it)->onRequestTemporaryError(chatApi, request, e);
    }

    MegaChatRequestListener* listener = request->getListener();
    if (listener)
    {
        listener->onRequestTemporaryError(chatApi, request, e);
    }

    delete e;
}

void MegaChatApiImpl::fireOnChatCallStart(MegaChatCallPrivate *call)
{
    KR_LOG_INFO("Starting chat call");

    for(set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallStart(chatApi, call);
    }

    fireOnChatCallStateChange(call);
}

void MegaChatApiImpl::fireOnChatCallStateChange(MegaChatCallPrivate *call)
{
    KR_LOG_INFO("Chat call state changed to %s", call->getStatus());

    for(set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallStateChange(chatApi, call);
    }
}

void MegaChatApiImpl::fireOnChatCallTemporaryError(MegaChatCallPrivate *call, MegaChatError *e)
{
    KR_LOG_INFO("Chat call temporary error: %s", e->getErrorString());

    for(set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallTemporaryError(chatApi, call, e);
    }
}

void MegaChatApiImpl::fireOnChatCallFinish(MegaChatCallPrivate *call, MegaChatError *e)
{
    if(e->getErrorCode())
    {
        KR_LOG_INFO("Chat call finished with error: %s", e->getErrorString());
    }
    else
    {
        KR_LOG_INFO("Chat call finished");
    }

    call->setStatus(MegaChatCall::CALL_STATUS_DISCONNECTED);
    fireOnChatCallStateChange(call);

    for (set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallFinish(chatApi, call, e);
    }

    callMap.erase(call->getTag());

    delete call;
    delete e;
}

void MegaChatApiImpl::fireOnChatRemoteVideoData(MegaChatCallPrivate *call, int width, int height, char *buffer)
{
    KR_LOG_INFO("Remote video data");

    for(set<MegaChatVideoListener *>::iterator it = remoteVideoListeners.begin(); it != remoteVideoListeners.end() ; it++)
    {
        (*it)->onChatVideoData(chatApi, call, width, height, buffer);
    }
}

void MegaChatApiImpl::fireOnChatLocalVideoData(MegaChatCallPrivate *call, int width, int height, char *buffer)
{
    KR_LOG_INFO("Local video data");

    for(set<MegaChatVideoListener *>::iterator it = localVideoListeners.begin(); it != localVideoListeners.end() ; it++)
    {
        (*it)->onChatVideoData(chatApi, call, width, height, buffer);
    }
}

void MegaChatApiImpl::fireOnChatRoomUpdate(MegaChatRoomList *chats)
{
    KR_LOG_INFO("Initialization complete");

    for(set<MegaChatGlobalListener *>::iterator it = globalListeners.begin(); it != globalListeners.end() ; it++)
    {
        (*it)->onChatRoomUpdate(chatApi, chats);
    }
}

void MegaChatApiImpl::fireOnOnlineStatusUpdate(MegaChatApi::Status status)
{
    KR_LOG_INFO("Online status changed");

    for(set<MegaChatGlobalListener *>::iterator it = globalListeners.begin(); it != globalListeners.end() ; it++)
    {
        (*it)->onOnlineStatusUpdate(chatApi, status);
    }
}

void MegaChatApiImpl::init()
{
//    sdkMutex.lock();
    mClient->initWithExistingSession();
//    sdkMutex.unlock();
}

void MegaChatApiImpl::connect(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_CONNECT, listener);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::setOnlineStatus(int status, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SET_ONLINE_STATUS, listener);
    request->setNumber(status);
    requestQueue.push(request);
    waiter->notify();
}

MegaChatRoomList *MegaChatApiImpl::getChatRooms()
{
    // TODO: get the chatlist from mClient and create the corresponding MegaChatRoom's
    MegaChatRoomListPrivate *chats = new MegaChatRoomListPrivate();

    ChatRoomList::iterator it;
    for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
    {
        chats->addChatRoom(new MegaChatRoomPrivate(it->second));
    }

    return chats;
}

void MegaChatApiImpl::createChat(bool group, MegaChatPeerList *peerList, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_CREATE_CHATROOM, listener);
    request->setFlag(group);
    request->setMegaChatPeerList(peerList);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::inviteToChat(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_INVITE_TO_CHATROOM, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(uh);
    request->setPrivilege(privilege);
    requestQueue.push(request);
    waiter->notify();
}

MegaStringList *MegaChatApiImpl::getChatAudioInDevices()
{
    return NULL;
}

MegaStringList *MegaChatApiImpl::getChatVideoInDevices()
{
    return NULL;
}

bool MegaChatApiImpl::setChatAudioInDevice(const char *device)
{
    return true;
}

bool MegaChatApiImpl::setChatVideoInDevice(const char *device)
{
    return true;
}

void MegaChatApiImpl::startChatCall(MegaUser *peer, bool enableVideo, MegaChatRequestListener *listener)
{

}

void MegaChatApiImpl::answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener)
{

}

void MegaChatApiImpl::hangAllChatCalls()
{

}

void MegaChatApiImpl::addChatGlobalListener(MegaChatGlobalListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    globalListeners.insert(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatCallListener(MegaChatCallListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    callListeners.insert(listener);
//    sdkMutex.unlock();

}

void MegaChatApiImpl::addChatRequestListener(MegaChatRequestListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    requestListeners.insert(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatLocalVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    localVideoListeners.insert(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    remoteVideoListeners.insert(listener);
    //    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatListener(MegaChatListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    listeners.insert(listener);
    //    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatGlobalListener(MegaChatGlobalListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    globalListeners.erase(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatCallListener(MegaChatCallListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    callListeners.erase(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatRequestListener(MegaChatRequestListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    requestListeners.erase(listener);

    std::map<int,MegaChatRequestPrivate*>::iterator it = requestMap.begin();
    while (it != requestMap.end())
    {
        MegaChatRequestPrivate* request = it->second;
        if(request->getListener() == listener)
        {
            request->setListener(NULL);
        }

        it++;
    }

    requestQueue.removeListener(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatLocalVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    localVideoListeners.erase(listener);
//    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    remoteVideoListeners.erase(listener);
    //    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatListener(MegaChatListener *listener)
{
    if (!listener)
    {
        return;
    }

//    sdkMutex.lock();
    listeners.erase(listener);
    //    sdkMutex.unlock();
}

karere::IApp::IChatHandler *MegaChatApiImpl::createChatHandler(karere::ChatRoom &room)
{
    // TODO: create the chat handler and add listeners to it
    // TODO: return the object implementing IChatHandler

    return NULL;
}

karere::IApp::IContactListHandler& MegaChatApiImpl::contactListHandler()
{
    return *this;
}

void MegaChatApiImpl::onIncomingContactRequest(const MegaContactRequest &req)
{
    // it is notified to the app by the existing MegaApi
}

rtcModule::IEventHandler *MegaChatApiImpl::onIncomingCall(const std::shared_ptr<rtcModule::ICallAnswer> &ans)
{
    // TODO: create the call object implementing IEventHandler and return it
    return new MegaChatCallPrivate(ans);
}

karere::IApp::IContactListItem *MegaChatApiImpl::addContactItem(karere::Contact &contact)
{
    // TODO: create the object MegaChatListItemHandler, save it in a list and return it
    return new MegaChatListItemHandler();
}

karere::IApp::IContactListItem *MegaChatApiImpl::addGroupChatItem(karere::GroupChatRoom &room)
{
    // TODO: create the object MegaChatListItemHandler, save it in a list and return it
    return new MegaChatListItemHandler();
}

void MegaChatApiImpl::removeContactItem(IContactListItem *item)
{
    // TODO: remove the corresponding MegaChatListItemHandler from the list
}

void MegaChatApiImpl::removeGroupChatItem(IContactListItem *item)
{
    // TODO: remove the corresponding MegaChatListItemHandler from the list
}

karere::IApp::IChatHandler& MegaChatApiImpl::chatHandlerForPeer(uint64_t handle)
{
    // TODO: create a new chatroomhandler and keep the reference, so events can
    // be processed and notified to the app
    MegaChatRoomHandler *chatroomHandler = new MegaChatRoomHandler();
    return *chatroomHandler;
}

void MegaChatApiImpl::onOwnPresence(karere::Presence pres)
{
    this->status = (MegaChatApi::Status) pres.status();
    fireOnOnlineStatusUpdate(status);
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
    this->retry = 0;
    this->flag = false;
    this->peerList = NULL;
    this->chatid = INVALID_HANDLE;
    this->userHandle = INVALID_HANDLE;
    this->privilege = MegaChatPeerList::PRIV_UNKNOWN;
}

MegaChatRequestPrivate::MegaChatRequestPrivate(MegaChatRequestPrivate &request)
{
    this->type = request.getType();
    this->listener = request.getListener();
    this->setTag(request.getTag());

    this->setNumber(request.getNumber());
    this->setNumRetry(request.getNumRetry());
    this->setFlag(request.getFlag());
    this->setMegaChatPeerList(request.getMegaChatPeerList());
    this->setChatHandle(request.getChatHandle());
    this->setUserHandle(request.getUserHandle());
    this->setPrivilege(request.getPrivilege());
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
        case TYPE_CONNECT: return "CONNECT";
        case TYPE_SET_ONLINE_STATUS: return "SET_CHAT_STATUS";
        case TYPE_CREATE_CHATROOM: return "CREATE CHATROOM";
        case TYPE_INVITE_TO_CHATROOM: return "INVITE_TO_CHATROOM";

        case TYPE_START_CHAT_CALL: return "START_CHAT_CALL";
        case TYPE_ANSWER_CHAT_CALL: return "ANSWER_CHAT_CALL";
    }
    return "UNKNOWN";
}

const char *MegaChatRequestPrivate::toString() const
{
    return getRequestString();
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

int MegaChatRequestPrivate::getNumRetry() const
{
    return retry;
}

bool MegaChatRequestPrivate::getFlag() const
{
    return flag;
}

MegaChatPeerList *MegaChatRequestPrivate::getMegaChatPeerList()
{
    return peerList;
}

MegaChatHandle MegaChatRequestPrivate::getChatHandle()
{
    return chatid;
}

MegaChatHandle MegaChatRequestPrivate::getUserHandle()
{
    return userHandle;
}

int MegaChatRequestPrivate::getPrivilege()
{
    return privilege;
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

void MegaChatRequestPrivate::setNumRetry(int retry)
{
    this->retry = retry;
}

void MegaChatRequestPrivate::setFlag(bool flag)
{
    this->flag = flag;
}

void MegaChatRequestPrivate::setMegaChatPeerList(MegaChatPeerList *peerList)
{
    if (this->peerList)
        delete this->peerList;

    this->peerList = peerList->copy();
}

void MegaChatRequestPrivate::setChatHandle(MegaChatHandle chatid)
{
    this->chatid = chatid;
}

void MegaChatRequestPrivate::setUserHandle(MegaChatHandle userhandle)
{
    this->userHandle = userhandle;
}

void MegaChatRequestPrivate::setPrivilege(int priv)
{
    this->privilege = priv;
}

MegaChatCallPrivate::MegaChatCallPrivate(const std::shared_ptr<rtcModule::ICallAnswer> &ans)
{
    mAns = ans;
    this->peer = mAns->call()->peerJid().c_str();
    status = 0;
    tag = 0;
    videoReceiver = NULL;
}

MegaChatCallPrivate::MegaChatCallPrivate(const char *peer)
{
    this->peer = mega::MegaApi::strdup(peer);
    status = 0;
    tag = 0;
    videoReceiver = NULL;
    mAns = NULL;
}

MegaChatCallPrivate::MegaChatCallPrivate(const MegaChatCallPrivate &call)
{
    this->peer = mega::MegaApi::strdup(call.getPeer());
    this->status = call.getStatus();
    this->tag = call.getTag();
    this->videoReceiver = NULL;
    mAns = NULL;
}

MegaChatCallPrivate::~MegaChatCallPrivate()
{
    delete [] peer;
//    delete mAns;  it's a shared pointer, no need to delete
    // videoReceiver is deleted on onVideoDetach, because it's called after the call is finished
}

MegaChatCall *MegaChatCallPrivate::copy()
{
    return new MegaChatCallPrivate(*this);
}

int MegaChatCallPrivate::getStatus() const
{
    return status;
}

int MegaChatCallPrivate::getTag() const
{
    return tag;
}

MegaChatHandle MegaChatCallPrivate::getContactHandle() const
{
    if(!peer)
    {
        return INVALID_HANDLE;
    }

    MegaChatHandle userHandle = INVALID_HANDLE;
    string tmp = peer;
    tmp.resize(13);
    Base32::atob(tmp.data(), (byte *)&userHandle, sizeof(userHandle));
    return userHandle;
}

//shared_ptr<rtcModule::ICallAnswer> MegaChatCallPrivate::getAnswerObject()
//{
//    return mAns;
//}

const char *MegaChatCallPrivate::getPeer() const
{
    return peer;
}

void MegaChatCallPrivate::setStatus(int status)
{
    this->status = status;
}

void MegaChatCallPrivate::setTag(int tag)
{
    this->tag = tag;
}

void MegaChatCallPrivate::setVideoReceiver(MegaChatVideoReceiver *videoReceiver)
{
    delete this->videoReceiver;
    this->videoReceiver = videoReceiver;
}

std::shared_ptr<rtcModule::ICall> MegaChatCallPrivate::call() const
{
    return nullptr;
}

bool MegaChatCallPrivate::reqStillValid() const
{
    return false;
}

std::set<string> *MegaChatCallPrivate::files() const
{
    return NULL;
}

AvFlags MegaChatCallPrivate::peerMedia() const
{
    AvFlags ret;
    return ret;
}

bool MegaChatCallPrivate::answer(bool accept, AvFlags ownMedia)
{
    return false;
}

//void MegaChatCallPrivate::setAnswerObject(rtcModule::ICallAnswer *answerObject)
//{
//    this->mAns = answerObject;
//}

MegaChatVideoReceiver::MegaChatVideoReceiver(MegaChatApiImpl *chatApi, MegaChatCallPrivate *call, bool local)
{
    this->chatApi = chatApi;
    this->call = call;
    this->local = local;
}

MegaChatVideoReceiver::~MegaChatVideoReceiver()
{
}

unsigned char *MegaChatVideoReceiver::getImageBuffer(unsigned short width, unsigned short height, void **userData)
{
    MegaChatVideoFrame *frame = new MegaChatVideoFrame;
    frame->width = width;
    frame->height = height;
    frame->buffer = new byte[width * height * 4];  // in format ARGB: 4 bytes per pixel
    *userData = frame;
    return frame->buffer;
}

void MegaChatVideoReceiver::frameComplete(void *userData)
{
    MegaChatVideoFrame *frame = (MegaChatVideoFrame *)userData;
    if(local)
    {
        chatApi->fireOnChatLocalVideoData(call, frame->width, frame->height, (char *)frame->buffer);
    }
    else
    {
        chatApi->fireOnChatRemoteVideoData(call, frame->width, frame->height, (char *)frame->buffer);
    }
    delete frame->buffer;
    delete frame;
}

void MegaChatVideoReceiver::onVideoAttach()
{

}

void MegaChatVideoReceiver::onVideoDetach()
{
    delete this;
}

void MegaChatVideoReceiver::clearViewport()
{
}

void MegaChatVideoReceiver::released()
{

}


IApp::ICallHandler *MegaChatRoomHandler::callHandler()
{
    // TODO: create a MegaChatCallPrivate() with the peer information and return it
    return NULL;
}

void MegaChatRoomHandler::onTitleChanged(const string &title)
{

}

void MegaChatRoomHandler::onPresenceChanged(karere::Presence state)
{

}

void MegaChatRoomHandler::init(chatd::Chat &messages, chatd::DbInterface *&dbIntf)
{

}


MegaChatErrorPrivate::MegaChatErrorPrivate(const string &msg, int code, int type)
    : promise::Error(msg, code, type)
{

}

MegaChatErrorPrivate::MegaChatErrorPrivate(int code, int type)
    : promise::Error(MegaChatErrorPrivate::getGenericErrorString(code), code, type)
{
}

const char* MegaChatErrorPrivate::getGenericErrorString(int errorCode)
{
    switch(errorCode)
    {
    case ERROR_OK:
        return "No error";
    case ERROR_ARGS:
        return "Invalid argument";
    case ERROR_UNKNOWN:
    default:
        return "Unknown error";
    }
}


MegaChatErrorPrivate::MegaChatErrorPrivate(const MegaChatErrorPrivate *error)
    : promise::Error(error->getErrorString(), error->getErrorCode(), error->getErrorType())
{

}

int MegaChatErrorPrivate::getErrorCode() const
{
    return code();
}

int MegaChatErrorPrivate::getErrorType() const
{
    return type();
}

const char *MegaChatErrorPrivate::getErrorString() const
{
    return what();
}

const char *MegaChatErrorPrivate::toString() const
{
    char *errorString = new char[msg().size()+1];
    strcpy(errorString, what());
    return errorString;
}

MegaChatError *MegaChatErrorPrivate::copy()
{
    return new MegaChatErrorPrivate(this);
}


MegaChatRoomListPrivate::MegaChatRoomListPrivate()
{

}

MegaChatRoomListPrivate::MegaChatRoomListPrivate(const MegaChatRoomListPrivate *list)
{
    MegaChatRoomPrivate *chat;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        chat = new MegaChatRoomPrivate(list->get(i));
        this->list.push_back(chat);
    }
}

MegaChatRoomList *MegaChatRoomListPrivate::copy() const
{
    return new MegaChatRoomListPrivate(this);
}

const MegaChatRoom *MegaChatRoomListPrivate::get(unsigned int i) const
{
    if (i >= size())
    {
        return NULL;
    }
    else
    {
        return list.at(i);
    }
}

unsigned int MegaChatRoomListPrivate::size() const
{
    return list.size();
}

void MegaChatRoomListPrivate::addChatRoom(MegaChatRoom *chat)
{
    list.push_back(chat);
}


MegaChatRoomPrivate::MegaChatRoomPrivate(const MegaChatRoom *chat)
{
    this->id = chat->getHandle();
    this->priv = chat->getOwnPrivilege();
    for (unsigned int i = 0; i < chat->getPeerCount(); i++)
    {
        peers.push_back(userpriv_pair(chat->getPeerHandle(i), (privilege_t) chat->getPeerPrivilege(i)));
    }
    this->group = chat->isGroup();
    this->title = chat->getTitle();
}

MegaChatRoomPrivate::MegaChatRoomPrivate(karere::ChatRoom *chat)
{
    this->id = chat->chatid();
    this->priv = chat->ownPriv();

    this->group = chat->isGroup();
    this->title = chat->titleString();

    if (group)
    {
        karere::GroupChatRoom *groupchat = (GroupChatRoom*) chat;

        karere::GroupChatRoom::MemberMap peers = groupchat->peers();

        karere::GroupChatRoom::MemberMap::iterator it;
        for (it = peers.begin(); it != peers.end(); it++)
        {
            this->peers.push_back(userpriv_pair(it->first,
                                          (privilege_t) it->second->priv()));
        }
    }
}

MegaChatHandle MegaChatRoomPrivate::getHandle() const
{
    return id;
}

int MegaChatRoomPrivate::getOwnPrivilege() const
{
    return priv;
}

int MegaChatRoomPrivate::getPeerPrivilege(MegaChatHandle userhandle) const
{
    for (unsigned int i = 0; i < peers.size(); i++)
    {
        if (peers.at(i).first == userhandle)
        {
            return peers.at(i).second;
        }
    }

    return PRIV_UNKNOWN;
}

int MegaChatRoomPrivate::getPeerPrivilege(unsigned int i) const
{
    return peers.at(i).second;
}

unsigned int MegaChatRoomPrivate::getPeerCount() const
{
    return peers.size();
}

MegaChatHandle MegaChatRoomPrivate::getPeerHandle(unsigned int i) const
{
    return peers.at(i).first;
}

bool MegaChatRoomPrivate::isGroup() const
{
    return group;
}

const char *MegaChatRoomPrivate::getTitle() const
{
    return title.c_str();
}


void MegaChatListItemHandler::onVisibilityChanged(int newVisibility)
{

}

void MegaChatListItemHandler::onTitleChanged(const string &title)
{

}

void MegaChatListItemHandler::onPresenceChanged(karere::Presence state)
{

}

MegaChatPeerListPrivate::MegaChatPeerListPrivate()
{

}

MegaChatPeerListPrivate::~MegaChatPeerListPrivate()
{

}

MegaChatPeerList *MegaChatPeerListPrivate::copy() const
{
    MegaChatPeerListPrivate *ret = new MegaChatPeerListPrivate;

    for (int i = 0; i < size(); i++)
    {
        ret->addPeer(list.at(i).first, list.at(i).second);
    }

    return ret;
}

void MegaChatPeerListPrivate::addPeer(MegaChatHandle h, int priv)
{
    list.push_back(userpriv_pair(h, (privilege_t) priv));
}

MegaChatHandle MegaChatPeerListPrivate::getPeerHandle(int i) const
{
    if (i > size())
    {
        return INVALID_HANDLE;
    }
    else
    {
        return list.at(i).first;
    }
}

int MegaChatPeerListPrivate::getPeerPrivilege(int i) const
{
    if (i > size())
    {
        return PRIV_UNKNOWN;
    }
    else
    {
        return list.at(i).second;
    }
}

int MegaChatPeerListPrivate::size() const
{
    return list.size();
}

const userpriv_vector *MegaChatPeerListPrivate::getList() const
{
    return &list;
}

MegaChatPeerListPrivate::MegaChatPeerListPrivate(userpriv_vector *userpriv)
{
    handle uh;
    privilege_t priv;

    for (unsigned i = 0; i < userpriv->size(); i++)
    {
        uh = userpriv->at(i).first;
        priv = userpriv->at(i).second;

        this->addPeer(uh, priv);
    }
}
