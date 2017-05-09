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

void MegaChatApi::setLogWithColors(bool useColors)
{
    MegaChatApiImpl::setLogWithColors(useColors);
}

int MegaChatApi::init(const char *sid)
{
    return pImpl->init(sid);
}

int MegaChatApi::getInitState()
{
    return pImpl->getInitState();
}

void MegaChatApi::connect(MegaChatRequestListener *listener)
{
    pImpl->connect(listener);
}

void MegaChatApi::disconnect(MegaChatRequestListener *listener)
{
    pImpl->disconnect(listener);
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

void MegaChatApi::setPresenceAutoaway(bool enable, int64_t timeout)
{
    pImpl->setPresenceAutoaway(enable, timeout);
}

bool MegaChatApi::isSignalActivityRequired()
{
    return pImpl->isSignalActivityRequired();
}

void MegaChatApi::setPresencePersist(bool enable)
{
    pImpl->setPresencePersist(enable);
}

void MegaChatApi::signalPresenceActivity()
{
    pImpl->signalPresenceActivity();
}

int MegaChatApi::getOnlineStatus()
{
    return pImpl->getOnlineStatus();
}

MegaChatPresenceConfig *MegaChatApi::getPresenceConfig()
{
    return pImpl->getPresenceConfig();
}

int MegaChatApi::getUserOnlineStatus(MegaChatHandle userhandle)
{
    return pImpl->getUserOnlineStatus(userhandle);
}

void MegaChatApi::getUserFirstname(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    pImpl->getUserFirstname(userhandle, listener);
}

void MegaChatApi::getUserLastname(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    pImpl->getUserLastname(userhandle, listener);
}

void MegaChatApi::getUserEmail(MegaChatHandle userhandle, MegaChatRequestListener *listener)
{
    pImpl->getUserEmail(userhandle, listener);
}

char *MegaChatApi::getContactEmail(MegaChatHandle userhandle)
{
    return pImpl->getContactEmail(userhandle);
}

MegaChatHandle MegaChatApi::getMyUserHandle()
{
    return pImpl->getMyUserHandle();
}

char *MegaChatApi::getMyFirstname()
{
    return pImpl->getMyFirstname();
}

char *MegaChatApi::getMyLastname()
{
    return pImpl->getMyLastname();
}

char *MegaChatApi::getMyFullname()
{
    return pImpl->getMyFullname();
}

char *MegaChatApi::getMyEmail()
{
    return pImpl->getMyEmail();
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

MegaChatListItemList *MegaChatApi::getChatListItems()
{
    return pImpl->getChatListItems();
}

MegaChatListItem *MegaChatApi::getChatListItem(MegaChatHandle chatid)
{
    return pImpl->getChatListItem(chatid);
}

int MegaChatApi::getUnreadChats()
{
    return pImpl->getUnreadChats();
}

MegaChatListItemList *MegaChatApi::getActiveChatListItems()
{
    return pImpl->getActiveChatListItems();
}

MegaChatListItemList *MegaChatApi::getInactiveChatListItems()
{
    return pImpl->getInactiveChatListItems();
}

MegaChatHandle MegaChatApi::getChatHandleByUser(MegaChatHandle userhandle)
{
    return pImpl->getChatHandleByUser(userhandle);
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

MegaChatMessage *MegaChatApi::sendMessage(MegaChatHandle chatid, const char *msg)
{
    return pImpl->sendMessage(chatid, msg);
}

MegaChatMessage *MegaChatApi::attachContacts(MegaChatHandle chatid, unsigned int contactsNumber, MegaChatHandle *contacts)
{
    return pImpl->attachContacts(chatid, contactsNumber, contacts);
}

MegaChatMessage *MegaChatApi::attachContacts(MegaChatHandle chatid, MegaChatHandleList *handles)
{
   return pImpl->attachContacts(chatid, handles);
}

void MegaChatApi::attachNodes(MegaChatHandle chatid, MegaNodeList *nodes, MegaChatRequestListener *listener)
{
    pImpl->attachNodes(chatid, nodes, listener);
    return;
}

void MegaChatApi::revokeAttachment(MegaChatHandle chatid, MegaChatHandle nodeHandle, MegaChatRequestListener *listener)
{
    pImpl->revokeAttachment(chatid, nodeHandle, listener);
    return;
}

MegaChatMessage *MegaChatApi::editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char *msg)
{
    if (!msg)   // force to use deleteMessage() to delete message instead
    {
        return NULL;
    }

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

void MegaChatApi::removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowId)
{
    pImpl->removeUnsentMessage(chatid, rowId);
}

void MegaChatApi::sendTypingNotification(MegaChatHandle chatid)
{
    pImpl->sendTypingNotification(chatid);
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

MegaChatMessage *MegaChatRequest::getMegaChatMessage()
{
    return NULL;
}

MegaNodeList *MegaChatRequest::getMegaNodeList()
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

const char *MegaChatRoom::statusToString(int status)
{
    switch (status)
    {
    case MegaChatApi::STATUS_OFFLINE: return "offline";
    case MegaChatApi::STATUS_BUSY: return "busy";
    case MegaChatApi::STATUS_AWAY: return "away";
    case MegaChatApi::STATUS_ONLINE:return "online";
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

const char *MegaChatRoom::getPeerFirstnameByHandle(MegaChatHandle userhandle) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerLastnameByHandle(MegaChatHandle userhandle) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerFullnameByHandle(MegaChatHandle userhandle) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerEmailByHandle(MegaChatHandle userhandle) const
{
    return NULL;
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

const char *MegaChatRoom::getPeerFirstname(unsigned int i) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerLastname(unsigned int i) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerFullname(unsigned int i) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerEmail(unsigned int i) const
{
    return NULL;
}

bool MegaChatRoom::isGroup() const
{
    return false;
}

const char *MegaChatRoom::getTitle() const
{
    return NULL;
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

MegaChatHandle MegaChatRoom::getUserTyping() const
{
    return MEGACHAT_INVALID_HANDLE;
}

bool MegaChatRoom::isActive() const
{
    return false;
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

void MegaChatListener::onChatInitStateUpdate(MegaChatApi *api, int newState)
{

}

void MegaChatListener::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress)
{

}

void MegaChatListener::onChatPresenceConfigUpdate(MegaChatApi *api, MegaChatPresenceConfig *config)
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

int MegaChatListItem::getOwnPrivilege() const
{
    return PRIV_UNKNOWN;
}

int MegaChatListItem::getUnreadCount() const
{
    return 0;
}

const char *MegaChatListItem::getLastMessage() const
{
    return NULL;
}

int MegaChatListItem::getLastMessageType() const
{
    return MegaChatMessage::TYPE_INVALID;
}

MegaChatHandle MegaChatListItem::getLastMessageSender() const
{
    return MEGACHAT_INVALID_HANDLE;
}

int64_t MegaChatListItem::getLastTimestamp() const
{
    return 0;
}

bool MegaChatListItem::isGroup() const
{
    return false;
}

bool MegaChatListItem::isActive() const
{
    return false;
}

MegaChatHandle MegaChatListItem::getPeerHandle() const
{
    return MEGACHAT_INVALID_HANDLE;
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

int MegaChatMessage::getType() const
{
    return MegaChatMessage::TYPE_INVALID;
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

bool MegaChatMessage::isEditable() const
{
    return false;
}

bool MegaChatMessage::isManagementMessage() const
{
    return false;
}

MegaChatHandle MegaChatMessage::getHandleOfAction() const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatMessage::getPrivilege() const
{
    return MegaChatRoom::PRIV_UNKNOWN;
}

int MegaChatMessage::getChanges() const
{
    return 0;
}

bool MegaChatMessage::hasChanged(int) const
{
    return false;
}

int MegaChatMessage::getCode() const
{
    return 0;
}

unsigned int MegaChatMessage::getUsersCount() const
{
    return 0;
}

MegaChatHandle MegaChatMessage::getUserHandle(unsigned int) const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaChatHandle MegaChatMessage::getRowId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

const char *MegaChatMessage::getUserName(unsigned int) const
{
    return NULL;
}

const char *MegaChatMessage::getUserEmail(unsigned int) const
{
    return NULL;
}

MegaNodeList *MegaChatMessage::getMegaNodeList() const
{
    return NULL;
}

void MegaChatLogger::log(int , const char *)
{

}


MegaChatListItemList *MegaChatListItemList::copy() const
{
    return NULL;
}

const MegaChatListItem *MegaChatListItemList::get(unsigned int i) const
{
    return NULL;
}

unsigned int MegaChatListItemList::size() const
{
    return 0;
}

MegaChatPresenceConfig *MegaChatPresenceConfig::copy() const
{
    return NULL;
}

int MegaChatPresenceConfig::getOnlineStatus() const
{
    return MegaChatApi::STATUS_INVALID;
}

bool MegaChatPresenceConfig::isAutoawayEnabled() const
{
    return false;
}

int64_t MegaChatPresenceConfig::getAutoawayTimeout() const
{
    return 0;
}

bool MegaChatPresenceConfig::isPersist() const
{
    return false;
}

bool MegaChatPresenceConfig::isPending() const
{
    return false;
}

bool MegaChatPresenceConfig::isSignalActivityRequired() const
{
    return false;
}
