#include "chatd.h"
#include <base/cservices.h>
#include <gcmpp.h>
#include <retryHandler.h>
#include<libws_log.h>

using namespace std;
using namespace promise;

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
    Messages* msgs = new Messages(*conn, chatid, listener);
    mMessagesForChatId.emplace(std::piecewise_construct, std::forward_as_tuple(chatid),
                               std::forward_as_tuple(msgs));
    if (histFetchCount)
    {
        msgs->mInitialHistoryFetchCount = histFetchCount;
    }
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
        mConnection.sendCommand(Command(OP_HIST) + mChatId + (int32_t)(-count));
        return true;
    }
}

void Messages::getHistoryFromDb(int count)
{
    assert(mOldestKnownMsgId);
    std::vector<Message*> messages;
    bool ret = mListener->fetchDbHistory(lownum()-1, count, messages);
    for (auto msg: messages)
    {
        msgIncoming(false, msg, true);
    }
    if (!ret && mOldestKnownMsgId)
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
        CHATD_LOG_DEBUG("RECV %s", Command::opcodeToStr(opcode));
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
                CHATD_LOG_DEBUG("Join or privilege change - user '%s' on '%s' with privilege level %d",
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

                CHATD_LOG_DEBUG("%s message '%s' from user '%s' on chatid '%s' at time %u with len %u",
                    (isNewMsg ? "New" : "Old"), ID_CSTR(msgid), ID_CSTR(userid),
                    ID_CSTR(chatid), ts, msglen);

                mClient.chatidMessages(chatid).msgIncoming(
                    isNewMsg, new Message(msgid, userid, ts, msg, msglen), false);
                break;
            }
            case OP_MSGUPD:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
//              READ_32(ts, 24);
                pos+=4; //to silence unused var warning, we don't read the ts
                READ_32(msglen, 28);
                const char* msg = buf.read(pos, msglen);
                pos += msglen;
//at offset 16 is a nullid (id equal to zero) TODO: maybe remove it?
                CHATD_LOG_DEBUG("Message %s EDIT/DELETION: '%.*s'", ID_CSTR(msgid), msglen, msg);
                mClient.chatidMessages(chatid).onMsgUpdCommand(msgid, msg, msglen);
                break;
            }
            case OP_SEEN:
            {
            //TODO: why do we test the whole buffer's len to determine the current command's len?
            //buffer may contain other commands following it
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                CHATD_LOG_DEBUG("Newest seen message on %s for user %s: %s",
                                ID_CSTR(chatid), ID_CSTR(userid), ID_CSTR(msgid));
                mClient.chatidMessages(chatid).onLastSeen(msgid);
                break;
            }
            case OP_RECEIVED:
            {
                READ_ID(chatid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("Newest delivered message on %s : %s", ID_CSTR(chatid), ID_CSTR(msgid));
                mClient.chatidMessages(chatid).onLastReceived(msgid);
                break;
            }
            case OP_RETENTION:
            {
                READ_ID(chatid, 0);
                READ_ID(userid, 8);
                READ_32(period, 16);
                CHATD_LOG_DEBUG("Retention policy change on %s by %s to %u second(s)",
                                ID_CSTR(chatid), ID_CSTR(userid), period);
                break;
            }
            case OP_MSGID:
            {
                READ_ID(msgxid, 0);
                READ_ID(msgid, 8);
                CHATD_LOG_DEBUG("Sent message ID confirmed: %s->%s", ID_CSTR(msgxid), ID_CSTR(msgid));
                mClient.msgConfirm(msgxid, msgid);
                break;
            }
            case OP_RANGE:
            {
                READ_ID(chatid, 0);
                READ_ID(oldest, 8);
                READ_ID(newest, 16);
                CHATD_LOG_DEBUG("Known chat message IDs - oldest: %s, newest: %s",
                                ID_CSTR(oldest), ID_CSTR(newest));
                mClient.chatidMessages(chatid).initialFetchHistory(newest);
                break;
            }
            case OP_REJECT:
            {
                READ_ID(id, 0);
                READ_32(op, 8);
                READ_32(code, 12); //TODO: what's this code?
                CHATD_LOG_DEBUG("Command was rejected: %d / %d", op, code);
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
                CHATD_LOG_DEBUG("History retrieval of chat '%s' finished", ID_CSTR(chatid));
                auto& msgs = mClient.chatidMessages(chatid);
                if(!msgs.mJoinPromise->done())
                {
//resolve only on the first HISTDONE - we may have other history requests afterwards
                    msgs.mJoinPromise->resolve();
                    msgs.setOnlineState(kChatStateOnline);
                }
                else
                {
                    msgs.mListener->onHistoryDone();
                }
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

void Connection::msgUpdate(const Id& chatid, const Message& message)
{
    sendCommand(Command(OP_MSGUPD) + chatid + Id::null() + message.id + (uint32_t)0 + message);
}

size_t Messages::msgSubmit(const char* msg, size_t msglen)
{
    // allocate a transactionid for the new message
    const Id& msgxid = mConnection.mClient.nextTransactionId();

    // write the new message to the message buffer and mark as in sending state
    Message* message = new Message(msgxid, mConnection.mClient.mUserId, time(NULL), msg, msglen);
    push_forward(message);
    auto num = mSending[msgxid] = highnum();

    //if we believe to be online, send immediately
    //we don't sent a msgStatusChange event to the listener, as the GUI should initialize the
    //message's status with something already, so it's redundant.
    //The GUI should by default show it as sending
    if (mConnection.isOnline())
    {
        mConnection.msgSend(mChatId, *message);
    }
    return num;
}

void Messages::onLastReceived(const Id& msgid)
{
    mLastReceivedId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    { // we don't have that message in the buffer yet, so we don't know its index
        mLastReceivedIdx = 0;
        return;
    }
    if (at(it->second).userid != mClient.mUserId)
    {
        CHATD_LOG_WARNING("Last-received pointer points to a message by a peer, possibly the pointer was set incorrectly");
    }
    if (mLastReceivedIdx) //we have a previous last-received index, notify user about received messages
    {
        auto idx = it->second;
        if (mLastReceivedIdx > idx)
        {
            CHATD_LOG_ERROR("onLastReceived: Tried to set the index to an older message, ignoring");
            CHATD_LOG_DEBUG("highnum() = %zu, mLastReceivedIdx = %zu, idx = %zu", highnum(), mLastReceivedIdx, idx);
            return;
        }
        auto lastidx = mLastReceivedIdx;
        mLastReceivedIdx = idx;
        for (size_t i=lastidx; i<=idx; i++)
        {
            auto& msg = at(i);
            if ((msg.userid == mClient.mUserId) && (mModified.find(msgid) == mModified.end()))
            {
                mListener->onMessageStatusChange(i, Message::kDelivered, 0);
            }
        }
    }
    else
    {
        mLastReceivedIdx = it->second;
    }
}

void Messages::onLastSeen(const Id& msgid)
{
    mLastSeenId = msgid;
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        mLastSeenIdx = 0;
        return;
    }
    if (at(it->second).userid == mClient.mUserId)
    {
        CHATD_LOG_WARNING("Last-seen points to a message by us, possibly the pointer was not set properly");
    }
    if (mLastSeenIdx)
    {
        auto idx = it->second;
        if (idx < mLastSeenIdx)
            throw std::runtime_error("onLastSeen: Can't set last seen index to an older message");
        assert(idx != mLastSeenIdx); //we were called redundantly - the msgid and index were already set and the same. This will not hurt, except for generating one redundant onMessageStateChange for the message that is pointed to by the index
        auto prevIdx = mLastSeenIdx;
        mLastSeenIdx = idx;
        for (size_t i=prevIdx; i<=idx; i++)
        {
            auto& msg = at(i);
            if (msg.userid != mClient.mUserId)
            {
                mListener->onMessageStatusChange(i, Message::kSeen, 0);
            }
        }
    }
    else
    {
        mLastSeenIdx = it->second;
    }
}
bool Messages::setMessageSeen(size_t idx)
{
    if (idx <= mLastSeenIdx)
        return false;
    Message& msg = at(idx);
    if (msg.userid == mClient.mUserId)
    {
        CHATD_LOG_DEBUG("Asked to mark own message %s as seen, ignoring", ID_CSTR(msg.id));
        return false;
    }
    mConnection.sendCommand(Command(OP_SEEN) + mChatId + mClient.mUserId + msg.id);
    return true;
}

void Messages::msgModify(size_t msgnum, const char* msgdata, size_t msglen)
{
    // modify pending message so that a potential resend includes the change
    Message& msg = at(msgnum);
    if (msg.userid != mClient.mUserId)
        throw std::runtime_error("You cannot modify other people's messages");
    // record this modification for resending purposes
    msg.assign(msgdata, msglen);

    auto it = mSending.find(msg.id); //id is a msgxid
    if (it == mSending.end())
    {
        if (mConnection.isOnline())
        {
            mConnection.msgUpdate(mChatId, msg);
        }
    }
    mModified[msgnum] = &msg;
    mListener->onMessageStatusChange(msgnum, Message::kSending, 0);
}

void Messages::resendPending()
{
    // resend all pending new messages and modifications
    for (auto& item: mSending)
    {
        mConnection.msgSend(mChatId, at(item.second));
    }
    for (auto& item: mModified)
    {    // resend all pending modifications of completed messages
        mConnection.msgUpdate(mChatId, *item.second);
    }
}

// after a reconnect, we tell the chatd the oldest and newest buffered message
void Messages::range()
{
    if (mOldestKnownMsgId)
    {
        mConnection.sendCommand(Command(OP_RANGE) + mChatId + mOldestKnownMsgId + mNewestKnownMsgId);
        return;
    }
    if (empty())
        return;

    auto highest = highnum();
    size_t i = lownum();
    for (; i<=highest; i++)
    {
        auto& msg = at(i);
        if (mSending.find(msg.id) != mSending.end())
        {
            i--;
            break;
        }
    }
    if (i < lownum()) //we have only unsent messages
        return;
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
bool Messages::confirm(const Id& msgxid, const Id& msgid)
{
    auto it = mSending.find(msgxid);
    if (it == mSending.end())
        return false;

    auto num = it->second;
    mSending.erase(it);
    Message& msg = at(num);
    //update transaction id to the actual msgid
    msg.id = msgid;
    mIdToIndexMap[msgid] = num;

    // we now have a proper msgid, resend MSGUPD in case the edit crossed the execution of the command
    if (mModified.find(num) != mModified.end())
    {
        mConnection.msgUpdate(mChatId, msg);
    }
    else
    {
        if (!msgid)
            mListener->onMessageStatusChange(num, Message::kServerRejected, 0);
        else
            mListener->onMessageStatusChange(num, Message::kServerReceived, Message::kCreated);
    }
    return true;
}
Message::Status Messages::getMsgStatus(size_t idx, const Id& userid)
{
    if (userid == mClient.mUserId)
    {
        Id& msgid = at(idx).id;
        if ((mSending.find(msgid) != mSending.end()) || (mModified.find(idx) != mModified.end()))
            return Message::kSending;
        else if (idx <= mLastReceivedIdx)
            return Message::kDelivered;
        else
            return Message::kServerReceived;
    } //message is from a peer
    else
    {
        if (!mLastSeenIdx)
            CHATD_LOG_DEBUG("getMsgStatus: mLastSeenIdx is yet unknown, returning status NotSeen");
        return (idx <= mLastSeenIdx) ? Message::kSeen : Message::kNotSeen;
    }
}

size_t Messages::msgIncoming(bool isNew, Message* message, bool isLocal)
{
    assert((isLocal && !isNew) || !isLocal);
    auto msgid = message->id;
    assert(msgid);
    size_t idx;

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
    //normally the indices will not be set if mLastXXXId == msgid, as there will be only
    //one chance to set the idx (we receive the msg only once).
    if (msgid == mLastSeenId) //we didn't have the message when we received the last seen id
    {
        assert(!mLastSeenIdx);
        CHATD_LOG_DEBUG("Received the message to which the last-seen msgid points, setting mLastSeenIndex to it");
        onLastSeen(msgid);
    }
    if (mLastReceivedId == msgid)
    {
        assert(!mLastReceivedIdx);
        //we didn't have the last-received message in the buffer when we received
        //the last received msgid, and now we received the message - set the index pointer
        CHATD_LOG_DEBUG("Received the message with the last-received msgid, setting the index to it");
        onLastReceived(msgid);
    }
    if (isNew)
        mListener->onRecvNewMessage(idx, *message, getMsgStatus(idx, message->userid));
    else
        mListener->onRecvHistoryMessage(idx, *message, getMsgStatus(idx, message->userid), isLocal);

    return idx;
}

void Messages::onMsgUpdCommand(const Id& msgid, const char* msgdata, size_t msglen)
{
    // CHECK: is it more efficient to maintain a full hash msgid -> num?
    // FIXME: eliminate namespace clash collision risk
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        CHATD_LOG_ERROR("Unknown msgid %s", ID_CSTR(msgid));
        return;
    }
    size_t idx = it->second;
    // if we modified the message, remove from this.modified.
    // if someone else did before us, resend the MSGUPD (might be redundant)
    auto& msg = at(idx);
    auto modit = mModified.find(idx);
    if (modit != mModified.end())
    {
        if (msg.dataEquals(msgdata, msglen))
        {
            mModified.erase(modit);
            mListener->onMessageStatusChange(idx, Message::kServerReceived, 0);
        }
        else
        {
            mConnection.msgUpdate(mChatId, msg);
        }
    }
    else //not in mModified
    {
        msg.assign(msgdata, msglen);
        mListener->onMessageEdited(idx, msg); //Notify GUI about updated message
    }
}

void Messages::initialFetchHistory(const Id& serverNewest)
{
//we have messages in the db, fetch the newest from there
    if (mOldestKnownMsgId)
    {
        getHistoryFromDb(mInitialHistoryFetchCount);
        if (at(highnum()).id != serverNewest)
        {
//the server has more recent msgs than the most recent in our db, retrieve all newer ones, after our RANGE
            mConnection.sendCommand(Command(OP_HIST) + mChatId + int32_t(0xffffffff));
        }
        else
        {
            assert(!mJoinPromise->done());
            mJoinPromise->resolve();
            setOnlineState(kChatStateOnline);
        }
    }
    else if (empty()) //we don't have messages in db, and we don't have messages in memory
    {                 //we haven't sent a RANGE, so we can get history only backwards
        mConnection.sendCommand(Command(OP_HIST) + mChatId + (int32_t)(-mInitialHistoryFetchCount));
    }
    else if (at(highnum()).id != serverNewest) //we have messages in memory and not in db, but not the latest one
    {
//we have sent a RANGE
        mConnection.sendCommand(Command(OP_HIST) + mChatId + int32_t(0xffffffff));
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
 "KEEPALIVE","JOIN", "OLDMSG", "NEWMSG", "MSGUPD ","SEEN",
 "RECEIVED","RETENTION","HIST", "RANGE","MSGID","REJECT",
 "BROADCAST", "HISTDONE"
};
const char* Message::statusNames[] =
{
  "Sending", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

}
