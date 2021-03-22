/**
 * @file megachatapi_impl.h
 * @brief Private header file of the intermediate layer for the MEGA Chat C++ SDK.
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

#ifndef MEGACHATAPI_IMPL_H
#define MEGACHATAPI_IMPL_H

#include "megachatapi.h"

//the megaapi.h header needs this defined externally
//#ifndef ENABLE_CHAT
//    #define ENABLE_CHAT 1
//#endif
#include <megaapi.h>
#include <megaapi_impl.h>

#include <rtcModule/webrtc.h>

#ifndef KARERE_DISABLE_WEBRTC
#include <IVideoRenderer.h>
#endif

#include <chatClient.h>
#include <chatd.h>
#include <sdkApi.h>
#include <karereCommon.h>
#include <logger.h>
#include <stdint.h>
#include "net/libwebsocketsIO.h"
#include "waiter/libuvWaiter.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996) // rapidjson: The std::iterator class template (used as a base class to provide typedefs) is deprecated in C++17. (The <iterator> header is NOT deprecated.) 
#endif

#include <rapidjson/document.h>

#ifdef _WIN32
#pragma warning(pop)
#endif


typedef LibwebsocketsIO MegaWebsocketsIO;
typedef ::mega::LibuvWaiter MegaChatWaiter;

namespace megachat
{
    
typedef std::set<MegaChatVideoListener *> MegaChatVideoListener_set;
typedef std::map<uint32_t, MegaChatVideoListener_set> MegaChatPeerVideoListener_map;

class MegaChatRequestPrivate : public MegaChatRequest
{

public:
    MegaChatRequestPrivate(int type, MegaChatRequestListener *listener = NULL);
    MegaChatRequestPrivate(MegaChatRequestPrivate &request);
    virtual ~MegaChatRequestPrivate();
    MegaChatRequest *copy();
    virtual int getType() const;
    virtual MegaChatRequestListener *getListener() const;
    virtual const char *getRequestString() const;
    virtual const char* toString() const;
    virtual int getTag() const;
    virtual long long getNumber() const;
    virtual int getNumRetry() const;
    virtual bool getFlag() const;
    virtual MegaChatPeerList *getMegaChatPeerList();
    virtual MegaChatHandle getChatHandle();
    virtual MegaChatHandle getUserHandle();
    virtual int getPrivilege();
    virtual const char *getText() const;
    virtual const char *getLink() const;
    virtual MegaChatMessage *getMegaChatMessage();
    virtual mega::MegaNodeList *getMegaNodeList();
    virtual mega::MegaHandleList *getMegaHandleListByChat(MegaChatHandle chatid);
    virtual mega::MegaHandleList *getMegaHandleList();
    virtual int getParamType();

    void setTag(int tag);
    void setListener(MegaChatRequestListener *listener);
    void setNumber(long long number);
    void setNumRetry(int retry);
    void setFlag(bool flag);
    void setMegaChatPeerList(MegaChatPeerList *peerList);
    void setChatHandle(MegaChatHandle chatid);
    void setUserHandle(MegaChatHandle userhandle);
    void setPrivilege(int priv);
    void setLink(const char *link);
    void setText(const char *text);
    void setMegaChatMessage(MegaChatMessage *message);
    void setMegaNodeList(mega::MegaNodeList *nodelist);
    void setMegaHandleList(mega::MegaHandleList *handlelist);
    void setMegaHandleListByChat(MegaChatHandle chatid, mega::MegaHandleList *handlelist);
    void setParamType(int paramType);

protected:
    int type;
    int tag;
    MegaChatRequestListener *listener;

    long long number;
    int retry;
    bool flag;
    MegaChatPeerList *peerList;
    MegaChatHandle chatid;
    MegaChatHandle userHandle;
    int privilege;
    const char* text;
    const char* link;
    MegaChatMessage* mMessage;
    mega::MegaNodeList* mMegaNodeList;
    mega::MegaHandleList *mMegaHandleList;
    std::map<MegaChatHandle, mega::MegaHandleList*> mMegaHandleListMap;
    int mParamType;
};

class MegaChatPresenceConfigPrivate : public MegaChatPresenceConfig
{
public:
    MegaChatPresenceConfigPrivate(const MegaChatPresenceConfigPrivate &config);
    MegaChatPresenceConfigPrivate(const presenced::Config &config, bool isPending);
    virtual ~MegaChatPresenceConfigPrivate();
    MegaChatPresenceConfig *copy() const override;

    int getOnlineStatus() const override;
    bool isAutoawayEnabled() const override;
    int64_t getAutoawayTimeout() const override;
    bool isPersist() const override;
    bool isPending() const override;
    bool isLastGreenVisible() const override;

private:
    int status;
    bool persistEnabled;
    bool autoawayEnabled;
    int64_t autoawayTimeout;
    bool pending;
    bool lastGreenVisible;
};


#ifndef KARERE_DISABLE_WEBRTC

class MegaChatVideoReceiver;

class MegaChatSessionPrivate : public MegaChatSession
{
public:
    MegaChatSessionPrivate(const rtcModule::ISession &session);
    MegaChatSessionPrivate(const MegaChatSessionPrivate &session);
    virtual ~MegaChatSessionPrivate() override;
    virtual MegaChatSession *copy() override;
    virtual int getStatus() const override;
    virtual MegaChatHandle getPeerid() const override;
    virtual MegaChatHandle getClientid() const override;
    virtual bool hasAudio() const override;
    virtual bool hasVideo() const override;
    virtual bool isHiResVideo() const override;
    virtual bool isLowResVideo() const override;
    virtual bool getAudioDetected() const override;
    virtual bool isOnHold() const override;
    virtual int getChanges() const override;
    virtual bool hasChanged(int changeType) const override;
    virtual bool isModerator() const override;
    virtual bool isAudioDetected() const override;
    virtual bool hasRequestSpeak() const override;
    virtual bool canRecvVideoHiRes() const;
    virtual bool canRecvVideoLowRes() const;

    karere::AvFlags getAvFlags() const; // for internal use
    void setState(uint8_t state);
    void setAvFlags(karere::AvFlags flags);
    void setNetworkQuality(int quality);
    void setAudioDetected(bool audioDetected);
    void setSessionFullyOperative();
    void setOnHold(bool onHold);
    void setTermCode(int termCode);
    void setChange(int change);
    void removeChanges();

private:
    uint8_t state = MegaChatSession::SESSION_STATUS_INVALID;
    karere::Id peerid;
    uint32_t clientid;
    karere::AvFlags mAvFlags = 0;
    int mChanged = MegaChatSession::CHANGE_TYPE_NO_CHANGES;
    karere::AvFlags mAVFlags;
    bool mHasRequestSpeak = false;
    bool mIsModerator = false;
    bool mAudioDetected = false;
    bool mHasHiResTrack = false;
    bool mHasLowResTrack = false;
};

class MegaChatCallPrivate : public MegaChatCall
{
public:
    MegaChatCallPrivate(const rtcModule::ICall& call);
    MegaChatCallPrivate(const MegaChatCallPrivate &call);

    virtual ~MegaChatCallPrivate() override;

    virtual MegaChatCall *copy() override;

    virtual int getStatus() const override;
    virtual MegaChatHandle getChatid() const override;
    virtual MegaChatHandle getCallId() const override;

    virtual bool hasLocalAudio() const override;
    virtual bool hasLocalVideo() const override;

    virtual int getChanges() const override;
    virtual bool hasChanged(int changeType) const override;
    virtual bool isAudioDetected() const override;

    virtual int64_t getDuration() const override;
    virtual int64_t getInitialTimeStamp() const override;
    virtual int64_t getFinalTimeStamp() const override;
    virtual int getTermCode() const override;
    virtual bool isRinging() const override;
    virtual mega::MegaHandleList *getSessionsClientid() const override;
    virtual MegaChatHandle getClientidCallCompositionChange() const override;
    virtual int getCallCompositionChange() const override;
    virtual MegaChatSession *getMegaChatSession(MegaChatHandle clientid) override;
    virtual int getNumParticipants() const override;
    virtual mega::MegaHandleList *getPeeridParticipants() const override;
    virtual bool isIgnored() const override;
    virtual bool isIncoming() const override;
    virtual bool isOutgoing() const override;
    virtual MegaChatHandle getCaller() const override;
    virtual bool isOnHold() const override;
    virtual bool isModerator() const override;
    bool isSpeakAllow() const override;

    void setStatus(int status);
    void setLocalAudioVideoFlags(karere::AvFlags localAVFlags);
    void removeChanges();
    void setChange(int mChanged);
    void setTermCode(rtcModule::TermCode termCode);
    void setIsRinging(bool ringing);
    MegaChatSessionPrivate *addSession(rtcModule::ISession &sess);

    int availableAudioSlots();
    int availableVideoSlots();
    void setPeerid(karere::Id peerid, bool added);
    bool isParticipating(karere::Id userid);
    void setId(karere::Id callid);
    void setCaller(karere::Id caller);
    void setOnHold(bool onHold);
    void setAudioDetected(bool mAudioDetected);
    static void convertTermCode(rtcModule::TermCode termCode, int &megaTermCode, bool &local);
    static int convertCallState(rtcModule::CallState newState);

protected:
    MegaChatHandle chatid;
    int status = MegaChatCall::CALL_STATUS_INITIAL;
    MegaChatHandle callid;
    karere::AvFlags localAVFlags;
    int mChanged = MegaChatCall::CHANGE_TYPE_NO_CHANGES;
    int64_t mInitialTs;
    int64_t mFinalTs;
    std::map<MegaChatHandle, std::unique_ptr<MegaChatSession>> mSessions;
    std::map<MegaChatHandle, karere::AvFlags> participants;
    MegaChatHandle mPeerId;
    int callCompositionChange = MegaChatCall::NO_COMPOSITION_CHANGE;
    MegaChatHandle callerId;

    int termCode;
    bool mIgnored;
    bool mAudioDetected = false;
    bool ringing = false;
    bool mIsCaller;
    bool mIsModerator = false;
    bool mIsSpeakAllow = false;
};

class MegaChatVideoFrame
{
public:
    unsigned char *buffer;
    int width;
    int height;
};

class MegaChatVideoReceiver : public rtcModule::IVideoRenderer
{
public:
    // no peerid --> local video from own user
    MegaChatVideoReceiver(MegaChatApiImpl *mChatApi, karere::Id mChatid, bool hiRes, uint32_t mClientid = 0);
    ~MegaChatVideoReceiver();

    void setWidth(int width);
    void setHeight(int height);

    // rtcModule::IVideoRenderer implementation
    virtual void* getImageBuffer(unsigned short width, unsigned short height, void*& userData);
    virtual void frameComplete(void* userData);
    virtual void onVideoAttach();
    virtual void onVideoDetach();
    virtual void clearViewport();
    virtual void released();

protected:
    MegaChatApiImpl *mChatApi;
    MegaChatHandle mChatid;
    bool mHiRes = false;
    uint32_t mClientid;
};

#endif

class MegaChatListItemPrivate : public MegaChatListItem
{
public:
    MegaChatListItemPrivate(karere::ChatRoom& chatroom);
    MegaChatListItemPrivate(const MegaChatListItem *item);
    virtual ~MegaChatListItemPrivate();
    virtual MegaChatListItem *copy() const;

private:
    int changed;

    MegaChatHandle chatid;
    int ownPriv;
    std::string title;
    int unreadCount;
    std::string lastMsg;
    int lastMsgType;
    MegaChatHandle lastMsgSender;
    int64_t lastTs;
    bool group;
    bool mPublicChat;
    bool mPreviewMode;
    bool active;
    bool archived;
    bool mIsCallInProgress;
    MegaChatHandle peerHandle;  // only for 1on1 chatrooms
    MegaChatHandle mLastMsgId;
    int lastMsgPriv;
    MegaChatHandle lastMsgHandle;
    unsigned int mNumPreviewers;

public:
    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual MegaChatHandle getChatId() const;
    virtual const char *getTitle() const;
    virtual int getOwnPrivilege() const;
    virtual int getUnreadCount() const;
    virtual const char *getLastMessage() const;
    virtual MegaChatHandle getLastMessageId() const;
    virtual int getLastMessageType() const;
    virtual MegaChatHandle getLastMessageSender() const;
    virtual int64_t getLastTimestamp() const;
    virtual bool isGroup() const;
    virtual bool isPublic() const;
    virtual bool isPreview() const;
    virtual bool isActive() const;
    virtual bool isArchived() const;
    virtual bool isCallInProgress() const;
    virtual MegaChatHandle getPeerHandle() const;
    virtual int getLastMessagePriv() const;
    virtual MegaChatHandle getLastMessageHandle() const;
    virtual unsigned int getNumPreviewers() const;

    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void changeUnreadCount();
    void setNumPreviewers(unsigned int numPrev);
    void setPreviewClosed();
    void setMembersUpdated();
    void setClosed();
    void setLastTimestamp(int64_t ts);
    void setArchived(bool);
    void setCallInProgress();

    /**
     * If the message is of type MegaChatMessage::TYPE_ATTACHMENT, this function
     * recives the filenames of the attached nodes. The filenames of nodes are separated
     * by ASCII character '0x01'
     * If the message is of type MegaChatMessage::TYPE_CONTACT, this function
     * recives the usernames. The usernames are separated
     * by ASCII character '0x01'
     */
    void setLastMessage();
    void setChatMode(bool mode);
};

