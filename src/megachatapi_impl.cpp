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

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "megachatapi_impl.h"
#include <base/cservices.h>
#include <base/logger.h>
#include <IGui.h>
#include <chatClient.h>
#include <mega/base64.h>

#ifndef _WIN32
#include <signal.h>
#endif

#ifndef KARERE_DISABLE_WEBRTC
namespace rtcModule {void globalCleanup(); }
#endif

using namespace std;
using namespace megachat;
using namespace mega;
using namespace karere;
using namespace chatd;

LoggerHandler *MegaChatApiImpl::loggerHandler = NULL;

MegaChatApiImpl::MegaChatApiImpl(MegaChatApi *chatApi, MegaApi *megaApi)
: sdkMutex(true), videoMutex(true)
{
    init(chatApi, megaApi);
}

MegaChatApiImpl::~MegaChatApiImpl()
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DELETE);
    requestQueue.push(request);
    waiter->notify();
    thread.join();

    // TODO: destruction of waiter hangs forever or may cause crashes
    //delete waiter;

    // TODO: destruction of network layer may cause hangs on MegaApi's network layer.
    // It may terminate the OpenSSL required by cUrl in SDK, so better to skip it.
    //delete websocketsIO;
}

void MegaChatApiImpl::init(MegaChatApi *chatApi, MegaApi *megaApi)
{
    if (!megaPostMessageToGui)
    {
        megaPostMessageToGui = MegaChatApiImpl::megaApiPostMessage;
    }

    this->chatApi = chatApi;
    this->megaApi = megaApi;

    this->mClient = NULL;
    this->terminating = false;
    this->waiter = new MegaChatWaiter();
    this->websocketsIO = new MegaWebsocketsIO(&sdkMutex, waiter, this);

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
    while (true)
    {
        sdkMutex.unlock();

        waiter->init(NEVER);
        waiter->wakeupby(websocketsIO, ::mega::Waiter::NEEDEXEC);
        waiter->wait();

        sdkMutex.lock();

        sendPendingEvents();
        sendPendingRequests();

        if (threadExit)
        {
            // There must be only one pending events, at maximum: the logout marshall call to delete the client
            assert(eventQueue.isEmpty() || (eventQueue.size() == 1));
            sendPendingEvents();

            sdkMutex.unlock();
            break;
        }
    }
}

