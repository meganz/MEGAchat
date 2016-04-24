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
#include <promise.h>
#include <base/timers.h>
#include "base64.h"

#define CHATD_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_chatd, fmtString, ##__VA_ARGS__)
#define CHATD_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_chatd, fmtString, ##__VA_ARGS__)

#define CHATD_MAX_EDIT_AGE 3600
namespace chatd
{
// command opcodes
enum Opcode
{
    OP_KEEPALIVE = 0,
    OP_JOIN = 1,
    OP_OLDMSG = 2,
    OP_NEWMSG = 3,
    OP_MSGUPD = 4,
    OP_SEEN = 5,
    OP_RECEIVED = 6,
    OP_RETENTION = 7,
    OP_HIST = 8,
    OP_RANGE = 9,
    OP_MSGID = 10,
    OP_REJECT = 11,
    OP_BROADCAST = 12,
    OP_HISTDONE = 13,
    OP_NEWKEY = 17,
    OP_KEYID = 18,
    OP_JOINRANGEHIST = 19,
    OP_MSGUPDX = 20,
    OP_LAST = OP_MSGUPDX
};

// privilege levels
enum Priv: signed char
{
    PRIV_NOCHANGE = -2,
    PRIV_NOTPRESENT = -1,
    PRIV_RDONLY = 0,
    PRIV_RDWR = 1,
    PRIV_FULL = 2,
    PRIV_OPER = 3
};

/// This type is used for ordered indexes in the message buffer
typedef int32_t Idx;

/// We want to fit in the positive range of a signed int64_t, for compatibility with sqlite which has no unsigned types
/// Don't use an enum as its implicit type may screw up our value
#define CHATD_IDX_RANGE_MIDDLE 0
#define CHATD_IDX_INVALID 0x7fffffff

class Id
{
public:
    uint64_t val;
    std::string toString() const { return base64urlencode(&val, sizeof(val)); }
    Id(const uint64_t& from=0): val(from){}
    explicit Id(const char* b64, size_t len=0) { base64urldecode(b64, len?len:strlen(b64), &val, sizeof(val)); }
    bool operator==(const Id& other) const { return val == other.val; }
    Id& operator=(const Id& other) { val = other.val; return *this; }
    Id& operator=(const uint64_t& aVal) { val = aVal; return *this; }
    operator const uint64_t&() const { return val; }
    bool operator<(const Id& other) const { return val < other.val; }
    static const Id null() { return static_cast<uint64_t>(0); }
};

class Chat;
class ICrypto;

typedef uint32_t KeyId;
class Key
{
protected:
    Id mUserid;
    void setData(const char* aData, uint16_t aLen)
    {
        assert(aLen <= kMaxLen);
        memcpy(data, aData, aLen);
    }
    friend class Chat;
public:
    enum { kMaxLen = 16 };
    enum { kInvalidId = 0, kUnconfirmedId = 0xffffffff };
    Id userid() const { return mUserid; }
    uint16_t len;
    char data[kMaxLen];
    Key(Id aUserid, uint16_t aLen, const char* aData): mUserid(aUserid)
    {
        setData(aData, aLen);
    }
    Key(Id aUserid): mUserid(aUserid), len(0){}
    bool isValid() const { return len > 0; }
};

class KeyWithId: public Key
{
protected:
    KeyId mId;
    void setId(KeyId aId) { mId = aId; }
    friend class Chat;
public:
    KeyId id() const { return mId; }
    KeyWithId(KeyId aId, Id aUserid, uint16_t aLen, const char* aData)
        :Key(aUserid, aLen, aData), mId(aId){}
    KeyWithId(Id aUserid=Id::null()): Key(aUserid), mId(Key::kInvalidId){}
    bool isValid() const { return mId != Key::kInvalidId; }
    bool isUnconfirmed() const { return mId == Key::kUnconfirmedId; }
    void confirm(KeyId keyid) { assert(isUnconfirmed()); mId = keyid; }
};

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
class Message: public Buffer
{
public:
    enum Type: unsigned char
    {
        kNormalMsg = 0,
        kUser = 16
    };
    enum Status
    {
        kSending,
        kServerReceived,
        kServerRejected,
        kDelivered,
        kLastOwnMessageStatus = kDelivered, //if a status is <= this, we created the msg, oherwise not
        kNotSeen,
        kSeen
    };
    enum EncryptedState: unsigned char
    {
        kEncrypted = 1,
        kDecryptError = 3
    };

private:
//avoid setting the id and flag pairs one by one by making them accessible only by setXXX(Id,bool)
    Id mId;
    bool mIdIsXid = false;
public:
    Id userid;
    uint32_t ts;
    uint16_t updated;
    uint32_t keyid;
    Type type;
    mutable void* userp;
    mutable uint32_t userFlags = 0;
    Id id() const { return mId; }
    bool isSending() const { return mIdIsXid; }
    void setId(Id aId, bool isXid) { mId = aId; mIdIsXid = isXid; }
    Message(Id aMsgid, Id aUserid, uint32_t aTs, uint16_t aUpdated,
          Buffer&& buf, bool aIsSending=false, KeyId aKeyid=Key::kInvalidId,
          Type aType=kNormalMsg, void* aUserp=nullptr)
      :Buffer(std::forward<Buffer>(buf)), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid),
          ts(aTs), updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}
    Message(Id aMsgid, Id aUserid, uint32_t aTs, uint16_t aUpdated,
            const char* msg, size_t msglen, bool aIsSending=false,
            KeyId aKeyid=Key::kInvalidId, Type aType=kNormalMsg, void* aUserp=nullptr)
        :Buffer(msg, msglen), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid), ts(aTs),
            updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp) {}
    static const char* statusToStr(unsigned status)
    {
        return (status > kSeen) ? "(invalid status)" : statusNames[status];
    }
