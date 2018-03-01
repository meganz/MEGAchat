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

#ifndef KARERE_DISABLE_WEBRTC
#include <rtcModule/webrtc.h>
#include <IVideoRenderer.h>
#endif

#include <chatClient.h>
#include <chatd.h>
#include <sdkApi.h>
#include <karereCommon.h>
#include <logger.h>

#include "net/websocketsIO.h"

#include <stdint.h>

#ifdef USE_LIBWEBSOCKETS

#include "net/libwebsocketsIO.h"
#include "waiter/libuvWaiter.h"

typedef LibwebsocketsIO MegaWebsocketsIO;
typedef ::mega::LibuvWaiter MegaChatWaiter;

#else

#include "net/libwsIO.h"
#include "waiter/libeventWaiter.h"

typedef LibwsIO MegaWebsocketsIO;
typedef ::mega::LibeventWaiter MegaChatWaiter;

#endif

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
    virtual MegaChatMessage *getMegaChatMessage();
    virtual mega::MegaNodeList *getMegaNodeList();
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
    void setText(const char *text);
    void setMegaChatMessage(MegaChatMessage *message);
    void setMegaNodeList(mega::MegaNodeList *nodelist);
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
    MegaChatMessage* mMessage;
    mega::MegaNodeList* mMegaNodeList;
    int mParamType;
};

class MegaChatPresenceConfigPrivate : public MegaChatPresenceConfig
{
public:
    MegaChatPresenceConfigPrivate(const MegaChatPresenceConfigPrivate &config);
    MegaChatPresenceConfigPrivate(const presenced::Config &config, bool isPending);
    virtual ~MegaChatPresenceConfigPrivate();
    virtual MegaChatPresenceConfig *copy() const;

    virtual int getOnlineStatus() const;
    virtual bool isAutoawayEnabled() const;
    virtual int64_t getAutoawayTimeout() const;
    virtual bool isPersist() const;
    virtual bool isPending() const;
    virtual bool isSignalActivityRequired() const;

private:
    int status;
    bool persistEnabled;
    bool autoawayEnabled;
    int64_t autoawayTimeout;
    bool pending;
};


#ifndef KARERE_DISABLE_WEBRTC

class MegaChatVideoReceiver;

class MegaChatCallPrivate :
        public MegaChatCall
{
public:
    MegaChatCallPrivate(const rtcModule::ICall& call);
    MegaChatCallPrivate(const MegaChatCallPrivate &call);

    virtual ~MegaChatCallPrivate();

    virtual MegaChatCall *copy();

    virtual int getStatus() const;
    virtual MegaChatHandle getChatid() const;
    virtual MegaChatHandle getId() const;

    virtual bool hasLocalAudio();
    virtual bool hasRemoteAudio();
    virtual bool hasLocalVideo();
    virtual bool hasRemoteVideo();

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual int64_t getDuration() const;
    virtual int64_t getInitialTimeStamp() const;
    virtual int64_t getFinalTimeStamp() const;
    virtual const char *getTemporaryError() const;
    virtual int getTermCode() const;
    virtual bool isLocalTermCode() const;
    virtual bool isRinging() const;

    void setStatus(int status);
    void setLocalAudioVideoFlags(karere::AvFlags localAVFlags);
    void setRemoteAudioVideoFlags(karere::AvFlags remoteAVFlags);
    void setInitialTimeStamp(int64_t timeStamp);
    void setFinalTimeStamp(int64_t timeStamp);
    void removeChanges();
    void setError(const std::string &temporaryError);
    void setTermCode(rtcModule::TermCode termCode);
    void setIsRinging(bool ringing);

protected:
    MegaChatHandle chatid;
    int status;
    MegaChatHandle callid;
    karere::AvFlags localAVFlags;
    karere::AvFlags remoteAVFlags;
    int changed;
    int64_t initialTs;
    int64_t finalTs;
    std::string temporaryError;

    int termCode;
    bool localTermCode;
    void convertTermCode(rtcModule::TermCode termCode);

    bool ringing;
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
    MegaChatVideoReceiver(MegaChatApiImpl *chatApi, rtcModule::ICall *call, bool local);
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
    MegaChatApiImpl *chatApi;
    rtcModule::ICall *call;
    MegaChatHandle chatid;
    bool local;
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
    bool active;
    MegaChatHandle peerHandle;  // only for 1on1 chatrooms
    MegaChatHandle mLastMsgId;

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
    virtual bool isActive() const;
    virtual MegaChatHandle getPeerHandle() const;

    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void setUnreadCount(int count);
    void setMembersUpdated();
    void setClosed();
    void setLastTimestamp(int64_t ts);

    /**
     * If the message is of type MegaChatMessage::TYPE_ATTACHMENT, this function
     * recives the filenames of the attached nodes. The filenames of nodes are separated
     * by ASCII character '0x01'
     * If the message is of type MegaChatMessage::TYPE_CONTACT, this function
     * recives the usernames. The usernames are separated
     * by ASCII character '0x01'
     */
    void setLastMessage(MegaChatHandle messageId, int type, const std::string &msg, const uint64_t uh);
};