void MegaChatApiImpl::megaApiPostMessage(void* msg, void* ctx)
{    
    MegaChatApiImpl *megaChatApi = (MegaChatApiImpl *)ctx;
    if (megaChatApi)
    {
        megaChatApi->postMessage(msg);
    }
    else
    {
        // For compatibility with the QT example app,
        // there are some marshallCall() without context
        // that don't need to be marshalled using the
        // intermediate layer
        megaProcessMessage(msg);
    }
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
        nextTag = ++reqtag;
        request->setTag(nextTag);
        requestMap[nextTag]=request;
        errorCode = MegaChatError::ERROR_OK;

        fireOnChatRequestStart(request);

        if (!mClient && request->getType() != MegaChatRequest::TYPE_DELETE)
        {
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_ACCESS);
            API_LOG_WARNING("Chat engine not initialized yet, cannot process the request");
            fireOnChatRequestFinish(request, megaChatError);
            continue;
        }

        if (terminating && request->getType() != MegaChatRequest::TYPE_DELETE)
        {
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_ACCESS);
            API_LOG_WARNING("Chat engine is terminated, cannot process the request");
            fireOnChatRequestFinish(request, megaChatError);
            continue;
        }

        switch (request->getType())
        {
        case MegaChatRequest::TYPE_CONNECT:
        {
            bool isInBackground = request->getFlag();

            mClient->connect(karere::Presence::kInvalid, isInBackground)
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
        case MegaChatRequest::TYPE_DISCONNECT:
        {
            mClient->disconnect();
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS:
        {
            mClient->retryPendingConnections()
                    .then([this, request]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
                    .fail([this, request](const promise::Error& e)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(e.msg(), e.code(), e.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_SEND_TYPING_NOTIF:
        {
            MegaChatHandle chatid = request->getChatHandle();
            ChatRoom *chatroom = findChatRoom(chatid);
            if (chatroom)
            {
                chatroom->sendTypingNotification();
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            }
            else
            {
                errorCode = MegaChatError::ERROR_ARGS;
            }
            break;
        }
        case MegaChatRequest::TYPE_SIGNAL_ACTIVITY:
        {
            mClient->presenced().signalActivity();
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY:
        {
            int64_t timeout = request->getNumber();
            bool enable = request->getFlag();

            mClient->presenced().setAutoaway(enable, timeout);

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_SET_PRESENCE_PERSIST:
        {
            bool enable = request->getFlag();

            mClient->presenced().setPersist(enable);

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_LOGOUT:
        {
            bool deleteDb = request->getFlag();
            terminating = true;
            mClient->terminate(deleteDb);

            API_LOG_INFO("Chat engine is logged out!");
            marshallCall([request, this]() //post destruction asynchronously so that all pending messages get processed before that
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);

                delete mClient;
                mClient = NULL;
                terminating = false;

#ifndef KARERE_DISABLE_WEBRTC
                cleanCallHandlerMap();
#endif
            }, this);

            break;
        }
        case MegaChatRequest::TYPE_DELETE:
        {
            if (mClient && !terminating)
            {
                mClient->terminate();
                API_LOG_INFO("Chat engine closed!");

                delete mClient;
                mClient = NULL;
            }

#ifndef KARERE_DISABLE_WEBRTC
            rtcModule::globalCleanup();
#endif

            threadExit = 1;
            break;
        }
        case MegaChatRequest::TYPE_SET_ONLINE_STATUS:
        {
            int status = request->getNumber();
            if (status < MegaChatApi::STATUS_OFFLINE || status > MegaChatApi::STATUS_BUSY)
            {
                fireOnChatRequestFinish(request, new MegaChatErrorPrivate("Invalid online status", MegaChatError::ERROR_ARGS));
                break;
            }

            std::vector<promise::Promise<void> > promises;
            if (status == MegaChatApi::STATUS_ONLINE)
            {
                // if setting to online, better to use dynamic in order to avoid sticky online that
                // would be kept even when the user goes offline
                promises.push_back(mClient->setPresence(karere::Presence::kClear));
            }

            promises.push_back(mClient->setPresence(request->getNumber()));
            promise::when(promises)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error setting online status: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }

        case MegaChatRequest::TYPE_CREATE_CHATROOM:
        {
            MegaChatPeerList *peersList = request->getMegaChatPeerList();
            if (!peersList || !peersList->size())   // refuse to create chats without participants
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            bool group = request->getFlag();
            const userpriv_vector *userpriv = ((MegaChatPeerListPrivate*)peersList)->getList();
            if (!userpriv)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            if (!group && peersList->size() > 1)
            {
                group = true;
                request->setFlag(group);
                API_LOG_INFO("Forcing group chat due to more than 2 participants");
            }

            if (group)
            {
                vector<std::pair<handle, Priv>> peers;
                for (unsigned int i = 0; i < userpriv->size(); i++)
                {
                    peers.push_back(std::make_pair(userpriv->at(i).first, (Priv) userpriv->at(i).second));
                }

                mClient->createGroupChat(peers)
                .then([request,this](Id chatid)
                {
                    request->setChatHandle(chatid);

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                })
                .fail([request,this](const promise::Error& err)
                {
                    API_LOG_ERROR("Error creating group chat: %s", err.what());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    fireOnChatRequestFinish(request, megaChatError);
                });

            }
            else    // 1on1 chat
            {
                ContactList::iterator it = mClient->contactList->find(peersList->getPeerHandle(0));
                if (it == mClient->contactList->end())
                {
                    // contact not found
                    errorCode = MegaChatError::ERROR_ARGS;
                    break;
                }
                it->second->createChatRoom()
                .then([request,this](ChatRoom* room)
                {
                    request->setChatHandle(room->chatid());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                })
                .fail([request,this](const promise::Error& err)
                {
                    API_LOG_ERROR("Error creating 1on1 chat: %s", err.what());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    fireOnChatRequestFinish(request, megaChatError);
                });
            }
            break;
        }
        case MegaChatRequest::TYPE_INVITE_TO_CHATROOM:
        {
            handle chatid = request->getChatHandle();
            handle uh = request->getUserHandle();
            Priv privilege = (Priv) request->getPrivilege();

            if (chatid == MEGACHAT_INVALID_HANDLE || uh == MEGACHAT_INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            if (!chatroom->isGroup())   // invite only for group chats
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }
            if (chatroom->ownPriv() != (Priv) MegaChatPeerList::PRIV_MODERATOR)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            ((GroupChatRoom *)chatroom)->invite(uh, privilege)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error adding user to group chat: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS:
        {
            handle chatid = request->getChatHandle();
            handle uh = request->getUserHandle();
            Priv privilege = (Priv) request->getPrivilege();

            if (chatid == MEGACHAT_INVALID_HANDLE || uh == MEGACHAT_INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            if (chatroom->ownPriv() != (Priv) MegaChatPeerList::PRIV_MODERATOR)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            ((GroupChatRoom *)chatroom)->setPrivilege(uh, privilege)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error updating peer privileges: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM:
        {
            handle chatid = request->getChatHandle();
            handle uh = request->getUserHandle();

            if (chatid == MEGACHAT_INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            if (!chatroom->isGroup())   // only for group chats can be left
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            if (chatroom->ownPriv() != (Priv) MegaChatPeerList::PRIV_MODERATOR &&
                    uh != MEGACHAT_INVALID_HANDLE)
            {
                    errorCode = MegaChatError::ERROR_ACCESS;
                    break;
            }

            if ( uh == MEGACHAT_INVALID_HANDLE || uh == mClient->myHandle())
            {
                request->setUserHandle(uh);

                ((GroupChatRoom *)chatroom)->leave()
                .then([request, this]()
                {
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                })
                .fail([request, this](const promise::Error& err)
                {
                    API_LOG_ERROR("Error leaving chat: %s", err.what());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    fireOnChatRequestFinish(request, megaChatError);
                });
            }
            else
            {
                ((GroupChatRoom *)chatroom)->excludeMember(uh)
                .then([request, this]()
                {
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                })
                .fail([request, this](const promise::Error& err)
                {
                    API_LOG_ERROR("Error removing peer from chat: %s", err.what());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    fireOnChatRequestFinish(request, megaChatError);
                });
            }
            break;
        }
        case MegaChatRequest::TYPE_TRUNCATE_HISTORY:
        {
            handle chatid = request->getChatHandle();

            if (chatid == MEGACHAT_INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            if (chatroom->ownPriv() != (Priv) MegaChatPeerList::PRIV_MODERATOR)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            handle messageid = request->getUserHandle();
            if (messageid == MEGACHAT_INVALID_HANDLE)   // clear the full history, from current message
            {
                if (chatroom->chat().empty())
                {
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                    break;
                }

                messageid = chatroom->chat().at(chatroom->chat().highnum()).id().val;
            }

            chatroom->truncateHistory(messageid)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error truncating chat history: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_EDIT_CHATROOM_NAME:
        {
            handle chatid = request->getChatHandle();
            const char *title = request->getText();
            if (chatid == MEGACHAT_INVALID_HANDLE || title == NULL)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }
            if (!chatroom->isGroup())   // only for group chats have a title
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            if (chatroom->ownPriv() != (Priv) MegaChatPeerList::PRIV_MODERATOR)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            string strTitle(title);
            strTitle = strTitle.substr(0, 30);
            request->setText(strTitle.c_str()); // update, in case it's been truncated

            ((GroupChatRoom *)chatroom)->setTitle(strTitle)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error editing chat title: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_GET_FIRSTNAME:
        {
            MegaChatHandle uh = request->getUserHandle();

            mClient->userAttrCache().getAttr(uh, MegaApi::USER_ATTR_FIRSTNAME)
            .then([request, this](Buffer *data)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                request->setText(data->buf());
                string firstname = string(data->buf(), data->dataSize());
                request->setText(firstname.c_str());
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error getting user firstname: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_GET_LASTNAME:
        {
            MegaChatHandle uh = request->getUserHandle();

            mClient->userAttrCache().getAttr(uh, MegaApi::USER_ATTR_LASTNAME)
            .then([request, this](Buffer *data)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                string lastname = string(data->buf(), data->dataSize());
                request->setText(lastname.c_str());
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error getting user lastname: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_GET_EMAIL:
        {
            MegaChatHandle uh = request->getUserHandle();

            mClient->userAttrCache().getAttr(uh, karere::USER_ATTR_EMAIL)
            .then([request, this](Buffer *data)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                string email = string(data->buf(), data->dataSize());
                request->setText(email.c_str());
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const promise::Error& err)
            {
                API_LOG_ERROR("Error getting user email: %s", err.what());

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE:
        {
            handle chatid = request->getChatHandle();
            MegaNodeList *nodeList = request->getMegaNodeList();
            handle h = request->getUserHandle();
            if (chatid == MEGACHAT_INVALID_HANDLE ||
                    ((!nodeList || !nodeList->size()) && (h == MEGACHAT_INVALID_HANDLE)))
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            // if only one node, prepare a list with a single element and update request
            MegaNodeList *nodeListAux = NULL;
            if (h != MEGACHAT_INVALID_HANDLE)
            {
                MegaNode *megaNode = megaApi->getNodeByHandle(h);
                if (!megaNode)
                {
                    errorCode = MegaChatError::ERROR_NOENT;
                    break;
                }

                nodeListAux = MegaNodeList::createInstance();
                nodeListAux->addNode(megaNode);
                request->setMegaNodeList(nodeListAux);
                nodeList = request->getMegaNodeList();

                delete megaNode;
                delete nodeListAux;
            }

            const char *buffer = JSonUtils::generateAttachNodeJSon(nodeList);
            if (!buffer)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            promise::Promise<void> promise = chatroom->requesGrantAccessToNodes(nodeList);

            promise::when(promise)
            .then([this, request, buffer]()
            {
                std::string stringToSend(buffer);
                sendAttachNodesMessage(stringToSend, request);
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);

                delete [] buffer;
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([this, request, buffer](const promise::Error& err)
            {
                MegaChatErrorPrivate *megaChatError = NULL;
                if (err.code() == MegaChatError::ERROR_EXIST)
                {
                    API_LOG_WARNING("Already granted access to this node previously");
                    std::string stringToSend(buffer);
                    sendAttachNodesMessage(stringToSend, request);
                    megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                }
                else
                {
                    megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    API_LOG_ERROR("Failed to grant access to some node");
                }

                delete [] buffer;
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE:
        {
            MegaChatHandle chatid = request->getChatHandle();
            MegaNode *node = megaApi->getNodeByHandle(request->getUserHandle());
            if (chatid == MEGACHAT_INVALID_HANDLE || !node)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            promise::Promise<void> promise = chatroom->requestRevokeAccessToNode(node);
            delete node;

            promise::when(promise)
            .then([this, request]()
            {
                ChatRoom *chatroom = findChatRoom(request->getChatHandle());
                char *base64Handle = MegaApi::handleToBase64(request->getUserHandle());
                std::string stringToSend = std::string(base64Handle);
                delete base64Handle;
                stringToSend.insert(stringToSend.begin(), Message::kMsgRevokeAttachment);
                stringToSend.insert(stringToSend.begin(), 0x0);
                Message *m = chatroom->chat().msgSubmit(stringToSend.c_str(), stringToSend.length(), Message::kMsgRevokeAttachment, NULL);
                MegaChatMessage* megaMsg = new MegaChatMessagePrivate(*m, Message::Status::kSending, CHATD_IDX_INVALID);
                request->setMegaChatMessage(megaMsg);
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);

            })
            .fail([this, request](const promise::Error& err)
            {
                API_LOG_ERROR("Failed to revoke access to attached node (%d)", request->getUserHandle());
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
            break;
        }
        case MegaChatRequest::TYPE_SET_BACKGROUND_STATUS:
        {
            bool background = request->getFlag();
            if (background)
            {
                mClient->notifyUserIdle();
            }
            else
            {
                mClient->notifyUserActive();
            }
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
#ifndef KARERE_DISABLE_WEBRTC
        case MegaChatRequest::TYPE_START_CHAT_CALL:
        {
            if (!mClient->rtc)
            {
                API_LOG_ERROR("Start call - WebRTC is not initialized");
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            MegaChatHandle chatid = request->getChatHandle();
            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            if (findChatCallHandler(chatid))
            {
                API_LOG_ERROR("Start call - One call already exists for speficied chat id");
                errorCode = MegaChatError::ERROR_EXIST;
                break;
            }

            bool enableVideo = request->getFlag();
            karere::AvFlags avFlags(true, enableVideo);

            MegaChatCallHandler *handler = new MegaChatCallHandler(this);
            chatroom->mediaCall(avFlags, *handler);
            callHandlers[chatid] = handler;
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_ANSWER_CHAT_CALL:
        {
            MegaChatHandle chatid = request->getChatHandle();
            bool enableVideo = request->getFlag();

            MegaChatCallHandler *handler = findChatCallHandler(chatid);
            if (!handler)
            {
                API_LOG_ERROR("Answer call - Failed to get the call handler associated to chat room");
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            rtcModule::ICall *call = handler->getCall();
            if (!call)
            {
                API_LOG_ERROR("Answer call - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
                errorCode = MegaChatError::ERROR_NOENT;
                assert(false);
                break;
            }

            karere::AvFlags avFlags(true, enableVideo); // audio is always enabled by default
            call->answer(avFlags);

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_HANG_CHAT_CALL:
        {
            if (!mClient->rtc)
            {
                API_LOG_ERROR("Hang up call - WebRTC is not initialized");
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            MegaChatHandle chatid = request->getChatHandle();
            if (chatid != MEGACHAT_INVALID_HANDLE)
            {
                MegaChatCallHandler *handler = findChatCallHandler(chatid);
                if (!handler)
                {
                    API_LOG_ERROR("Hang up call - Failed to get the call handler associated to chat room");
                    errorCode = MegaChatError::ERROR_NOENT;
                    break;
                }

                rtcModule::ICall *call = handler->getCall();
                if (!call)
                {
                    API_LOG_ERROR("Hang up call - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
                    errorCode = MegaChatError::ERROR_NOENT;
                    assert(false);
                    break;
                }

                call->hangup();
            }
            else    // hang all calls (no specific chatid)
            {
                mClient->rtc->hangupAll(rtcModule::TermCode::kInvalid);
            }

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL:
        {
            MegaChatHandle chatid = request->getChatHandle();
            bool enable = request->getFlag();
            int operationType = request->getParamType();

            MegaChatCallHandler *handler = findChatCallHandler(chatid);
            if (!handler)
            {
                API_LOG_ERROR("Disable AV flags - Failed to get the call handler associated to chat room");
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            MegaChatCallPrivate *chatCall = handler->getMegaChatCall();
            rtcModule::ICall *call = handler->getCall();

            if (!chatCall || !call)
            {
                API_LOG_ERROR("Disable AV flags - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
                errorCode = MegaChatError::ERROR_NOENT;
                assert(false);
                break;
            }

            karere::AvFlags currentFlags = call->sentAv();
            karere::AvFlags newFlags;
            if (operationType == MegaChatRequest::AUDIO)
            {
                karere::AvFlags flags(enable, currentFlags.video());
                newFlags = flags;
            }
            else if (operationType == MegaChatRequest::VIDEO)
            {
                karere::AvFlags flags(currentFlags.audio(), enable);
                newFlags = flags;
            }
            else
            {
                API_LOG_ERROR("Invalid flags to enable/disable audio/video stream");
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            karere::AvFlags avFlags = call->muteUnmute(newFlags);

            chatCall->setLocalAudioVideoFlags(avFlags);
            API_LOG_INFO("Local audio/video flags changed. ChatId: %s, callid: %s, AV: %s --> %s",
                         call->chat().chatId().toString().c_str(),
                         call->id().toString().c_str(),
                         currentFlags.toString().c_str(),
                         avFlags.toString().c_str());

            fireOnChatCallUpdate(chatCall);
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES:
        {
            if (!mClient->rtc)
            {
                API_LOG_ERROR("Load AV devices - WebRTC is not initialized");
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            mClient->rtc->loadDeviceList();
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
#endif
        default:
        {
            errorCode = MegaChatError::ERROR_UNKNOWN;
        }
        }   // end of switch(request->getType())


        if(errorCode)
        {
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(errorCode);
            API_LOG_WARNING("Error starting request: %s", megaChatError->getErrorString());
            fireOnChatRequestFinish(request, megaChatError);
        }
    }
}

void MegaChatApiImpl::sendPendingEvents()
{
    void *msg;
    while ((msg = eventQueue.pop()))
    {
        megaProcessMessage(msg);
    }
}

void MegaChatApiImpl::setLogLevel(int logLevel)
{
    if (!loggerHandler)
    {
        loggerHandler = new LoggerHandler();
    }
    loggerHandler->setLogLevel(logLevel);
}

void MegaChatApiImpl::setLogWithColors(bool useColors)
{
    if (loggerHandler)
    {
        loggerHandler->setLogWithColors(useColors);
    }
}

void MegaChatApiImpl::setLogToConsole(bool enable)
{
    if (loggerHandler)
    {
        loggerHandler->setLogToConsole(enable);
    }
}

void MegaChatApiImpl::setLoggerClass(MegaChatLogger *megaLogger)
{
    if (!megaLogger)   // removing logger
    {
        delete loggerHandler;
        loggerHandler = NULL;
    }
    else
    {
        if (!loggerHandler)
        {
            loggerHandler = new LoggerHandler();
        }
        loggerHandler->setMegaChatLogger(megaLogger);
    }
}

int MegaChatApiImpl::init(const char *sid)
{
    sdkMutex.lock();
    if (!mClient)
    {
#ifndef KARERE_DISABLE_WEBRTC
        uint8_t caps = karere::kClientIsMobile | karere::kClientCanWebrtc;
#else
        uint8_t caps = karere::kClientIsMobile;
#endif
        mClient = new karere::Client(*this->megaApi, websocketsIO, *this, this->megaApi->getBasePath(), caps, this);
        terminating = false;
    }

    int state = mClient->init(sid);
    if (state != karere::Client::kInitErrNoCache &&
            state != karere::Client::kInitWaitingNewSession &&
            state != karere::Client::kInitHasOfflineSession)
    {
        // there's been an error during initialization
        localLogout();
    }

    sdkMutex.unlock();

    return MegaChatApiImpl::convertInitState(state);
}

int MegaChatApiImpl::getInitState()
{
    int initState;

    sdkMutex.lock();
    if (mClient)
    {
        initState = MegaChatApiImpl::convertInitState(mClient->initState());
    }
    else
    {
        initState = MegaChatApi::INIT_NOT_DONE;
    }
    sdkMutex.unlock();

    return initState;
}

MegaChatRoomHandler *MegaChatApiImpl::getChatRoomHandler(MegaChatHandle chatid)
{
    map<MegaChatHandle, MegaChatRoomHandler*>::iterator it = chatRoomHandler.find(chatid);
    if (it == chatRoomHandler.end())
    {
        chatRoomHandler[chatid] = new MegaChatRoomHandler(this, chatid);
    }

    return chatRoomHandler[chatid];
}

void MegaChatApiImpl::removeChatRoomHandler(MegaChatHandle chatid)
{
    chatRoomHandler.erase(chatid);
}

ChatRoom *MegaChatApiImpl::findChatRoom(MegaChatHandle chatid)
{
    ChatRoom *chatroom = NULL;

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it = mClient->chats->find(chatid);
        if (it != mClient->chats->end())
        {
            chatroom = it->second;
        }
    }

    sdkMutex.unlock();

    return chatroom;
}

karere::ChatRoom *MegaChatApiImpl::findChatRoomByUser(MegaChatHandle userhandle)
{
    ChatRoom *chatroom = NULL;

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ContactList::iterator it = mClient->contactList->find(userhandle);
        if (it != mClient->contactList->end())
        {
            chatroom = it->second->chatRoom();
        }
    }

    sdkMutex.unlock();

    return chatroom;
}

chatd::Message *MegaChatApiImpl::findMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    Message *msg = NULL;

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Idx index = chat.msgIndexFromId(msgid);
        if (index != CHATD_IDX_INVALID)
        {
            msg = chat.findOrNull(index);
        }
    }

    sdkMutex.unlock();

    return msg;
}

chatd::Message *MegaChatApiImpl::findMessageNotConfirmed(MegaChatHandle chatid, MegaChatHandle msgxid)
{
    Message *msg = NULL;

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        msg = chat.getMsgByXid(msgxid);
    }

    sdkMutex.unlock();

    return msg;
}

void MegaChatApiImpl::setCatchException(bool enable)
{
    karere::gCatchException = enable;
}

void MegaChatApiImpl::fireOnChatRequestStart(MegaChatRequestPrivate *request)
{
    API_LOG_INFO("Request (%s) starting", request->getRequestString());

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
        API_LOG_INFO("Request (%s) finished with error: %s", request->getRequestString(), e->getErrorString());
    }
    else
    {
        API_LOG_INFO("Request (%s) finished", request->getRequestString());
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

#ifndef KARERE_DISABLE_WEBRTC

void MegaChatApiImpl::fireOnChatCallUpdate(MegaChatCallPrivate *call)
{
    for (set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallUpdate(chatApi, call);
    }

    call->removeChanges();
}

void MegaChatApiImpl::fireOnChatRemoteVideoData(MegaChatHandle chatid, int width, int height, char *buffer)
{
    for(set<MegaChatVideoListener *>::iterator it = remoteVideoListeners.begin(); it != remoteVideoListeners.end() ; it++)
    {
        (*it)->onChatVideoData(chatApi, chatid, width, height, buffer, width * height * 4);
    }
}

void MegaChatApiImpl::fireOnChatLocalVideoData(MegaChatHandle chatid, int width, int height, char *buffer)
{
    for(set<MegaChatVideoListener *>::iterator it = localVideoListeners.begin(); it != localVideoListeners.end() ; it++)
    {
        (*it)->onChatVideoData(chatApi, chatid, width, height, buffer, width * height * 4);
    }
}

#endif  // webrtc

void MegaChatApiImpl::fireOnChatListItemUpdate(MegaChatListItem *item)
{
    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatListItemUpdate(chatApi, item);
    }

    delete item;
}

void MegaChatApiImpl::fireOnChatInitStateUpdate(int newState)
{
    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatInitStateUpdate(chatApi, newState);
    }
}

void MegaChatApiImpl::fireOnChatOnlineStatusUpdate(MegaChatHandle userhandle, int status, bool inProgress)
{
    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatOnlineStatusUpdate(chatApi, userhandle, status, inProgress);
    }
}

void MegaChatApiImpl::fireOnChatPresenceConfigUpdate(MegaChatPresenceConfig *config)
{
    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatPresenceConfigUpdate(chatApi, config);
    }

    delete config;
}

void MegaChatApiImpl::fireOnChatConnectionStateUpdate(MegaChatHandle chatid, int newState)
{
    // check if connected to all chats (and logged in)
    bool allConnected = false;
    if (newState == MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        allConnected = true;
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            if (it->second->isActive() && it->second->chat().onlineState() != chatd::kChatStateOnline)
            {
                allConnected = false;
                break;
            }
        }
    }

    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatConnectionStateUpdate(chatApi, chatid, newState);

        if (allConnected)
        {
            (*it)->onChatConnectionStateUpdate(chatApi, MEGACHAT_INVALID_HANDLE, newState);
        }
    }
}

void MegaChatApiImpl::connect(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_CONNECT, listener);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::connectInBackground(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_CONNECT, listener);
    request->setFlag(true);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::disconnect(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DISCONNECT, listener);
    requestQueue.push(request);
    waiter->notify();
}

int MegaChatApiImpl::getConnectionState()
{
    int ret = 0;

    sdkMutex.lock();
    ret = mClient ? (int) mClient->connState() : MegaChatApi::DISCONNECTED;
    sdkMutex.unlock();

    return ret;
}

int MegaChatApiImpl::getChatConnectionState(MegaChatHandle chatid)
{
    int ret = MegaChatApi::CHAT_CONNECTION_OFFLINE;

    sdkMutex.lock();
    ChatRoom *room = findChatRoom(chatid);
    if (room)
    {
        ret = MegaChatApiImpl::convertChatConnectionState(room->chatdOnlineState());
    }
    sdkMutex.unlock();

    return ret;
}

void MegaChatApiImpl::retryPendingConnections(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS, listener);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::logout(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_LOGOUT, listener);
    request->setFlag(true);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::localLogout(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_LOGOUT, listener);
    request->setFlag(false);
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

void MegaChatApiImpl::setPresenceAutoaway(bool enable, int64_t timeout, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY, listener);
    request->setFlag(enable);
    request->setNumber(timeout);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::setPresencePersist(bool enable, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SET_PRESENCE_PERSIST, listener);
    request->setFlag(enable);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::signalPresenceActivity(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SIGNAL_ACTIVITY, listener);
    requestQueue.push(request);
    waiter->notify();
}

MegaChatPresenceConfig *MegaChatApiImpl::getPresenceConfig()
{
    MegaChatPresenceConfigPrivate *config = NULL;

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        const ::presenced::Config &cfg = mClient->presenced().config();
        if (cfg.presence().isValid())
        {
            config = new MegaChatPresenceConfigPrivate(cfg, mClient->presenced().isConfigAcknowledged());
        }
    }

    sdkMutex.unlock();

    return config;
}

bool MegaChatApiImpl::isSignalActivityRequired()
{
    sdkMutex.lock();

    bool enabled = mClient ? mClient->presenced().autoAwayInEffect() : false;

    sdkMutex.unlock();

    return enabled;
}

int MegaChatApiImpl::getOnlineStatus()
{
    sdkMutex.lock();

    int status = mClient ? mClient->ownPresence().status() : MegaChatApi::STATUS_INVALID;

    sdkMutex.unlock();

    return status;
}

bool MegaChatApiImpl::isOnlineStatusPending()
{
    sdkMutex.lock();

    bool statusInProgress = mClient ? mClient->presenced().isConfigAcknowledged() : false;

    sdkMutex.unlock();

    return statusInProgress;
}

int MegaChatApiImpl::getUserOnlineStatus(MegaChatHandle userhandle)
{
    int status = MegaChatApi::STATUS_INVALID;

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ContactList::iterator it = mClient->contactList->find(userhandle);
        if (it != mClient->contactList->end())
        {
            status = it->second->presence().status();
        }
        else if (userhandle == mClient->myHandle())
        {
            status = getOnlineStatus();
        }
        else
        {
            for (auto it = mClient->chats->begin(); it != mClient->chats->end(); it++)
            {
                if (!it->second->isGroup())
                    continue;

                GroupChatRoom *chat = (GroupChatRoom*) it->second;
                const GroupChatRoom::MemberMap &membersMap = chat->peers();
                GroupChatRoom::MemberMap::const_iterator itMembers = membersMap.find(userhandle);
                if (itMembers != membersMap.end())
                {
                    status = itMembers->second->presence().status();
                    sdkMutex.unlock();
                    return status;
                }
            }
        }
    }

    sdkMutex.unlock();

    return status;
}

void MegaChatApiImpl::setBackgroundStatus(bool background, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SET_BACKGROUND_STATUS, listener);
    request->setFlag(background);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::getUserFirstname(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_GET_FIRSTNAME, listener);
    request->setUserHandle(userhandle);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::getUserLastname(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_GET_LASTNAME, listener);
    request->setUserHandle(userhandle);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::getUserEmail(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_GET_EMAIL, listener);
    request->setUserHandle(userhandle);
    requestQueue.push(request);
    waiter->notify();
}

char *MegaChatApiImpl::getContactEmail(MegaChatHandle userhandle)
{
    char *ret = NULL;

    sdkMutex.lock();

    const std::string *email = mClient ? mClient->contactList->getUserEmail(userhandle) : NULL;
    if (email)
    {
        ret = MegaApi::strdup(email->c_str());
    }

    sdkMutex.unlock();

    return ret;
}

MegaChatHandle MegaChatApiImpl::getUserHandleByEmail(const char *email)
{
    MegaChatHandle uh = MEGACHAT_INVALID_HANDLE;

    if (email)
    {
        sdkMutex.lock();

        Contact *contact = mClient ? mClient->contactList->contactFromEmail(email) : NULL;
        if (contact)
        {
            uh = contact->userId();
        }

        sdkMutex.unlock();
    }

    return uh;
}

MegaChatHandle MegaChatApiImpl::getMyUserHandle()
{
    return mClient ? (MegaChatHandle) mClient->myHandle() : MEGACHAT_INVALID_HANDLE;
}

char *MegaChatApiImpl::getMyFirstname()
{
    if (!mClient)
    {
        return NULL;
    }

    return MegaChatRoomPrivate::firstnameFromBuffer(mClient->myName());
}

char *MegaChatApiImpl::getMyLastname()
{
    if (!mClient)
    {
        return NULL;
    }

    return MegaChatRoomPrivate::lastnameFromBuffer(mClient->myName());
}

char *MegaChatApiImpl::getMyFullname()
{
    if (!mClient)
    {
        return NULL;
    }

    return MegaApi::strdup(mClient->myName().substr(1).c_str());
}

char *MegaChatApiImpl::getMyEmail()
{
    if (!mClient)
    {
        return NULL;
    }

    return MegaApi::strdup(mClient->myEmail().c_str());
}

MegaChatRoomList *MegaChatApiImpl::getChatRooms()
{
    MegaChatRoomListPrivate *chats = new MegaChatRoomListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            chats->addChatRoom(new MegaChatRoomPrivate(*it->second));
        }
    }

    sdkMutex.unlock();

    return chats;
}

MegaChatRoom *MegaChatApiImpl::getChatRoom(MegaChatHandle chatid)
{
    MegaChatRoomPrivate *chat = NULL;

    sdkMutex.lock();

    ChatRoom *chatRoom = findChatRoom(chatid);
    if (chatRoom)
    {
        chat = new MegaChatRoomPrivate(*chatRoom);
    }

    sdkMutex.unlock();

    return chat;
}

MegaChatRoom *MegaChatApiImpl::getChatRoomByUser(MegaChatHandle userhandle)
{
    MegaChatRoomPrivate *chat = NULL;

    sdkMutex.lock();

    ChatRoom *chatRoom = findChatRoomByUser(userhandle);
    if (chatRoom)
    {
        chat = new MegaChatRoomPrivate(*chatRoom);
    }

    sdkMutex.unlock();

    return chat;
}

MegaChatListItemList *MegaChatApiImpl::getChatListItems()
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            items->addChatListItem(new MegaChatListItemPrivate(*it->second));
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatListItem *MegaChatApiImpl::getChatListItem(MegaChatHandle chatid)
{
    MegaChatListItemPrivate *item = NULL;

    sdkMutex.lock();

    ChatRoom *chatRoom = findChatRoom(chatid);
    if (chatRoom)
    {
        item = new MegaChatListItemPrivate(*chatRoom);
    }

    sdkMutex.unlock();

    return item;
}

int MegaChatApiImpl::getUnreadChats()
{
    int count = 0;

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            ChatRoom *room = it->second;
            if (room->isActive() && room->chat().unreadMsgCount())
            {
                count++;
            }
        }
    }

    sdkMutex.unlock();

    return count;
}

MegaChatListItemList *MegaChatApiImpl::getActiveChatListItems()
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            if (it->second->isActive())
            {
                items->addChatListItem(new MegaChatListItemPrivate(*it->second));
            }
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatListItemList *MegaChatApiImpl::getInactiveChatListItems()
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            if (!it->second->isActive())
            {
                items->addChatListItem(new MegaChatListItemPrivate(*it->second));
            }
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatListItemList *MegaChatApiImpl::getUnreadChatListItems()
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            ChatRoom *room = it->second;
            if (room->isActive() && room->chat().unreadMsgCount())
            {
                items->addChatListItem(new MegaChatListItemPrivate(*it->second));
            }
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatHandle MegaChatApiImpl::getChatHandleByUser(MegaChatHandle userhandle)
{
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;

    sdkMutex.lock();

    ChatRoom *chatRoom = findChatRoomByUser(userhandle);
    if (chatRoom)
    {
        chatid = chatRoom->chatid();
    }

    sdkMutex.unlock();

    return chatid;
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

void MegaChatApiImpl::removeFromChat(MegaChatHandle chatid, MegaChatHandle uh, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(uh);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::updateChatPermissions(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(uh);
    request->setPrivilege(privilege);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::truncateChat(MegaChatHandle chatid, MegaChatHandle messageid, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_TRUNCATE_HISTORY, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(messageid);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::setChatTitle(MegaChatHandle chatid, const char *title, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_EDIT_CHATROOM_NAME, listener);
    request->setChatHandle(chatid);
    request->setText(title);
    requestQueue.push(request);
    waiter->notify();
}

bool MegaChatApiImpl::openChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener)
{    
    if (!listener)
    {
        return false;
    }

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        MegaChatRoomHandler *roomHandler = getChatRoomHandler(chatid);
        roomHandler->addChatRoomListener(listener);
        addChatRoomListener(chatid, listener);
        chatroom->setAppChatHandler(roomHandler);
    }

    sdkMutex.unlock();
    return chatroom;
}

void MegaChatApiImpl::closeChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        chatroom->removeAppChatHandler();

        MegaChatRoomHandler *roomHandler = getChatRoomHandler(chatid);
        roomHandler->removeChatRoomListener(listener);
        removeChatRoomHandler(chatid);
    }

    sdkMutex.unlock();
}

int MegaChatApiImpl::loadMessages(MegaChatHandle chatid, int count)
{
    int ret = MegaChatApi::SOURCE_NONE;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        HistSource source = chat.getHistory(count);
        switch (source)
        {
        case kHistSourceNone:   ret = MegaChatApi::SOURCE_NONE; break;
        case kHistSourceRam:
        case kHistSourceDb:     ret = MegaChatApi::SOURCE_LOCAL; break;
        case kHistSourceServer: ret = MegaChatApi::SOURCE_REMOTE; break;
        case kHistSourceServerOffline: ret = MegaChatApi::SOURCE_ERROR; break;
        default:
            API_LOG_ERROR("Unknown source of messages at loadMessages()");
            break;
        }
    }

    sdkMutex.unlock();
    return ret;
}

bool MegaChatApiImpl::isFullHistoryLoaded(MegaChatHandle chatid)
{
    bool ret = false;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        ret = chat.haveAllHistoryNotified();
    }

    sdkMutex.unlock();
    return ret;
}

MegaChatMessage *MegaChatApiImpl::getMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Idx index = chat.msgIndexFromId(msgid);
        if (index != CHATD_IDX_INVALID)     // only confirmed messages have index
        {
            Message *msg = chat.findOrNull(index);
            if (msg)
            {
                megaMsg = new MegaChatMessagePrivate(*msg, chat.getMsgStatus(*msg, index), index);
            }
            else
            {
                API_LOG_ERROR("Failed to find message by index, being index retrieved from message id (index: %d, id: %d)", index, msgid);
            }
        }
        else    // message still not confirmed, search in sending-queue
        {
            Message *msg = chat.getMsgByXid(msgid);
            if (msg)
            {
                megaMsg = new MegaChatMessagePrivate(*msg, Message::Status::kSending, MEGACHAT_INVALID_INDEX);
            }
            else
            {
                API_LOG_ERROR("Failed to find message by temporal id (id: %d)", msgid);
            }
        }
    }
    else
    {
        API_LOG_ERROR("Chatroom not found (chatid: %d)", chatid);
    }

    sdkMutex.unlock();
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid)
{

    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        chatd::ManualSendReason reason;
        chatd::Message *msg = chat.getManualSending(rowid, reason);
        if (msg)
        {
            megaMsg = new MegaChatMessagePrivate(*msg, chatd::Message::kSendingManual, MEGACHAT_INVALID_INDEX);
            delete msg;

            megaMsg->setStatus(MegaChatMessage::STATUS_SENDING_MANUAL);
            megaMsg->setRowId(rowid);
            megaMsg->setCode(reason);
        }
        else
        {
            API_LOG_ERROR("Message not found (rowid: %d)", rowid);
        }
    }
    else
    {
        API_LOG_ERROR("Chatroom not found (chatid: %d)", chatid);
    }

    sdkMutex.unlock();
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::sendMessage(MegaChatHandle chatid, const char *msg)
{
    if (!msg)
    {
        return NULL;
    }

    size_t msgLen = strlen(msg);
    while (msgLen)
    {
        if (msg[msgLen-1] == '\n' || msg[msgLen-1] == '\r')
        {
            msgLen--;
        }
        else
        {
            break;
        }
    }
    if (!msgLen)
    {
        return NULL;
    }

    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        unsigned char t = MegaChatMessage::TYPE_NORMAL;
        Message *m = chatroom->chat().msgSubmit(msg, msgLen, t, NULL);

        megaMsg = new MegaChatMessagePrivate(*m, Message::Status::kSending, CHATD_IDX_INVALID);
    }

    sdkMutex.unlock();
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::attachContacts(MegaChatHandle chatid, MegaHandleList *handles)
{
    if (!mClient || chatid == MEGACHAT_INVALID_HANDLE || handles == NULL || handles->size() == 0)
    {
        return NULL;
    }

    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        bool error = false;
        rapidjson::Document jSonDocument(rapidjson::kArrayType);
        for (unsigned int i = 0; i < handles->size(); ++i)
        {
            auto contactIterator = mClient->contactList->find(handles->get(i));
            if (contactIterator != mClient->contactList->end())
            {
                karere::Contact* contact = contactIterator->second;

                rapidjson::Value jSonContact(rapidjson::kObjectType);
                const char *base64Handle = MegaApi::userHandleToBase64(contact->userId());
                std::string handleString(base64Handle);
                rapidjson::Value userHandleValue(rapidjson::kStringType);
                userHandleValue.SetString(handleString.c_str(), handleString.length(), jSonDocument.GetAllocator());
                jSonContact.AddMember(rapidjson::Value("u"), userHandleValue, jSonDocument.GetAllocator());
                delete [] base64Handle;

                rapidjson::Value emailValue(rapidjson::kStringType);
                emailValue.SetString(contact->email().c_str(), contact->email().length(), jSonDocument.GetAllocator());
                jSonContact.AddMember(rapidjson::Value("email"), emailValue, jSonDocument.GetAllocator());

                std::string nameString = contact->titleString();
                nameString.erase(0, 1);
                rapidjson::Value nameValue(rapidjson::kStringType);
                nameValue.SetString(nameString.c_str(), nameString.length(), jSonDocument.GetAllocator());
                jSonContact.AddMember(rapidjson::Value("name"), nameValue, jSonDocument.GetAllocator());

                jSonDocument.PushBack(jSonContact, jSonDocument.GetAllocator());
            }
            else
            {
                error = true;
                API_LOG_ERROR("Failed to find the contact: %d", handles->get(i));
                break;
            }
        }

        if (!error)
        {
            unsigned char zero = 0x0;
            unsigned char contactType = Message::kMsgContact;
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            jSonDocument.Accept(writer);
            std::string stringToSend(buffer.GetString());
            stringToSend.insert(stringToSend.begin(), contactType);
            stringToSend.insert(stringToSend.begin(), zero);
            Message *m = chatroom->chat().msgSubmit(stringToSend.c_str(), stringToSend.length(), Message::kMsgContact, NULL);
            megaMsg = new MegaChatMessagePrivate(*m, Message::Status::kSending, CHATD_IDX_INVALID);
        }
    }

    sdkMutex.unlock();
    return megaMsg;
}

void MegaChatApiImpl::attachNodes(MegaChatHandle chatid, MegaNodeList *nodes, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE, listener);
    request->setChatHandle(chatid);
    request->setMegaNodeList(nodes);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(nodehandle);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::revokeAttachment(MegaChatHandle chatid, MegaChatHandle handle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(handle);
    requestQueue.push(request);
    waiter->notify();
}

bool MegaChatApiImpl::isRevoked(MegaChatHandle chatid, MegaChatHandle nodeHandle)
{
    bool ret = false;

    sdkMutex.lock();

    auto it = chatRoomHandler.find(chatid);
    if (it != chatRoomHandler.end())
    {
        ret = it->second->isRevoked(nodeHandle);
    }

    sdkMutex.unlock();

    return ret;
}

MegaChatMessage *MegaChatApiImpl::editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char *msg)
{
    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Message *originalMsg = findMessage(chatid, msgid);
        Idx index;
        if (originalMsg)
        {
            index = chat.msgIndexFromId(msgid);
        }
        else   // message may not have an index yet (not confirmed)
        {
            index = MEGACHAT_INVALID_INDEX;
            originalMsg = findMessageNotConfirmed(chatid, msgid);   // find by transactional id
        }

        if (originalMsg)
        {
            size_t msgLen = msg ? strlen(msg) : 0;
            if (msg)    // actually not deletion, but edit
            {
                while (msgLen)
                {
                    if (msg[msgLen-1] == '\n' || msg[msgLen-1] == '\r')
                    {
                        msgLen--;
                    }
                    else
                    {
                        break;
                    }
                }
                if (!msgLen)
                {
                    sdkMutex.unlock();
                    return NULL;
                }
            }

            const Message *editedMsg = chatroom->chat().msgModify(*originalMsg, msg, msgLen, NULL);
            if (editedMsg)
            {
                megaMsg = new MegaChatMessagePrivate(*editedMsg, Message::kSending, index);
            }
        }
    }

    sdkMutex.unlock();
    return megaMsg;
}

bool MegaChatApiImpl::setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid)
{
    bool ret = false;

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        ret = chatroom->chat().setMessageSeen((Id) msgid);
    }    

    sdkMutex.unlock();

    return ret;
}

MegaChatMessage *MegaChatApiImpl::getLastMessageSeen(MegaChatHandle chatid)
{
    MegaChatMessagePrivate *megaMsg = NULL;

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Idx index = chat.lastSeenIdx();
        if (index != CHATD_IDX_INVALID)
        {
            const Message *msg = chat.findOrNull(index);
            if (msg)
            {
                Message::Status status = chat.getMsgStatus(*msg, index);
                megaMsg = new MegaChatMessagePrivate(*msg, status, index);
            }
        }
    }

    sdkMutex.unlock();

    return megaMsg;
}

void MegaChatApiImpl::removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowid)
{
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        chat.removeManualSend(rowid);
    }

    sdkMutex.unlock();
}

void MegaChatApiImpl::sendTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SEND_TYPING_NOTIF, listener);
    request->setChatHandle(chatid);
    requestQueue.push(request);
    waiter->notify();
}

bool MegaChatApiImpl::isMessageReceptionConfirmationActive() const
{
    return mClient ? mClient->chatd->isMessageReceivedConfirmationActive() : false;
}

void MegaChatApiImpl::saveCurrentState()
{
    sdkMutex.lock();

    if (mClient && !terminating)
    {
        mClient->saveDb();
    }

    sdkMutex.unlock();
}

#ifndef KARERE_DISABLE_WEBRTC

MegaStringList *MegaChatApiImpl::getChatAudioInDevices()
{
    std::vector<std::string> devicesVector;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        mClient->rtc->getAudioInDevices(devicesVector);
    }
    else
    {
        API_LOG_ERROR("Failed to get audio-in devices");
    }
    sdkMutex.unlock();

    MegaStringList *devices = getChatInDevices(devicesVector);

    return devices;

}

MegaStringList *MegaChatApiImpl::getChatVideoInDevices()
{
    std::vector<std::string> devicesVector;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        mClient->rtc->getVideoInDevices(devicesVector);
    }
    else
    {
        API_LOG_ERROR("Failed to get video-in devices");
    }
    sdkMutex.unlock();

    MegaStringList *devices = getChatInDevices(devicesVector);

    return devices;
}

bool MegaChatApiImpl::setChatAudioInDevice(const char *device)
{
    bool returnedValue = false;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        returnedValue = mClient->rtc->selectAudioInDevice(device);
    }
    else
    {
        API_LOG_ERROR("Failed to set audio-in devices");
    }
    sdkMutex.unlock();

    return returnedValue;
}