protected:
    static const char* statusNames[];
    friend class Chat;
};

enum ChatState
{kChatStateOffline = 0, kChatStateConnecting, kChatStateJoining, kChatStateOnline};
static inline const char* chatStateToStr(unsigned state);

class DbInterface;
class Listener
{
public:
/// This is the first call chatd makes to the Listener, passing it necessary objects and
/// retrieving info about the local history database
/// @param messages - the Chat object that can be used to access the message buffer etc
/// @param out - The interface for sending messages
/// @param[out] oldestDbId, @param[out] newestDbId The range of messages
/// that we have in the local database
/// @param newestDbIdx - the index of the newestDbId message, so that the message buffer indexes
/// will be adjusted to match the ones in the local db
    virtual void init(Chat& messages, DbInterface*& dbIntf) = 0;
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
    virtual void onUnsentMsgLoaded(const Message& msg){}
/// A message sent by us was acknoledged by the server, assigning it a MSGID.
/// At this stage, the message state is "received-by-server", and it is in the history
/// buffer when this callback is called.
/// @param msgxid - The request-response match id that we generated for the sent message.
/// Normally the application doesn't need to care about it
/// @param msg - The message object - \c id() returns a real msgid, and \c isSending() is \c false
/// @param idx - The history buffer index at which the message was put
    virtual void onMessageConfirmed(Id msgxid, const Message& msg, Idx idx){}
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
/// The chatroom connection (to the chatd server shard) state state has changed.
    virtual void onOnlineStateChange(ChatState state){}
/// A user has joined or left the room, or their privilege has changed. If user has left
/// the room priv is PRIV_NOTPRESENT
    virtual void onUserJoinLeave(Id userid, Priv privilege){}
///Unread message count has changed
    virtual void onUnreadChanged() {}
};

class MsgCommand;

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
protected:
    static const char* opcodeNames[];
