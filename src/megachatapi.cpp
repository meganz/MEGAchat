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
    APP_ALWAYS_EXPORT const std::string& getAppDir()
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

MegaChatSession::~MegaChatSession()
{
}

MegaChatSession *MegaChatSession::copy()
{
    return NULL;
}

int MegaChatSession::getStatus() const
{
    return 0;
}

MegaChatHandle MegaChatSession::getPeerid() const
{
    return MEGACHAT_INVALID_HANDLE;
}

bool MegaChatSession::hasAudio() const
{
    return false;
}

bool MegaChatSession::hasVideo() const
{
    return false;
}

int MegaChatSession::getNetworkQuality() const
{
    return 0;
}

bool MegaChatSession::getAudioDetected() const
{
    return false;
}

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

MegaChatHandle MegaChatCall::getChatid() const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaChatHandle MegaChatCall::getId() const
{
    return MEGACHAT_INVALID_HANDLE;
}

bool MegaChatCall::hasLocalAudio() const
{
    return false;
}

bool MegaChatCall::hasAudioInitialCall() const
{
    return false;
}

bool MegaChatCall::hasLocalVideo() const
{
    return false;
}

bool MegaChatCall::hasVideoInitialCall() const
{
    return false;
}

int MegaChatCall::getChanges() const
{
    return 0;
}

bool MegaChatCall::hasChanged(int ) const
{
    return false;
}

int64_t MegaChatCall::getDuration() const
{
    return 0;
}

int64_t MegaChatCall::getInitialTimeStamp() const
{
    return 0;
}

int64_t MegaChatCall::getFinalTimeStamp() const
{
    return 0;
}

const char* MegaChatCall::getTemporaryError() const
{
    return NULL;
}

int MegaChatCall::getTermCode() const
{
    return TERM_CODE_NOT_FINISHED;
}

bool MegaChatCall::isLocalTermCode() const
{
    return false;
}

bool MegaChatCall::isRinging() const
{
    return false;
}

MegaHandleList *MegaChatCall::getSessions() const
{
    return NULL;
}

MegaChatSession *MegaChatCall::getMegaChatSession(MegaChatHandle /*peerId*/)
{
    return NULL;
}

MegaChatHandle MegaChatCall::getPeerSessionStatusChange() const
{
    return MEGACHAT_INVALID_HANDLE;
}

MegaHandleList *MegaChatCall::getParticipants() const
{
    return NULL;
}

int MegaChatCall::getNumParticipants() const
{
    return 0;
}

bool MegaChatCall::isIgnored() const
{
    return false;
}

bool MegaChatCall::isIncoming() const
{
    return false;
}

bool MegaChatCall::isOutgoing() const
{
    return false;
}

MegaChatHandle MegaChatCall::getCaller() const
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

