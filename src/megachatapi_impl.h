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
#include <bitset>

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
typedef std::map<Cid_t, MegaChatVideoListener_set> MegaChatPeerVideoListener_map;

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4250) // many, many of this form: 'megachat::MegaChatPeerListItemHandler': inherits 'megachat::MegaChatListItemHandler::megachat::MegaChatListItemHandler::onLastMessageUpdated' via dominance
#endif

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
    virtual MegaChatScheduledMeetingList* getMegaChatScheduledMeetingList() const;
    virtual MegaChatScheduledMeetingOccurrList* getMegaChatScheduledMeetingOccurrList() const;

    bool hasPerformRequest() const { return mPerformRequest != nullptr; }
    int performRequest() const { assert(hasPerformRequest()); return mPerformRequest(); }

    void setPerformRequest(std::function<int()> f) { mPerformRequest = f; }
    void setMegaChatScheduledMeetingList(const MegaChatScheduledMeetingList* schedMeetingList);
    void setMegaChatScheduledMeetingOccurrList(const MegaChatScheduledMeetingOccurrList* schedMeetingOccurrList);
    void setTag(int tag);
    void setListener(MegaChatRequestListener *listener);
    void setStringList(mega::MegaStringList* stringList);
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
    void setMegaHandleList(const mega::MegaHandleList* handlelist);
    void setMegaHandleListByChat(MegaChatHandle chatid, mega::MegaHandleList *handlelist);
    void setParamType(int paramType);

private:
    mega::MegaHandleList *doGetMegaHandleListByChat(MegaChatHandle chatid);
    // Perform the request by executing this function, instead of adding code to sendPendingRequests()
    std::function<int()> mPerformRequest;

protected:
    int mType;
    int mTag;
    MegaChatRequestListener *mListener;

    long long mNumber;
    int mRetry;
    bool mFlag;
    MegaChatPeerList *mPeerList;
    MegaChatHandle mChatid;
    MegaChatHandle mUserHandle;
    int mPrivilege;
    const char* mText;
    const char* mLink;
    MegaChatMessage* mMessage;
    mega::MegaNodeList* mMegaNodeList;
    mega::MegaHandleList *mMegaHandleList;
    std::map<MegaChatHandle, mega::MegaHandleList*> mMegaHandleListMap;
    std::unique_ptr<MegaChatScheduledMeetingList> mScheduledMeetingList;
    std::unique_ptr<MegaChatScheduledMeetingOccurrList> mScheduledMeetingOccurrList;
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
    virtual bool hasScreenShare() const override;
    virtual bool isHiResScreenShare() const override;
    virtual bool isLowResScreenShare() const override;
    virtual bool hasCamera() const override;
    virtual bool isLowResCamera() const override;
    virtual bool isHiResCamera() const override;
    virtual bool isOnHold() const override;
    virtual bool isRecording() const override;
    virtual int getChanges() const override;
    virtual int getTermCode() const override;
    virtual bool hasChanged(int changeType) const override;
    virtual bool isAudioDetected() const override;
    virtual bool canRecvVideoHiRes() const override;
    virtual bool canRecvVideoLowRes() const override;
    virtual bool isModerator() const override;

    char* avFlagsToString() const override;
    karere::AvFlags getAvFlags() const; // for internal use
    void setState(uint8_t state);
    void setAudioDetected(bool audioDetected);
    void setOnHold(bool onHold);
    void setChange(int change);
    void setRecording(const bool isRecording);
    void removeChanges();
    int convertTermCode(rtcModule::TermCode termCode);

private:
    uint8_t mState = MegaChatSession::SESSION_STATUS_INVALID;
    karere::Id mPeerId;
    Cid_t mClientId;
    karere::AvFlags mAvFlags = karere::AvFlags::kEmpty;
    int mTermCode = MegaChatSession::SESS_TERM_CODE_INVALID;
    int mChanged = MegaChatSession::CHANGE_TYPE_NO_CHANGES;
    bool mAudioDetected = false;
    bool mHasHiResTrack = false;
    bool mHasLowResTrack = false;
    bool mIsModerator = false;
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
    bool hasLocalScreenShare() const override;

    virtual int getChanges() const override;
    virtual bool hasChanged(int changeType) const override;

    bool hasUserHandRaised(const MegaChatHandle uh) const override;
    virtual bool hasUserSpeakPermission(const MegaChatHandle uh) const override;
    virtual int64_t getDuration() const override;
    virtual int64_t getInitialTimeStamp() const override;
    virtual int64_t getFinalTimeStamp() const override;
    virtual int getTermCode() const override;
    int getEndCallReason() const override;
    bool isSpeakRequestEnabled() const override;
    int getNotificationType() const override;
    MegaChatHandle getAuxHandle() const override;
    virtual bool isRinging() const override;
    virtual bool isOwnModerator() const override;
    mega::MegaHandleList* getSessionsClientidByUserHandle(const MegaChatHandle uh) const override;
    virtual mega::MegaHandleList *getSessionsClientid() const override;
    virtual MegaChatHandle getPeeridCallCompositionChange() const override;
    virtual int getCallCompositionChange() const override;
    virtual MegaChatSession *getMegaChatSession(MegaChatHandle clientId) override;
    virtual int getNumParticipants() const override;
    virtual mega::MegaHandleList *getPeeridParticipants() const override;
    virtual const mega::MegaHandleList* getModerators() const override;
    virtual const mega::MegaHandleList* getRaiseHandsList() const override;
    virtual bool isIgnored() const override;
    virtual bool isIncoming() const override;
    virtual bool isOutgoing() const override;
    virtual bool isOwnClientCaller() const override;
    virtual MegaChatHandle getCaller() const override;
    virtual bool isOnHold() const override;
    const char* getGenericMessage() const override;
    int getNetworkQuality() const override;
    bool hasUserPendingSpeakRequest(const MegaChatHandle uh) const override;
    int getWrJoiningState() const override;
    const MegaChatWaitingRoom* getWaitingRoom() const override;
    MegaChatHandle getHandle() const override;
    void setHandle(const MegaChatHandle h);
    bool getFlag() const override;
    void setFlag(const bool f);
    MegaChatTimeStamp getCallWillEndTs() const override;
    int getCallDurationLimit() const override;
    int getCallUsersLimit() const override;
    int getCallClientsLimit() const override;
    int getCallClientsPerUserLimit() const override;
    sfu::SfuInterface::CallLimits getCallLimits() const;
    void setStatus(int status);
    void removeChanges();
    void setChange(int changed);
    MegaChatSessionPrivate *addSession(rtcModule::ISession &sess);

    int availableAudioSlots();
    int availableVideoSlots();
    void setPeerid(const karere::Id& peerid, bool added);
    bool isParticipating(const karere::Id& userid) const;
    void setId(const karere::Id& callid);
    void setCaller(const karere::Id& caller);
    void setHandleList(const mega::MegaHandleList* handleList);
    void setSpeakersList(const ::mega::MegaHandleList* speakersList);
    const ::mega::MegaHandleList* getSpeakRequestsList() const override;
    const mega::MegaHandleList* getSpeakersList() const override;
    const mega::MegaHandleList* getHandleList() const override;
    void setNotificationType(int notificationType);
    void setAuxHandle(const MegaChatHandle h);
    void setTermCode(int termCode);
    void setMessage(const std::string &errMsg);
    void setOnHold(bool onHold);
    static int convertCallState(rtcModule::CallState newState);
    int convertTermCode(rtcModule::TermCode termCode);
    int convertSfuCmdToCode(const std::string& cmd) const;

