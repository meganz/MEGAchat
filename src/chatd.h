#ifndef __CHATD_H__
#define __CHATD_H__

#include <libws.h>
#include <stdint.h>
#include <string>
#include <buffer.h>
#include <map>
#include <set>
#include <list>
#include <deque>
#include <base/promise.h>
#include <base/timers.hpp>
#include "chatdMsg.h"

#define CHATD_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_chatd, fmtString, ##__VA_ARGS__)

#define CHATD_MAX_EDIT_AGE 3600
namespace chatd
{

/// This type is used for ordered indexes in the message buffer
typedef int32_t Idx;

/// We want to fit in the positive range of a signed int64_t, for compatibility with sqlite which has no unsigned types
/// Don't use an enum as its implicit type may screw up our value
#define CHATD_IDX_RANGE_MIDDLE 0
#define CHATD_IDX_INVALID 0x7fffffff

class Chat;
class ICrypto;

class Url
{
protected:
    uint16_t getPortFromProtocol() const;
public:
    std::string protocol;
    std::string host;
    uint16_t port;
    std::string path;
    bool isSecure;
    Url(const std::string& url) { parse(url); }
    Url(): isSecure(false) {}
    void parse(const std::string& url);
    bool isValid() const { return !host.empty(); }
};


class DbInterface;
class Listener
{
public:
/** @brief
 *  This is the first call chatd makes to the Listener, passing it necessary objects and
 * retrieving info about the local history database
 * @param chat - the Chat object that can be used to access the message buffer etc
 * @param dbIntf[out] reference to the internal pointer to the database abstraction
 * layer interface. Set this pointer to a newly created instance of a db abstraction
 * instance. @note The dbIntf creation is handled by karere, and in the app interface,
 * this callback should not change this pointer (which is set to NULL)
 */
    virtual void init(Chat& chat, DbInterface*& dbIntf) = 0;
/// Called when that chatroom instance is being destroyed (e.g. on application close)
    virtual void onDestroy(){}
/// A new message was received. This can be just sent by a peer, or retrieved from the server.
/// @param idx The index of the message in the buffer @param status - The 'seen' status of the
/// message. Normally it should be 'not seen', until we call setMessageSeen() on it
    virtual void onRecvNewMessage(Idx idx, Message& msg, Message::Status status){}
/// A history message has been received.
/// @param isFromDb The message can be received from the server, or from the app's local
/// history db, via fetchDbHistory() - this parameter specifies the source
    virtual void onRecvHistoryMessage(Idx idx, Message& msg, Message::Status status, bool isFromDb){}
///The retrieval of the requested history batch, via getHistory(), was completed
/// @param isFromDb Whether the history was retrieved from localDb, via fetchDbHistory() or from
/// the server
    virtual void onHistoryDone(bool isFromDb) {}
    virtual void onUnsentMsgLoaded(Message& msg) {}
    virtual void onUnsentEditLoaded(Message& msg, bool oriMsgIsSending) {}
/// A message sent by us was acknoledged by the server, assigning it a MSGID.
/// At this stage, the message state is "received-by-server", and it is in the history
/// buffer when this callback is called.
/// @param msgxid - The request-response match id that we generated for the sent message.
/// Normally the application doesn't need to care about it
/// @param msg - The message object - \c id() returns a real msgid, and \c isSending() is \c false
/// @param idx - The history buffer index at which the message was put
    virtual void onMessageConfirmed(karere::Id msgxid, const Message& msg, Idx idx){}
/// A message was rejected by the server for some reason. As the message is not yet
/// in the history buffer, its \c id() is a msgxid, and \c isSending() is true
    virtual void onMessageRejected(const Message& msg){}
/** A message was delivered, seen, etc. When the seen/received pointers are advanced,
 * this will be called for each message of the pointer-advanced range, so the application
 * doesn't need to iterate over ranges by itself */
    virtual void onMessageStatusChange(Idx idx, Message::Status newStatus, const Message& msg){}
/** Called when a message edit is received, i.e. MSGUPD is received. The message is already
 * updated in the history buffer and in the db, and the GUI should also update it.
 * \attention If the edited message is not in memory, it is still updated in the database,
 * but this callback will not be called.
 * @param idx - the index of the edited message */
    virtual void onMessageEdited(const Message& msg, Idx idx){}
    virtual void onEditRejected(const Message& msg, uint8_t opcode){}
/// The chatroom connection (to the chatd server shard) state state has changed.
    virtual void onOnlineStateChange(ChatState state){}
/// A user has joined or left the room, or their privilege has changed. If user has left
/// the room priv is PRIV_NOTPRESENT
    virtual void onUserJoin(karere::Id userid, Priv privilege){}
    virtual void onUserLeave(karere::Id userid){}
///Unread message count has changed
    virtual void onUnreadChanged() {}
    //Ownership of \c msg is passed to application.
    virtual void onManualSendRequired(Message* msg, uint64_t id, int reason) {}
    virtual void onHistoryTruncated(const Message& msg, Idx idx) {}
    virtual void onMsgOrderVerificationFail(const Message& msg, Idx idx, const std::string& errmsg)
    {
        CHATD_LOG_ERROR("msgOrderFail[msgid %s]: %s", msg.id().toString().c_str(), errmsg.c_str());
    }
};

class Client;

class Connection
{
public:
    enum State { kStateDisconnected, kStateConnecting, kStateConnected };
protected:
    Client& mClient;
    int mShardNo;
    std::set<karere::Id> mChatIds;
    ws_t mWebSocket = nullptr;
    State mState = kStateDisconnected;
    Url mUrl;
    megaHandle mInactivityTimer = 0;
    int mInactivityBeats = 0;
    bool mTerminating = false;
    promise::Promise<void> mConnectPromise;
    Connection(Client& client, int shardNo): mClient(client), mShardNo(shardNo){}
    State getState() { return mState; }
    bool isOnline() const
    {
        return (mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
    }
    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
        size_t reason_len, void *arg);
    void onSocketClose(int ercode, int errtype, const std::string& reason);
    promise::Promise<void> reconnect();
    void disconnect();
    void enableInactivityTimer();
    void disableInactivityTimer();
    void reset();
// Destroys the buffer content
    bool sendBuf(Buffer&& buf);
    void rejoinExistingChats();
    void resendPending();
    void join(karere::Id chatid);
    void hist(karere::Id chatid, long count);
    void execCommand(const StaticBuffer& buf);
    friend class Client;
    friend class Chat;
public:
    ~Connection()
    {
        disableInactivityTimer();
        reset();
    }
};

enum HistFetchState
{
/** History is not being fetched, and we don't have any more history neither in db nor on server */
    kHistNoMore = 0,
/** History is not being fetched, and there is probably history to fetch available */
    kHistNotFetching = 1,
/** We are fetching old messages if this flag is set, i.e. ones added to the back of
 *  the history buffer. If this flag is no set, we are fetching new messages, i.e. ones
 *  appended to the front of the history buffer */
    kHistOldFlag = 2,
/** We are fetching from the server if flag is set, otherwise we are fetching from
 * local db */
    kHistFetchingFromServerFlag = 4,
    kHistFetchingOldFromServer = kHistFetchingFromServerFlag | kHistOldFlag,
    kHistFetchingNewFromServer = kHistFetchingFromServerFlag | 0,
/** We are currently fetching history from db - always old messages */
    kHistFetchingFromDb = 0 | kHistOldFlag,
    kHistDecryptingFlag = 8,
    kHistDecryptingOld = kHistDecryptingFlag | kHistOldFlag,
    kHistDecryptingNew = kHistDecryptingFlag | 0
};

/** Reason codes passed to Listener::onManualSendRequired() */
enum
{
    kManualSendUsersChanged = 1, ///< Group chat participants have changed
    kManualSendTooOld = 2 ///< Message is older than CHATD_MAX_AUTOSEND_AGE seconds
};

// message storage subsystem
// the message buffer can grow in two directions and is always contiguous, i.e. there are no "holes"
// there is no guarantee as to ordering
class Chat
{
public:
    struct SendingItem
    {
    protected:
        uint8_t mOpcode;
    public:
        uint64_t rowid;
 /** When sending a message, we attach the Message object here to avoid
  * double-converting it when queued as a raw command in Sending, and after
  * that (when server confirms) move it as a Message object to history buffer */
        Message* msg;
        std::unique_ptr<MsgCommand> msgCmd;
        std::unique_ptr<KeyCommand> keyCmd;
        karere::SetOfIds recipients;
        uint8_t opcode() const { return mOpcode; }
        void setOpcode(uint8_t op) { mOpcode = op; }
        SendingItem(uint8_t aOpcode, Message* aMsg, MsgCommand* aMsgCmd,
            KeyCommand* aKeyCmd, const karere::SetOfIds& aRcpts, uint64_t aRowid=0)
        : mOpcode(aOpcode), rowid(aRowid), msg(aMsg), msgCmd(aMsgCmd), keyCmd(aKeyCmd),
            recipients(aRcpts){}
        ~SendingItem(){ if (msg) delete msg; }
        bool isMessage() const { return ((mOpcode == OP_NEWMSG) || (mOpcode == OP_MSGUPD) || (mOpcode == OP_MSGUPDX)); }
        bool isEdit() const { return mOpcode == OP_MSGUPD || mOpcode == OP_MSGUPDX; }
        void setKeyId(KeyId keyid)
        {
            msg->keyid = keyid;
            if (msgCmd) msgCmd->setKeyId(keyid);
        }
    };
    typedef std::list<SendingItem> OutputQueue;
    struct ManualSendItem
    {
        Message* msg;
        uint64_t rowid;
        uint8_t opcode;
        uint8_t reason;
        ManualSendItem(Message* aMsg, uint64_t aRowid, uint8_t aOpcode, uint8_t aReason)
            :msg(aMsg), rowid(aRowid), opcode(aOpcode), reason(aReason){}
    };

protected:
    Connection& mConnection;
    Client& mClient;
    karere::Id mChatId;
    Idx mForwardStart;
    std::vector<std::unique_ptr<Message>> mForwardList;
    std::vector<std::unique_ptr<Message>> mBackwardList;
    OutputQueue mSending;
    OutputQueue::iterator mNextUnsent;
    bool mIsFirstJoin = true;
    std::map<karere::Id, Idx> mIdToIndexMap;
    karere::Id mLastReceivedId;
    Idx mLastReceivedIdx = CHATD_IDX_INVALID;
    karere::Id mLastSeenId;
    Idx mLastSeenIdx = CHATD_IDX_INVALID;
    bool mHasMoreHistoryInDb = false;
    Listener* mListener;
    ChatState mOnlineState = kChatStateOffline;
    karere::SetOfIds mUsers;
    karere::SetOfIds mUserDump; //< The initial dump of JOINs goes here, then after join is complete, mUsers is set to this in one step
    /// db-supplied initial range, that we use until we see the message with mOldestKnownMsgId
    /// Before that happens, missing messages are supposed to be in the database and
    /// incrementally fetched from there as needed. After we see the mOldestKnownMsgId,
    /// we disable this range by setting mOldestKnownMsgId to 0, and recalculate
    /// range() only from the buffer items
    karere::Id mOldestKnownMsgId;
    karere::Id mNewestKnownMsgId;
    unsigned mLastReqdHistCount = 0; ///< The amount of old/new history messages last requested either from server or from db
    unsigned mLastHistFetchCount = 0; ///< The number of history messages that have been fetched so far by the currently active or the last history fetch. It is reset upon new history fetch initiation
    unsigned mLastHistObtainCount = 0; ///< Similar to mLastHistFetchCount, but reflects the current number of message passed through the decrypt process, which may be less than mLastHistFetchCount at a given moment
    HistFetchState mHistFetchState = kHistNotFetching;
    DbInterface* mDbInterface = nullptr;
    ICrypto* mCrypto;
    /** If crypto can't decrypt immediately, we set this flag and only the plaintext
     * path of further messages to be sent is written to db, without calling encrypt().
     * Once encryption is finished, this flag is cleared, and all queued unencrypted
     * messages are passed to encryption, updating their command BLOB in the sending
     * db table. This, until another (or the same) encrypt call can't encrypt immediately,
     * in which case the flag is set again and the queue is blocked again */
    bool mEncryptionHalted = false;
    /** If an incoming new message can't be decrypted immediately, this is set to its
     * index in the hitory buffer, as it is already added there (in memory only!).
     * Further received new messages are only added to memory history buffer, and
     * not processed further until the delayed encryption of the message completes
     * or fails. After that, mDecryptNewHaltedAt is cleared (set to CHATD_IDX_INVALID),
     * the (supposedly, but not necessarily) decrypted message is added to db history,
     * SEEN and RECEIVED pointers are handled, and app callbacks are called with that
     * message. Then, processing of any newer messages, accumulated in the in-memory histiry
     * buffer is resumed, until decryption can't happen immediately again or all messages
     * are processed. The app may be terminated while a delayed decrypt is in progress
     * and there are newer undecrypted messages accumulated in the memory history buffer.
     * In that case, the app will resume its state from the point where the last message
     * decrypted (and saved to db), re-downloading all newer messages from server again.
     * Thus, not writing anything about queued undecrypted messages to the db allows
     * for a clean resume from the last known good point in message history. */
    Idx mDecryptNewHaltedAt = CHATD_IDX_INVALID;
    /** Similar to mDecryptNewhaltedAt, but for history messages, retrieved backwards
     * in regard to time and index in history buffer. Note that the two
     *  mDecryptXXXHaltedAt operate independently. I.e. decryption of old messages may
     * be blocked by delayed decryption of a message, while at the same time decryption
     * of new messages may work synchronously and not be delayed.
     */
    Idx mDecryptOldHaltedAt = CHATD_IDX_INVALID;
    std::map<karere::Id, Message*> mPendingEdits;
    std::map<BackRefId, Idx> mRefidToIdxMap;
    Chat(Connection& conn, karere::Id chatid, Listener* listener,
         const karere::SetOfIds& users, ICrypto* crypto);
    void push_forward(Message* msg) { mForwardList.emplace_back(msg); }
    void push_back(Message* msg) { mBackwardList.emplace_back(msg); }
    Message* first() const { return (!mBackwardList.empty()) ? mBackwardList.front().get() : mForwardList.back().get(); }
    Message* last() const { return (!mForwardList.empty())? mForwardList.front().get() : mBackwardList.back().get(); }
    void clear()
    {
        mBackwardList.clear();
        mForwardList.clear();
    }
    // msgid can be 0 in case of rejections
    Idx msgConfirm(karere::Id msgxid, karere::Id msgid);
    Idx msgIncoming(bool isNew, Message* msg, bool isLocal=false);
    bool msgIncomingAfterAdd(bool isNew, bool isLocal, Message& msg, Idx idx);
    void msgIncomingAfterDecrypt(bool isNew, bool isLocal, Message& msg, Idx idx);
    void onUserJoin(karere::Id userid, Priv priv);
    void onUserLeave(karere::Id userid);
    void onJoinComplete();
    void loadAndProcessUnsent();
    void initialFetchHistory(karere::Id serverNewest);
    void requestHistoryFromServer(int32_t count);
    void getHistoryFromDb(unsigned count);
    void onLastReceived(karere::Id msgid);
    void onLastSeen(karere::Id msgid);
    void handleLastReceivedSeen(karere::Id msgid);
    // As sending data over libws is destructive to the buffer, we have two versions
    // of sendCommand - the one with the rvalue reference is picked by the compiler
    // whenever the command object is a temporary, avoiding copying the buffer,
    // and the const reference one is picked when the Command object has to be preserved
    bool sendCommand(Command&& cmd);
    bool sendCommand(const Command& cmd);
    bool msgSend(const Message& message);
    void setOnlineState(ChatState state);
    SendingItem* postItemToSending(uint8_t opcode, Message* msg, MsgCommand* msgCmd,
        KeyCommand* keyCmd);
    bool flushOutputQueue(bool fromStart=false);
    karere::Id makeRandomId();
    void login();
    void join();
    void joinRangeHist();
    void onHistDone(); //called upont receipt of HISTDONE from server
    void onNewKeys(StaticBuffer&& keybuf);
    void logSend(const Command& cmd);
    friend class Connection;
    friend class Client;
public:
    unsigned initialHistoryFetchCount = 32; //< This is the amount of messages that will be requested from server _only_ in case local db is empty
    const karere::SetOfIds& users() const { return mUsers; }
    ~Chat();
    karere::Id chatId() const { return mChatId; }
    Client& client() const { return mClient; }
    Idx lownum() const { return mForwardStart - (Idx)mBackwardList.size(); }
    Idx highnum() const { return mForwardStart + (Idx)mForwardList.size()-1;}
    Idx size() const { return mForwardList.size() + mBackwardList.size(); }
    bool empty() const { return mForwardList.empty() && mBackwardList.empty();}
    Idx decryptedLownum() const
    {
        return (mDecryptOldHaltedAt == CHATD_IDX_INVALID)
            ? lownum() : mDecryptOldHaltedAt+1;
    }
    Idx decryptedHighnum() const
    {
        return(mDecryptNewHaltedAt == CHATD_IDX_INVALID)
            ? highnum() : mDecryptNewHaltedAt-1;
    }
    ChatState onlineState() const { return mOnlineState; }
    Message::Status getMsgStatus(const Message& msg, Idx idx);
    const std::map<karere::Id, Message*>& pendingEdits() const { return mPendingEdits; }
    Listener* listener() const { return mListener; }
    bool isFetchingHistory() const { return mHistFetchState > kHistNotFetching; }
    bool isFetchingFromDb() const { return mHistFetchState & kHistFetchingFromDb; }
    HistFetchState histFetchState() const { return mHistFetchState; }
    bool isFetchDecrypting() const { return ((mHistFetchState > kHistNotFetching) && (mHistFetchState < kHistDecryptingFlag)); }
    unsigned lastReqdHistCount() const { return mLastReqdHistCount; }
    unsigned lastHistObtainCount() const { return mLastHistObtainCount; }
    inline Message* findOrNull(Idx num) const
    {
        if (num < mForwardStart) //look in mBackwardList
        {
            Idx idx = mForwardStart - num - 1; //always >= 0
            if (static_cast<size_t>(idx) >= mBackwardList.size())
                return nullptr;
            return mBackwardList[idx].get();
        }
        else
        {
            Idx idx = num - mForwardStart;
            if (static_cast<size_t>(idx) >= mForwardList.size())
                return nullptr;
            return mForwardList[idx].get();
        }
    }
    Message& at(Idx num) const
    {
        Message* msg = findOrNull(num);
        if (!msg)
        {
            throw std::runtime_error("Chat::operator[idx]: idx = "+
                std::to_string(num)+" is outside of ["+std::to_string(lownum())+":"+
                std::to_string(highnum())+"] range");
        }
        return *msg;
    }