const char *MegaChatApi::getAppDir()
{
    return karere::getAppDir().c_str();
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

void MegaChatApi::setLogToConsole(bool enable)
{
    MegaChatApiImpl::setLogToConsole(enable);
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

void MegaChatApi::connectInBackground(MegaChatRequestListener *listener)
{
    pImpl->connectInBackground(listener);
}

void MegaChatApi::disconnect(MegaChatRequestListener *listener)
{
    pImpl->disconnect(listener);
}

int MegaChatApi::getConnectionState()
{
    return pImpl->getConnectionState();
}

int MegaChatApi::getChatConnectionState(MegaChatHandle chatid)
{
    return pImpl->getChatConnectionState(chatid);
}

bool MegaChatApi::areAllChatsLoggedIn()
{
    return pImpl->areAllChatsLoggedIn();
}

void MegaChatApi::retryPendingConnections(bool disconnect, MegaChatRequestListener *listener)
{
    pImpl->retryPendingConnections(disconnect, listener);
}

void MegaChatApi::logout(MegaChatRequestListener *listener)
{
    pImpl->logout(listener);
}

void MegaChatApi::localLogout(MegaChatRequestListener *listener)
{
    pImpl->localLogout(listener);
}

void MegaChatApi::setOnlineStatus(int status, MegaChatRequestListener *listener)
{
    pImpl->setOnlineStatus(status, listener);
}

void MegaChatApi::setPresenceAutoaway(bool enable, int64_t timeout, MegaChatRequestListener *listener)
{
    pImpl->setPresenceAutoaway(enable, timeout, listener);
}

bool MegaChatApi::isSignalActivityRequired()
{
    return pImpl->isSignalActivityRequired();
}

void MegaChatApi::setPresencePersist(bool enable, MegaChatRequestListener *listener)
{
    pImpl->setPresencePersist(enable, listener);
}

void MegaChatApi::setLastGreenVisible(bool enable, MegaChatRequestListener *listener)
{
    pImpl->setLastGreenVisible(enable, listener);
}

void MegaChatApi::requestLastGreen(MegaChatHandle userid, MegaChatRequestListener *listener)
{
    pImpl->requestLastGreen(userid, listener);
}

void MegaChatApi::signalPresenceActivity(MegaChatRequestListener *listener)
{
    pImpl->signalPresenceActivity(listener);
}

int MegaChatApi::getOnlineStatus()
{
    return pImpl->getOnlineStatus();
}

bool MegaChatApi::isOnlineStatusPending()
{
    return pImpl->isOnlineStatusPending();
}

MegaChatPresenceConfig *MegaChatApi::getPresenceConfig()
{
    return pImpl->getPresenceConfig();
}

int MegaChatApi::getUserOnlineStatus(MegaChatHandle userhandle)
{
    return pImpl->getUserOnlineStatus(userhandle);
}

void MegaChatApi::setBackgroundStatus(bool background, MegaChatRequestListener *listener)
{
    pImpl->setBackgroundStatus(background, listener);
}

int MegaChatApi::getBackgroundStatus()
{
    return pImpl->getBackgroundStatus();
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

MegaChatHandle MegaChatApi::getUserHandleByEmail(const char *email)
{
    return pImpl->getUserHandleByEmail(email);
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

MegaChatListItemList *MegaChatApi::getChatListItemsByPeers(MegaChatPeerList *peers)
{
    return pImpl->getChatListItemsByPeers(peers);
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

MegaChatListItemList *MegaChatApi::getArchivedChatListItems()
{
    return pImpl->getArchivedChatListItems();
}

MegaChatListItemList *MegaChatApi::getUnreadChatListItems()
{
    return pImpl->getUnreadChatListItems();
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

void MegaChatApi::archiveChat(MegaChatHandle chatid, bool archive, MegaChatRequestListener *listener)
{
    pImpl->archiveChat(chatid, archive, listener);
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

MegaChatMessage *MegaChatApi::getMessageFromNodeHistory(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->getMessageFromNodeHistory(chatid, msgid);
}

MegaChatMessage *MegaChatApi::getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid)
{
    return pImpl->getManualSendingMessage(chatid, rowid);
}

MegaChatMessage *MegaChatApi::sendMessage(MegaChatHandle chatid, const char *msg)
{
    return pImpl->sendMessage(chatid, msg, msg ? strlen(msg) : 0);
}

MegaChatMessage *MegaChatApi::attachContacts(MegaChatHandle chatid, MegaHandleList *handles)
{
    return pImpl->attachContacts(chatid, handles);
}

MegaChatMessage *MegaChatApi::forwardContact(MegaChatHandle sourceChatid, MegaChatHandle msgid, MegaChatHandle targetChatId)
{
    return pImpl->forwardContact(sourceChatid, msgid, targetChatId);
}

void MegaChatApi::attachNodes(MegaChatHandle chatid, MegaNodeList *nodes, MegaChatRequestListener *listener)
{
    pImpl->attachNodes(chatid, nodes, listener);
}

MegaChatMessage * MegaChatApi::sendGeolocation(MegaChatHandle chatid, float longitude, float latitude, const char *img)
{
    return pImpl->sendGeolocation(chatid, longitude, latitude, img);
}

MegaChatMessage *MegaChatApi::editGeolocation(MegaChatHandle chatid, MegaChatHandle msgid, float longitude, float latitude, const char *img)
{
    return pImpl->editGeolocation(chatid, msgid, longitude, latitude, img);
}

void MegaChatApi::revokeAttachment(MegaChatHandle chatid, MegaChatHandle nodeHandle, MegaChatRequestListener *listener)
{
    pImpl->revokeAttachment(chatid, nodeHandle, listener);
}

void MegaChatApi::attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener)
{
    pImpl->attachNode(chatid, nodehandle, listener);
}

void MegaChatApi::attachVoiceMessage(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener)
{
    pImpl->attachVoiceMessage(chatid, nodehandle, listener);
}

MegaChatMessage *MegaChatApi::revokeAttachmentMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return deleteMessage(chatid, msgid);
}

bool MegaChatApi::isRevoked(MegaChatHandle chatid, MegaChatHandle nodeHandle) const
{
    return pImpl->isRevoked(chatid, nodeHandle);
}

MegaChatMessage *MegaChatApi::editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char *msg)
{
    if (!msg)   // force to use deleteMessage() to delete message instead
    {
        return NULL;
    }

    return pImpl->editMessage(chatid, msgid, msg, strlen(msg));
}

MegaChatMessage *MegaChatApi::deleteMessage(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->editMessage(chatid, msgid, NULL, 0);
}

MegaChatMessage *MegaChatApi::removeRichLink(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->removeRichLink(chatid, msgid);
}

bool MegaChatApi::setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid)
{
    return pImpl->setMessageSeen(chatid, msgid);
}

MegaChatMessage *MegaChatApi::getLastMessageSeen(MegaChatHandle chatid)
{
    return  pImpl->getLastMessageSeen(chatid);
}

MegaChatHandle MegaChatApi::getLastMessageSeenId(MegaChatHandle chatid)
{
    return pImpl->getLastMessageSeenId(chatid);
}

void MegaChatApi::removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowId)
{
    pImpl->removeUnsentMessage(chatid, rowId);
}

void MegaChatApi::sendTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->sendTypingNotification(chatid, listener);
}