protected:
    MegaChatHandle mChatid = MEGACHAT_INVALID_HANDLE;;
    int mStatus = MegaChatCall::CALL_STATUS_INITIAL;
    MegaChatHandle mCallId = MEGACHAT_INVALID_HANDLE;;
    karere::AvFlags mLocalAVFlags;
    int mChanged = MegaChatCall::CHANGE_TYPE_NO_CHANGES;
    int64_t mInitialTs = 0;
    int64_t mFinalTs = 0;
    std::map<MegaChatHandle, std::unique_ptr<MegaChatSession>> mSessions;
    std::vector<MegaChatHandle> mParticipants;
    MegaChatHandle mPeerId = MEGACHAT_INVALID_HANDLE;
    MegaChatHandle mHandle = MEGACHAT_INVALID_HANDLE;
    int mCallCompositionChange = MegaChatCall::NO_COMPOSITION_CHANGE;
    MegaChatHandle mCallerId;
    std::string mMessage;
    std::unique_ptr<::mega::MegaHandleList> mModerators;
    std::unique_ptr<::mega::MegaHandleList> mHandleList;
    std::unique_ptr<::mega::MegaHandleList> mSpeakersList;
    std::unique_ptr<::mega::MegaHandleList> mSpeakRequestsList;
    std::unique_ptr<::mega::MegaHandleList> mRaiseHandsList;
    std::unique_ptr<MegaChatWaitingRoom> mMegaChatWaitingRoom;

    int mTermCode = MegaChatCall::TERM_CODE_INVALID;
    int mEndCallReason = MegaChatCall::END_CALL_REASON_INVALID;
    int mNotificationType = MegaChatCall::NOTIFICATION_TYPE_INVALID;
    bool mIgnored = false;
    bool mRinging = false;
    bool mIsCaller = false;
    bool mIsOwnClientCaller = false;
    bool mOwnModerator = false;
    bool mSpeakRequest = false;
    bool mFlag = false;
    int mNetworkQuality = rtcModule::kNetworkQualityGood;
    int mWrJoiningState = MegaChatWaitingRoom::MWR_UNKNOWN;
    MegaChatHandle mAuxHandle = MEGACHAT_INVALID_HANDLE;
    mega::m_time_t mCallWillEndTs =
        mega::mega_invalid_timestamp; // Time stamp of the call finishing time if there is a call
                                      // duration restriction (mega_invalid_timestamp if not)
    sfu::SfuInterface::CallLimits mCallLimits; // Object storing all the limits for the call
};

class MegaChatWaitingRoomPrivate: public MegaChatWaitingRoom
{
public:
    virtual ~MegaChatWaitingRoomPrivate() override {};
    MegaChatWaitingRoom* copy() const override
    {
        return new MegaChatWaitingRoomPrivate(*this);
    }

    size_t size() const override
    {
        return mWaitingRoomUsers ? mWaitingRoomUsers->size() : 0;
    }

    mega::MegaHandleList* getUsers() const override;

    int getUserStatus(const uint64_t& userid) const override
    {
        if (!mWaitingRoomUsers) { return MWR_UNKNOWN; }
        return convertIntoValidStatus(mWaitingRoomUsers->getUserStatus(userid));
    }

protected:
    MegaChatWaitingRoomPrivate() = delete;
    MegaChatWaitingRoomPrivate(const MegaChatWaitingRoomPrivate&& other) = delete;
    MegaChatWaitingRoomPrivate& operator = (const MegaChatWaitingRoomPrivate& other) = delete;
    MegaChatWaitingRoomPrivate& operator = (MegaChatWaitingRoomPrivate&& other) = delete;

    MegaChatWaitingRoomPrivate(const rtcModule::KarereWaitingRoom& other)
    {
        mWaitingRoomUsers.reset(new rtcModule::KarereWaitingRoom(other));
    }

    MegaChatWaitingRoomPrivate(const MegaChatWaitingRoomPrivate& other)
        : mWaitingRoomUsers(other.getWaitingRoomUsers()
                                ? new rtcModule::KarereWaitingRoom(*other.getWaitingRoomUsers())
                                : nullptr)
    {
    }

    int convertIntoValidStatus(const int status) const
    {

        switch (static_cast<sfu::WrState>(status))
        {
            case sfu::WrState::WR_NOT_ALLOWED: return MWR_NOT_ALLOWED;
            case sfu::WrState::WR_ALLOWED:     return MWR_ALLOWED;
            case sfu::WrState::WR_UNKNOWN:
            default:                           return MWR_UNKNOWN;
        }
    }

    const rtcModule::KarereWaitingRoom* getWaitingRoomUsers() const { return mWaitingRoomUsers.get(); }
    friend class MegaChatCallPrivate;
private:

    std::unique_ptr<rtcModule::KarereWaitingRoom> mWaitingRoomUsers;
};

class MegaChatVideoFrame
{
public:
    unsigned char *buffer;
    int width;
    int height;
    int sourceType;
};

class MegaChatVideoReceiver : public rtcModule::IVideoRenderer
{
public:
    // no peerid --> local video from own user
    MegaChatVideoReceiver(MegaChatApiImpl *chatApi, const karere::Id& chatid, rtcModule::VideoResolution videoResolution, uint32_t clientId = 0);
    ~MegaChatVideoReceiver();

    void setWidth(int width);
    void setHeight(int height);

    // rtcModule::IVideoRenderer implementation
    virtual void* getImageBuffer(unsigned short width, unsigned short height, int sourceType, void*& userData);
    virtual void frameComplete(void* userData);
    virtual void onVideoAttach();
    virtual void onVideoDetach();
    virtual void clearViewport();
    virtual void released();

protected:
    MegaChatApiImpl *mChatApi;
    MegaChatHandle mChatid;
    rtcModule::VideoResolution mVideoResolution;
    uint32_t mClientId;
};

#endif

class MegaChatListItemPrivate : public MegaChatListItem
{
public:
    MegaChatListItemPrivate(karere::ChatRoom& chatroom);
    MegaChatListItemPrivate(const MegaChatListItem *item);
    virtual ~MegaChatListItemPrivate();
    MegaChatListItem *copy() const override;

private:
    int mChanged;

    MegaChatHandle chatid;
    int mOwnPriv;
    std::string mTitle;
    int unreadCount;
    std::string lastMsg;
    int lastMsgType;
    MegaChatHandle lastMsgSender;
    int64_t lastTs;
    bool group;
    bool mPublicChat;
    bool mPreviewMode;
    bool active;
    bool mArchived;
    bool mDeleted;  // only true when chatlink is takendown (see removeGroupChatItem())
    bool mIsCallInProgress;
    MegaChatHandle peerHandle;  // only for 1on1 chatrooms
    MegaChatHandle mLastMsgId;
    int lastMsgPriv;
    MegaChatHandle lastMsgHandle;
    unsigned int mNumPreviewers;
    bool mMeeting;

public:
    int getChanges() const override;
    bool hasChanged(int changeType) const override;