public:
    Command(): Buffer(){}
    Command(Command&& other): Buffer(std::forward<Buffer>(other)) {assert(!other.buf() && !other.bufSize() && !other.dataSize());}
    Command(uint8_t opcode): Buffer(64) { write(0, opcode); }
    template<class T>
    Command&& operator+(const T& val)
    {
        append(val);
        return std::move(*this);
    }
    Command&& operator+(const Buffer& msg)
    {
        append<uint32_t>(msg.dataSize());
        append(msg.buf(), msg.dataSize());
        return std::move(*this);
    }
    uint8_t opcode() const { return *reinterpret_cast<uint8_t*>(read(0,1)); }
    const char* opcodeName() const { return opcodeToStr(opcode()); }
    static const char* opcodeToStr(uint8_t code)
    {
        return (code > OP_LAST) ? "(invalid opcpde)" : opcodeNames[code];
    }
    virtual ~Command(){}
};
//we need that special class because we may update key ids after keys get confirmed,
//so in case of NEWMSG with keyxid, if the client reconnects between key confirm and
//NEWMSG send, the NEWMSG would not use the no longer valid keyxid, but a real key id
class MsgCommand: public Command
{
public:
    MsgCommand(uint8_t opcode, Id chatid, Id userid, Id msgid,
                   KeyId keyid=Key::kInvalidId, uint32_t ts=0, uint16_t updated=0)
    :Command(opcode)
    {
        write(1, chatid);write(9, userid);write(17, msgid);write(25, ts);
        write(29, updated);write(31, keyid);write(35, 0); //msglen
    }
    Id msgid() const { return read<uint64_t>(17); }
    void setId(Id aMsgid) { write(17, aMsgid); }
    KeyId keyId() const { return read<KeyId>(31); }
    void setKeyId(KeyId aKeyid) { write(31, aKeyid); }
    uint32_t msglen() const { return read<uint32_t>(39); }
    void clearMsg()
    {
        if (msglen() > 0)
            memset(buf()+43, 0, msglen()); //clear old message memory
        write(39, (uint32_t)0);
    }
    void setMsg(const char* msg, uint32_t msglen)
    {
        write(39, msglen);
        memcpy(writePtr(43, msglen), msg, msglen);
    }
};

//for exception message purposes
static inline std::string operator+(const char* str, const Id& id)
{
    std::string result(str);
    result.append(id.toString());
    return result;
}
static inline std::string& operator+(std::string&& str, const Id& id)
{
    str.append(id.toString());
    return str;
}

class Client;

class Connection
{
protected:
    Client& mClient;
    int mShardNo;
    std::set<Id> mChatIds;
    ws_t mWebSocket = nullptr;
    Url mUrl;
    megaHandle mInactivityTimer = 0;
    int mInactivityBeats = 0;
    bool mTerminating = false;
    std::unique_ptr<promise::Promise<void> > mConnectPromise;
    std::unique_ptr<promise::Promise<void> > mDisconnectPromise;
    Connection(Client& client, int shardNo): mClient(client), mShardNo(shardNo){}
    int getState() { return mWebSocket ? ws_get_state(mWebSocket) : WS_STATE_CLOSED_CLEANLY; }
    bool isOnline() const
    {
        return (mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
    }
    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
        size_t reason_len, void *arg);
    void onSocketClose();
    promise::Promise<void> reconnect();
    promise::Promise<void> disconnect();
    void enableInactivityTimer();
    void disableInactivityTimer();
    void reset();
// Destroys the buffer content
    bool sendBuf(Buffer&& buf);
    void rejoinExistingChats();
    void resendPending();
    void join(const Id& chatid);
    void hist(const Id& chatid, long count);
    void execCommand(const StaticBuffer& buf);
    friend class Client;
    friend class Chat;
public:
    ~Connection()
    {
        disableInactivityTimer();
    }
};

