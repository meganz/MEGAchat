#ifndef __CHATD_H__
#define __CHATD_H__

#include <libws.h>
#include <stdint.h>
#include <string>
#include <buffer.h>
#include <map>
#include <set>
#include <list>
#include <promise.h>
#include <base/timers.h>

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
    OP_LAST = OP_HISTDONE
};

// privilege levels
enum Priv
{
    PRIV_NOCHANGE = -2,
    PRIV_NOTPRESENT = -1,
    PRIV_RDONLY = 0,
    PRIV_RDWR = 1,
    PRIV_FULL = 2,
    PRIV_OPER = 3
};

std::string base64urlencode(const void *data, size_t inlen);
size_t base64urldecode(const char* str, size_t len, void* bin, size_t binlen);
class Id
{
public:
    uint64_t val;
    std::string toString() const { return base64urlencode(&val, sizeof(val)); }
    Id(const uint64_t& from=0): val(from){}
    Id(const char* b64, size_t len=0) { base64urldecode(b64, len?len:strlen(b64), &val, sizeof(val)); }
    bool operator==(const Id& other) const { return val == other.val; }
    Id& operator=(const Id& other) { val = other.val; return *this; }
    Id& operator=(const uint64_t& aVal) { val = aVal; return *this; }
    operator const uint64_t&() const { return val; }
    bool operator<(const Id& other) const { return val < other.val; }
    static const Id null() { return static_cast<uint64_t>(0); }
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
    Id id;
    Id userid;
    uint32_t ts;
    Message(const Id& aMsgid, const Id& aUserid, uint32_t aTs, const char* msg, size_t msglen)
        :Buffer(msg, msglen), id(aMsgid), userid(aUserid), ts(aTs){}
};

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
protected:
    static const char* opcodeNames[];
public:
    Command(Command&& other): Buffer(std::forward<Buffer>(other)) {assert(!other.buf() && !other.bufSize() && !other.dataSize());}
    Command(uint8_t opcode): Buffer(64) { write(0, opcode); }
    template<class T>
    Command&& operator+(const T& val)
    {
        append(val);
        return std::move(*this);
    }
    Command&& operator+(const Message& msg)
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
    megaHandle mPingTimer = 0;
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
    void reset();
    bool sendCommand(Command&& cmd);
    void rejoinExistingChats();
    void resendPending();
    void join(const Id& chatid);
    void hist(const Id& chatid, long count);
    void execCommand(const StaticBuffer& buf);
    void msgSend(const Id& chatid, const Message& message);
    void msgUpdate(const Id& chatid, const Message& message);
    friend class Client;
    friend class Messages;
public:
    ~Connection()
    {
        if (mPingTimer)
            ::mega::cancelInterval(mPingTimer);
    }
};

// message storage subsystem
// the message buffer can grow in two directions and is always contiguous, i.e. there are no "holes"
// there is no guarantee as to ordering
class Messages
{
protected:
    Connection& mConnection;
    Id mChatId;
    uint32_t mForwardStart;
    std::vector<Message*> mForwardList;
    std::vector<Message*> mBackwardList;
    std::map<Id, size_t> mSending;
    std::map<size_t, Message*> mModified;
    std::map<Id, size_t> mIdToIndexMap;
    Id mLastReceivedId;
    size_t mLastReceivedIdx = 0;
    Id mLastSeenId;
    size_t mLastSeenIdx = 0;
    std::unique_ptr<promise::Promise<void>> mJoinPromise;
    Messages(Connection& conn, const Id& chatid, size_t lowest)
        : mConnection(conn), mChatId(chatid), mForwardStart(lowest),
          mJoinPromise(new promise::Promise<void>) {}
    void push_forward(Message* msg) { mForwardList.push_back(msg); }
    void push_back(Message* msg) { mBackwardList.push_back(msg); }
    Message* first() const { return mBackwardList.front(); }
    Message* last() const { return mForwardList.front(); }
    bool empty() const { return mForwardList.empty() && mBackwardList.empty();}
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
    bool confirm(const Id& msgxid, const Id& msgid);
    size_t store(bool isNew, const Id& msgid, const Id& userid, uint32_t timestamp,
               const char* msg, size_t msglen);
    void onMsgUpdCommand(const Id& msgid, const char* msgdata, size_t msglen);
    bool checkFetchHistory(const Id& chatid, const Id& msgid);
    void onLastReceived(const Id& msgid);
    void onLastSeen(const Id& msgid);
    friend class Connection;
    friend class Client;

public:
    ~Messages() { clear(); }
    size_t lownum() const { return mForwardStart - mBackwardList.size(); }
    size_t highnum() const { return mForwardStart + mForwardList.size()-1;}
    inline Message* findOrNull(size_t num) const
    {
        if (num < mForwardStart) //look in mBackwardList
        {
            size_t idx = mForwardStart - num - 1; //always >= 0
            if (idx >= mBackwardList.size())
                return nullptr;
            return mBackwardList[idx];
        }
        else
        {
            size_t idx = num - mForwardStart;
            if (idx >= mForwardList.size())
                return nullptr;
            return mForwardList[idx];
        }
    }
    Message& at(size_t num) const
    {
        Message* msg = findOrNull(num);
        if (!msg)
        {
            throw std::runtime_error("Messages::operator[num]: num is outside of [lownum:highnum] range");
        }
        return *msg;
    }

