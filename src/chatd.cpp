#include "chatd.h"
#include <base/cservices.h>
#include <gcmpp.h>
#include <retryHandler.h>
#include<libws_log.h>
#include <event2/dns.h>
#include "base64.h"
#include <algorithm>

using namespace std;
using namespace promise;

#define CHATD_LOG_LISTENER_CALLS //DB_CALLS

#define ID_CSTR(id) id.toString().c_str()

// logging for a specific chatid - prepends the chatid and calls the normal logging macro
#define CHATID_LOG_DEBUG(fmtString,...) CHATD_LOG_DEBUG("%s: " fmtString, ID_CSTR(chatId()), ##__VA_ARGS__)
#define CHATID_LOG_WARNING(fmtString,...) CHATD_LOG_WARNING("%s: " fmtString, ID_CSTR(chatId()), ##__VA_ARGS__)
#define CHATID_LOG_ERROR(fmtString,...) CHATD_LOG_ERROR("%s: " fmtString, ID_CSTR(chatId()), ##__VA_ARGS__)

#ifdef CHATD_LOG_LISTENER_CALLS
    #define CHATD_LOG_LISTENER_CALL(fmtString,...) CHATID_LOG_DEBUG(fmtString, ##__VA_ARGS__)
#else
    #define CHATD_LOG_LISTENER_CALL(...)
#endif

#ifdef CHATD_LOG_CRYPTO_CALLS
    #define CHATD_LOG_CRYPTO_CALL(fmtString,...) CHATID_LOG_DEBUG(fmtString, ##__VA_ARGS__)
#else
    #define CHATD_LOG_CRYPTO_CALL(...)
#endif

#ifdef CHATD_LOG_DB_CALLS
    #define CHATD_LOG_DB_CALL(fmtString,...) CHATID_LOG_DEBUG(fmtString, ##__VA_ARGS__)
#else
    #define CHATD_LOG_DB_CALL(...)
#endif

#define CALL_LISTENER(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
          CHATD_LOG_LISTENER_CALL("Calling Listener::" #methodName "()");                       \
          mListener->methodName(__VA_ARGS__);                                                   \
      } catch(std::exception& e) {                                                              \
          CHATD_LOG_WARNING("Exception thrown from Listener::" #methodName "():\n%s", e.what());\
      }                                                                                         \
    } while(0)

#define CALL_CRYPTO(methodName,...)                                                             \
    do {                                                                                        \
      try {                                                                                     \
          CHATD_LOG_CRYPTO_CALL("Calling ICrypto::" #methodName "()");                          \
          mCrypto->methodName(__VA_ARGS__);                                                     \
      } catch(std::exception& e) {                                                              \
          CHATD_LOG_WARNING("Exception thrown from ICrypto::" #methodName "():\n%s", e.what()); \
      }                                                                                         \
    } while(0)

#define CALL_DB(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
          CHATD_LOG_DB_CALL("Calling DbInterface::" #methodName "()");                               \
          mDbInterface->methodName(__VA_ARGS__);                                                   \
      } catch(std::exception& e) {                                                              \
          CHATID_LOG_ERROR("Exception thrown from DbInterface::" #methodName "():\n%s", e.what());\
      }                                                                                         \
    } while(0)

#ifndef CHATD_ASYNC_MSG_CALLBACKS
    #define CHATD_ASYNC_MSG_CALLBACKS 1
#endif

namespace chatd
{

// message storage subsystem
// the message buffer can grow in two directions and is always contiguous, i.e. there are no "holes"
// there is no guarantee as to ordering

ws_base_s Client::sWebsocketContext;
bool Client::sWebsockCtxInitialized = false;

Client::Client(const Id& userId)
:mUserId(userId)
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
void Client::join(const Id& chatid, int shardNo, const std::string& url, Listener* listener,
    ICrypto* crypto)
{
    auto msgsit = mChatForChatId.find(chatid);
    if (msgsit != mChatForChatId.end())
    {
        return;
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
    Chat* msgs = new Chat(*conn, chatid, listener, crypto);
    mChatForChatId.emplace(std::piecewise_construct, std::forward_as_tuple(chatid),
                               std::forward_as_tuple(msgs));

    // attempt a connection ONLY if this is a new shard.
    if(isNew)
    {
        conn->reconnect();
    }
    else if (conn->isOnline())
    {
        msgs->join();
        msgs->range();
    }
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
    if (url.empty())
        throw std::runtime_error("Url::Parse: Url is empty");
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
    if (host.empty())
        throw std::runtime_error("Url::parse: Invalid URL '"+url+"', host is empty");
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
        ::mega::marshallCall([self]()
        {
            //we don't want to initiate websocket reconnect from within a websocket callback
            self->onSocketClose();
        });
    }
    catch(std::exception& e)
    {
        CHATD_LOG_ERROR("Exception in websocket close callback: %s", e.what());
    }
}

void Connection::onSocketClose()
{
    CHATD_LOG_DEBUG("Socket to shard %d closed", mShardNo);
    disableInactivityTimer();
    for (auto& chatid: mChatIds)
    {
        mClient.chats(chatid).setOnlineState(kChatStateOffline);
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
    if (!mTerminating)
    {
        disconnect();
        reconnect();
    }
}

void Connection::disableInactivityTimer()
{
    if (mInactivityTimer)
    {
        ::mega::cancelInterval(mInactivityTimer);
        mInactivityTimer = 0;
    }
}

Promise<void> Connection::reconnect()
{
    int state = getState();
    if (state == WS_STATE_CONNECTING)
        throw std::runtime_error("Connection::reconnect: Already connecting");

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
                self->mInactivityBeats = 0;
                self->execCommand(StaticBuffer(msg, len));
            }, this);

        mConnectPromise.reset(new Promise<void>);
        if (mUrl.isSecure)
        {
             ws_set_ssl_state(mWebSocket, LIBWS_SSL_SELFSIGNED);
        }
        for (auto& chatid: mChatIds)
        {
            mClient.chats(chatid).setOnlineState(kChatStateConnecting);
        }
        checkLibwsCall((ws_connect(mWebSocket, mUrl.host.c_str(), mUrl.port, (mUrl.path).c_str())), "connect");
        return *mConnectPromise;
    }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL)
    .then([this]()
    {
        enableInactivityTimer();
        rejoinExistingChats();
    });
}
void Connection::enableInactivityTimer()
{
    if (mInactivityTimer)
        return;

    mInactivityTimer = ::mega::setInterval([this]()
    {
        if (mInactivityBeats++ > 3)
        {
            disableInactivityTimer();
            CHATD_LOG_WARNING("Connection to shard %d inactive for too long, reconnecting",
                mShardNo);
            reconnect();
        }
    }, 10000);
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
    ws_close_immediately(mWebSocket);
    ws_destroy(&mWebSocket);
    assert(!mWebSocket);
}