void MegaChatApi::sendStopTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->sendStopTypingNotification(chatid, listener);
}

bool MegaChatApi::isMessageReceptionConfirmationActive() const
{
    return pImpl->isMessageReceptionConfirmationActive();
}

void MegaChatApi::saveCurrentState()
{
    pImpl->saveCurrentState();
}

void MegaChatApi::pushReceived(bool beep, MegaChatRequestListener *listener)
{
    pImpl->pushReceived(beep, MEGACHAT_INVALID_HANDLE, 0, listener);
}

void MegaChatApi::pushReceived(bool beep, MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->pushReceived(beep, chatid, 1, listener);
}

#ifndef KARERE_DISABLE_WEBRTC

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

void MegaChatApi::startChatCall(MegaChatHandle chatid, bool enableVideo, MegaChatRequestListener *listener)
{
    pImpl->startChatCall(chatid, enableVideo, listener);
}

void MegaChatApi::answerChatCall(MegaChatHandle chatid, bool enableVideo, MegaChatRequestListener *listener)
{
    pImpl->answerChatCall(chatid, enableVideo, listener);
}

void MegaChatApi::hangChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->hangChatCall(chatid, listener);
}

void MegaChatApi::hangAllChatCalls(MegaChatRequestListener *listener)
{
    pImpl->hangAllChatCalls(listener);
}

void MegaChatApi::enableAudio(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->setAudioEnable(chatid, true, listener);
}

void MegaChatApi::disableAudio(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->setAudioEnable(chatid, false, listener);
}

void MegaChatApi::enableVideo(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->setVideoEnable(chatid, true, listener);
}

