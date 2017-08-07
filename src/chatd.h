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
#include <base/trackDelete.h>
#include "chatdMsg.h"
#include "url.h"

namespace karere {
    class Client;
}

class MyMegaApi;

#define CHATD_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_chatd, fmtString, ##__VA_ARGS__)

enum: uint32_t { kPromiseErrtype_chatd = 0x3e9ac47d }; //should resemble 'megachtd'

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


/** @brief Reason codes passed to Listener::onManualSendRequired() */
enum ManualSendReason: uint8_t
{
    kManualSendInvalidReason = 0,
    kManualSendUsersChanged = 1, ///< Group chat participants have changed
    kManualSendTooOld = 2, ///< Message is older than CHATD_MAX_EDIT_AGE seconds
    kManualSendGeneralReject = 3, ///< chatd rejected the message, for unknown reason
    kManualSendNoWriteAccess = 4,  ///< Read-only privilege or not belong to the chatroom
    kManualSendEditNoChange = 6     /// Edit message has same content than message in server
};

/** The source from where history is being retrieved by the app */
enum HistSource
{
    kHistSourceNone = 0, //< History is not being retrieved
    kHistSourceRam = 1, //< History is being retrieved from the history buffer in RAM
    kHistSourceDb = 2, //<History is being retrieved from the local DB
    kHistSourceServer = 3, //< History is being retrieved from the server
    kHistSourceServerOffline = 4 //< History has to be fetched from server, but we are offline
};

class DbInterface;
struct LastTextMsg;

class Listener
{
public:
    /** @brief
     * This is the first call chatd makes to the Listener, passing it necessary objects and
     * retrieving info about the local history database
     * If you want to replay notifications related to this chat and the history retrieval to
     * start from the beginning (so every message is notified again), call \c Chat::resetListenerState
     * @param chat - the Chat object that can be used to access the message buffer etc
     * @param dbIntf[out] reference to the internal pointer to the database abstraction
     * layer interface. Set this pointer to a newly created instance of a db abstraction
     * instance.
     * @note The \c dbIntf creation is handled by karere, and in the app interface,
     * this callback should not change this pointer (which is set to NULL)
     */
    virtual void init(Chat& chat, DbInterface*& dbIntf) = 0;
    /** @brief Called when that chatroom instance is being destroyed
     *  (e.g. on application close)
     */
    virtual void onDestroy(){}
    /** @brief A new message was received. This can be just sent by a peer, or retrieved from the server.
     * @param idx The index of the message in the buffer
     * @param status - The 'seen' status of the message. Normally it should be
     * 'not seen', until we call setMessageSeen() on it
     */
    virtual void onRecvNewMessage(Idx idx, Message& msg, Message::Status status){}

    /** @brief A history message has been received, as a result of getHistory().
     * @param The index of the message in the history buffer
     * @param The message itself
     * @param status The 'seen' status of the message
     * @param isFromDb The message can be received from the server, or from the app's local
     * history db via \c fetchDbHistory() - this parameter specifies the source
     */
    virtual void onRecvHistoryMessage(Idx idx, Message& msg, Message::Status status, bool isLocal){}

    /**
     * @brief The retrieval of the requested history batch, via \c getHistory(), was completed
     * @param source The source from where the last message of the history
     * chunk was returned. This basically means that if some of it was returned from RAM
     * and then from DB, then source will be kHistSourceDb. This is not valid
     * from mixing messages from local source and from server, as they are never
     * mixed in one history chunk.
     */
    virtual void onHistoryDone(HistSource source) {}

    /**
     * @brief An unsent message was loaded from local db. The app should normally
     * display it at the end of the message history, indicating that it has not been
     * sent. This callback is called for all unsent messages, in order in which
     * the message posting ocurrent, from the oldest to the newest,
     * i.e. subsequent onUnsentMsgLoaded() calls are for newer unsent messages
     */
    virtual void onUnsentMsgLoaded(Message& msg) {}

    /**
     * @brief An unsent edit of a message was loaded. Similar to \c onUnsentMsgLoaded()
     * @param msg The edited message
     * @param oriIsSending - whether the original message has been sent or not
     * yet sent (on the send queue).
     * @note The calls to \c onUnsentMsgLoaded() and \c onUnsentEditLoaded()
     * are done in the order of the corresponding events (send, edit)
     */
    virtual void onUnsentEditLoaded(Message& msg, bool oriMsgIsSending) {}

    /** @brief A message sent by us was acknoledged by the server, assigning it a MSGID.
      * At this stage, the message state is "received-by-server", and it is in the history
      * buffer when this callback is called.
      * @param msgxid - The request-response match id that we generated for the sent message.
      * Normally the application doesn't need to care about it
      * @param msg - The message object - \c id() returns a real msgid, and \c isSending() is \c false
      * @param idx - The history buffer index at which the message was put
      */
    virtual void onMessageConfirmed(karere::Id msgxid, const Message& msg, Idx idx){}