bool Connection::sendCommand(Command&& cmd)
{
    if (!isOnline())
        return false;
//WARNING: ws_send_msg_ex() is destructive to the buffer - it applies the websocket mask directly
    auto rc = ws_send_msg_ex(mWebSocket, cmd.buf(), cmd.dataSize(), 1);
    bool result = (!rc && isOnline());
    cmd.free(); //just in case, as it's content is xor-ed with the websock datamask so it's unusable
    return result;
}

// rejoin all open chats after reconnection (this is mandatory)
void Connection::rejoinExistingChats()
{
    for (auto& chatid: mChatIds)
    try
    {
        // rejoin chat and immediately set the locally buffered message range
        Chat& msgs = mClient.chats(chatid);
        msgs.join();
        msgs.range();
    }
    catch(std::exception& e)
    {
        CHATD_LOG_ERROR("%s: rejoinExistingChats: Exception: %s", chatid.toString().c_str(), e.what());
    }
}

bool Chat::sendCommand(Command&& cmd)
{
    auto opcode = cmd.opcode();
    bool ret = mConnection.sendCommand(std::move(cmd));
    if (ret)
        CHATID_LOG_DEBUG("send %s", Command::opcodeToStr(opcode));
    else
        CHATD_LOG_DEBUG("%s: Can't send %s, we are offline", ID_CSTR(mChatId), Command::opcodeToStr(opcode));
    return ret;
}

// send JOIN
void Chat::join()
{
//also reset handshake state, as we may be reconnecting
    mInitialFetchHistoryCalled = false;
    setOnlineState(kChatStateJoining);
    sendCommand(Command(OP_JOIN) + mChatId + mClient.mUserId + (int8_t)PRIV_NOCHANGE);
}

