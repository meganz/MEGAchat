#include "chatd.h"
#include <base/cservices.h>
#include <gcmpp.h>
#include <retryHandler.h>
#include<libws_log.h>
#include <event2/dns.h>

using namespace std;
using namespace promise;

//#define CHATD_LOG_LISTENER_CALLS

#ifdef CHATD_LOG_LISTENER_CALLS
#define CHATD_LOG_LISTENER_CALL(fmtString,...) CHATD_LOG_DEBUG(fmtString, ##__VA_ARGS__)
#else
#define CHATD_LOG_LISTENER_CALL(...)
#endif

#define CALL_LISTENER(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
          CHATD_LOG_LISTENER_CALL("calling " #methodName "()");                               \
          mListener->methodName(__VA_ARGS__);                                                   \
      } catch(std::exception& e) {                                                              \
          CHATD_LOG_WARNING("Exception thrown from Listener::" #methodName "() app handler:\n%s", e.what());\
      }                                                                                         \
    } while(0)

namespace chatd
{

// message storage subsystem
// the message buffer can grow in two directions and is always contiguous, i.e. there are no "holes"
// there is no guarantee as to ordering

ws_base_s Client::sWebsocketContext;
bool Client::sWebsockCtxInitialized = false;

Client::Client(const Id& userId, uint32_t options)
:mUserId(userId), mOptions(options)
{
    if (!sWebsockCtxInitialized)
    {
        ws_global_init(&sWebsocketContext, services_get_event_loop(), services_dns_eventbase,
        [](struct bufferevent* bev, void* userp)
        {
            ::mega::marshallCall([bev, userp]()
            {
                //CHATD_LOG_DEBUG("Read event");
                ws_read_callback(bev, userp);
            });
        },
        [](struct bufferevent* bev, short events, void* userp)
        {
            ::mega::marshallCall([bev, events, userp]()
            {
                //CHATD_LOG_DEBUG("Buffer event 0x%x", events);
                ws_event_callback(bev, events, userp);
            });
        },
        [](int fd, short events, void* userp)
        {
            ::mega::marshallCall([events, userp]()
            {
                //CHATD_LOG_DEBUG("Timer %p event", userp);
                ws_handle_marshall_timer_cb(0, events, userp);
            });
        });
//        ws_set_log_cb(ws_default_log_cb);
//        ws_set_log_level(LIBWS_TRACE);
        sWebsockCtxInitialized = true;
    }
    /// random starting point for the new message transaction ID
    /// FIXME: use cryptographically strong PRNG instead
    /// CHECK: is this sufficiently collision-proof? a collision would have to occur in the same second for the same userid.
//    for (int i = 0; i < 8; i++)
//    {
//        static_cast<uint8_t*>(&mMsgTransactionId.val)[i] = rand() & 0xFF;
//    }
}

#define checkLibwsCall(call, opname) \
    do {                             \
        int _cls_ret = (call);       \
        if (_cls_ret) throw std::runtime_error("Websocket error " +std::to_string(_cls_ret) + \
        " on operation " #opname);   \
    } while(0)

//Stale event from a previous connect attempt?
#define ASSERT_NOT_ANOTHER_WS(event)    \
    if (ws != self->mWebSocket) {       \
        CHATD_LOG_WARNING("Websocket '" event "' callback: ws param is not equal to self->mWebSocket, ignoring"); \
    }

// add a new chatd shard
Promise<void> Client::join(const Id& chatid, int shardNo, const std::string& url, Listener* listener,
                        unsigned histFetchCount)
{
    auto msgsit = mMessagesForChatId.find(chatid);
    if (msgsit != mMessagesForChatId.end())
    {
        return *msgsit->second->mJoinPromise;
    }

    // instantiate a Connection object for this shard if needed
    Connection* conn;
    bool isNew;
    auto it = mConnections.find(shardNo);
    if (it == mConnections.end())
    {
        isNew = true;
        conn = new Connection(*this, shardNo);
        mConnections.emplace(std::piecewise_construct, std::forward_as_tuple(shardNo),
                             std::forward_as_tuple(conn));
    }
    else
    {
        isNew = false;
        conn = it->second.get();
    }

    conn->mUrl.parse(url);
    // map chatid to this shard
    mConnectionForChatId[chatid] = conn;

    // add chatid to the connection's chatids
    conn->mChatIds.insert(chatid);
    // always update the URL to give the API an opportunity to migrate chat shards between hosts
    Messages* msgs = new Messages(*conn, chatid, listener, histFetchCount);
    mMessagesForChatId.emplace(std::piecewise_construct, std::forward_as_tuple(chatid),
                               std::forward_as_tuple(msgs));

    // attempt a connection ONLY if this is a new shard.
    if(isNew)
    {
        conn->reconnect();
    }
    else
    {
        msgs->join();
        msgs->range();
    }
    return *msgs->mJoinPromise;
}

void Connection::websockConnectCb(ws_t ws, void* arg)
{
    Connection* self = static_cast<Connection*>(arg);
    ASSERT_NOT_ANOTHER_WS("connect");
    CHATD_LOG_DEBUG("Chatd connected");

    assert(self->mConnectPromise);
    self->mConnectPromise->resolve();
}

void Url::parse(const std::string& url)
{
    protocol.clear();
    port = 0;
    host.clear();
    path.clear();
    size_t ss = url.find("://");
    if (ss != std::string::npos)
    {
        protocol = url.substr(0, ss);
        std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);
        ss += 3;
    }
    else
    {
        ss = 0;
        protocol = "http";
    }
    char last = protocol[protocol.size()-1];
    isSecure = (last == 's');

    size_t i = ss;
    for (; i<url.size(); i++)
    {
        char ch = url[i];
        if (ch == ':') //we have port
        {
            size_t ps = i+1;
            host = url.substr(ss, i-ss);
            for (; i<url.size(); i++)
            {
                ch = url[i];
                if ((ch == '/') || (ch == '?'))
                {
                    break;
                }
            }
            port = std::stol(url.substr(ps, i-ps));
            break;
        }
        else if ((ch == '/') || (ch == '?'))
            break;
    }

    host = url.substr(ss, i-ss);

    if (i < url.size()) //not only host and protocol
    {
        //i now points to '/' or '?' and host and port must have been set
        path = (url[i] == '/') ? url.substr(i+1) : url.substr(i); //ignore the leading '/'
    }
    if (!port)
    {
        port = getPortFromProtocol();
    }
    assert(!host.empty());
}

uint16_t Url::getPortFromProtocol() const
{
    if ((protocol == "http") || (protocol == "ws"))
        return 80;
    else if ((protocol == "https") || (protocol == "wss"))
        return 443;
    else
        return 0;
}
void Connection::websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
                                size_t reason_len, void *arg)
{
    auto self = static_cast<Connection*>(arg);
    ASSERT_NOT_ANOTHER_WS("close/error");
    try
    {
        CHATD_LOG_DEBUG("Socket close on connection to shard %d. Reason: %.*s",
                        self->mShardNo, reason_len, reason);
        if (errtype == WS_ERRTYPE_DNS)
        {
            CHATD_LOG_DEBUG("DNS error: forcing libevent to re-read /etc/resolv.conf");
            //if we didn't have our network interface up at app startup, and resolv.conf is
            //genereated dynamically, dns may never work unless we re-read the resolv.conf file
#ifndef _WIN32
            evdns_base_clear_host_addresses(services_dns_eventbase);
            evdns_base_resolv_conf_parse(services_dns_eventbase,
                DNS_OPTIONS_ALL & (~DNS_OPTION_SEARCH), "/etc/resolv.conf");
#else
        evdns_config_windows_nameservers(services_dns_eventbase);
#endif
        }
        self->onSocketClose();
    }
    catch(std::exception& e)
    {
        CHATD_LOG_ERROR("Exception in websocket close callback: %s", e.what());
    }
}