class MegaChatListItemHandler :public virtual karere::IApp::IChatListItem
{
public:
    MegaChatListItemHandler(MegaChatApiImpl&, karere::ChatRoom&);

    // karere::IApp::IListItem::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged(int count);

    // karere::IApp::IListItem::IChatListItem implementation
    virtual void onExcludedFromChat();
    virtual void onRejoinedChat();
    virtual void onLastMessageUpdated(const chatd::LastTextMsg& msg);
    virtual void onLastTsUpdated(uint32_t ts);
    virtual void onChatOnlineState(const chatd::ChatState state);

    virtual const karere::ChatRoom& getChatRoom() const;

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
    MegaChatRoomHandler(MegaChatApiImpl*, MegaChatHandle chatid);

    // karere::IApp::IChatHandler implementation
#ifndef KARERE_DISABLE_WEBRTC
    virtual rtcModule::ICallHandler* callHandler();
#endif
    virtual void onMemberNameChanged(uint64_t userid, const std::string &newName);
    //virtual void* userp();


    // karere::IApp::IChatHandler::ITitleHandler implementation
    virtual void onTitleChanged(const std::string& title);
    virtual void onUnreadCountChanged(int count);

    // karere::IApp::IChatHandler::chatd::Listener implementation
    virtual void init(chatd::Chat& chat, chatd::DbInterface*&);
    virtual void onDestroy();
    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status);
    virtual void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isLocal);
    virtual void onHistoryDone(chatd::HistSource source);
    virtual void onUnsentMsgLoaded(chatd::Message& msg);
    virtual void onUnsentEditLoaded(chatd::Message& msg, bool oriMsgIsSending);
    virtual void onMessageConfirmed(karere::Id msgxid, const chatd::Message& msg, chatd::Idx idx);
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
    virtual void onManualSendRequired(chatd::Message* msg, uint64_t id, chatd::ManualSendReason reason);
    //virtual void onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx);
    //virtual void onMsgOrderVerificationFail(const chatd::Message& msg, chatd::Idx idx, const std::string& errmsg);
    virtual void onUserTyping(karere::Id user);
    virtual void onLastTextMessageUpdated(const chatd::LastTextMsg& msg);
    virtual void onLastMessageTsUpdated(uint32_t ts);
    virtual void onHistoryReloaded();

    bool isRevoked(MegaChatHandle h);
    // update access to attachments
    void handleHistoryMessage(MegaChatMessage *message);
    // update access to attachments, returns messages requiring updates (you take ownership)
    std::set<MegaChatHandle> *handleNewMessage(MegaChatMessage *msg);

protected:

private:
    MegaChatApiImpl *chatApi;
    MegaChatHandle chatid;

    chatd::Chat *mChat;
    karere::ChatRoom *mRoom;

    // nodes with granted/revoked access from loaded messsages
    std::map<MegaChatHandle, bool> attachmentsAccess;  // handle, access
    std::map<MegaChatHandle, std::set<MegaChatHandle>> attachmentsIds;    // nodehandle, msgids
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
    mega::MegaMutex mutex;
    MegaChatLogger *megaLogger;
};

#ifndef KARERE_DISABLE_WEBRTC
class MegaChatSessionHandler;

class MegaChatCallHandler : public rtcModule::ICallHandler
{
public:
    MegaChatCallHandler(MegaChatApiImpl *megaChatApi);
    MegaChatCallHandler(const MegaChatCallHandler& callHandler);
    virtual ~MegaChatCallHandler();
    virtual void setCall(rtcModule::ICall* call);
    virtual void onStateChange(uint8_t newState);
    virtual void onDestroy(rtcModule::TermCode reason, bool byPeer, const std::string& msg);
    virtual rtcModule::ISessionHandler *onNewSession(rtcModule::ISession& sess);
    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer*& rendererOut);
    virtual void onLocalMediaError(const std::string errors);
    virtual void onRingOut(karere::Id peer);
    virtual void onCallStarting();
    virtual void onCallStarted();
    virtual ICallHandler* copy();

    rtcModule::ICall *getCall();
    MegaChatCallPrivate *getMegaChatCall();
