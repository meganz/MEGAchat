#include <libws.h>
#include <stdint.h>
#include <string>

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
        ws_global_init(&sWebsockBaseContext, services_get_event_loop(), services_dns_eventbase,
        [](struct bufferevent* bev, void* userp)
        {
            mega::marshallCall([bev, userp]()
            {
                ws_read_callback(bev, userp);
            });
        },
        [](struct bufferevent* bev, short events, void* userp)
        {
            mega::marshallCall([bev, events, userp]()
            {
                ws_event_callback(bev, events, userp);
            });
        },
        [](int fd, short events, void* userp)
        {
            mega::marshallCall([events, userp]()
            {
                ws_handle_marshall_timer_cb(0, events, userp);
            })
        });
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
Connection& Client::getOrCreateConnection(const Id& chatid, int shardNo, const std::string& url, bool& isNew)
{
    // instantiate a Connection object for this shard if needed
    Connection* conn;
    auto it = mConnections.find(shardNo);
    if (it == mConnections.end())
    {
        isNew = true;
        conn = mConnections[shardNo] = new Connection(this, shardNo);
    }
    else
    {
        isNew = false;
        conn = it->second.get();
    }

    // map chatid to this shard
    mConnectionForChatId[chatid] = conn;
    // add chatid to the connection's chatids
    conn->mChatIds.insert(chatid);
    // always update the URL to give the API an opportunity to migrate chat shards between hosts
    conn->mUrl.parse(url);

    // attempt a connection ONLY if this is a new shard.
    if(isNew)
    {
        conn->reconnect();
    }

    return *conn;
}

void Connection::websockConnectCb(ws_t ws, void* arg)
{
    Connection* self = static_cast<Connection*>(arg);
    ASSERT_NOT_ANOTHER_WS("connect");

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
        protocol = 'http';
    }
    char last = protocol[protocol.size()-1];
    isSecure = (last == 's') || (last == 'S');

    size_t i = ss;
    for (; i<url.size(); i++)
    {
        char ch = url[i];
        if (ch == ':') //we have port
        {
            size_t ps = i+1;
            mServer = url.substr(s, i-ss);
            for (; i<url.size(); i++)
            {
                ch = url[i];
                if (ch == '/')
                {
                    port = std::stol(url.substr(ps, i-ps));
                    break;
                }
            }
            break;
        }
        else if ((ch == '/') || (ch == '?'))
        {
            host = url.substr(ss, i-ss);
        }
    }
    if (i >= url.size()) //only host and protocol
    {
        path = "/";
        if (!port)
        {
            port = getPortFromProtocol();
        }
        return;
    }
    //i now points to '/' or '?' and host and port must have been set
    assert(!host.empty());
    path = (url[i] == '?') ? ("/"+url.substr(i)) : mPath = url.substr(i);
    if (!port)
    {
        port = getPortFromProtocol();
    }
}

uint16_t Url::getPortFromProtocol()
{
    if ((protocol == "http") || (protocol == "ws"))
        return 80;
    else if ((protocol = "https") || (protocol == "wss"))
        return 443;
    else
        return 0;
}
void Connection::websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
                                size_t reason_len, void *arg)
{
    Connection* self = static_cast<Connection*>(arg);
    ASSERT_NOT_ANOTHER_WS("close/error");

    auto pms = self->mConnectPromise.get();
    if (pms && !pms->done())
    {
        pms.reject(state());
    }
    pms = self->mDisconnectPromise.get();
    if (pms && !pms->done())
    {
        pms.resolve();
    }
}