void Connection::onSocketClose()
{
    if (mPingTimer)
    {
        ::mega::cancelInterval(mPingTimer);
        mPingTimer = 0;
    }
    for (auto& chatid: mChatIds)
    {
        auto& msgs = mClient.chatidMessages(chatid);
        auto& pms = *msgs.mJoinPromise;
// If promise is not done, we were not joined successfully so far, and we may be retrying.
// In that case we shouldn't re-new the promise. However if the promise is done(),
// user will have the promise resolved even though we are not connected
        if (pms.done())
        {
            msgs.mJoinPromise.reset(new promise::Promise<void>);
        }
        msgs.setOnlineState(kChatStateOffline);
    }

    auto pms = mConnectPromise.get();
    if (pms && !pms->done())
    {
        pms->reject(getState(), 0x3e9a4a1d);
    }
    pms = mDisconnectPromise.get();
    if (pms && !pms->done())
    {
        pms->resolve();
    }
}

Promise<void> Connection::reconnect()
{
    int state = getState();
    if ((state == WS_STATE_CONNECTING) || (state == WS_STATE_CONNECTED))
    {
        throw std::runtime_error("Connection::reconnect: Already connected/connecting");
    }
    return
    ::mega::retry([this](int no)
    {
        reset();
        CHATD_LOG_DEBUG("Chatd connecting to shard %d...", mShardNo);
        checkLibwsCall((ws_init(&mWebSocket, &Client::sWebsocketContext)), "create socket");
        ws_set_onconnect_cb(mWebSocket, &websockConnectCb, this);
        ws_set_onclose_cb(mWebSocket, &websockCloseCb, this);
        ws_set_onmsg_cb(mWebSocket,
            [](ws_t ws, char *msg, uint64_t len, int binary, void *arg)
            {
                Connection* self = static_cast<Connection*>(arg);
                ASSERT_NOT_ANOTHER_WS("message");
                self->execCommand(StaticBuffer(msg, len));
            }, this);

        mConnectPromise.reset(new Promise<void>);
        if (mUrl.isSecure)
        {
             ws_set_ssl_state(mWebSocket, LIBWS_SSL_SELFSIGNED);
        }
        for (auto& chatid: mChatIds)
        {
            mClient.chatidMessages(chatid).setOnlineState(kChatStateConnecting);
        }
        checkLibwsCall((ws_connect(mWebSocket, mUrl.host.c_str(), mUrl.port, (mUrl.path).c_str())), "connect");
        return *mConnectPromise;
    }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL)
    .then([this]()
    {
        rejoinExistingChats();
        resendPending();
        if (!mPingTimer)
        {
            mPingTimer = ::mega::setInterval([this]()
            {
                if (!isOnline())
                    return;
                sendCommand(Command(OP_KEEPALIVE));
            }, mClient.pingIntervalSec*1000);
        }
    });
}