private:
    MegaChatApiImpl *megaChatApi;
    rtcModule::ICall *call;
    MegaChatCallPrivate *chatCall;

    MegaChatSessionHandler *sessionHandler;
    rtcModule::IVideoRenderer *localVideoReceiver;
};

class MegaChatSessionHandler : public rtcModule::ISessionHandler
{
public:
    MegaChatSessionHandler(MegaChatApiImpl *megaChatApi, MegaChatCallHandler* callHandler, rtcModule::ISession *session);
    ~MegaChatSessionHandler();
    virtual void onSessStateChange(uint8_t newState);
    virtual void onSessDestroy(rtcModule::TermCode reason, bool byPeer, const std::string& msg);
    virtual void onRemoteStreamAdded(rtcModule::IVideoRenderer*& rendererOut);
    virtual void onRemoteStreamRemoved();
    virtual void onPeerMute(karere::AvFlags av, karere::AvFlags oldAv);
    virtual void onVideoRecv();

private:
    MegaChatApiImpl *megaChatApi;
    MegaChatCallHandler *callHandler;
    rtcModule::ISession *session;
    rtcModule::IVideoRenderer *remoteVideoRender;

};
#endif

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
    virtual const char *getTitle() const;
    virtual bool isActive() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    virtual int getUnreadCount() const;
    virtual MegaChatHandle getUserTyping() const;

    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void setUnreadCount(int count);
    void setMembersUpdated();
    void setUserTyping(MegaChatHandle uh);
    void setClosed();

private:
    int changed;

    MegaChatHandle chatid;
    mega::privilege_t priv;
    mega::userpriv_vector peers;
    std::vector<std::string> peerFirstnames;
    std::vector<std::string> peerLastnames;
    std::vector<std::string> peerEmails;
    bool group;
    bool active;

    std::string title;
    int unreadCount;
    MegaChatHandle uh;

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

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    void setStatus(int status);
    void setTempId(MegaChatHandle tempId);
    void setRowId(int id);
    void setContentChanged();
    void setCode(int code);
    void setAccess();

private:
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
    std::vector<MegaChatAttachedUser>* megaChatUsers;
    mega::MegaNodeList* megaNodeList;
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
    bool isEmpty();
    size_t size();
};

class MegaChatApiImpl :
        public karere::IApp,
        public karere::IApp::IChatListHandler
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, mega::MegaApi *megaApi);
    virtual ~MegaChatApiImpl();

    mega::MegaMutex sdkMutex;
    mega::MegaMutex videoMutex;
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
    std::set<MegaChatRoomListener *> roomListeners;
    std::set<MegaChatRequestListener *> requestListeners;

    std::set<MegaChatPeerListItemHandler *> chatPeerListItemHandler;
    std::set<MegaChatGroupListItemHandler *> chatGroupListItemHandler;
    std::map<MegaChatHandle, MegaChatRoomHandler*> chatRoomHandler;

    int reqtag;
    std::map<int, MegaChatRequestPrivate *> requestMap;

#ifndef KARERE_DISABLE_WEBRTC
    std::set<MegaChatCallListener *> callListeners;
    std::set<MegaChatVideoListener *> localVideoListeners;
    std::set<MegaChatVideoListener *> remoteVideoListeners;

    std::map<MegaChatHandle, MegaChatCallHandler*> callHandlers;

    mega::MegaStringList *getChatInDevices(const std::vector<std::string> &devicesVector);
    void cleanCallHandlerMap();
#endif

    static int convertInitState(int state);

    void sendAttachNodesMessage(std::string buffer, MegaChatRequestPrivate* request);

public:
    static void megaApiPostMessage(void* msg, void* ctx);
    void postMessage(void *msg);

    void sendPendingRequests();
    void sendPendingEvents();

    static void setLogLevel(int logLevel);
    static void setLoggerClass(MegaChatLogger *megaLogger);
    static void setLogWithColors(bool useColors);
    static void setLogToConsole(bool enable);

    int init(const char *sid);
    int getInitState();

    MegaChatRoomHandler* getChatRoomHandler(MegaChatHandle chatid);
    void removeChatRoomHandler(MegaChatHandle chatid);

    karere::ChatRoom *findChatRoom(MegaChatHandle chatid);
    karere::ChatRoom *findChatRoomByUser(MegaChatHandle userhandle);
    chatd::Message *findMessage(MegaChatHandle chatid, MegaChatHandle msgid);
    chatd::Message *findMessageNotConfirmed(MegaChatHandle chatid, MegaChatHandle msgxid);