    Message& operator[](Idx num) const { return at(num); }
    bool hasNum(Idx num) const
    {
        if (num < mForwardStart)
            return (static_cast<size_t>(mForwardStart - num) <= mBackwardList.size());
        else
            return (num < mForwardStart + static_cast<int>(mForwardList.size()));
    }
    Idx msgIndexFromId(karere::Id id)
    {
        auto it = mIdToIndexMap.find(id);
        return (it == mIdToIndexMap.end()) ? CHATD_IDX_INVALID : it->second;
    }
    bool getHistory(int count); ///@ returns whether the fetch is from network (true), or database (false), so the app knows whether to display a progress bar/ui or not
    bool setMessageSeen(Idx idx);
    bool setMessageSeen(karere::Id msgid);
    Idx lastSeenIdx() const { return mLastSeenIdx; }
    bool historyFetchIsFromDb() const { return (mOldestKnownMsgId != 0); }
    void replayUnsentNotifications();
    void loadManualSending();
    ICrypto* crypto() const { return mCrypto; }
// Message output methods
    Message* msgSubmit(const char* msg, size_t msglen, Message::Type type, void* userp);
//Queues a message as a edit message for \c orig. \attention Will delete a previous edit if
//the original was not yet ack-ed by the server. That is, there can be only one pending
//edit for a not-yet-sent message, and if there was a previous one, it will be deleted.
//The user is responsible to clear any reference to a previous edit to avoid a dangling pointer.
    Message* msgModify(Message& msg, const char* newdata, size_t newlen, void* userp);
    int unreadMsgCount() const;
    void setListener(Listener* newListener) { mListener = newListener; }
// ==== Methods intended for the crypto module ====
    /** @brief The crypto module must call this method when it returned \c false from
     * \c msgEncrypt() and now it is able to successfully encrypt that message
     */
    void removeManualSend(uint64_t id);
protected:
    void msgSubmit(Message* msg);
    bool msgEncryptAndSend(Message* msg, uint8_t opcode, SendingItem* existingItem=nullptr);
    void continueEncryptNextPending();
    void onMsgUpdated(Message* msg);
    void keyConfirm(KeyId keyxid, KeyId keyid);
    void rejectMsgupd(uint8_t opcode, karere::Id id);
    template <bool mustBeInSending=false>
    void rejectGeneric(uint8_t opcode);
    void moveItemToManualSending(OutputQueue::iterator it, int reason);
    void handleTruncate(const Message& msg, Idx idx);
    void deleteMessagesBefore(Idx idx);
    void createMsgBackRefs(Message& msg);
    uint64_t generateRefId();
    void verifyMsgOrder(const Message& msg, Idx idx);
//===
};

class Client
{
protected:
/// maps the chatd shard number to its corresponding Shard connection
    std::map<int, std::shared_ptr<Connection>> mConnections;
/// maps a chatid to the handling Shard connection
    std::map<karere::Id, Connection*> mConnectionForChatId;
/// maps chatids to the Message object
    std::map<karere::Id, std::shared_ptr<Chat>> mChatForChatId;
    karere::Id mUserId;
    static bool sWebsockCtxInitialized;
    Connection& chatidConn(karere::Id chatid)
    {
        auto it = mConnectionForChatId.find(chatid);
        if (it == mConnectionForChatId.end())
            throw std::runtime_error("chatidConn: Unknown chatid "+chatid.toString());
        return *it->second;
    }
    void msgConfirm(karere::Id msgxid, karere::Id msgid);
public:
    static ws_base_s sWebsocketContext;
    unsigned inactivityCheckIntervalSec = 20;
    karere::Id userId() const { return mUserId; }
    Client(karere::Id userId);
    ~Client(){}
    Chat& chats(karere::Id chatid) const
    {
        auto it = mChatForChatId.find(chatid);
        if (it == mChatForChatId.end())
            throw std::runtime_error("chatidChat: Unknown chatid "+chatid.toString());
        return *it->second;
    }