bool Chat::getHistory(int count)
{
    if (mOldestKnownMsgId) //we are within the db range
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
void Chat::requestHistoryFromServer(int32_t count)
{
        mLastHistFetchCount = 0;
        mHistFetchState = kHistFetchingFromServer;
        sendCommand(Command(OP_HIST) + mChatId + count);
}

Chat::Chat(Connection& conn, const Id& chatid, Listener* listener, ICrypto* crypto)
    : mConnection(conn), mClient(conn.mClient), mChatId(chatid), mListener(listener),
      mCrypto(crypto)
{
    assert(mChatId);
    assert(mListener);
    assert(mCrypto);
    mNextUnsent = mSending.begin();
    Idx newestDbIdx;
    //we don't use CALL_LISTENER here because if init() throws, then something is wrong and we should not continue
    mListener->init(*this, mDbInterface);
    mCrypto->init(*this);
    assert(mDbInterface);
    mDbInterface->getHistoryInfo(mOldestKnownMsgId, mNewestKnownMsgId, newestDbIdx);
    if (!mOldestKnownMsgId)
    {
        CHATID_LOG_DEBUG("Db has no local history for chat");
        mForwardStart = CHATD_IDX_RANGE_MIDDLE;
        mNewestKnownMsgId = Id::null();
    }
    else
    {
        assert(mNewestKnownMsgId); assert(newestDbIdx != CHATD_IDX_INVALID);
        mForwardStart = newestDbIdx + 1;
        CHATID_LOG_DEBUG("Db has local history: %s - %s (middle point: %u)",
            ID_CSTR(mChatId), ID_CSTR(mOldestKnownMsgId), ID_CSTR(mNewestKnownMsgId), mForwardStart);
        getHistoryFromDb(1); //to know if we have the latest message on server, we must at least load the latest db message
    }
}
Chat::~Chat()
{
    CALL_LISTENER(onDestroy); //we don't delete because it may have its own idea of its lifetime (i.e. it could be a GUI class)
    try { delete mCrypto; }
    catch(std::exception& e)
    { CHATID_LOG_ERROR("EXCEPTION from ICrypto destructor: %s", e.what()); }
    mCrypto = nullptr;
    clear();
    try { delete mDbInterface; }
    catch(std::exception& e)
    { CHATID_LOG_ERROR("EXCEPTION from DbInterface destructor: %s", e.what()); }
    mDbInterface = nullptr;
}

void Chat::getHistoryFromDb(unsigned count)
{
    assert(mOldestKnownMsgId); //we are within the db range
    mHistFetchState = kHistFetchingFromDb;
    std::vector<Message*> messages;
    CALL_DB(fetchDbHistory, lownum()-1, count, messages);
    mLastHistFetchCount = 0;
    for (auto msg: messages)
    {
        msgIncoming(false, msg, true); //increments mLastHistFetchCount
    }
    mHistFetchState = kHistNotFetching;
    CALL_CRYPTO(onHistoryDone);
    CALL_LISTENER(onHistoryDone, true);

    // If we haven't yet seen the message with the last-seen msgid, then all messages
    // in the buffer are unseen
    if (mLastSeenIdx == CHATD_IDX_INVALID) //msgIncoming calls onUnreadChanged only for encrypted messages
        CALL_LISTENER(onUnreadChanged);
    if ((messages.size() < count) && mOldestKnownMsgId)
        throw std::runtime_error("Db says it has no more messages, but we still haven't seen specified oldest message id");
}

#define READ_ID(varname, offset)\
    assert(offset==pos-base); Id varname(buf.read<uint64_t>(pos)); pos+=sizeof(uint64_t)
#define READ_32(varname, offset)\
    assert(offset==pos-base); uint32_t varname(buf.read<uint32_t>(pos)); pos+=4
#define READ_16(varname, offset)\
    assert(offset==pos-base); uint16_t varname(buf.read<uint16_t>(pos)); pos+=2

// inbound command processing
// multiple commands can appear as one WebSocket frame, but commands never cross frame boundaries
// CHECK: is this assumption correct on all browsers and under all circumstances?
void Connection::execCommand(const StaticBuffer& buf)
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
#ifndef NDEBUG
        size_t base = pos;
#endif
//        CHATD_LOG_DEBUG("RECV %s", Command::opcodeToStr(opcode));
        switch (opcode)
        {
            case OP_KEEPALIVE:
            {
                //CHATD_LOG_DEBUG("Server heartbeat received");
                sendCommand(Command(OP_KEEPALIVE));
                break;
            }
            case OP_JOIN:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                Priv priv = (Priv)buf.read<int8_t>(pos);
                pos++;
                CHATD_LOG_DEBUG("%s: recv JOIN - user '%s' with privilege level %d",
                                ID_CSTR(chatid), ID_CSTR(userid), priv);
                auto& msgs =  mClient.chats(chatid);
                msgs.onUserJoin(userid, priv);
                break;
            }
            case OP_OLDMSG:
            case OP_NEWMSG:
            case OP_MSGUPD:
            {
                bool isNewMsg = (opcode == OP_NEWMSG);
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                READ_32(ts, 24);
                READ_16(updated, 28);
                READ_32(keyid, 30);
                READ_32(msglen, 38);
                const char* msgdata = buf.read(pos, msglen);
                pos += msglen;
                CHATD_LOG_DEBUG("%s: recv %s: '%s', from user '%s' with keyid %s",
                    ID_CSTR(chatid), Command::opcodeToStr(opcode), ID_CSTR(msgid),
                    ID_CSTR(userid), keyid);
                auto msg = new Message(msgid, userid, ts, updated, keyid, msgdata, msglen, false);
                Chat& chat = mClient.chats(chatid);
                if (opcode == OP_MSGUPD)
                {
                    chat.msgUpdated(msg);
                }
                else
                {
                    chat.msgIncoming((opcode == NEWMSG), msg, false);
                }
                break;
            }
            case OP_SEEN:
            {
            //TODO: why do we test the whole buffer's len to determine the current command's len?
            //buffer may contain other commands following it
                READ_ID(chatid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("%s: recv SEEN - msgid: '%s'",
                                ID_CSTR(chatid), ID_CSTR(msgid));
                mClient.chats(chatid).onLastSeen(msgid);
                break;
            }
            case OP_RECEIVED:
            {
                READ_ID(chatid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("%s: recv RECEIVED - msgid: '%s'", ID_CSTR(chatid), ID_CSTR(msgid));
                mClient.chats(chatid).onLastReceived(msgid);
                break;
            }
            case OP_RETENTION:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_32(period, 16);
                CHATD_LOG_DEBUG("%s: recv RETENTION by user '%s' to %u second(s)",
                                ID_CSTR(chatid), ID_CSTR(userid), period);
                break;
            }
            case OP_MSGID:
            {
                READ_ID(msgxid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("recv MSGID: '%s' -> '%s'", ID_CSTR(msgxid), ID_CSTR(msgid));
                mClient.msgConfirm(msgxid, msgid);
                break;
            }
            case OP_RANGE:
            {
                READ_ID(chatid, 0);
                READ_ID(oldest, 8);
                READ_ID(newest, 16);
                CHATD_LOG_DEBUG("%s: recv RANGE: %s - %s",
                                ID_CSTR(chatid), ID_CSTR(oldest), ID_CSTR(newest));
                auto& msgs = mClient.chats(chatid);
                if (msgs.onlineState() == kChatStateJoining)
                    msgs.initialFetchHistory(newest);
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
                    CHATD_LOG_WARNING("%s rejected", Command::opcodeToStr(op));
                }
                break;
            }
            case OP_HISTDONE:
            {
                READ_ID(chatid, 0);
                CHATD_LOG_DEBUG("%s: recv HISTDONE: history retrieval finished", ID_CSTR(chatid));
                mClient.chats(chatid).onHistDone();
                break;
            }
            case OP_KEYID:
            {
                READ_ID(chatid, 0);
                READ_32(keyxid, 8);
                READ_32(keyid, 12);
                mClient.chats(chatid).keyConfirm(keyxid, keyid);
                break;
            }
            case OP_NEWKEY:
            {
                READ_ID(chatid, 0);
                READ_32(totalLen, 16);
                const char* keys = buf.read(pos, totalLen);
                mClient.chats(chatid).onNewKeys(StaticBuffer(keys, totalLen));
                break;
            }
            default:
                CHATD_LOG_ERROR("Unknown opcode %d, ignoring all subsequent comment", opcode);
                return;
        }
      }
      catch(BufferRangeError& e)
      {
            CHATD_LOG_ERROR("Buffer bound check error while parsing %s:\n\t%s\n\tAborting command processing", Command::opcodeToStr(opcode), e.what());
            return;
      }
      catch(std::exception& e)
      {
            CHATD_LOG_ERROR("Exception while processing incoming %s: %s", Command::opcodeToStr(opcode), e.what());
      }
    }
}