class MegaChatListItemHandler :public virtual karere::IApp::IChatListItem
{
public:
    MegaChatListItemHandler(MegaChatApiImpl&, karere::ChatRoom&);

    // karere::IApp::IListItem::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged();

    // karere::IApp::IListItem::IChatListItem implementation
    virtual void onExcludedFromChat();
    virtual void onRejoinedChat();
    virtual void onLastMessageUpdated(const chatd::LastTextMsg& msg);
    virtual void onLastTsUpdated(uint32_t ts);
    virtual void onChatOnlineState(const chatd::ChatState state);
    virtual void onChatModeChanged(bool mode);
    virtual void onChatArchived(bool archived);
    virtual void onPreviewersCountUpdate(uint32_t numPrev);
    virtual void onPreviewClosed();

protected:
    MegaChatApiImpl &chatApi;
    karere::ChatRoom &mRoom;
};

class MegaChatGroupListItemHandler :
        public MegaChatListItemHandler,
        public virtual karere::IApp::IGroupChatListItem
{
public:
    MegaChatGroupListItemHandler(MegaChatApiImpl&, karere::ChatRoom&);

    // karere::IApp::IListItem::IGroupChatListItem implementation
    virtual void onUserJoin(uint64_t userid, chatd::Priv priv);
    virtual void onUserLeave(uint64_t userid);
};