     /** @brief A message was rejected by the server for some reason.
      * As the message is not yet in the history buffer, its \c id()
      * is a msgxid, and \c msg.isSending() is \c true.
      * The message may have actually been received by the server, but we
      * didn't know about that.
      * The message is already removed from the client's send queue.
      * The app must remove this message from the 'pending' GUI list.
      * @param msg - The message that was rejected.
      * @param reason - The reason for the reject.
      * When the reason code is 0, the client has received a MSGID, i.e.
      * the message is already received by the server.
      * Possible scenarions when this can happens are:
      * - We went offline after sending a message but just before receiving
      *  the confirmation for it.
      * - We tried to send the message while offline and restarted the app
      * while still offline, then went online. On *nix systems, the packets
      * from the previous app run are kept in the TCP output queue, and once
      * the machine goes online, they are sent, effectively behaving like a
      * second client that sent the same message with the same msgxid.
      * When the actual client tries to send it again, the server sees the
      * same msgxid and returns OP_MSGID with the already assigned id
      * of the message. The client must have already received this message as
      * a NEWMSG upon reconnect, so it can just remove the pending message.
      */
    virtual void onMessageRejected(const Message& msg, uint8_t reason){}

    /** @brief A message was delivered, seen, etc. When the seen/received pointers are advanced,
     * this will be called for each message of the pointer-advanced range, so the application
     * doesn't need to iterate over ranges by itself
     */
    virtual void onMessageStatusChange(Idx idx, Message::Status newStatus, const Message& msg){}

    /**
     * @brief Called when a message edit is received, i.e. MSGUPD is received.
     * The message is already updated in the history buffer and in the db,
     * and the GUI should also update it.
     * \attention If the edited message is not in memory, it is still updated in
     * the database, but this callback will not be called.
     * @param msg The edited message
     * @param idx - the index of the edited message
     */
    virtual void onMessageEdited(const Message& msg, Idx idx){}

    /** @brief An edit posted by us was rejected for some reason.
     * // TODO
     * @param msg
     * @param reason
     */
    virtual void onEditRejected(const Message& msg, ManualSendReason reason){}

    /** @brief The chatroom connection (to the chatd server shard) state
     * has changed.
     */
    virtual void onOnlineStateChange(ChatState state) = 0;

    /** @brief A user has joined the room, or their privilege has
     * changed.
     */
    virtual void onUserJoin(karere::Id userid, Priv privilege){}

    /**
     * @brief onUserLeave User has been excluded from the group chat
     * @param userid The userid of the user
     */
    virtual void onUserLeave(karere::Id userid){}

    /** @brief We have been excluded from this chatroom */
    virtual void onExcludedFromChat() {}

    /** @brief We have rejoined the room
     */
    virtual void onRejoinedChat() {}

    /** @brief Unread message count has changed */
    virtual void onUnreadChanged() {}

    /** @brief A message could not be sent automatically, due to some reason.
     * User-initiated retry is required.
     * @attention Ownership of \c msg is passed to application.
     * The application can re-send or discard the message. In both cases, it should
     * call removeManualSend() when it's about to resend or discard, in order to
     * remove the message from the pending-manual-action list.
     * To re-send, just call msgSubmit() in the normal way.
     * @param id The send queue id of the message. As the message has no msgid,
     * this is used to identify the message in seubsequent retry/cancel
     * @param reason - The code of the reason why the message could not be auto sent
     */
    virtual void onManualSendRequired(Message* msg, uint64_t id, ManualSendReason reason) {}

    /**
     * @brief onHistoryTruncated The history of the chat was truncated by someone
     * at the message \c msg.
     * @param msg The message at which the history is truncated. The message before
     * it (if it exists) is the last preserved message. The \c msg message is
     * overwritten with a management message that contains information who truncated the message.
     * @param idx - The index of \c msg
     */
    virtual void onHistoryTruncated(const Message& msg, Idx idx) {}

    /**
     * @brief onMsgOrderVerificationFail The message ordering check for \c msg has
     * failed. The message may have been tampered.
     * @param msg The message
     * @param idx Index of \c msg
     * @param errmsg The error string describing what exactly is the problem
     */
    virtual void onMsgOrderVerificationFail(const Message& msg, Idx idx, const std::string& errmsg)
    {
        CHATD_LOG_ERROR("msgOrderFail[msgid %s]: %s", msg.id().toString().c_str(), errmsg.c_str());
    }

    /**
     * @brief onUserTyping Called when a signal is received that a peer
     * is typing a message. Normally the app should have a timer that
     * is reset each time a typing notification is received. When the timer
     * expires, it should hide the notification GUI.
     * @param user The user that is typing. The app can use the user attrib
     * cache to get a human-readable name for the user.
     */
    virtual void onUserTyping(karere::Id userid) {}