void Chat::onNewKeys(StaticBuffer&& keybuf)
{
    uint16_t keylen = 0;
    for(size_t pos = 0; pos < keybuf.dataSize(); pos+=(14+keylen))
    {
        uint64_t userid = keybuf.read<uint64_t>(pos);
        uint32_t keyid = keybuf.read<uint32_t>(pos+8);

        auto it = mPeerKeys.find(keyid);
        if (it != mPeerKeys.end()) //we already that key, but check if it's the same
        {
            auto& key = it->second;
            if (key.userid != userid)
                CHAT_LOG_ERROR("NEWKEY: Key %u from user '%s' already known, but from a different user '%s', skipping",
                    keyid, ID_CSTR(userid), ID_CSTR(key.userid));
            else
                CHAT_LOG_WARNING("NEWKEY: Key %u from user '%s' already known, skipping", keyid, ID_CSTR(userid));
            continue;
        }
        keylen = keybuf.read<uint16_t>(pos+12);
        char decbuf[Key::kMaxLen];
        try
        {
            uint16_t declen = mCrypto->decryptNewKey(keyid, userid, keylen, keybuf.read(pos+14, keylen), decbuf);
            mPeerKeys.emplace(std::piecewise_construct, std::forward_as_tuple(keyid),
                std::forward_as_tuple(userid, declen, decbuf));
        }
        catch(std::exception& e)
        {
            CHAT_LOG_ERROR("Error decrypting key %u from user '%s': %s", keyid, ID_CSTR(userid), e.what());
        }
    }
}

void Chat::onHistDone()
{
    assert(mHistFetchState == kHistFetchingFromServer);
    mHistFetchState = (mLastHistFetchCount > 0) ? kHistNotFetching : kHistNoMore;
    CALL_CRYPTO(onHistoryDone);
    CALL_LISTENER(onHistoryDone, false);
    if (mLastSeenIdx == CHATD_IDX_INVALID)
        CALL_LISTENER(onUnreadChanged);

    if(mOnlineState == kChatStateJoining)
    {
        //only on the first HISTDONE - we may have other history requests afterwards
        onJoinComplete();
    }
}

void Chat::loadAndProcessUnsent()
{
    std::vector<Message*> messages;
    CALL_DB(loadSendingTable, messages);
    for (auto msg: messages)
    {
        if (msg->edits())
        {
            std::unique_ptr<Message> autodel(msg); //in case msgModify() throws
            msgModify(msg->edits(), msg->editsIsXid(), msg->buf(), msg->dataSize(), nullptr, msg->id());
        }
        else
        {
            doMsgSubmit(msg);
        }
        CALL_LISTENER(onUnsentMsgLoaded, *msg);
    }
}