bool MegaChatApiImpl::setChatVideoInDevice(const char *device)
{
    bool returnedValue = false;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        returnedValue = mClient->rtc->selectVideoInDevice(device);
    }
    else
    {
        API_LOG_ERROR("Failed to set video-in devices");
    }
    sdkMutex.unlock();

    return returnedValue;
}

void MegaChatApiImpl::startChatCall(MegaChatHandle chatid, bool enableVideo, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_START_CHAT_CALL, listener);
    request->setChatHandle(chatid);
    request->setFlag(enableVideo);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::answerChatCall(MegaChatHandle chatid, bool enableVideo, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ANSWER_CHAT_CALL, listener);
    request->setChatHandle(chatid);
    request->setFlag(enableVideo);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::hangChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_HANG_CHAT_CALL, listener);
    request->setChatHandle(chatid);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::hangAllChatCalls(MegaChatRequestListener *listener = NULL)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_HANG_CHAT_CALL, listener);
    request->setChatHandle(MEGACHAT_INVALID_HANDLE);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::setAudioEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL, listener);
    request->setChatHandle(chatid);
    request->setFlag(enable);
    request->setParamType(MegaChatRequest::AUDIO);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::setVideoEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL, listener);
    request->setChatHandle(chatid);
    request->setFlag(enable);
    request->setParamType(MegaChatRequest::VIDEO);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::loadAudioVideoDeviceList(MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES, listener);
    requestQueue.push(request);
    waiter->notify();
}

MegaChatCall *MegaChatApiImpl::getChatCall(MegaChatHandle chatId)
{
    MegaChatCall *chatCall = NULL;

    sdkMutex.lock();
    std::map<MegaChatHandle, MegaChatCallHandler*>::iterator it = callHandlers.find(chatId);

    if (it != callHandlers.end())
    {
        if (it->second != NULL)
        {
            chatCall = it->second->getMegaChatCall();
            if (!chatCall)
            {
                API_LOG_ERROR("MegaChatApiImpl::getChatCall - Invalid MegaChatCall at MegaChatCallHandler");
                assert(false);
            }
            else
            {
                chatCall = chatCall->copy();
            }
        }
        else
        {
            API_LOG_ERROR("MegaChatApiImpl::getChatCallByChatId - Invalid MegaChatCallHandler at callHandlers");
            assert(false);
        }
    }

    sdkMutex.unlock();
    return chatCall;
}

MegaChatCall *MegaChatApiImpl::getChatCallByCallId(MegaChatHandle callId)
{
    MegaChatCall *chatCall = NULL;

    sdkMutex.lock();
    std::map<MegaChatHandle, MegaChatCallHandler*>::iterator it;
    for (it = callHandlers.begin(); it != callHandlers.end(); ++it)
    {
        if (it->second != NULL)
        {
            MegaChatCall *call = it->second->getMegaChatCall();
            if (call != NULL)
            {
                if (callId == call->getId())
                {
                    chatCall = call->copy();
                    break;
                }
            }
            else
            {
                API_LOG_ERROR("MegaChatApiImpl::getChatCall - Invalid MegaChatCall at MegaChatCallHandler");
                assert(false);
            }
        }
        else
        {
            API_LOG_ERROR("MegaChatApiImpl::getChatCall - Invalid MegaChatCallHandler at callHandlers");
            assert(false);
        }
    }

    sdkMutex.unlock();
    return chatCall;
}

int MegaChatApiImpl::getNumCalls()
{
    int callsNumber = 0;
    sdkMutex.lock();
    callsNumber = callHandlers.size();
    sdkMutex.unlock();

    return callsNumber;
}