enum HistFetchState
{
    kHistNotFetching = 0, ///< History is not being fetched, and there is probably history to fetch available
    kHistNoMore = 1, ///< History is not being fetched, and we don't have any more history neither in db nor on server
    kHistFetchingFlag = 2, ///< Set in case we are fetching either from server or db
    kHistFetchingFromServer = 0 | kHistFetchingFlag, ///< We are currently fetching history from server
    kHistFetchingFromDb = 1 | kHistFetchingFlag ///< We are currently fetching history from db
};

typedef std::map<Id,Priv> UserPrivMap;
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
        std::unique_ptr<Command> cmd;
        void* data; //can be a Message or Key, depending on opcode
        /** When sending a message, we attach the Message object here to avoid
         * double-converting it when queued as a raw command in Sending, and after
         * that (when server confirms) move it as a Message object to history buffer */
        uint8_t opcode() const { return mOpcode; }
        SendingItem(uint8_t aOpcode, Command* aCmd, void* aData, uint64_t aRowid=0)
            : mOpcode(aOpcode), rowid(aRowid), cmd(aCmd), data(aData){}
        ~SendingItem()
        {
            if (data)
            {
                if (isMessage())
                    delete reinterpret_cast<Message*>(data);
                else if (mOpcode == OP_NEWKEY)
                    delete reinterpret_cast<Key*>(data);
                else
                    throw std::runtime_error("SendingItem dtor: Don't know how to delete data of opcode "+std::to_string(mOpcode));
                data = nullptr;
            }
        }
        bool isMessage() const { return ((mOpcode == OP_NEWMSG) || (mOpcode == OP_MSGUPD) || (mOpcode == OP_MSGUPDX)); }
        MsgCommand* msgCommand() const { assert(isMessage()); return static_cast<MsgCommand*>(cmd.get()); }
        Message* msg() const { assert(isMessage()); return reinterpret_cast<Message*>(data); }
        void setKeyId(KeyId keyid)
        {
            assert(isMessage());
            reinterpret_cast<Message*>(data)->keyid = keyid;
            if (cmd) static_cast<MsgCommand*>(cmd.get())->setKeyId(keyid);
        }
    };
    typedef std::list<SendingItem> OutputQueue;
protected:
    Connection& mConnection;
    Client& mClient;
    Id mChatId;
    Idx mForwardStart;
    std::vector<Message*> mForwardList;
    std::vector<Message*> mBackwardList;
    OutputQueue mSending;
    OutputQueue::iterator mNextUnsent;
    bool mIsFirstJoin = true;
    std::map<Id, Idx> mIdToIndexMap;
    Id mLastReceivedId;
    Idx mLastReceivedIdx = CHATD_IDX_INVALID;
    Id mLastSeenId;
    Idx mLastSeenIdx = CHATD_IDX_INVALID;
    bool mHasMoreHistoryInDb = false;
    Listener* mListener;
    ChatState mOnlineState = kChatStateOffline;