class MegaChatPeerListItemHandler :
        public MegaChatListItemHandler,
        public virtual karere::IApp::IPeerChatListItem
{
public:
    MegaChatPeerListItemHandler(MegaChatApiImpl &, karere::ChatRoom&);
};

class MegaChatRoomHandler :public karere::IApp::IChatHandler
{
public:
    MegaChatRoomHandler(MegaChatApiImpl *chatApiImpl, MegaChatApi *chatApi, mega::MegaApi *megaApi, MegaChatHandle chatid);

    void addChatRoomListener(MegaChatRoomListener *listener);
    void removeChatRoomListener(MegaChatRoomListener *listener);

    // MegaChatRoomListener callbacks
    void fireOnChatRoomUpdate(MegaChatRoom *chat);
    void fireOnMessageLoaded(MegaChatMessage *msg);
    void fireOnMessageReceived(MegaChatMessage *msg);
    void fireOnHistoryTruncatedByRetentionTime(MegaChatMessage *msg);
    void fireOnMessageUpdate(MegaChatMessage *msg);
    void fireOnHistoryReloaded(MegaChatRoom *chat);
    void fireOnReactionUpdate(MegaChatHandle msgid, const char *reaction, int count);
    // karere::IApp::IChatHandler implementation
    virtual void onMemberNameChanged(uint64_t userid, const std::string &newName);
    virtual void onChatArchived(bool archived);
    //virtual void* userp();


    // karere::IApp::IChatHandler::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged();
    virtual void onPreviewersCountUpdate(uint32_t numPrev);

    // karere::IApp::IChatHandler::chatd::Listener implementation
    virtual void init(chatd::Chat& chat, chatd::DbInterface*&);
    virtual void onDestroy();
    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status);
    virtual void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isLocal);
    virtual void onHistoryDone(chatd::HistSource source);
    virtual void onUnsentMsgLoaded(chatd::Message& msg);
    virtual void onUnsentEditLoaded(chatd::Message& msg, bool oriMsgIsSending);
    virtual void onMessageConfirmed(karere::Id msgxid, const chatd::Message& msg, chatd::Idx idx, bool tsUpdated);
    virtual void onMessageRejected(const chatd::Message& msg, uint8_t reason);
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg);
    virtual void onMessageEdited(const chatd::Message& msg, chatd::Idx idx);
    virtual void onEditRejected(const chatd::Message& msg, chatd::ManualSendReason reason);
    virtual void onOnlineStateChange(chatd::ChatState state);
    virtual void onUserJoin(karere::Id userid, chatd::Priv privilege);
    virtual void onUserLeave(karere::Id userid);
    virtual void onExcludedFromChat();
    virtual void onRejoinedChat();
    virtual void onUnreadChanged();
    void onRetentionTimeUpdated(unsigned int period) override;
    void onPreviewersUpdate();
    virtual void onManualSendRequired(chatd::Message* msg, uint64_t id, chatd::ManualSendReason reason);
    //virtual void onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx);
    //virtual void onMsgOrderVerificationFail(const chatd::Message& msg, chatd::Idx idx, const std::string& errmsg);
    virtual void onUserTyping(karere::Id user);
    virtual void onUserStopTyping(karere::Id user);
    virtual void onLastTextMessageUpdated(const chatd::LastTextMsg& msg);
    virtual void onLastMessageTsUpdated(uint32_t ts);
    virtual void onHistoryReloaded();
    virtual void onChatModeChanged(bool mode);
    virtual void onReactionUpdate(karere::Id msgid, const char *reaction, int count);
    void onHistoryTruncatedByRetentionTime(const chatd::Message &msg, const chatd::Idx &idx, const chatd::Message::Status &status) override;

    bool isRevoked(MegaChatHandle h);
    // update access to attachments
    void handleHistoryMessage(MegaChatMessage *message);
    // update access to attachments, returns messages requiring updates (you take ownership)
    std::set<MegaChatHandle> *handleNewMessage(MegaChatMessage *msg);

protected:

private:
    MegaChatApiImpl *chatApiImpl;
    MegaChatApi *chatApi;       // for notifications in callbacks
    mega::MegaApi *megaApi;
    MegaChatHandle chatid;

    chatd::Chat *mChat;
    karere::ChatRoom *mRoom;

    std::set<MegaChatRoomListener *> roomListeners;

    // nodes with granted/revoked access from loaded messsages
    std::map<MegaChatHandle, bool> attachmentsAccess;  // handle, access
    std::map<MegaChatHandle, std::set<MegaChatHandle>> attachmentsIds;    // nodehandle, msgids
};

class MegaChatNodeHistoryHandler : public chatd::FilteredHistoryHandler
{
public:
    MegaChatNodeHistoryHandler(MegaChatApi *api);
     virtual ~MegaChatNodeHistoryHandler(){}

    void fireOnAttachmentReceived(MegaChatMessage *message);
    void fireOnAttachmentLoaded(MegaChatMessage *message);
    void fireOnAttachmentDeleted(karere::Id id);
    void fireOnTruncate(karere::Id id);

    virtual void onReceived(chatd::Message* msg, chatd::Idx idx);
    virtual void onLoaded(chatd::Message* msg, chatd::Idx idx);
    virtual void onDeleted(karere::Id id);
    virtual void onTruncated(karere::Id id);

    void addMegaNodeHistoryListener(MegaChatNodeHistoryListener *listener);
    void removeMegaNodeHistoryListener(MegaChatNodeHistoryListener *listener);

protected:
    std::set<MegaChatNodeHistoryListener *>nodeHistoryListeners;
    MegaChatApi *chatApi;
};

class LoggerHandler : public karere::Logger::ILoggerBackend
{
public:
    LoggerHandler();
    virtual ~LoggerHandler();