Message* Chat::msgSubmit(const char* msg, size_t msglen, Message::Type type, void* userp)
{
    // write the new message to the message buffer and mark as in sending state
    Message* message = new Message(Id::null(), mConnection.mClient.mUserId, time(NULL),
        0, 0, msg, msglen, type, userp, true);
    assert(message->isSending());
    assert(message->id() == Id::null());
    assert(message->keyId == Key::kInvalidId);
    msgEncryptAndSend(*message, OP_NEWMSG);
    return message;
}

void Chat::msgEncryptAndSend(Message* msg, uint8_t opcode)
{
    //opcode can be NEWMSG, MSGUPD or MSGUPDX
    std::unique_ptr<MessageCommand> cmd(new MessageCommand(opcode, mChatId, mUserId,
         msg->id(), Key::kInvalidId, msg->ts, 0));

    CHATD_LOG_CRYPTO_CALL("Calling ICrypto::encrypt()");
    bool encryptOk = mCrypto->encrypt(*msg, *cmd);
    auto rowid = mDbInterface->getNextRowId();
    if (msg->id() == Id::null())
    {
        assert(opcode == OP_NEWMSG); //assure it's really a new message, not an edit
        message.setId(makeRandomId(), true);  //set msgxid for the message
    }

    if (!encryptOk)
    {
        mEncryptionHalted = true;
        cmd.reset();
        CHATID_LOG_DEBUG("Can't encrypt message, halting output");
        mSending.emplace_back(opcode, nullptr, msg);
        CALL_DB(mSending.back());
    }
    else //encrypt success
    {
        mSending.emplace_back(opcode, cmd.release(), msg);
        CALL_DB(mSending.back());
    }

    if (mNextUnsent == mSending.end())
    {
        mNextUnsent--;
        //the only unsent message is this one, only try sending if encrypt was ok
        if (encryptOk)
            flushOutputQueue();
    }
    else
    {
        //there were other unsent messages before this, try sending regardless of encrypt success
        flushOutputQueue();
    }
    //we don't sent a msgStatusChange event to the listener, as the GUI should initialize the
    //message's status with something already, so it's redundant.
    //The GUI should by default show it as sending
}

void Chat::onCanEncryptAgain()
{
    mEncryptionHalted = false;
    for (auto it = mNextUnsent; it != mSending.end(); it++)
    {
        if (it->isMessage() && !it->cmd) //this is an unencrypted message
        {
            auto& msg = *it->msg();
            std::unique_ptr<Command> outCmd(new MessageCommand(opcode, mChatId,
                Id::null(), msg.id(), Key::kInvalidId, msg.ts, 0));
            bool encryptOk = mCrypto->encrypt(msg, *outCmd);
            if (!encryptOk)
                return;
            it->cmd = outCmd.release();
            CALL_DB(addCommandBlobToSendingItem, it->rowId, *it->cmd);
        }
    }
    flushOutputQueue();
}
//can be called for a message in history or a NEWMSG,MSGUPD,MSGUPDX message in sending queue
Message* Chat::msgModify(const Message& msg, const char* newdata, size_t newlen, void* userp)
{
    uint32_t age = time(NULL) - msg.ts;
    if (age > CHATD_MAX_EDIT_AGE)
        return nullptr;
    if (msg.isSending()) //update the original as well, trying to avoid sending the original content
    {
        SendingItem* item = nullptr;
        for (auto& loopItem: mSending)
        {
            if (loopItem.isMessage() && (loopItem.msg()->id() == msg.id()))
            {
                item = &loopItem;
                break;
            }
        }
        assert(item);
        msg.assign(newdata, newlen);
        auto cmd = item->msgCommand();
        if (cmd) //it's already encrypted, re-encrypt
        {
            cmd->clearMsg();
            bool encryptOk = mCrypto->encrypt(msg, *cmd);
            if (!encryptOk)
                throw std::runtime_error(mChatId.toString()+": msgModify: Message re-encrypt failed, and it shouldn't");
            CALL_DB(removeItemFromSending, item->rowId);
            CALL_DB(item);
        }
        else
        {
            CALL_DB(updateMsgPlaintextInSending, msg);
        }
    } //end msg.isSending()
    auto upd = new Message(msg.id(), msg.userid, msg.ts, age-1, newdata, newlen,
        msg.type, userp, msg.isSending());
    msgEncryptAndSend(upd, upd.isSending() ? OP_MSGUPDX : OP_MSGUPD);
    return upd;
}