Promise<void> Connection::reconnect()
{
    int state = state();
    if ((state == WS_STATE_CONNECTING) || (state == WS_STATE_CONNECTED))
    {
        throw std::runtime_error("Connection::reconnect: Already connected/connecting");
    }
    return
    mega::retry([this](int no)
    {
        reset();
        checkLibwsCall((libws_init(&client->mWebsocketContext, &mWebSocket)), "create socket");
        ws_set_onconnect_cb(mWebSocket, &websockConnectCb, this);
        ws_set_onclose_cb(mWebSocket, &websockCloseCb, this);
        ws_set_on_msg_cb(mWebSocket,
            [](ws_t ws, char *msg, uint64_t len, int binary, void *arg)
            {
                Connection* self = static_cast<Connection*>(arg);
                ASSERT_NOT_ANOTHER_WS("message");
                self->execCommand(msg, len);
            }, this);

        //TODO: attach other event handlers

        mConnectPromise.reset(new Promise<void>);
        checkLibwsCall((ws_connect(mWebSocket, mUrl.host.c_str(), mUrl.port, mUrl.path.c_str())), "connect");
        return *mConnectPromise;
    }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL)
    .then([this]()
    {
        rejoinExistingChats();
        resendPending();
        //TODO: maybe return promise from this setup?
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
        mDisconnectPromise = new Promise<void>;
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

Connection::sendCommand(Command&& cmd)
{
    //console.error("CMD SENT: ", constStateToText(Chatd.Opcode, opcode), cmd);
    if (mCommandQueue.empty())
    {
        mCommandQueue.assign(std::move(cmd));
    }
    else
    {
        mCommandQueue.append(cmd);
    }
    if (!isOnline())
    {
        return;
    }
    auto rc = ws_send_msg_ex(mWebSocket, mCommandQueue.buf(), mCommandQueue.dataSize(), 1);
    if (!rc && isOnline())
    {
        mCommandQueue.free();
    }
}

// rejoin all open chats after reconnection (this is mandatory)
void Connection::rejoinExistingChats()
{
    for (auto& chatid: mChatIds)
    {
        // rejoin chat and immediately set the locally buffered message range
        join(chatid);
        mClient.range(chatid);
    }
}

// resend all unconfirmed messages (this is mandatory)
void Connection::resendPending()
{
    for (auto& chatid: mChatIds)
    {
        mClient.mMessagesForChatId[chatid].resendPending();
    }
}

// send JOIN
void Connection::join(const Id& chatid)
{
    sendCommand(Command(OP_JOIN) + chatid + mClient.mUserId + PRIV_NOCHANGE);
}

void Client::sendCommand(const Id& chatid, Command&& cmd)
{
    auto conn = chatIdConn(chatid);
    conn.sendCommand(std::forward<Command>(cmd));
}

void Client::getHistory(const Id& chatid, int count)
{
    chatIdConn(chatid).hist(chatid, count);
}

// send RANGE
void Client::range(const Id& chatid)
{
    chatIdMessages(chatid).range(chatid);
}

// send HIST
void Connection::hist(const Id& chatid, long count)
{
    sendCommand(Command(OP_HIST) + chatid + htonl(count));
}

//These throw ParseError and will cause the loop to abort.
//These are critical errors that are not per-command

#define READ_ID(varname, offset) Id varname(buf.read<uint64_t>(pos+offset))
#define READ_32(varname, offset) uint32_t varname(ntohl(buf.read<uint32_t>(pos+offset)))
#define ID_CSTR(id) id.toString().c_str()

// inbound command processing
// multiple commands can appear as one WebSocket frame, but commands never cross frame boundaries
// CHECK: is this assumption correct on all browsers and under all circumstances?
void Connection::execCommand(const Buffer& buf)
{
    size_t pos = 0;
//IMPORTANT: Increment pos before calling the command handler, because the handler may throw, in which
//case the next iteration will not advance and will execute the same command again, resulting in
//infinite loop
    while (pos < len)
    {
      try
      {
        char opcode = buf.buf()[pos];
        switch (opcode)
        {
            case OP_KEEPALIVE:
            {
                CHATD_LOG_DEBUG("Server heartbeat received");
                execCommand(Command(OP_KEEPALIVE));
                pos += 1;
                break;
            }
            case OP_JOIN:
            {
                READ_ID(userid, 9);
                READ_ID(chatid, 1);
                int priv = buf.read<uint8_t>(17);
                CHATD_LOG_DEBUG("Join or privilege change - user '%s' on '%s' with privilege level %d", userid.c_str(), chatid.c_str(), priv);

//                self.chatd.trigger('onMembersUpdated', {
                pos += 18;
                break;
            }
            case Chatd.Opcode.OLDMSG:
            case Chatd.Opcode.NEWMSG:
            {
                bool isNewMsg = (opcode == OP_NEWMSG);
                READ_ID(msgid, 17);
                READ_ID(userid, 9);
                READ_ID(chatid, 1);
                READ_32(ts, 25);
                READ_32(msglen, 29);
                const char* msg = buf.read(33, msglen);
                CHATD_LOG_DEBUG("%s message '%s' from '%s' on '%s' at '%u': '%.*s'",
                    (isNewMsg ? "New" : "Old"), msgid.c_str(), userid.c_str(), chatid.c_str(), ts, msglen, msg);

                pos += 33;
                pos += msglen;

                size_t idx = mClient.msgStore(isNewMsg, chatid, new Message(msgid, userid, ts, msg, msglen));
                sendCommand(Command(OP_RECEIVED) + chatid + msgid);
                //onMessage
                break;
            }
            case OP_MSGUPD:
            {
                READ_32(msglen, 29);
                READ_ID(chatid, 1);
                READ_ID(msgid, 9);
                const char* msg = buf.read(33, msglen);
//at offset 16 is a nullid (id equal to zero) TODO: maybe remove it?
                CHATD_LOG_DEBUG("Message %s EDIT/DELETION: '%.*s'", msgid.c_str(), msglen, msg);
                pos += 33;
                pos += msglen;
                mClient.onMsgUpdCommand(chatid, msgid, msg, msglen);
                break;
            }
            case OP_SEEN:
            {
            //TODO: why do we test the whole buffer's len to determine the current command's len?
            //buffer may contain other commands following it
                READ_ID(chatid, 1);
                READ_ID(userid, 9);
                READ_ID(msgid, 17);
                CHATD_LOG_DEBUG("Newest seen message on %s for user %s: %s",
                                ID_CSTR(chatid), ID_CSTR(userid), ID_CSTR(msgid));
                Messages& messages = mClient.chatidMessages(chatid);
                messages.onLastSeen(msgid);
                pos += 25;
                break;
            }
            case OP_RECEIVED:
            {
                READ_ID(chatid, 1);
                READ_ID(msgid, 9);
                CHATD_LOG_DEBUG("Newest delivered message on %s : %s", ID_CSTR(chatid), ID_CSTR(msgid));
                Messages& messages = mClient.chatidMessages(chatid);
                messages.onLastReceived(msgid);
                pos += 17;
                break;
            }
            case OP_RETENTION:
            {
                READ_ID(chatid,1);
                READ_ID(userid,9);
                READ_32(period, 17);

                CHATD_LOG_DEBUG("Retention policy change on %s by %s to %u second(s)", chatid.c_str(), userid.c_str(), period);
                pos += 21;
                break;
            }
            case OP_MSGID:
            {
                READ_ID(msgxid, 1);
                READ_ID(msgid, 9);
                CHATD_LOG_DEBUG("Sent message ID confirmed: %s", msgxid.c_str());
                pos += 17;
                mClient.msgConfirm(msgxid, msgid);
                break;
            }
            case OP_RANGE:
            {
                READ_ID(chatid, 1);
                READ_ID(oldest, 9);
                READ_ID(newest, 17);
                CHATD_LOG_DEBUG("Known chat message IDs - oldest: %s, newest: %s",
                                oldest.c_str, newest.c_str());
                pos += 25;
                mClient.msgCheck(chatid, newest);
                break;
            }
            case OP_REJECT:
            {
                READ_32(op, 9);
                READ_32(code, 13); //TODO: what's this code?
                CHATD_LOG_DEBUG("Command was rejected: %d / %d", op, code);
                pos += 17;
                if (op == OP_NEWMSG)  // the message was rejected
                {
                    READ_ID(msgid, 1);
                    mClient.msgConfirm(msgid, 0);
                }
                else
                {
                    CHATD_LOG_WARNING("Something (not NEWMSG) was rejected, but not sure what, ignoring");
                }
                break;
            }
            case OP_HISTDONE:
            {
                READ_ID(chatid, 1);
                CHATD_LOG_DEBUG("History retrieval of chat %s finished" + ID_CSTR(chatid));
//                self.chatd.trigger('onMessagesHistoryDone',
                pos += 9;
                break;
            }
            default:
                CHATD_LOG_ERROR("Unknown opcode %d, ignoring all subsequent comment", opcode);
                return;
        }

        if (pos >= len)
        {
            CHATD_LOG_ERROR("Short WebSocket frame - got %d, expected %d", len, pos);
            // remove the command from the queue, its already processed, if this is not done, the code will loop forever
            return;
        }
      }
      catch(Buffer::RangeError& e)
      {
            CHATD_LOG_ERROR("Bound check error while parsing command: %s\nAborting command processing");
            return;
      }
      catch(std::exception& e)
      {
            CHATD_LOG_ERROR("Exception while processing incoming command: %s", e.what());
      }
    }
}

void Client::join(const Id& chatid, int shardNo, const std::string& url)
{
    bool isNew;
    if (mMessagesForChatId.find(chatid) != mMessagesForChatId.end())
        throw std::runtime_error("Client::join: Already joined chat "+chatid);

    Connection& conn = getOrCreateConnection(chatid, shardNo, url, isNew);
    mMessagesForChatId[chatid] = new Messages(conn, chatid);
    if (!isNew)
    {
        conn.join(chatid);
    }
}

// submit a new message to the chatid
size_t Client::msgSubmit(const Id& chatid, const char* msg, size_t msglen)
{
    return chatidMessages(chatid).submit(message);
}

void Connection::msgSend(const Id& chatid, const Message& message)
{
    sendCommand(Command(OP_NEWMSG) + chatid + Id(0) + message.id + (uint32_t)htonl(message.ts) + message);
}

void Connection::msgUpdate(const Id& chatid, const Message& message)
{
    sendCommand(Command(OP_MSGUPD) + chatid + Id(0) + message.id + (uint32_t)0 + message);
}

size_t Messages::submit(const char* msg, size_t msglen)
{
    // allocate a transactionid for the new message
    const Id& msgxid = mConnection.mClient.nextTransactionId();

    // write the new message to the message buffer and mark as in sending state
    push_forward(new Message(msgxid, this.mClient.mUserId, time(NULL), msg, msglen));
    auto num = mSending[msgxid] = highnum();

    // if we believe to be online, send immediately
    if (isOnline())
    {
        chatIdConn(mChatId).msgSend(mChatId, message);
    }
    return num;
}

void Messages::onLastReceived(const Id& msgid)
{
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
        throw std::runtime_error("Messages::onLastReceived: Unknown msgid %s for chatid %s", ID_CSTR(msgid), ID_CSTR(mChatId));
    mLastReceivedIdx = it->second;
}

void Messages::onLastSeen(const Id& msgid)
{
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
        throw std::runtime_error("Messages::onLastSeen: Unknown msgid %s for chatid %s", ID_CSTR(msgid), ID_CSTR(mChatId));
    mLastSeenIdx = it->second;
}

void Messages::modify(size_t msgnum, const char* msgdata, size_t msglen)
{
    // modify pending message so that a potential resend includes the change
    Message& msg = at(msgnum);
    // record this modification for resending purposes
    auto it = mSending.find(msg.id); //id is a msgxid
    if (it != mSending.end())
    {
            it->second->assign(msgdata, msglen);
    }
    else
    {
        auto& conn = chatIdConn(mChatId);
        if (conn.isOnline())
        {
            conn.msgUpdate(mChatId, *msg);
        }
    }
    mModified[msgnum] = &msg;
}

void Messages::resendPending()
{
    // resend all pending new messages and modifications
    for (auto& item: mSending)
    {
        mConnection.msgSend(mChatId, *item.second);
    }
    for (auto& item: mModified)
    {    // resend all pending modifications of completed messages
        mConnection.msgUpdate(mChatId, *item.second);
    }
}

// after a reconnect, we tell the chatd the oldest and newest buffered message
void Messages::range(const Id& chatid)
{
    Id lowid;
    Id highid;
    size_t highest = highnum();
    for (low = lownum(); low <= highest; low++)
    {
        auto lowmsg = at(low);
        auto sit = mSending.find(lowmsg.id);
        if (sit == mSending.end()) //message is not being sent
        {
            lowid = lowmsg.id;
            for (size_t high = highest; high > low; high--)
            {
                auto highmsg = at(high);
                sit = mSending.find(highmsg.id);
                if (sit == mSending.end())
                {
                    highid = highmsg.id;
                    break;
                }
            }
            if (!highid)
            {
                highid = lowid;
            }
            mConnection.sendCommand(Command(OP_RANGE) + chatid + lowid + highid);
            return;
        }
    }
}

void Client::msgConfirm(const Id& msgxid, const Id& msgid)
{
    // CHECK: is it more efficient to keep a separate mapping of msgxid to messages?
    for (auto& messages: mMessagesForChatId)
    {
        if (messages.second.confirm(sit, msgid))
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
    Message& msg = mMessages[num];
    //update transaction id to the actual msgid
    msg.id = msgid;
    mIdToIndexMap[msgid] = num;
    if (!msgid)
    {
//        this.chatd.trigger('onMessageReject', {
    }
    else
    {
//        this.chatd.trigger('onMessageConfirm', {
    }

    // we now have a proper msgid, resend MSGUPD in case the edit crossed the execution of the command
    if (mModified.find(num) != mModified.end())
    {
        mConnection.msgUpdate(chatid, msg);
    }
    return true;
}

size_t Client::msgStore(bool isNew, const Id& chatid, const Id& userid, const Id& msgid, uint32_t ts, const char* msg, size_t msglen)
{
    return chatidMessages(chatid).store(isNew, userid, msgid, ts, msg, msglen);
}

size_t Messages::store(bool isNew, const Id& userid, const Id& msgid, uint32_t timestamp,const char* msg, size_t msg)
{
    Message* msg = new Message(msgid, userid, timestamp, msg, msglen);
    // store message
    size_t idx;
    if (isNew)
    {
        push_forward(msg);
        idx = highnum();
        mLastReceivedIdx = idx;
    }
    else
    {
        push_backward(msg);
        idx = lownum();
    }
    mMsgToIndexMap[msgid] = idx;
    return idx;
}

void Client::onMsgUpdCommand(const Id& chatid, const Id& msgid, const char* msg, size_t msglen)
{
    // an existing message has been modified
    chatidMessages(chatid).onMsgUpdCommand(msgid, msg, msglen);
}

void Messages::onMsgUpdCommand(const Id& msgid, const char* msgdata, size_t msglen)
{
    // CHECK: is it more efficient to maintain a full hash msgid -> num?
    // FIXME: eliminate namespace clash collision risk
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    {
        CHATD_LOG_ERROR("Unknown chat id %s", ID_CSTR(chatid));
        return;
    }
    size_t idx = it->second;
    // if we modified the message, remove from this.modified.
    // if someone else did before us, resend the MSGUPD (might be redundant)
    auto it = mModified.find(idx);
    if (it != mModified.end())
    {
        if (msg.dataEquals(msgdata, msglen))
        {
            mModified.erase(it);
        }
        else
        {
            mConnection.msgUpdate(mChatId, msg);
        }
    }
    else //not in mModified
    {
        msg.assign(msg, msglen);
        //Notify GUI about updated message
    }
}

bool Client::msgCheck(const Id& chatid, const Id& msgid)
{
    auto it = mMessagesForChatId.find(chatid);
    if (it == mMessagesForChatId.end())
        return false;
    return it->second->check(chatid, msgid);
}

bool Messages::check(const Id& chatid, const Id& msgid)
{
    Message& msg = *at(highnum());
    // if the newest held message is not current, initiate a fetch of newer messages just in case
    if (msg.id != msgid)
    {
        mConnection.mClient.sendCommand(Command(P_HIST) + chatid + uint32_t(32));
        return true;
    }
    return false;
}


static char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
std::string base64urlencode(const unsigned char *data, size_t inlen)
{
    std::string encoded_data;
    encoded_data.reserve(((inlen+2) / 3) * 4);
    for (int i = 0; i < inlen;)
    {
        uint8_t octet_a = i < inlen ? data[i++] : 0;
        uint8_t octet_b = i < inlen ? data[i++] : 0;
        uint8_t octet_c = i < inlen ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data+= encoding_table[(triple >> 18) & 0x3F];
        encoded_data+= encoding_table[(triple >> 12) & 0x3F];
        encoded_data+= encoding_table[(triple >> 6) & 0x3F];
        encoded_data+= encoding_table[triple & 0x3F];
    }
    int mod = inlen % 3;
    if (mod)
    {
        encoded_data.resize(encoded_data.size() - (3 - mod));
    }
    return encoded_data;
}