void MegaChatApi::disableVideo(MegaChatHandle chatid, MegaChatRequestListener *listener)
{
    pImpl->setVideoEnable(chatid,false, listener);
}

void MegaChatApi::loadAudioVideoDeviceList(MegaChatRequestListener *listener)
{
    pImpl->loadAudioVideoDeviceList(listener);
}

MegaChatCall *MegaChatApi::getChatCall(MegaChatHandle chatid)
{
    return pImpl->getChatCall(chatid);
}

void MegaChatApi::setIgnoredCall(MegaChatHandle chatid)
{
    pImpl->setIgnoredCall(chatid);
}

MegaChatCall *MegaChatApi::getChatCallByCallId(MegaChatHandle callId)
{
    return pImpl->getChatCallByCallId(callId);
}

int MegaChatApi::getNumCalls()
{
    return pImpl->getNumCalls();
}

MegaHandleList *MegaChatApi::getChatCalls()
{
    return pImpl->getChatCalls();
}

MegaHandleList *MegaChatApi::getChatCallsIds()
{
    return pImpl->getChatCallsIds();
}

bool MegaChatApi::hasCallInChatRoom(MegaChatHandle chatid)
{
    return pImpl->hasCallInChatRoom(chatid);
}

void MegaChatApi::enableGroupChatCalls(bool enable)
{
    pImpl->enableGroupChatCalls(enable);
}

bool MegaChatApi::areGroupChatCallEnabled()
{
    return pImpl->areGroupChatCallEnabled();
}

int MegaChatApi::getMaxCallParticipants()
{
    return pImpl->getMaxCallParticipants();
}

int MegaChatApi::getMaxVideoCallParticipants()
{
    return pImpl->getMaxVideoCallParticipants();
}

void MegaChatApi::addChatCallListener(MegaChatCallListener *listener)
{
    pImpl->addChatCallListener(listener);
}

void MegaChatApi::removeChatCallListener(MegaChatCallListener *listener)
{
    pImpl->removeChatCallListener(listener);
}

void MegaChatApi::addChatLocalVideoListener(MegaChatHandle chatid, MegaChatVideoListener *listener)
{
    pImpl->addChatVideoListener(chatid, MEGACHAT_INVALID_HANDLE, listener);
}

void MegaChatApi::removeChatLocalVideoListener(MegaChatHandle chatid, MegaChatVideoListener *listener)
{
    pImpl->removeChatVideoListener(chatid, MEGACHAT_INVALID_HANDLE, listener);
}

void MegaChatApi::addChatRemoteVideoListener(MegaChatHandle chatid, MegaChatHandle peerid, MegaChatVideoListener *listener)
{
    pImpl->addChatVideoListener(chatid, peerid, listener);
}

void MegaChatApi::removeChatRemoteVideoListener(MegaChatHandle chatid, MegaChatHandle peerid, MegaChatVideoListener *listener)
{
    pImpl->removeChatVideoListener(chatid, peerid, listener);
}

#endif

void MegaChatApi::setCatchException(bool enable)
{
    MegaChatApiImpl::setCatchException(enable);
}

bool MegaChatApi::hasUrl(const char *text)
{
    return MegaChatApiImpl::hasUrl(text);
}

bool MegaChatApi::openNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    return pImpl->openNodeHistory(chatid, listener);
}

bool MegaChatApi::closeNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    return pImpl->closeNodeHistory(chatid, listener);
}

void MegaChatApi::addNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    pImpl->addNodeHistoryListener(chatid, listener);
}

void MegaChatApi::removeNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener)
{
    pImpl->removeNodeHistoryListener(chatid, listener);
}

int MegaChatApi::loadAttachments(MegaChatHandle chatid, int count)
{
    return pImpl->loadAttachments(chatid, count);
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

void MegaChatApi::removeChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener)
{
    pImpl->removeChatRoomListener(chatid, listener);
}

void MegaChatApi::addChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->addChatRequestListener(listener);
}

void MegaChatApi::removeChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->removeChatRequestListener(listener);
}