    void setMegaChatLogger(MegaChatLogger *logger);
    void setLogLevel(int logLevel);
    void setLogWithColors(bool useColors);
    void setLogToConsole(bool enable);
    virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags);

private:
    std::recursive_mutex mutex;
    MegaChatLogger *megaLogger;
};

#ifndef KARERE_DISABLE_WEBRTC
class MegaChatSessionHandler;

class MegaChatCallHandler : public rtcModule::CallHandler
{
public:
    MegaChatCallHandler(MegaChatApiImpl *megaChatApi);
    ~MegaChatCallHandler();
    void onCallStateChange(rtcModule::ICall& call) override;
    void onCallRinging(rtcModule::ICall &call) override;
    void onNewSession(rtcModule::ISession& session, const rtcModule::ICall& call) override;
    void onModeratorChange(const rtcModule::ICall& call) override;
    void onAudioApproved(const rtcModule::ICall& call) override;
    void onLocalFlagsChanged(const rtcModule::ICall& call) override;
    void onLocalAudioDetected(const rtcModule::ICall& call) override;
    void onOnHold(const rtcModule::ICall& call) override;

protected:
    MegaChatApiImpl* mMegaChatApi;
};

class MegaChatSessionHandler : public rtcModule::SessionHandler
{
public:
    MegaChatSessionHandler(MegaChatApiImpl *mMegaChatApi, const rtcModule::ICall& call);
    virtual ~MegaChatSessionHandler();
    void onSpeakRequest(rtcModule::ISession& session, bool requested) override;
    void onVThumbReceived(rtcModule::ISession& session) override;
    void onHiResReceived(rtcModule::ISession& session) override;
    void onDestroySession(rtcModule::ISession& session) override;
    void onModeratorChange(rtcModule::ISession& session) override;
    void onAudioRequested(rtcModule::ISession& session) override;
    void onRemoteFlagsChanged(rtcModule::ISession& session) override;
    void onOnHold(rtcModule::ISession& session) override;
    void onRemoteAudioDetected(rtcModule::ISession& session) override;

private:
    MegaChatApiImpl *mMegaChatApi;
    MegaChatHandle mCallid;
    MegaChatHandle mChatid;
};
#endif

class MegaChatErrorPrivate :
        public MegaChatError,
        private ::promise::Error
{
public:

    MegaChatErrorPrivate(const std::string& msg, int code=ERROR_OK, int type=promise::kErrorTypeGeneric);
    MegaChatErrorPrivate(int code=ERROR_OK, int type=promise::kErrorTypeGeneric);
    virtual ~MegaChatErrorPrivate() {}

private:
    MegaChatErrorPrivate(const MegaChatErrorPrivate *);
    static const char* getGenericErrorString(int errorCode);

    // MegaChatError interface
public:
    MegaChatError *copy();
    int getErrorCode() const;
    int getErrorType() const;
    const char *getErrorString() const;
    const char *toString() const;
};

class MegaChatPeerListPrivate : public MegaChatPeerList
{
public:
    MegaChatPeerListPrivate();
    MegaChatPeerListPrivate(mega::userpriv_vector *);

    virtual ~MegaChatPeerListPrivate();
    virtual MegaChatPeerList *copy() const;

    virtual void addPeer(MegaChatHandle h, int priv);
    virtual MegaChatHandle getPeerHandle(int i) const;
    virtual int getPeerPrivilege(int i) const;
    virtual int size() const;

    // returns the list of user-privilege (this object keeps the ownership)
    const mega::userpriv_vector * getList() const;

private:
    mega::userpriv_vector list;
};

class MegaChatListItemListPrivate :  public MegaChatListItemList
{
public:
    MegaChatListItemListPrivate();
    virtual ~MegaChatListItemListPrivate();
    virtual MegaChatListItemListPrivate *copy() const;

    virtual const MegaChatListItem *get(unsigned int i) const;
    virtual unsigned int size() const;

    void addChatListItem(MegaChatListItem*);

private:
    MegaChatListItemListPrivate(const MegaChatListItemListPrivate *list);
    std::vector<MegaChatListItem*> list;
};

class MegaChatRoomPrivate : public MegaChatRoom
{
public:
    MegaChatRoomPrivate(const MegaChatRoom *);
    MegaChatRoomPrivate(const karere::ChatRoom&);

    virtual ~MegaChatRoomPrivate() {}
    virtual MegaChatRoom *copy() const;

    virtual MegaChatHandle getChatId() const;
    virtual int getOwnPrivilege() const;
    virtual unsigned int getNumPreviewers() const;
    virtual int getPeerPrivilegeByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerFirstnameByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerLastnameByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerFullnameByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerEmailByHandle(MegaChatHandle userhandle) const;
    virtual int getPeerPrivilege(unsigned int i) const;
    virtual unsigned int getPeerCount() const;
    virtual MegaChatHandle getPeerHandle(unsigned int i) const;
    virtual const char *getPeerFirstname(unsigned int i) const;
    virtual const char *getPeerLastname(unsigned int i) const;
    virtual const char *getPeerFullname(unsigned int i) const;
    virtual const char *getPeerEmail(unsigned int i) const;
    virtual bool isGroup() const;
    virtual bool isPublic() const;
    virtual bool isPreview() const;
    virtual const char *getAuthorizationToken() const;
    virtual const char *getTitle() const;
    virtual bool hasCustomTitle() const;
    virtual bool isActive() const;
    virtual bool isArchived() const;
    virtual int64_t getCreationTs() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual int getUnreadCount() const;
    virtual MegaChatHandle getUserHandle() const;
    virtual MegaChatHandle getUserTyping() const;
    unsigned getRetentionTime() const override;

    void setRetentionTime(unsigned int period);
    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void changeUnreadCount();
    void setNumPreviewers(unsigned int numPrev);
    void setMembersUpdated(MegaChatHandle uh);
    void setUserTyping(MegaChatHandle uh);
    void setUserStopTyping(MegaChatHandle uh);
    void setClosed();
    void setChatMode(bool mode);
    void setArchived(bool archived);

private:
    int changed;

    MegaChatHandle chatid;
    mega::privilege_t priv;
    mega::userpriv_vector peers;
    std::vector<std::string> peerFirstnames;
    std::vector<std::string> peerLastnames;
    std::vector<std::string> peerEmails;
    bool group;
    bool mPublicChat;
    karere::Id mAuthToken;
    bool active;
    bool archived;
    bool mHasCustomTitle;
    int64_t mCreationTs;

    std::string title;
    int unreadCount;
    unsigned int mNumPreviewers;
    MegaChatHandle uh;
    uint32_t mRetentionTime;

public:
    // you take the ownership of return value
    static char *firstnameFromBuffer(const std::string &buffer);

    // you take the ownership of return value
    static char *lastnameFromBuffer(const std::string &buffer);
};

class MegaChatRoomListPrivate :  public MegaChatRoomList
{
public:
    MegaChatRoomListPrivate();
    virtual ~MegaChatRoomListPrivate() {}
    virtual MegaChatRoomList *copy() const;