MegaHandleList *MegaChatApiImpl::getChatCalls()
{
    MegaHandleListPrivate *callList = new MegaHandleListPrivate();

    sdkMutex.lock();
    for (auto it = callHandlers.begin(); it != callHandlers.end(); it++)
    {
        callList->addMegaHandle(it->first);
    }

    sdkMutex.unlock();
    return callList;
}

MegaHandleList *MegaChatApiImpl::getChatCallsIds()
{
    MegaHandleListPrivate *callList = new MegaHandleListPrivate();

    sdkMutex.lock();
    for (auto it = callHandlers.begin(); it != callHandlers.end(); it++)
    {
        callList->addMegaHandle(it->second->getCall()->id());
    }

    sdkMutex.unlock();
    return callList;
}

void MegaChatApiImpl::addChatCallListener(MegaChatCallListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    callListeners.insert(listener);
    sdkMutex.unlock();

}

void MegaChatApiImpl::addChatLocalVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    localVideoListeners.insert(listener);
    videoMutex.unlock();
}

void MegaChatApiImpl::addChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    remoteVideoListeners.insert(listener);
    videoMutex.unlock();
}

#endif

void MegaChatApiImpl::addChatRequestListener(MegaChatRequestListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    requestListeners.insert(listener);
    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatListener(MegaChatListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    listeners.insert(listener);
    sdkMutex.unlock();
}

void MegaChatApiImpl::addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    if (!listener || chatid == MEGACHAT_INVALID_HANDLE)
    {
        return;
    }

    sdkMutex.lock();
    MegaChatRoomHandler *roomHandler = getChatRoomHandler(chatid);
    roomHandler->addChatRoomListener(listener);
    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatRequestListener(MegaChatRequestListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    requestListeners.erase(listener);

    map<int,MegaChatRequestPrivate*>::iterator it = requestMap.begin();
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
    sdkMutex.unlock();
}

#ifndef KARERE_DISABLE_WEBRTC

void MegaChatApiImpl::removeChatCallListener(MegaChatCallListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    callListeners.erase(listener);
    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatLocalVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    localVideoListeners.erase(listener);
    videoMutex.unlock();
}

void MegaChatApiImpl::removeChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    remoteVideoListeners.erase(listener);
    videoMutex.unlock();
}

#endif  // webrtc

void MegaChatApiImpl::removeChatListener(MegaChatListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    listeners.erase(listener);
    sdkMutex.unlock();
}

void MegaChatApiImpl::removeChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    MegaChatRoomHandler *roomHandler = getChatRoomHandler(chatid);
    roomHandler->removeChatRoomListener(listener);
    sdkMutex.unlock();
}

IApp::IChatHandler *MegaChatApiImpl::createChatHandler(ChatRoom &room)
{
    return getChatRoomHandler(room.chatid());
}

IApp::IContactListHandler *MegaChatApiImpl::contactListHandler()
{
    return nullptr;
}

IApp::IChatListHandler *MegaChatApiImpl::chatListHandler()
{
    return this;
}

void MegaChatApiImpl::onIncomingContactRequest(const MegaContactRequest &req)
{
    // it is notified to the app by the existing MegaApi
}

#ifndef KARERE_DISABLE_WEBRTC

rtcModule::ICallHandler *MegaChatApiImpl::onIncomingCall(rtcModule::ICall& call, karere::AvFlags av)
{
    MegaChatCallHandler *chatCallHandler = new MegaChatCallHandler(this);
    chatCallHandler->setCall(&call);
    MegaChatHandle chatid = call.chat().chatId();
    callHandlers[chatid] = chatCallHandler;
    chatCallHandler->getMegaChatCall()->setRemoteAudioVideoFlags(av);

    // Notify onIncomingCall like state change becouse rtcModule::ICall::kStateRingIn status
    // it is not notify
    chatCallHandler->onStateChange(call.state());

    return chatCallHandler;
}

MegaStringList *MegaChatApiImpl::getChatInDevices(const std::vector<string> &devicesVector)
{
    int devicesNumber = devicesVector.size();
    char **devicesArray = NULL;
    if (devicesNumber > 0)
    {
        devicesArray = new char*[devicesNumber];
        for (int i = 0; i < devicesNumber; ++i)
        {
            char *device = MegaApi::strdup(devicesVector[i].c_str());
            devicesArray[i] = device;
        }
    }

    MegaStringList *devices = new MegaStringListPrivate(devicesArray, devicesNumber);
    delete [] devicesArray;

    return devices;

}

void MegaChatApiImpl::cleanCallHandlerMap()
{
    std::map<MegaChatHandle, MegaChatCallHandler*>::iterator callHandlersIterator;
    for (callHandlersIterator = callHandlers.begin(); callHandlersIterator != callHandlers.end(); ++callHandlersIterator)
    {
        MegaChatCallHandler* callHandler = callHandlersIterator->second;
        delete callHandler;
        callHandlersIterator->second = NULL;
    }

    callHandlers.clear();
}

MegaChatCallHandler *MegaChatApiImpl::findChatCallHandler(MegaChatHandle chatid)
{
    std::map<MegaChatHandle, MegaChatCallHandler*>::iterator it = callHandlers.find(chatid);
    if (it != callHandlers.end())
    {
        return it->second;
    }

    return NULL;
}

void MegaChatApiImpl::removeChatCallHandler(MegaChatHandle chatid)
{
    callHandlers.erase(chatid);
}

#endif

void MegaChatApiImpl::onInitStateChange(int newState)
{
    API_LOG_DEBUG("Karere initialization state has changed: %d", newState);

    if (newState == karere::Client::kInitErrSidInvalid)
    {
        API_LOG_WARNING("Invalid session detected (API_ESID). Logging out...");
        logout();
        return;
    }

    int state = MegaChatApiImpl::convertInitState(newState);

    // only notify meaningful state to the app
    if (state == MegaChatApi::INIT_ERROR ||
            state == MegaChatApi::INIT_WAITING_NEW_SESSION ||
            state == MegaChatApi::INIT_OFFLINE_SESSION ||
            state == MegaChatApi::INIT_ONLINE_SESSION ||
            state == MegaChatApi::INIT_NO_CACHE)
    {
        fireOnChatInitStateUpdate(state);
    }
}

int MegaChatApiImpl::convertInitState(int state)
{
    switch (state)
    {
    case karere::Client::kInitErrGeneric:
    case karere::Client::kInitErrCorruptCache:
    case karere::Client::kInitErrSidMismatch:
    case karere::Client::kInitErrAlready:
        return MegaChatApi::INIT_ERROR;

    case karere::Client::kInitCreated:
        return MegaChatApi::INIT_NOT_DONE;

    case karere::Client::kInitErrNoCache:
        return MegaChatApi::INIT_NO_CACHE;

    case karere::Client::kInitWaitingNewSession:
        return MegaChatApi::INIT_WAITING_NEW_SESSION;

    case karere::Client::kInitHasOfflineSession:
        return MegaChatApi::INIT_OFFLINE_SESSION;

    case karere::Client::kInitHasOnlineSession:
        return MegaChatApi::INIT_ONLINE_SESSION;

    case karere::Client::kInitTerminated:
    case karere::Client::kInitErrSidInvalid:
    default:
        return state;
    }
}

int MegaChatApiImpl::convertChatConnectionState(ChatState state)
{
    switch(state)
    {
    case ChatState::kChatStateOffline:
        return MegaChatApi::CHAT_CONNECTION_OFFLINE;
    case ChatState::kChatStateConnecting:
        return MegaChatApi::CHAT_CONNECTION_IN_PROGRESS;
    case ChatState::kChatStateJoining:
        return MegaChatApi::CHAT_CONNECTION_LOGGING;
    case ChatState::kChatStateOnline:
        return MegaChatApi::CHAT_CONNECTION_ONLINE;
    }

    assert(false);  // check compilation warnings, new ChatState not considered
    return state;
}

void MegaChatApiImpl::sendAttachNodesMessage(std::string buffer, MegaChatRequestPrivate *request)
{
    ChatRoom *chatroom = findChatRoom(request->getChatHandle());

    buffer.insert(buffer.begin(), Message::kMsgAttachment);
    buffer.insert(buffer.begin(), 0x0);

    Message *m = chatroom->chat().msgSubmit(buffer.c_str(), buffer.length(), Message::kMsgAttachment, NULL);
    MegaChatMessage *megaMsg = new MegaChatMessagePrivate(*m, Message::Status::kSending, CHATD_IDX_INVALID);
    request->setMegaChatMessage(megaMsg);
}

IApp::IGroupChatListItem *MegaChatApiImpl::addGroupChatItem(GroupChatRoom &chat)
{
    MegaChatGroupListItemHandler *itemHandler = new MegaChatGroupListItemHandler(*this, chat);
    chatGroupListItemHandler.insert(itemHandler);

    // notify the app about the new chatroom
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(chat);
    fireOnChatListItemUpdate(item);

    return (IGroupChatListItem *) itemHandler;
}

IApp::IPeerChatListItem *MegaChatApiImpl::addPeerChatItem(PeerChatRoom &chat)
{
    MegaChatPeerListItemHandler *itemHandler = new MegaChatPeerListItemHandler(*this, chat);
    chatPeerListItemHandler.insert(itemHandler);

    // notify the app about the new chatroom
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(chat);
    fireOnChatListItemUpdate(item);

    return (IPeerChatListItem *) itemHandler;
}

void MegaChatApiImpl::removeGroupChatItem(IGroupChatListItem &item)
{
    set<MegaChatGroupListItemHandler *>::iterator it = chatGroupListItemHandler.begin();
    while (it != chatGroupListItemHandler.end())
    {
        IGroupChatListItem *itemHandler = (*it);
        if (itemHandler == &item)
        {
//            TODO: Redmine ticket #5693
//            MegaChatListItemPrivate *listItem = new MegaChatListItemPrivate((*it)->getChatRoom());
//            listItem->setClosed();
//            fireOnChatListItemUpdate(listItem);

            delete (itemHandler);
            chatGroupListItemHandler.erase(it);
            return;
        }

        it++;
    }
}

void MegaChatApiImpl::removePeerChatItem(IPeerChatListItem &item)
{
    set<MegaChatPeerListItemHandler *>::iterator it = chatPeerListItemHandler.begin();
    while (it != chatPeerListItemHandler.end())
    {
        IPeerChatListItem *itemHandler = (*it);
        if (itemHandler == &item)
        {
//            TODO: Redmine ticket #5693
//            MegaChatListItemPrivate *listItem = new MegaChatListItemPrivate((*it)->getChatRoom());
//            listItem->setClosed();
//            fireOnChatListItemUpdate(listItem);

            delete (itemHandler);
            chatPeerListItemHandler.erase(it);
            return;
        }

        it++;
    }
}

void MegaChatApiImpl::onPresenceChanged(Id userid, Presence pres, bool inProgress)
{
    if (inProgress)
    {
        API_LOG_INFO("My own presence is being changed to %s", pres.toString());
    }
    else
    {
        API_LOG_INFO("Presence of user %s has been changed to %s", userid.toString().c_str(), pres.toString());
    }
    fireOnChatOnlineStatusUpdate(userid.val, pres.status(), inProgress);
}