    MegaChatHandle getChatId() const override;
    const char *getTitle() const override;
    int getOwnPrivilege() const override;
    int getUnreadCount() const override;
    const char *getLastMessage() const override;
    MegaChatHandle getLastMessageId() const override;
    int getLastMessageType() const override;
    MegaChatHandle getLastMessageSender() const override;
    int64_t getLastTimestamp() const override;
    bool isGroup() const override;
    bool isPublic() const override;
    bool isNoteToSelf() const override;
    bool isPreview() const override;
    bool isActive() const override;
    bool isArchived() const override;
    bool isDeleted() const override;
    bool isCallInProgress() const override;
    MegaChatHandle getPeerHandle() const override;
    int getLastMessagePriv() const override;
    MegaChatHandle getLastMessageHandle() const override;
    unsigned int getNumPreviewers() const override;
    bool isMeeting() const override;

    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void changeUnreadCount();
    void setNumPreviewers(unsigned int numPrev);
    void setPreviewClosed();
    void setMembersUpdated();
    void setClosed();
    void setLastTimestamp(int64_t ts);
    void setArchived(bool);
    void setDeleted();
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
    void onTitleChanged(const std::string& title) override;
    void onUnreadCountChanged() override;

    // karere::IApp::IListItem::IChatListItem implementation
    void onExcludedFromChat() override;
    void onRejoinedChat() override;
    void onLastMessageUpdated(const chatd::LastTextMsg& msg) override;
    void onLastTsUpdated(uint32_t ts) override;
    void onChatOnlineState(const chatd::ChatState state) override;
    void onChatModeChanged(bool mode) override;
    void onChatArchived(bool archived) override;
    void onChatDeleted() const override;
    void onPreviewersCountUpdate(uint32_t numPrev) override;
    void onPreviewClosed() override;

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
    void onUserJoin(uint64_t userid, chatd::Priv priv) override;
    void onUserLeave(uint64_t userid) override;
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
    virtual void onMemberNameChanged(uint64_t userid, const std::string &newName) override;
    virtual void onChatArchived(bool archived) override;
    //virtual void* userp();


    // karere::IApp::IChatHandler::ITitleHandler implementation
    void onTitleChanged(const std::string& title) override;
    void onUnreadCountChanged() override;
    void onPreviewersCountUpdate(uint32_t numPrev) override;

    // karere::IApp::IChatHandler::chatd::Listener implementation
    void init(chatd::Chat& chat, chatd::DbInterface*&) override;
    void onDestroy() override;
    void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status) override;
    void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isLocal) override;
    void onHistoryDone(chatd::HistSource source) override;
    void onUnsentMsgLoaded(chatd::Message& msg) override;
    void onUnsentEditLoaded(chatd::Message& msg, bool oriMsgIsSending) override;
    void onMessageConfirmed(karere::Id msgxid, const chatd::Message& msg, chatd::Idx idx, bool tsUpdated) override;
    void onMessageRejected(const chatd::Message& msg, uint8_t reason) override;
    void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg) override;
    void onMessageEdited(const chatd::Message& msg, chatd::Idx idx) override;
    void onEditRejected(const chatd::Message& msg, chatd::ManualSendReason reason) override;
    void onOnlineStateChange(chatd::ChatState state) override;
    void onUserJoin(karere::Id userid, chatd::Priv privilege) override;
    void onUserLeave(karere::Id userid) override;
    void onExcludedFromChat() override;
    void onRejoinedChat() override;
    void onUnreadChanged() override;
    void onRetentionTimeUpdated(unsigned int period) override;
    void onPreviewersUpdate() override;
    void onManualSendRequired(chatd::Message* msg, uint64_t id, chatd::ManualSendReason reason) override;
    //virtual void onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx);
    //virtual void onMsgOrderVerificationFail(const chatd::Message& msg, chatd::Idx idx, const std::string& errmsg);
    void onUserTyping(karere::Id user) override;
    void onUserStopTyping(karere::Id user) override;
    void onLastTextMessageUpdated(const chatd::LastTextMsg& msg) override;
    void onLastMessageTsUpdated(uint32_t ts) override;
    void onHistoryReloaded() override;
    void onChatModeChanged(bool mode) override;
    void onChatOptionsChanged(int option) override;
    void onReactionUpdate(karere::Id msgid, const char *reaction, int count) override;
    void onHistoryTruncatedByRetentionTime(const chatd::Message &msg, const chatd::Idx &idx, const chatd::Message::Status &status) override;

    bool isRevoked(MegaChatHandle h);
    // update access to attachments
    void handleHistoryMessage(MegaChatMessage *message);
    // update access to attachments, returns messages requiring updates (you take ownership)
    std::set<MegaChatHandle> *handleNewMessage(MegaChatMessage *msg);

protected:

private:
    MegaChatApiImpl *mChatApiImpl;
    MegaChatApi *mChatApi;       // for notifications in callbacks
    mega::MegaApi *mMegaApi;
    MegaChatHandle mChatid;

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
    void fireOnAttachmentDeleted(const karere::Id& id);
    void fireOnTruncate(const karere::Id& id);

    void onReceived(chatd::Message* msg, chatd::Idx idx) override;
    void onLoaded(chatd::Message* msg, chatd::Idx idx) override;
    void onDeleted(karere::Id id) override;
    void onTruncated(karere::Id id) override;

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
    void setKarereMaxLogLevel(const unsigned int logLevel);
    unsigned int getKarereMaxLogLevel();
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
    void onCallWillEndr(rtcModule::ICall &call) override;
    void onCallLimitsUpdated(rtcModule::ICall &call) override;
    void onCallError(rtcModule::ICall &call, int code, const std::string &errMsg) override;
    void onNewSession(rtcModule::ISession& session, const rtcModule::ICall& call) override;
    void onLocalFlagsChanged(const rtcModule::ICall& call, const Cid_t cidPerf = K_INVALID_CID) override;
    void onOnHold(const rtcModule::ICall& call) override;
    void onAddPeer(const rtcModule::ICall &call, karere::Id peer) override;
    void onRemovePeer(const rtcModule::ICall &call,  karere::Id peer) override;
    void onNetworkQualityChanged(const rtcModule::ICall &call) override;
    void onStopOutgoingRinging(const rtcModule::ICall& call) override;
    void onPermissionsChanged(const rtcModule::ICall& call) override;
    void onWrUsersAllow(const rtcModule::ICall& call, const mega::MegaHandleList* users) override;
    void onWrUsersDeny(const rtcModule::ICall& call, const mega::MegaHandleList* users) override;
    void onWrUserDump(const rtcModule::ICall& call) override;
    void onWrAllow(const rtcModule::ICall& call) override;
    void onWrDeny(const rtcModule::ICall& call) override;
    void onWrUsersEntered(const rtcModule::ICall& call, const mega::MegaHandleList* users) override;
    void onWrUsersLeave(const rtcModule::ICall& call, const mega::MegaHandleList* users) override;
    void onWrPushedFromCall(const rtcModule::ICall& call) override;
    void onCallDeny(const rtcModule::ICall& call, const std::string& cmd, const std::string& msg) override;
    void onUserSpeakStatusUpdate(const rtcModule::ICall& call, const karere::Id& userid, const bool add) override;
    void onRaiseHandAddedRemoved(const rtcModule::ICall& call, const karere::Id& userid, const bool add) override;
    void onSpeakRequest(const rtcModule::ICall& call, const karere::Id& userid, const bool add) override;

private:
    MegaChatApiImpl* mMegaChatApi;
};
#endif

class MegaChatScheduledMeetingHandler: public karere::ScheduledMeetingHandler
{
public:
    MegaChatScheduledMeetingHandler(MegaChatApiImpl* megaChatApi);
    ~MegaChatScheduledMeetingHandler();
    void onSchedMeetingChange(const karere::KarereScheduledMeeting* sm, unsigned long diff) override;
    void onSchedMeetingOccurrencesChange(const karere::Id& id, bool append) override;

private:
    MegaChatApiImpl* mMegaChatApi;
};