    virtual const MegaChatRoom *get(unsigned int i) const;
    virtual unsigned int size() const;

    void addChatRoom(MegaChatRoom*);

private:
    MegaChatRoomListPrivate(const MegaChatRoomListPrivate *list);
    std::vector<MegaChatRoom*> list;
};

class MegaChatAttachedUser;
class MegaChatRichPreviewPrivate;
class MegaChatContainsMetaPrivate;

class MegaChatMessagePrivate : public MegaChatMessage
{
public:
    MegaChatMessagePrivate(const MegaChatMessage *msg);
    MegaChatMessagePrivate(const chatd::Message &msg, chatd::Message::Status status, chatd::Idx index);

    virtual ~MegaChatMessagePrivate();
    virtual MegaChatMessage *copy() const;

    // MegaChatMessage interface
    virtual int getStatus() const;
    virtual MegaChatHandle getMsgId() const;
    virtual MegaChatHandle getTempId() const;
    virtual int getMsgIndex() const;
    virtual MegaChatHandle getUserHandle() const;
    virtual int getType() const;
    virtual bool hasConfirmedReactions() const;
    virtual int64_t getTimestamp() const;
    virtual const char *getContent() const;
    virtual bool isEdited() const;
    virtual bool isDeleted() const;
    virtual bool isEditable() const;
    virtual bool isDeletable() const;
    virtual bool isManagementMessage() const;
    virtual MegaChatHandle getHandleOfAction() const;
    virtual int getPrivilege() const;
    virtual int getCode() const;
    virtual MegaChatHandle getRowId() const;
    virtual unsigned int getUsersCount() const;
    virtual MegaChatHandle getUserHandle(unsigned int index) const;
    virtual const char *getUserName(unsigned int index) const;
    virtual const char *getUserEmail(unsigned int index) const;
    virtual mega::MegaNodeList *getMegaNodeList() const;
    virtual const MegaChatContainsMeta *getContainsMeta() const;
    virtual mega::MegaHandleList *getMegaHandleList() const;
    virtual int getDuration() const;
    unsigned getRetentionTime() const override;
    virtual int getTermCode() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    void setStatus(int status);
    void setTempId(MegaChatHandle tempId);
    void setRowId(int id);
    void setContentChanged();
    void setCode(int code);
    void setAccess();
    void setTsUpdated();

    static int convertEndCallTermCodeToUI(const chatd::Message::CallEndedInfo &callEndInfo);

private:
    bool isGiphy() const;

    int changed;

    int type;
    int status;
    MegaChatHandle msgId;   // definitive unique ID given by server
    MegaChatHandle tempId;  // used until it's given a definitive ID by server
    MegaChatHandle rowId;   // used to identify messages in the manual-sending queue
    MegaChatHandle uh;
    MegaChatHandle hAction;// certain messages need additional handle: such us priv changes, revoke attachment
    int index;              // position within the history buffer
    int64_t ts;
    const char *msg;
    bool edited;
    bool deleted;
    int priv;               // certain messages need additional info, like priv changes
    int code;               // generic field for additional information (ie. the reason of manual sending)
    bool mHasReactions;
    std::vector<MegaChatAttachedUser> *megaChatUsers = NULL;
    mega::MegaNodeList *megaNodeList = NULL;
    mega::MegaHandleList *megaHandleList = NULL;
    const MegaChatContainsMeta *mContainsMeta = NULL;
};

//Thread safe request queue
class ChatRequestQueue
{
    protected:
        std::deque<MegaChatRequestPrivate *> requests;
        std::mutex mutex;

    public:
        void push(MegaChatRequestPrivate *request);
        void push_front(MegaChatRequestPrivate *request);
        MegaChatRequestPrivate * pop();
        void removeListener(MegaChatRequestListener *listener);
};

//Thread safe transfer queue
class EventQueue
{
protected:
    std::deque<megaMessage*> events;
    std::mutex mutex;

public:
    void push(megaMessage* event);
    void push_front(megaMessage* event);
    megaMessage *pop();
    bool isEmpty();
    size_t size();
};

class MegaChatApiImpl :
        public karere::IApp,
        public karere::IApp::IChatListHandler,
        public rtcModule::IGlobalCallHandler
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, mega::MegaApi *megaApi);
    virtual ~MegaChatApiImpl();

    using SdkMutexGuard = std::unique_lock<std::recursive_mutex>;   // (equivalent to typedef)
    std::recursive_mutex sdkMutex;
    std::recursive_mutex videoMutex;
    mega::Waiter *waiter;
private:
    MegaChatApi *chatApi;
    mega::MegaApi *megaApi;
    WebsocketsIO *websocketsIO;
    karere::Client *mClient;
    bool terminating;

    mega::MegaThread thread;
    int threadExit;
    static void *threadEntryPoint(void *param);
    void loop();

    void init(MegaChatApi *chatApi, mega::MegaApi *megaApi);

    static LoggerHandler *loggerHandler;

    ChatRequestQueue requestQueue;
    EventQueue eventQueue;

    std::set<MegaChatListener *> listeners;
    std::set<MegaChatNotificationListener *> notificationListeners;
    std::set<MegaChatRequestListener *> requestListeners;

    std::set<MegaChatPeerListItemHandler *> chatPeerListItemHandler;
    std::set<MegaChatGroupListItemHandler *> chatGroupListItemHandler;
    std::map<MegaChatHandle, MegaChatRoomHandler*> chatRoomHandler;
    std::map<MegaChatHandle, MegaChatNodeHistoryHandler*> nodeHistoryHandlers;

    int reqtag;
    std::map<int, MegaChatRequestPrivate *> requestMap;

#ifndef KARERE_DISABLE_WEBRTC
    std::set<MegaChatCallListener *> callListeners;
    std::map<MegaChatHandle, MegaChatPeerVideoListener_map> mVideoListenersHiRes;
    std::map<MegaChatHandle, MegaChatPeerVideoListener_map> mVideoListenersLowRes;
    std::map<MegaChatHandle, MegaChatVideoListener_set> mLocalVideoListeners;

    mega::MegaStringList *getChatInDevices(const std::set<std::string> &devices);
    void cleanCalls();
#endif

    void cleanChatHandlers();

    static int convertInitState(int state);

public:
    static void megaApiPostMessage(megaMessage *msg, void* ctx);
    void postMessage(megaMessage *msg);

    void sendPendingRequests();
    void sendPendingEvents();

    static void setLogLevel(int logLevel);
    static void setLoggerClass(MegaChatLogger *megaLogger);
    static void setLogWithColors(bool useColors);
    static void setLogToConsole(bool enable);

    int init(const char *sid, bool waitForFetchnodesToConnect = true);
    int initAnonymous();
    void createKarereClient();
    void resetClientid();
    int getInitState();

    void importMessages(const char *externalDbPath, MegaChatRequestListener *listener);

    MegaChatRoomHandler* getChatRoomHandler(MegaChatHandle chatid);
    void removeChatRoomHandler(MegaChatHandle chatid);

    karere::ChatRoom *findChatRoom(MegaChatHandle chatid);
    karere::ChatRoom *findChatRoomByUser(MegaChatHandle userhandle);
    chatd::Message *findMessage(MegaChatHandle chatid, MegaChatHandle msgid);
    chatd::Message *findMessageNotConfirmed(MegaChatHandle chatid, MegaChatHandle msgxid);