//    UserPrivMap mUsers;
    /// db-supplied initial range, that we use until we see the message with mOldestKnownMsgId
    /// Before that happens, missing messages are supposed to be in the database and
    /// incrementally fetched from there as needed. After we see the mOldestKnownMsgId,
    /// we disable this range by setting mOldestKnownMsgId to 0, and recalculate
    /// range() only from the buffer items
    Id mOldestKnownMsgId;
    Id mNewestKnownMsgId;
    unsigned mLastHistFetchCount = 0; ///< The number of history messages that have been fetched so far by the currently active or the last history fetch. It is reset upon new history fetch initiation
    HistFetchState mHistFetchState = kHistNotFetching;
    DbInterface* mDbInterface = nullptr;
    std::map<uint32_t, Key*> keys;
    KeyWithId currentSendKey; //we use a pointer here because we may have no key yet
    ICrypto* mCrypto;
    bool mEncryptionHalted = false;
    Chat(Connection& conn, const Id& chatid, Listener* listener, ICrypto* crypto);
    void push_forward(Message* msg) { mForwardList.push_back(msg); }
    void push_back(Message* msg) { mBackwardList.push_back(msg); }
    Message* first() const { return (!mBackwardList.empty()) ? mBackwardList.front() : mForwardList.back(); }
    Message* last() const { return (!mForwardList.empty())? mForwardList.front() : mBackwardList.back(); }
    void clear()
    {
        for (auto& msg: mBackwardList)
            delete msg;
        mBackwardList.clear();
        for (auto& msg: mForwardList)
            delete msg;
        mForwardList.clear();
    }
    // msgid can be 0 in case of rejections
    Idx msgConfirm(Id msgxid, Id msgid);
    Idx msgIncoming(bool isNew, Message* msg, bool isLocal=false);
    void onUserJoin(const Id& userid, Priv priv);
    void onJoinComplete();
    void loadAndProcessUnsent();
    void initialFetchHistory(Id serverNewest);
    void requestHistoryFromServer(int32_t count);
    void getHistoryFromDb(unsigned count);
    void onLastReceived(const Id& msgid);
    void onLastSeen(const Id& msgid);
    // As sending data over libws is destructive to the buffer, we have two versions
    // of sendCommand - the one with the rvalue reference is picked by the compiler
    // whenever the command object is a temporary, avoiding copying the buffer,
    // and the const reference one is picked when the Command object has to be preserved
    bool sendCommand(Command&& cmd);
    bool sendCommand(const Command& cmd);
    bool msgSend(const Message& message);
    void setOnlineState(ChatState state);
    void enqueueMsgForSend(Message* msg);
    bool flushOutputQueue(bool fromStart=false);
    Id makeRandomId();
    void login();
    void join();
    void joinRangeHist();
    void onHistDone(); //called upont receipt of HISTDONE from server
    void onNewKeys(StaticBuffer&& keybuf);
    friend class Connection;
    friend class Client;
public:
    unsigned initialHistoryFetchCount = 32; //< This is the amount of messages that will be requested from server _only_ in case local db is empty