    /**
     * @brief Called when the last known text message changes/is updated, so that
     * the app can display it next to the chat title
     * @param msg Contains the properties of the last text message
     */
    virtual void onLastTextMessageUpdated(const LastTextMsg& msg) {}
    /**
     * @brief Called when a message with a newer timestamp/modification time
     * is encountered. This can be used by the app to order chats in the chat
     * list GUI based on last interaction.
     * @param ts The timestamp of the newer message. If a message is edited,
     * ts is the sum of the original message timestamp and the update delta.
     */
    virtual void onLastMessageTsUpdated(uint32_t ts) {}
};

class Client;

// need DeleteTrackable for graceful disconnect timeout
class Connection: public karere::DeleteTrackable
{
public:
    enum State { kStateNew, kStateDisconnected, kStateConnecting, kStateConnected, kStateLoggedIn };
protected:
    Client& mClient;
    int mShardNo;
    std::set<karere::Id> mChatIds;
    ws_t mWebSocket = nullptr;
    State mState = kStateNew;
    karere::Url mUrl;
    megaHandle mInactivityTimer = 0;
    int mInactivityBeats = 0;
    bool mTerminating = false;
    promise::Promise<void> mConnectPromise;
    promise::Promise<void> mDisconnectPromise;
    promise::Promise<void> mLoginPromise;
    Connection(Client& client, int shardNo): mClient(client), mShardNo(shardNo){}
    State state() { return mState; }
    bool isOnline() const
    {
        return mState >= kStateConnected; //(mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
    }
    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
        size_t reason_len, void *arg);
    static void websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg);
    void onSocketClose(int ercode, int errtype, const std::string& reason);
    promise::Promise<void> reconnect(const std::string& url=std::string());
    promise::Promise<void> disconnect(int timeoutMs=2000);
    void notifyLoggedIn();
    void enableInactivityTimer();
    void disableInactivityTimer();
    void reset();
// Destroys the buffer content
    bool sendBuf(Buffer&& buf);
    promise::Promise<void> rejoinExistingChats();
    void resendPending();
    void join(karere::Id chatid);
    void hist(karere::Id chatid, long count);
    void execCommand(const StaticBuffer& buf);
    bool sendKeepalive(uint8_t opcode);
    friend class Client;
    friend class Chat;
public:
    promise::Promise<void> retryPendingConnection();
    ~Connection()
    {
        disableInactivityTimer();
        reset();
    }
};

enum ServerHistFetchState
{
/** Thie least significant 2 bits signify the history fetch source,
 * and correspond to HistSource values */

/** History is not being fetched, and there is probably history to fetch available */
    kHistNotFetching = 4,
/** We are fetching old messages if this flag is set, i.e. ones added to the back of
 *  the history buffer. If this flag is no set, we are fetching new messages, i.e. ones
 *  appended to the front of the history buffer */
    kHistOldFlag = 8,
/** We are fetching from the server if flag is set, otherwise we are fetching from
 * local db */
    kHistFetchingOldFromServer = kHistSourceServer | kHistOldFlag,
    kHistFetchingNewFromServer = kHistSourceServer | 0,
/** We are currently fetching history from db - always old messages */
/** Fething from RAM history buffer, always old messages */
    kHistDecryptingFlag = 16,
    kHistDecryptingOld = kHistDecryptingFlag | kHistOldFlag,
    kHistDecryptingNew = kHistDecryptingFlag | 0
};

/** @brief This is a class used to hold all properties of the last text
 * message that the app is interested in
 */
struct LastTextMsg
{
    /** @brief The sender of the message */
    karere::Id sender() const { return mSender; }
    /**
     * @brief Type of the last message
     *
     * This function returns the type of the message, as in Message::type.
     * @see \c Message::kMsgXXX enums.
     *
     * If no text message exists in history, type is \c LastTextMsg::kNone.
     * If the message is being fetched from server, type is \c LastTextMsg::kFetching.
     * If an error has occurred when trying to determine the message, like
     * server being offline, then LastTextMsg::kOffline will be returned.
     */
    uint8_t type() const { return mType; }
    /**
     * @brief Content of the message
     *
     * The message contents in text form, so it can be displayed as it is in the UI.
     * If it's a special message, then this string contains the most important part,
     * like filename for attachment messages.
     */
    const std::string& contents() const { return mContents; }

    Idx idx() const { return mIdx; }
    karere::Id id() const { assert(mIdx != CHATD_IDX_INVALID); return mId; }
    karere::Id xid() const { assert(mIdx == CHATD_IDX_INVALID); return mId; }

protected:
    uint8_t mType = Message::kMsgInvalid;
    karere::Id mSender;
    std::string mContents;
    Idx mIdx = CHATD_IDX_INVALID;
    karere::Id mId;
};

/** @brief Internal class that maintains the last-text-message state */
struct LastTextMsgState: public LastTextMsg
{
    /** Enum for mState */
    enum: uint8_t { kNone = 0x0, kFetching = 0xff, kOffline = 0xfe, kHave = 0x1 };

