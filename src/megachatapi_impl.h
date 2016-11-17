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

#include <IRtcModule.h>
#include <IVideoRenderer.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include <chatd.h>
#include <sdkApi.h>
#include <mstrophepp.h>
#include <karereCommon.h>
#include <logger.h>


namespace megachat
{

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

    void setTag(int tag);
    void setListener(MegaChatRequestListener *listener);
    void setNumber(long long number);
    void setNumRetry(int retry);
    void setFlag(bool flag);
    void setMegaChatPeerList(MegaChatPeerList *peerList);
    void setChatHandle(MegaChatHandle chatid);
    void setUserHandle(MegaChatHandle userhandle);
    void setPrivilege(int priv);
    void setText(const char *text);

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
};

class MegaChatVideoReceiver;

class MegaChatCallPrivate :
        public MegaChatCall,
        public karere::IApp::ICallHandler,
        public rtcModule::ICallAnswer
{
public:
    MegaChatCallPrivate(const std::shared_ptr<rtcModule::ICallAnswer> &ans);
    MegaChatCallPrivate(const char *peer);
    MegaChatCallPrivate(const MegaChatCallPrivate &call);

    virtual ~MegaChatCallPrivate();

    virtual MegaChatCall *copy();

    virtual int getStatus() const;
    virtual int getTag() const;
    virtual MegaChatHandle getContactHandle() const;

//    shared_ptr<rtcModule::ICallAnswer> getAnswerObject();

    const char* getPeer() const;
    void setStatus(int status);
    void setTag(int tag);
    void setVideoReceiver(MegaChatVideoReceiver *videoReceiver);
    //void setAnswerObject(rtcModule::ICallAnswer *answerObject);

    // IApp::ICallHandler implementation (empty)

    // rtcModule::IEventHandler implementation (inherit from ICallHandler)
//    virtual void onLocalMediaFail(const std::string& errMsg, bool* cont);
//    virtual void onOutgoingCallCreated(const std::shared_ptr<ICall>& call);
//    virtual void onCallAnswered(const std::string& peerFullJid);
//    virtual void onLocalStreamObtained(IVideoRenderer*& localVidRenderer);
//    virtual void removeRemotePlayer();
//    virtual void onMediaRecv(stats::Options& statOptions);
//    virtual void onCallEnded(TermCode termcode, const std::string& text,
//                             const std::shared_ptr<stats::IRtcStats>& stats);
//    virtual void onRemoteSdpRecv(IVideoRenderer*& rendererRet);
//    virtual void onPeerMute(AvFlags what);
//    virtual void onPeerUnmute(AvFlags what);

    // rtcModule::ICallAnswer implementation
    virtual std::shared_ptr<rtcModule::ICall> call() const;
    virtual bool reqStillValid() const;
    virtual std::set<std::string>* files() const;
    virtual karere::AvFlags peerMedia() const;
    virtual bool answer(bool accept, karere::AvFlags ownMedia);


protected:
    int tag;
    int status;
    const char *peer;
    MegaChatVideoReceiver *videoReceiver;
    std::shared_ptr<rtcModule::ICallAnswer> mAns;
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
    MegaChatVideoReceiver(MegaChatApiImpl *chatApi, MegaChatCallPrivate *call, bool local);
    ~MegaChatVideoReceiver();

    void setWidth(int width);
    void setHeight(int height);

    // rtcModule::IVideoRenderer implementation
    virtual unsigned char* getImageBuffer(unsigned short width, unsigned short height, void** userData);
    virtual void frameComplete(void* userData);
    virtual void onVideoAttach();
    virtual void onVideoDetach();
    virtual void clearViewport();
    virtual void released();

protected:
    MegaChatApiImpl *chatApi;
    MegaChatCallPrivate *call;
    bool local;
};

class MegaChatListItemPrivate : public MegaChatListItem
{
public:
    MegaChatListItemPrivate(const karere::ChatRoom &chat);
    MegaChatListItemPrivate(const MegaChatListItem *item);
    virtual ~MegaChatListItemPrivate();
    virtual MegaChatListItem *copy() const;

private:
    int changed;

    MegaChatHandle chatid;
    mega::visibility_t visibility;
    std::string title;
    int unreadCount;
    int status;
    MegaChatMessage *lastMsg;

public:
    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual MegaChatHandle getChatId() const;
    virtual const char *getTitle() const;
    virtual int getVisibility() const;
    virtual int getUnreadCount() const;
    virtual int getOnlineStatus() const;
    virtual MegaChatMessage *getLastMessage() const;