#ifndef KARERE_DISABLE_WEBRTC
    rtcModule::ICall* findCall(MegaChatHandle chatid);
#endif

    static void setCatchException(bool enable);
    static bool hasUrl(const char* text);
    bool openNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);
    bool closeNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);
    void addNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);
    void removeNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);
    int loadAttachments(MegaChatHandle chatid, int count);

    // ============= Listeners ================

    // Registration
    void addChatRequestListener(MegaChatRequestListener *listener);
    void addChatListener(MegaChatListener *listener);
    void addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);
    void addChatNotificationListener(MegaChatNotificationListener *listener);
    void removeChatRequestListener(MegaChatRequestListener *listener);
    void removeChatListener(MegaChatListener *listener);
    void removeChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);
    void removeChatNotificationListener(MegaChatNotificationListener *listener);
    int getMessageReactionCount(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction);
    mega::MegaStringList* getMessageReactions(MegaChatHandle chatid, MegaChatHandle msgid);
    mega::MegaHandleList* getReactionUsers(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction);
#ifndef KARERE_DISABLE_WEBRTC
    void addChatCallListener(MegaChatCallListener *listener);
    void removeChatCallListener(MegaChatCallListener *listener);
    void addChatVideoListener(MegaChatHandle chatid, MegaChatHandle clientid, bool hiRes, MegaChatVideoListener *listener);
    void removeChatVideoListener(MegaChatHandle chatid, MegaChatHandle clientid, bool hiRes, MegaChatVideoListener *listener);
#endif

    // MegaChatRequestListener callbacks
    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaChatError *e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaChatError *e);

#ifndef KARERE_DISABLE_WEBRTC
    // MegaChatCallListener callbacks
    void fireOnChatCallUpdate(MegaChatCallPrivate *call);
    void fireOnChatSessionUpdate(MegaChatHandle chatid, MegaChatHandle callid, MegaChatSessionPrivate *session);

    // MegaChatVideoListener callbacks
    void fireOnChatVideoData(MegaChatHandle chatid, uint32_t clientid, int width, int height, char*buffer, bool hiRes);
#endif

    // MegaChatListener callbacks (specific ones)
    void fireOnChatListItemUpdate(MegaChatListItem *item);
    void fireOnChatInitStateUpdate(int newState);
    void fireOnChatOnlineStatusUpdate(MegaChatHandle userhandle, int status, bool inProgress);
    void fireOnChatPresenceConfigUpdate(MegaChatPresenceConfig *config);
    void fireOnChatPresenceLastGreenUpdated(MegaChatHandle userhandle, int lastGreen);
    void fireOnChatConnectionStateUpdate(MegaChatHandle chatid, int newState);

    // MegaChatNotificationListener callbacks
    void fireOnChatNotification(MegaChatHandle chatid, MegaChatMessage *msg);

    // ============= API requests ================

    // General chat methods
    void connect(MegaChatRequestListener *listener = NULL);
    void connectInBackground(MegaChatRequestListener *listener = NULL);
    void disconnect(MegaChatRequestListener *listener = NULL);
    int getConnectionState();
    int getChatConnectionState(MegaChatHandle chatid);
    bool areAllChatsLoggedIn();
    static int convertChatConnectionState(chatd::ChatState state);
    void retryPendingConnections(bool disconnect = false, bool refreshURL = false, MegaChatRequestListener *listener = NULL);
    void logout(MegaChatRequestListener *listener = NULL);
    void localLogout(MegaChatRequestListener *listener = NULL);

    void setOnlineStatus(int status, MegaChatRequestListener *listener = NULL);
    int getOnlineStatus();
    bool isOnlineStatusPending();

    void setPresenceAutoaway(bool enable, int64_t timeout, MegaChatRequestListener *listener = NULL);
    void setPresencePersist(bool enable, MegaChatRequestListener *listener = NULL);
    void signalPresenceActivity(MegaChatRequestListener *listener = NULL);
    void setLastGreenVisible(bool enable, MegaChatRequestListener *listener = NULL);
    void requestLastGreen(MegaChatHandle userid, MegaChatRequestListener *listener = NULL);
    MegaChatPresenceConfig *getPresenceConfig();
    bool isSignalActivityRequired();

    int getUserOnlineStatus(MegaChatHandle userhandle);
    void setBackgroundStatus(bool background, MegaChatRequestListener *listener = NULL);
    int getBackgroundStatus();

    void getUserFirstname(MegaChatHandle userhandle, const char *authorizationToken, MegaChatRequestListener *listener = NULL);
    const char* getUserFirstnameFromCache(MegaChatHandle userhandle);
    void getUserLastname(MegaChatHandle userhandle, const char *authorizationToken, MegaChatRequestListener *listener = NULL);
    const char* getUserLastnameFromCache(MegaChatHandle userhandle);
    const char* getUserFullnameFromCache(MegaChatHandle userhandle);
    void getUserEmail(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    const char* getUserEmailFromCache(MegaChatHandle userhandle);
    void loadUserAttributes(MegaChatHandle chatid, mega::MegaHandleList* userList, MegaChatRequestListener *listener = nullptr);
    unsigned int getMaxParticipantsWithAttributes();
    char *getContactEmail(MegaChatHandle userhandle);
    MegaChatHandle getUserHandleByEmail(const char *email);
    MegaChatHandle getMyUserHandle();
    MegaChatHandle getMyClientidHandle(MegaChatHandle chatid);
    char *getMyFirstname();
    char *getMyLastname();
    char *getMyFullname();
    char *getMyEmail();
    MegaChatRoomList* getChatRooms();
    MegaChatRoom* getChatRoom(MegaChatHandle chatid);
    MegaChatRoom *getChatRoomByUser(MegaChatHandle userhandle);
    MegaChatListItemList *getChatListItems();
    MegaChatListItemList *getChatListItemsByPeers(MegaChatPeerList *peers);
    MegaChatListItem *getChatListItem(MegaChatHandle chatid);
    int getUnreadChats();
    MegaChatListItemList *getActiveChatListItems();
    MegaChatListItemList *getInactiveChatListItems();
    MegaChatListItemList *getArchivedChatListItems();
    MegaChatListItemList *getUnreadChatListItems();
    MegaChatHandle getChatHandleByUser(MegaChatHandle userhandle);

    // Chatrooms management
    void createChat(bool group, MegaChatPeerList *peerList, MegaChatRequestListener *listener = NULL);
    void createChat(bool group, MegaChatPeerList *peerList, const char *title, MegaChatRequestListener *listener = NULL);
    void createPublicChat(MegaChatPeerList *peerList, const char *title = NULL, MegaChatRequestListener *listener = NULL);
    void chatLinkHandle(MegaChatHandle chatid, bool del, bool createifmissing, MegaChatRequestListener *listener = NULL);
    void inviteToChat(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);
    void autojoinPublicChat(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void autorejoinPublicChat(MegaChatHandle chatid, MegaChatHandle ph, MegaChatRequestListener *listener = NULL);
    void removeFromChat(MegaChatHandle chatid, MegaChatHandle uh = MEGACHAT_INVALID_HANDLE, MegaChatRequestListener *listener = NULL);
    void updateChatPermissions(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);
    void truncateChat(MegaChatHandle chatid, MegaChatHandle messageid, MegaChatRequestListener *listener = NULL);
    void setChatTitle(MegaChatHandle chatid, const char *title, MegaChatRequestListener *listener = NULL);
    void openChatPreview(const char *link, MegaChatRequestListener *listener = NULL);
    void checkChatLink(const char *link, MegaChatRequestListener *listener = NULL);
    void setPublicChatToPrivate(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void removeChatLink(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void archiveChat(MegaChatHandle chatid, bool archive, MegaChatRequestListener *listener = NULL);
    void setChatRetentionTime(MegaChatHandle chatid, unsigned period, MegaChatRequestListener *listener = NULL);

    bool openChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener = NULL);
    void closeChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener = NULL);
    void closeChatPreview(MegaChatHandle chatid);

    int loadMessages(MegaChatHandle chatid, int count);
    bool isFullHistoryLoaded(MegaChatHandle chatid);
    void manageReaction(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction, bool add, MegaChatRequestListener *listener = NULL);
    MegaChatMessage *getMessage(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *getMessageFromNodeHistory(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid);
    MegaChatMessage *sendMessage(MegaChatHandle chatid, const char* msg, size_t msgLen, int type = MegaChatMessage::TYPE_NORMAL);
    MegaChatMessage *attachContacts(MegaChatHandle chatid, mega::MegaHandleList* contacts);
    MegaChatMessage *forwardContact(MegaChatHandle sourceChatid, MegaChatHandle msgid, MegaChatHandle targetChatId);
    void attachNodes(MegaChatHandle chatid, mega::MegaNodeList *nodes, MegaChatRequestListener *listener = NULL);
    void attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);
    MegaChatMessage *sendGeolocation(MegaChatHandle chatid, float longitude, float latitude, const char *img = NULL);
    MegaChatMessage *editGeolocation(MegaChatHandle chatid, MegaChatHandle msgid, float longitude, float latitude, const char *img = NULL);
    MegaChatMessage *sendGiphy(MegaChatHandle chatid, const char* srcMp4, const char* srcWebp, long long sizeMp4, long long sizeWebp, int width, int height, const char* title);
    void attachVoiceMessage(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);
    void revokeAttachment(MegaChatHandle chatid, MegaChatHandle handle, MegaChatRequestListener *listener = NULL);
    bool isRevoked(MegaChatHandle chatid, MegaChatHandle nodeHandle);
    MegaChatMessage *editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char* msg, size_t msgLen);
    MegaChatMessage *removeRichLink(MegaChatHandle chatid, MegaChatHandle msgid);
    bool setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *getLastMessageSeen(MegaChatHandle chatid);
    MegaChatHandle getLastMessageSeenId(MegaChatHandle chatid);
    void removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowid);
    void sendTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void sendStopTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    bool isMessageReceptionConfirmationActive() const;
    void saveCurrentState();
    void pushReceived(bool beep, MegaChatHandle chatid, int type, MegaChatRequestListener *listener = NULL);