void MegaChatApi::addChatNotificationListener(MegaChatNotificationListener *listener)
{
    pImpl->addChatNotificationListener(listener);
}

void MegaChatApi::removeChatNotificationListener(MegaChatNotificationListener *listener)
{
    pImpl->removeChatNotificationListener(listener);
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

MegaHandleList *MegaChatRequest::getMegaHandleList()
{
    return NULL;
}

MegaHandleList *MegaChatRequest::getMegaHandleListByChat(MegaChatHandle)
{
    return NULL;
}

int MegaChatRequest::getParamType()
{
    return -1;
}

MegaChatRoomList *MegaChatRoomList::copy() const
{
    return NULL;
}

const MegaChatRoom *MegaChatRoomList::get(unsigned int /*i*/) const
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

int MegaChatRoom::getPeerPrivilegeByHandle(MegaChatHandle /*userhandle*/) const
{
    return PRIV_UNKNOWN;
}

const char *MegaChatRoom::getPeerFirstnameByHandle(MegaChatHandle /*userhandle*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerLastnameByHandle(MegaChatHandle /*userhandle*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerFullnameByHandle(MegaChatHandle /*userhandle*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerEmailByHandle(MegaChatHandle /*userhandle*/) const
{
    return NULL;
}

unsigned int MegaChatRoom::getPeerCount() const
{
    return 0;
}

MegaChatHandle MegaChatRoom::getPeerHandle(unsigned int /*i*/) const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatRoom::getPeerPrivilege(unsigned int /*i*/) const
{
    return PRIV_UNKNOWN;
}

const char *MegaChatRoom::getPeerFirstname(unsigned int /*i*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerLastname(unsigned int /*i*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerFullname(unsigned int /*i*/) const
{
    return NULL;
}

const char *MegaChatRoom::getPeerEmail(unsigned int /*i*/) const
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

bool MegaChatRoom::hasCustomTitle() const
{
    return false;
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

bool MegaChatRoom::isArchived() const
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


void MegaChatVideoListener::onChatVideoData(MegaChatApi * /*api*/, MegaChatHandle /*chatid*/, int /*width*/, int /*height*/, char * /*buffer*/, size_t /*size*/)
{

}


void MegaChatCallListener::onChatCallUpdate(MegaChatApi * /*api*/, MegaChatCall * /*call*/)
{

}

void MegaChatListener::onChatListItemUpdate(MegaChatApi * /*api*/, MegaChatListItem * /*item*/)
{

}

void MegaChatListener::onChatInitStateUpdate(MegaChatApi * /*api*/, int /*newState*/)
{

}

void MegaChatListener::onChatOnlineStatusUpdate(MegaChatApi* /*api*/, MegaChatHandle /*userhandle*/, int /*status*/, bool /*inProgress*/)
{

}

void MegaChatListener::onChatPresenceConfigUpdate(MegaChatApi * /*api*/, MegaChatPresenceConfig * /*config*/)
{

}

void MegaChatListener::onChatConnectionStateUpdate(MegaChatApi * /*api*/, MegaChatHandle /*chatid*/, int /*newState*/)
{

}

void MegaChatListener::onChatPresenceLastGreen(MegaChatApi * /*api*/, MegaChatHandle /*userhandle*/, int /*lastGreen*/)
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

bool MegaChatListItem::hasChanged(int /*changeType*/) const
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

MegaChatHandle MegaChatListItem::getLastMessageId() const
{
    return MEGACHAT_INVALID_HANDLE;
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

bool MegaChatListItem::isArchived() const
{
    return false;
}

bool MegaChatListItem::isCallInProgress() const
{
    return false;
}

MegaChatHandle MegaChatListItem::getPeerHandle() const
{
    return MEGACHAT_INVALID_HANDLE;
}

int MegaChatListItem::getLastMessagePriv() const
{
    return MegaChatRoom::PRIV_UNKNOWN;
}

MegaChatHandle MegaChatListItem::getLastMessageHandle() const
{
    return MEGACHAT_INVALID_HANDLE;
}

void MegaChatRoomListener::onChatRoomUpdate(MegaChatApi * /*api*/, MegaChatRoom * /*chat*/)
{

}

void MegaChatRoomListener::onMessageLoaded(MegaChatApi * /*api*/, MegaChatMessage * /*msg*/)
{

}

void MegaChatRoomListener::onMessageReceived(MegaChatApi * /*api*/, MegaChatMessage * /*msg*/)
{

}

void MegaChatRoomListener::onMessageUpdate(MegaChatApi * /*api*/, MegaChatMessage * /*msg*/)
{

}

void MegaChatRoomListener::onHistoryReloaded(MegaChatApi * /*api*/, MegaChatRoom * /*chat*/)
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

bool MegaChatMessage::isDeletable() const
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

const MegaChatContainsMeta *MegaChatMessage::getContainsMeta() const
{
    return NULL;
}

MegaChatRichPreview *MegaChatRichPreview::copy() const
{
    return NULL;
}

const char *MegaChatRichPreview::getText() const
{
    return NULL;
}

const char *MegaChatRichPreview::getTitle() const
{
    return NULL;
}

const char *MegaChatRichPreview::getDescription() const
{
    return NULL;
}

const char *MegaChatRichPreview::getImage() const
{
    return NULL;
}

const char *MegaChatRichPreview::getImageFormat() const
{
    return NULL;
}

const char *MegaChatRichPreview::getIcon() const
{
    return NULL;
}

const char *MegaChatRichPreview::getIconFormat() const
{
    return NULL;
}

const char *MegaChatRichPreview::getUrl() const
{
    return NULL;
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

MegaHandleList *MegaChatMessage::getMegaHandleList() const
{
    return NULL;
}

int MegaChatMessage::getDuration() const
{
    return 0;
}

int MegaChatMessage::getTermCode() const
{
    return 0;
}

void MegaChatLogger::log(int , const char *)
{

}


MegaChatListItemList *MegaChatListItemList::copy() const
{
    return NULL;
}

const MegaChatListItem *MegaChatListItemList::get(unsigned int /*i*/) const
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

bool MegaChatPresenceConfig::isLastGreenVisible() const
{
    return false;
}

void MegaChatNotificationListener::onChatNotification(MegaChatApi *, MegaChatHandle , MegaChatMessage *)
{

}

MegaChatContainsMeta *MegaChatContainsMeta::copy() const
{
    return NULL;
}

int MegaChatContainsMeta::getType() const
{
    return MegaChatContainsMeta::CONTAINS_META_INVALID;
}

const char *MegaChatContainsMeta::getTextMessage() const
{
    return NULL;
}

const MegaChatRichPreview *MegaChatContainsMeta::getRichPreview() const
{
    return NULL;
}

const MegaChatGeolocation *MegaChatContainsMeta::getGeolocation() const
{
    return NULL;
}

const char *MegaChatRichPreview::getDomainName() const
{
    return NULL;
}

MegaChatGeolocation *MegaChatGeolocation::copy() const
{
    return NULL;
}

float MegaChatGeolocation::getLongitude() const
{
    return 0;
}

float MegaChatGeolocation::getLatitude() const
{
    return 0;
}

const char *MegaChatGeolocation::getImage() const
{
    return NULL;
}

void MegaChatNodeHistoryListener::onAttachmentLoaded(MegaChatApi */*api*/, MegaChatMessage */*msg*/)
{
}

void MegaChatNodeHistoryListener::onAttachmentReceived(MegaChatApi */*api*/, MegaChatMessage */*msg*/)
{
}

void MegaChatNodeHistoryListener::onAttachmentDeleted(MegaChatApi */*api*/, MegaChatHandle /*msgid*/)
{
}

void MegaChatNodeHistoryListener::onTruncate(MegaChatApi */*api*/, MegaChatHandle /*msgid*/)
{
}