#ifndef KARERE_DISABLE_WEBRTC
class MegaChatSessionHandler : public rtcModule::SessionHandler
{
public:
    MegaChatSessionHandler(MegaChatApiImpl *mMegaChatApi, const rtcModule::ICall& call);
    virtual ~MegaChatSessionHandler();
    void onVThumbReceived(rtcModule::ISession& session) override;
    void onHiResReceived(rtcModule::ISession& session) override;
    void onDestroySession(rtcModule::ISession& session) override;
    void onRemoteFlagsChanged(rtcModule::ISession& session) override;
    void onOnHold(rtcModule::ISession& session) override;
    void onRemoteAudioDetected(rtcModule::ISession& session) override;
    void onPermissionsChanged(rtcModule::ISession& session) override;
    void onRecordingChanged(rtcModule::ISession& session) override;

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
    MegaChatErrorPrivate(const ::promise::Error& err);
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
    std::vector<MegaChatListItem*> mList;
};

class MegaChatRoomPrivate : public MegaChatRoom
{
public:
    MegaChatRoomPrivate(const MegaChatRoom *);
    MegaChatRoomPrivate(const karere::ChatRoom&);

    virtual ~MegaChatRoomPrivate() {}
    MegaChatRoom *copy() const override;

    MegaChatHandle getChatId() const override;
    int getOwnPrivilege() const override;
    unsigned int getNumPreviewers() const override;
    int getPeerPrivilegeByHandle(MegaChatHandle userhandle) const override;
    int getPeerPrivilege(unsigned int i) const override;
    unsigned int getPeerCount() const override;
    MegaChatHandle getPeerHandle(unsigned int i) const override;
    bool isGroup() const override;
    bool isPublic() const override;
    bool isNoteToSelf() const override;
    bool isPreview() const override;
    const char *getAuthorizationToken() const override;
    const char *getTitle() const override;
    bool hasCustomTitle() const override;
    bool isActive() const override;
    bool isArchived() const override;
    bool isMeeting() const override;
    bool isWaitingRoom() const override;
    bool isOpenInvite() const override;
    bool isSpeakRequest() const override;
    int64_t getCreationTs() const override;

    int getChanges() const override;
    bool hasChanged(int changeType) const override;

    int getUnreadCount() const override;
    MegaChatHandle getUserHandle() const override;
    MegaChatHandle getUserTyping() const override;

    unsigned getRetentionTime() const override;

    void setRetentionTime(unsigned int period);
    void setOwnPriv(int ownPriv);
    void setTitle(const std::string &title);
    void changeUnreadCount();
    void changeChatRoomOption(int option);
    void setNumPreviewers(unsigned int numPrev);
    void setMembersUpdated(MegaChatHandle uh);
    void setUserTyping(MegaChatHandle uh);
    void setUserStopTyping(MegaChatHandle uh);
    void setClosed();
    void setChatMode(bool mode);
    void setArchived(bool archived);

private:
    int mChanged;

    MegaChatHandle mChatid;
    mega::privilege_t priv;
    mega::userpriv_vector mPeers;
    bool group;
    bool mPublicChat;
    karere::Id mAuthToken;
    bool active;
    bool mArchived;
    bool mHasCustomTitle;
    int64_t mCreationTs;
    bool mMeeting = false;
    bool mWaitingRoom = false;
    bool mOpenInvite = false;
    bool mSpeakRequest = false;

    std::string mTitle;
    int unreadCount;
    unsigned int mNumPreviewers;
    MegaChatHandle mUh;
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
    virtual ~MegaChatRoomListPrivate();
    virtual MegaChatRoomList *copy() const;
    virtual const MegaChatRoom *get(unsigned int i) const;
    virtual unsigned int size() const;

    void addChatRoom(MegaChatRoom*);

private:
    MegaChatRoomListPrivate(const MegaChatRoomListPrivate *list);
    std::vector<MegaChatRoom*> mList;
};

class MegaChatScheduledFlagsPrivate: public MegaChatScheduledFlags
{
public:
    MegaChatScheduledFlagsPrivate();
    MegaChatScheduledFlagsPrivate(const unsigned long numericValue);
    MegaChatScheduledFlagsPrivate(const MegaChatScheduledFlagsPrivate *flags);
    MegaChatScheduledFlagsPrivate(const karere::KarereScheduledFlags* flags);
    ~MegaChatScheduledFlagsPrivate() override = default;
    MegaChatScheduledFlagsPrivate(const MegaChatScheduledFlagsPrivate&) = delete;
    MegaChatScheduledFlagsPrivate(const MegaChatScheduledFlagsPrivate&&) = delete;
    MegaChatScheduledFlagsPrivate& operator=(const MegaChatScheduledFlagsPrivate&) = delete;
    MegaChatScheduledFlagsPrivate& operator=(const MegaChatScheduledFlagsPrivate&&) = delete;

    void reset() override;
    void setSendEmails(bool enabled) override;

    unsigned long getNumericValue() const;
    bool sendEmails() const override;
    bool isEmpty() const override;

    MegaChatScheduledFlagsPrivate* copy() const override { return new MegaChatScheduledFlagsPrivate(this); }
    std::unique_ptr<karere::KarereScheduledFlags> getKarereScheduledFlags() const;

private:
    std::unique_ptr<karere::KarereScheduledFlags> mKScheduledFlags;
};

class MegaChatScheduledRulesPrivate : public MegaChatScheduledRules
{
public:
    MegaChatScheduledRulesPrivate(const int freq,
                                  const int interval = INTERVAL_INVALID,
                                  const MegaChatTimeStamp until = MEGACHAT_INVALID_TIMESTAMP,
                                  const mega::MegaIntegerList* byWeekDay = nullptr,
                                  const mega::MegaIntegerList* byMonthDay = nullptr,
                                  const mega::MegaIntegerMap* byMonthWeekDay = nullptr);

    MegaChatScheduledRulesPrivate(const MegaChatScheduledRulesPrivate *rules);
    MegaChatScheduledRulesPrivate(const karere::KarereScheduledRules* rules);
    ~MegaChatScheduledRulesPrivate() override = default;
    MegaChatScheduledRulesPrivate(const MegaChatScheduledRulesPrivate&) = delete;
    MegaChatScheduledRulesPrivate(const MegaChatScheduledRulesPrivate&&) = delete;
    MegaChatScheduledRulesPrivate& operator=(const MegaChatScheduledRulesPrivate&) = delete;
    MegaChatScheduledRulesPrivate& operator=(const MegaChatScheduledRulesPrivate&&) = delete;

    void setFreq(int freq) override;
    void setInterval(int interval) override;
    void setUntil(MegaChatTimeStamp until) override;
    void setByWeekDay(const mega::MegaIntegerList* byWeekDay) override;
    void setByMonthDay(const mega::MegaIntegerList* byMonthDay)  override;
    void setByMonthWeekDay(const mega::MegaIntegerMap* byMonthWeekDay) override;

    int freq() const override;
    int interval() const override;
    MegaChatTimeStamp until() const override;
    const mega::MegaIntegerList* byWeekDay()  const override;
    const mega::MegaIntegerList* byMonthDay()  const  override;
    const mega::MegaIntegerMap* byMonthWeekDay() const override;

    MegaChatScheduledRulesPrivate* copy() const override { return new MegaChatScheduledRulesPrivate(this); }
    std::unique_ptr<karere::KarereScheduledRules> getKarereScheduledRules() const;
    static bool isValidFreq(const int freq)              { return (freq >= FREQ_DAILY && freq <= FREQ_MONTHLY); }
    static bool isValidInterval(const int interval)      { return interval > INTERVAL_INVALID; }
    static bool isValidUntil(const mega::m_time_t until) { return until > MEGACHAT_INVALID_TIMESTAMP; }

private:
    std::unique_ptr<karere::KarereScheduledRules> mKScheduledRules;