    void setVisibility(mega::visibility_t visibility);
    void setTitle(const std::string &title);
    void setUnreadCount(int count);
    void setOnlineStatus(int status);
    void setMembersUpdated();
    void setClosed();
    void setLastMessage(MegaChatMessage *msg);
};

class MegaChatListItemHandler :public virtual karere::IApp::IChatListItem
{
public:
    MegaChatListItemHandler(MegaChatApiImpl&, const karere::ChatRoom&);

    // karere::IApp::IListItem::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged(int count);
    virtual void onPresenceChanged(karere::Presence state);

    // karere::IApp::IListItem::IChatListItem implementation
    virtual void onExcludedFromChat();
    virtual void onRejoinedChat();
    virtual void onLastMessageUpdated(const chatd::Message& msg, chatd::Message::Status status, chatd::Idx idx);

    virtual const karere::ChatRoom& getChatRoom() const;

protected:
    MegaChatApiImpl &chatApi;
    const karere::ChatRoom &mRoom;
};

class MegaChatGroupListItemHandler :
        public MegaChatListItemHandler,
        public virtual karere::IApp::IGroupChatListItem
{
public:
    MegaChatGroupListItemHandler(MegaChatApiImpl&, const karere::ChatRoom&);

    // karere::IApp::IListItem::IGroupChatListItem implementation
    virtual void onUserJoin(uint64_t userid, chatd::Priv priv);
    virtual void onUserLeave(uint64_t userid);
};

class MegaChatPeerListItemHandler :
        public MegaChatListItemHandler,
        public virtual karere::IApp::IPeerChatListItem
{
public:
    MegaChatPeerListItemHandler(MegaChatApiImpl &, const karere::ChatRoom&);
};

class MegaChatRoomHandler :public karere::IApp::IChatHandler
{
public:    
    MegaChatRoomHandler(MegaChatApiImpl*, MegaChatHandle chatid);

    // karere::IApp::IChatHandler implementation
    virtual karere::IApp::ICallHandler* callHandler();
    virtual void onUserTyping(karere::Id user);
    virtual void onMemberNameChanged(uint64_t userid, const std::string &newName);
    //virtual void* userp();


    // karere::IApp::IChatHandler::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged(int count);
    virtual void onPresenceChanged(karere::Presence state);
//    virtual void onLastMessageUpdate();   // TBD in IGui.h

    // karere::IApp::IChatHandler::chatd::Listener implementation
    virtual void init(chatd::Chat& chat, chatd::DbInterface*&);
    virtual void onDestroy();
    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status);
    virtual void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isLocal);
    virtual void onHistoryDone(chatd::HistSource source);
    virtual void onUnsentMsgLoaded(chatd::Message& msg);
    virtual void onUnsentEditLoaded(chatd::Message& msg, bool oriMsgIsSending);
    virtual void onMessageConfirmed(karere::Id msgxid, const chatd::Message& msg, chatd::Idx idx);
    virtual void onMessageRejected(const chatd::Message& msg);
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg);
    virtual void onMessageEdited(const chatd::Message& msg, chatd::Idx idx);
    virtual void onEditRejected(const chatd::Message& msg, bool oriIsConfirmed);
    virtual void onOnlineStateChange(chatd::ChatState state);
    virtual void onUserJoin(karere::Id userid, chatd::Priv privilege);
    virtual void onUserLeave(karere::Id userid);
    virtual void onExcludedFromChat();
    virtual void onRejoinedChat();
    virtual void onUnreadChanged();
    virtual void onManualSendRequired(chatd::Message* msg, uint64_t id, chatd::ManualSendReason reason);
    //virtual void onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx);
    //virtual void onMsgOrderVerificationFail(const chatd::Message& msg, chatd::Idx idx, const std::string& errmsg);


protected:

private:
    MegaChatApiImpl *chatApi;
    MegaChatHandle chatid;

    chatd::Chat *mChat;
    karere::ChatRoom *mRoom;
};

class LoggerHandler : public karere::Logger::ILoggerBackend
{
public:
    LoggerHandler();
    virtual ~LoggerHandler();

    void setMegaChatLogger(MegaChatLogger *logger);
    void setLogLevel(int logLevel);
    virtual void log(krLogLevel level, const char* msg, size_t len, unsigned flags);

private:
    mega::MegaMutex mutex;
    MegaChatLogger *megaLogger;
};