Promise<void> Connection::disconnect()
{
    if (!mWebSocket)
    {
        return promise::_Void();
    }
    if (!mDisconnectPromise)
    {
        mDisconnectPromise.reset(new Promise<void>);
    }
    ws_close(mWebSocket);
    return *mDisconnectPromise;
}

void Connection::reset()
{
    if (!mWebSocket)
    {
        return;
    }
    CHATD_LOG_DEBUG("Reset connection to shard %d", mShardNo);
    ws_close_immediately(mWebSocket);
    ws_destroy(&mWebSocket);
    assert(!mWebSocket);
}

bool Connection::sendCommand(Command&& cmd)
{
    if (!isOnline())
        return false;
    auto opcode = cmd.opcode();
//WARNING: ws_send_msg_ex() is destructive to the buffer - it applies the websocket mask directly
    auto rc = ws_send_msg_ex(mWebSocket, cmd.buf(), cmd.dataSize(), 1);
    bool result = (!rc && isOnline());
    if (result)
    {
        CHATD_LOG_DEBUG("SENT %s, len %zu", Command::opcodeToStr(opcode), cmd.dataSize());
    }
    else
    {
        CHATD_LOG_DEBUG("Can't send %s, len %zu - we are offline", Command::opcodeToStr(opcode), cmd.dataSize());
    }

    cmd.free(); //just in case, as it's content is xor-ed with the websock datamask so it's unusable
    return result;
}

// rejoin all open chats after reconnection (this is mandatory)
void Connection::rejoinExistingChats()
{
    for (auto& chatid: mChatIds)
    {
        // rejoin chat and immediately set the locally buffered message range
        Messages& msgs = mClient.chatidMessages(chatid);
//        msgs.mJoinPromise.reset(new Promise<void>);
        msgs.join();
        msgs.range();
    }
}

// resend all unconfirmed messages (this is mandatory)
void Connection::resendPending()
{
    for (auto& chatid: mChatIds)
    {
        mClient.chatidMessages(chatid).resendPending();
    }
}

// send JOIN
void Messages::join()
{
    setOnlineState(kChatStateJoining);
    mConnection.sendCommand(Command(OP_JOIN) + mChatId + mClient.mUserId + (int8_t)PRIV_NOCHANGE);
}