#ifndef KARERE_DISABLE_WEBRTC

    // Audio/Video devices
    mega::MegaStringList *getChatVideoInDevices();
    void setChatVideoInDevice(const char *device, MegaChatRequestListener *listener = NULL);
    char *getVideoDeviceSelected();

    // Calls
    void startChatCall(MegaChatHandle chatid, bool enableVideo = true,  bool enableAudio = true, MegaChatRequestListener *listener = NULL);
    void answerChatCall(MegaChatHandle chatid, bool enableVideo = true,  bool enableAudio = true, MegaChatRequestListener *listener = NULL);
    void hangChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void endChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void setAudioEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void setVideoEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void openVideoDevice(MegaChatRequestListener *listener = NULL);
    void releaseVideoDevice(MegaChatRequestListener *listener = NULL);
    void setCallOnHold(MegaChatHandle chatid, bool setOnHold, MegaChatRequestListener *listener = NULL);
    MegaChatCall *getChatCall(MegaChatHandle chatId);
    bool setIgnoredCall(MegaChatHandle chatId);
    MegaChatCall *getChatCallByCallId(MegaChatHandle callId);
    int getNumCalls();
    mega::MegaHandleList *getChatCalls(int callState = -1);
    mega::MegaHandleList *getChatCallsIds();
    bool hasCallInChatRoom(MegaChatHandle chatid);
    int getMaxCallParticipants();
    int getMaxVideoCallParticipants();
    bool isAudioLevelMonitorEnabled(MegaChatHandle chatid);  /// ***Deprecated
    void enableAudioLevelMonitor(bool enable, MegaChatHandle chatid, MegaChatRequestListener *listener = NULL); /// ***Deprecated
    void requestSpeak(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void removeRequestSpeak(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void approveSpeakRequest(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);
    void rejectSpeakRequest(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);
    void requestHiResVideo(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);
    void stoptHiResVideo(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);
    void requestLowResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);
    void stoptLowResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);

    void onNewCall(rtcModule::ICall& call) override;
    void onAddPeer(rtcModule::ICall& call, karere::Id peer) override;
    void onRemovePeer(rtcModule::ICall& call, karere::Id peer) override;
    void onEndCall(rtcModule::ICall& call) override;
#endif

//    MegaChatCallPrivate *getChatCallByPeer(const char* jid);


    // ============= karere API implementation ================

    // karere::IApp implementation
    //virtual ILoginDialog* createLoginDialog();
    virtual IApp::IChatHandler *createChatHandler(karere::ChatRoom &chat);
    virtual IApp::IChatListHandler *chatListHandler();
    virtual void onPresenceChanged(karere::Id userid, karere::Presence pres, bool inProgress);
    virtual void onPresenceConfigChanged(const presenced::Config& state, bool pending);
    virtual void onPresenceLastGreenUpdated(karere::Id userid, uint16_t lastGreen);
    virtual void onInitStateChange(int newState);
    virtual void onChatNotification(karere::Id chatid, const chatd::Message &msg, chatd::Message::Status status, chatd::Idx idx);

    // rtcModule::IChatListHandler implementation
    virtual IApp::IGroupChatListItem *addGroupChatItem(karere::GroupChatRoom &chat);
    virtual void removeGroupChatItem(IApp::IGroupChatListItem& item);
    virtual IApp::IPeerChatListItem *addPeerChatItem(karere::PeerChatRoom& chat);
    virtual void removePeerChatItem(IApp::IPeerChatListItem& item);
};