class MegaChatErrorPrivate :
        public MegaChatError,
        private promise::Error
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

class MegaChatRoomPrivate : public MegaChatRoom
{
public:
    MegaChatRoomPrivate(const MegaChatRoom *);
    MegaChatRoomPrivate(const karere::ChatRoom&);

    virtual ~MegaChatRoomPrivate() {}
    virtual MegaChatRoom *copy() const;

    virtual MegaChatHandle getChatId() const;
    virtual int getOwnPrivilege() const;
    virtual int getPeerPrivilegeByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerFirstnameByHandle(MegaChatHandle userhandle) const;
    virtual const char *getPeerLastnameByHandle(MegaChatHandle userhandle) const;
    virtual int getPeerPrivilege(unsigned int i) const;
    virtual unsigned int getPeerCount() const;
    virtual MegaChatHandle getPeerHandle(unsigned int i) const;
    virtual const char *getPeerFirstname(unsigned int i) const;
    virtual const char *getPeerLastname(unsigned int i) const;
    virtual bool isGroup() const;
    virtual const char *getTitle() const;
    virtual int getOnlineState() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual int getUnreadCount() const;
    virtual int getOnlineStatus() const;
    virtual MegaChatHandle getUserTyping() const;

    void setTitle(const std::string &title);
    void setUnreadCount(int count);
    void setOnlineStatus(int status);
    void setMembersUpdated();
    void setOnlineState(int state);
    void setUserTyping(MegaChatHandle uh);
    void setClosed();

private:
    int changed;

    MegaChatHandle chatid;
    int priv;
    mega::userpriv_vector peers;
    std::vector<std::string> peerFirstnames;
    std::vector<std::string> peerLastnames;
    bool group;

    std::string title;
    int unreadCount;
    int status;
    int chatState;
    MegaChatHandle uh;

public:
    // you take the ownership of return value
    static const char *firstnameFromBuffer(const std::string &buffer);

    // you take the ownership of return value
    static const char *lastnameFromBuffer(const std::string &buffer);
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
    virtual int64_t getTimestamp() const;
    virtual const char *getContent() const;
    virtual bool isEdited() const;
    virtual bool isDeleted() const;
    virtual bool isEditable() const;
    virtual bool isManagementMessage() const;
    virtual MegaChatHandle getUserHandleOfAction() const;
    virtual int getPrivilege() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    void setStatus(int status);
    void setTempId(MegaChatHandle tempId);
    void setContentChanged();

private:
    int changed;

    int type;
    int status;
    MegaChatHandle msgId;   // definitive unique ID given by server
    MegaChatHandle tempId;  // used until it's given a definitive ID by server
    MegaChatHandle uh;
    MegaChatHandle uhAction;// certain messages need additional userhandle, such us priv changes
    int index;              // position within the history buffer
    int64_t ts;
    char *msg;
    bool edited;
    bool deleted;
    int priv;               // certain messages need additional info, like priv changes
};

//Thread safe request queue
class ChatRequestQueue
{
    protected:
        std::deque<MegaChatRequestPrivate *> requests;
        mega::MegaMutex mutex;

    public:
        ChatRequestQueue();
        void push(MegaChatRequestPrivate *request);
        void push_front(MegaChatRequestPrivate *request);
        MegaChatRequestPrivate * pop();
        void removeListener(MegaChatRequestListener *listener);
};

//Thread safe transfer queue
class EventQueue
{
protected:
    std::deque<void *> events;
    mega::MegaMutex mutex;

public:
    EventQueue();
    void push(void* event);
    void push_front(void *event);
    void* pop();
};


class MegaChatApiImpl :
        public karere::IApp,
        public karere::IApp::IChatListHandler,
        public mega::MegaRequestListener
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, mega::MegaApi *megaApi);
//    MegaChatApiImpl(MegaChatApi *chatApi, const char *appKey, const char *appDir);
    virtual ~MegaChatApiImpl();

    static std::vector<MegaChatApiImpl *> megaChatApiRefs;
    static mega::MegaMutex refsMutex;
    static mega::MegaMutex sdkMutex;