void Chat::onLastReceived(const Id& msgid)
{
    mLastReceivedId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    { // we don't have that message in the buffer yet, so we don't know its index
        Idx idx = mDbInterface->getIdxOfMsgid(msgid);
        if (idx != CHATD_IDX_INVALID)
        {
            if ((mLastReceivedIdx != CHATD_IDX_INVALID) && (idx < mLastReceivedIdx))
            {
                CHATID_LOG_ERROR("onLastReceived: Tried to set the index to an older message, ignoring");
                CHATID_LOG_DEBUG("highnum() = %zu, mLastReceivedIdx = %zu, idx = %zu", highnum(), mLastReceivedIdx, idx);
            }
            else
            {
                mLastReceivedIdx = idx;
            }
        }
        return; //last-received is behind history in memory, so nothing to notify about
    }

    auto idx = it->second;
    if (idx == mLastReceivedIdx)
        return; //probably set from db
    if (at(idx).userid != mClient.mUserId)
    {
        CHATID_LOG_WARNING("Last-received pointer points to a message by a peer,"
            " possibly the pointer was set incorrectly");
    }
    //notify about messages that become 'received'
    Idx notifyOldest;
    if (mLastReceivedIdx != CHATD_IDX_INVALID) //we have a previous last-received index, notify user about received messages
    {
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
        mLastReceivedIdx = idx;
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

void Chat::onLastSeen(const Id& msgid)
{
    mLastSeenId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        Idx idx = mDbInterface->getIdxOfMsgid(msgid);
        //last seen is older than our history, so all history in memory is 'unseen', even if there was a previous, older last-seen
        if (idx != CHATD_IDX_INVALID)
        {
            if ((mLastSeenIdx != CHATD_IDX_INVALID) && (idx < mLastSeenIdx))
            {
                CHATD_LOG_ERROR("onLastSeen: Can't set last seen index to an older message");
                return;
            }
            else
            {
                mLastSeenIdx = idx;
            }
        }
    }
    else
    {
        auto idx = it->second;
        if (idx == mLastSeenIdx)
            return; //we may have set it from db already
        if(at(idx).userid == mClient.mUserId)
        {
            CHATD_LOG_WARNING("Last-seen points to a message by us, possibly the pointer was not set properly");
        }
        //notify about messages that have become 'seen'
        Idx notifyOldest;
        if (mLastSeenIdx != CHATD_IDX_INVALID)
        {
            if (idx < mLastSeenIdx)
                CHATID_LOG_ERROR("onLastSeen: Can't set last seen index to an older "
                    "message: current idx: %u, new: %u", mLastSeenIdx, idx);
            notifyOldest = mLastSeenIdx;
            mLastSeenIdx = idx;
        }
        else
        {
            mLastSeenIdx = idx;
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
    CALL_LISTENER(onUnreadChanged);
}

bool Chat::setMessageSeen(Idx idx)
{
    assert(idx != CHATD_IDX_INVALID);
    if ((mLastSeenIdx != CHATD_IDX_INVALID) && (idx <= mLastSeenIdx))
        return false;

    auto& msg = at(idx);
    if (msg.userid == mClient.mUserId)
    {
        CHATID_LOG_DEBUG("Asked to mark own message %s as seen, ignoring", ID_CSTR(msg.id()));
        return false;
    }
    Idx prevLastSeen = mLastSeenIdx;
    mLastSeenIdx = idx;
    sendCommand(Command(OP_SEEN) + mChatId + msg.id());
    for (Idx i=prevLastSeen+1; i<=mLastSeenIdx; i++)
    {
        auto& m = at(i);
        if (m.userid != mClient.mUserId)
        {
            mListener->onMessageStatusChange(i, Message::kSeen, m);
        }
    }
    CALL_LISTENER(onUnreadChanged);
    return true;
}

bool Chat::setMessageSeen(Id msgid)
{
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        CHATID_LOG_WARNING("setMessageSeen: unknown msgid '%s'", ID_CSTR(msgid));
        return false;
    }
    return setMessageSeen(it->second);
}

int Chat::unreadMsgCount() const
{
    if (mLastSeenIdx == CHATD_IDX_INVALID)
        return -mDbInterface->getPeerMsgCountAfterIdx(CHATD_IDX_INVALID);
    else if (mLastSeenIdx < lownum())
        return mDbInterface->getPeerMsgCountAfterIdx(mLastSeenIdx);

    Idx first = mLastSeenIdx+1;
    bool mayHaveMore = false;
    unsigned count = 0;
    auto last = highnum();
    for (Idx i=first; i<=last; i++)
    {
        auto& msg = at(i);
        if ((msg.userid != mClient.userId()) && !msg.edits()) //FIXME: This is not accurate - an edit of a seen message will not be counted
            count++;
    }
    return mayHaveMore?-count:count;
}

bool Chat::flushOutputQueue(bool fromStart)
{
    if(!mConnection.isOnline())
        return false;

    if (fromStart)
        mNextUnsent = mSending.begin();
    // resend all pending new messages
    if (mNextUnsent == mSending.end())
        return true;
    for (; mNextUnsent!=mSending.end(); mNextUnsent++)
    {
        if (!mNextUnsent->cmd)
        {
            assert(mNextUnsent->msg); //only not-yet-encrypted messages are allowed to have cmd=nullptr
            return false;
        }
        if (!mConnection.sendCommand(mNextUnsent->cmd))
            return false;
    }
    return true;
}

// after a reconnect, we tell the chatd the oldest and newest buffered message
void Chat::range()
{
    if (mOldestKnownMsgId)
    {
        CHATID_LOG_DEBUG("Sending RANGE based on app db: %s - %s",
            mOldestKnownMsgId.toString().c_str(), mNewestKnownMsgId.toString().c_str());
        sendCommand(Command(OP_RANGE) + mChatId + mOldestKnownMsgId + mNewestKnownMsgId);
        return;
    }
    if (empty())
    {
        CHATID_LOG_DEBUG("No local history, no range to send");
        initialFetchHistory(Id::null());
        return;
    }

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
    if (i > highest)
        i = highest;
    CHATID_LOG_DEBUG("Sending RANGE calculated from memory buffer: %s - %s",
        at(lownum()).id().toString().c_str(), at(i).id().toString().c_str());
    sendCommand(Command(OP_RANGE) + mChatId + at(lownum()).id() + at(i).id());
}

void Client::msgConfirm(const Id& msgxid, const Id& msgid)
{
    // CHECK: is it more efficient to keep a separate mapping of msgxid to messages?
    for (auto& messages: mChatForChatId)
    {
        if (messages.second->confirm(msgxid, msgid) != CHATD_IDX_INVALID)
            return;
    }
    CHATD_LOG_DEBUG("confirm: Unknown message transaction id %s", ID_CSTR(msgxid));
}

// msgid can be 0 in case of rejections
Idx Chat::msgConfirm(const Id& msgxid, const Id& msgid)
{
    // as msgConirm() is tried on all chatids, it's normal that we don't have the message,
    // so no error logging of error, just return invalid index
    if (mSending.empty())
    {
        CHATID_LOG_ERROR("msgConfirm: MSGID received but sending queue is empty");
        return CHATD_IDX_INVALID;
    }
    auto& item = mSending.front();
    if ((item.opcode() != OP_NEWMSG) || (item.msg()->id() != msgxid))
    {
        CHATID_LOG_ERROR("msgConfirm: Front of sending queue does not match confirmation message");
        return CHATD_IDX_INVALID;
    }
    if (!item.cmd)
    {//don't assert as this depends on external input
        CHATID_LOG_ERROR("msgConfirm: Sending item has no associated Command object");
        return CHATD_IDX_INVALID;
    }

    auto msg = item.msg();
    item.data = nullptr;
    assert(msg);
    assert(msg->isSending());
    CALL_DB(deleteItemFromSending, item.rowId);

    if (mNextUnsent == mSending.begin())
        mNextUnsent++; //because we remove the first element
    mSending.pop_front(); //deletes item

    if (!msgid)
    {
        CALL_LISTENER(onMessageRejected, *msg);
        delete msg;
        return CHATD_IDX_INVALID;
    }

    //put into history
    msg->setId(msgid, false);
    push_forward(msg);
    auto idx = mIdToIndexMap[msgid] = highnum();
    CALL_DB(addMsgToHistory, *msg, idx);
    CALL_LISTENER(onMessageConfirmed, msgxid, *msg, idx);
    return idx;
}

void Chat::keyConfirm(KeyId keyxid, KeyId keyid)
{
    if (mSending.empty())
    {
        CHATID_LOG_ERROR("keyConfirm: Sending queue is empty");
        return;
    }
    if (mSending.front()->opcode() != OP_NEWKEY)
    {
        CHATID_LOG_ERROR("keyConfirm: Front of sending queue is not a NEWKEY command");
        return;
    }
    if (!currentSendKey || !currentSendKey->isUnconfirmed())
    {
        CHATID_LOG_ERROR("keyConfirm: There is no current send key, or it is already confirmed");
        return;
    }
    if (keyxid != 0xffffffff)
    {
        CHATID_LOG_ERROR("keyConfirm: Key transaction id != 0xffffffff, continuing anyway");
    }
    //update keyxids to keyids: if client disconnects the keyxids will become invalid
    for (auto item: mSending)
    {
        if (item->isMessage() && (item->msg()->keyid == Key::kUnconfirmedId))
        {
            item->msg()->keyid = keyid;
            CALL_DB(updateMsgKeyIdInSending, item->rowid, keyid);
        }
    }
    //Remove sending item and move key object to keys map
    auto& item = mSending.front();
    auto key = static_cast<Key*>(item.data);
    item.data = nullptr;
    CALL_DB(deleteItemFromSending, item->rowId);

    if (mNextUnsent == mSending.begin())
        mNextUnsent++; //because we remove the first element
    mSending.pop_front();

    currentSendKey->setId(keyid);
#ifndef NDEBUG
    auto ret =
#endif
    keys.insert(keyid, key);
    assert(ret.second);
}

void Chat::msgUpdConfirm(msgid, const StaticBuffer& msg)
{
//remove any queued msgupds from sending, even if they are not the same edit
//in case multiple clients edit the same msg
    for (auto it = mSending.begin(); it != mSending.end(); )
    {
        auto& item = *it;
        if (item.opcode() != OP_MSGUPD) && (item.opcode() != OP_MSGUPDX))
        {
            it++;
            continue;
        }
        //erase item
        CALL_DB(deleteItemFromSending, item.rowid);
        if (!item.msg()->dataEquals(msg.buf(), msg.dataSize()))
             CHATD_LOG_WANRING("msgUpdConfirm: Discarding a different queued edit than the one received");
        auto erased = it;
        it++;
        mSending.erase(erased);
    }
    CALL_DB(updateMsgInHistory, msg, msgid);
    auto msgit = mIdToIndexMap.find(msgid);
    if (msgit != mIdToIndexMap.end())
    {
        auto& histmsg = at(msgit->second);
        histmsg.assign(msg);
        CALL_LISTENER(onMessageEdited, histmsg, msgit->second);
    }
}

Message::Status Chat::getMsgStatus(Idx idx, const Id& userid)
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
        if (mLastSeenIdx == CHATD_IDX_INVALID)
            return Message::kNotSeen;
        return (idx <= mLastSeenIdx) ? Message::kSeen : Message::kNotSeen;
    }
}