    Message& operator[](size_t num) const { return at(num); }
    bool hasNum(size_t num) const
    {
        if (num < mForwardStart)
            return (mForwardStart - num <= mBackwardList.size());
        else
            return (num < mForwardStart + mForwardList.size());
    }
    bool isMsgSeen(size_t idx) const { return idx <= mLastSeenIdx; }
    bool isMsgReceived(size_t idx) const { return idx <= mLastReceivedIdx; }
    bool isMsgSending(size_t idx) const { return mSending.find(at(idx).id) == mSending.end(); }
    size_t submit(const char* msg, size_t msglen);
    void modify(size_t msgnum, const char* msgdata, size_t msglen);
    void resendPending();
    void range();

};

class Client
{
protected:
/// maps the chatd shard number to its corresponding Shard connection
    std::map<int, std::shared_ptr<Connection>> mConnections;
/// maps a chatid to the handling Shard connection
    std::map<Id, Connection*> mConnectionForChatId;
/// maps chatids to the Message object
    std::map<Id, std::shared_ptr<Messages>> mMessagesForChatId;
    Id mUserId;
    Id mMsgTransactionId;
    static bool sWebsockCtxInitialized;
    uint32_t mOptions = 0;
    void sendCommand(const Id& chatid, Command&& cmd);
    Connection& getOrCreateConnection(const Id& chatid, int shardNo, const std::string& url, bool& isNew);
    Connection& chatidConn(const Id& chatid)
    {
        auto it = mConnectionForChatId.find(chatid);
        if (it == mConnectionForChatId.end())
            throw std::runtime_error("chatidConn: Unknown chatid "+chatid);
        return *it->second;
    }
    Messages& chatidMessages(const Id& chatid) const
    {
        auto it = mMessagesForChatId.find(chatid);
        if (it == mMessagesForChatId.end())
            throw std::runtime_error("chatidMessages: Unknown chatid "+chatid);
        return *it->second;
    }
    void range(const Id& chatid);
    const Id& nextTransactionId() { mMsgTransactionId.val++; return mMsgTransactionId; }
    void onMsgUpdCommand(const Id& chatid, const Id& msgid, const char* msg, size_t msglen);
    bool checkFetchHistory(const Id& chatid, const Id& msgid);
    void msgConfirm(const Id& msgxid, const Id& msgid);
    size_t msgStore(bool isNew, const Id& chatid, const Id& msgid, const Id& userid, uint32_t ts,
                  const char* msg, size_t msglen);
public:
    static ws_base_s sWebsocketContext;
    size_t pingIntervalSec = 30;
    Client(const Id& userId, uint32_t options);
    ~Client(){}

    void getHistory(const Id& chatid, int count);
    promise::Promise<void> join(const Id& chatid, int shardNo, const std::string& url);
    promise::Promise<void>& joinPromise(const Id& chatid) const { return *chatidMessages(chatid).mJoinPromise;}
    size_t msgSubmit(const Id& chatid, const char* msg, size_t msglen);
    size_t msgSubmit(const Id& chatid, const std::string& msg) { return msgSubmit(chatid, msg.c_str(), msg.size()); }
    friend class Connection;
    friend class Messages;
};


}
namespace std
{
    template<>
    struct hash<chatd::Id> { size_t operator()(const chatd::Id& id) const { return hash<uint64_t>()(id.val); } };
}

#endif