private:
    MegaChatApi *chatApi;
    mega::MegaApi *megaApi;

    karere::Client *mClient;

    mega::MegaWaiter *waiter;
    mega::MegaThread thread;
    int threadExit;
    static void *threadEntryPoint(void *param);
    void loop();

    void init(MegaChatApi *chatApi, mega::MegaApi *megaApi);
    bool resumeSession;
    MegaChatError *initResult;
    MegaChatRequestPrivate *initRequest;

    static LoggerHandler *loggerHandler;

    ChatRequestQueue requestQueue;
    EventQueue eventQueue;

    std::set<MegaChatListener *> listeners;
    std::set<MegaChatRoomListener *> roomListeners;
    std::set<MegaChatRequestListener *> requestListeners;
    std::set<MegaChatCallListener *> callListeners;
    std::set<MegaChatVideoListener *> localVideoListeners;
    std::set<MegaChatVideoListener *> remoteVideoListeners;

    std::set<MegaChatPeerListItemHandler *> chatPeerListItemHandler;
    std::set<MegaChatGroupListItemHandler *> chatGroupListItemHandler;
    std::map<MegaChatHandle, MegaChatRoomHandler*> chatRoomHandler;

    int reqtag;
    std::map<int, MegaChatRequestPrivate *> requestMap;
    std::map<int, MegaChatCallPrivate *> callMap;
    MegaChatVideoReceiver *localVideoReceiver;

    // online status of user
    int status;

public:    
    static void megaApiPostMessage(void* msg);
    void postMessage(void *msg);

    void sendPendingRequests();
    void sendPendingEvents();

    static void setLogLevel(int logLevel);
    static void setLoggerClass(MegaChatLogger *megaLogger);

    MegaChatRoomHandler* getChatRoomHandler(MegaChatHandle chatid);
    void removeChatRoomHandler(MegaChatHandle chatid);

    karere::ChatRoom *findChatRoom(MegaChatHandle chatid);
    karere::ChatRoom *findChatRoomByUser(MegaChatHandle userhandle);
    chatd::Message *findMessage(MegaChatHandle chatid, MegaChatHandle msgid);
    chatd::Message *findMessageNotConfirmed(MegaChatHandle chatid, MegaChatHandle msgxid);

    // ============= Listeners ================

    // Registration
    void addChatCallListener(MegaChatCallListener *listener);
    void addChatRequestListener(MegaChatRequestListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void addChatListener(MegaChatListener *listener);
    void addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);
    void removeChatCallListener(MegaChatCallListener *listener);
    void removeChatRequestListener(MegaChatRequestListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatListener(MegaChatListener *listener);
    void removeChatRoomListener(MegaChatRoomListener *listener);

    // MegaChatRequestListener callbacks
    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaChatError *e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaChatError *e);

    // MegaChatCallListener callbacks
    void fireOnChatCallStart(MegaChatCallPrivate *call);
    void fireOnChatCallStateChange(MegaChatCallPrivate *call);
    void fireOnChatCallTemporaryError(MegaChatCallPrivate *call, MegaChatError *e);
    void fireOnChatCallFinish(MegaChatCallPrivate *call, MegaChatError *e);

    // MegaChatVideoListener callbacks
    void fireOnChatRemoteVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer);
    void fireOnChatLocalVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer);

    // MegaChatRoomListener callbacks
    void fireOnChatRoomUpdate(MegaChatRoom *chat);
    void fireOnMessageLoaded(MegaChatMessage *msg);
    void fireOnMessageReceived(MegaChatMessage *msg);
    void fireOnMessageUpdate(MegaChatMessage *msg);

    // MegaChatRoomListener callbacks (specific ones)
    void fireOnChatListItemUpdate(MegaChatListItem *item);

    // ============= API requests ================

    // General chat methods
    void init(MegaChatRequestListener *listener = NULL);
    void connect(MegaChatRequestListener *listener = NULL);
    void logout(MegaChatRequestListener *listener = NULL);
    void localLogout(MegaChatRequestListener *listener = NULL);

    void setOnlineStatus(int status, MegaChatRequestListener *listener = NULL);
    int getOnlineStatus();
    int getUserOnlineStatus(MegaChatHandle userhandle);
    void getUserFirstname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    void getUserLastname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    MegaChatRoomList* getChatRooms();
    MegaChatRoom* getChatRoom(MegaChatHandle chatid);
    MegaChatRoom *getChatRoomByUser(MegaChatHandle userhandle);

    // Chatrooms management
    void createChat(bool group, MegaChatPeerList *peerList, MegaChatRequestListener *listener = NULL);
    void inviteToChat(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);
    void removeFromChat(MegaChatHandle chatid, MegaChatHandle uh = MEGACHAT_INVALID_HANDLE, MegaChatRequestListener *listener = NULL);
    void updateChatPermissions(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);
    void truncateChat(MegaChatHandle chatid, MegaChatHandle messageid, MegaChatRequestListener *listener = NULL);
    void setChatTitle(MegaChatHandle chatid, const char *title, MegaChatRequestListener *listener = NULL);

    bool openChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener = NULL);
    void closeChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener = NULL);

    int loadMessages(MegaChatHandle chatid, int count);
    bool isFullHistoryLoaded(MegaChatHandle chatid);
    MegaChatMessage *getMessage(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *sendMessage(MegaChatHandle chatid, const char* msg);
    MegaChatMessage *editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char* msg);
    bool setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *getLastMessageSeen(MegaChatHandle chatid);
    void removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle tempid);

    // Audio/Video devices
    mega::MegaStringList *getChatAudioInDevices();
    mega::MegaStringList *getChatVideoInDevices();
    bool setChatAudioInDevice(const char *device);
    bool setChatVideoInDevice(const char *device);

    // Calls
    void startChatCall(mega::MegaUser *peer, bool enableVideo = true, MegaChatRequestListener *listener = NULL);
    void answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener = NULL);
    void hangAllChatCalls();