    // temp memory must be held somewhere since there is a data transformation and ownership is not returned in the getters
    // (to be removed once MegaChatAPI homogenizes with MegaAPI)
    mutable std::unique_ptr<mega::MegaIntegerList> mTransformedByWeekDay;
    mutable std::unique_ptr<mega::MegaIntegerList> mTransformedByMonthDay;
    mutable std::unique_ptr<mega::MegaIntegerMap> mTransformedByMonthWeekDay;
};

class MegaChatScheduledMeetingPrivate: public MegaChatScheduledMeeting
{
public:
    using megachat_sched_bs_t = karere::KarereScheduledMeeting::sched_bs_t;

    MegaChatScheduledMeetingPrivate(const MegaChatHandle chatid,
                                    const char* timezone,
                                    const MegaChatTimeStamp startDateTime,
                                    const MegaChatTimeStamp endDateTime,
                                    const char* title,
                                    const char* description,
                                    const MegaChatHandle schedId = MEGACHAT_INVALID_HANDLE,
                                    const MegaChatHandle parentSchedId = MEGACHAT_INVALID_HANDLE,
                                    const MegaChatHandle organizerUserId = MEGACHAT_INVALID_HANDLE,
                                    const int cancelled = -1,
                                    const char* attributes = nullptr,
                                    const MegaChatTimeStamp overrides = MEGACHAT_INVALID_TIMESTAMP,
                                    const MegaChatScheduledFlags* flags = nullptr,
                                    const MegaChatScheduledRules* rules = nullptr);

    MegaChatScheduledMeetingPrivate(const MegaChatScheduledMeetingPrivate *scheduledMeeting);
    MegaChatScheduledMeetingPrivate(const karere::KarereScheduledMeeting* scheduledMeeting);
    ~MegaChatScheduledMeetingPrivate() override = default;
    MegaChatScheduledMeetingPrivate(const MegaChatScheduledMeetingPrivate&) = delete;
    MegaChatScheduledMeetingPrivate(const MegaChatScheduledMeetingPrivate&&) = delete;
    MegaChatScheduledMeetingPrivate& operator=(const MegaChatScheduledMeetingPrivate&) = delete;
    MegaChatScheduledMeetingPrivate& operator=(const MegaChatScheduledMeetingPrivate&&) = delete;
    MegaChatScheduledMeetingPrivate(const mega::MegaScheduledMeeting& msm)
        : mKScheduledMeeting(std::make_unique<karere::KarereScheduledMeeting>(msm.chatid()
                                                                              , msm.organizerUserid()
                                                                              , msm.timezone() ? msm.timezone() : std::string()
                                                                              , msm.startDateTime()
                                                                              , msm.endDateTime()
                                                                              , msm.title() ? msm.title() : std::string()
                                                                              , msm.description() ? msm.description() : std::string()
                                                                              , msm.schedId()
                                                                              , msm.parentSchedId()
                                                                              , msm.cancelled()
                                                                              , msm.attributes() ? msm.attributes() : std::string()
                                                                              , msm.overrides()
                                                                              , msm.flags() ? std::make_unique<karere::KarereScheduledFlags>(msm.flags()).get() : nullptr
                                                                              , msm.rules() ? std::make_unique<karere::KarereScheduledRules>(msm.rules()).get() : nullptr))
    {}

    void setChanged(const unsigned long val) { mChanged = megachat_sched_bs_t(val); }
    megachat_sched_bs_t getChanges() const   { return mChanged; }

    megachat_sched_bs_t getChanged() const   { return mChanged; }
    MegaChatHandle chatId() const override;
    MegaChatHandle schedId() const override;
    MegaChatHandle parentSchedId() const override;
    MegaChatHandle organizerUserId() const override;
    const char* timezone() const override;
    MegaChatTimeStamp startDateTime() const override;
    MegaChatTimeStamp endDateTime() const override;
    const char* title() const override;
    const char* description() const override;
    const char* attributes() const override;
    MegaChatTimeStamp overrides() const override;
    int cancelled() const override;
    MegaChatScheduledFlags* flags() const override;
    MegaChatScheduledRules* rules() const override;
    bool hasChanged(size_t changeType) const override;
    bool isNew() const override;
    bool isDeleted() const override;

    MegaChatScheduledMeetingPrivate* copy() const override { return new MegaChatScheduledMeetingPrivate(this); }

private:
    std::unique_ptr<karere::KarereScheduledMeeting> mKScheduledMeeting;

    // changed bitmap
    megachat_sched_bs_t mChanged;

    // temp memory must be held somewhere since there is a data transformation and ownership is not returned in the getters
    mutable std::unique_ptr<MegaChatScheduledFlags> mTransformedMCSFlags;
    mutable std::unique_ptr<MegaChatScheduledRules> mTransformedMCSRules;
};


class MegaChatScheduledMeetingOccurrPrivate: public MegaChatScheduledMeetingOccurr
{
public:
    MegaChatScheduledMeetingOccurrPrivate(const MegaChatScheduledMeetingOccurrPrivate *scheduledMeeting);
    MegaChatScheduledMeetingOccurrPrivate(const karere::KarereScheduledMeetingOccurr* scheduledMeeting);
    virtual ~MegaChatScheduledMeetingOccurrPrivate();
    MegaChatScheduledMeetingOccurrPrivate* copy() const override;
    MegaChatHandle schedId() const override;
    MegaChatHandle parentSchedId() const override;
    const char* timezone() const override;
    MegaChatTimeStamp startDateTime() const override;
    MegaChatTimeStamp endDateTime() const override;
    MegaChatTimeStamp overrides() const override;
    int cancelled() const override;

private:
    // scheduled meeting handle
    MegaChatHandle mSchedId;

    // parent scheduled meeting handle
    MegaChatHandle mParentSchedId;

    // start dateTime of the original meeting series event to be replaced (unix timestamp)
    MegaChatTimeStamp mOverrides;

    // timeZone
    std::string mTimezone;

    // start dateTime (unix timestamp)
    MegaChatTimeStamp mStartDateTime;

    // end dateTime (unix timestamp)
    MegaChatTimeStamp mEndDateTime;

    // cancelled flag
    int mCancelled;
};

class MegaChatScheduledMeetingListPrivate: public MegaChatScheduledMeetingList
{
public:
    MegaChatScheduledMeetingListPrivate();
    MegaChatScheduledMeetingListPrivate(const MegaChatScheduledMeetingListPrivate &l);
    ~MegaChatScheduledMeetingListPrivate();

    MegaChatScheduledMeetingListPrivate *copy() const override;

    // getters
    unsigned long size() const override;
    const MegaChatScheduledMeeting *at(unsigned long i) const override;

    // setters
    void insert(MegaChatScheduledMeeting *sm) override;
    void clear() override;

private:
    std::vector<std::unique_ptr<MegaChatScheduledMeeting>> mList;
};

class MegaChatScheduledMeetingOccurrListPrivate: public MegaChatScheduledMeetingOccurrList
{
public:
    MegaChatScheduledMeetingOccurrListPrivate();
    MegaChatScheduledMeetingOccurrListPrivate(const MegaChatScheduledMeetingOccurrListPrivate &l);
    ~MegaChatScheduledMeetingOccurrListPrivate();