#ifndef KARERE_DISABLE_WEBRTC
    MegaChatCallHandler *findChatCallHandler(MegaChatHandle chatid);
    void removeChatCallHandler(MegaChatHandle chatid);
#endif

    static void setCatchException(bool enable);

    // ============= Listeners ================

    // Registration
    void addChatRequestListener(MegaChatRequestListener *listener);
    void addChatListener(MegaChatListener *listener);
    void addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);
    void removeChatRequestListener(MegaChatRequestListener *listener);
    void removeChatListener(MegaChatListener *listener);
    void removeChatRoomListener(MegaChatRoomListener *listener);
#ifndef KARERE_DISABLE_WEBRTC
    void addChatCallListener(MegaChatCallListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatCallListener(MegaChatCallListener *listener);
#endif

    // MegaChatRequestListener callbacks
    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaChatError *e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaChatError *e);

#ifndef KARERE_DISABLE_WEBRTC
    // MegaChatCallListener callbacks
    void fireOnChatCallUpdate(MegaChatCallPrivate *call);

    // MegaChatVideoListener callbacks
    void fireOnChatRemoteVideoData(MegaChatHandle chatid, int width, int height, char*buffer);
    void fireOnChatLocalVideoData(MegaChatHandle chatid, int width, int height, char*buffer);
#endif

    // MegaChatRoomListener callbacks
    void fireOnChatRoomUpdate(MegaChatRoom *chat);
    void fireOnMessageLoaded(MegaChatMessage *msg);
    void fireOnMessageReceived(MegaChatMessage *msg);
    void fireOnMessageUpdate(MegaChatMessage *msg);
    void fireOnHistoryReloaded(MegaChatRoom *chat);

    // MegaChatListener callbacks (specific ones)
    void fireOnChatListItemUpdate(MegaChatListItem *item);
    void fireOnChatInitStateUpdate(int newState);
    void fireOnChatOnlineStatusUpdate(MegaChatHandle userhandle, int status, bool inProgress);
    void fireOnChatPresenceConfigUpdate(MegaChatPresenceConfig *config);
    void fireOnChatConnectionStateUpdate(MegaChatHandle chatid, int newState);

    // ============= API requests ================

    // General chat methods
    void connect(MegaChatRequestListener *listener = NULL);
    void connectInBackground(MegaChatRequestListener *listener = NULL);
    void disconnect(MegaChatRequestListener *listener = NULL);
    int getConnectionState();
    int getChatConnectionState(MegaChatHandle chatid);
    static int convertChatConnectionState(chatd::ChatState state);
    void retryPendingConnections(MegaChatRequestListener *listener = NULL);
    void logout(MegaChatRequestListener *listener = NULL);
    void localLogout(MegaChatRequestListener *listener = NULL);

    void setOnlineStatus(int status, MegaChatRequestListener *listener = NULL);
    int getOnlineStatus();
    bool isOnlineStatusPending();

    void setPresenceAutoaway(bool enable, int64_t timeout, MegaChatRequestListener *listener = NULL);
    void setPresencePersist(bool enable, MegaChatRequestListener *listener = NULL);
    void signalPresenceActivity(MegaChatRequestListener *listener = NULL);
    MegaChatPresenceConfig *getPresenceConfig();
    bool isSignalActivityRequired();

    int getUserOnlineStatus(MegaChatHandle userhandle);
    void setBackgroundStatus(bool background, MegaChatRequestListener *listener = NULL);

    void getUserFirstname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    void getUserLastname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    void getUserEmail(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);
    char *getContactEmail(MegaChatHandle userhandle);
    MegaChatHandle getUserHandleByEmail(const char *email);
    MegaChatHandle getMyUserHandle();
    char *getMyFirstname();
    char *getMyLastname();
    char *getMyFullname();
    char *getMyEmail();
    MegaChatRoomList* getChatRooms();
    MegaChatRoom* getChatRoom(MegaChatHandle chatid);
    MegaChatRoom *getChatRoomByUser(MegaChatHandle userhandle);
    MegaChatListItemList *getChatListItems();
    MegaChatListItem *getChatListItem(MegaChatHandle chatid);
    int getUnreadChats();
    MegaChatListItemList *getActiveChatListItems();
    MegaChatListItemList *getInactiveChatListItems();
    MegaChatListItemList *getUnreadChatListItems();
    MegaChatHandle getChatHandleByUser(MegaChatHandle userhandle);

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
    MegaChatMessage *getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid);
    MegaChatMessage *sendMessage(MegaChatHandle chatid, const char* msg);
    MegaChatMessage *attachContacts(MegaChatHandle chatid, mega::MegaHandleList* handles);
    void attachNodes(MegaChatHandle chatid, mega::MegaNodeList *nodes, MegaChatRequestListener *listener = NULL);
    void attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);
    void revokeAttachment(MegaChatHandle chatid, MegaChatHandle handle, MegaChatRequestListener *listener = NULL);
    bool isRevoked(MegaChatHandle chatid, MegaChatHandle nodeHandle);
    MegaChatMessage *editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char* msg);
    bool setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid);
    MegaChatMessage *getLastMessageSeen(MegaChatHandle chatid);
    void removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowid);
    void sendTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    bool isMessageReceptionConfirmationActive() const;
    void saveCurrentState();

