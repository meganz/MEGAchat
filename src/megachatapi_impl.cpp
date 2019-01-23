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

    delete request;
    for (auto it = chatPeerListItemHandler.begin(); it != chatPeerListItemHandler.end(); it++)
    {
        delete *it;
    }
    for (auto it = chatGroupListItemHandler.begin(); it != chatGroupListItemHandler.end(); it++)
    {
        delete *it;
    }
    for (auto it = chatRoomHandler.begin(); it != chatRoomHandler.end(); it++)
    {
        delete it->second;
    }
    for (auto it = nodeHistoryHandlers.begin(); it != nodeHistoryHandlers.end(); it++)
    {
        delete it->second;
    }

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
    this->websocketsIO = new MegaWebsocketsIO(&sdkMutex, waiter, megaApi, this);

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
    sdkMutex.lock();
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

#ifndef KARERE_DISABLE_WEBRTC
    rtcModule::globalCleanup();
#endif
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
            .fail([request, this](const ::promise::Error& e)
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(e.msg(), e.code(), e.type());
                fireOnChatRequestFinish(request, megaChatError);
            });

            break;
        }
        case MegaChatRequest::TYPE_DISCONNECT:
        {
            // mClient->disconnect();   --> obsolete
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_ACCESS);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS:
        {
            bool disconnect = request->getFlag();
            mClient->retryPendingConnections(disconnect);

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_SEND_TYPING_NOTIF:
        {
            MegaChatHandle chatid = request->getChatHandle();
            ChatRoom *chatroom = findChatRoom(chatid);
            if (chatroom)
            {
                if (request->getFlag())
                {
                    chatroom->sendTypingNotification();
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                }
                else
                {
                    chatroom->sendStopTypingNotification();
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                }
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

            if (timeout > presenced::Config::kMaxAutoawayTimeout)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

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
        case MegaChatRequest::TYPE_SET_LAST_GREEN_VISIBLE:
        {
            bool enable = request->getFlag();
            mClient->presenced().setLastGreenVisible(enable);
            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);

            break;
        }
        case MegaChatRequest::TYPE_LAST_GREEN:
        {
            MegaChatHandle userid = request->getUserHandle();
            mClient->presenced().requestLastGreen(userid);
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

                for (auto it = chatRoomHandler.begin(); it != chatRoomHandler.end(); it++)
                {
                    delete it->second;
                }
                chatRoomHandler.clear();

                for (auto it = nodeHistoryHandlers.begin(); it != nodeHistoryHandlers.end(); it++)
                {
                    delete it->second;
                }
                nodeHistoryHandlers.clear();

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

            mClient->setPresence(request->getNumber())
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const ::promise::Error& err)
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
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            bool group = request->getFlag();
            const userpriv_vector *userpriv = ((MegaChatPeerListPrivate*)peersList)->getList();
            if (!userpriv)
            {
                errorCode = MegaChatError::ERROR_ACCESS;
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
                .fail([request,this](const ::promise::Error& err)
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
                    errorCode = MegaChatError::ERROR_NOENT;
                    break;
                }
                it->second->createChatRoom()
                .then([request,this](ChatRoom* room)
                {
                    request->setChatHandle(room->chatid());

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                    fireOnChatRequestFinish(request, megaChatError);
                })
                .fail([request,this](const ::promise::Error& err)
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
                errorCode = MegaChatError::ERROR_NOENT;
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
            .fail([request, this](const ::promise::Error& err)
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
                errorCode = MegaChatError::ERROR_NOENT;
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
            .fail([request, this](const ::promise::Error& err)
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
                errorCode = MegaChatError::ERROR_NOENT;
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
                .fail([request, this](const ::promise::Error& err)
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
                .fail([request, this](const ::promise::Error& err)
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
            .fail([request, this](const ::promise::Error& err)
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
            if (chatid == MEGACHAT_INVALID_HANDLE || title == NULL || !strlen(title))
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
            .fail([request, this](const ::promise::Error& err)
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
                string firstname = string(data->buf(), data->dataSize());
                request->setText(firstname.c_str());
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const ::promise::Error& err)
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
            .fail([request, this](const ::promise::Error& err)
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
            .fail([request, this](const ::promise::Error& err)
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
            bool isVoiceMessage = (request->getParamType() == 1);
            if (chatid == MEGACHAT_INVALID_HANDLE
                    || ((!nodeList || !nodeList->size()) && (h == MEGACHAT_INVALID_HANDLE))
                    || (isVoiceMessage && h == MEGACHAT_INVALID_HANDLE))
            {
                errorCode = MegaChatError::ERROR_NOENT;
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

            uint8_t msgType = Message::kMsgInvalid;
            switch (request->getParamType())
            {
                case 0: // regular attachment
                    msgType = Message::kMsgAttachment;
                    break;

                case 1:  // voice-message
                    msgType = Message::kMsgVoiceClip;
                    break;
            }
            if (msgType == Message::kMsgInvalid)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            string buffer = JSonUtils::generateAttachNodeJSon(nodeList, msgType);
            if (buffer.empty())
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            chatroom->requesGrantAccessToNodes(nodeList)
            .then([this, request, buffer, msgType]()
            {
                int errorCode = MegaChatError::ERROR_ARGS;
                MegaChatMessage *msg = sendMessage(request->getChatHandle(), buffer.c_str(), buffer.size(), msgType);
                if (msg)
                {
                    request->setMegaChatMessage(msg);
                    errorCode = MegaChatError::ERROR_OK;
                }

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(errorCode);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([this, request, buffer, msgType](const ::promise::Error& err)
            {
                MegaChatErrorPrivate *megaChatError = NULL;
                if (err.code() == MegaChatError::ERROR_EXIST)
                {
                    API_LOG_WARNING("Already granted access to this node previously");

                    int errorCode = MegaChatError::ERROR_ARGS;
                    MegaChatMessage *msg = sendMessage(request->getChatHandle(), buffer.c_str(), buffer.size(), msgType);
                    if (msg)
                    {
                        request->setMegaChatMessage(msg);
                        errorCode = MegaChatError::ERROR_OK;
                    }

                    megaChatError = new MegaChatErrorPrivate(errorCode);
                }
                else
                {
                    megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                    API_LOG_ERROR("Failed to grant access to some nodes");
                }

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
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            ::promise::Promise<void> promise = chatroom->requestRevokeAccessToNode(node);
            delete node;

            ::promise::when(promise)
            .then([this, request]()
            {
                std::string buf = Id(request->getUserHandle()).toString();
                buf.insert(buf.begin(), Message::kMsgRevokeAttachment - Message::kMsgOffset);
                buf.insert(buf.begin(), 0x0);

                MegaChatMessage *megaMsg = sendMessage(request->getChatHandle(), buf.c_str(), buf.length());
                request->setMegaChatMessage(megaMsg);

                int errorCode = MegaChatError::ERROR_OK;
                if (!megaMsg)
                {
                    errorCode = MegaChatError::ERROR_ARGS;
                }

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(errorCode);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([this, request](const ::promise::Error& err)
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
        case MegaChatRequest::TYPE_PUSH_RECEIVED:
        {
            MegaChatHandle chatid = request->getChatHandle();
            int type = request->getParamType();
            if (type == 1 && chatid != MEGACHAT_INVALID_HANDLE) // if iOS specifies a chatid, check it's valid
            {
                ChatRoom *room = findChatRoom(chatid);
                if (!room)
                {
                    megaApi->sendEvent(99006, "iOS PUSH received for non-existing chatid");

                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_NOENT);
                    fireOnChatRequestFinish(request, megaChatError);
                    return;
                }
                else if (room->isArchived()) // don't want to generate notifications for archived chats
                {
                    MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_ACCESS);
                    fireOnChatRequestFinish(request, megaChatError);
                    return;
                }
            }

            mClient->pushReceived(chatid)
            .then([this, request]()
            {
                int type = request->getParamType();
                if (type == 0)  // Android
                {
                    // for Android, we prepare a list of msgids for every chatid that are candidates for
                    // notifications. Android doesn't really know why they received a push, so the previous
                    // notifications are cleanup and the new set of messages are notified

                    MegaHandleList *chatids = MegaHandleList::createInstance();

                    // for each chatroom, load all unread messages)
                    for (auto it = mClient->chats->begin(); it != mClient->chats->end(); it++)
                    {
                        // don't want to generate notifications for archived chats
                        if (it->second->isArchived())
                            continue;

                        MegaHandleList *msgids = MegaHandleList::createInstance();

                        MegaChatHandle chatid = it->first;
                        const Chat &chat = it->second->chat();
                        Idx lastSeenIdx = chat.lastSeenIdx();

                        // first msg to consider: last-seen if loaded in memory. Otherwise, the oldest loaded msg
                        Idx first = chat.lownum();
                        if (lastSeenIdx != CHATD_IDX_INVALID        // message is known locally
                                && chat.findOrNull(lastSeenIdx))    // message is loaded in RAM
                        {
                            first = lastSeenIdx + 1;
                        }
                        Idx last = chat.highnum();
                        int maxCount = 6;   // do not notify more than 6 messages per chat
                        for (Idx i = last; (i >= first && maxCount > 0); i--)
                        {
                            auto& msg = chat.at(i);
                            if (msg.isValidUnread(mClient->myHandle()))
                            {
                                maxCount--;
                                msgids->addMegaHandle(msg.id());
                            }
                        }

                        if (msgids->size())
                        {
                            chatids->addMegaHandle(chatid);
                            request->setMegaHandleListByChat(chatid, msgids);
                        }

                        delete msgids;
                    }

                    request->setMegaHandleList(chatids);    // always a valid list, even if empty
                    delete chatids;
                }
                //else    // iOS

                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([this, request](const ::promise::Error& err)
            {
                API_LOG_ERROR("Failed to retrieve current state");
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(err.msg(), err.code(), err.type());
                fireOnChatRequestFinish(request, megaChatError);
            });
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
                API_LOG_ERROR("Start call - Chatroom has not been found");
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            if (!chatroom->chat().connection().clientId())
            {
                API_LOG_ERROR("Start call - Refusing start/join a call, clientid no yet assigned by shard: %d", chatroom->chat().connection().shardNo());
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

            MegaChatCallHandler *handler = findChatCallHandler(chatid);
            if (handler && (handler->getCall() || !chatroom->isGroup()))
            {
                // only groupchats allow to join the call in multiple clients, in 1on1 it's not allowed
                API_LOG_ERROR("A call exists in this chatroom and we already participate or it's not a groupchat");
                errorCode = MegaChatError::ERROR_EXIST;
                break;
            }

            bool enableVideo = request->getFlag();
            if (handler)
            {
                assert(handler->callParticipants() > 0);
                if (handler->callParticipants() >= rtcModule::IRtcModule::kMaxCallReceivers)
                {
                    API_LOG_ERROR("Cannot join the call because it reached the maximum number of participants "
                                  "(current: %d, max: %d)", handler->callParticipants(), rtcModule::IRtcModule::kMaxCallReceivers);
                    errorCode = MegaChatError::ERROR_TOOMANY;
                    break;
                }

                MegaChatCallPrivate *chatCall = handler->getMegaChatCall();
                if (!chatCall->availableAudioSlots())   // audio is always enabled by default
                {
                    API_LOG_ERROR("Cannot answer the call because it reached the maximum number of audio senders "
                                  "(max: %d)", rtcModule::IRtcModule::kMaxCallAudioSenders);
                    errorCode = MegaChatError::ERROR_TOOMANY;
                    break;
                }
                if (enableVideo && !chatCall->availableVideoSlots())
                {
                    API_LOG_ERROR("The call reached the maximum number of video senders (%d): video automatically disabled",
                                  rtcModule::IRtcModule::kMaxCallAudioSenders);
                    enableVideo = false;
                    request->setFlag(enableVideo);
                }

                karere::AvFlags newFlags(true, enableVideo);
                chatroom->joinCall(newFlags, *handler, chatCall->getId());
            }
            else
            {
                handler = new MegaChatCallHandler(this);
                mClient->rtc->addCallHandler(chatid, handler);
                karere::AvFlags avFlags(true, enableVideo);
                chatroom->mediaCall(avFlags, *handler);
                handler->getMegaChatCall()->setInitialAudioVideoFlags(avFlags);
                request->setFlag(true);
            }

            MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
            fireOnChatRequestFinish(request, megaChatError);
            break;
        }
        case MegaChatRequest::TYPE_ANSWER_CHAT_CALL:
        {
            MegaChatHandle chatid = request->getChatHandle();
            bool enableVideo = request->getFlag();

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                API_LOG_ERROR("Answer call - Chatroom has not been found");
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            if (!chatroom->chat().connection().clientId())
            {
                API_LOG_ERROR("Answer call - Refusing answer a call, clientid no yet assigned by shard: %d", chatroom->chat().connection().shardNo());
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

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

            if (handler->callParticipants() >= rtcModule::IRtcModule::kMaxCallReceivers)
            {
                API_LOG_ERROR("Cannot answer the call because it reached the maximum number of participants "
                              "(current: %d, max: %d)", handler->callParticipants(), rtcModule::IRtcModule::kMaxCallReceivers);
                errorCode = MegaChatError::ERROR_TOOMANY;
                break;
            }

            MegaChatCallPrivate *chatCall = handler->getMegaChatCall();
            if (!chatCall->availableAudioSlots())   // audio is always enabled by default
            {
                API_LOG_ERROR("Cannot answer the call because it reached the maximum number of audio senders "
                              "(max: %d)", rtcModule::IRtcModule::kMaxCallAudioSenders);
                errorCode = MegaChatError::ERROR_TOOMANY;
                break;
            }
            if (enableVideo && !chatCall->availableVideoSlots())
            {
                API_LOG_ERROR("The call reached the maximum number of video senders (%d): video automatically disabled",
                              rtcModule::IRtcModule::kMaxCallAudioSenders);
                enableVideo = false;
                request->setFlag(enableVideo);
            }

            karere::AvFlags newFlags(true, enableVideo);
            if (!call->answer(newFlags))
            {
                errorCode = MegaChatError::ERROR_ACCESS;
                break;
            }

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

            if (operationType != MegaChatRequest::AUDIO && operationType != MegaChatRequest::VIDEO)
            {
                errorCode = MegaChatError::ERROR_ARGS;
                break;
            }

            if (enable)
            {
                if ((operationType == MegaChatRequest::AUDIO && !chatCall->availableAudioSlots())
                        || (operationType == MegaChatRequest::VIDEO && !chatCall->availableVideoSlots()))
                {
                    API_LOG_ERROR("Cannot enable the A/V because the call doesn't have available A/V slots");
                    errorCode = MegaChatError::ERROR_TOOMANY;
                    break;
                }
            }

            karere::AvFlags currentFlags = call->sentAv();
            karere::AvFlags requestedFlags = currentFlags;
            if (operationType == MegaChatRequest::AUDIO)
            {
                requestedFlags.setAudio(enable);
            }
            else // (operationType == MegaChatRequest::VIDEO)
            {
                requestedFlags.setVideo(enable);
            }

            karere::AvFlags effectiveFlags = call->muteUnmute(requestedFlags);
            chatCall->setLocalAudioVideoFlags(effectiveFlags);
            API_LOG_INFO("Local audio/video flags changed. ChatId: %s, callid: %s, AV: %s --> %s --> %s",
                         call->chat().chatId().toString().c_str(),
                         call->id().toString().c_str(),
                         currentFlags.toString().c_str(),
                         requestedFlags.toString().c_str(),
                         effectiveFlags.toString().c_str());

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
        case MegaChatRequest::TYPE_ARCHIVE_CHATROOM:
        {
            handle chatid = request->getChatHandle();
            bool archive = request->getFlag();
            if (chatid == MEGACHAT_INVALID_HANDLE)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            ChatRoom *chatroom = findChatRoom(chatid);
            if (!chatroom)
            {
                errorCode = MegaChatError::ERROR_NOENT;
                break;
            }

            chatroom->archiveChat(archive)
            .then([request, this]()
            {
                MegaChatErrorPrivate *megaChatError = new MegaChatErrorPrivate(MegaChatError::ERROR_OK);
                fireOnChatRequestFinish(request, megaChatError);
            })
            .fail([request, this](const ::promise::Error& err)
            {
                API_LOG_ERROR("Error archiving chat: %s", err.what());

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
        uint8_t caps = karere::kClientIsMobile | karere::kClientCanWebrtc | kClientSupportLastGreen;
#else
        uint8_t caps = karere::kClientIsMobile | kClientSupportLastGreen;
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
        chatRoomHandler[chatid] = new MegaChatRoomHandler(this, chatApi, chatid);
    }

    return chatRoomHandler[chatid];
}

void MegaChatApiImpl::removeChatRoomHandler(MegaChatHandle chatid)
{
    map<MegaChatHandle, MegaChatRoomHandler*>::iterator it = chatRoomHandler.find(chatid);
    if (it == chatRoomHandler.end())
    {
        API_LOG_WARNING("removeChatRoomHandler: chatroom handler not found (chatid: %s)", karere::Id(chatid).toString().c_str());
        return;
    }

    MegaChatRoomHandler *roomHandler = chatRoomHandler[chatid];
    chatRoomHandler.erase(it);
    delete roomHandler;
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

bool MegaChatApiImpl::hasUrl(const char *text)
{
    std::string url;
    return chatd::Message::hasUrl(text, url);
}

bool MegaChatApiImpl::openNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    if (!listener || chatid == MEGACHAT_INVALID_HANDLE)
    {
        return false;
    }

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        auto it = nodeHistoryHandlers.find(chatid);
        if (it != nodeHistoryHandlers.end())
        {
            sdkMutex.unlock();
            API_LOG_WARNING("openNodeHistory: node history is already open for this chatroom (chatid: %s), close it before open it again", karere::Id(chatid).toString().c_str());
            throw std::runtime_error("App node history handler is already set, remove it first");
            return false;
        }

        MegaChatNodeHistoryHandler *handler = new MegaChatNodeHistoryHandler(chatApi);
        chatroom->chat().setNodeHistoryHandler(handler);
        nodeHistoryHandlers[chatid] = handler;
        handler->addMegaNodeHistoryListener(listener);
    }

    sdkMutex.unlock();
    return chatroom;
}

bool MegaChatApiImpl::closeNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    if (!listener || chatid == MEGACHAT_INVALID_HANDLE)
    {
        return false;
    }

    sdkMutex.lock();
    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        auto it = nodeHistoryHandlers.find(chatid);
        if (it != nodeHistoryHandlers.end())
        {
            MegaChatNodeHistoryHandler *handler = it->second;
            nodeHistoryHandlers.erase(it);
            delete handler;
            chatroom->chat().unsetHandlerToNodeHistory();

            sdkMutex.unlock();
            return true;
        }
    }

    sdkMutex.unlock();
    return false;
}

void MegaChatApiImpl::addNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    if (!listener || chatid == MEGACHAT_INVALID_HANDLE)
    {
        return;
    }

    sdkMutex.lock();
    auto it = nodeHistoryHandlers.find(chatid);
    if (it != nodeHistoryHandlers.end())
    {
        MegaChatNodeHistoryHandler *handler = it->second;
        handler->addMegaNodeHistoryListener(listener);

    }
    else
    {
        assert(false);
        API_LOG_WARNING("addNodeHistoryListener: node history handler not found (chatid: %s)", karere::Id(chatid).toString().c_str());
    }

    sdkMutex.unlock();
}

void MegaChatApiImpl::removeNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    if (!listener || chatid == MEGACHAT_INVALID_HANDLE)
    {
        return;
    }

    sdkMutex.lock();
    auto it = nodeHistoryHandlers.find(chatid);
    if (it != nodeHistoryHandlers.end())
    {
        MegaChatNodeHistoryHandler *handler = it->second;
        handler->removeMegaNodeHistoryListener(listener);

    }
    else
    {
        assert(false);
        API_LOG_WARNING("removeNodeHistoryListener: node history handler not found (chatid: %s)", karere::Id(chatid).toString().c_str());
    }

    sdkMutex.unlock();

}

int MegaChatApiImpl::loadAttachments(MegaChatHandle chatid, int count)
{
    int ret = MegaChatApi::SOURCE_NONE;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        HistSource source = chat.getNodeHistory(count);
        switch (source)
        {
        case kHistSourceNone:   ret = MegaChatApi::SOURCE_NONE; break;
        case kHistSourceRam:
        case kHistSourceDb:     ret = MegaChatApi::SOURCE_LOCAL; break;
        case kHistSourceServer: ret = MegaChatApi::SOURCE_REMOTE; break;
        case kHistSourceNotLoggedIn: ret = MegaChatApi::SOURCE_ERROR; break;
        default:
            API_LOG_ERROR("Unknown source of messages at loadAttachments()");
            break;
        }
    }

    sdkMutex.unlock();
    return ret;
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
    if (call->getId() == Id::inval())
    {
        // if a call have no id yet, it's because we haven't received yet the initial CALLDATA,
        // but just some previous opcodes related to the call, like INCALLs or CALLTIME (which
        // do not include the callid)
        return;
    }

    if (terminating)
    {
        return;
    }

    for (set<MegaChatCallListener *>::iterator it = callListeners.begin(); it != callListeners.end() ; it++)
    {
        (*it)->onChatCallUpdate(chatApi, call);
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_STATUS)
            && (call->getStatus() == MegaChatCall::CALL_STATUS_RING_IN              // for callee, incoming call
                || call->getStatus() == MegaChatCall::CALL_STATUS_USER_NO_PRESENT   // for callee (groupcalls)
                || call->getStatus() == MegaChatCall::CALL_STATUS_REQUEST_SENT      // for caller, outgoing call
                || call->getStatus() == MegaChatCall::CALL_STATUS_DESTROYED))       // call finished
    {
        // notify at MegaChatListItem level about new calls and calls being terminated
        ChatRoom *room = findChatRoom(call->getChatid());
        MegaChatListItemPrivate *item = new MegaChatListItemPrivate(*room);
        item->setCallInProgress();

        fireOnChatListItemUpdate(item);
    }

    call->removeChanges();
}

void MegaChatApiImpl::fireOnChatVideoData(MegaChatHandle chatid, MegaChatHandle peerid, int width, int height, char *buffer)
{
    std::map<MegaChatHandle, MegaChatPeerVideoListener_map>::iterator it = videoListeners.find(chatid);
    if (it != videoListeners.end())
    {
        MegaChatPeerVideoListener_map::iterator peerVideoIterator = it->second.find(peerid);
        if (peerVideoIterator != it->second.end())
        {
            for( MegaChatVideoListener_set::iterator videoListenerIterator = peerVideoIterator->second.begin();
                 videoListenerIterator != peerVideoIterator->second.end();
                 videoListenerIterator++)
            {
                (*videoListenerIterator)->onChatVideoData(chatApi, chatid, width, height, buffer, width * height * 4);
            }
        }
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

void MegaChatApiImpl::fireOnChatPresenceLastGreenUpdated(MegaChatHandle userhandle, int lastGreen)
{
    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatPresenceLastGreen(chatApi, userhandle, lastGreen);
    }
}

void MegaChatApiImpl::fireOnChatConnectionStateUpdate(MegaChatHandle chatid, int newState)
{
    bool allConnected = (newState == MegaChatApi::CHAT_CONNECTION_ONLINE) ? mClient->mChatdClient->areAllChatsLoggedIn() : false;

    for(set<MegaChatListener *>::iterator it = listeners.begin(); it != listeners.end() ; it++)
    {
        (*it)->onChatConnectionStateUpdate(chatApi, chatid, newState);

        if (allConnected)
        {
            (*it)->onChatConnectionStateUpdate(chatApi, MEGACHAT_INVALID_HANDLE, newState);
        }
    }
}

void MegaChatApiImpl::fireOnChatNotification(MegaChatHandle chatid, MegaChatMessage *msg)
{
    for(set<MegaChatNotificationListener *>::iterator it = notificationListeners.begin(); it != notificationListeners.end() ; it++)
    {
        (*it)->onChatNotification(chatApi, chatid, msg);
    }

    delete msg;
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

bool MegaChatApiImpl::areAllChatsLoggedIn()
{
    sdkMutex.lock();
    bool ret = mClient->mChatdClient->areAllChatsLoggedIn();
    sdkMutex.unlock();

    return ret;
}

void MegaChatApiImpl::retryPendingConnections(bool disconnect, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS, listener);
    request->setFlag(disconnect);
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

void MegaChatApiImpl::setLastGreenVisible(bool enable, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SET_LAST_GREEN_VISIBLE, listener);
    request->setFlag(enable);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::requestLastGreen(MegaChatHandle userid, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_LAST_GREEN, listener);
    request->setUserHandle(userid);
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

    int status = mClient ? mClient->ownPresence().status() : (int)MegaChatApi::STATUS_INVALID;

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

int MegaChatApiImpl::getBackgroundStatus()
{
    int status = -1;

    sdkMutex.lock();

    if (mClient && mClient->mChatdClient)
    {
        status = (mClient->mChatdClient->keepaliveType() == chatd::OP_KEEPALIVE) ? 0 : 1;
    }

    sdkMutex.unlock();

    return status;
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
            if (!it->second->isArchived())
            {
                items->addChatListItem(new MegaChatListItemPrivate(*it->second));
            }
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatListItemList *MegaChatApiImpl::getChatListItemsByPeers(MegaChatPeerList *peers)
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            bool sameParticipants = true;
            if (it->second->isGroup())
            {
                GroupChatRoom *chatroom = (GroupChatRoom*) it->second;
                if ((int)chatroom->peers().size() != peers->size())
                {
                    continue;
                }

                for (int i = 0; i < peers->size(); i++)
                {
                    // if the peer in the list is part of the members in the chatroom...
                    MegaChatHandle uh = peers->getPeerHandle(i);
                    if (chatroom->peers().find(uh) == chatroom->peers().end())
                    {
                        sameParticipants = false;
                        break;
                    }
                }
                if (sameParticipants == true)
                {
                    items->addChatListItem(new MegaChatListItemPrivate(*it->second));
                }

            }
            else    // 1on1
            {
                if (peers->size() != 1)
                {
                    continue;
                }

                PeerChatRoom *chatroom = (PeerChatRoom*) it->second;
                if (chatroom->peer() == peers->getPeerHandle(0))
                {
                    items->addChatListItem(new MegaChatListItemPrivate(*it->second));
                }
            }
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
            if (!room->isArchived() && room->chat().unreadMsgCount())
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
            if (!it->second->isArchived() && it->second->isActive())
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
            if (!it->second->isArchived() && !it->second->isActive())
            {
                items->addChatListItem(new MegaChatListItemPrivate(*it->second));
            }
        }
    }

    sdkMutex.unlock();

    return items;
}

MegaChatListItemList *MegaChatApiImpl::getArchivedChatListItems()
{
    MegaChatListItemListPrivate *items = new MegaChatListItemListPrivate();

    sdkMutex.lock();

    if (mClient && !terminating)
    {
        ChatRoomList::iterator it;
        for (it = mClient->chats->begin(); it != mClient->chats->end(); it++)
        {
            if (it->second->isArchived())
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
            if (!room->isArchived() && room->chat().unreadMsgCount())
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

void MegaChatApiImpl::archiveChat(MegaChatHandle chatid, bool archive, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ARCHIVE_CHATROOM, listener);
    request->setChatHandle(chatid);
    request->setFlag(archive);
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
        addChatRoomListener(chatid, listener);
        chatroom->setAppChatHandler(getChatRoomHandler(chatid));
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
    }
    else
    {
        API_LOG_WARNING("Try to close nonexistent chatroom: %s", karere::Id(chatid).toString().c_str());
    }

    removeChatRoomListener(chatid, listener);
    removeChatRoomHandler(chatid);

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
        case kHistSourceNotLoggedIn: ret = MegaChatApi::SOURCE_ERROR; break;
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

MegaChatMessage *MegaChatApiImpl::getMessageFromNodeHistory(MegaChatHandle chatid, MegaChatHandle msgid)
{
    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Message *msg = chat.getMessageFromNodeHistory(msgid);
        if (msg)
        {
            Idx idx = chat.getIdxFromNodeHistory(msgid);
            assert(idx != CHATD_IDX_INVALID);
            Message::Status status = (msg->userid == mClient->myHandle()) ? Message::Status::kServerReceived : Message::Status::kSeen;
            megaMsg = new MegaChatMessagePrivate(*msg, status, idx);
        }
        else
        {
            API_LOG_ERROR("Failed to find message at node history (id: %d)",  msgid);
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

MegaChatMessage *MegaChatApiImpl::sendMessage(MegaChatHandle chatid, const char *msg, size_t msgLen, int type)
{
    if (!msg)
    {
        return NULL;
    }

    if (type == Message::kMsgNormal)
    {
        // remove ending carrier-returns
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
        Message *m = chatroom->chat().msgSubmit(msg, msgLen, type, NULL);

        if (!m)
        {
            sdkMutex.unlock();
            return NULL;
        }
        megaMsg = new MegaChatMessagePrivate(*m, Message::Status::kSending, CHATD_IDX_INVALID);
    }

    sdkMutex.unlock();
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::attachContacts(MegaChatHandle chatid, MegaHandleList *contacts)
{
    if (!mClient)
    {
        return NULL;
    }

    sdkMutex.lock();

    string buf = JSonUtils::generateAttachContactJSon(contacts, mClient->contactList.get());
    MegaChatMessage *megaMsg = sendMessage(chatid, buf.c_str(), buf.size(), Message::kMsgContact);

    sdkMutex.unlock();

    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::forwardContact(MegaChatHandle sourceChatid, MegaChatHandle msgid, MegaChatHandle targetChatId)
{
    if (!mClient || sourceChatid == MEGACHAT_INVALID_HANDLE || msgid == MEGACHAT_INVALID_HANDLE || targetChatId == MEGACHAT_INVALID_HANDLE)
    {
        return NULL;
    }

    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroomTarget = findChatRoom(targetChatId);
    ChatRoom *chatroomSource = findChatRoom(sourceChatid);
    if (chatroomSource && chatroomTarget)
    {
        chatd::Chat &chat = chatroomSource->chat();
        Idx idx =  chat.msgIndexFromId(msgid);
        chatd::Message *msg = chatroomSource->chat().findOrNull(idx);
        if (msg && msg->type == chatd::Message::kMsgContact)
        {
            std::string contactMsg;
            unsigned char zero = 0x0;
            unsigned char contactType = Message::kMsgContact - Message::kMsgOffset;
            contactMsg.push_back(zero);
            contactMsg.push_back(contactType);
            contactMsg.append(msg->toText());
            Message *m = chatroomTarget->chat().msgSubmit(contactMsg.c_str(), contactMsg.length(), Message::kMsgContact, NULL);
            if (!m)
            {
                sdkMutex.unlock();
                return NULL;
            }
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
    request->setParamType(0);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(nodehandle);
    request->setParamType(0);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::attachVoiceMessage(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE, listener);
    request->setChatHandle(chatid);
    request->setUserHandle(nodehandle);
    request->setParamType(1);
    requestQueue.push(request);
    waiter->notify();
}

MegaChatMessage * MegaChatApiImpl::sendGeolocation(MegaChatHandle chatid, float longitude, float latitude, const char *img)
{
    string buf = JSonUtils::generateGeolocationJSon(longitude, latitude, img);
    MegaChatMessage *megaMsg = sendMessage(chatid, buf.c_str(), buf.size(), Message::kMsgContainsMeta);
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::editGeolocation(MegaChatHandle chatid, MegaChatHandle msgid, float longitude, float latitude, const char *img)
{
    string buf = JSonUtils::generateGeolocationJSon(longitude, latitude, img);
    MegaChatMessage *megaMsg = editMessage(chatid, msgid, buf.c_str(), buf.size());
    return megaMsg;
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

MegaChatMessage *MegaChatApiImpl::editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char *msg, size_t msgLen)
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
            unsigned char newtype = (originalMsg->containMetaSubtype() == Message::ContainsMetaSubType::kRichLink)
                    ? (unsigned char) Message::kMsgNormal
                    : originalMsg->type;

            if (msg && newtype == Message::kMsgNormal)    // actually not deletion, but edit
            {
                // remove ending carrier-returns
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

            const Message *editedMsg = chatroom->chat().msgModify(*originalMsg, msg, msgLen, NULL, newtype);
            if (editedMsg)
            {
                megaMsg = new MegaChatMessagePrivate(*editedMsg, Message::kSending, index);
            }
        }
    }

    sdkMutex.unlock();
    return megaMsg;
}

MegaChatMessage *MegaChatApiImpl::removeRichLink(MegaChatHandle chatid, MegaChatHandle msgid)
{
    MegaChatMessagePrivate *megaMsg = NULL;
    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        Message *originalMsg = findMessage(chatid, msgid);
        if (!originalMsg || originalMsg->containMetaSubtype() != Message::ContainsMetaSubType::kRichLink)
        {
            sdkMutex.unlock();
            return NULL;
        }

        uint8_t containsMetaType = originalMsg->containMetaSubtype();
        std::string containsMetaJson = originalMsg->containsMetaJson();
        const MegaChatContainsMeta *containsMeta = JSonUtils::parseContainsMeta(containsMetaJson.c_str(), containsMetaType);
        if (!containsMeta || containsMeta->getType() != MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW)
        {
            delete containsMeta;
            sdkMutex.unlock();
            return NULL;
        }

        const char *msg = containsMeta->getRichPreview()->getText();
        assert(msg);
        string content = msg ? msg : "";

        const Message *editedMsg = chatroom->chat().removeRichLink(*originalMsg, content);
        if (editedMsg)
        {
            Idx index = chat.msgIndexFromId(msgid);
            megaMsg = new MegaChatMessagePrivate(*editedMsg, Message::kSending, index);
        }

        delete containsMeta;
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

MegaChatHandle MegaChatApiImpl::getLastMessageSeenId(MegaChatHandle chatid)
{
    MegaChatHandle lastMessageSeenId = MEGACHAT_INVALID_HANDLE;

    sdkMutex.lock();

    ChatRoom *chatroom = findChatRoom(chatid);
    if (chatroom)
    {
        Chat &chat = chatroom->chat();
        lastMessageSeenId = chat.lastSeenId();
    }

    sdkMutex.unlock();

    return lastMessageSeenId;
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
    request->setFlag(true);
    requestQueue.push(request);
    waiter->notify();
}

void MegaChatApiImpl::sendStopTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_SEND_TYPING_NOTIF, listener);
    request->setChatHandle(chatid);
    request->setFlag(false);
    requestQueue.push(request);
    waiter->notify();
}

bool MegaChatApiImpl::isMessageReceptionConfirmationActive() const
{
    return mClient ? mClient->mChatdClient->isMessageReceivedConfirmationActive() : false;
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

void MegaChatApiImpl::pushReceived(bool beep, MegaChatHandle chatid, int type, MegaChatRequestListener *listener)
{
    MegaChatRequestPrivate *request = new MegaChatRequestPrivate(MegaChatRequest::TYPE_PUSH_RECEIVED, listener);
    request->setFlag(beep);
    request->setChatHandle(chatid);
    request->setParamType(type);
    requestQueue.push(request);
    waiter->notify();
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

void MegaChatApiImpl::setIgnoredCall(MegaChatHandle chatId)
{
    if (!mClient->rtc)
    {
        API_LOG_ERROR("Ignore call - WebRTC is not initialized");
        return;
    }

    if (chatId != MEGACHAT_INVALID_HANDLE)
    {
        MegaChatCallHandler *handler = findChatCallHandler(chatId);
        if (!handler)
        {
            API_LOG_ERROR("Ignore call - Failed to get the call handler associated to chat room");
            return;
        }

        MegaChatCallPrivate *chatCall = handler->getMegaChatCall();
        if (!chatCall)
        {
            API_LOG_ERROR("Ignore call - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
            assert(false);
            return;
        }

        chatCall->setIgnoredCall(true);
     }
}

MegaChatCall *MegaChatApiImpl::getChatCall(MegaChatHandle chatId)
{
    MegaChatCall *chatCall = NULL;

    sdkMutex.lock();
    MegaChatCallHandler *handler = findChatCallHandler(chatId);
    if (handler)
    {
        chatCall = handler->getMegaChatCall();
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
        API_LOG_ERROR("MegaChatApiImpl::getChatCall - There aren't any calls at this chatroom");
    }

    sdkMutex.unlock();
    return chatCall;
}

MegaChatCall *MegaChatApiImpl::getChatCallByCallId(MegaChatHandle callId)
{
    MegaChatCall *chatCall = NULL;

    sdkMutex.lock();

    MegaHandleList *calls = getChatCalls();
    for (unsigned int i = 0; i < calls->size(); i++)
    {
        karere::Id chatId = calls->get(i);
        MegaChatCall *call = getChatCall(chatId);
        if (call && call->getId() == callId)
        {
            chatCall =  call;
            break;
        }
        else
        {
            delete call;
        }
    }

    delete calls;

    sdkMutex.unlock();
    return chatCall;
}

int MegaChatApiImpl::getNumCalls()
{
    int numCalls = 0;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        numCalls = mClient->rtc->numCalls();
    }
    sdkMutex.unlock();

    return numCalls;
}

MegaHandleList *MegaChatApiImpl::getChatCalls()
{
    MegaHandleListPrivate *callList = new MegaHandleListPrivate();

    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        std::vector<karere::Id> chatids = mClient->rtc->chatsWithCall();
        for (unsigned int i = 0; i < chatids.size(); i++)
        {
            callList->addMegaHandle(chatids[i]);
        }
    }

    sdkMutex.unlock();
    return callList;
}

MegaHandleList *MegaChatApiImpl::getChatCallsIds()
{
    MegaHandleListPrivate *callList = new MegaHandleListPrivate();

    sdkMutex.lock();

    MegaHandleList *chatids = getChatCalls();
    for (unsigned int i = 0; i < chatids->size(); i++)
    {
        karere::Id chatId = chatids->get(i);
        MegaChatCall *call = getChatCall(chatId);
        if (call)
        {
            callList->addMegaHandle(call->getId());
            delete call;
        }
    }

    delete chatids;

    sdkMutex.unlock();
    return callList;
}

bool MegaChatApiImpl::hasCallInChatRoom(MegaChatHandle chatid)
{
    bool hasCall = false;
    sdkMutex.lock();

    if (mClient && mClient->rtc)
    {
        hasCall = mClient->rtc->findCallHandler(chatid);
    }

    sdkMutex.unlock();
    return hasCall;
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

void MegaChatApiImpl::enableGroupChatCalls(bool enable)
{
    sdkMutex.lock();
    if (mClient)
    {
        mClient->enableGroupCalls(enable);
    }
    else
    {
        API_LOG_ERROR("MegaChatApiImpl::enableGroupChatCalls - Client is not initialized");
        assert(false);
    }
    sdkMutex.unlock();
}

bool MegaChatApiImpl::areGroupChatCallEnabled()
{
    sdkMutex.lock();
    bool enabledGroupCalls = false;
    if (mClient)
    {
        enabledGroupCalls = mClient->areGroupCallsEnabled();
    }
    else
    {
        API_LOG_ERROR("MegaChatApiImpl::areGroupChatCallEnabled - Client is not initialized");
    }

    sdkMutex.unlock();

    return enabledGroupCalls;
}

int MegaChatApiImpl::getMaxCallParticipants()
{
    return rtcModule::IRtcModule::kMaxCallReceivers;
}

int MegaChatApiImpl::getMaxVideoCallParticipants()
{
    return rtcModule::IRtcModule::kMaxCallVideoSenders;
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

void MegaChatApiImpl::addChatNotificationListener(MegaChatNotificationListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    notificationListeners.insert(listener);
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

void MegaChatApiImpl::addChatVideoListener(MegaChatHandle chatid, MegaChatHandle peerid, MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    videoListeners[chatid][peerid].insert(listener);
    videoMutex.unlock();
}

void MegaChatApiImpl::removeChatVideoListener(MegaChatHandle chatid, MegaChatHandle peerid, MegaChatVideoListener *listener)
{
    if (!listener)
    {
        return;
    }

    videoMutex.lock();
    videoListeners[chatid][peerid].erase(listener);

    if (videoListeners[chatid][peerid].empty())
    {
        videoListeners[chatid].erase(peerid);
    }

    if (videoListeners[chatid].empty())
    {
        videoListeners.erase(chatid);
    }

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

void MegaChatApiImpl::removeChatNotificationListener(MegaChatNotificationListener *listener)
{
    if (!listener)
    {
        return;
    }

    sdkMutex.lock();
    notificationListeners.erase(listener);
    sdkMutex.unlock();
}

IApp::IChatHandler *MegaChatApiImpl::createChatHandler(ChatRoom &room)
{
    return getChatRoomHandler(room.chatid());
}

IApp::IChatListHandler *MegaChatApiImpl::chatListHandler()
{
    return this;
}

#ifndef KARERE_DISABLE_WEBRTC

rtcModule::ICallHandler *MegaChatApiImpl::onIncomingCall(rtcModule::ICall& call, karere::AvFlags av)
{
    MegaChatHandle chatid = call.chat().chatId();
    MegaChatCallHandler *chatCallHandler = static_cast<MegaChatCallHandler *>(mClient->rtc->findCallHandler(chatid));
    if (!chatCallHandler)
    {
        chatCallHandler = new MegaChatCallHandler(this);
        mClient->rtc->addCallHandler(chatid, chatCallHandler);
    }

    chatCallHandler->setCall(&call);
    chatCallHandler->getMegaChatCall()->setInitialAudioVideoFlags(av);

    // Notify onIncomingCall like state change becouse rtcModule::ICall::kStateRingIn status
    // it is not notify
    chatCallHandler->onStateChange(call.state());

    return chatCallHandler;
}

rtcModule::ICallHandler *MegaChatApiImpl::onGroupCallActive(Id chatid, Id callid, uint32_t duration)
{
    MegaChatCallHandler *chatCallHandler = new MegaChatCallHandler(this);
    chatCallHandler->setCallNotPresent(chatid, callid, duration);
    chatCallHandler->onStateChange(MegaChatCall::CALL_STATUS_USER_NO_PRESENT);

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
    sdkMutex.lock();

    if (mClient && mClient->rtc)
    {
        std::vector<karere::Id> chatids = mClient->rtc->chatsWithCall();
        for (unsigned int i = 0; i < chatids.size(); i++)
        {
            mClient->rtc->removeCall(chatids[i]);
        }
    }

    sdkMutex.unlock();
}

MegaChatCallHandler *MegaChatApiImpl::findChatCallHandler(MegaChatHandle chatid)
{
    MegaChatCallHandler *callHandler = NULL;
    sdkMutex.lock();
    if (mClient && mClient->rtc)
    {
        callHandler = static_cast<MegaChatCallHandler *>(mClient->rtc->findCallHandler(chatid));
    }

    sdkMutex.unlock();
    return callHandler;
}

void MegaChatApiImpl::removeCall(MegaChatHandle chatid)
{
    sdkMutex.lock();
    mClient->rtc->removeCall(chatid);
    sdkMutex.unlock();
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

void MegaChatApiImpl::onChatNotification(karere::Id chatid, const Message &msg, Message::Status status, Idx idx)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    fireOnChatNotification(chatid, message);
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

void MegaChatApiImpl::onPresenceLastGreenUpdated(Id userid, uint16_t lastGreen)
{
    fireOnChatPresenceLastGreenUpdated(userid, lastGreen);
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
    this->mMegaHandleList = NULL;
}

MegaChatRequestPrivate::MegaChatRequestPrivate(MegaChatRequestPrivate &request)
{
    this->text = NULL;
    this->peerList = NULL;
    this->mMessage = NULL;
    this->mMegaNodeList = NULL;
    this->mMegaHandleList = NULL;

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
    this->setMegaHandleList(request.getMegaHandleList());
    if (mMegaHandleList)
    {
        for (unsigned int i = 0; i < mMegaHandleList->size(); i++)
        {
            MegaChatHandle chatid = mMegaHandleList->get(i);
            this->setMegaHandleListByChat(chatid, request.getMegaHandleListByChat(chatid));
        }
    }
}

MegaChatRequestPrivate::~MegaChatRequestPrivate()
{
    delete peerList;
    delete [] text;
    delete mMessage;
    delete mMegaNodeList;
    delete mMegaHandleList;
    for (map<MegaChatHandle, MegaHandleList*>::iterator it = mMegaHandleListMap.begin(); it != mMegaHandleListMap.end(); it++)
    {
        delete it->second;
    }
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
        case TYPE_ARCHIVE_CHATROOM: return "ARCHIVE_CHATROOM";
        case TYPE_PUSH_RECEIVED: return "PUSH_RECEIVED";
        case TYPE_SET_LAST_GREEN_VISIBLE: return "SET_LAST_GREEN_VISIBLE";
        case TYPE_LAST_GREEN: return "TYPE_LAST_GREEN";
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

void MegaChatRequestPrivate::setMegaHandleList(MegaHandleList *handlelist)
{
    if (mMegaHandleList != NULL)
    {
        delete mMegaHandleList;
    }

    mMegaHandleList = handlelist ? handlelist->copy() : NULL;
}

void MegaChatRequestPrivate::setMegaHandleListByChat(MegaChatHandle chatid, MegaHandleList *handlelist)
{
    MegaHandleList *list = getMegaHandleListByChat(chatid);
    if (list)
    {
        delete list;
    }

    mMegaHandleListMap[chatid] = handlelist ? handlelist->copy() : NULL;
}

MegaNodeList *MegaChatRequestPrivate::getMegaNodeList()
{
    return mMegaNodeList;
}

MegaHandleList *MegaChatRequestPrivate::getMegaHandleListByChat(MegaChatHandle chatid)
{
    map<MegaChatHandle, MegaHandleList*>::iterator it = mMegaHandleListMap.find(chatid);
    if (it != mMegaHandleListMap.end())
    {
        return it->second;
    }

    return NULL;
}

MegaHandleList *MegaChatRequestPrivate::getMegaHandleList()
{
    return mMegaHandleList;
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

MegaChatSessionPrivate::MegaChatSessionPrivate(const rtcModule::ISession &session)
    : state(convertSessionState(session.getState())), peerid(session.peer()), av(session.receivedAv())
{
}

MegaChatSessionPrivate::MegaChatSessionPrivate(const MegaChatSessionPrivate &session)
    : state(session.getStatus()), peerid(session.getPeerid()), av(session.hasAudio(), session.hasVideo()),
      networkQuality(session.getNetworkQuality()), audioDetected(session.getAudioDetected())
{
}

MegaChatSessionPrivate::~MegaChatSessionPrivate()
{
}

MegaChatSession *MegaChatSessionPrivate::copy()
{
    return new MegaChatSessionPrivate(*this);
}

int MegaChatSessionPrivate::getStatus() const
{
    return state;
}

MegaChatHandle MegaChatSessionPrivate::getPeerid() const
{
    return peerid;
}

bool MegaChatSessionPrivate::hasAudio() const
{
    return av.audio();
}

bool MegaChatSessionPrivate::hasVideo() const
{
    return av.video();
}

int MegaChatSessionPrivate::getNetworkQuality() const
{
    return networkQuality;
}

bool MegaChatSessionPrivate::getAudioDetected() const
{
    return audioDetected;
}

uint8_t MegaChatSessionPrivate::convertSessionState(uint8_t state)
{
    uint8_t sessionState = MegaChatSession::SESSION_STATUS_INVALID;
    switch (state)
    {
        case rtcModule::ISession::kStateInitial:
        case rtcModule::ISession::kStateWaitSdpOffer:
        case rtcModule::ISession::kStateWaitSdpAnswer:
        case rtcModule::ISession::kStateWaitLocalSdpAnswer:
        {
            sessionState = MegaChatSession::SESSION_STATUS_INITIAL;
            break;
        }
        case rtcModule::ISession::kStateInProgress:
        {
            sessionState = MegaChatSession::SESSION_STATUS_IN_PROGRESS;
            break;
        }
        case rtcModule::ISession::kStateTerminating:
        case rtcModule::ISession::kStateDestroyed:
        {
            sessionState = MegaChatSession::SESSION_STATUS_DESTROYED;
            break;
        }
        default:
        {
            API_LOG_ERROR("Unexpected session state, state: %d", state);
            assert(false);
            break;
        }
    }

    return sessionState;
}

void MegaChatSessionPrivate::setState(uint8_t state)
{
    this->state = convertSessionState(state);
}

void MegaChatSessionPrivate::setAvFlags(AvFlags flags)
{
    av = flags;
}

void MegaChatSessionPrivate::setNetworkQuality(int quality)
{
    networkQuality = quality;
}

void MegaChatSessionPrivate::setAudioDetected(bool audioDetected)
{
    this->audioDetected = audioDetected;
}

MegaChatCallPrivate::MegaChatCallPrivate(const rtcModule::ICall& call)
{
    status = call.state();
    chatid = call.chat().chatId();
    callid = call.id();
    mIsCaller = call.isCaller();
    // sentAv are invalid until state change to rtcModule::ICall::KStateHasLocalStream
    localAVFlags = call.sentAv();
    initialAVFlags = karere::AvFlags(false, false);
    initialTs = 0;
    finalTs = 0;
    temporaryError = std::string("");
    termCode = MegaChatCall::TERM_CODE_NOT_FINISHED;
    localTermCode = false;
    ringing = false;
    ignored = false;
    changed = 0;
    peerId = 0;
    callerId = call.caller() ? call.caller().val : MEGACHAT_INVALID_HANDLE;
    // At this point, there aren't any Session. It isn't neccesary create `sessionStatus` from Icall::sessionState()
}

MegaChatCallPrivate::MegaChatCallPrivate(Id chatid, Id callid, uint32_t duration)
{
    status = CALL_STATUS_USER_NO_PRESENT;
    this->chatid = chatid;
    this->callid = callid;
    // localAVFlags are invalid until state change to rtcModule::ICall::KStateHasLocalStream
    localAVFlags = karere::AvFlags(false, false);
    initialAVFlags = karere::AvFlags(false, false);
    initialTs = 0;
    if (duration > 0)
    {
        initialTs = time(NULL) - duration;
    }

    finalTs = 0;
    temporaryError = std::string("");
    termCode = MegaChatCall::TERM_CODE_NOT_FINISHED;
    localTermCode = false;
    ringing = false;
    ignored = false;
    changed = 0;
    peerId = 0;
    callerId = MEGACHAT_INVALID_HANDLE;
    mIsCaller = false;
}

MegaChatCallPrivate::MegaChatCallPrivate(const MegaChatCallPrivate &call)
{
    this->status = call.getStatus();
    this->chatid = call.getChatid();
    this->callid = call.getId();
    this->mIsCaller = call.isOutgoing();
    this->localAVFlags = call.localAVFlags;
    this->initialAVFlags = call.initialAVFlags;
    this->changed = call.changed;
    this->initialTs = call.initialTs;
    this->finalTs = call.finalTs;
    this->temporaryError = call.temporaryError;
    this->termCode = call.termCode;
    this->localTermCode = call.localTermCode;
    this->ringing = call.ringing;
    this->ignored = call.ignored;
    this->peerId = call.peerId;
    this->callerId = call.callerId;

    for (std::map<karere::Id, MegaChatSession *>::const_iterator it = call.sessions.begin(); it != call.sessions.end(); it++)
    {
        this->sessions[it->first] = it->second->copy();
    }

    this->participants = call.participants;

    this->termCode = call.termCode;
    this->ignored = call.ignored;
    this->localTermCode = call.localTermCode;
    this->ringing = call.ringing;
}

MegaChatCallPrivate::~MegaChatCallPrivate()
{
    for (std::map<karere::Id, MegaChatSession *>::iterator it = sessions.begin(); it != sessions.end(); it++)
    {
        MegaChatSession *session = it->second;
        delete session;
    }

    sessions.clear();
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

bool MegaChatCallPrivate::hasLocalAudio() const
{
    return localAVFlags.audio();
}

bool MegaChatCallPrivate::hasAudioInitialCall() const
{
    return initialAVFlags.audio();
}

bool MegaChatCallPrivate::hasLocalVideo() const
{
    return localAVFlags.video();
}

bool MegaChatCallPrivate::hasVideoInitialCall() const
{
    return initialAVFlags.video();
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

MegaHandleList *MegaChatCallPrivate::getSessions() const
{
    MegaHandleListPrivate *sessionList = new MegaHandleListPrivate();

    for (auto it = sessions.begin(); it != sessions.end(); it++)
    {
        sessionList->addMegaHandle(it->first);
    }

    return sessionList;
}

MegaChatHandle MegaChatCallPrivate::getPeerSessionStatusChange() const
{
    return peerId;
}

MegaChatSession *MegaChatCallPrivate::getMegaChatSession(MegaChatHandle peerId)
{
    auto it = sessions.find(peerId);
    if (it != sessions.end())
    {
        return it->second;
    }

    return NULL;
}

int MegaChatCallPrivate::getNumParticipants() const
{
    return participants.size();
}

MegaHandleList *MegaChatCallPrivate::getParticipants() const
{
    MegaHandleListPrivate *participantsList = new MegaHandleListPrivate();

    for (auto it = participants.begin(); it != participants.end(); it++)
    {
        chatd::EndpointId endPoint = it->first;
        participantsList->addMegaHandle(endPoint.userid);
    }

    return participantsList;
}

bool MegaChatCallPrivate::isIgnored() const
{
    return ignored;
}

bool MegaChatCallPrivate::isIncoming() const
{
    return !mIsCaller;
}

bool MegaChatCallPrivate::isOutgoing() const
{
    return mIsCaller;
}

MegaChatHandle MegaChatCallPrivate::getCaller() const
{
    return callerId;
}

void MegaChatCallPrivate::setStatus(int status)
{
    this->status = status;
    changed |= MegaChatCall::CHANGE_TYPE_STATUS;

    if (status == MegaChatCall::CALL_STATUS_DESTROYED)
    {
        setFinalTimeStamp(time(NULL));
        API_LOG_INFO("Call Destroyed. ChatId: %s, callid: %s, duration: %d (s)",
                     karere::Id(getChatid()).toString().c_str(),
                     karere::Id(getId()).toString().c_str(), getDuration());
    }
}

void MegaChatCallPrivate::setLocalAudioVideoFlags(AvFlags localAVFlags)
{
    this->localAVFlags = localAVFlags;
    changed |= MegaChatCall::CHANGE_TYPE_LOCAL_AVFLAGS;
}

void MegaChatCallPrivate::setInitialAudioVideoFlags(AvFlags initialAVFlags)
{
    this->initialAVFlags = initialAVFlags;
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
    changed = MegaChatCall::CHANGE_TYPE_NO_CHANGES;
    temporaryError.clear();
}

void MegaChatCallPrivate::setError(const string &temporaryError)
{
    this->temporaryError = temporaryError;
    changed |= MegaChatCall::CHANGE_TYPE_TEMPORARY_ERROR;
}

void MegaChatCallPrivate::setTermCode(rtcModule::TermCode termCode)
{
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
        case rtcModule::TermCode::kCallReqCancel:
            this->termCode = MegaChatCall::TERM_CODE_CALL_REQ_CANCEL;
            break;
        case rtcModule::TermCode::kCallRejected:
            this->termCode = MegaChatCall::TERM_CODE_CALL_REJECT;
            break;
        case rtcModule::TermCode::kAnsElsewhere:
            this->termCode = MegaChatCall::TERM_CODE_ANSWER_ELSE_WHERE;
            break;
        case rtcModule::TermCode::kRejElsewhere:
            this->termCode = MegaChatCall::TEMR_CODE_REJECT_ELSE_WHERE;
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
        case rtcModule::TermCode::kDestroyByCallCollision:
            this->termCode = MegaChatCall::TERM_CODE_DESTROY_BY_COLLISION;
            break;
        case rtcModule::TermCode::kCallerGone:
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

void MegaChatCallPrivate::setIgnoredCall(bool ignored)
{
    this->ignored = ignored;
}

MegaChatSessionPrivate *MegaChatCallPrivate::addSession(rtcModule::ISession &sess)
{
    auto it = sessions.find(sess.peer());
    if (it != sessions.end())
    {
        API_LOG_WARNING("addSession: this peer (%s) already has a session. Removing it...", sess.peer().toString().c_str());
        delete it->second;
    }

    MegaChatSessionPrivate *session = new MegaChatSessionPrivate(sess);
    sessions[sess.peer()] = session;
    return session;
}

void MegaChatCallPrivate::removeSession(Id peerid)
{
    std::map<karere::Id, MegaChatSession *>::iterator it = sessions.find(peerid);
    if (it != sessions.end())
    {
        delete it->second;
        sessions.erase(it);
    }
    else
    {
        API_LOG_ERROR("removeSession: Try to remove a session that doesn't exist (peer: %s)", peerid.toString().c_str());
    }
}

void MegaChatCallPrivate::sessionUpdated(Id peerid, int changeType)
{
    this->peerId = peerid;
    changed |= changeType;
}

int MegaChatCallPrivate::availableAudioSlots()
{
    int usedSlots = 0;
    for (auto it = participants.begin(); it != participants.end(); it++)
    {
        if (it->second.audio())
        {
            usedSlots++;
        }
    }

    int availableSlots = 0;
    if (usedSlots < rtcModule::IRtcModule::kMaxCallAudioSenders)
    {
        availableSlots = rtcModule::IRtcModule::kMaxCallAudioSenders;
    }

    return availableSlots;
}

int MegaChatCallPrivate::availableVideoSlots()
{
    int usedSlots = 0;
    for (auto it = participants.begin(); it != participants.end(); it++)
    {
        if (it->second.video())
        {
            usedSlots++;
        }
    }

    int availableSlots = 0;
    if (usedSlots < rtcModule::IRtcModule::kMaxCallVideoSenders)
    {
        availableSlots = rtcModule::IRtcModule::kMaxCallVideoSenders;
    }

    return availableSlots;
}

bool MegaChatCallPrivate::addOrUpdateParticipant(Id userid, uint32_t clientid, AvFlags flags)
{
    bool notify = false;

    chatd::EndpointId endPointId(userid, clientid);
    std::map<chatd::EndpointId, karere::AvFlags>::iterator it = participants.find(endPointId);
    if (it == participants.end())   // new participant
    {
        changed |= MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION;
        notify = true;
        participants[endPointId] = flags;
    }
    else    // existing participant --> just update flags
    {
        it->second = flags;
    }

    return notify;
}

bool MegaChatCallPrivate::removeParticipant(Id userid, uint32_t clientid)
{
    bool notify = false;

    chatd::EndpointId endPointId(userid, clientid);
    std::map<chatd::EndpointId, karere::AvFlags>::iterator it = participants.find(endPointId);
    if (it != participants.end())
    {
        participants.erase(it);
        changed |= MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION;
        notify = true;
    }

    return notify;
}

bool MegaChatCallPrivate::isParticipating(Id userid)
{
    for (auto it = participants.begin(); it != participants.end(); it++)
    {
        if (it->first.userid == userid)
        {
            return true;
        }
    }

    return false;
}

void MegaChatCallPrivate::removeAllParticipants()
{
    participants.clear();
    changed |= MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION;
}

void MegaChatCallPrivate::setId(Id callid)
{
    this->callid = callid;
}

void MegaChatCallPrivate::setCaller(Id caller)
{
    this->callerId = caller;
}

MegaChatVideoReceiver::MegaChatVideoReceiver(MegaChatApiImpl *chatApi, rtcModule::ICall *call, MegaChatHandle peerid)
{
    this->chatApi = chatApi;
    chatid = call->chat().chatId();
    this->peerid = peerid;
}

MegaChatVideoReceiver::~MegaChatVideoReceiver()
{
}

void* MegaChatVideoReceiver::getImageBuffer(unsigned short width, unsigned short height, void*& userData)
{
    MegaChatVideoFrame *frame = new MegaChatVideoFrame;
    frame->width = width;
    frame->height = height;
    frame->buffer = new ::mega::byte[width * height * 4];  // in format ARGB: 4 bytes per pixel
    userData = frame;
    return frame->buffer;
}

void MegaChatVideoReceiver::frameComplete(void *userData)
{
    chatApi->videoMutex.lock();
    MegaChatVideoFrame *frame = (MegaChatVideoFrame *)userData;
    chatApi->fireOnChatVideoData(chatid, peerid, frame->width, frame->height, (char *)frame->buffer);
    chatApi->videoMutex.unlock();
    delete [] frame->buffer;
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
    return chatApiImpl->findChatCallHandler(chatid);
}
#endif

MegaChatRoomHandler::MegaChatRoomHandler(MegaChatApiImpl *chatApiImpl, MegaChatApi *chatApi, MegaChatHandle chatid)
{
    this->chatApiImpl = chatApiImpl;
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
        (*it)->onChatRoomUpdate(chatApi, chat);
    }

    delete chat;
}

void MegaChatRoomHandler::fireOnMessageLoaded(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageLoaded(chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnMessageReceived(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageReceived(chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnMessageUpdate(MegaChatMessage *msg)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onMessageUpdate(chatApi, msg);
    }

    delete msg;
}

void MegaChatRoomHandler::fireOnHistoryReloaded(MegaChatRoom *chat)
{
    for(set<MegaChatRoomListener *>::iterator it = roomListeners.begin(); it != roomListeners.end() ; it++)
    {
        (*it)->onHistoryReloaded(chatApi, chat);
    }

    delete chat;
}

void MegaChatRoomHandler::onUserTyping(karere::Id user)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setUserTyping(user.val);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onUserStopTyping(karere::Id user)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setUserStopTyping(user.val);

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
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
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

void MegaChatRoomHandler::onMemberNameChanged(uint64_t /*userid*/, const std::string &/*newName*/)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setMembersUpdated();

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onChatArchived(bool archived)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setArchived(archived);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onTitleChanged(const string &title)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setTitle(title);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::onUnreadCountChanged(int count)
{
    MegaChatRoomPrivate *chat = (MegaChatRoomPrivate *) chatApiImpl->getChatRoom(chatid);
    chat->setUnreadCount(count);

    fireOnChatRoomUpdate(chat);
}

void MegaChatRoomHandler::init(Chat &chat, DbInterface *&)
{
    mChat = &chat;
    mRoom = chatApiImpl->findChatRoom(chatid);

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
            MegaChatMessagePrivate *msg = (MegaChatMessagePrivate *)chatApiImpl->getMessage(chatid, *itMsgId);
            if (msg)
            {
                msg->setAccess();
                fireOnMessageUpdate(msg);
            }
        }
        delete msgToUpdate;
    }

    // check if notification is required
    if ( (msg.type == chatd::Message::kMsgTruncate)   // truncate received from a peer or from myself in another client
         || (msg.userid != chatApi->getMyUserHandle() && status == chatd::Message::kNotSeen) )  // new (unseen) message received from a peer
    {

        MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
        chatApiImpl->fireOnChatNotification(chatid, message);
    }
}

void MegaChatRoomHandler::onRecvHistoryMessage(Idx idx, Message &msg, Message::Status status, bool /*isLocal*/)
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
            MegaChatMessagePrivate *msgUpdated = (MegaChatMessagePrivate *)chatApiImpl->getMessage(chatid, *itMsgId);
            if (msgUpdated)
            {
                msgUpdated->setAccess();
                fireOnMessageUpdate(msgUpdated);
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

void MegaChatRoomHandler::onMessageStatusChange(Idx idx, Message::Status status, const Message &msg)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    message->setStatus(status);
    fireOnMessageUpdate(message);

    if (msg.userid != chatApi->getMyUserHandle() && status == chatd::Message::kSeen)  // received message from a peer changed to seen
    {
        MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
        chatApiImpl->fireOnChatNotification(chatid, message);
    }
}

void MegaChatRoomHandler::onMessageEdited(const Message &msg, chatd::Idx idx)
{
    Message::Status status = mChat->getMsgStatus(msg, idx);
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
    message->setContentChanged();
    fireOnMessageUpdate(message);

    //TODO: check a truncate always comes as an edit, even if no history exist at all (new chat)
    // and, if so, remove the block from `onRecvNewMessage()`
    if ( (msg.type == chatd::Message::kMsgTruncate) // truncate received from a peer or from myself in another client
         || (msg.userid != chatApi->getMyUserHandle() && status == chatd::Message::kNotSeen) )    // received message from a peer, still unseen, was edited / deleted
    {
        MegaChatMessagePrivate *message = new MegaChatMessagePrivate(msg, status, idx);
        chatApiImpl->fireOnChatNotification(chatid, message);
    }
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
        if (userid.val == chatApiImpl->getMyUserHandle())
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
    : ::promise::Error(msg, code, type)
{
    this->setHandled();
}

MegaChatErrorPrivate::MegaChatErrorPrivate(int code, int type)
    : ::promise::Error(MegaChatErrorPrivate::getGenericErrorString(code), code, type)
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
    : ::promise::Error(error->getErrorString(), error->getErrorCode(), error->getErrorType())
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
    this->mHasCustomTitle = chat->hasCustomTitle();
    this->unreadCount = chat->getUnreadCount();
    this->active = chat->isActive();
    this->archived = chat->isArchived();
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
    this->mHasCustomTitle = chat.isGroup() ? ((GroupChatRoom*)&chat)->hasTitle() : false;
    this->unreadCount = chat.chat().unreadMsgCount();
    this->active = chat.isActive();
    this->archived = chat.isArchived();
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

bool MegaChatRoomPrivate::hasCustomTitle() const
{
    return mHasCustomTitle;
}

bool MegaChatRoomPrivate::isActive() const
{
    return active;
}

bool MegaChatRoomPrivate::isArchived() const
{
    return archived;
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

void MegaChatRoomPrivate::setUserStopTyping(MegaChatHandle uh)
{
    this->uh = uh;
    this->changed |= MegaChatRoom::CHANGE_TYPE_USER_STOP_TYPING;
}

void MegaChatRoomPrivate::setClosed()
{
    this->changed |= MegaChatRoom::CHANGE_TYPE_CLOSED;
}

void MegaChatRoomPrivate::setArchived(bool archived)
{
    this->archived = archived;
    this->changed |= MegaChatRoom::CHANGE_TYPE_ARCHIVE;
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
    this->archived =  chatroom.isArchived();
    this->mIsCallInProgress = chatroom.isCallActive();
    this->changed = 0;
    this->peerHandle = !group ? ((PeerChatRoom&)chatroom).peer() : MEGACHAT_INVALID_HANDLE;
    this->lastMsgPriv = Priv::PRIV_INVALID;
    this->lastMsgHandle = MEGACHAT_INVALID_HANDLE;

    LastTextMsg tmp;
    LastTextMsg *message = &tmp;
    LastTextMsg *&msg = message;
    uint8_t lastMsgStatus = chatroom.chat().lastTextMessage(msg);
    if (lastMsgStatus == LastTextMsgState::kHave)
    {        
        this->lastMsgSender = msg->sender();
        this->lastMsgType = msg->type();
        this->mLastMsgId = (msg->idx() == CHATD_IDX_INVALID) ? msg->xid() : msg->id();

        switch (lastMsgType)
        {
            case MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
            case MegaChatMessage::TYPE_NODE_ATTACHMENT:
            case MegaChatMessage::TYPE_CONTAINS_META:
            case MegaChatMessage::TYPE_VOICE_CLIP:
                this->lastMsg = JSonUtils::getLastMessageContent(msg->contents(), msg->type());
                break;

            case MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
            case MegaChatMessage::TYPE_PRIV_CHANGE:
            {
                const Message::ManagementInfo *management = reinterpret_cast<const Message::ManagementInfo*>(msg->contents().data());
                this->lastMsgPriv = management->privilege;
                this->lastMsgHandle = (MegaChatHandle)management->target;
                break;
            }

            case MegaChatMessage::TYPE_NORMAL:
            case MegaChatMessage::TYPE_CHAT_TITLE:
                this->lastMsg = msg->contents();
                break;

            case MegaChatMessage::TYPE_CALL_ENDED:
            {
                Message::CallEndedInfo *callEndedInfo = Message::CallEndedInfo::fromBuffer(msg->contents().data(), msg->contents().size());
                if (callEndedInfo)
                {
                    this->lastMsg = std::to_string(callEndedInfo->duration);
                    this->lastMsg.push_back(0x01);
                    int termCode = MegaChatMessagePrivate::convertEndCallTermCodeToUI(*callEndedInfo);
                    this->lastMsg += std::to_string(termCode);
                    for (unsigned int i = 0; i < callEndedInfo->participants.size(); i++)
                    {
                        this->lastMsg.push_back(0x01);
                        karere::Id id(callEndedInfo->participants[i]);
                        this->lastMsg += id.toString();
                    }
                    delete callEndedInfo;
                }
                break;
            }

            case MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT:  // deprecated: should not be notified as last-message
            case MegaChatMessage::TYPE_TRUNCATE:    // no content at all
            case MegaChatMessage::TYPE_CALL_STARTED:    // no content at all
            default:
                break;
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
    this->archived = item->isArchived();
    this->mIsCallInProgress = item->isCallInProgress();
    this->lastMsgPriv = item->getLastMessagePriv();
    this->lastMsgHandle = item->getLastMessageHandle();
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

bool MegaChatListItemPrivate::isArchived() const
{
    return archived;
}

bool MegaChatListItemPrivate::isCallInProgress() const
{
    return mIsCallInProgress;
}

MegaChatHandle MegaChatListItemPrivate::getPeerHandle() const
{
    return peerHandle;
}

int MegaChatListItemPrivate::getLastMessagePriv() const
{
    return lastMsgPriv;
}

MegaChatHandle MegaChatListItemPrivate::getLastMessageHandle() const
{
    return lastMsgHandle;
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

void MegaChatListItemPrivate::setArchived(bool archived)
{
    this->archived = archived;
    this->changed |= MegaChatListItem::CHANGE_TYPE_ARCHIVE;
}

void MegaChatListItemPrivate::setCallInProgress()
{
    this->changed |= MegaChatListItem::CHANGE_TYPE_CALL;
}

void MegaChatListItemPrivate::setLastMessage()
{
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

void MegaChatListItemHandler::onLastMessageUpdated(const LastTextMsg& /*msg*/)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setLastMessage();
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

void MegaChatListItemHandler::onChatArchived(bool archived)
{
    MegaChatListItemPrivate *item = new MegaChatListItemPrivate(this->mRoom);
    item->setArchived(archived);
    chatApi.fireOnChatListItemUpdate(item);
}

MegaChatPeerListItemHandler::MegaChatPeerListItemHandler(MegaChatApiImpl &chatApi, ChatRoom &room)
    : MegaChatListItemHandler(chatApi, room)
{

}

MegaChatMessagePrivate::MegaChatMessagePrivate(const MegaChatMessage *msg)
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
    this->megaHandleList = msg->getMegaHandleList() ? msg->getMegaHandleList()->copy() : NULL;

    if (msg->getUsersCount() != 0)
    {
        this->megaChatUsers = new std::vector<MegaChatAttachedUser>();

        for (unsigned int i = 0; i < msg->getUsersCount(); ++i)
        {
            MegaChatAttachedUser megaChatUser(msg->getUserHandle(i), msg->getUserEmail(i), msg->getUserName(i));

            this->megaChatUsers->push_back(megaChatUser);
        }
    }

    if (msg->getType() == TYPE_CONTAINS_META)
    {
        this->mContainsMeta = msg->getContainsMeta()->copy();
    }
}

MegaChatMessagePrivate::MegaChatMessagePrivate(const Message &msg, Message::Status status, Idx index)
{
    if (msg.type == TYPE_NORMAL || msg.type == TYPE_CHAT_TITLE)
    {
        string tmp(msg.buf(), msg.size());
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
        case MegaChatMessage::TYPE_VOICE_CLIP:
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
        case MegaChatMessage::TYPE_CONTAINS_META:
        {
            uint8_t containsMetaType = msg.containMetaSubtype();
            string containsMetaJson = msg.containsMetaJson();
            mContainsMeta = JSonUtils::parseContainsMeta(containsMetaJson.c_str(), containsMetaType);
            break;
        }
        case MegaChatMessage::TYPE_CALL_ENDED:
        {
            megaHandleList = new MegaHandleListPrivate();
            Message::CallEndedInfo *callEndInfo = Message::CallEndedInfo::fromBuffer(msg.buf(), msg.size());
            if (callEndInfo)
            {
                for (size_t i = 0; i < callEndInfo->participants.size(); i++)
                {
                    megaHandleList->addMegaHandle(callEndInfo->participants[i]);
                }

                priv = callEndInfo->duration;
                code = MegaChatMessagePrivate::convertEndCallTermCodeToUI(*callEndInfo);
                delete callEndInfo;
            }
            break;
        }
        case MegaChatMessage::TYPE_NORMAL:
        case MegaChatMessage::TYPE_CHAT_TITLE:
        case MegaChatMessage::TYPE_TRUNCATE:
        case MegaChatMessage::TYPE_CALL_STARTED:    // no content at all
            break;
        default:
        {
            this->type = MegaChatMessage::TYPE_UNKNOWN;
            break;
        }
    }

    int encryptionState = msg.isEncrypted();
    switch (encryptionState)
    {
    case Message::kEncryptedPending:    // transient, app will receive update once decrypted
    case Message::kEncryptedNoKey:
    case Message::kEncryptedNoType:
        this->code = encryptionState;
        this->type = MegaChatMessage::TYPE_UNKNOWN; // --> ignore/hide them
        break;
    case Message::kEncryptedMalformed:
    case Message::kEncryptedSignature:
        this->code = encryptionState;
        this->type = MegaChatMessage::TYPE_INVALID; // --> show a warning
        break;
    case Message::kNotEncrypted:
        break;
    }
}

MegaChatMessagePrivate::~MegaChatMessagePrivate()
{
    delete [] msg;
    delete megaChatUsers;
    delete megaNodeList;
    delete mContainsMeta;
    delete megaHandleList;
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
    // if message contains meta and is of rich-link type, return the original content
    if (type == MegaChatMessage::TYPE_CONTAINS_META)
    {
        return getContainsMeta()->getTextMessage();

    }
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
    return ((type == TYPE_NORMAL || type == TYPE_CONTAINS_META) && !isDeleted() && ((time(NULL) - ts) < CHATD_MAX_EDIT_AGE));
}

bool MegaChatMessagePrivate::isDeletable() const
{
    return ((type == TYPE_NORMAL || type == TYPE_CONTACT_ATTACHMENT || type == TYPE_NODE_ATTACHMENT || type == TYPE_CONTAINS_META || type == TYPE_VOICE_CLIP)
            && !isDeleted() && ((time(NULL) - ts) < CHATD_MAX_EDIT_AGE));
}

bool MegaChatMessagePrivate::isManagementMessage() const
{
    return (type >= TYPE_LOWEST_MANAGEMENT
            && type <= TYPE_HIGHEST_MANAGEMENT);
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

int MegaChatMessagePrivate::convertEndCallTermCodeToUI(const Message::CallEndedInfo  &callEndInfo)
{
    int code;
    switch (callEndInfo.termCode)
    {
        case END_CALL_REASON_CANCELLED:
            code = END_CALL_REASON_NO_ANSWER;
            break;
        case END_CALL_REASON_ENDED:;
        case END_CALL_REASON_FAILED:
            if (callEndInfo.duration > 0)
            {
                code =  END_CALL_REASON_ENDED;
            }
            else
            {
                code = END_CALL_REASON_FAILED;
            }
            break;
        default:
            code = callEndInfo.termCode;
            break;
    }

    return code;
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

const MegaChatContainsMeta *MegaChatMessagePrivate::getContainsMeta() const
{
    return mContainsMeta;
}

MegaHandleList *MegaChatMessagePrivate::getMegaHandleList() const
{
    return megaHandleList;
}

int MegaChatMessagePrivate::getDuration() const
{
    return priv;
}

int MegaChatMessagePrivate::getTermCode() const
{
    return code;
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
    mutex.lock();
    this->megaLogger = logger;
    mutex.unlock();
}

void LoggerHandler::setLogLevel(int logLevel)
{
    mutex.lock();
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
    mutex.unlock();
}

void LoggerHandler::setLogWithColors(bool useColors)
{
    gLogger.logToConsoleUseColors(useColors);
}

void LoggerHandler::setLogToConsole(bool enable)
{
    gLogger.logToConsole(enable);
}

void LoggerHandler::log(krLogLevel level, const char *msg, size_t /*len*/, unsigned /*flags*/)
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
    if (chatCall && chatCall->getStatus() != MegaChatCall::CALL_STATUS_DESTROYED)
    {
        chatCall->setStatus(MegaChatCall::CALL_STATUS_DESTROYED);
        megaChatApi->fireOnChatCallUpdate(chatCall);
    }

    delete chatCall;
}

void MegaChatCallHandler::setCall(rtcModule::ICall *call)
{
    assert(!this->call);
    this->call = call;
    if (!chatCall)
    {
        chatCall = new MegaChatCallPrivate(*call);
    }
    else
    {
        chatCall->setStatus(call->state());
        chatCall->setLocalAudioVideoFlags(call->sentAv());
        assert(chatCall->getId() == call->id());
    }
}

void MegaChatCallHandler::onStateChange(uint8_t newState)
{
    assert(chatCall);
    if (chatCall)
    {
        API_LOG_INFO("Call state changed. ChatId: %s, callid: %s, state: %s --> %s",
                     karere::Id(chatCall->getChatid()).toString().c_str(),
                     karere::Id(chatCall->getId()).toString().c_str(),
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
                assert(call);
                chatCall->setCaller(call->caller());
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
                state = MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION;
                chatCall->setIsRinging(false);
                chatCall->setTermCode(call->termCode());
                API_LOG_INFO("Terminating call. ChatId: %s, callid: %s, termCode: %s , isLocal: %d, duration: %d (s)",
                             karere::Id(chatCall->getChatid()).toString().c_str(),
                             karere::Id(chatCall->getId()).toString().c_str(),
                             rtcModule::termCodeToStr(call->termCode() & (~rtcModule::TermCode::kPeer)),
                             chatCall->isLocalTermCode(), chatCall->getDuration());
                break;
            case rtcModule::ICall::kStateDestroyed:
                return;
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

void MegaChatCallHandler::onDestroy(rtcModule::TermCode /*reason*/, bool /*byPeer*/, const string &/*msg*/)
{
    assert(chatCall);
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    call = NULL;
    if (chatCall != NULL)
    {
        chatid = chatCall->getChatid();
        MegaHandleList *participants = chatCall->getParticipants();
        bool uniqueParticipant = (participants && participants->size() == 1 && participants->get(0) == megaChatApi->getMyUserHandle());
        if (participants && participants->size() > 0 && !uniqueParticipant)
        {
            chatCall->setStatus(MegaChatCall::CALL_STATUS_USER_NO_PRESENT);
            megaChatApi->fireOnChatCallUpdate(chatCall);
        }
        else
        {
            chatCall->setStatus(MegaChatCall::CALL_STATUS_DESTROYED);
            megaChatApi->fireOnChatCallUpdate(chatCall);
            megaChatApi->removeCall(chatid);
        }

        delete participants;
    }
    else
    {
        API_LOG_ERROR("MegaChatCallHandler::onDestroy - There is not any MegaChatCallPrivate associated to MegaChatCallHandler");
        delete this;    // should not happen but, just-in-case, avoid the memory leak
    }

}

rtcModule::ISessionHandler *MegaChatCallHandler::onNewSession(rtcModule::ISession &sess)
{
    MegaChatSessionPrivate *megaChatSession = chatCall->addSession(sess);

    return new MegaChatSessionHandler(megaChatApi, this, megaChatSession, sess);
}

void MegaChatCallHandler::onLocalStreamObtained(rtcModule::IVideoRenderer *&rendererOut)
{
    assert(chatCall != NULL);
    if (chatCall != NULL)
    {
        if (localVideoReceiver != NULL)
        {
            API_LOG_WARNING("MegaChatCallHandler::onLocalStreamObtained - A local video receiver already exists for this MegaChatCallPrivate");
            delete localVideoReceiver;
        }

        rendererOut = new MegaChatVideoReceiver(megaChatApi, call);
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
}

void MegaChatCallHandler::addParticipant(Id userid, uint32_t clientid, AvFlags flags)
{
    assert(chatCall);
    if (chatCall)
    {
        bool notify = chatCall->addOrUpdateParticipant(userid, clientid, flags);
        if (notify)
        {
            megaChatApi->fireOnChatCallUpdate(chatCall);
        }
    }
}

bool MegaChatCallHandler::removeParticipant(Id userid, uint32_t clientid)
{
    assert(chatCall);
    if (chatCall)
    {
        bool notify = chatCall->removeParticipant(userid, clientid);
        if (notify)
        {
            megaChatApi->fireOnChatCallUpdate(chatCall);
        }

        MegaHandleList *participants = chatCall->getParticipants();
        if (participants && participants->size() < 1 && !call)
        {
            chatCall->setStatus(MegaChatCall::CALL_STATUS_DESTROYED);
            megaChatApi->fireOnChatCallUpdate(chatCall);
            delete participants;
            return true;
        }

        delete participants;
    }

    return false;
}

int MegaChatCallHandler::callParticipants()
{
    assert(chatCall);
    return chatCall ? chatCall->getNumParticipants(): 0;
}

bool MegaChatCallHandler::isParticipating(Id userid)
{
    assert(chatCall);
    return chatCall->isParticipating(userid);
}

void MegaChatCallHandler::removeAllParticipants()
{
    chatCall->removeAllParticipants();
    megaChatApi->fireOnChatCallUpdate(chatCall);
}

karere::Id MegaChatCallHandler::getCallId() const
{
    assert(chatCall);
    return chatCall->getId();
}

void MegaChatCallHandler::setCallId(karere::Id callid)
{
    assert(chatCall);
    chatCall->setId(callid);
    if (chatCall->getChanges() != MegaChatCall::CHANGE_TYPE_NO_CHANGES)
    {
        megaChatApi->fireOnChatCallUpdate(chatCall);
    }
}

void MegaChatCallHandler::setInitialTimeStamp(int64_t timeStamp)
{
    assert(chatCall);
    if (!chatCall->getInitialTimeStamp())
    {
        chatCall->setInitialTimeStamp(timeStamp);
    }
}

int64_t MegaChatCallHandler::getInitialTimeStamp()
{
    assert(chatCall);
    return chatCall->getInitialTimeStamp();
}

rtcModule::ICall *MegaChatCallHandler::getCall()
{
    return call;
}

MegaChatCallPrivate *MegaChatCallHandler::getMegaChatCall()
{
    return chatCall;
}

void MegaChatCallHandler::setCallNotPresent(Id chatid, Id callid, uint32_t duration)
{
    this->call = NULL;
    chatCall = new MegaChatCallPrivate(chatid, callid, duration);
}

MegaChatSessionHandler::MegaChatSessionHandler(MegaChatApiImpl *megaChatApi, MegaChatCallHandler *callHandler, MegaChatSessionPrivate *megaChatSession, rtcModule::ISession &session)
{
    this->megaChatApi = megaChatApi;
    this->callHandler = callHandler;
    this->session = &session;
    this->remoteVideoRender = NULL;
    this->megaChatSession = megaChatSession;
}

MegaChatSessionHandler::~MegaChatSessionHandler()
{
    delete remoteVideoRender;
}

void MegaChatSessionHandler::onSessStateChange(uint8_t newState)
{
    switch (newState)
    {
        case rtcModule::ISession::kStateWaitSdpOffer:
        case rtcModule::ISession::kStateWaitSdpAnswer:
        case rtcModule::ISession::kStateWaitLocalSdpAnswer:
        {
            MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
            megaChatSession->setState(newState);
            chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_SESSION_STATUS);
            megaChatApi->fireOnChatCallUpdate(chatCall);
            break;
        }
        case rtcModule::ISession::kStateInProgress:
        {
            MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
            megaChatSession->setAvFlags(session->receivedAv());
            chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS);
            API_LOG_INFO("Initial remote audio/video flags. ChatId: %s, callid: %s, AV: %s",
                         Id(chatCall->getChatid()).toString().c_str(),
                         Id(chatCall->getId()).toString().c_str(),
                         session->receivedAv().toString().c_str());

            megaChatSession->setState(newState);
            chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_SESSION_STATUS);
            megaChatApi->fireOnChatCallUpdate(chatCall);
            break;
        }
        case rtcModule::ISession::kStateDestroyed:
        {
            if (callHandler->getCall()->state() < rtcModule::ICall::kStateTerminating)
            {
                MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
                megaChatSession->setState(newState);
                chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_SESSION_STATUS);
                megaChatApi->fireOnChatCallUpdate(chatCall);
            }

            break;
        }
        default:
            break;
    }
}

void MegaChatSessionHandler::onSessDestroy(rtcModule::TermCode /*reason*/, bool /*byPeer*/, const std::string& /*msg*/)
{
    MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
    chatCall->removeSession(session->peer());
    delete this;
}

void MegaChatSessionHandler::onRemoteStreamAdded(rtcModule::IVideoRenderer *&rendererOut)
{
    rtcModule::ICall *call = callHandler->getCall();
    assert(call != NULL);

    if (remoteVideoRender != NULL)
    {
       delete remoteVideoRender;
    }

    rendererOut = new MegaChatVideoReceiver(megaChatApi, call, session->peer());
    remoteVideoRender = rendererOut;
}

void MegaChatSessionHandler::onRemoteStreamRemoved()
{
    delete remoteVideoRender;
    remoteVideoRender = NULL;
}

void MegaChatSessionHandler::onPeerMute(karere::AvFlags av, karere::AvFlags oldAv)
{
    MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
    megaChatSession->setAvFlags(av);
    chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS);
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

void MegaChatSessionHandler::onSessionNetworkQualityChange(int currentQuality)
{
    MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
    megaChatSession->setNetworkQuality(currentQuality);
    chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_SESSION_NETWORK_QUALITY);
    API_LOG_INFO("Network quality change. ChatId: %s, peer: %s, value: %d",
                 Id(chatCall->getChatid()).toString().c_str(),
                 session->peer().toString().c_str(),
                 currentQuality);

    megaChatApi->fireOnChatCallUpdate(chatCall);
}

void MegaChatSessionHandler::onSessionAudioDetected(bool audioDetected)
{
    MegaChatCallPrivate *chatCall = callHandler->getMegaChatCall();
    megaChatSession->setAudioDetected(audioDetected);
    chatCall->sessionUpdated(session->peer(), MegaChatCall::CHANGE_TYPE_SESSION_AUDIO_LEVEL);
    API_LOG_INFO("Change Audio level. ChatId: %s, peer: %s, value: %s",
                 Id(chatCall->getChatid()).toString().c_str(),
                 session->peer().toString().c_str(),
                 audioDetected ? "Active" : "Inactive");

    megaChatApi->fireOnChatCallUpdate(chatCall);
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
    this->lastGreenVisible = config.isLastGreenVisible();
    this->pending = config.isPending();
}

MegaChatPresenceConfigPrivate::MegaChatPresenceConfigPrivate(const presenced::Config &config, bool isPending)
{
    this->status = config.presence().status();
    this->autoawayEnabled = config.autoawayActive();
    this->autoawayTimeout = config.autoawayTimeout();
    this->persistEnabled = config.persist();
    this->lastGreenVisible = config.lastGreenVisible();
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

bool MegaChatPresenceConfigPrivate::isLastGreenVisible() const
{
    return lastGreenVisible;
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

int MegaChatContainsMetaPrivate::getType() const
{
    return mType;
}

const char *MegaChatContainsMetaPrivate::getTextMessage() const
{
    return mText.c_str();
}

const MegaChatRichPreview *MegaChatContainsMetaPrivate::getRichPreview() const
{
    return mRichPreview;
}

const MegaChatGeolocation *MegaChatContainsMetaPrivate::getGeolocation() const
{
    return mGeolocation;
}

void MegaChatContainsMetaPrivate::setRichPreview(MegaChatRichPreview *richPreview)
{
    if (mRichPreview)
    {
        delete mRichPreview;
    }

    if (richPreview)
    {
        mType = MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW;
        mRichPreview = richPreview;
    }
    else
    {
        mType = MegaChatContainsMeta::CONTAINS_META_INVALID;
        mRichPreview = NULL;
    }
}

void MegaChatContainsMetaPrivate::setGeolocation(MegaChatGeolocation *geolocation)
{
    if (mGeolocation)
    {
        delete mGeolocation;
    }

    if (geolocation)
    {
        mType = MegaChatContainsMeta::CONTAINS_META_GEOLOCATION;
        mGeolocation = geolocation;
    }
    else
    {
        mType = MegaChatContainsMeta::CONTAINS_META_INVALID;
        mGeolocation = NULL;
    }
}

void MegaChatContainsMetaPrivate::setTextMessage(const string &text)
{
    mText = text;
}

MegaChatContainsMetaPrivate::MegaChatContainsMetaPrivate(const MegaChatContainsMeta *containsMeta)
{
    if (!containsMeta)
    {
        return;
    }

    this->mType = containsMeta->getType();
    this->mRichPreview = containsMeta->getRichPreview() ? containsMeta->getRichPreview()->copy() : NULL;
    this->mGeolocation = containsMeta->getGeolocation() ? containsMeta->getGeolocation()->copy() : NULL;
    this->mText = containsMeta->getTextMessage();
}

MegaChatContainsMetaPrivate::~MegaChatContainsMetaPrivate()
{
    delete mRichPreview;
}

MegaChatContainsMeta *MegaChatContainsMetaPrivate::copy() const
{
    return new MegaChatContainsMetaPrivate(this);
}

MegaChatRichPreviewPrivate::MegaChatRichPreviewPrivate(const MegaChatRichPreview *richPreview)
{
    this->mText = richPreview->getText();
    this->mTitle = richPreview->getTitle();
    this->mDescription = richPreview->getDescription();
    this->mImage = richPreview->getImage() ? richPreview->getImage() : "";
    this->mImageFormat = richPreview->getImageFormat();
    this->mIcon = richPreview->getIcon() ? richPreview->getIcon() : "";
    this->mIconFormat = richPreview->getIconFormat();
    this->mUrl = richPreview->getUrl();
    this->mDomainName = richPreview->getDomainName();
}

MegaChatRichPreviewPrivate::MegaChatRichPreviewPrivate(const string &text, const string &title, const string &description,
                                         const string &image, const string &imageFormat, const string &icon,
                                         const string &iconFormat, const string &url)
    : mText(text), mTitle(title), mDescription(description)
    , mImage(image), mImageFormat(imageFormat), mIcon(icon)
    , mIconFormat(iconFormat), mUrl(url)
{
    mDomainName = mUrl;
    std::string::size_type position = mDomainName.find("://");
    if (position != std::string::npos)
    {
         mDomainName = mDomainName.substr(position + 3);
    }

    position = mDomainName.find("/");
    if (position != std::string::npos)
    {
        mDomainName = mDomainName.substr(0, position);
    }
}

const char *MegaChatRichPreviewPrivate::getText() const
{
    return mText.c_str();
}

const char *MegaChatRichPreviewPrivate::getTitle() const
{
    return mTitle.c_str();
}

const char *MegaChatRichPreviewPrivate::getDescription() const
{
    return mDescription.c_str();
}

const char *MegaChatRichPreviewPrivate::getImage() const
{
    return mImage.size() ? mImage.c_str() : NULL;
}

const char *MegaChatRichPreviewPrivate::getImageFormat() const
{
    return mImageFormat.c_str();
}

const char *MegaChatRichPreviewPrivate::getIcon() const
{
    return mIcon.size() ? mIcon.c_str() : NULL;
}

const char *MegaChatRichPreviewPrivate::getIconFormat() const
{
    return mIconFormat.c_str();
}

const char *MegaChatRichPreviewPrivate::getUrl() const
{
    return mUrl.c_str();
}

MegaChatRichPreview *MegaChatRichPreviewPrivate::copy() const
{
    return new MegaChatRichPreviewPrivate(this);
}

MegaChatRichPreviewPrivate::~MegaChatRichPreviewPrivate()
{

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

std::string JSonUtils::generateAttachNodeJSon(MegaNodeList *nodes, uint8_t type)
{
    std::string ret;
    if (!nodes || type == Message::kMsgInvalid)
    {
        return ret;
    }

    rapidjson::Document jSonAttachmentNodes(rapidjson::kArrayType);
    for (int i = 0; i < nodes->size(); ++i)
    {
        rapidjson::Value jsonNode(rapidjson::kObjectType);

        MegaNode *megaNode = nodes->get(i);
        if (megaNode == NULL)
        {
            API_LOG_ERROR("Invalid node at index %d", i);
            return ret;
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
        Base64::atob(base64Key, (::mega::byte*)tempKey, FILENODEKEYLENGTH);
        delete [] base64Key;

        std::vector<int32_t> keyVector = DataTranslation::b_to_vector(std::string(tempKey, FILENODEKEYLENGTH));
        rapidjson::Value keyVectorNode(rapidjson::kArrayType);
        if (keyVector.size() != 8)
        {
            API_LOG_ERROR("Invalid nodekey for attached node: %d", megaNode->getHandle());
            return ret;
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
        const char *fingerprintMega = megaNode->getFingerprint();
        char *fingerprint = NULL;
        if (fingerprintMega)
        {
            fingerprint = MegaApiImpl::getMegaFingerprintFromSdkFingerprint(fingerprintMega);
        }

        if (fingerprint)
        {
            rapidjson::Value fpValue(rapidjson::kStringType);
            fpValue.SetString(fingerprint, strlen(fingerprint), jSonAttachmentNodes.GetAllocator());
            jsonNode.AddMember(rapidjson::Value("hash"), fpValue, jSonAttachmentNodes.GetAllocator());
            delete [] fingerprint;
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

    ret.assign(buffer.GetString(), buffer.GetSize());
    ret.insert(ret.begin(), type - Message::kMsgOffset);
    ret.insert(ret.begin(), 0x0);

    return ret;
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

    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        API_LOG_ERROR("parseAttachNodeJSon: Parser json error");
        return NULL;
    }

    MegaNodeList *megaNodeList = new MegaNodeListPrivate();

    int attachmentNumber = document.Capacity();
    for (int i = 0; i < attachmentNumber; ++i)
    {
        const rapidjson::Value& file = document[i];

        // nodehandle
        rapidjson::Value::ConstMemberIterator iteratorHandle = file.FindMember("h");
        if (iteratorHandle == file.MemberEnd() || !iteratorHandle->value.IsString())
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid nodehandle in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        MegaHandle megaHandle = MegaApi::base64ToHandle(iteratorHandle->value.GetString());

        // filename
        rapidjson::Value::ConstMemberIterator iteratorName = file.FindMember("name");
        if (iteratorName == file.MemberEnd() || !iteratorName->value.IsString())
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid filename in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        std::string nameString = iteratorName->value.GetString();

        // nodekey
        rapidjson::Value::ConstMemberIterator iteratorKey = file.FindMember("k");
        if (!iteratorKey->value.IsArray())
        {
            iteratorKey = file.FindMember("key");
        }
        if (iteratorKey == file.MemberEnd() || !iteratorKey->value.IsArray()
                || iteratorKey->value.Capacity() != 8)
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid nodekey in attachment JSON");
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
                API_LOG_ERROR("parseAttachNodeJSon: Invalid nodekey data in attachment JSON");
                delete megaNodeList;
                return NULL;
            }
        }
        std::string key = DataTranslation::vector_to_b(kElements);

        // size
        rapidjson::Value::ConstMemberIterator iteratorSize = file.FindMember("s");
        if (iteratorSize == file.MemberEnd() || !iteratorSize->value.IsInt64())
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid size in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int64_t size = iteratorSize->value.GetInt64();

        // fingerprint
        rapidjson::Value::ConstMemberIterator iteratorFp = file.FindMember("hash");
        std::string fp;
        if (iteratorFp == file.MemberEnd() || !iteratorFp->value.IsString())
        {
            API_LOG_WARNING("parseAttachNodeJSon: Missing fingerprint in attachment JSON. Old message?");
        }
        else
        {
            fp = iteratorFp->value.GetString();
        }
        // convert MEGA's fingerprint to the internal format used by SDK (includes size)
        char *sdkFingerprint = !fp.empty() ? MegaApiImpl::getSdkFingerprintFromMegaFingerprint(fp.c_str(), size) : NULL;

        // nodetype
        rapidjson::Value::ConstMemberIterator iteratorType = file.FindMember("t");
        if (iteratorType == file.MemberEnd() || !iteratorType->value.IsInt())
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid type in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int type = iteratorType->value.GetInt();

        // timestamp
        rapidjson::Value::ConstMemberIterator iteratorTimeStamp = file.FindMember("ts");
        if (iteratorTimeStamp == file.MemberEnd() || !iteratorTimeStamp->value.IsInt64())
        {
            API_LOG_ERROR("parseAttachNodeJSon: Invalid timestamp in attachment JSON");
            delete megaNodeList;
            return NULL;
        }
        int64_t timeStamp = iteratorTimeStamp->value.GetInt64();

        // file-attrstring
        rapidjson::Value::ConstMemberIterator iteratorFa = file.FindMember("fa");
        std::string fa;
        if (iteratorFa != file.MemberEnd() && iteratorFa->value.IsString())
        {
            fa = iteratorFa->value.GetString();
        }

        std::string attrstring;
        MegaNodePrivate node(nameString.c_str(), type, size, timeStamp, timeStamp,
                             megaHandle, &key, &attrstring, &fa, sdkFingerprint, INVALID_HANDLE,
                             NULL, NULL, false, true);

        megaNodeList->addNode(&node);

        delete [] sdkFingerprint;
    }

    return megaNodeList;
}

std::string JSonUtils::generateAttachContactJSon(MegaHandleList *contacts, ContactList *contactList)
{
    std::string ret;
    if (!contacts || contacts->size() == 0 || !contactList || contacts->size() > contactList->size())
    {
        API_LOG_ERROR("parseAttachContactJSon: no contacts available");
        return ret;
    }

    rapidjson::Document jSonDocument(rapidjson::kArrayType);
    for (unsigned int i = 0; i < contacts->size(); ++i)
    {
        auto contactIterator = contactList->find(contacts->get(i));
        if (contactIterator != contactList->end())
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
            API_LOG_ERROR("Failed to find the contact: %d", contacts->get(i));
            return ret;
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    jSonDocument.Accept(writer);

    // assemble final message with the type
    ret.assign(buffer.GetString(), buffer.GetSize());
    ret.insert(ret.begin(), Message::kMsgContact - Message::kMsgOffset);
    ret.insert(ret.begin(), 0x0);

    return ret;
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

    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        API_LOG_ERROR("parseAttachContactJSon: Parser json error");
        return NULL;
    }

    std::vector<MegaChatAttachedUser> *megaChatUsers = new std::vector<MegaChatAttachedUser>();

    int contactNumber = document.Capacity();
    for (int i = 0; i < contactNumber; ++i)
    {
        const rapidjson::Value& user = document[i];

        rapidjson::Value::ConstMemberIterator iteratorEmail = user.FindMember("email");
        if (iteratorEmail == user.MemberEnd() || !iteratorEmail->value.IsString())
        {
            API_LOG_ERROR("parseAttachContactJSon: Invalid email in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string emailString = iteratorEmail->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorHandle = user.FindMember("u");
        if (iteratorHandle == user.MemberEnd() || !iteratorHandle->value.IsString())
        {
            API_LOG_ERROR("parseAttachContactJSon: Invalid userhandle in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string handleString = iteratorHandle->value.GetString();

        rapidjson::Value::ConstMemberIterator iteratorName = user.FindMember("name");
        if (iteratorName == user.MemberEnd() || !iteratorName->value.IsString())
        {
            API_LOG_ERROR("parseAttachContactJSon: Invalid username in contact-attachment JSON");
            delete megaChatUsers;
            return NULL;
        }
        std::string nameString = iteratorName->value.GetString();

        MegaChatAttachedUser megaChatUser(MegaApi::base64ToUserHandle(handleString.c_str()) , emailString, nameString);
        megaChatUsers->push_back(megaChatUser);
    }

    return megaChatUsers;

}

std::string JSonUtils::generateGeolocationJSon(float longitude, float latitude, const char *img)
{
    std::string textMessage("https://www.google.com/maps/search/?api=1&query=");
    textMessage.append(std::to_string(latitude)).append(",").append(std::to_string(longitude));

    // Add generic `textMessage`
    rapidjson::Document jsonContainsMeta(rapidjson::kObjectType);
    rapidjson::Value jsonTextMessage(rapidjson::kStringType);
    jsonTextMessage.SetString(textMessage.c_str(), textMessage.length(), jsonContainsMeta.GetAllocator());
    jsonContainsMeta.AddMember(rapidjson::Value("textMessage"), jsonTextMessage, jsonContainsMeta.GetAllocator());

    // prepare geolocation object: longitude, latitude, image
    rapidjson::Value jsonGeolocation(rapidjson::kObjectType);
    // longitud
    rapidjson::Value jsonLongitude(rapidjson::kStringType);
    std::string longitudeString = std::to_string(longitude);
    jsonLongitude.SetString(longitudeString.c_str(), longitudeString.length());
    jsonGeolocation.AddMember(rapidjson::Value("lng"), jsonLongitude, jsonContainsMeta.GetAllocator());
    // latitude
    rapidjson::Value jsonLatitude(rapidjson::kStringType);
    std::string latitudeString = std::to_string(latitude);
    jsonLatitude.SetString(latitudeString.c_str(), latitudeString.length());
    jsonGeolocation.AddMember(rapidjson::Value("la"), jsonLatitude, jsonContainsMeta.GetAllocator());
    // image/thumbnail
    if (img)
    {
        rapidjson::Value jsonImage(rapidjson::kStringType);
        jsonImage.SetString(img, strlen(img), jsonContainsMeta.GetAllocator());
        jsonGeolocation.AddMember(rapidjson::Value("img"), jsonImage, jsonContainsMeta.GetAllocator());
    }

    // Add the `extra` with the geolocation data
    rapidjson::Value jsonExtra(rapidjson::kArrayType);
    jsonExtra.PushBack(jsonGeolocation, jsonContainsMeta.GetAllocator());
    jsonContainsMeta.AddMember(rapidjson::Value("extra"), jsonExtra, jsonContainsMeta.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    jsonContainsMeta.Accept(writer);

    // assemble final message with the type (contains-meta) and subtype (geolocation)
    std::string message(buffer.GetString(), buffer.GetSize());
    message.insert(message.begin(), Message::ContainsMetaSubType::kGeoLocation);
    message.insert(message.begin(), Message::kMsgContainsMeta - Message::kMsgOffset);
    message.insert(message.begin(), 0x0);

    return message;
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
        case MegaChatMessage::TYPE_VOICE_CLIP:  // fall-through
        case MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
            // Remove the first two characters. [0] = 0x0 | [1] = Message::kMsgAttachment/kMsgVoiceClip
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
        case MegaChatMessage::TYPE_CONTAINS_META:
        {
            if (content.size() > 4)
            {
                // Remove the first three characters. [0] = 0x0 | [1] = Message::kMsgContaintsMeta | [2] = subtype
                uint8_t containsMetaType = content.at(2);
                const char *containsMetaJson = content.data() + 3;
                const MegaChatContainsMeta *containsMeta = JSonUtils::parseContainsMeta(containsMetaJson, containsMetaType, true);
                messageContents = containsMeta->getTextMessage();
                delete containsMeta;
            }
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

const MegaChatContainsMeta* JSonUtils::parseContainsMeta(const char *json, uint8_t type, bool onlyTextMessage)
{
    MegaChatContainsMetaPrivate *containsMeta = new MegaChatContainsMetaPrivate();
    if (!json || !strlen(json))
    {
        API_LOG_ERROR("parseContainsMeta: invalid JSON struct - JSON contains no data, only includes type of meta");
        return containsMeta;
    }

    rapidjson::StringStream stringStream(json);

    rapidjson::Document document;
    document.ParseStream(stringStream);
    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        API_LOG_ERROR("parseContainsMeta: Parser JSON error");
        return containsMeta;
    }

    rapidjson::Value::ConstMemberIterator iteratorTextMessage = document.FindMember("textMessage");
    if (iteratorTextMessage == document.MemberEnd() || !iteratorTextMessage->value.IsString())
    {
        API_LOG_ERROR("parseRichPreview: invalid JSON struct - \"textMessage\" field not found");
        return containsMeta;
    }
    std::string textMessage = iteratorTextMessage->value.GetString();
    containsMeta->setTextMessage(textMessage);

    if (!onlyTextMessage)
    {
        switch (type)
        {
            case MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW:
            {
                MegaChatRichPreview *richPreview = parseRichPreview(document, textMessage);
                containsMeta->setRichPreview(richPreview);
                break;
            }
            case MegaChatContainsMeta::CONTAINS_META_GEOLOCATION:
            {
                MegaChatGeolocation *geolocation = parseGeolocation(document);
                containsMeta->setGeolocation(geolocation);
                break;
            }
            default:
            {
                API_LOG_ERROR("parseContainsMeta: unknown type of message with meta contained");
                break;
            }
        }
    }

    return containsMeta;
}

MegaChatRichPreview *JSonUtils::parseRichPreview(rapidjson::Document &document, std::string &textMessage)
{
    rapidjson::Value::ConstMemberIterator iteratorExtra = document.FindMember("extra");
    if (iteratorExtra == document.MemberEnd() || iteratorExtra->value.IsObject())
    {
        API_LOG_ERROR("parseRichPreview: invalid JSON struct - \"extra\" field not found");
        return NULL;
    }

    std::string title;
    std::string description;
    std::string image;
    std::string imageFormat;
    std::string icon;
    std::string iconFormat;
    std::string url;

    if (iteratorExtra->value.Capacity() == 1)
    {
        const rapidjson::Value& richPreview = iteratorExtra->value[0];

        rapidjson::Value::ConstMemberIterator iteratorTitle = richPreview.FindMember("t");
        if (iteratorTitle != richPreview.MemberEnd() && iteratorTitle->value.IsString())
        {
            title = iteratorTitle->value.GetString();
        }

        rapidjson::Value::ConstMemberIterator iteratorDescription = richPreview.FindMember("d");
        if (iteratorDescription != richPreview.MemberEnd() && iteratorDescription->value.IsString())
        {
            description = iteratorDescription->value.GetString();
        }

        rapidjson::Value::ConstMemberIterator iteratorImage = richPreview.FindMember("i");
        if (iteratorImage != richPreview.MemberEnd() && iteratorImage->value.IsString())
        {
            const char *imagePointer = iteratorImage->value.GetString();
            imageFormat = getImageFormat(imagePointer);
            imagePointer = imagePointer + imageFormat.size() + 1; // remove format.size() + ':'
            rapidjson::SizeType sizeImage = iteratorImage->value.GetStringLength() - (imageFormat.size() + 1);
            image = std::string(imagePointer, sizeImage);
        }

        rapidjson::Value::ConstMemberIterator iteratorIcon = richPreview.FindMember("ic");
        if (iteratorIcon != richPreview.MemberEnd() && iteratorIcon->value.IsString())
        {
            const char *iconPointer = iteratorIcon->value.GetString();
            iconFormat = getImageFormat(iconPointer);
            iconPointer = iconPointer + iconFormat.size() + 1; // remove format.size() + ':'
            rapidjson::SizeType sizeIcon = iteratorIcon->value.GetStringLength() - (iconFormat.size() + 1);
            icon = std::string(iconPointer, sizeIcon);
        }

        rapidjson::Value::ConstMemberIterator iteratorURL = richPreview.FindMember("url");
        if (iteratorURL != richPreview.MemberEnd() && iteratorURL->value.IsString())
        {
            url = iteratorURL->value.GetString();
        }
    }

    return new MegaChatRichPreviewPrivate(textMessage, title, description, image, imageFormat, icon, iconFormat, url);
}

MegaChatGeolocation *JSonUtils::parseGeolocation(rapidjson::Document &document)
{
    rapidjson::Value::ConstMemberIterator iteratorExtra = document.FindMember("extra");
    if (iteratorExtra == document.MemberEnd() || iteratorExtra->value.IsObject())
    {
        API_LOG_ERROR("parseGeolocation: invalid JSON struct - \"extra\" field not found");
        return NULL;
    }

    if (iteratorExtra->value.Capacity() != 1)
    {
        API_LOG_ERROR("parseGeolocation: invalid JSON struct - invalid format");
        return NULL;
    }

    float longitude;
    float latitude;
    std::string image;

    const rapidjson::Value &geolocationValue = iteratorExtra->value[0];

    rapidjson::Value::ConstMemberIterator iteratorLongitude = geolocationValue.FindMember("lng");
    if (iteratorLongitude != geolocationValue.MemberEnd() && iteratorLongitude->value.IsString())
    {
        const char *longitudeString = iteratorLongitude->value.GetString();
        longitude = atof(longitudeString);
    }
    else
    {
        API_LOG_ERROR("parseGeolocation: invalid JSON struct - \"lng\" not found");
        return NULL;
    }

    rapidjson::Value::ConstMemberIterator iteratorLatitude = geolocationValue.FindMember("la");
    if (iteratorLatitude != geolocationValue.MemberEnd() && iteratorLatitude->value.IsString())
    {
        const char *latitudeString = iteratorLatitude->value.GetString();
        latitude = atof(latitudeString);
    }
    else
    {
        API_LOG_ERROR("parseGeolocation: invalid JSON struct - \"la\" not found");
        return NULL;
    }

    rapidjson::Value::ConstMemberIterator iteratorImage = geolocationValue.FindMember("img");
    if (iteratorImage != geolocationValue.MemberEnd() && iteratorImage->value.IsString())
    {
        const char *imagePointer = iteratorImage->value.GetString();
        size_t imageSize = iteratorImage->value.GetStringLength();
        image.assign(imagePointer, imageSize);
    }
    else
    {
        API_LOG_WARNING("parseGeolocation: invalid JSON struct - \"img\" not found");
        // image is not mandatory
    }

    return new MegaChatGeolocationPrivate(longitude, latitude, image);
}

string JSonUtils::getImageFormat(const char *imagen)
{
    std::string format;

    size_t i = 0;
    while (imagen[i] != ':')
    {
        format += imagen[i];
        i++;
    }

    return format;
}

const char *MegaChatRichPreviewPrivate::getDomainName() const
{
    return mDomainName.c_str();
}

MegaChatGeolocationPrivate::MegaChatGeolocationPrivate(float longitude, float latitude, const string &image)
    : mLongitude(longitude), mLatitude(latitude), mImage(image)
{
}

MegaChatGeolocationPrivate::MegaChatGeolocationPrivate(const MegaChatGeolocationPrivate *geolocation)
{
    mLongitude = geolocation->mLongitude;
    mLatitude = geolocation->mLatitude;
    mImage = geolocation->mImage;
}

MegaChatGeolocation *MegaChatGeolocationPrivate::copy() const
{
    return new MegaChatGeolocationPrivate(this);
}

float MegaChatGeolocationPrivate::getLongitude() const
{
    return mLongitude;
}

float MegaChatGeolocationPrivate::getLatitude() const
{
    return mLatitude;
}

const char *MegaChatGeolocationPrivate::getImage() const
{
    return mImage.size() ? mImage.c_str() : NULL;
}

MegaChatNodeHistoryHandler::MegaChatNodeHistoryHandler(MegaChatApi *api)
    : chatApi(api)
{
}

void MegaChatNodeHistoryHandler::fireOnAttachmentReceived(MegaChatMessage *message)
{
    for(set<MegaChatNodeHistoryListener *>::iterator it = nodeHistoryListeners.begin(); it != nodeHistoryListeners.end() ; it++)
    {
        (*it)->onAttachmentReceived(chatApi, message);
    }

    delete message;
}

void MegaChatNodeHistoryHandler::fireOnAttachmentLoaded(MegaChatMessage *message)
{
    for(set<MegaChatNodeHistoryListener *>::iterator it = nodeHistoryListeners.begin(); it != nodeHistoryListeners.end() ; it++)
    {
        (*it)->onAttachmentLoaded(chatApi, message);
    }

    delete message;
}

void MegaChatNodeHistoryHandler::fireOnAttachmentDeleted(Id id)
{
    for(set<MegaChatNodeHistoryListener *>::iterator it = nodeHistoryListeners.begin(); it != nodeHistoryListeners.end() ; it++)
    {
        (*it)->onAttachmentDeleted(chatApi, id);
    }
}

void MegaChatNodeHistoryHandler::fireOnTruncate(Id id)
{
    for(set<MegaChatNodeHistoryListener *>::iterator it = nodeHistoryListeners.begin(); it != nodeHistoryListeners.end() ; it++)
    {
        (*it)->onTruncate(chatApi, id);
    }
}

void MegaChatNodeHistoryHandler::onReceived(Message *msg, Idx idx)
{
    MegaChatMessagePrivate *message = new MegaChatMessagePrivate(*msg, Message::Status::kServerReceived, idx);
    fireOnAttachmentReceived(message);
}

void MegaChatNodeHistoryHandler::onLoaded(Message *msg, Idx idx)
{
    MegaChatMessagePrivate *message = NULL;
    if (msg)
    {
        message = new MegaChatMessagePrivate(*msg, Message::Status::kServerReceived, idx);
    }

    fireOnAttachmentLoaded(message);
}

void MegaChatNodeHistoryHandler::onDeleted(Id id)
{
    fireOnAttachmentDeleted(id);
}

void MegaChatNodeHistoryHandler::onTruncated(Id id)
{
    fireOnTruncate(id);
}


void MegaChatNodeHistoryHandler::addMegaNodeHistoryListener(MegaChatNodeHistoryListener *listener)
{
    nodeHistoryListeners.insert(listener);
}

void MegaChatNodeHistoryHandler::removeMegaNodeHistoryListener(MegaChatNodeHistoryListener *listener)
{
    nodeHistoryListeners.insert(listener);
}