bool Messages::getHistory(int count)
{
    if (mOldestKnownMsgId)
    {
        getHistoryFromDb(count);
        return false;
    }
    else
    {
        requestHistoryFromServer(-count);
        return true;
    }
}
void Messages::requestHistoryFromServer(int32_t count)
{
        mLastHistFetchCount = 0;
        mHistFetchState = kHistFetchingFromServer;
        mConnection.sendCommand(Command(OP_HIST) + mChatId + count);
}

Messages::Messages(Connection& conn, const Id& chatid, Listener* listener, unsigned histFetchCount)
    : mConnection(conn), mClient(conn.mClient), mChatId(chatid),
      mJoinPromise(new promise::Promise<void>), mListener(listener)
{
    assert(listener);
    Idx newestDbIdx;
    //we don't use CALL_LISTENER here because if init() throws, then something is wrong and we should not continue
    listener->init(this, this, mOldestKnownMsgId, mNewestKnownMsgId, newestDbIdx);
    if (!mOldestKnownMsgId)
    {
        CHATD_LOG_DEBUG("App says there is no local history");
        mForwardStart = CHATD_IDX_RANGE_MIDDLE;
        mNewestKnownMsgId = Id::null();
    }
    else
    {
        assert(mNewestKnownMsgId); assert(newestDbIdx != CHATD_IDX_INVALID);
        mForwardStart = newestDbIdx + 1;
        CHATD_LOG_DEBUG("App has a local history range of %s - %s (%zu)",
            mOldestKnownMsgId.toString().c_str(), mNewestKnownMsgId.toString().c_str(), mForwardStart);
        if (histFetchCount)
            mInitialHistoryFetchCount = histFetchCount;
        CHATD_LOG_DEBUG("Loading local history");
        getHistoryFromDb(mInitialHistoryFetchCount);
    }
}

void Messages::getHistoryFromDb(unsigned count)
{
    assert(mOldestKnownMsgId);
    mHistFetchState = kHistFetchingFromDb;
    std::vector<Message*> messages;
    CALL_LISTENER(fetchDbHistory, lownum()-1, count, messages);
    mLastHistFetchCount = 0;
    for (auto msg: messages)
    {
        msgIncoming(false, msg, true); //increments mLastHistFetchCount
    }
    mHistFetchState = kHistNotFetching;
    CALL_LISTENER(onHistoryDone, true);
    if ((messages.size() < count) && mOldestKnownMsgId)
        throw std::runtime_error("Application says no more messages in db, but we still haven't seen specified oldest message id");
}

//These throw ParseError and will cause the loop to abort.
//These are critical errors that are not per-command

#define READ_ID(varname, offset)\
    assert(offset==pos-base); Id varname(buf.read<uint64_t>(pos)); pos+=sizeof(uint64_t)
#define READ_32(varname, offset)\
    assert(offset==pos-base); uint32_t varname(buf.read<uint32_t>(pos)); pos+= sizeof(uint32_t)
#define ID_CSTR(id) id.toString().c_str()