Idx Chat::msgIncoming(bool isNew, Message* message, bool isLocal)
{
    assert((isLocal && !isNew) || !isLocal);
    auto msgid = message->id();
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
        if (msgid == mOldestKnownMsgId) //we have a range, and we have just added the oldest message that we know we have (maybe in db), the range is no longer up-to-date, so disable it
        {
            mOldestKnownMsgId = 0;
            mNewestKnownMsgId = 0;
        }
        idx = lownum();
    }
    mIdToIndexMap[msgid] = idx;
    CHATD_LOG_CRYPTO_CALL("Calling ICrypto::decrypt()");
    if (!isLocal) //decrypt and save to history db
    {
        mCrypto->decrypt(*message);
        CALL_DB(addMsgToHistory, *message, idx);
        if ((message->userid != mClient.mUserId) &&
           ((mLastReceivedIdx == CHATD_IDX_INVALID) || (idx > mLastReceivedIdx)))
        {
            sendCommand(Command(OP_RECEIVED) + mChatId + msgid);
        }
    }
    auto status = getMsgStatus(idx, message->userid);
    if (isNew)
        CALL_LISTENER(onRecvNewMessage, idx, *message, status);
    else
        CALL_LISTENER(onRecvHistoryMessage, idx, *message, status, isLocal);

    //normally the indices will not be set if mLastXXXId == msgid, as there will be only
    //one chance to set the idx (we receive the msg only once).
    if (msgid == mLastSeenId) //we didn't have the message when we received the last seen id
    {
        CHATID_LOG_DEBUG("Received the message with the last-seen msgid '%s', "
            "setting the index pointer to it", ID_CSTR(msgid));
        onLastSeen(msgid);
    }
    if (mLastReceivedId == msgid)
    {
        //we didn't have the message when we received the last received msgid pointer,
        //and now we just received the message - set the index pointer
        CHATID_LOG_DEBUG("Received the message with the last-received msgid '%s', "
            "setting the index pointer to it", ID_CSTR(msgid));
        onLastReceived(msgid);
    }
    if (isNew)
        CALL_LISTENER(onUnreadChanged);
    return idx;
}
// procedure is as follows:
// set state as joining
// send join
// receive joins
// if (we don't have anything in DB or memory)
// {
//      call initialFetchHistory(0) - this results in sending HIST (-initialHistFetchCount)
// }
// if (server has history)
// {
//      receive server RANGE
//      call initialFetchHistory(rangeLast) - ignored if it was already called in prev step
// }
// [receive history from server, if any]
// receive HISTDONE
// login is complete, set state to online