void MegaChatApiImpl::onPresenceConfigChanged(const presenced::Config &state, bool pending)
{
    MegaChatPresenceConfigPrivate *config = new MegaChatPresenceConfigPrivate(state, pending);
    fireOnChatPresenceConfigUpdate(config);
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

    deque<MegaChatRequestPrivate *>::iterator it = requests.begin();
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

bool EventQueue::isEmpty()
{
    bool ret;

    mutex.lock();
    ret = events.empty();
    mutex.unlock();

    return ret;
}

size_t EventQueue::size()
{
    size_t ret;

    mutex.lock();
    ret = events.size();
    mutex.unlock();

    return ret;
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
    this->chatid = MEGACHAT_INVALID_HANDLE;
    this->userHandle = MEGACHAT_INVALID_HANDLE;
    this->privilege = MegaChatPeerList::PRIV_UNKNOWN;
    this->text = NULL;
    this->mMessage = NULL;
    this->mMegaNodeList = NULL;
}

MegaChatRequestPrivate::MegaChatRequestPrivate(MegaChatRequestPrivate &request)
{
    this->text = NULL;
    this->peerList = NULL;
    this->mMessage = NULL;
    this->mMegaNodeList = NULL;

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
    this->setText(request.getText());
    this->setMegaChatMessage(request.getMegaChatMessage());
    this->setMegaNodeList(request.getMegaNodeList());
}

MegaChatRequestPrivate::~MegaChatRequestPrivate()
{
    delete peerList;
    delete [] text;
    delete mMessage;
    delete mMegaNodeList;
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
        case TYPE_LOGOUT: return "LOGOUT";
        case TYPE_CONNECT: return "CONNECT";
        case TYPE_INITIALIZE: return "INITIALIZE";
        case TYPE_SET_ONLINE_STATUS: return "SET_CHAT_STATUS";
        case TYPE_CREATE_CHATROOM: return "CREATE CHATROOM";
        case TYPE_INVITE_TO_CHATROOM: return "INVITE_TO_CHATROOM";
        case TYPE_REMOVE_FROM_CHATROOM: return "REMOVE_FROM_CHATROOM";
        case TYPE_UPDATE_PEER_PERMISSIONS: return "UPDATE_PEER_PERMISSIONS";
        case TYPE_TRUNCATE_HISTORY: return "TRUNCATE_HISTORY";
        case TYPE_EDIT_CHATROOM_NAME: return "EDIT_CHATROOM_NAME";
        case TYPE_EDIT_CHATROOM_PIC: return "EDIT_CHATROOM_PIC";
        case TYPE_GET_FIRSTNAME: return "GET_FIRSTNAME";
        case TYPE_GET_LASTNAME: return "GET_LASTNAME";
        case TYPE_GET_EMAIL: return "GET_EMAIL";
        case TYPE_DISCONNECT: return "DISCONNECT";
        case TYPE_SET_BACKGROUND_STATUS: return "SET_BACKGROUND_STATUS";
        case TYPE_RETRY_PENDING_CONNECTIONS: return "RETRY_PENDING_CONNECTIONS";
        case TYPE_START_CHAT_CALL: return "START_CHAT_CALL";
        case TYPE_ANSWER_CHAT_CALL: return "ANSWER_CHAT_CALL";
        case TYPE_DISABLE_AUDIO_VIDEO_CALL: return "DISABLE_AUDIO_VIDEO_CALL";
        case TYPE_HANG_CHAT_CALL: return "HANG_CHAT_CALL";
        case TYPE_LOAD_AUDIO_VIDEO_DEVICES: return "LOAD_AUDIO_VIDEO_DEVICES";
        case TYPE_ATTACH_NODE_MESSAGE: return "ATTACH_NODE_MESSAGE";
        case TYPE_REVOKE_NODE_MESSAGE: return "REVOKE_NODE_MESSAGE";
        case TYPE_SHARE_CONTACT: return "SHARE_CONTACT";
        case TYPE_SEND_TYPING_NOTIF: return "SEND_TYPING_NOTIF";
        case TYPE_SIGNAL_ACTIVITY: return "SIGNAL_ACTIVITY";
        case TYPE_SET_PRESENCE_PERSIST: return "SET_PRESENCE_PERSIST";
        case TYPE_SET_PRESENCE_AUTOAWAY: return "SET_PRESENCE_AUTOAWAY";
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

const char *MegaChatRequestPrivate::getText() const
{
    return text;
}

MegaChatMessage *MegaChatRequestPrivate::getMegaChatMessage()
{
    return mMessage;
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

    this->peerList = peerList ? peerList->copy() : NULL;
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

void MegaChatRequestPrivate::setText(const char *text)
{
    if(this->text)
    {
        delete [] this->text;
    }
    this->text = MegaApi::strdup(text);
}

void MegaChatRequestPrivate::setMegaChatMessage(MegaChatMessage *message)
{
    if (mMessage != NULL)
    {
        delete mMessage;
    }

    mMessage = message ? message->copy() : NULL;
}

MegaNodeList *MegaChatRequestPrivate::getMegaNodeList()
{
    return mMegaNodeList;
}

int MegaChatRequestPrivate::getParamType()
{
    return mParamType;
}

void MegaChatRequestPrivate::setMegaNodeList(MegaNodeList *nodelist)
{
    if (mMegaNodeList != NULL)
    {
        delete mMegaNodeList;
    }

    mMegaNodeList = nodelist ? nodelist->copy() : NULL;
}

void MegaChatRequestPrivate::setParamType(int paramType)
{
    this->mParamType = paramType;
}

#ifndef KARERE_DISABLE_WEBRTC

MegaChatCallPrivate::MegaChatCallPrivate(const rtcModule::ICall& call)
{
    status = call.state();
    chatid = call.chat().chatId();
    callid = call.id();
    // sentAv are invalid until state change to rtcModule::ICall::KStateHasLocalStream
    localAVFlags = call.sentAv();
    std::map<karere::Id, karere::AvFlags> remoteFlags = call.avFlagsRemotePeers();
    remoteAVFlags = karere::AvFlags(false, false);
    if (remoteFlags.size() > 0)
    {
        // With peer to peer call, there is only one session (one element at map)
        remoteAVFlags = remoteFlags.begin()->second;
    }

    initialTs = 0;
    finalTs = 0;
    temporaryError = std::string("");
    termCode = MegaChatCall::TERM_CODE_NOT_FINISHED;
    localTermCode = false;
    ringing = false;
    changed = 0;
}

MegaChatCallPrivate::MegaChatCallPrivate(const MegaChatCallPrivate &call)
{
    this->status = call.getStatus();
    this->chatid = call.getChatid();
    this->callid = call.getId();
    this->localAVFlags = call.localAVFlags;
    this->remoteAVFlags = call.remoteAVFlags;
    this->changed = call.changed;
    this->initialTs = call.initialTs;
    this->finalTs = call.finalTs;
    this->temporaryError = call.temporaryError;
    this->termCode = call.termCode;
    this->localTermCode = call.localTermCode;
    this->ringing = call.ringing;
}

MegaChatCallPrivate::~MegaChatCallPrivate()
{
}

MegaChatCall *MegaChatCallPrivate::copy()
{
    return new MegaChatCallPrivate(*this);
}

int MegaChatCallPrivate::getStatus() const
{
    return status;
}

MegaChatHandle MegaChatCallPrivate::getChatid() const
{
    return chatid;
}

MegaChatHandle MegaChatCallPrivate::getId() const
{
    return callid;
}

bool MegaChatCallPrivate::hasLocalAudio()
{
    return localAVFlags.audio();
}

bool MegaChatCallPrivate::hasRemoteAudio()
{
    return remoteAVFlags.audio();
}

bool MegaChatCallPrivate::hasLocalVideo()
{
    return localAVFlags.video();
}

bool MegaChatCallPrivate::hasRemoteVideo()
{
    return remoteAVFlags.video();
}

int MegaChatCallPrivate::getChanges() const
{
    return changed;
}

bool MegaChatCallPrivate::hasChanged(int changeType) const
{
    return (changed & changeType);
}

int64_t MegaChatCallPrivate::getDuration() const
{
    int64_t duration = 0;

    if (initialTs > 0)
    {
        if (finalTs > 0)
        {
            duration = finalTs - initialTs;
        }
        else
        {
            duration = time(NULL) - initialTs;
        }
    }

    return duration;
}

int64_t MegaChatCallPrivate::getInitialTimeStamp() const
{
    return initialTs;
}

int64_t MegaChatCallPrivate::getFinalTimeStamp() const
{
    return finalTs;
}

const char *MegaChatCallPrivate::getTemporaryError() const
{
    return temporaryError.c_str();
}

int MegaChatCallPrivate::getTermCode() const
{
    return termCode;
}

bool MegaChatCallPrivate::isLocalTermCode() const
{
    return localTermCode;
}

bool MegaChatCallPrivate::isRinging() const
{
    return ringing;
}

void MegaChatCallPrivate::setStatus(int status)
{
    this->status = status;
    changed |= MegaChatCall::CHANGE_TYPE_STATUS;
}

void MegaChatCallPrivate::setLocalAudioVideoFlags(AvFlags localAVFlags)
{
    this->localAVFlags = localAVFlags;
    changed |= MegaChatCall::CHANGE_TYPE_LOCAL_AVFLAGS;
}

void MegaChatCallPrivate::setRemoteAudioVideoFlags(AvFlags remoteAVFlags)
{
    this->remoteAVFlags = remoteAVFlags;
    changed |= MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS;
}

void MegaChatCallPrivate::setInitialTimeStamp(int64_t timeStamp)
{
    initialTs = timeStamp;
}

void MegaChatCallPrivate::setFinalTimeStamp(int64_t timeStamp)
{
    if (initialTs > 0)
    {
        finalTs = timeStamp;
    }
}

void MegaChatCallPrivate::removeChanges()
{
    changed = 0;
    temporaryError.clear();
}

void MegaChatCallPrivate::setError(const string &temporaryError)
{
    this->temporaryError = temporaryError;
    changed |= MegaChatCall::CHANGE_TYPE_TEMPORARY_ERROR;
}

void MegaChatCallPrivate::setTermCode(rtcModule::TermCode termCode)
{
    assert(this->termCode == MegaChatCall::TERM_CODE_NOT_FINISHED);
    convertTermCode(termCode);
}

void MegaChatCallPrivate::convertTermCode(rtcModule::TermCode termCode)
{
    // Last four bits indicate the termination code and fifth bit indicate local or peer
    switch (termCode & (~rtcModule::TermCode::kPeer))
    {
    case rtcModule::TermCode::kUserHangup:
        this->termCode = MegaChatCall::TERM_CODE_USER_HANGUP;
        break;
    case rtcModule::TermCode::kCallRejected:
        this->termCode = MegaChatCall::TERM_CODE_CALL_REJECT;
        break;
    case rtcModule::TermCode::kAnsElsewhere:
        this->termCode = MegaChatCall::TERM_CODE_ANSWER_ELSE_WHERE;
        break;
    case rtcModule::TermCode::kAnswerTimeout:
        this->termCode = MegaChatCall::TERM_CODE_ANSWER_TIMEOUT;
        break;
    case rtcModule::TermCode::kRingOutTimeout:
        this->termCode = MegaChatCall::TERM_CODE_RING_OUT_TIMEOUT;
        break;
    case rtcModule::TermCode::kAppTerminating:
        this->termCode = MegaChatCall::TERM_CODE_APP_TERMINATING;
        break;
    case rtcModule::TermCode::kBusy:
        this->termCode = MegaChatCall::TERM_CODE_BUSY;
        break;
    case rtcModule::TermCode::kNotFinished:
        this->termCode = MegaChatCall::TERM_CODE_NOT_FINISHED;
        break;
    case rtcModule::TermCode::kCallGone:
    case rtcModule::TermCode::kInvalid:
    default:
        this->termCode = MegaChatCall::TERM_CODE_ERROR;
        break;
    }

    if (termCode & rtcModule::TermCode::kPeer)
    {
        localTermCode = false;
    }
    else
    {
        localTermCode = true;
    }
}

void MegaChatCallPrivate::setIsRinging(bool ringing)
{
    this->ringing = ringing;
    changed |= MegaChatCall::CHANGE_TYPE_RINGING_STATUS;
}

MegaChatVideoReceiver::MegaChatVideoReceiver(MegaChatApiImpl *chatApi, rtcModule::ICall *call, bool local)
{
    this->chatApi = chatApi;
    chatid = call->chat().chatId();
    this->local = local;
}

MegaChatVideoReceiver::~MegaChatVideoReceiver()
{
}

void* MegaChatVideoReceiver::getImageBuffer(unsigned short width, unsigned short height, void*& userData)
{
    MegaChatVideoFrame *frame = new MegaChatVideoFrame;
    frame->width = width;
    frame->height = height;
    frame->buffer = new byte[width * height * 4];  // in format ARGB: 4 bytes per pixel
    userData = frame;
    return frame->buffer;
}

void MegaChatVideoReceiver::frameComplete(void *userData)
{
    chatApi->videoMutex.lock();
    MegaChatVideoFrame *frame = (MegaChatVideoFrame *)userData;
    if(local)
    {
        chatApi->fireOnChatLocalVideoData(chatid, frame->width, frame->height, (char *)frame->buffer);
    }
    else
    {
        chatApi->fireOnChatRemoteVideoData(chatid, frame->width, frame->height, (char *)frame->buffer);
    }
    chatApi->videoMutex.unlock();
    delete frame->buffer;
    delete frame;
}

void MegaChatVideoReceiver::onVideoAttach()
{
}

void MegaChatVideoReceiver::onVideoDetach()
{
}

void MegaChatVideoReceiver::clearViewport()
{
}

void MegaChatVideoReceiver::released()
{
}

rtcModule::ICallHandler *MegaChatRoomHandler::callHandler()
{
    return chatApi->findChatCallHandler(chatid);
}
#endif

MegaChatRoomHandler::MegaChatRoomHandler(MegaChatApiImpl *chatApi, MegaChatHandle chatid)
{
    this->chatApi = chatApi;
    this->chatid = chatid;

    this->mRoom = NULL;
    this->mChat = NULL;
}

void MegaChatRoomHandler::addChatRoomListener(MegaChatRoomListener *listener)
{
    roomListeners.insert(listener);
}

void MegaChatRoomHandler::removeChatRoomListener(MegaChatRoomListener *listener)
{
    roomListeners.erase(listener);
}

void MegaChatRoomHandler::fireOnChatRoomUpdate(MegaChatRoom *chat)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onChatRoomUpdate((MegaChatApi*)chatApi, chat);
    }

    delete chat;
}

void MegaChatRoomHandler::fireOnMessageLoaded(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageLoaded((MegaChatApi*)chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnMessageReceived(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageReceived((MegaChatApi*)chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnMessageUpdate(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageUpdate((MegaChatApi*)chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnHistoryReloaded(MegaChatRoom *chat)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onHistoryReloaded((MegaChatApi*)chatApi, chat);
    }

    delete chat;
}

void MegaChatRoomHandler::onUserTyping(karere::Id user)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApi->getChatRoom(chatid);
    chat->setUserTyping(user.val);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onLastTextMessageUpdated(const chatd::LastTextMsg& msg)
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onLastTextMessageUpdated(msg);
    }
}

void MegaChatRoomHandler::onLastMessageTsUpdated(uint32_t ts)
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onLastMessageTsUpdated(ts);
    }
}

void MegaChatRoomHandler::onHistoryReloaded()
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApi->getChatRoom(chatid);
    fireOnHistoryReloaded(chat);
}

bool MegaChatRoomHandler::isRevoked(MegaChatHandle h)
{
    auto it = attachmentsAccess.find(h);
    if (it != attachmentsAccess.end())
    {
        return !it->second;
    }

    return false;
}

void MegaChatRoomHandler::handleHistoryMessage(MegaChatMessage *message)
{
    if (message->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT)
    {
        MegaNodeList *nodeList = message->getMegaNodeList();
        for (int i = 0; i < nodeList->size(); i++)
        {
            MegaChatHandle h = nodeList->get(i)->getHandle();
            auto itAccess = attachmentsAccess.find(h);
            if (itAccess == attachmentsAccess.end())
            {
                attachmentsAccess[h] = true;
            }
            attachmentsIds[h].insert(message->getMsgId());
        }
    }
    else if (message->getType() == MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT)
    {
        MegaChatHandle h = message->getHandleOfAction();
        auto itAccess = attachmentsAccess.find(h);
        if (itAccess == attachmentsAccess.end())
        {
            attachmentsAccess[h] = false;
        }
    }
}

std::set<MegaChatHandle> *MegaChatRoomHandler::handleNewMessage(MegaChatMessage *message)
{
    set <MegaChatHandle> *msgToUpdate = NULL;

    // new messages overwrite any current access to nodes
    if (message->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT)
    {
        MegaNodeList *nodeList = message->getMegaNodeList();
        for (int i = 0; i < nodeList->size(); i++)
        {
            MegaChatHandle h = nodeList->get(i)->getHandle();
            auto itAccess = attachmentsAccess.find(h);
            if (itAccess != attachmentsAccess.end() && !itAccess->second)
            {
                // access changed from revoked to granted --> update attachment messages
                if (!msgToUpdate)
                {
                    msgToUpdate = new set <MegaChatHandle>;
                }
                msgToUpdate->insert(attachmentsIds[h].begin(), attachmentsIds[h].end());
            }
            attachmentsAccess[h] = true;
            attachmentsIds[h].insert(message->getMsgId());
        }
    }
    else if (message->getType() == MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT)
    {
        MegaChatHandle h = message->getHandleOfAction();
        auto itAccess = attachmentsAccess.find(h);
        if (itAccess != attachmentsAccess.end() && itAccess->second)
        {
            // access changed from granted to revoked --> update attachment messages
            if (!msgToUpdate)
            {
                msgToUpdate = new set <MegaChatHandle>;
            }
            msgToUpdate->insert(attachmentsIds[h].begin(), attachmentsIds[h].end());
        }
        attachmentsAccess[h] = false;
    }

    return msgToUpdate;
}

void MegaChatRoomHandler::onMemberNameChanged(uint64_t userid, const std::string &newName)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApi->getChatRoom(chatid);
    chat->setMembersUpdated();

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onTitleChanged(const string &title)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApi->getChatRoom(chatid);
    chat->setTitle(title);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onUnreadCountChanged(int count)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApi->getChatRoom(chatid);
    chat->setUnreadCount(count);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::init(Chat &chat, DbInterface *&)
{
    mChat = &chat;
    mRoom = chatApi->findChatRoom(chatid);

    attachmentsAccess.clear();
    attachmentsIds.clear();
    mChat->resetListenerState();
}

void MegaChatRoomHandler::onDestroy()
{
    mChat = NULL;
    mRoom = NULL;
    attachmentsAccess.clear();
    attachmentsIds.clear();
}

void MegaChatRoomHandler::onRecvNewMessage(Idx idx, Message &msg, Message::Status status)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    set <MegaChatHandle> *msgToUpdate = handleNewMessage(message);

    fireOnMessageReceived(message);

    if (msgToUpdate)
    {
        for (auto itMsgId = msgToUpdate->begin(); itMsgId != msgToUpdate->end(); itMsgId++)
        {
            MegaChatMessagePrivate *msg = (MegaChatMessagePrivate *)chatApi->getMessage(chatid, *itMsgId);
            if (msg)
            {
                msg->setAccess();
                fireOnMessageUpdate(msg);
            }
        }
        delete msgToUpdate;
    }
}

void MegaChatRoomHandler::onRecvHistoryMessage(Idx idx, Message &msg, Message::Status status, bool isLocal)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    handleHistoryMessage(message);

    fireOnMessageLoaded(message);
}

void MegaChatRoomHandler::onHistoryDone(chatd::HistSource /*source*/)
{
    fireOnMessageLoaded(NULL);
}

void MegaChatRoomHandler::onUnsentMsgLoaded(chatd::Message &msg)
{
    Message::Status status = (Message::Status) MegaChatMessage::STATUS_SENDING;
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, MEGACHAT_INVALID_INDEX);
    fireOnMessageLoaded(message);
}

void MegaChatRoomHandler::onUnsentEditLoaded(chatd::Message &msg, bool oriMsgIsSending)
{
    Idx index = MEGACHAT_INVALID_INDEX;
    if (!oriMsgIsSending)   // original message was already sent
    {
        index = mChat->msgIndexFromId(msg.id());
    }
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, Message::kSending, index);
    message->setContentChanged();
    fireOnMessageLoaded(message);
}

void MegaChatRoomHandler::onMessageConfirmed(Id msgxid, const Message &msg, Idx idx)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, Message::kServerReceived, idx);
    message->setStatus(MegaChatMessage::STATUS_SERVER_RECEIVED);
    message->setTempId(msgxid);     // to allow the app to find the "temporal" message

    std::set <MegaChatHandle> *msgToUpdate = handleNewMessage(message);

    fireOnMessageUpdate(message);

    if (msgToUpdate)
    {
        for (auto itMsgId = msgToUpdate->begin(); itMsgId != msgToUpdate->end(); itMsgId++)
        {
            MegaChatMessagePrivate *msg = (MegaChatMessagePrivate *)chatApi->getMessage(chatid, *itMsgId);
            if (msg)
            {
                msg->setAccess();
                fireOnMessageUpdate(msg);
            }
        }
        delete msgToUpdate;
    }
}

void MegaChatRoomHandler::onMessageRejected(const Message &msg, uint8_t reason)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, Message::kServerRejected, MEGACHAT_INVALID_INDEX);
    message->setStatus(MegaChatMessage::STATUS_SERVER_REJECTED);
    message->setCode(reason);
    fireOnMessageUpdate(message);
}

void MegaChatRoomHandler::onMessageStatusChange(Idx idx, Message::Status newStatus, const Message &msg)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, newStatus, idx);
    message->setStatus(newStatus);
    fireOnMessageUpdate(message);
}

void MegaChatRoomHandler::onMessageEdited(const Message &msg, chatd::Idx idx)
{
    Message::Status status = mChat->getMsgStatus(msg, idx);
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    message->setContentChanged();
    fireOnMessageUpdate(message);
}

void MegaChatRoomHandler::onEditRejected(const Message &msg, ManualSendReason reason)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, Message::kSendingManual, MEGACHAT_INVALID_INDEX);
    if (reason == ManualSendReason::kManualSendEditNoChange)
    {
        API_LOG_WARNING("Edit message rejected because of same content");
        message->setStatus(mChat->getMsgStatus(msg, msg.id()));
    }
    else
    {
        API_LOG_WARNING("Edit message rejected, reason: %d", reason);
        message->setCode(reason);
    }
    fireOnMessageUpdate(message);
}

void MegaChatRoomHandler::onOnlineStateChange(ChatState state)
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onOnlineStateChange(state);
    }
}

void MegaChatRoomHandler::onUserJoin(Id userid, Priv privilege)
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onUserJoin(userid, privilege);

        MegaChatRoomPrivate *chatroom = new MegaChatRoomPrivate(*mRoom);
        if (userid.val == chatApi->getMyUserHandle())
        {
            chatroom->setOwnPriv(privilege);
        }
        else
        {
            chatroom->setMembersUpdated();
        }
        fireOnChatRoomUpdate(chatroom);
    }
}

void MegaChatRoomHandler::onUserLeave(Id userid)
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onUserLeave(userid);

        MegaChatRoomPrivate *chatroom = new MegaChatRoomPrivate(*mRoom);
        chatroom->setMembersUpdated();
        fireOnChatRoomUpdate(chatroom);
    }
}