// inbound command processing
// multiple commands can appear as one WebSocket frame, but commands never cross frame boundaries
// CHECK: is this assumption correct on all browsers and under all circumstances?
void Connection::execCommand(const StaticBuffer &buf)
{
    size_t pos = 0;
//IMPORTANT: Increment pos before calling the command handler, because the handler may throw, in which
//case the next iteration will not advance and will execute the same command again, resulting in
//infinite loop
    while (pos < buf.dataSize())
    {
      char opcode = buf.buf()[pos];
      try
      {
        pos++;
#ifndef _NDEBUG
        size_t base = pos;
#endif
//        CHATD_LOG_DEBUG("RECV %s", Command::opcodeToStr(opcode));
        switch (opcode)
        {
            case OP_KEEPALIVE:
            {
                CHATD_LOG_DEBUG("Server heartbeat received");
                sendCommand(Command(OP_KEEPALIVE));
                break;
            }
            case OP_JOIN:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                int priv = buf.read<int8_t>(pos);
                pos++;
                CHATD_LOG_DEBUG("recv JOIN - user '%s' on '%s' with privilege level %d",
                                ID_CSTR(userid), ID_CSTR(chatid), priv);
                auto listener = mClient.chatidMessages(chatid).mListener;
                if (priv != PRIV_NOTPRESENT)
                    listener->onUserJoined(userid, priv);
                else
                    listener->onUserLeft(userid);
                break;
            }
            case OP_OLDMSG:
            case OP_NEWMSG:
            {
                bool isNewMsg = (opcode == OP_NEWMSG);
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                READ_32(ts, 24);
                READ_32(msglen, 28);
                const char* msg = buf.read(pos, msglen);
                pos += msglen;
                CHATD_LOG_DEBUG("recv %s: %s, user '%s' on chatid '%s' at time %u with len %u",
                    (isNewMsg ? "NEWMSG" : "OLDMSG"), ID_CSTR(msgid), ID_CSTR(userid),
                    ID_CSTR(chatid), ts, msglen);

                mClient.chatidMessages(chatid).msgIncoming(
                    isNewMsg, new Message(msgid, userid, ts, msg, msglen, nullptr), false);
                break;
            }
            case OP_SEEN:
            {
            //TODO: why do we test the whole buffer's len to determine the current command's len?
            //buffer may contain other commands following it
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                CHATD_LOG_DEBUG("recv SEEN on %s for user %s, msgid: %s",
                                ID_CSTR(chatid), ID_CSTR(userid), ID_CSTR(msgid));
                mClient.chatidMessages(chatid).onLastSeen(msgid);
                break;
            }
            case OP_RECEIVED:
            {
                READ_ID(chatid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("recv RECEIVED on %s, msgid: %s", ID_CSTR(chatid), ID_CSTR(msgid));
                mClient.chatidMessages(chatid).onLastReceived(msgid);
                break;
            }
            case OP_RETENTION:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_32(period, 16);
                CHATD_LOG_DEBUG("recv RETENTION on %s by user %s to %u second(s)",
                                ID_CSTR(chatid), ID_CSTR(userid), period);
                break;
            }
            case OP_MSGID:
            {
                READ_ID(msgxid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("recv MSGID: %s->%s", ID_CSTR(msgxid), ID_CSTR(msgid));
                mClient.msgConfirm(msgxid, msgid);
                break;
            }
            case OP_RANGE:
            {
                READ_ID(chatid, 0);
                READ_ID(oldest, 8);
                READ_ID(newest, 16);
                CHATD_LOG_DEBUG("recv RANGE on %s: oldest: %s, newest: %s",
                                ID_CSTR(chatid), ID_CSTR(oldest), ID_CSTR(newest));
                mClient.chatidMessages(chatid).initialFetchHistory(newest);
                break;
            }
            case OP_REJECT:
            {
                READ_ID(id, 0);
                READ_32(op, 8);
                READ_32(code, 12); //TODO: what's this code?
                CHATD_LOG_DEBUG("recv REJECT: id='%s', %d / %d", ID_CSTR(id), op, code);
                if (op == OP_NEWMSG)  // the message was rejected
                {
                    mClient.msgConfirm(id, Id::null());
                }
                else
                {
                    CHATD_LOG_WARNING("Something (not NEWMSG) was rejected, but not sure what, ignoring");
                }
                break;
            }
            case OP_HISTDONE:
            {
                READ_ID(chatid, 0);
                CHATD_LOG_DEBUG("recv HISTDONE: retrieval of chat '%s' finished", ID_CSTR(chatid));
                mClient.chatidMessages(chatid).onHistDone();
                break;
            }
            default:
                CHATD_LOG_ERROR("Unknown opcode %d, ignoring all subsequent comment", opcode);
                return;
        }
      }
      catch(BufferRangeError& e)
      {
            CHATD_LOG_ERROR("Buffer bound check error while parsing %s: %s\nAborting command processing", Command::opcodeToStr(opcode), e.what());
            return;
      }
      catch(std::exception& e)
      {
            CHATD_LOG_ERROR("Exception while processing incoming %s: %s", Command::opcodeToStr(opcode), e.what());
      }
    }
}

void Connection::msgSend(const Id& chatid, const Message& message)
{
    sendCommand(Command(OP_NEWMSG) + chatid + Id::null() + message.id + message.ts + message);
}

void Messages::onHistDone()
{
    assert(mHistFetchState == kHistFetchingFromServer);
    mHistFetchState = (mLastHistFetchCount > 0) ? kHistNotFetching : kHistNoMore;
    mListener->onHistoryDone(false);
    if(!mJoinPromise->done())
    {
//resolve only on the first HISTDONE - we may have other history requests afterwards
        mJoinPromise->resolve();
        setOnlineState(kChatStateOnline);
    }
}