    bool mIsNotified = false;
    uint8_t state() const { return mState; }
    bool isValid() const { return mState == kHave; }
    bool isFetching() const { return mState == kFetching; }
    void setState(uint8_t state) { mState = state; }
    void assign(const chatd::Message& from, Idx idx)
    {
        assign(from, from.type, from.id(), idx, from.userid);
    }
    void assign(const Buffer& buf, uint8_t type, karere::Id id, Idx idx, karere::Id sender)
    {
        mType = type;
        mIdx = idx;
        mId = id;
        mContents.assign(buf.buf(), buf.dataSize());
        mSender = sender;
        mState = kHave;
        mIsNotified = false;
    }
    //assign both idx and proper msgid (was msgxid until now)
    void confirm(Idx idx, karere::Id msgid)
    {
        assert(mIdx == CHATD_IDX_INVALID);
        mIdx = idx;
        mId = msgid;
    }
    void clear() { mState = kNone; mType = Message::kMsgInvalid; mContents.clear(); }
protected:
    friend class Chat;
    uint8_t mState = kNone;
};

struct ChatDbInfo;

/** @brief Represents a single chatroom together with the message history.
 * Message sending is done by calling methods on this class.
 * The history buffer can grow in two directions and is always contiguous, i.e.
 * there are no "holes".
 */