void MegaChatRoomHandler::onRejoinedChat()
{
    if (mRoom)
    {
        MegaChatRoomPrivate *chatroom = new MegaChatRoomPrivate(*mRoom);
        fireOnChatRoomUpdate(chatroom);
    }
}

void MegaChatRoomHandler::onExcludedFromChat()
{
    if (mRoom)
    {
        MegaChatRoomPrivate *chatroom = new MegaChatRoomPrivate(*mRoom);
        chatroom->setClosed();
        fireOnChatRoomUpdate(chatroom);
    }
}

void MegaChatRoomHandler::onUnreadChanged()
{
    if (mRoom)
    {
        // forward the event to the chatroom, so chatlist items also receive the notification
        mRoom->onUnreadChanged();

        if (mChat)
        {
            MegaChatRoomPrivate *chatroom = new MegaChatRoomPrivate(*mRoom);
            chatroom->setUnreadCount(mChat->unreadMsgCount());
            fireOnChatRoomUpdate(chatroom);
        }
    }
}

void MegaChatRoomHandler::onManualSendRequired(chatd::Message *msg, uint64_t id, chatd::ManualSendReason reason)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(*msg, Message::kSendingManual, MEGACHAT_INVALID_INDEX);
    delete msg; // we take ownership of the Message

    message->setStatus(MegaChatMessage::STATUS_SENDING_MANUAL);
    message->setRowId(id); // identifier for the manual-send queue, for removal from queue
    message->setCode(reason);
    fireOnMessageLoaded(message);
}


MegaChatErrorPrivate::MegaChatErrorPrivate(const string &msg, int code, int type)
    : promise::Error(msg, code, type)
{
    this->setHandled();
}

MegaChatErrorPrivate::MegaChatErrorPrivate(int code, int type)
    : promise::Error(MegaChatErrorPrivate::getGenericErrorString(code), code, type)
{
    this->setHandled();
}

const char* MegaChatErrorPrivate::getGenericErrorString(int errorCode)
{
    switch(errorCode)
    {
    case ERROR_OK:
        return "No error";
    case ERROR_ARGS:
        return "Invalid argument";
    case ERROR_ACCESS:
        return "Access denied";
    case ERROR_NOENT:
        return "Resouce does not exist";
    case ERROR_EXIST:
        return "Resource already exists";
    case ERROR_UNKNOWN:
    default:
        return "Unknown error";
    }
}


MegaChatErrorPrivate::MegaChatErrorPrivate(const MegaChatErrorPrivate *error)
    : promise::Error(error->getErrorString(), error->getErrorCode(), error->getErrorType())
{
    this->setHandled();
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
    this->chatid = chat->getChatId();
    this->priv = (privilege_t) chat->getOwnPrivilege();
    for (unsigned int i = 0; i < chat->getPeerCount(); i++)
    {
        MegaChatHandle uh = chat->getPeerHandle(i);
        peers.push_back(userpriv_pair(uh, (privilege_t) chat->getPeerPrivilege(i)));
        peerFirstnames.push_back(chat->getPeerFirstname(i));
        peerLastnames.push_back(chat->getPeerLastname(i));
        peerEmails.push_back(chat->getPeerEmail(i));
    }
    this->group = chat->isGroup();
    this->title = chat->getTitle();
    this->unreadCount = chat->getUnreadCount();
    this->active = chat->isActive();
    this->changed = chat->getChanges();
    this->uh = chat->getUserTyping();
}

MegaChatRoomPrivate::MegaChatRoomPrivate(const ChatRoom &chat)
{
    this->changed = 0;
    this->chatid = chat.chatid();
    this->priv = (privilege_t) chat.ownPriv();
    this->group = chat.isGroup();
    this->title = chat.titleString();
    this->unreadCount = chat.chat().unreadMsgCount();
    this->active = chat.isActive();
    this->uh = MEGACHAT_INVALID_HANDLE;

    if (group)
    {
        GroupChatRoom &groupchat = (GroupChatRoom&) chat;
        GroupChatRoom::MemberMap peers = groupchat.peers();

        GroupChatRoom::MemberMap::iterator it;
        for (it = peers.begin(); it != peers.end(); it++)
        {
            this->peers.push_back(userpriv_pair(it->first, (privilege_t) it->second->priv()));

            const char *buffer = MegaChatRoomPrivate::firstnameFromBuffer(it->second->name());
            this->peerFirstnames.push_back(buffer ? buffer : "");
            delete [] buffer;

            buffer = MegaChatRoomPrivate::lastnameFromBuffer(it->second->name());
            this->peerLastnames.push_back(buffer ? buffer : "");
            delete [] buffer;

            this->peerEmails.push_back(it->second->email());
        }
    }
    else
    {
        PeerChatRoom &peerchat = (PeerChatRoom&) chat;
        privilege_t priv = (privilege_t) peerchat.peerPrivilege();
        handle uh = peerchat.peer();
        string name = peerchat.completeTitleString();

        this->peers.push_back(userpriv_pair(uh, priv));

        const char *buffer = MegaChatRoomPrivate::firstnameFromBuffer(name);
        this->peerFirstnames.push_back(buffer ? buffer : "");
        delete [] buffer;

        buffer = MegaChatRoomPrivate::lastnameFromBuffer(name);
        this->peerLastnames.push_back(buffer ? buffer : "");
        delete [] buffer;

        this->peerEmails.push_back(peerchat.email());
    }
}

MegaChatRoom *MegaChatRoomPrivate::copy() const
{
    return new MegaChatRoomPrivate(this);
}

MegaChatHandle MegaChatRoomPrivate::getChatId() const
{
    return chatid;
}

int MegaChatRoomPrivate::getOwnPrivilege() const
{
    return priv;
}

int MegaChatRoomPrivate::getPeerPrivilegeByHandle(MegaChatHandle userhandle) const
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

const char *MegaChatRoomPrivate::getPeerFirstnameByHandle(MegaChatHandle userhandle) const
{
    for (unsigned int i = 0; i < peers.size(); i++)
    {
        if (peers.at(i).first == userhandle)
        {
            return peerFirstnames.at(i).c_str();
        }
    }

    return NULL;
}

const char *MegaChatRoomPrivate::getPeerLastnameByHandle(MegaChatHandle userhandle) const
{
    for (unsigned int i = 0; i < peers.size(); i++)
    {
        if (peers.at(i).first == userhandle)
        {
            return peerLastnames.at(i).c_str();
        }
    }

    return NULL;
}

const char *MegaChatRoomPrivate::getPeerFullnameByHandle(MegaChatHandle userhandle) const
{
    for (unsigned int i = 0; i < peers.size(); i++)
    {
        if (peers.at(i).first == userhandle)
        {
            string ret = peerFirstnames.at(i);
            if (!peerFirstnames.at(i).empty() && !peerLastnames.at(i).empty())
            {
                ret.append(" ");
            }
            ret.append(peerLastnames.at(i));

            return MegaApi::strdup(ret.c_str());
        }
    }

    return NULL;
}

const char *MegaChatRoomPrivate::getPeerEmailByHandle(MegaChatHandle userhandle) const
{
    for (unsigned int i = 0; i < peerEmails.size(); i++)
    {
        if (peers.at(i).first == userhandle)
        {
            return peerEmails.at(i).c_str();
        }
    }

    return NULL;
}

int MegaChatRoomPrivate::getPeerPrivilege(unsigned int i) const
{
    if (i >= peers.size())
    {
        return MegaChatRoom::PRIV_UNKNOWN;
    }

    return peers.at(i).second;
}

unsigned int MegaChatRoomPrivate::getPeerCount() const
{
    return peers.size();
}

MegaChatHandle MegaChatRoomPrivate::getPeerHandle(unsigned int i) const
{
    if (i >= peers.size())
    {
        return MEGACHAT_INVALID_HANDLE;
    }

    return peers.at(i).first;
}

const char *MegaChatRoomPrivate::getPeerFirstname(unsigned int i) const
{
    if (i >= peerFirstnames.size())
    {
        return NULL;
    }

    return peerFirstnames.at(i).c_str();
}

const char *MegaChatRoomPrivate::getPeerLastname(unsigned int i) const
{
    if (i >= peerLastnames.size())
    {
        return NULL;
    }

    return peerLastnames.at(i).c_str();
}

const char *MegaChatRoomPrivate::getPeerFullname(unsigned int i) const
{
    if (i >= peerLastnames.size() || i >= peerFirstnames.size())
    {
        return NULL;
    }

    string ret = peerFirstnames.at(i);
    if (!peerFirstnames.at(i).empty() && !peerLastnames.at(i).empty())
    {
        ret.append(" ");
    }
    ret.append(peerLastnames.at(i));

    return MegaApi::strdup(ret.c_str());
}

const char *MegaChatRoomPrivate::getPeerEmail(unsigned int i) const
{
    if (i >= peerEmails.size())
    {
        return NULL;
    }

    return peerEmails.at(i).c_str();
}

bool MegaChatRoomPrivate::isGroup() const
{
    return group;
}

const char *MegaChatRoomPrivate::getTitle() const
{
    return title.c_str();
}

bool MegaChatRoomPrivate::isActive() const
{
    return active;
}

int MegaChatRoomPrivate::getChanges() const
{
    return changed;
}

bool MegaChatRoomPrivate::hasChanged(int changeType) const
{
    return (changed & changeType);
}

int MegaChatRoomPrivate::getUnreadCount() const
{
    return unreadCount;
}

MegaChatHandle MegaChatRoomPrivate::getUserTyping() const
{
    return uh;
}

void MegaChatRoomPrivate::setOwnPriv(int ownPriv)
{
    this->priv = (privilege_t) ownPriv;
    this->changed |= MegaChatRoom::CHANGE_TYPE_OWN_PRIV;
}

void MegaChatRoomPrivate::setTitle(const string& title)
{
    this->title = title;
    this->changed |= MegaChatRoom::CHANGE_TYPE_TITLE;
}

void MegaChatRoomPrivate::setUnreadCount(int count)
{
    this->unreadCount = count;
    this->changed |= MegaChatRoom::CHANGE_TYPE_UNREAD_COUNT;
}

void MegaChatRoomPrivate::setMembersUpdated()
{
    this->changed |= MegaChatRoom::CHANGE_TYPE_PARTICIPANTS;
}

void MegaChatRoomPrivate::setUserTyping(MegaChatHandle uh)
{
    this->uh = uh;
    this->changed |= MegaChatRoom::CHANGE_TYPE_USER_TYPING;
}

void MegaChatRoomPrivate::setClosed()
{
    this->changed |= MegaChatRoom::CHANGE_TYPE_CLOSED;
}

char *MegaChatRoomPrivate::firstnameFromBuffer(const string &buffer)
{
    char *ret = NULL;
    int len = buffer.length() ? buffer.at(0) : 0;

    if (len > 0)
    {
        ret = new char[len + 1];
        strncpy(ret, buffer.data() + 1, len);
        ret[len] = '\0';
    }

    return ret;
}

char *MegaChatRoomPrivate::lastnameFromBuffer(const string &buffer)
{
    char *ret = NULL;

    if (buffer.length() && (int)buffer.length() >= buffer.at(0))
    {
        int lenLastname = buffer.length() - buffer.at(0) - 1;
        if (lenLastname)
        {
            const char *start = buffer.data() + 1 + buffer.at(0);
            if (buffer.at(0) != 0)
            {
                start++;    // there's a space separator
                lenLastname--;
            }

            ret = new char[lenLastname + 1];
            strncpy(ret, start, lenLastname);
            ret[lenLastname] = '\0';
        }
    }

    return ret;
}

void MegaChatListItemHandler::onTitleChanged(const string &title)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setTitle(title);

    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onUnreadCountChanged(int count)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setUnreadCount(count);

    chatApi.fireOnChatListItemUpdate(item);
}

const ChatRoom &MegaChatListItemHandler::getChatRoom() const
{
    return mRoom;
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
        return MEGACHAT_INVALID_HANDLE;
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


MegaChatListItemHandler::MegaChatListItemHandler(MegaChatApiImpl &chatApi, ChatRoom &room)
    :chatApi(chatApi), mRoom(room)
{
}

MegaChatListItemPrivate::MegaChatListItemPrivate(ChatRoom &chatroom)
    : MegaChatListItem()
{
    this->chatid = chatroom.chatid();
    this->title = chatroom.titleString();
    this->unreadCount = chatroom.chat().unreadMsgCount();
    this->group = chatroom.isGroup();
    this->active = chatroom.isActive();
    this->ownPriv = chatroom.ownPriv();
    this->changed = 0;
    this->peerHandle = !group ? ((PeerChatRoom&)chatroom).peer() : MEGACHAT_INVALID_HANDLE;

    LastTextMsg tmp;
    LastTextMsg *message = &tmp;
    LastTextMsg *&msg = message;
    uint8_t lastMsgStatus = chatroom.chat().lastTextMessage(msg);
    if (lastMsgStatus == LastTextMsgState::kHave)
    {
        this->lastMsg = JSonUtils::getLastMessageContent(msg->contents(), msg->type());
        this->lastMsgSender = msg->sender();
        this->lastMsgType = msg->type();
        if (msg->idx() == CHATD_IDX_INVALID)
        {
            this->mLastMsgId = (MegaChatHandle) msg->xid();
        }
        else
        {
            this->mLastMsgId = (MegaChatHandle) msg->id();
        }
    }
    else
    {
        this->lastMsg = "";
        this->lastMsgSender = MEGACHAT_INVALID_HANDLE;
        this->lastMsgType = lastMsgStatus;
        this->mLastMsgId = MEGACHAT_INVALID_HANDLE;
    }

    this->lastTs = chatroom.chat().lastMessageTs();
}

MegaChatListItemPrivate::MegaChatListItemPrivate(const MegaChatListItem *item)
{
    this->chatid = item->getChatId();
    this->title = item->getTitle();
    this->ownPriv = item->getOwnPrivilege();
    this->unreadCount = item->getUnreadCount();
    this->changed = item->getChanges();
    this->lastTs = item->getLastTimestamp();
    this->lastMsg = item->getLastMessage();
    this->lastMsgType = item->getLastMessageType();
    this->lastMsgSender = item->getLastMessageSender();
    this->group = item->isGroup();
    this->active = item->isActive();
    this->peerHandle = item->getPeerHandle();
    this->mLastMsgId = item->getLastMessageId();
}

MegaChatListItemPrivate::~MegaChatListItemPrivate()
{
}

MegaChatListItem *MegaChatListItemPrivate::copy() const
{
    return new MegaChatListItemPrivate(this);
}

int MegaChatListItemPrivate::getChanges() const
{
    return changed;
}

bool MegaChatListItemPrivate::hasChanged(int changeType) const
{
    return (changed & changeType);
}

MegaChatHandle MegaChatListItemPrivate::getChatId() const
{
    return chatid;
}

const char *MegaChatListItemPrivate::getTitle() const
{
    return title.c_str();
}

int MegaChatListItemPrivate::getOwnPrivilege() const
{
    return ownPriv;
}

int MegaChatListItemPrivate::getUnreadCount() const
{
    return unreadCount;
}

const char *MegaChatListItemPrivate::getLastMessage() const
{
    return lastMsg.c_str();
}

MegaChatHandle MegaChatListItemPrivate::getLastMessageId() const
{
    return mLastMsgId;
}

int MegaChatListItemPrivate::getLastMessageType() const
{
    return lastMsgType;
}

MegaChatHandle MegaChatListItemPrivate::getLastMessageSender() const
{
    return lastMsgSender;
}

int64_t MegaChatListItemPrivate::getLastTimestamp() const
{
    return lastTs;
}

bool MegaChatListItemPrivate::isGroup() const
{
    return group;
}

bool MegaChatListItemPrivate::isActive() const
{
    return active;
}

MegaChatHandle MegaChatListItemPrivate::getPeerHandle() const
{
    return peerHandle;
}

void MegaChatListItemPrivate::setOwnPriv(int ownPriv)
{
    this->ownPriv = ownPriv;
    this->changed |= MegaChatListItem::CHANGE_TYPE_OWN_PRIV;
}

void MegaChatListItemPrivate::setTitle(const string &title)
{
    this->title = title;
    this->changed |= MegaChatListItem::CHANGE_TYPE_TITLE;
}

void MegaChatListItemPrivate::setUnreadCount(int count)
{
    this->unreadCount = count;
    this->changed |= MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT;
}

void MegaChatListItemPrivate::setMembersUpdated()
{
    this->changed |= MegaChatListItem::CHANGE_TYPE_PARTICIPANTS;
}

void MegaChatListItemPrivate::setClosed()
{
    this->changed |= MegaChatListItem::CHANGE_TYPE_CLOSED;
}

void MegaChatListItemPrivate::setLastTimestamp(int64_t ts)
{
    this->lastTs = ts;
    this->changed |= MegaChatListItem::CHANGE_TYPE_LAST_TS;
}

void MegaChatListItemPrivate::setLastMessage(MegaChatHandle messageId, int type, const string &msg, const uint64_t uh)
{
    this->lastMsg = msg;
    this->lastMsgType = type;
    this->lastMsgSender = uh;
    this->mLastMsgId = messageId;
    this->changed |= MegaChatListItem::CHANGE_TYPE_LAST_MSG;
}

MegaChatGroupListItemHandler::MegaChatGroupListItemHandler(MegaChatApiImpl &chatApi, ChatRoom &room)
    : MegaChatListItemHandler(chatApi, room)
{

}

void MegaChatGroupListItemHandler::onUserJoin(uint64_t userid, Priv priv)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    if (userid == chatApi.getMyUserHandle())
    {
        item->setOwnPriv(priv);
    }
    else
    {
        item->setMembersUpdated();
    }

    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatGroupListItemHandler::onUserLeave(uint64_t )
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setMembersUpdated();

    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onExcludedFromChat()
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setOwnPriv(item->getOwnPrivilege());
    item->setClosed();
    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onRejoinedChat()
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setOwnPriv(item->getOwnPrivilege());
    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onLastMessageUpdated(const LastTextMsg& msg)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);

    std::string lastMessageContent = JSonUtils::getLastMessageContent(msg.contents(), msg.type());

    MegaChatHandle messageId = MEGACHAT_INVALID_HANDLE;
    if (msg.idx() == CHATD_IDX_INVALID)
    {
        messageId = (MegaChatHandle) msg.xid();
    }
    else
    {
        messageId = (MegaChatHandle) msg.id();
    }

    item->setLastMessage(messageId, msg.type(), lastMessageContent, msg.sender());
    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onLastTsUpdated(uint32_t ts)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setLastTimestamp(ts);
    chatApi.fireOnChatListItemUpdate(item);
}