Message* Messages::msgSubmit(const char* msg, size_t msglen, const Id& aMsgxid, void* userp)
{
    const Id& msgxid = aMsgxid ? aMsgxid : mClient.nextTransactionId();
    // write the new message to the message buffer and mark as in sending state
    Message* message = new Message(msgxid, mConnection.mClient.mUserId, time(NULL), msg, msglen, userp, true);
    assert(message->isSending());
    auto ret = mSending.insert(std::make_pair(msgxid, message));
    if (!ret.second)
        throw std::runtime_error("msgSubmit: provided msgxid is not unique");

    //if we believe to be online, send immediately
    //we don't sent a msgStatusChange event to the listener, as the GUI should initialize the
    //message's status with something already, so it's redundant.
    //The GUI should by default show it as sending
    if (mConnection.isOnline())
    {
        mConnection.msgSend(mChatId, *message);
    }
    return message;
}

void Messages::onLastReceived(const Id& msgid)
{
    mLastReceivedId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    { // we don't have that message in the buffer yet, so we don't know its index
        mLastReceivedIdx = CHATD_IDX_INVALID;
        return;
    }
    if (at(it->second).userid != mClient.mUserId)
    {
        CHATD_LOG_WARNING("Last-received pointer points to a message by a peer, possibly the pointer was set incorrectly");
    }
    Idx notifyOldest;
    if (mLastReceivedIdx != CHATD_IDX_INVALID) //we have a previous last-received index, notify user about received messages
    {
        auto idx = it->second;
        if (mLastReceivedIdx > idx)
        {
            CHATD_LOG_ERROR("onLastReceived: Tried to set the index to an older message, ignoring");
            CHATD_LOG_DEBUG("highnum() = %zu, mLastReceivedIdx = %zu, idx = %zu", highnum(), mLastReceivedIdx, idx);
            return;
        }
        notifyOldest = mLastReceivedIdx;
        mLastReceivedIdx = idx;
    } //no mLastReceivedIdx
    else
    {
        mLastReceivedIdx = it->second;
        notifyOldest = lownum();
    }
    for (Idx i=notifyOldest; i<=mLastReceivedIdx; i++)
    {
        auto& msg = at(i);
        if (msg.userid == mClient.mUserId)
        {
            mListener->onMessageStatusChange(i, Message::kDelivered, msg);
        }
    }
}

void Messages::onLastSeen(const Id& msgid)
{
    mLastSeenId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        mLastSeenIdx = CHATD_IDX_INVALID;
        return;
    }
    if (at(it->second).userid == mClient.mUserId)
    {
        CHATD_LOG_WARNING("Last-seen points to a message by us, possibly the pointer was not set properly");
    }
    Idx notifyOldest;
    if (mLastSeenIdx != CHATD_IDX_INVALID)
    {
        auto idx = it->second;
        if (idx < mLastSeenIdx)
            throw std::runtime_error("onLastSeen: Can't set last seen index to an older message");
        assert(idx != mLastSeenIdx); //we were called redundantly - the msgid and index were already set and the same. This will not hurt, except for generating one redundant onMessageStateChange for the message that is pointed to by the index
        notifyOldest = mLastSeenIdx;
        mLastSeenIdx = idx;
    }
    else
    {
        mLastSeenIdx = it->second;
        notifyOldest = lownum();
    }
    for (Idx i=notifyOldest; i<=mLastSeenIdx; i++)
    {
        auto& msg = at(i);
        if (msg.userid != mClient.mUserId)
        {
            mListener->onMessageStatusChange(i, Message::kSeen, msg);
        }
    }
}
bool Messages::setMessageSeen(Idx idx)
{
    assert(idx != CHATD_IDX_INVALID);
    if (idx <= mLastSeenIdx)
    {
        CHATD_LOG_DEBUG("Attempted to move the last-seen pointer backward, ignoring");
        return false;
    }
    auto& msg = at(idx);
    if (msg.userid == mClient.mUserId)
    {
        CHATD_LOG_DEBUG("Asked to mark own message %s as seen, ignoring", ID_CSTR(msg.id));
        return false;
    }
    mConnection.sendCommand(Command(OP_SEEN) + mChatId + mClient.mUserId + msg.id);
    for (Idx i=mLastSeenIdx; i<=idx; i++)
    {
        auto& m = at(i);
        if (m.userid != mClient.mUserId)
        {
            mListener->onMessageStatusChange(i, Message::kSeen, m);
        }
    }
    return true;
}