class Chat: public karere::DeleteTrackable
{
///@cond PRIVATE
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
        karere::SetOfIds recipients;
        uint8_t opcode() const { return mOpcode; }
        void setOpcode(uint8_t op) { mOpcode = op; }
        SendingItem(uint8_t aOpcode, Message* aMsg, const karere::SetOfIds& aRcpts,
            uint64_t aRowid=0)
        : mOpcode(aOpcode), rowid(aRowid), msg(aMsg), recipients(aRcpts){}
        ~SendingItem(){ if (msg) delete msg; }
        bool isMessage() const { return ((mOpcode == OP_NEWMSG) || (mOpcode == OP_MSGUPD) || (mOpcode == OP_MSGUPDX)); }
        bool isEdit() const { return mOpcode == OP_MSGUPD || mOpcode == OP_MSGUPDX; }
        void setKeyId(KeyId keyid)
        {
            msg->keyid = keyid;
        }
    };
    typedef std::list<SendingItem> OutputQueue;
    struct ManualSendItem
    {
        Message* msg;
        uint64_t rowid;
        uint8_t opcode;
        ManualSendReason reason;
        ManualSendItem(Message* aMsg, uint64_t aRowid, uint8_t aOpcode, ManualSendReason aReason)
            :msg(aMsg), rowid(aRowid), opcode(aOpcode), reason(aReason){}
        ManualSendItem()
            :msg(nullptr), rowid(0), opcode(0), reason(kManualSendInvalidReason){}
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
    Listener* mListener;
    ChatState mOnlineState = kChatStateOffline;
    Priv mOwnPrivilege = PRIV_INVALID;
    karere::SetOfIds mUsers;
    karere::SetOfIds mUserDump; //< The initial dump of JOINs goes here, then after join is complete, mUsers is set to this in one step
    /// db-supplied initial range, that we use until we see the message with mOldestKnownMsgId
    /// Before that happens, missing messages are supposed to be in the database and
    /// incrementally fetched from there as needed. After we see the mOldestKnownMsgId,
    /// we disable this range by setting mOldestKnownMsgId to 0, and recalculate
    /// range() only from the buffer items
    karere::Id mOldestKnownMsgId;
    unsigned mLastServerHistFetchCount = 0; ///< The number of history messages that have been fetched so far by the currently active or the last history fetch. It is reset upon new history fetch initiation
    unsigned mLastHistDecryptCount = 0; ///< Similar to mLastServerHistFetchCount, but reflects the current number of message passed through the decrypt process, which may be less than mLastServerHistFetchCount at a given moment

    /** @brief The state of history fetching from server */
    ServerHistFetchState mServerFetchState = kHistNotFetching;

    /** @brief Whether we have more not-loaded history in db */
    bool mHasMoreHistoryInDb = false;
    bool mServerOldHistCbEnabled = false;
    bool mHaveAllHistory = false;
    bool mIsDisabled = false;
    Idx mNextHistFetchIdx = CHATD_IDX_INVALID;
    DbInterface* mDbInterface = nullptr;
    // last text message stuff
    LastTextMsgState mLastTextMsg;
    // crypto stuff
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
    uint32_t mLastMsgTs;
    // ====
    std::map<karere::Id, Message*> mPendingEdits;
    std::map<BackRefId, Idx> mRefidToIdxMap;
    Chat(Connection& conn, karere::Id chatid, Listener* listener,
         const karere::SetOfIds& users, uint32_t chatCreationTs, ICrypto* crypto);
    void push_forward(Message* msg) { mForwardList.emplace_back(msg); }
    void push_back(Message* msg) { mBackwardList.emplace_back(msg); }
    Message* oldest() const { return (!mBackwardList.empty()) ? mBackwardList.back().get() : mForwardList.front().get(); }
    Message* newest() const { return (!mForwardList.empty())? mForwardList.back().get() : mBackwardList.front().get(); }
    void clear()
    {
        mBackwardList.clear();
        mForwardList.clear();
    }
    // msgid can be 0 in case of rejections
    Idx msgConfirm(karere::Id msgxid, karere::Id msgid);
    bool msgAlreadySent(karere::Id msgxid, karere::Id msgid);
    Message* msgRemoveFromSending(karere::Id msgxid, karere::Id msgid);
    Idx msgIncoming(bool isNew, Message* msg, bool isLocal=false);
    bool msgIncomingAfterAdd(bool isNew, bool isLocal, Message& msg, Idx idx);
    void msgIncomingAfterDecrypt(bool isNew, bool isLocal, Message& msg, Idx idx);
    void onUserJoin(karere::Id userid, Priv priv);
    void onUserLeave(karere::Id userid);
    void onJoinComplete();
    void loadAndProcessUnsent();
    void initialFetchHistory(karere::Id serverNewest);
    void requestHistoryFromServer(int32_t count);
    Idx getHistoryFromDb(unsigned count);
    HistSource getHistoryFromDbOrServer(unsigned count);
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
    SendingItem* postMsgToSending(uint8_t opcode, Message* msg);
    bool sendKeyAndMessage(std::pair<MsgCommand*, KeyCommand*> cmd);
    void flushOutputQueue(bool fromStart=false);
    karere::Id makeRandomId();
    void login();
    void join();
    void joinRangeHist(const ChatDbInfo& dbInfo);
    void onDisconnect();
    void onHistDone(); //called upont receipt of HISTDONE from server
    void onFetchHistDone(); //called by onHistDone() if we are receiving old history (not new, and not via JOINRANGEHIST)
    void onNewKeys(StaticBuffer&& keybuf);
    void logSend(const Command& cmd);
    void handleBroadcast(karere::Id userid, uint8_t type);
    void findAndNotifyLastTextMsg();
    void notifyLastTextMsg();
    void onMsgTimestamp(uint32_t ts); //support for newest-message-timestamp
    bool manualResendWhenUserJoins() const;
    friend class Connection;
    friend class Client;
/// @endcond PRIVATE
public:
    unsigned initialHistoryFetchCount = 32; //< This is the amount of messages that will be requested from server _only_ in case local db is empty
    /** @brief users The current set of users in the chatroom */
    const karere::SetOfIds& users() const { return mUsers; }
    ~Chat();
    /** @brief The chatid of this chat */
    karere::Id chatId() const { return mChatId; }
    /** @brief The chatd client */
    Client& client() const { return mClient; }
    /** @brief The lowest index of a message in the RAM history buffer */
    Idx lownum() const { return mForwardStart - (Idx)mBackwardList.size(); }
    /** @brief The highest index of a message in the RAM history buffer */
    Idx highnum() const { return mForwardStart + (Idx)mForwardList.size()-1;}
    /** @brief Needed only for debugging purposes */
    Idx forwardStart() const { return mForwardStart; }
    /** The number of messages currently in the history buffer (in RAM).
     * @note Note that there may be more messages in history db, but not loaded
     * into memory*/
    Idx size() const { return mForwardList.size() + mBackwardList.size(); }
    /** @brief Whether we have any messages in the history buffer */
    bool empty() const { return mForwardList.empty() && mBackwardList.empty();}
    bool isDisabled() const { return mIsDisabled; }
    bool isFirstJoin() const { return mIsFirstJoin; }
    void disable(bool state) { mIsDisabled = state; }
    /** The index of the oldest decrypted message in the RAM history buffer.
     * This will be greater than lownum() if there are not-yet-decrypted messages
     * at the start of the buffer, i.e. when more history has been fetched, but
     * decryption keys have not yet been loaded for these messages.
     */
    Idx decryptedLownum() const
    {
        return (mDecryptOldHaltedAt == CHATD_IDX_INVALID)
            ? lownum() : mDecryptOldHaltedAt+1;
    }
    /** @brief Similar to decryptedLownum() */
    Idx decryptedHighnum() const
    {
        return(mDecryptNewHaltedAt == CHATD_IDX_INVALID)
            ? highnum() : mDecryptNewHaltedAt-1;
    }

    /** @brief connects the chatroom for the first time. A chatroom
      * is initialized in two stages: first it is created via Client::createChatRoom(),
      * after which it can be accessed in offline mode. The second stage is
      * connect(), after which it initiates or uses an existing connection to
      * chatd
      */
    void connect(const std::string& url=std::string());

    void disconnect();
    /** @brief The online state of the chatroom */
    ChatState onlineState() const { return mOnlineState; }

    /** @brief Get the seen/received status of a message. Both the message object
     * and its index in the history buffer must be provided */
    Message::Status getMsgStatus(const Message& msg, Idx idx) const;

    /** @brief Contains all not-yet-confirmed edits of messages.
      *  This can be used by the app to replace the text of messages who have
      * been edited before they have been sent/confirmed. Normally the app needs
      * to display the edited text in the unsent message.*/
    const std::map<karere::Id, Message*>& pendingEdits() const { return mPendingEdits; }

    /** @brief The chatd::Listener currently attached to this chat */
    Listener* listener() const { return mListener; }

    /** @brief Whether the listener will be notified upon receiving
     * old history messages from the server.
     */
    bool isServerOldHistCbEnabled() const { return mServerOldHistCbEnabled;}

    /** @brief Returns whether history is being fetched from server _and_
     * send to the application callback via \c onRecvHistoryMsg().
     */
    bool isNotifyingOldHistFromServer() const { return mServerOldHistCbEnabled && (mServerFetchState & kHistOldFlag); }

    /** @brief Returns whether we are fetching old or new history at the moment */
    bool isFetchingFromServer() const { return (mServerFetchState & kHistNotFetching) == 0; }

    /** @brief The current history fetch state */
    ServerHistFetchState serverFetchState() const { return mServerFetchState; }

    /** @brief Whether we are decrypting the fetched history. The app may need
     * to differentiate whether the history fetch process is doing the actual fetch, or
     * is waiting for the decryption (i.e. fetching chat keys etc)
     */
    bool isServerFetchDecrypting() const { return mServerFetchState & kHistDecryptingFlag; }

    /**
     * @brief haveAllHistory
     * Returned whether we have locally all existing history.
     * Note that this doesn't mean that we have sent all history the app
     * via getHistory() - the client may still have history that hasn't yet
     * been sent to the app after a getHistory(), i.e. because resetGetHistory()
     * has been called.
     */
    bool haveAllHistory() const { return mHaveAllHistory; }
    /** @brief returns whether the app has received all existing history
     * for the current history fetch session (since the chat creation or
     * sinte the last call to \c resetGetHistory()
     */
    bool haveAllHistoryNotified() const;
    /**
     * @brief The last number of history messages that have actually been
     * returned to the app via * \c getHitory() */
    unsigned lastHistDecryptCount() const { return mLastHistDecryptCount; }

    /** @brief
     * Get the message with the specified index, or \c NULL if that
     * index is out of range
     */
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

    /**
     * @brief Returns the message at the specified index in the RAM history buffer.
     * Throws if index is out of range
     */
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

    /**
     * @brief Returns the message at the specified index in the RAM history buffer.
     * Throws if index is out of range
     */
    Message& operator[](Idx num) const { return at(num); }

    /** @brief Returns whether the specified RAM history buffer index is valid or out
     * of range
     */
    bool hasNum(Idx num) const
    {
        if (num < mForwardStart)
            return (static_cast<size_t>(mForwardStart - num) <= mBackwardList.size());
        else
            return (num < mForwardStart + static_cast<int>(mForwardList.size()));
    }

    /**
     * @brief Returns the index of the message with the specified msgid.
     * @param msgid The message id whose index to find
     * @returns The index of the message inside the RAM history buffer.
     *  If no such message exists in the RAM history buffer, CHATD_IDX_INVALID
     * is returned
     */
    Idx msgIndexFromId(karere::Id msgid) const
    {
        auto it = mIdToIndexMap.find(msgid);
        return (it == mIdToIndexMap.end()) ? CHATD_IDX_INVALID : it->second;
    }

    /**
     * @brief Initiates fetching more history - from local RAM history buffer,
     * from local db or from server.
     * If ram + local db have less than the number of requested messages,
     * loading stops when local db is exhausted, returning less than \count
     * messages. Next call to this function will fetch history from server.
     * If there is no history in local db, loading is done from the chatd server.
     * Fetching from RAM and DB can be combined, if there is not enough history
     * in RAM. In that case, kHistSourceDb will be returned.
     * @param count - The number of requested messages to load. The actual number
     * of messages loaded can be less than this.
     * @returns The source from where history is fetched.
     * The app may use this to decide whether to display a progress bar/ui in case
     * the fetch is from server.
     */
    HistSource getHistory(unsigned count);

    /**
     * @brief Resets sending of history to the app, so that next getHistory()
     * will start from the newest known message. Note that this doesn't affect
     * the actual fetching of history from the server to the chatd client,
     * only the sending from the chatd client to the app.
     */
    void resetGetHistory();

    /**
     * @brief setMessageSeen Move the last-seen-by-us pointer to the message with the
     * specified index.
     * @return Whether the pointer was successfully set. Setting may fail if
     * it was attempted to set the pointer to an older than the current position.
     */
    bool setMessageSeen(Idx idx);

    /**
     * @brief Sets the last-seen-by-us pointer to the message with the specified
     * msgid.
     * @return Whether the pointer was successfully set. Setting may fail if
     * it was attempted to set the pointer to an older than the current position.
     */
    bool setMessageSeen(karere::Id msgid);

    /** @brief The last-seen-by-us pointer */
    Idx lastSeenIdx() const { return mLastSeenIdx; }

    /** @brief Whether the next history fetch will be from local db or from server */
    bool historyFetchIsFromDb() const { return (mOldestKnownMsgId != 0); }

    /** @brief The interface of the Strongvelope crypto module instance associated with this
      * chat
      */
    ICrypto* crypto() const { return mCrypto; }

    /** @group Message output methods */

    /** @brief Submits a message for sending.
     * @param msg - The message contents buffer
     * @param msglen - The size of the message contents buffer
     * @param type - The type of the message - normal message, type of management message,
     * application-specific type like link, share, picture etc.
     * @param userp - An optional user pointer to associate with the message object
     */
    Message* msgSubmit(const char* msg, size_t msglen, unsigned char type, void* userp);

    /** @brief Queues a message as an edit message for the specified original message.
     * @param msg - the original message
     * @param newdata - The new contents
     * @param newlen - The size of the new contents
     * @param userp - An optional user pointer to associate with the newly created
     * edit message
     * \attention Will delete a previous edit if the original was not yet ack-ed by
     * the server. That is, there can be only one pending edit for a not-yet-sent
     * message, and if there was a previous one, it will be deleted.
     * The user is responsible to clear any reference to a previous edit to avoid a
     * dangling pointer.
     */
    Message* msgModify(Message& msg, const char* newdata, size_t newlen, void* userp);

    /** @brief The number of unread messages. Calculated based on the last-seen-by-us pointer.
      * It's possible that the exact count is not yet known, if the last seen message is not
      * known by the client yet. In that case, the client knows the minumum count,
      * (which is the total count of locally loaded messages at the moment),
      * and returns the number as negative. The application may use this to
      * to display for example '1+' instead of '1' in the unread indicator.
      * When more history is fetched, the negative count increases in absolute value,
      * (but is still negative), until the actual last seen message is obtained,
      * at which point the count becomes positive.
      * An example: client has only one message pre-fetched from server, and
      * its msgid is not the same as the last-seen-by-us msgid. The client
      * will then return the unread count as -1. When one more message is loaded
      * from server but it's still not the last-seen one, the count will become
      * -2. A third message is fetched, and its msgid matches the last-seen-msgid.
      * The client will then return the unread count as +2, and will not change
      * as more history is fetched from server.
      * Example 1: Client has 1 message pre-fetched and its msgid is the same
      * as the last-seen-msgid. The count will be returned as 0.
      */
    int unreadMsgCount() const;

    /** @brief Returns the text of the most-recent message in the chat that can
     * be displayed as text in the chat list. If it is not found in RAM,
     * the database will be queried. If not found there as well, server is queried,
     * and 0xff is returned. When the message is received from server, the
     * \c onLastTextMsgUpdated callback will be called. If the network connection
     * is offline, then 0xfe will be returned, and upon reconnect and join, the
     * client will fetch messages from the server until it finds a text message
     * and will call the callback with it.
     * @param [out] msg Output pointer that will be set to the internal last-text-message
     * object. The object is owned by the client, and you should use this
     * pointer synchronously after the call to this function, and not in an
     * async-delayed way.
     * @return If there is currently a last-text-message, 1 is returned and
     * the output pointer will be set to the internal object. Otherwise,
     * the output pointer will be set to \c NULL, and an error code will be
     * returner, as follows:
     *   0xff - no text message is avaliable locally, the client is fetching
     * more history from server. The fetching will continue until a text
     * message is found, at which point the callback will be called.
     *   0xfe - no text message is available locally, and there is no internet
     * connection to fetch more history from server. When connection is
     * restored, the client will fetch history until a text message is found,
     * then it will call the callback.
     */
    uint8_t lastTextMessage(LastTextMsg*& msg);

    /** @brief Returns the timestamp of the newest known message */
    uint32_t lastMessageTs() { return mLastMsgTs; }

    /** @brief Changes the Listener */
    void setListener(Listener* newListener) { mListener = newListener; }

    /**
     * @brief Resets the state of the listener, initiating all initial
     * callbacks, such as the onManualSendRequired(), onUnsentMsgLoaded,
     * and resets the getHistory() pointer, so that subsequent getHistory()
     * calls will start returning history from the newest message.
     * You may want to call this method at \c chatd::Listener::init, so the
     * history retrieval starts from the beginning.
     */
    void resetListenerState();
    /**
     * @brief getMsgByXid searches the send queue for a message with the specified
     * msgxid. The message is supposed to be unconfirmed, but in reality the
     * message may have been received and recorded by the server,
     * and the client may have not received the confirmation.
     * @param msgxid - The transaction id of the message
     * @returns Pointer to the Message object, or nullptr if a message with
     * that msgxid does not exist in the send queue.
     */
    Message* getMsgByXid(karere::Id msgxid);
    /**
     * @brief Removes the specified manual-send message, from the manual send queue.
     * Normally should be called when the user opts to not retry sending the message
     * @param id The id of the message, provided by \c onManualSendRequired()
     */
    void removeManualSend(uint64_t id);

    /** @brief Broadcasts a notification that the user is typing. This will trigged
     * other clients receiving \c onUserTyping() callbacks
     */
    void sendTypingNotification();
    /**
     * @brief Generates a backreference id. Must be public because strongvelope
     *  uses it to generate chat title messages
     * @param aCrypto - the crypto module interface to use for random number
     * generation.
     */
    static uint64_t generateRefId(const ICrypto* aCrypto);
    Message *getManualSending(uint64_t rowid, chatd::ManualSendReason& reason);
protected:
    void msgSubmit(Message* msg);
    bool msgEncryptAndSend(OutputQueue::iterator it);
    void continueEncryptNextPending();
    void onMsgUpdated(Message* msg);
    void onJoinRejected();
    void keyConfirm(KeyId keyxid, KeyId keyid);
    void rejectMsgupd(karere::Id id, uint8_t serverReason);
    template <bool mustBeInSending=false>
    void rejectGeneric(uint8_t opcode);
    void moveItemToManualSending(OutputQueue::iterator it, ManualSendReason reason);
    void handleTruncate(const Message& msg, Idx idx);
    void deleteMessagesBefore(Idx idx);
    void createMsgBackRefs(Message& msg);
    void verifyMsgOrder(const Message& msg, Idx idx);
    /**
     * @brief Initiates replaying of callbacks about unsent messages and unsent
     * edits, i.e. \c onUnsentMsgLoaded() and \c onUnsentEditLoaded().
     * This may be needed when the listener is switched, in order to init the new
     * listener state */
    void replayUnsentNotifications();
    void onLastTextMsgUpdated(const Message& msg, Idx idx=CHATD_IDX_INVALID);
    void findLastTextMsg();
    /**
     * @brief Initiates loading of the queue with messages that require user
     * approval for re-sending */
    void loadManualSending();

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
    bool onMsgAlreadySent(karere::Id msgxid, karere::Id msgid);
    void msgConfirm(karere::Id msgxid, karere::Id msgid);
public:
    enum: uint32_t { kOptManualResendWhenUserJoins = 1 };
    static ws_base_s sWebsocketContext;
    unsigned inactivityCheckIntervalSec = 20;
    uint32_t options = 0;
    MyMegaApi *mApi;
    uint8_t mKeepaliveType = OP_KEEPALIVE;
    karere::Id userId() const { return mUserId; }
    Client(MyMegaApi *api, karere::Id userId);
    ~Client(){}
    Chat& chats(karere::Id chatid) const
    {
        auto it = mChatForChatId.find(chatid);
        if (it == mChatForChatId.end())
            throw std::runtime_error("chatidChat: Unknown chatid "+chatid.toString());
        return *it->second;
    }
    /** @brief Joins the specifed chatroom on the specified shard, using the specified
     * url, and assocuates the specified Listener and ICRypto instances
     * with the newly created Chat object.
     */
    Chat& createChat(karere::Id chatid, int shardNo, const std::string& url,
        Listener* listener, const karere::SetOfIds& initialUsers, ICrypto* crypto, uint32_t chatCreationTs);
    /** @brief Leaves the specified chatroom */
    void leave(karere::Id chatid);
    promise::Promise<void> disconnect();
    void sendKeepalive();
    promise::Promise<void> retryPendingConnections();
    bool manualResendWhenUserJoins() const { return options & kOptManualResendWhenUserJoins; }
    void notifyUserIdle();
    void notifyUserActive();
    friend class Connection;
    friend class Chat;
};