    MegaChatScheduledMeetingOccurrListPrivate *copy() const override;

    // getters
    unsigned long size() const override;
    const MegaChatScheduledMeetingOccurr *at(unsigned long i) const override;

    // setters
    void insert(MegaChatScheduledMeetingOccurr *sm) override;
    void clear() override;

private:
    std::vector<std::unique_ptr<MegaChatScheduledMeetingOccurr>> mList;
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
    MegaChatMessage *copy() const override;

    // MegaChatMessage interface
    int getStatus() const override;
    MegaChatHandle getMsgId() const override;
    MegaChatHandle getTempId() const override;
    int getMsgIndex() const override;
    MegaChatHandle getUserHandle() const override;
    int getType() const override;
    bool hasConfirmedReactions() const override;
    int64_t getTimestamp() const override;
    const char *getContent() const override;
    bool isEdited() const override;
    bool isDeleted() const override;
    bool isEditable() const override;
    bool isDeletable() const override;
    bool isManagementMessage() const override;
    MegaChatHandle getHandleOfAction() const override;
    int getPrivilege() const override;
    int getCode() const override;
    MegaChatHandle getRowId() const override;
    unsigned int getUsersCount() const override;
    MegaChatHandle getUserHandle(unsigned int index) const override;
    const char *getUserName(unsigned int index) const override;
    const char *getUserEmail(unsigned int index) const override;
    mega::MegaNodeList *getMegaNodeList() const override;
    const MegaChatContainsMeta *getContainsMeta() const override;
    mega::MegaHandleList *getMegaHandleList() const override;
    int getDuration() const override;
    unsigned getRetentionTime() const override;
    int getTermCode() const override;
    bool hasSchedMeetingChanged(unsigned int change) const override;

    const mega::MegaStringList* getStringList() const override;
    const mega::MegaStringListMap* getStringListMap() const override;
    const mega::MegaStringList* getScheduledMeetingChange(const unsigned int changeType) const override;
    const MegaChatScheduledRules* getScheduledMeetingRules() const override;

    int getChanges() const override;
    bool hasChanged(int changeType) const override;

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
    int mStatus;
    MegaChatHandle msgId;   // definitive unique ID given by server
    MegaChatHandle mTempId;  // used until it's given a definitive ID by server
    MegaChatHandle rowId;   // used to identify messages in the manual-sending queue
    MegaChatHandle uh;
    MegaChatHandle hAction;// certain messages need additional handle: such us priv changes, revoke attachment
    int mIndex;              // position within the history buffer
    int64_t ts;
    const char *mMsg;
    bool edited;
    bool deleted;
    int priv;               // certain messages need additional info, like priv changes
    int mCode;               // generic field for additional information (ie. the reason of manual sending)
    bool mHasReactions;
    std::vector<MegaChatAttachedUser> *megaChatUsers = NULL;
    mega::MegaNodeList *megaNodeList = NULL;
    mega::MegaHandleList *megaHandleList = NULL;
    const MegaChatContainsMeta *mContainsMeta = NULL;
    std::unique_ptr<::mega::MegaStringList> mStringList;
    std::unique_ptr<::mega::MegaStringListMap> mStringListMap;
    std::unique_ptr<MegaChatScheduledRules> mScheduledRules;
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
        public karere::IApp::IChatListHandler
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, mega::MegaApi *megaApi);
    virtual ~MegaChatApiImpl();

    using SdkMutexGuard = std::unique_lock<std::recursive_mutex>;   // (equivalent to typedef)
    mutable std::recursive_mutex sdkMutex;
    std::recursive_mutex videoMutex;
    mega::Waiter *waiter;
private:
    MegaChatApi *mChatApi;
    mega::MegaApi *mMegaApi;
    WebsocketsIO *mWebsocketsIO;
    karere::Client *mClient;
    bool mTerminating;

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
    std::map<MegaChatHandle, MegaChatVideoListener_set> mLocalCameraVideoListeners;
    std::map<MegaChatHandle, MegaChatVideoListener_set> mLocalScreenVideoListeners;

    mega::MegaStringList *getChatInDevices(const std::set<std::string> &devices);
    void cleanCalls();
    std::unique_ptr<MegaChatCallHandler> mCallHandler;
#endif

    std::unique_ptr<MegaChatScheduledMeetingHandler> mScheduledMeetingHandler;
    std::set<MegaChatScheduledMeetingListener *> mSchedMeetingListeners;

    void cleanChatHandlers();

    static int convertInitState(int state);
    static int convertDbError(int errCode);
    bool isChatroomFromType(const karere::ChatRoom& chat, int type) const;

    int performRequest_retryPendingConnections(MegaChatRequestPrivate* request);
    int performRequest_signalActivity(MegaChatRequestPrivate* request);
    int performRequest_setPresenceAutoaway(MegaChatRequestPrivate* request);
    int performRequest_setPresencePersist(MegaChatRequestPrivate* request);
    int performRequest_setLastGreenVisible(MegaChatRequestPrivate* request);
    int performRequest_lastGreen(MegaChatRequestPrivate* request);
    int performRequest_logout(MegaChatRequestPrivate* request);
    int performRequest_delete(MegaChatRequestPrivate* request);
    int performRequest_setOnlineStatus(MegaChatRequestPrivate* request);
    int performRequest_createChatroom(MegaChatRequestPrivate* request);
    int performRequest_setChatroomOptions(MegaChatRequestPrivate* request);
    int performRequest_inviteToChatroom(MegaChatRequestPrivate* request);
    int performRequest_autojoinPublicChat(MegaChatRequestPrivate* request);
    int performRequest_updatePeerPermissions(MegaChatRequestPrivate* request);
    int performRequest_removeFromChatroom(MegaChatRequestPrivate* request);
    int performRequest_truncateHistory(MegaChatRequestPrivate* request);
    int performRequest_editChatroomName(MegaChatRequestPrivate* request);
    int performRequest_loadPreview(MegaChatRequestPrivate* request);
    int performRequest_setPrivateMode(MegaChatRequestPrivate* request);
    int performRequest_chatLinkHandle(MegaChatRequestPrivate* request);
    int performRequest_getFirstname(MegaChatRequestPrivate* request);
    int performRequest_getLastname(MegaChatRequestPrivate* request);
    int performRequest_getEmail(MegaChatRequestPrivate* request);
    int performRequest_attachNodeMessage(MegaChatRequestPrivate* request);
    int performRequest_revokeNodeMessage(MegaChatRequestPrivate* request);
    int performRequest_setBackgroundStatus(MegaChatRequestPrivate* request);
    int performRequest_pushReceived(MegaChatRequestPrivate* request);
    int performRequest_archiveChat(MegaChatRequestPrivate* request);
    int performRequest_loadUserAttributes(MegaChatRequestPrivate* request);
    int performRequest_setChatRetentionTime(MegaChatRequestPrivate* request);
    int performRequest_manageReaction(MegaChatRequestPrivate* request);
    int performRequest_importMessages(MegaChatRequestPrivate* request);
    int performRequest_sendTypingNotification(MegaChatRequestPrivate* request);