void Messages::resendPending()
{
    // resend all pending new messages
    for (auto& item: mSending)
    {
        assert(item.second->isSending());
        mConnection.msgSend(mChatId, *item.second);
    }
}

// after a reconnect, we tell the chatd the oldest and newest buffered message
void Messages::range()
{
    if (mOldestKnownMsgId)
    {
        CHATD_LOG_DEBUG("Sending RANGE based on app db: %s - %s", mOldestKnownMsgId.toString().c_str(), mNewestKnownMsgId.toString().c_str());
        mConnection.sendCommand(Command(OP_RANGE) + mChatId + mOldestKnownMsgId + mNewestKnownMsgId);
        return;
    }
    if (empty())
        return;

    auto highest = highnum();
    Idx i = lownum();
    for (; i<=highest; i++)
    {
        auto& msg = at(i);
        if (msg.isSending())
        {
            i--;
            break;
        }
    }
    if (i < lownum()) //we have only unsent messages
        return;
    CHATD_LOG_DEBUG("Sending RANGE calculated from memory buffer: %s - %s", at(lownum()).id.toString().c_str(), at(i).id.toString().c_str());
    mConnection.sendCommand(Command(OP_RANGE) + mChatId + at(lownum()).id + at(i).id);
}

void Client::msgConfirm(const Id& msgxid, const Id& msgid)
{
    // CHECK: is it more efficient to keep a separate mapping of msgxid to messages?
    for (auto& messages: mMessagesForChatId)
    {
        if (messages.second->confirm(msgxid, msgid))
            return;
    }
    throw std::runtime_error("msgConfirm: Unknown msgxid "+msgxid);
}

// msgid can be 0 in case of rejections
Idx Messages::confirm(const Id& msgxid, const Id& msgid)
{
    auto it = mSending.find(msgxid);
    if (it == mSending.end())
        return false;

    Message* msg(it->second);
    mSending.erase(it);
    assert(msg->isSending());
    msg->setIsSending(false);

    if (!msgid)
    {
        CALL_LISTENER(onMessageRejected, msgxid);
        delete msg;
        return 0;
    }
    //update transaction id to the actual msgid
    msg->id = msgid;
    push_forward(msg);
    auto idx = mIdToIndexMap[msgid] = highnum();
    CALL_LISTENER(onMessageConfirmed, msgxid, msgid, idx);
    return idx;
}
Message::Status Messages::getMsgStatus(Idx idx, const Id& userid)
{
    assert(idx != CHATD_IDX_INVALID);
    if (userid == mClient.mUserId)
    {
        auto& msg = at(idx);
        if (msg.isSending())
            return Message::kSending;
        else if (idx <= mLastReceivedIdx)
            return Message::kDelivered;
        else
        {
            return Message::kServerReceived;
        }
    } //message is from a peer
    else
    {
        return (idx <= mLastSeenIdx) ? Message::kSeen : Message::kNotSeen;
    }
}

Idx Messages::msgIncoming(bool isNew, Message* message, bool isLocal)
{
    assert((isLocal && !isNew) || !isLocal);
    auto msgid = message->id;
    assert(msgid);
    Idx idx;

    if (isNew)
    {
        push_forward(message);
        if (mOldestKnownMsgId)
            mNewestKnownMsgId = msgid; //expand the range with newer network-received messages, we don't fetch new messages from db
        idx = highnum();
    }
    else
    {
        push_back(message);
        assert(mHistFetchState & kHistFetchingFlag);
        mLastHistFetchCount++;
        if (msgid == mOldestKnownMsgId) //we have a user-set range, and we have just added the oldest message that we know we have (maybe in db), the user range is no longer up-to-date, so disable it
        {
            mOldestKnownMsgId = 0;
            mNewestKnownMsgId = 0;
        }
        idx = lownum();
    }
    mIdToIndexMap[msgid] = idx;
    if ((message->userid != mClient.mUserId) && !isLocal)
    {
        mConnection.sendCommand(Command(OP_RECEIVED) + mChatId + msgid);
    }
    if (isNew)
        CALL_LISTENER(onRecvNewMessage, idx, *message, (getMsgStatus(idx, message->userid)));
    else
        CALL_LISTENER(onRecvHistoryMessage, idx, *message, (getMsgStatus(idx, message->userid)), isLocal);

    //normally the indices will not be set if mLastXXXId == msgid, as there will be only
    //one chance to set the idx (we receive the msg only once).
    if (msgid == mLastSeenId) //we didn't have the message when we received the last seen id
    {
        assert(mLastSeenIdx == CHATD_IDX_INVALID);
        CHATD_LOG_DEBUG("Received the message with the last-seen msgid, setting the index pointer to it");
        onLastSeen(msgid);
    }
    if (mLastReceivedId == msgid)
    {
        assert(mLastReceivedIdx == CHATD_IDX_INVALID);
        //we didn't have the message when we received the last received msgid pointer,
        //and now we just received the message - set the index pointer
        CHATD_LOG_DEBUG("Received the message with the last-received msgid, setting the index pointer to it");
        onLastReceived(msgid);
    }
    return idx;
}