void Chat::initialFetchHistory(Id serverNewest)
{
    if (mInitialFetchHistoryCalled)
        return;
    mInitialFetchHistoryCalled = true;
    assert(mOnlineState == kChatStateJoining);
    if (empty()) //if we have messages in db, we must always have loaded some
    {
        assert(!mOldestKnownMsgId);
        //we don't have messages in db, and we don't have messages in memory
        //we haven't sent a RANGE, so we can get history only backwards
        requestHistoryFromServer(-initialHistoryFetchCount);
    }
    else
    {
        if (at(highnum()).id() != serverNewest)
        {
            CHATID_LOG_DEBUG("There are new messages on the server, requesting them");
//the server has more recent msgs than the most recent in our db, retrieve all newer ones, after our RANGE
            requestHistoryFromServer(0x0fffffff);
        }
        else
        {
            onJoinComplete();
        }
    }
}

void Chat::onUserJoin(const Id& userid, Priv priv)
{
    if (priv != PRIV_NOTPRESENT)
    {
        CALL_CRYPTO(onUserJoined, userid, priv);
        CALL_LISTENER(onUserJoined, userid, priv);
    }
    else
    {
        CALL_CRYPTO(onUserLeft, userid);
        CALL_LISTENER(onUserLeft, userid);
    }
}

void Chat::onJoinComplete()
{
    setOnlineState(kChatStateOnline);
    if (mIsFirstJoin)
    {
        mIsFirstJoin = false;
        loadAndProcessUnsent();
    }
    flushOutputQueue(true);
}

void Chat::setOnlineState(ChatState state)
{
    if (state == mOnlineState)
        return;
    mOnlineState = state;
    CALL_CRYPTO(onOnlineStateChange, state);
    CALL_LISTENER(onOnlineStateChange, state);
}

void Client::leave(Id chatid)
{
    mConnectionForChatId.erase(chatid);
    mChatForChatId.erase(chatid);
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