#ifndef KARERE_DISABLE_WEBRTC
    int performRequest_startChatCall(MegaChatRequestPrivate* request);
    int performRequest_answerChatCall(MegaChatRequestPrivate* request);
    int performRequest_hangChatCall(MegaChatRequestPrivate* request);
    int performRequest_setAudioVideoEnable(MegaChatRequestPrivate* request);
    int performRequest_setCallOnHold(MegaChatRequestPrivate* request);
    int performRequest_setVideoCapturerInDevice(MegaChatRequestPrivate* request);
    int performRequest_enableAudioLevelMonitor(MegaChatRequestPrivate* request);
    int performRequest_addDelspeakRequest(MegaChatRequestPrivate* request);
    int performRequest_addRevokeSpeakePermission(MegaChatRequestPrivate* request);
    int performRequest_hiResVideo(MegaChatRequestPrivate* request);
    int performRequest_lowResVideo(MegaChatRequestPrivate* request);
    int performRequest_openCloseVideoDevice(MegaChatRequestPrivate* request);
    int performRequest_raiseHandToSpeak(MegaChatRequestPrivate* request);
    int performRequest_requestHiResQuality(MegaChatRequestPrivate* request);
    int performRequest_pushOrAllowJoinCall(MegaChatRequestPrivate* request);
    int performRequest_kickUsersFromCall(MegaChatRequestPrivate* request);
    int performRequest_rejectCall(MegaChatRequestPrivate* request);
    int performRequest_sendRingIndividualInACall(MegaChatRequestPrivate* request);
    int performRequest_mutePeersInCall(MegaChatRequestPrivate* request);
    int performRequest_setLimitsInCall(MegaChatRequestPrivate* request);
#endif
    int performRequest_removeScheduledMeeting(MegaChatRequestPrivate* request);
    int performRequest_fetchScheduledMeetingOccurrences(MegaChatRequestPrivate* request);
    int performRequest_updateScheduledMeetingOccurrence(MegaChatRequestPrivate* request);
    int performRequest_updateScheduledMeeting(MegaChatRequestPrivate* request);

public:
    static void megaApiPostMessage(megaMessage *msg, void* ctx);
    void postMessage(megaMessage *msg);

    void sendPendingRequests();
    void sendPendingEvents();

    static int getInternalMaxLogLevel();
    static bool setInternalMaxLogLevel(const unsigned int logLevel);
    static void setLogLevel(int logLevel);
    static void setLoggerClass(MegaChatLogger *megaLogger);
    static void setLogWithColors(bool useColors);
    static void setLogToConsole(bool enable);
    void setLoggingName(const char* loggingName);

    /**
     * @brief Aux method to get the logging name from mClient if it is defined.
     *
     * @note The return value is guaranteed to be not nullptr. If not defined "" is returned.
     */
    const char* getLoggingName() const;

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
    int getCurrentInputVideoTracksLimit() const;
    bool setCurrentInputVideoTracksLimit(const int numInputVideoTracks);
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
    void setPublicKeyPinning(bool enable);
#ifndef KARERE_DISABLE_WEBRTC
    void addChatCallListener(MegaChatCallListener *listener);
    void addSchedMeetingListener(MegaChatScheduledMeetingListener* listener);
    void removeChatCallListener(MegaChatCallListener *listener);
    void removeSchedMeetingListener(MegaChatScheduledMeetingListener* listener);
    void addChatVideoListener(MegaChatHandle chatid, MegaChatHandle clientId, rtcModule::VideoResolution videoResolution, const int capturerType, MegaChatVideoListener *listener);
    void removeChatVideoListener(MegaChatHandle chatid, MegaChatHandle clientId, rtcModule::VideoResolution videoResolution, const int capturerType, MegaChatVideoListener *listener);
    void setSFUid(int sfuid);
#endif

    // MegaChatRequestListener callbacks
    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaChatError *e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaChatError *e);

#ifndef KARERE_DISABLE_WEBRTC
    // MegaChatScheduledMeetListener callbacks
    void fireOnChatSchedMeetingUpdate(MegaChatScheduledMeetingPrivate* sm);
    void fireOnSchedMeetingOccurrencesChange(const karere::Id& id, bool append);

    // MegaChatCallListener callbacks
    void fireOnChatCallUpdate(MegaChatCallPrivate *call);
    void fireOnChatSessionUpdate(MegaChatHandle chatid, MegaChatHandle callid, MegaChatSessionPrivate *session);

    // MegaChatVideoListener callbacks
    void fireOnChatVideoData(MegaChatHandle chatid, uint32_t clientId, int width, int height, int sourceType, char*buffer, rtcModule::VideoResolution videoResolution);
#endif

    // MegaChatListener callbacks (specific ones)
    void fireOnChatListItemUpdate(MegaChatListItem *item);
    void fireOnChatInitStateUpdate(int newState);
    void fireOnChatOnlineStatusUpdate(MegaChatHandle userhandle, int status, bool inProgress);
    void fireOnChatPresenceConfigUpdate(MegaChatPresenceConfig *config);
    void fireOnChatPresenceLastGreenUpdated(MegaChatHandle userhandle, int lastGreen);
    void fireOnChatConnectionStateUpdate(MegaChatHandle chatid, int newState);
    void fireOnDbError(int error, const char* msg);

    // MegaChatNotificationListener callbacks
    void fireOnChatNotification(MegaChatHandle chatid, MegaChatMessage *msg);

    // ============= API requests ================

    // General chat methods
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
    const char* getUserAliasFromCache(MegaChatHandle userhandle);
    ::mega::MegaStringMap *getUserAliasesFromCache();
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
    MegaChatRoomList* getChatRoomsByType(int type);
    MegaChatRoom* getChatRoom(MegaChatHandle chatid);
    MegaChatRoom *getChatRoomByUser(MegaChatHandle userhandle);
    MegaChatListItemList* getChatListItems(const int mask, const int filter) const;
    MegaChatListItemList *getChatListItems() const;
    MegaChatListItemList* getChatListItemsByType(int type);
    MegaChatListItemList *getChatListItemsByPeers(MegaChatPeerList *peers);
    MegaChatListItem *getChatListItem(MegaChatHandle chatid);
    int getUnreadChats();
    MegaChatListItemList *getActiveChatListItems();
    MegaChatListItemList *getInactiveChatListItems();
    MegaChatListItemList *getArchivedChatListItems();
    MegaChatListItemList *getUnreadChatListItems();
    MegaChatHandle getChatHandleByUser(MegaChatHandle userhandle);

    // Chatrooms management
    void createChatroomAndSchedMeeting(MegaChatPeerList* peerList, bool isMeeting, bool publicChat, const char* title, bool speakRequest, bool waitingRoom, bool openInvite,
                                                          const char* timezone, MegaChatTimeStamp startDate, MegaChatTimeStamp endDate, const char* description,
                                                          const MegaChatScheduledFlags* flags, const MegaChatScheduledRules* rules,
                                                          const char* attributes, MegaChatRequestListener* listener = nullptr);

    // updates a scheduled meeting
    void updateScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId, const char* timezone, MegaChatTimeStamp startDate, MegaChatTimeStamp endDate,
                                             const char* title, const char* description, bool cancelled, const MegaChatScheduledFlags* flags, const MegaChatScheduledRules* rules,
                                             const bool updateChatTitle, MegaChatRequestListener* listener = nullptr);

    // updates a scheduled meeting ocurrence
    void updateScheduledMeetingOccurrence(MegaChatHandle chatid, MegaChatHandle schedId, MegaChatTimeStamp overrides, MegaChatTimeStamp newStartDate,
                                                       MegaChatTimeStamp newEndDate, bool cancelled, MegaChatRequestListener* listener = nullptr);

    // removes a scheduled meeting
    void removeScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId, MegaChatRequestListener* listener = nullptr);

    // get all scheduled meetings given a chatid
    MegaChatScheduledMeetingList* getScheduledMeetingsByChat(MegaChatHandle chatid);

    // return a specific scheduled meeting given a chatid and a scheduled meeting id
    MegaChatScheduledMeeting* getScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId);

    // return a list of scheduled meeting for all chatrooms
    MegaChatScheduledMeetingList* getAllScheduledMeetings();

    // get all future scheduled meetings occurrences given a chatid, if there are not enough occurrences, MEGAChat will fetch automatically from API
    void fetchScheduledMeetingOccurrencesByChat(MegaChatHandle chatid, MegaChatTimeStamp since, MegaChatTimeStamp until, MegaChatRequestListener* listener);

    void setChatOption(MegaChatHandle chatid, int option, bool enabled, MegaChatRequestListener* listener = NULL);
    void createChat(bool group, MegaChatPeerList *peerList, MegaChatRequestListener *listener = NULL);
    void createChat(bool group, MegaChatPeerList* peerList, const char* title, bool speakRequest, bool waitingRoom, bool openInvite, MegaChatRequestListener* listener = NULL);
    void createPublicChat(MegaChatPeerList *peerList, bool meeting, const char *title = NULL, bool speakRequest = false, bool waitingRoom = false, bool openInvite = false,  MegaChatRequestListener *listener = NULL);
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
    int createChatOptionsBitMask(bool speakRequest, bool waitingRoom, bool openInvite);
    bool isValidChatOptionsBitMask(int chatOptionsBitMask);
    static bool hasChatOptionEnabled(int option, int chatOptionsBitMask);

