/**
 * @file megachatapi.cpp
 * @brief Intermediate layer for the MEGA Chat C++ SDK.
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


#include <megaapi_impl.h>
#include <megaapi.h>
#include "megachatapi.h"
#include "megachatapi_impl.h"

#include <chatClient.h>
#include <karereCommon.h>

// define the weak symbol for Logger to know where to create the log file
namespace karere
{
    APP_ALWAYS_EXPORT std::string getAppDir() 
    {
        #ifdef __ANDROID__
            return "/data/data/mega.privacy.android.app"; 
        #else
            return karere::createAppDir();
        #endif
    }
}

using namespace mega;
using namespace megachat;

MegaChatCall::~MegaChatCall()
{
}

MegaChatCall *MegaChatCall::copy()
{
    return NULL;
}

int MegaChatCall::getStatus() const
{
    return 0;
}

int MegaChatCall::getTag() const
{
    return 0;
}

MegaChatHandle MegaChatCall::getContactHandle() const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaChatApi::MegaChatApi(MegaApi *megaApi)
{
    this->pImpl = new MegaChatApiImpl(this, megaApi);
}

MegaChatApi::~MegaChatApi()
{
    delete pImpl;
}

void MegaChatApi::setLoggerObject(MegaChatLogger *megaLogger)
{
    MegaChatApiImpl::setLoggerClass(megaLogger);
}

void MegaChatApi::setLogLevel(int logLevel)
{
    MegaChatApiImpl::setLogLevel(logLevel);
}

void MegaChatApi::init(MegaChatRequestListener *listener)
{
    pImpl->init(listener);
}

void MegaChatApi::connect(MegaChatRequestListener *listener)
{
    pImpl->connect(listener);
}

void MegaChatApi::logout(MegaChatRequestListener *listener)
{
    pImpl->logout(listener);
}

void MegaChatApi::localLogout(MegaChatRequestListener *listener)
{
    pImpl->localLogout(listener);
}

//MegaChatApi::MegaChatApi(const char *appKey, const char *appDir)
//{
//    this->pImpl = new MegaChatApiImpl(this, appKey, appDir);
//}

void MegaChatApi::setOnlineStatus(int status, MegaChatRequestListener *listener)
{
    pImpl->setOnlineStatus(status, listener);
}

MegaChatRoomList *MegaChatApi::getChatRooms()
{
    return pImpl->getChatRooms();
}

MegaChatRoom *MegaChatApi::getChatRoom(MegaChatHandle chatid)
{
    return pImpl->getChatRoom(chatid);
}

MegaChatRoom *MegaChatApi::getChatRoomByUser(MegaChatHandle userhandle)
{
    return pImpl->getChatRoomByUser(userhandle);
}

void MegaChatApi::createChat(bool group, MegaChatPeerList *peers, MegaChatRequestListener *listener)
{
    pImpl->createChat(group, peers, listener);
}

void MegaChatApi::inviteToChat(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener)
{
    pImpl->inviteToChat(chatid, uh, privilege, listener);
}

void MegaChatApi::removeFromChat(MegaChatHandle chatid, MegaChatHandle uh, MegaChatRequestListener *listener)
{
    pImpl->removeFromChat(chatid, uh, listener);
}

void MegaChatApi::leaveChat(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->removeFromChat(chatid, MEGACHAT_INVALID_HANDLE, listener);
}

void MegaChatApi::updateChatPermissions(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener)
{
    pImpl->updateChatPermissions(chatid, uh, privilege, listener);
}

void MegaChatApi::truncateChat(MegaChatHandle chatid, MegaChatHandle messageid, MegaChatRequestListener *listener)
{
    pImpl->truncateChat(chatid, messageid, listener);
}

void MegaChatApi::clearChatHistory(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->truncateChat(chatid, MEGACHAT_INVALID_HANDLE, listener);
}

void MegaChatApi::setChatTitle(MegaChatHandle chatid, const char *title, MegaChatRequestListener *listener)
{
    pImpl->setChatTitle(chatid, title, listener);
}

bool MegaChatApi::openChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    return pImpl->openChatRoom(chatid, listener);
}

void MegaChatApi::closeChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    pImpl->closeChatRoom(chatid, listener);
}

int MegaChatApi::loadMessages(MegaChatHandle chatid, int count)
{
    return pImpl->loadMessages(chatid, count);
}

bool MegaChatApi::isFullHistoryLoaded(MegaChatHandle chatid)
{
    return pImpl->isFullHistoryLoaded(chatid);
}

MegaChatMessage *MegaChatApi::getMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->getMessage(chatid, msgid);
}

MegaChatMessage *MegaChatApi::sendMessage(MegaChatHandle chatid, const char *msg, MegaChatMessage::Type type)
{
    return pImpl->sendMessage(chatid, msg, type);
}

MegaChatMessage *MegaChatApi::editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char *msg)
{
    return pImpl->editMessage(chatid, msgid, msg);
}

MegaChatMessage *MegaChatApi::deleteMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->editMessage(chatid, msgid, NULL);
}

bool MegaChatApi::setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->setMessageSeen(chatid, msgid);
}

MegaChatMessage *MegaChatApi::getLastMessageSeen(MegaChatHandle chatid)
{
    return  pImpl->getLastMessageSeen(chatid);
}

MegaStringList *MegaChatApi::getChatAudioInDevices()
{
    return pImpl->getChatAudioInDevices();
}

MegaStringList *MegaChatApi::getChatVideoInDevices()
{
    return pImpl->getChatVideoInDevices();
}

bool MegaChatApi::setChatAudioInDevice(const char *device)
{
    return pImpl->setChatAudioInDevice(device);
}

bool MegaChatApi::setChatVideoInDevice(const char *device)
{
    return pImpl->setChatVideoInDevice(device);
}

void MegaChatApi::startChatCall(MegaUser *peer, bool enableVideo, MegaChatRequestListener *listener)
{
    pImpl->startChatCall(peer, enableVideo, listener);
}

void MegaChatApi::answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener)
{
    pImpl->answerChatCall(call, accept, listener);
}

void MegaChatApi::hangAllChatCalls()
{
    pImpl->hangAllChatCalls();
}

void MegaChatApi::addChatCallListener(MegaChatCallListener *listener)
{
    pImpl->addChatCallListener(listener);
}

void MegaChatApi::removeChatCallListener(MegaChatCallListener *listener)
{
    pImpl->removeChatCallListener(listener);
}

void MegaChatApi::addChatLocalVideoListener(MegaChatVideoListener *listener)
{
    pImpl->addChatLocalVideoListener(listener);
}

void MegaChatApi::removeChatLocalVideoListener(MegaChatVideoListener *listener)
{
    pImpl->removeChatLocalVideoListener(listener);
}

void MegaChatApi::addChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    pImpl->addChatRemoteVideoListener(listener);
}

void MegaChatApi::removeChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    pImpl->removeChatRemoteVideoListener(listener);
}

void MegaChatApi::addChatListener(MegaChatListener *listener)
{
    pImpl->addChatListener(listener);
}

void MegaChatApi::removeChatListener(MegaChatListener *listener)
{
    pImpl->removeChatListener(listener);
}

void MegaChatApi::addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    pImpl->addChatRoomListener(chatid, listener);
}

void MegaChatApi::removeChatRoomListener(MegaChatRoomListener *listener)
{
    pImpl->removeChatRoomListener(listener);
}

void MegaChatApi::addChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->addChatRequestListener(listener);
}

void MegaChatApi::removeChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->removeChatRequestListener(listener);
}

MegaChatRequest::~MegaChatRequest() { }
MegaChatRequest *MegaChatRequest::copy()
{
    return NULL;
}

int MegaChatRequest::getType() const
{
    return 0;
}

const char *MegaChatRequest::getRequestString() const
{
    return NULL;
}

const char *MegaChatRequest::toString() const
{
    return NULL;
}

int MegaChatRequest::getTag() const
{
    return 0;
}

long long MegaChatRequest::getNumber() const
{
    return 0;
}

int MegaChatRequest::getNumRetry() const
{
    return 0;
}

bool MegaChatRequest::getFlag() const
{
    return false;
}

MegaChatPeerList *MegaChatRequest::getMegaChatPeerList()
{
    return NULL;
}

MegaHandle MegaChatRequest::getChatHandle()
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaHandle MegaChatRequest::getUserHandle()
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatRequest::getPrivilege()
{
    return MegaChatPeerList::PRIV_UNKNOWN;
}

const char *MegaChatRequest::getText() const
{
    return NULL;
}

MegaChatRoomList *MegaChatRoomList::copy() const
{
    return NULL;
}

const MegaChatRoom *MegaChatRoomList::get(unsigned int i) const
{
    return NULL;
}

unsigned int MegaChatRoomList::size() const
{
    return 0;
}

//Request callbacks
void MegaChatRequestListener::onRequestStart(MegaChatApi *, MegaChatRequest *)
{ }
void MegaChatRequestListener::onRequestFinish(MegaChatApi *, MegaChatRequest *, MegaChatError *)
{ }
void MegaChatRequestListener::onRequestUpdate(MegaChatApi *, MegaChatRequest *)
{ }
void MegaChatRequestListener::onRequestTemporaryError(MegaChatApi *, MegaChatRequest *, MegaChatError *)
{ }
MegaChatRequestListener::~MegaChatRequestListener() {}


MegaChatRoom *MegaChatRoom::copy() const
{
    return NULL;
}

const char *MegaChatRoom::privToString(int priv)
{
    switch (priv)
    {
    case PRIV_RM: return "removed";
    case PRIV_RO: return "read-only";
    case PRIV_STANDARD: return "standard";
    case PRIV_MODERATOR:return "moderator";
    case PRIV_UNKNOWN:
    default: return "unknown privilege";
    }
}

const char *MegaChatRoom::stateToString(int status)
{
    switch (status)
    {
    case STATE_OFFLINE: return "offline";
    case STATE_CONNECTING: return "connecting";
    case STATE_JOINING: return "joining";
    case STATE_ONLINE:return "online";
    default: return "unknown state";
    }
}

const char *MegaChatRoom::statusToString(MegaChatApi::Status status)
{
    switch (status)
    {
    case MegaChatApi::STATUS_OFFLINE: return "offline";
    case MegaChatApi::STATUS_BUSY: return "busy";
    case MegaChatApi::STATUS_AWAY: return "away";
    case MegaChatApi::STATUS_ONLINE:return "online";
    case MegaChatApi::STATUS_CHATTY:return "chatty";
    default: return "unknown status";
    }
}

MegaChatHandle MegaChatRoom::getChatId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatRoom::getOwnPrivilege() const
{
    return PRIV_UNKNOWN;
}

int MegaChatRoom::getPeerPrivilegeByHandle(MegaChatHandle userhandle) const
{
    return PRIV_UNKNOWN;
}

unsigned int MegaChatRoom::getPeerCount() const
{
    return 0;
}

MegaChatHandle MegaChatRoom::getPeerHandle(unsigned int i) const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatRoom::getPeerPrivilege(unsigned int i) const
{
    return PRIV_UNKNOWN;
}

bool MegaChatRoom::isGroup() const
{
    return false;
}

const char *MegaChatRoom::getTitle() const
{
    return NULL;
}

int MegaChatRoom::getOnlineState() const
{
    return MegaChatRoom::STATE_OFFLINE;
}

int MegaChatRoom::getChanges() const
{
    return 0;
}

bool MegaChatRoom::hasChanged(int) const
{
    return false;
}

int MegaChatRoom::getUnreadCount() const
{
    return 0;
}

MegaChatApi::Status MegaChatRoom::getOnlineStatus() const
{
    return MegaChatApi::STATUS_OFFLINE;
}

MegaChatPeerList * MegaChatPeerList::createInstance()
{
    return new MegaChatPeerListPrivate();
}

MegaChatPeerList::MegaChatPeerList()
{

}

MegaChatPeerList::~MegaChatPeerList()
{

}

MegaChatPeerList *MegaChatPeerList::copy() const
{
    return NULL;
}

void MegaChatPeerList::addPeer(MegaChatHandle, int)
{
}

MegaChatHandle MegaChatPeerList::getPeerHandle(int) const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatPeerList::getPeerPrivilege(int) const
{
    return PRIV_UNKNOWN;
}

int MegaChatPeerList::size() const
{
    return 0;
}


void MegaChatVideoListener::onChatVideoData(MegaChatApi *api, MegaChatCall *chatCall, int width, int height, char *buffer)
{

}


void MegaChatCallListener::onChatCallStart(MegaChatApi *api, MegaChatCall *call)
{

}

void MegaChatCallListener::onChatCallStateChange(MegaChatApi *api, MegaChatCall *call)
{

}

void MegaChatCallListener::onChatCallTemporaryError(MegaChatApi *api, MegaChatCall *call, MegaChatError *error)
{

}

void MegaChatCallListener::onChatCallFinish(MegaChatApi *api, MegaChatCall *call, MegaChatError *error)
{

}


void MegaChatListener::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{

}

void MegaChatListener::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chats)
{

}

MegaChatListItem *MegaChatListItem::copy() const
{
    return NULL;
}

int MegaChatListItem::getChanges() const
{
    return 0;
}

bool MegaChatListItem::hasChanged(int changeType) const
{
    return 0;
}

MegaChatHandle MegaChatListItem::getChatId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

const char *MegaChatListItem::getTitle() const
{
    return NULL;
}

int MegaChatListItem::getVisibility() const
{
    return VISIBILITY_UNKNOWN;
}

int MegaChatListItem::getUnreadCount() const
{
    return 0;
}

MegaChatApi::Status MegaChatListItem::getOnlineStatus() const
{
    return MegaChatApi::STATUS_OFFLINE;
}


void MegaChatRoomListener::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{

}

void MegaChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{

}

void MegaChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{

}

void MegaChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{

}

MegaChatMessage *MegaChatMessage::copy() const
{
    return NULL;
}

int MegaChatMessage::getStatus() const
{
    return MegaChatMessage::STATUS_UNKNOWN;
}

MegaChatHandle MegaChatMessage::getMsgId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaChatHandle MegaChatMessage::getTempId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatMessage::getMsgIndex() const
{
    return 0;
}

MegaChatHandle MegaChatMessage::getUserHandle() const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaChatMessage::Type MegaChatMessage::getType() const
{
    return MegaChatMessage::TYPE_UNKNOWN;
}

int64_t MegaChatMessage::getTimestamp() const
{
    return 0;
}

const char *MegaChatMessage::getContent() const
{
    return NULL;
}

bool MegaChatMessage::isEdited() const
{
    return false;
}

bool MegaChatMessage::isDeleted() const
{
    return false;
}

int MegaChatMessage::getChanges() const
{
    return 0;
}

bool MegaChatMessage::hasChanged(int) const
{
    return false;
}


void MegaChatLogger::log(int , const char *)
{

}