void MegaChatListItemHandler::onChatOnlineState(const ChatState state)
{
    int newState = MegaChatApiImpl::convertChatConnectionState(state);
    chatApi.fireOnChatConnectionStateUpdate(this->mRoom.chatid(), newState);
}

MegaChatPeerListItemHandler::MegaChatPeerListItemHandler(MegaChatApiImpl &chatApi, ChatRoom &room)
    : MegaChatListItemHandler(chatApi, room)
{

}

MegaChatMessagePrivate::MegaChatMessagePrivate(const MegaChatMessage *msg)
    : megaChatUsers(NULL)
{
    this->msg = MegaApi::strdup(msg->getContent());
    this->uh = msg->getUserHandle();
    this->hAction = msg->getHandleOfAction();
    this->msgId = msg->getMsgId();
    this->tempId = msg->getTempId();
    this->index = msg->getMsgIndex();
    this->status = msg->getStatus();
    this->ts = msg->getTimestamp();
    this->type = msg->getType();
    this->changed = msg->getChanges();
    this->edited = msg->isEdited();
    this->deleted = msg->isDeleted();
    this->priv = msg->getPrivilege();
    this->code = msg->getCode();
    this->rowId = msg->getRowId();
    this->megaNodeList = msg->getMegaNodeList() ? msg->getMegaNodeList()->copy() : NULL;

    if (msg->getUsersCount() != 0)
    {
        this->megaChatUsers = new std::vector<MegaChatAttachedUser>();

        for (unsigned int i = 0; i < msg->getUsersCount(); ++i)
        {
            MegaChatAttachedUser megaChatUser(msg->getUserHandle(i), msg->getUserEmail(i), msg->getUserName(i));

            this->megaChatUsers->push_back(megaChatUser);
        }
    }
}

MegaChatMessagePrivate::MegaChatMessagePrivate(const Message &msg, Message::Status status, Idx index)
    : megaChatUsers(NULL)
    , megaNodeList(NULL)
{
    string tmp(msg.buf(), msg.size());

    if (msg.type == TYPE_NORMAL || msg.type == TYPE_CHAT_TITLE)
    {
        this->msg = msg.size() ? MegaApi::strdup(tmp.c_str()) : NULL;
    }
    else    // for other types, content is irrelevant
    {
        this->msg = NULL;
    }
    this->uh = msg.userid;
    this->msgId = msg.isSending() ? MEGACHAT_INVALID_HANDLE : (MegaChatHandle) msg.id();
    this->tempId = msg.isSending() ? (MegaChatHandle) msg.id() : MEGACHAT_INVALID_HANDLE;
    this->rowId = MEGACHAT_INVALID_HANDLE;
    this->type = msg.type;
    this->ts = msg.ts;
    this->status = status;
    this->index = index;
    this->changed = 0;
    this->edited = msg.updated && msg.size();
    this->deleted = msg.updated && !msg.size();
    this->code = 0;
    this->priv = PRIV_UNKNOWN;
    this->hAction = MEGACHAT_INVALID_HANDLE;

    switch (type)
    {
        case MegaChatMessage::TYPE_PRIV_CHANGE:
        case MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
        {
            const Message::ManagementInfo mngInfo = msg.mgmtInfo();

            this->priv = mngInfo.privilege;
            this->hAction = mngInfo.target;
            break;
        }
        case MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
            megaNodeList = JSonUtils::parseAttachNodeJSon(msg.toText().c_str());
            break;
        }
        case MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT:
        {
            this->hAction = MegaApi::base64ToHandle(msg.toText().c_str());
            break;
        }
        case MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
        {
            megaChatUsers = JSonUtils::parseAttachContactJSon(msg.toText().c_str());
            break;
        }
        case MegaChatMessage::TYPE_NORMAL:
        case MegaChatMessage::TYPE_CHAT_TITLE:
        case MegaChatMessage::TYPE_TRUNCATE:
        default:
            break;
    }
}

MegaChatMessagePrivate::~MegaChatMessagePrivate()
{
    delete [] msg;
    delete megaChatUsers;
    delete megaNodeList;
}

MegaChatMessage *MegaChatMessagePrivate::copy() const
{
    return new MegaChatMessagePrivate(this);
}

int MegaChatMessagePrivate::getStatus() const
{
    return status;
}

MegaChatHandle MegaChatMessagePrivate::getMsgId() const
{
    return msgId;
}

MegaChatHandle MegaChatMessagePrivate::getTempId() const
{
    return tempId;
}

int MegaChatMessagePrivate::getMsgIndex() const
{
    return index;
}

MegaChatHandle MegaChatMessagePrivate::getUserHandle() const
{
    return uh;
}

int MegaChatMessagePrivate::getType() const
{
    return type;
}

int64_t MegaChatMessagePrivate::getTimestamp() const
{
    return ts;
}

const char *MegaChatMessagePrivate::getContent() const
{
    return msg;
}

bool MegaChatMessagePrivate::isEdited() const
{
    return edited;
}

bool MegaChatMessagePrivate::isDeleted() const
{
    return deleted;
}

bool MegaChatMessagePrivate::isEditable() const
{
    return (type == TYPE_NORMAL && !isDeleted() && ((time(NULL) - ts) < CHATD_MAX_EDIT_AGE));
}

bool MegaChatMessagePrivate::isDeletable() const
{
    return ((type == TYPE_NORMAL || type == TYPE_CONTACT_ATTACHMENT || type == TYPE_NODE_ATTACHMENT)
            && !isDeleted() && ((time(NULL) - ts) < CHATD_MAX_EDIT_AGE));
}

bool MegaChatMessagePrivate::isManagementMessage() const
{
    return (type == TYPE_ALTER_PARTICIPANTS ||
            type == TYPE_PRIV_CHANGE ||
            type == TYPE_TRUNCATE ||
            type == TYPE_CHAT_TITLE);
}

MegaChatHandle MegaChatMessagePrivate::getHandleOfAction() const
{
    return hAction;
}

int MegaChatMessagePrivate::getPrivilege() const
{
    return priv;
}

int MegaChatMessagePrivate::getCode() const
{
    return code;
}

MegaChatHandle MegaChatMessagePrivate::getRowId() const
{
    return rowId;
}

int MegaChatMessagePrivate::getChanges() const
{
    return changed;
}

bool MegaChatMessagePrivate::hasChanged(int changeType) const
{
    return (changed & changeType);
}

void MegaChatMessagePrivate::setStatus(int status)
{
    this->status = status;
    this->changed |= MegaChatMessage::CHANGE_TYPE_STATUS;
}

void MegaChatMessagePrivate::setTempId(MegaChatHandle tempId)
{
    this->tempId = tempId;
}

void MegaChatMessagePrivate::setRowId(int id)
{
    this->rowId = id;
}

void MegaChatMessagePrivate::setContentChanged()
{
    this->changed |= MegaChatMessage::CHANGE_TYPE_CONTENT;
}

void MegaChatMessagePrivate::setCode(int code)
{
    this->code = code;
}

void MegaChatMessagePrivate::setAccess()
{
    this->changed |= MegaChatMessage::CHANGE_TYPE_ACCESS;
}

unsigned int MegaChatMessagePrivate::getUsersCount() const
{
    unsigned int size = 0;
    if (megaChatUsers != NULL)
    {
        size = megaChatUsers->size();
    }

    return size;
}

MegaChatHandle MegaChatMessagePrivate::getUserHandle(unsigned int index) const
{
    if (!megaChatUsers || index >= megaChatUsers->size())
    {
        return MEGACHAT_INVALID_HANDLE;
    }

    return megaChatUsers->at(index).getHandle();
}

const char *MegaChatMessagePrivate::getUserName(unsigned int index) const
{
    if (!megaChatUsers || index >= megaChatUsers->size())
    {
        return NULL;
    }

    return megaChatUsers->at(index).getName();
}

const char *MegaChatMessagePrivate::getUserEmail(unsigned int index) const
{
    if (!megaChatUsers || index >= megaChatUsers->size())
    {
        return NULL;
    }

    return megaChatUsers->at(index).getEmail();
}

MegaNodeList *MegaChatMessagePrivate::getMegaNodeList() const
{
    return megaNodeList;
}

LoggerHandler::LoggerHandler()
    : ILoggerBackend(MegaChatApi::LOG_LEVEL_INFO)
{
    mutex.init(true);
    this->megaLogger = NULL;

    gLogger.addUserLogger("MegaChatApi", this);
    gLogger.logChannels[krLogChannel_megasdk].logLevel = krLogLevelDebugVerbose;
    gLogger.logChannels[krLogChannel_websockets].logLevel = krLogLevelDebugVerbose;
    gLogger.logToConsoleUseColors(false);
}

LoggerHandler::~LoggerHandler()
{
    gLogger.removeUserLogger("MegaChatApi");
}

void LoggerHandler::setMegaChatLogger(MegaChatLogger *logger)
{
    this->megaLogger = logger;
}

void LoggerHandler::setLogLevel(int logLevel)
{
    this->maxLogLevel = logLevel;
    switch (logLevel)
    {
        case MegaChatApi::LOG_LEVEL_ERROR:
            MegaApi::setLogLevel(MegaApi::LOG_LEVEL_ERROR);
            break;

        case MegaChatApi::LOG_LEVEL_WARNING:
            MegaApi::setLogLevel(MegaApi::LOG_LEVEL_WARNING);
            break;

        case MegaChatApi::LOG_LEVEL_INFO:
            MegaApi::setLogLevel(MegaApi::LOG_LEVEL_INFO);
            break;

        case MegaChatApi::LOG_LEVEL_VERBOSE:
        case MegaChatApi::LOG_LEVEL_DEBUG:
            MegaApi::setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
            break;

        case MegaChatApi::LOG_LEVEL_MAX:
            MegaApi::setLogLevel(MegaApi::LOG_LEVEL_MAX);
            break;

        default:
            break;
    }
}

void LoggerHandler::setLogWithColors(bool useColors)
{
    gLogger.logToConsoleUseColors(useColors);
}

void LoggerHandler::setLogToConsole(bool enable)
{
    gLogger.logToConsole(enable);
}

void LoggerHandler::log(krLogLevel level, const char *msg, size_t len, unsigned flags)
{
    mutex.lock();
    if (megaLogger)
    {
        megaLogger->log(level, msg);
    }
    mutex.unlock();
}

#ifndef KARERE_DISABLE_WEBRTC

MegaChatCallHandler::MegaChatCallHandler(MegaChatApiImpl *megaChatApi)
{
    this->megaChatApi = megaChatApi;
    call = NULL;
    localVideoReceiver = NULL;
    chatCall = NULL;
}

MegaChatCallHandler::~MegaChatCallHandler()
{
    delete chatCall;
//    delete call;

//    for (int i = 0; i < sessionHandlers.size(); ++i)
//    {
//        delete sessionHandlers[i];
//    }

//    sessionHandlers.clear();

//    delete videoReceiver;
}

void MegaChatCallHandler::setCall(rtcModule::ICall *call)
{
    this->call = call;
    chatCall = new MegaChatCallPrivate(*call);
}

void MegaChatCallHandler::onStateChange(uint8_t newState)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        API_LOG_INFO("Call state changed. ChatId: %s, callid: %s, state: %s --> %s",
                     call->chat().chatId().toString().c_str(),
                     call->id().toString().c_str(),
                     rtcModule::ICall::stateToStr(chatCall->getStatus()),      // assume states are mapped 1 to 1
                     rtcModule::ICall::stateToStr(newState));

        int state = 0;
        switch(newState)
        {
            case rtcModule::ICall::kStateInitial:
                state = MegaChatCall::CALL_STATUS_INITIAL;
                break;
            case rtcModule::ICall::kStateHasLocalStream:
                state = MegaChatCall::CALL_STATUS_HAS_LOCAL_STREAM;
                chatCall->setLocalAudioVideoFlags(call->sentAv());
                break;
            case rtcModule::ICall::kStateReqSent:
                state = MegaChatCall::CALL_STATUS_REQUEST_SENT;
                break;
            case rtcModule::ICall::kStateRingIn:
                state = MegaChatCall::CALL_STATUS_RING_IN;
                break;
            case rtcModule::ICall::kStateJoining:
                state = MegaChatCall::CALL_STATUS_JOINING;
                break;
            case rtcModule::ICall::kStateInProgress:
                chatCall->setIsRinging(false);
                state = MegaChatCall::CALL_STATUS_IN_PROGRESS;
                break;
            case rtcModule::ICall::kStateTerminating:
                state = MegaChatCall::CALL_STATUS_TERMINATING;
                chatCall->setIsRinging(false);
                chatCall->setTermCode(call->termCode());
                chatCall->setFinalTimeStamp(time(NULL));
                API_LOG_INFO("Terminating call. ChatId: %s, callid: %s, termCode: %s , isLocal: %d, duration: %d (s)",
                             call->chat().chatId().toString().c_str(), call->id().toString().c_str(),
                              rtcModule::termCodeToStr(call->termCode() & (~rtcModule::TermCode::kPeer)),
                             chatCall->isLocalTermCode(), chatCall->getDuration());
                break;
            case rtcModule::ICall::kStateDestroyed:
                state = MegaChatCall::CALL_STATUS_DESTROYED;
                break;
            default:
                state = newState;
        }

        chatCall->setStatus(state);
        megaChatApi->fireOnChatCallUpdate(chatCall);
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onStateChange - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }
}

void MegaChatCallHandler::onDestroy(rtcModule::TermCode reason, bool byPeer, const string &msg)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        MegaChatHandle chatid = chatCall->getChatid();

        if (megaChatApi->findChatCallHandler(chatid) == this)
        {
            megaChatApi->removeChatCallHandler(chatCall->getChatid());
        }
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onDestroy - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }

    delete this;
}

rtcModule::ISessionHandler *MegaChatCallHandler::onNewSession(rtcModule::ISession &sess)
{
    sessionHandler = new MegaChatSessionHandler(megaChatApi, this, &sess);
    return sessionHandler;
}

void MegaChatCallHandler::onLocalStreamObtained(rtcModule::IVideoRenderer *&rendererOut)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        if (localVideoReceiver != NULL)
        {
            delete localVideoReceiver;
        }

        rendererOut = new MegaChatVideoReceiver(megaChatApi, call, true);
        localVideoReceiver = rendererOut;
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onLocalStreamObtained - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }
}

void MegaChatCallHandler::onLocalMediaError(const string errors)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        chatCall->setError(errors);
        API_LOG_INFO("Local media error at call. ChatId: %s, callid: %s, error: %s",
                     call->chat().chatId().toString().c_str(), call->id().toString().c_str(), errors.c_str());

        megaChatApi->fireOnChatCallUpdate(chatCall);
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onLocalMediaError - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }
}

void MegaChatCallHandler::onRingOut(Id peer)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        //Avoid notify several times Ring-In state when there are many clients
        if (!chatCall->isRinging())
        {
            chatCall->setIsRinging(true);
            API_LOG_INFO("Call starts ringing at remote peer. ChatId: %s, callid: %s, peer: %s",
                         call->chat().chatId().toString().c_str(), call->id().toString().c_str(), peer.toString().c_str());

            megaChatApi->fireOnChatCallUpdate(chatCall);
        }
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onRingOut - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }
}

void MegaChatCallHandler::onCallStarting()
{

}

void MegaChatCallHandler::onCallStarted()
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        chatCall->setInitialTimeStamp(time(NULL));
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onCallStarted - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
    }
}

rtcModule::ICall *MegaChatCallHandler::getCall()
{
    return call;
}

MegaChatCallPrivate *MegaChatCallHandler::getMegaChatCall()
{
    return chatCall;
}

MegaChatSessionHandler::MegaChatSessionHandler(MegaChatApiImpl *megaChatApi, MegaChatCallHandler* callHandler, rtcModule::ISession *session)
{
    this->megaChatApi = megaChatApi;
    this->callHandler = callHandler;
    this->session = session;
    this->remoteVideoRender = NULL;
}

MegaChatSessionHandler::~MegaChatSessionHandler()
{
    delete remoteVideoRender;
}