//    MegaChatCallPrivate *getChatCallByPeer(const char* jid);


    // ============= karere API implementation ================

    // karere::IApp implementation
    //virtual ILoginDialog* createLoginDialog();
    virtual IApp::IChatHandler *createChatHandler(karere::ChatRoom &chat);
    virtual IApp::IContactListHandler *contactListHandler();
    virtual IApp::IChatListHandler *chatListHandler();
    virtual void onOwnPresence(karere::Presence pres);
    virtual void onIncomingContactRequest(const mega::MegaContactRequest& req);
    virtual rtcModule::IEventHandler* onIncomingCall(const std::shared_ptr<rtcModule::ICallAnswer>& ans);
    virtual void notifyInvited(const karere::ChatRoom& room);
    virtual void onTerminate();

    // rtcModule::IChatListHandler implementation
    virtual IApp::IGroupChatListItem *addGroupChatItem(karere::GroupChatRoom &chat);
    virtual void removeGroupChatItem(IApp::IGroupChatListItem& item);
    virtual IApp::IPeerChatListItem *addPeerChatItem(karere::PeerChatRoom& chat);
    virtual void removePeerChatItem(IApp::IPeerChatListItem& item);

    // mega::MegaRequestListener implementation
//    virtual void onRequestStart(MegaApi* api, MegaRequest *request);
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e);
//    virtual void onRequestUpdate(MegaApi*api, MegaRequest *request);
//    virtual void onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError* error);

};


//public karere::IApp::IChatHandler
// public rtcModule::IEventHandler

// rtcModule::IEventHandler implementation
//    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer** renderer);
//    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, rtcModule::IVideoRenderer** rendererRet);
//    virtual void onCallIncomingRequest(rtcModule::ICallAnswer* ctrl);
//    virtual void onIncomingCallCanceled(const char *sid, const char *event, const char *by, int accepted, void **userp);
//    virtual void onCallEnded(rtcModule::IJingleSession *sess, const char* reason, const char* text, rtcModule::stats::IRtcStats *stats);
//    virtual void discoAddFeature(const char *feature);
//    virtual void onLocalMediaFail(const char* err, int* cont = nullptr);
//    virtual void onCallInit(rtcModule::IJingleSession* sess, int isDataCall);
//    virtual void onCallDeclined(const char* fullPeerJid, const char* sid, const char* reason, const char* text, int isDataCall);
//    virtual void onCallAnswerTimeout(const char* peer);
//    virtual void onCallAnswered(rtcModule::IJingleSession* sess);
//    virtual void remotePlayerRemove(rtcModule::IJingleSession* sess, rtcModule::IVideoRenderer* videoRenderer);
//    virtual void onMediaRecv(rtcModule::IJingleSession* sess, rtcModule::stats::Options* statOptions);
//    virtual void onJingleError(rtcModule::IJingleSession* sess, const char* origin, const char* stanza, const char* origXml, char type);
//    virtual void onLocalVideoDisabled();
//    virtual void onLocalVideoEnabled();

// karere::IApp::IChatHandler implementation
//    virtual ICallGui* callGui();
//    virtual rtcModule::IEventHandler* callEventHandler();
//    virtual void init(chatd::Chat& messages, chatd::DbInterface*& dbIntf);


}

#endif // MEGACHATAPI_IMPL_H