    void join(karere::Id chatid, int shardNo, const std::string& url,
        Listener* listener, const karere::SetOfIds& initialUsers, ICrypto* crypto);
    void leave(karere::Id chatid);
    friend class Connection;
    friend class Chat;
};

class DbInterface
{
public:
    virtual void getHistoryInfo(karere::Id& oldestDbId, karere::Id& newestDbId, Idx& newestDbIdx) = 0;
    /// Called when the client was requested to fetch history, and it knows the db contains the requested
    /// history range.
    /// @param startIdx - the start index of the requested history range
    /// @param count - the number of messages to return
    /// @param[out] messages - The app should put the messages in this vector, the most recent message being
    /// at position 0 in the vector, and the oldest being the last. If the returned message count is less
    /// than the requested by \c count, the client considers there is no more history in the db. However,
    /// if the application-specified \c oldestDbId in the call to \n init() has not been retrieved yet,
    /// an assertion will be triggered. Therefore, the application must always try to read not less than
    /// \c count messages, in case they are avaialble in the db.
    virtual void fetchDbHistory(Idx startIdx, unsigned count, std::vector<Message*>& messages) = 0;
    virtual void saveMsgToSending(Chat::SendingItem& msg) = 0;
    virtual void updateMsgInSending(const chatd::Chat::SendingItem& item) = 0;
    virtual void addBlobsToSendingItem(uint64_t rowid, const MsgCommand* msgCmd, const Command* keyCmd) = 0;
    virtual void deleteItemFromSending(uint64_t rowid) = 0;
    virtual void updateMsgPlaintextInSending(uint64_t rowid, const StaticBuffer& data) = 0;
    virtual void updateMsgKeyIdInSending(uint64_t rowid, KeyId keyid) = 0;
    virtual void loadSendQueue(Chat::OutputQueue& queue) = 0;
    virtual void addMsgToHistory(const Message& msg, Idx idx) = 0;
    virtual void confirmKeyOfSendingItem(uint64_t rowid, KeyId keyid) = 0;
    virtual void updateMsgInHistory(karere::Id msgid, const StaticBuffer& newdata) = 0;
    virtual Idx getIdxOfMsgid(karere::Id msgid) = 0;
    virtual Idx getPeerMsgCountAfterIdx(Idx idx) = 0;
    virtual void saveItemToManualSending(const Chat::SendingItem& item, int reason) = 0;
    virtual void loadManualSendItems(std::vector<Chat::ManualSendItem>& items) = 0;
    virtual bool deleteManualSendItem(uint64_t rowid) = 0;
    virtual void truncateHistory(karere::Id msgid) = 0;
    virtual karere::Id getOldestMsgid() = 0;
    virtual void sendingItemMsgupdxToMsgupd(const chatd::Chat::SendingItem& item, karere::Id msgid) = 0;
    virtual void addUser(karere::Id userid, Priv priv) = 0;
    virtual void removeUser(karere::Id userid) = 0;
    virtual ~DbInterface(){}
};

}

#endif