/**
 * @brief This class represents users attached to a message
 *
 * Messages of type MegaChatMessage::TYPE_CONTACT_ATTACHMENT include a list of
 * users with handle, email and name. The MegaChatMessage provides methods to
 * get each of them separatedly.
 *
 * This class is used internally by MegaChatMessagePrivate, not exposed to apps.
 *
 * @see MegaChatMessage::getUserHandle, MegaChatMessage::getUserName and
 * MegaChatMessage::getUserEmail.
 */
class MegaChatAttachedUser
{
public:
    MegaChatAttachedUser(MegaChatHandle contactId, const std::string& email, const std::string& name);
    virtual ~MegaChatAttachedUser();

    virtual MegaChatHandle getHandle() const;
    virtual const char *getEmail() const;
    virtual const char *getName() const;

protected:
    megachat::MegaChatHandle mHandle;
    std::string mEmail;
    std::string mName;
};

class MegaChatRichPreviewPrivate : public MegaChatRichPreview
{
public:
    MegaChatRichPreviewPrivate(const MegaChatRichPreview *richPreview);
    MegaChatRichPreviewPrivate(const std::string &text, const std::string &title, const std::string &description,
                        const std::string &image, const std::string &imageFormat, const std::string &icon,
                        const std::string &iconFormat, const std::string &url);

    virtual MegaChatRichPreview *copy() const;
    virtual ~MegaChatRichPreviewPrivate();

    virtual const char *getText() const;
    virtual const char *getTitle() const;
    virtual const char *getDescription() const;
    virtual const char *getImage() const;
    virtual const char *getImageFormat() const;
    virtual const char *getIcon() const;
    virtual const char *getIconFormat() const;
    virtual const char *getUrl() const;
    virtual const char *getDomainName() const;

protected:
    std::string mText;
    std::string mTitle;
    std::string mDescription;
    std::string mImage;
    std::string mImageFormat;
    std::string mIcon;
    std::string mIconFormat;
    std::string mUrl;
    std::string mDomainName;
};

class MegaChatGeolocationPrivate : public MegaChatGeolocation
{
public:
    MegaChatGeolocationPrivate(float longitude, float latitude, const std::string &image);
    MegaChatGeolocationPrivate(const MegaChatGeolocationPrivate *geolocation);
    virtual ~MegaChatGeolocationPrivate() {}
    virtual MegaChatGeolocation *copy() const;

    virtual float getLongitude() const;
    virtual float getLatitude() const;
    virtual const char *getImage() const;

protected:
    float mLongitude;
    float mLatitude;
    std::string mImage;
};

class MegaChatGiphyPrivate : public MegaChatGiphy
{
public:
    MegaChatGiphyPrivate(const std::string& srcMp4, const std::string& srcWebp, long long sizeMp4, long long sizeWebp, int width, int height, const std::string& title);
    MegaChatGiphyPrivate(const MegaChatGiphyPrivate* giphy);
    virtual ~MegaChatGiphyPrivate() {}
    virtual MegaChatGiphy* copy() const override;

    virtual const char* getMp4Src() const override;
    virtual const char* getWebpSrc() const override;
    virtual const char* getTitle() const override;
    virtual long getMp4Size() const override;
    virtual long getWebpSize() const override;
    virtual int getWidth() const override;
    virtual int getHeight() const override;

protected:
    std::string mMp4Src;
    std::string mWebpSrc;
    std::string mTitle;
    long mMp4Size   = 0;
    long mWebpSize  = 0;
    int mWidth      = 0;
    int mHeight     = 0;
};

class MegaChatContainsMetaPrivate : public MegaChatContainsMeta
{
public:
    MegaChatContainsMetaPrivate(const MegaChatContainsMeta *containsMeta = NULL);
    virtual ~MegaChatContainsMetaPrivate();

    virtual MegaChatContainsMeta *copy() const;

    virtual int getType() const;
    virtual const char *getTextMessage() const;
    virtual const MegaChatRichPreview *getRichPreview() const;
    virtual const MegaChatGeolocation *getGeolocation() const;
    virtual const MegaChatGiphy *getGiphy() const override;

    // This function take the property from memory that it receives as parameter
    void setRichPreview(MegaChatRichPreview *richPreview);
    void setGeolocation(MegaChatGeolocation *geolocation);
    void setTextMessage(const std::string &text);
    void setGiphy(std::unique_ptr<MegaChatGiphy> giphy);

protected:
    int mType = MegaChatContainsMeta::CONTAINS_META_INVALID;
    std::string mText;
    MegaChatRichPreview *mRichPreview = NULL;
    MegaChatGeolocation *mGeolocation = NULL;
    std::unique_ptr<MegaChatGiphy> mGiphy;
};

class JSonUtils
{
public:
    static std::string generateAttachNodeJSon(mega::MegaNodeList* nodes, uint8_t type);
    static std::string generateAttachContactJSon(mega::MegaHandleList *contacts, karere::ContactList *contactList);
    static std::string generateGeolocationJSon(float longitude, float latitude, const char* img);
    static std::string generateGiphyJSon(const char* srcMp4, const char* srcWebp, long long sizeMp4, long long sizeWebp, int width, int height, const char* title);

    // you take the ownership of returned value. NULL if error
    static mega::MegaNodeList *parseAttachNodeJSon(const char* json);

    // you take the ownership of returned value. NULL if error
    static std::vector<MegaChatAttachedUser> *parseAttachContactJSon(const char* json);

    // you take the ownership of the returned value. NULL if error
    static const MegaChatContainsMeta *parseContainsMeta(const char* json, uint8_t type, bool onlyTextMessage = false);

    /**
     * If the message is of type MegaChatMessage::TYPE_ATTACHMENT, this function
     * recives the filenames of the attached nodes. The filenames of nodes are separated
     * by ASCII character '0x01'
     * If the message is of type MegaChatMessage::TYPE_CONTACT, this function
     * recives the usernames. The usernames are separated
     * by ASCII character '0x01'
     */
    static std::string getLastMessageContent(const std::string &content, uint8_t type);

private:
    static std::string getImageFormat(const char* imagen);
    static void getRichLinckImageFromJson(const std::string& field, const rapidjson::Value& richPreviewValue, std::string& image, std::string& format);
    static MegaChatRichPreview *parseRichPreview(rapidjson::Document &document, std::string &textMessage);
    static MegaChatGeolocation *parseGeolocation(rapidjson::Document &document);
    static std::unique_ptr<MegaChatGiphy> parseGiphy(rapidjson::Document& document);
};

}

#endif // MEGACHATAPI_IMPL_H