void Messages::initialFetchHistory(const Id& serverNewest)
{
//we have messages in the db, fetch the newest from there
    if (empty())
    {
        assert(!mOldestKnownMsgId);
        //we don't have messages in db, and we don't have messages in memory
        //we haven't sent a RANGE, so we can get history only backwards
        requestHistoryFromServer(-mInitialHistoryFetchCount);
    }
    else
    {
        if (at(highnum()).id != serverNewest)
        {
            CHATD_LOG_DEBUG("There are new messages on the server, requesting them");
//the server has more recent msgs than the most recent in our db, retrieve all newer ones, after our RANGE
            requestHistoryFromServer(0x0fffffff);
        }
        else
        {
            assert(!mJoinPromise->done());
            mJoinPromise->resolve();
            setOnlineState(kChatStateOnline);
        }
    }
}


static char b64enctable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
std::string base64urlencode(const void *data, size_t inlen)
{
    std::string encoded_data;
    encoded_data.reserve(((inlen+2) / 3) * 4);
    for (int i = 0; i < inlen;)
    {
        uint8_t octet_a = i < inlen ? static_cast<const char*>(data)[i++] : 0;
        uint8_t octet_b = i < inlen ? static_cast<const char*>(data)[i++] : 0;
        uint8_t octet_c = i < inlen ? static_cast<const char*>(data)[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data+= b64enctable[(triple >> 18) & 0x3F];
        encoded_data+= b64enctable[(triple >> 12) & 0x3F];
        encoded_data+= b64enctable[(triple >> 6) & 0x3F];
        encoded_data+= b64enctable[triple & 0x3F];
    }
    int mod = inlen % 3;
    if (mod)
    {
        encoded_data.resize(encoded_data.size() - (3 - mod));
    }
    return encoded_data;
}

static const unsigned char b64dectable[] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,62, 255, 62,255, 63,
    52,  53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,255,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15,  16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255, 63,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41,  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

size_t base64urldecode(const char* str, size_t len, void* bin, size_t binlen)
{
    if (binlen < (len*3)/4)
        throw std::runtime_error("Insufficient output buffer space");

    const unsigned char* last = (const unsigned char*)str+len-1;
    const unsigned char* in = (const unsigned char*)str;
    unsigned char* out = (unsigned char*)bin;
    for(;in <= last;)
    {
        unsigned char one = b64dectable[*in++];
        if (one > 63)
            throw std::runtime_error(std::string("Invalid char in base64 stream at offset") + std::to_string((char*)in - str));

        unsigned char two = b64dectable[*in++];
        if (two > 63)
            throw std::runtime_error(std::string("Invalid char in base64 stream at offset") + std::to_string((char*)in - str));

        *out++ = (one << 2) | (two >> 4);
        if (in > last)
            break;

        unsigned char three = b64dectable[*in++];
        if (three > 63)
            throw std::runtime_error(std::string("Invalid char in base64 stream at offset") + std::to_string((char*)in - str));
        *out++ = (two << 4) | (three >> 2);

        if (in > last)
            break;

        unsigned char four = b64dectable[*in++];
        if (four > 63)
            throw std::runtime_error(std::string("Invalid char in base64 stream at offset") + std::to_string((char*)in - str));

        *out++ = (three << 6) | four;
    }
    return out-(unsigned char*)bin;
}

const char* Command::opcodeNames[] =
{
 "KEEPALIVE","JOIN", "OLDMSG", "NEWMSG", "MSGUPD(should not be used)","SEEN",
 "RECEIVED","RETENTION","HIST", "RANGE","MSGID","REJECT",
 "BROADCAST", "HISTDONE"
};
const char* Message::statusNames[] =
{
  "Sending", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

}