struct ChatDbInfo
{
    karere::Id oldestDbId;
    karere::Id newestDbId;
    Idx newestDbIdx;
    karere::Id lastSeenId;
    karere::Id lastRecvId;
};

class DbInterface
{
public:
    virtual void getHistoryInfo(ChatDbInfo& info) = 0;
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
    virtual void updateMsgInHistory(karere::Id msgid, const Message& msg) = 0;
    virtual Idx getIdxOfMsgid(karere::Id msgid) = 0;
    virtual Idx getPeerMsgCountAfterIdx(Idx idx) = 0;
    virtual void saveItemToManualSending(const Chat::SendingItem& item, int reason) = 0;
    virtual void loadManualSendItems(std::vector<Chat::ManualSendItem>& items) = 0;
    virtual bool deleteManualSendItem(uint64_t rowid) = 0;
    virtual void loadManualSendItem(uint64_t rowid, Chat::ManualSendItem& item) = 0;
    virtual void truncateHistory(const chatd::Message& msg) = 0;
    virtual void setLastSeen(karere::Id msgid) = 0;
    virtual void setLastReceived(karere::Id msgid) = 0;
    virtual chatd::Idx getOldestIdx() = 0;
    virtual void sendingItemMsgupdxToMsgupd(const chatd::Chat::SendingItem& item, karere::Id msgid) = 0;
    virtual void setHaveAllHistory() = 0;
    virtual bool haveAllHistory() = 0;
    virtual void getLastTextMessage(Idx from, chatd::LastTextMsgState& msg) = 0;
    virtual ~DbInterface(){}
};

}

#endif