#ifndef KARERE_DISABLE_WEBRTC

    // Audio/Video devices
    mega::MegaStringList* getChatScreenDevices();
    mega::MegaStringList *getChatVideoInDevices();
    void setVideoCapturerInDevice(const char* device, const int type, MegaChatRequestListener* listener = NULL);
    char *getCameraDeviceIdSelected();
    long getScreenDeviceIdSelected() const;
    char* getVideoDeviceNameById(const std::string& id) const;
    char* getScreenDeviceNameById(const long int id) const;

    // Calls
    void startChatCall(MegaChatHandle chatid, bool enableVideo = true,  bool enableAudio = true, bool notRinging = false, MegaChatRequestListener *listener = NULL);
    void ringIndividualInACall(const MegaChatHandle chatId, const MegaChatHandle userId, const int ringTimeout, MegaChatRequestListener* listener = nullptr);
    void answerChatCall(MegaChatHandle chatid, bool enableVideo = true,  bool enableAudio = true, MegaChatRequestListener *listener = NULL);
    void hangChatCall(MegaChatHandle callid, MegaChatRequestListener *listener = NULL);
    void endChatCall(MegaChatHandle callid, MegaChatRequestListener *listener = NULL);
    void setAudioEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void setVideoEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener *listener = NULL);
    void setScreenShareEnable(MegaChatHandle chatid, bool enable, MegaChatRequestListener* listener = nullptr);
    void openCloseCapurerDevice(const int deviceType, const bool open, MegaChatRequestListener *listener = NULL);
    void requestHiResQuality(MegaChatHandle chatid, MegaChatHandle clientId, int quality, MegaChatRequestListener *listener = NULL);
    void rejectCall(const MegaChatHandle callId, MegaChatRequestListener* listener = nullptr);
    void setCallOnHold(MegaChatHandle chatid, bool setOnHold, MegaChatRequestListener *listener = NULL);
    void pushUsersIntoWaitingRoom(MegaChatHandle chatid, mega::MegaHandleList* users, const bool all, MegaChatRequestListener* listener = nullptr);
    void allowUsersJoinCall(MegaChatHandle chatid, const mega::MegaHandleList* users, const bool all, MegaChatRequestListener* listener = nullptr);
    void kickUsersFromCall(MegaChatHandle chatid, mega::MegaHandleList* users, MegaChatRequestListener* listener = nullptr);
    void setLimitsInCall(const MegaChatHandle chatid, const unsigned long callDur, const unsigned long numUsers, const unsigned long numClientsPerUser, const unsigned long numClients, const unsigned long divider, MegaChatRequestListener* listener = nullptr);
    void mutePeers(const MegaChatHandle chatid, const MegaChatHandle clientId, MegaChatRequestListener* listener = nullptr);
    MegaChatCall *getChatCall(MegaChatHandle chatId);
    bool setIgnoredCall(MegaChatHandle chatId);
    MegaChatCall *getChatCallByCallId(MegaChatHandle callId);
    int getNumCalls();
    mega::MegaHandleList *getChatCalls(int callState = -1);
    mega::MegaHandleList *getChatCallsIds();
    bool hasCallInChatRoom(MegaChatHandle chatid);
    int getMaxCallParticipants();
    int getMaxSupportedVideoCallParticipants();
    bool isValidSimVideoTracks(const unsigned int maxSimVideoTracks) const;
    bool isAudioLevelMonitorEnabled(MegaChatHandle chatid);
    void enableAudioLevelMonitor(bool enable, MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);
    void addRevokeSpeakPermission(MegaChatHandle chatid, MegaChatHandle userid, bool add, MegaChatRequestListener* listener = NULL);
    void enableSpeakRequestSupportForCalls(const bool enable);
    void addDelSpeakRequest(MegaChatHandle chatid, MegaChatHandle userid, bool add, MegaChatRequestListener* listener = NULL);
    void requestHiResVideo(MegaChatHandle chatid, MegaChatHandle clientId, int quality, MegaChatRequestListener *listener = NULL);
    void raiseHandToSpeak(MegaChatHandle chatid, bool add, MegaChatRequestListener* listener = nullptr);
    void stopHiResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);
    void requestLowResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);
    void stopLowResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);
    std::pair<int, rtcModule::ICall*> getCall(const MegaChatHandle chatid, const std::string& msg, const bool ownPrivMod, karere::MTristate waitingRoom = karere::MTristate());
#endif

//    MegaChatCallPrivate *getChatCallByPeer(const char* jid);


    // ============= karere API implementation ================

    // karere::IApp implementation
    //virtual ILoginDialog* createLoginDialog();
    virtual IApp::IChatHandler *createChatHandler(karere::ChatRoom &chat);
    IApp::IChatListHandler *chatListHandler() override;
    void onPresenceChanged(karere::Id userid, karere::Presence pres, bool inProgress) override;
    void onPresenceConfigChanged(const presenced::Config& state, bool pending) override;
    void onPresenceLastGreenUpdated(karere::Id userid, uint16_t lastGreen) override;
    void onInitStateChange(int newState) override;
    void onChatNotification(karere::Id chatid, const chatd::Message &msg, chatd::Message::Status status, chatd::Idx idx) override;
    void onDbError(int error, const std::string &msg) override;

    // rtcModule::IChatListHandler implementation
    IApp::IGroupChatListItem *addGroupChatItem(karere::GroupChatRoom &chat) override;
    void removeGroupChatItem(IApp::IGroupChatListItem& item) override;
    IApp::IPeerChatListItem *addPeerChatItem(karere::PeerChatRoom& chat) override;
    void removePeerChatItem(IApp::IPeerChatListItem& item) override;
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

    MegaChatContainsMeta *copy() const override;

    int getType() const override;
    const char *getTextMessage() const override;
    const MegaChatRichPreview *getRichPreview() const override;
    const MegaChatGeolocation *getGeolocation() const override;
    const MegaChatGiphy *getGiphy() const override;

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

#ifdef _WIN32
#pragma warning(pop) // C2450
#endif

}

#endif // MEGACHATAPI_IMPL_H