//    const UserPrivMap& users() const { return mUsers; }
    ~Chat();
    const Id& chatId() const { return mChatId; }
    Client& client() const { return mClient; }
    Idx lownum() const { return mForwardStart - mBackwardList.size(); }
    Idx highnum() const { return mForwardStart + mForwardList.size()-1;}
    Idx size() const { return mForwardList.size() + mBackwardList.size(); }
    bool empty() const { return mForwardList.empty() && mBackwardList.empty();}
    ChatState onlineState() const { return mOnlineState; }
    Message::Status getMsgStatus(Idx idx, const Id& userid);
    Listener* listener() const { return mListener; }
    bool isFetchingHistory() const { return mHistFetchState & kHistFetchingFlag; }
    HistFetchState histFetchState() const { return mHistFetchState; }
    unsigned lastHistFetchCount() const { return mLastHistFetchCount; }
    inline Message* findOrNull(Idx num) const
    {
        if (num < mForwardStart) //look in mBackwardList
        {
            Idx idx = mForwardStart - num - 1; //always >= 0
            if (static_cast<size_t>(idx) >= mBackwardList.size())
                return nullptr;
            return mBackwardList[idx];
        }
        else
        {
            Idx idx = num - mForwardStart;
            if (static_cast<size_t>(idx) >= mForwardList.size())
                return nullptr;
            return mForwardList[idx];
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
    Idx msgIndexFromId(Id id)
    {
        auto it = mIdToIndexMap.find(id);
        return (it == mIdToIndexMap.end()) ? CHATD_IDX_INVALID : it->second;
    }
    bool getHistory(int count); ///@ returns whether the fetch is from network (true), or database (false), so the app knows whether to display a progress bar/ui or not
    bool setMessageSeen(Idx idx);
    bool setMessageSeen(Id msgid);
    Idx lastSeenIdx() const { return mLastSeenIdx; }
    bool historyFetchIsFromDb() const { return (mOldestKnownMsgId != 0); }
    void onCanEncryptAgain();
// Message output methods
    Message* msgSubmit(const char* msg, size_t msglen, Message::Type type, void* userp);
//Queues a message as a edit message for \c orig. \attention Will delete a previous edit if
//the original was not yet ack-ed by the server. That is, there can be only one pending
//edit for a not-yet-sent message, and if there was a previous one, it will be deleted.
//The user is responsible to clear any reference to a previous edit to avoid a dangling pointer.
    Message* msgModify(Message& msg, const char* newdata, size_t newlen, void* userp);
    int unreadMsgCount() const;
    void setListener(Listener* newListener) { mListener = newListener; }
protected:
    void msgEncryptAndSend(Message* msg, uint8_t opcode);
    void onMsgUpdated(const Message& msg);
    void keyConfirm(KeyId keyxid, KeyId keyid);
    void setNewSendKey(Key* key, Command* cmd);

//===
};

class Client
{
protected:
/// maps the chatd shard number to its corresponding Shard connection
    std::map<int, std::shared_ptr<Connection>> mConnections;
/// maps a chatid to the handling Shard connection
    std::map<Id, Connection*> mConnectionForChatId;
/// maps chatids to the Message object
    std::map<Id, std::shared_ptr<Chat>> mChatForChatId;
    Id mUserId;
    static bool sWebsockCtxInitialized;
    Connection& chatidConn(const Id& chatid)
    {
        auto it = mConnectionForChatId.find(chatid);
        if (it == mConnectionForChatId.end())
            throw std::runtime_error("chatidConn: Unknown chatid "+chatid);
        return *it->second;
    }
    void msgConfirm(Id msgxid, Id msgid);
public:
    static ws_base_s sWebsocketContext;
    unsigned inactivityCheckIntervalSec = 20;
    const Id& userId() const { return mUserId; }
    Client(const Id& userId);
    ~Client(){}
    Chat& chats(const Id& chatid) const
    {
        auto it = mChatForChatId.find(chatid);
        if (it == mChatForChatId.end())
            throw std::runtime_error("chatidChat: Unknown chatid "+chatid);
        return *it->second;
    }

    void join(const Id& chatid, int shardNo, const std::string& url, Listener* listener, ICrypto* crypto);
    void leave(Id chatid);
    friend class Connection;
    friend class Chat;
};

class DbInterface
{
public:
    virtual void getHistoryInfo(Id& oldestDbId, Id& newestDbId, Idx& newestDbIdx) = 0;
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
    virtual void saveItemToSending(Chat::SendingItem& msg) = 0;
    virtual void addCommandBlobToSendingItem(uint64_t rowid, const Command& cmd) = 0;
    virtual void deleteItemFromSending(uint64_t rowid) = 0;
    virtual void updateMsgPlaintextInSending(uint64_t rowid, const StaticBuffer& data) = 0;
    virtual void updateMsgKeyIdInSending(uint64_t rowid, KeyId keyid) = 0;
    virtual void loadSendQueue(Chat::OutputQueue& queue) = 0;
    virtual void addMsgToHistory(const Message& msg, Idx idx) = 0;
    virtual void updateMsgInHistory(Id msgid, const StaticBuffer& newdata) = 0;
    virtual Idx getIdxOfMsgid(Id msgid) = 0;
    virtual Idx getPeerMsgCountAfterIdx(Idx idx) = 0;
    virtual ~DbInterface(){}
};

static inline const char* chatStateToStr(unsigned state)
{
    static const char* chatStates[] =
    { "Offline", "Connecting", "Joining", "Online"};

    if (state > chatd::kChatStateOnline)
        return "(unkown state)";
    else
        return chatStates[state];
}


}
namespace std
{
    template<>
    struct hash<chatd::Id> { size_t operator()(const chatd::Id& id) const { return hash<uint64_t>()(id.val); } };
}



#endif