#ifndef KARERE_DISABLE_WEBRTC

    // Audio/Video devices
    mega::MegaStringList *getChatAudioInDevices();
    mega::MegaStringList *getChatVideoInDevices();
    bool setChatAudioInDevice(const char *device);
    bool setChatVideoInDevice(const char *device);

    // Calls
    void startChatCall(MegaChatHandle chatid, bool enableVideo = true, MegaChatRequestListener *listener = NULL);
    void answerChatCall(MegaChatHandle chatid, bool enableVideo = true, MegaChatRequestListener *listener = NULL);
    void hangChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void hangAllChatCalls(MegaChatRequestListener *listener);
    void setAudioEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void setVideoEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void loadAudioVideoDeviceList(MegaChatRequestListener *listener = NULL);
    MegaChatCall *getChatCall(MegaChatHandle chatId);
    MegaChatCall *getChatCallByCallId(MegaChatHandle callId);
    int getNumCalls();
    mega::MegaHandleList *getChatCalls();
    mega::MegaHandleList *getChatCallsIds();
#endif

//    MegaChatCallPrivate *getChatCallByPeer(const char* jid);


    // ============= karere API implementation ================

    // karere::IApp implementation
    //virtual ILoginDialog* createLoginDialog();
    virtual IApp::IChatHandler *createChatHandler(karere::ChatRoom &chat);
    virtual IApp::IContactListHandler *contactListHandler();
    virtual IApp::IChatListHandler *chatListHandler();
    virtual void onPresenceChanged(karere::Id userid, karere::Presence pres, bool inProgress);
    virtual void onPresenceConfigChanged(const presenced::Config& state, bool pending);
    virtual void onIncomingContactRequest(const mega::MegaContactRequest& req);
#ifndef KARERE_DISABLE_WEBRTC
    virtual rtcModule::ICallHandler *onIncomingCall(rtcModule::ICall& call, karere::AvFlags av);
#endif
    virtual void notifyInvited(const karere::ChatRoom& room);
    virtual void onInitStateChange(int newState);

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

class DataTranslation
{
public:
    /**
     * @brief Transform binary string into a vector. For example: string.length() = 32 => vector.size() = 8
     * The vector output is similar to "[669070598,-250738112,2059051645,-1942187558, 324123143, 86148965]"
     * @param data string in binary format
     * @return vector
     */
    static std::vector<int32_t> b_to_vector(const std::string& data);

    /**
     * @brief Transform int32_t vector into a birnary string. For example: string.length() = 32 => vector.size() = 8
     * The vector input is similar to "[669070598,-250738112,2059051645,-1942187558, 324123143, 86148965]"
     * @param data vector of int32_t
     * @return binary string
     */
    static std::string vector_to_b(std::vector<int32_t> vector);

};

class JSonUtils
{
public:
    // you take the ownership of the returned value. NULL if error
    static const char* generateAttachNodeJSon(mega::MegaNodeList* nodes);
    // you take the ownership of returned value. NULL if error
    static mega::MegaNodeList *parseAttachNodeJSon(const char* json);
    // you take the ownership of returned value. NULL if error
    static std::vector<MegaChatAttachedUser> *parseAttachContactJSon(const char* json);
    static std::string getLastMessageContent(const std::string &content, uint8_t type);

};

}

#endif // MEGACHATAPI_IMPL_H