void MegaChatSessionHandler::onSessStateChange(uint8_t newState)
{
    if (rtcModule::ISession::kStateInProgress == newState)
    {
        MegaChatCallPrivate* chatCall = callHandler->getMegaChatCall();
        chatCall->setRemoteAudioVideoFlags(session->receivedAv());
        API_LOG_INFO("Initial remote audio/video flags. ChatId: %s, callid: %s, AV: %s",
                     Id(chatCall->getChatid()).toString().c_str(),
                     Id(chatCall->getId()).toString().c_str(),
                     session->receivedAv().toString().c_str());

        megaChatApi->fireOnChatCallUpdate(chatCall);
    }
}

void MegaChatSessionHandler::onSessDestroy(rtcModule::TermCode reason, bool byPeer, const std::string& msg)
{
}

void MegaChatSessionHandler::onRemoteStreamAdded(rtcModule::IVideoRenderer *&rendererOut)
{
    rtcModule::ICall *call = callHandler->getCall();
    assert(call != NULL);

    if (remoteVideoRender != NULL)
    {
       delete remoteVideoRender;
    }

    rendererOut = new MegaChatVideoReceiver(megaChatApi, call, false);
    remoteVideoRender = rendererOut;
}

void MegaChatSessionHandler::onRemoteStreamRemoved()
{
    delete remoteVideoRender;
    remoteVideoRender = NULL;
}

void MegaChatSessionHandler::onPeerMute(karere::AvFlags av, karere::AvFlags oldAv)
{
    MegaChatCallPrivate* chatCall = callHandler->getMegaChatCall();
    chatCall->setRemoteAudioVideoFlags(av);
    API_LOG_INFO("Remote audio/video flags changed. ChatId: %s, callid: %s, AV: %s --> %s",
                 Id(chatCall->getChatid()).toString().c_str(),
                 Id(chatCall->getId()).toString().c_str(),
                 oldAv.toString().c_str(),
                 av.toString().c_str());

    megaChatApi->fireOnChatCallUpdate(chatCall);
}

void MegaChatSessionHandler::onVideoRecv()
{

}

#endif

MegaChatListItemListPrivate::MegaChatListItemListPrivate()
{
}

MegaChatListItemListPrivate::~MegaChatListItemListPrivate()
{
    for (unsigned int i = 0; i < list.size(); i++)
    {
        delete list[i];
        list[i] = NULL;
    }

    list.clear();
}

MegaChatListItemListPrivate::MegaChatListItemListPrivate(const MegaChatListItemListPrivate *list)
{
    MegaChatListItemPrivate *item;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        item = new MegaChatListItemPrivate(list->get(i));
        this->list.push_back(item);
    }
}

MegaChatListItemListPrivate *MegaChatListItemListPrivate::copy() const
{
    return new MegaChatListItemListPrivate(this);
}

const MegaChatListItem *MegaChatListItemListPrivate::get(unsigned int i) const
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

unsigned int MegaChatListItemListPrivate::size() const
{
    return list.size();
}

void MegaChatListItemListPrivate::addChatListItem(MegaChatListItem *item)
{
    list.push_back(item);
}

MegaChatPresenceConfigPrivate::MegaChatPresenceConfigPrivate(const MegaChatPresenceConfigPrivate &config)
{
    this->status = config.getOnlineStatus();
    this->autoawayEnabled = config.isAutoawayEnabled();
    this->autoawayTimeout = config.getAutoawayTimeout();
    this->persistEnabled = config.isPersist();
    this->pending = config.isPending();
}

MegaChatPresenceConfigPrivate::MegaChatPresenceConfigPrivate(const presenced::Config &config, bool isPending)
{
    this->status = config.presence().status();
    this->autoawayEnabled = config.autoawayActive();
    this->autoawayTimeout = config.autoawayTimeout();
    this->persistEnabled = config.persist();
    this->pending = isPending;
}

MegaChatPresenceConfigPrivate::~MegaChatPresenceConfigPrivate()
{

}

MegaChatPresenceConfig *MegaChatPresenceConfigPrivate::copy() const
{
    return new MegaChatPresenceConfigPrivate(*this);
}

int MegaChatPresenceConfigPrivate::getOnlineStatus() const
{
    return status;
}

bool MegaChatPresenceConfigPrivate::isAutoawayEnabled() const
{
    return autoawayEnabled;
}

int64_t MegaChatPresenceConfigPrivate::getAutoawayTimeout() const
{
    return autoawayTimeout;
}

bool MegaChatPresenceConfigPrivate::isPersist() const
{
    return persistEnabled;
}

bool MegaChatPresenceConfigPrivate::isPending() const
{
    return pending;
}

bool MegaChatPresenceConfigPrivate::isSignalActivityRequired() const
{
    return (!persistEnabled
            && status != MegaChatApi::STATUS_OFFLINE
            && status != MegaChatApi::STATUS_AWAY
            && autoawayEnabled && autoawayTimeout);
}

MegaChatAttachedUser::MegaChatAttachedUser(MegaChatHandle contactId, const std::string &email, const std::string& name)
    : mHandle(contactId)
    , mEmail(email)
    , mName(name)
{
}

MegaChatAttachedUser::~MegaChatAttachedUser()
{
}

MegaChatHandle MegaChatAttachedUser::getHandle() const
{
    return mHandle;
}

const char *MegaChatAttachedUser::getEmail() const
{
    return mEmail.c_str();
}

const char *MegaChatAttachedUser::getName() const
{
    return mName.c_str();
}

std::vector<int32_t> DataTranslation::b_to_vector(const std::string& data)
{
    int length = data.length();
    std::vector<int32_t> vector(length / sizeof(int32_t));

    for (int i = 0; i < length; ++i)
    {
        // i >> 2 = i / 4
        vector[i >> 2] |= (data[i] & 255) << (24 - (i & 3) * 8);
    }

    return vector;
}

std::string DataTranslation::vector_to_b(std::vector<int32_t> vector)
{
    int length = vector.size() * sizeof(int32_t);
    char* data = new char[length];

    for (int i = 0; i < length; ++i)
    {
        // i >> 2 = i / 4
        data[i] = (vector[i >> 2] >> (24 - (i & 3) * 8)) & 255;
    }

    std::string dataToReturn(data, length);

    delete[] data;

    return dataToReturn;
}

const char *JSonUtils::generateAttachNodeJSon(MegaNodeList *nodes)
{
    if (!nodes)
    {
        return NULL;
    }

    rapidjson::Document jSonAttachmentNodes(rapidjson::kArrayType);
    for (int i = 0; i < nodes->size(); ++i)
    {
        rapidjson::Value jsonNode(rapidjson::kObjectType);

        MegaNode *megaNode = nodes->get(i);

        if (megaNode == NULL)
        {
            API_LOG_ERROR("Invalid node at index %d", i);
            return NULL;
        }

        // h -> handle
        char *base64Handle = MegaApi::handleToBase64(megaNode->getHandle());
        std::string handleString(base64Handle);
        delete [] base64Handle;
        rapidjson::Value nodeHandleValue(rapidjson::kStringType);
        nodeHandleValue.SetString(handleString.c_str(), handleString.length(), jSonAttachmentNodes.GetAllocator());
        jsonNode.AddMember(rapidjson::Value("h"), nodeHandleValue, jSonAttachmentNodes.GetAllocator());

        // k -> binary key
        char tempKey[FILENODEKEYLENGTH];
        char *base64Key = megaNode->getBase64Key();
        Base64::atob(base64Key, (byte*)tempKey, FILENODEKEYLENGTH);
        delete base64Key;

        std::vector<int32_t> keyVector = DataTranslation::b_to_vector(std::string(tempKey, FILENODEKEYLENGTH));
        rapidjson::Value keyVectorNode(rapidjson::kArrayType);
        if (keyVector.size() != 8)
        {
            API_LOG_ERROR("Invalid nodekey for attached node: %d", megaNode->getHandle());
            return NULL;
        }
        for (unsigned int j = 0; j < keyVector.size(); ++j)
        {
            keyVectorNode.PushBack(rapidjson::Value(keyVector[j]), jSonAttachmentNodes.GetAllocator());
        }

        jsonNode.AddMember(rapidjson::Value("k"), keyVectorNode, jSonAttachmentNodes.GetAllocator());

        // t -> type
        jsonNode.AddMember(rapidjson::Value("t"), rapidjson::Value(megaNode->getType()), jSonAttachmentNodes.GetAllocator());

        // name -> name
        std::string nameString = std::string(megaNode->getName());
        rapidjson::Value nameValue(rapidjson::kStringType);
        nameValue.SetString(nameString.c_str(), nameString.length(), jSonAttachmentNodes.GetAllocator());
        jsonNode.AddMember(rapidjson::Value("name"), nameValue, jSonAttachmentNodes.GetAllocator());

        // s -> size
        jsonNode.AddMember(rapidjson::Value("s"), rapidjson::Value(megaNode->getSize()), jSonAttachmentNodes.GetAllocator());

        // hash -> fingerprint
        const char *fingerprint = megaNode->getFingerprint();
        if (fingerprint)
        {
            rapidjson::Value fpValue(rapidjson::kStringType);
            fpValue.SetString(fingerprint, strlen(fingerprint), jSonAttachmentNodes.GetAllocator());
            jsonNode.AddMember(rapidjson::Value("hash"), fpValue, jSonAttachmentNodes.GetAllocator());
        }

        // fa -> image thumbnail/preview/mediainfo
        const char *fa = megaNode->getFileAttrString();
        if (fa)
        {
            std::string faString(fa);
            delete [] fa;

            rapidjson::Value faValue(rapidjson::kStringType);
            faValue.SetString(faString.c_str(), faString.length(), jSonAttachmentNodes.GetAllocator());
            jsonNode.AddMember(rapidjson::Value("fa"), faValue, jSonAttachmentNodes.GetAllocator());
        }
        else
        {
            // ar -> empty
            rapidjson::Value arValue(rapidjson::kObjectType);
            jsonNode.AddMember(rapidjson::Value("ar"), arValue, jSonAttachmentNodes.GetAllocator());
        }

        // ts -> time stamp
        jsonNode.AddMember(rapidjson::Value("ts"), rapidjson::Value(megaNode->getModificationTime()), jSonAttachmentNodes.GetAllocator());

        jSonAttachmentNodes.PushBack(jsonNode, jSonAttachmentNodes.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    jSonAttachmentNodes.Accept(writer);

    return MegaApi::strdup(buffer.GetString());
}

MegaNodeList *JSonUtils::parseAttachNodeJSon(const char *json)
{
    if (!json || strcmp(json, "") == 0)
    {
        API_LOG_ERROR("Invalid attachment JSON");
        return NULL;
    }

    rapidjson::StringStream stringStream(json);
    rapidjson::Document document;
    document.ParseStream(stringStream);

    MegaNodeList *megaNodeList = new MegaNodeListPrivate();

    int attachmentNumber = document.Capacity();
    for (int i = 0; i < attachmentNumber; ++i)
    {
        const rapidjson::Value& file = document[i];

        rapidjson::Value::ConstMemberIterator iteratorHandle = file.FindMember("h");
        if (iteratorHandle == file.MemberEnd() || !iteratorHandle->value.IsString())
        {
            API_LOG_ERROR("Invalid nodehandle in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        std::string handleString = iteratorHandle->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorName = file.FindMember("name");
        if (iteratorName == file.MemberEnd() || !iteratorName->value.IsString())
        {
            API_LOG_ERROR("Invalid filename in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        std::string nameString = iteratorName->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorKey = file.FindMember("k");
        if (!iteratorKey->value.IsArray())
        {
            iteratorKey = file.FindMember("key");
        }
        if (iteratorKey == file.MemberEnd() || !iteratorKey->value.IsArray()
                || iteratorKey->value.Capacity() != 8)
        {
            API_LOG_ERROR("Invalid nodekey in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        std::vector<int32_t> kElements;
        for (unsigned int j = 0; j < iteratorKey->value.Capacity(); ++j)
        {
            if (iteratorKey->value[j].IsInt())
            {
                int32_t value = iteratorKey->value[j].GetInt();
                kElements.push_back(value);
            }
            else
            {
                API_LOG_ERROR("Invalid nodekey data in attachment JSON");
                delete megaNodeList;
                return NULL;
            }
        }

        rapidjson::Value::ConstMemberIterator iteratorSize = file.FindMember("s");
        if (iteratorSize == file.MemberEnd() || !iteratorSize->value.IsInt64())
        {
            API_LOG_ERROR("Invalid size in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int64_t size = iteratorSize->value.GetInt64();

        rapidjson::Value::ConstMemberIterator iteratorFp = file.FindMember("hash");
        std::string fp;
        if (iteratorFp == file.MemberEnd() || !iteratorFp->value.IsString())
        {
            API_LOG_WARNING("Missing fingerprint in attachment JSON. Old message?");
        }
        else
        {
            fp = iteratorFp->value.GetString();
        }

        rapidjson::Value::ConstMemberIterator iteratorType = file.FindMember("t");
        if (iteratorType == file.MemberEnd() || !iteratorType->value.IsInt())
        {
            API_LOG_ERROR("Invalid type in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int type = iteratorType->value.GetInt();

        rapidjson::Value::ConstMemberIterator iteratorTimeStamp = file.FindMember("ts");
        if (iteratorTimeStamp == file.MemberEnd() || !iteratorTimeStamp->value.IsInt64())
        {
            API_LOG_ERROR("Invalid timestamp in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int64_t timeStamp = iteratorTimeStamp->value.GetInt64();

        rapidjson::Value::ConstMemberIterator iteratorFa = file.FindMember("fa");
        std::string fa;
        if (iteratorFa != file.MemberEnd() && iteratorFa->value.IsString())
        {
            fa = iteratorFa->value.GetString();
        }

        MegaHandle megaHandle = MegaApi::base64ToHandle(handleString.c_str());
        std::string attrstring;
        const char* fingerprint = !fp.empty() ? fp.c_str() : NULL;

        std::string key = DataTranslation::vector_to_b(kElements);

        MegaNodePrivate node(nameString.c_str(), type, size, timeStamp, timeStamp,
                             megaHandle, &key, &attrstring, &fa, fingerprint, INVALID_HANDLE,
                             NULL, NULL, false, true);

        megaNodeList->addNode(&node);
    }

    return megaNodeList;
}

std::vector<MegaChatAttachedUser> *JSonUtils::parseAttachContactJSon(const char *json)
{
    if (!json  || strcmp(json, "") == 0)
    {
        return NULL;
    }

    rapidjson::StringStream stringStream(json);

    rapidjson::Document document;
    document.ParseStream(stringStream);

    std::vector<MegaChatAttachedUser> *megaChatUsers = new std::vector<MegaChatAttachedUser>();

    int contactNumber = document.Capacity();
    for (int i = 0; i < contactNumber; ++i)
    {
        const rapidjson::Value& user = document[i];

        rapidjson::Value::ConstMemberIterator iteratorEmail = user.FindMember("email");
        if (iteratorEmail == user.MemberEnd() || !iteratorEmail->value.IsString())
        {
            API_LOG_ERROR("Invalid email in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string emailString = iteratorEmail->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorHandle = user.FindMember("u");
        if (iteratorHandle == user.MemberEnd() || !iteratorHandle->value.IsString())
        {
            API_LOG_ERROR("Invalid userhandle in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string handleString = iteratorHandle->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorName = user.FindMember("name");
        if (iteratorName == user.MemberEnd() || !iteratorName->value.IsString())
        {
            API_LOG_ERROR("Invalid username in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string nameString = iteratorName->value.GetString();

        MegaChatAttachedUser megaChatUser(MegaApi::base64ToUserHandle(handleString.c_str()) , emailString, nameString);
        megaChatUsers->push_back(megaChatUser);
    }

    return megaChatUsers;

}

string JSonUtils::getLastMessageContent(const string& content, uint8_t type)
{
    std::string messageContents;
    switch (type)
    {
        case MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
        {
            // Remove the first two characters. [0] = 0x0 | [1] = Message::kMsgContact
            std::string messageAttach = content;
            messageAttach.erase(messageAttach.begin(), messageAttach.begin() + 2);

            std::vector<MegaChatAttachedUser> *userVector = JSonUtils::parseAttachContactJSon(messageAttach.c_str());
            if (userVector && userVector->size() > 0)
            {
                for (unsigned int i = 0; i < userVector->size() - 1; ++i)
                {
                    messageContents.append(userVector->at(i).getName());
                    // We use character 0x01 as separator
                    messageContents.push_back(0x01);
                }

                messageContents.append(userVector->at(userVector->size() - 1).getName());
            }

            delete userVector;

            break;
        }
        case MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
            // Remove the first two characters. [0] = 0x0 | [1] = Message::kMsgAttachment
            std::string messageAttach = content;
            messageAttach.erase(messageAttach.begin(), messageAttach.begin() + 2);

            MegaNodeList *megaNodeList = JSonUtils::parseAttachNodeJSon(messageAttach.c_str());
            if (megaNodeList && megaNodeList->size() > 0)
            {
                for (int i = 0; i < megaNodeList->size() - 1; ++i)
                {
                    messageContents.append(megaNodeList->get(i)->getName());
                    // We use character 0x01 as separator
                    messageContents.push_back(0x01);
                }

                messageContents.append(megaNodeList->get(megaNodeList->size() - 1)->getName());
            }

            delete megaNodeList;

            break;
        }
        default:
        {
            messageContents = content;
            break;
        }
    }

    return messageContents;
}

