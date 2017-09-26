#include "presenced.h"
#include <base/cservices.h>
#include <gcmpp.h>
#include <retryHandler.h>
#include <libws_log.h>
#include <event2/dns.h>
#include <event2/dns_compat.h>
#include <chatClient.h>
#include <arpa/inet.h>
#include <libws.h>

#ifdef __ANDROID__
    #include <sys/system_properties.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #ifdef TARGET_OS_IPHONE
        #include <resolv.h>
    #endif
#endif

using namespace std;
using namespace promise;
using namespace karere;

#define ID_CSTR(id) id.toString().c_str()
#define PRESENCED_LOG_LISTENER_CALLS

#ifdef PRESENCED_LOG_LISTENER_CALLS
    #define LOG_LISTENER_CALL(fmtString,...) PRESENCED_LOG_DEBUG(fmtString, ##__VA_ARGS__)
#else
    #define LOG_LISTENER_CALL(...)
#endif

#define CALL_LISTENER(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
          LOG_LISTENER_CALL("Calling Listener::" #methodName "()");                       \
          mListener->methodName(__VA_ARGS__);                                                   \
      } catch(std::exception& e) {                                                              \
          PRESENCED_LOG_WARNING("Exception thrown from Listener::" #methodName "():\n%s", e.what());\
      }                                                                                         \
    } while(0)


namespace presenced
{
ws_base_s Client::sWebsocketContext;
bool Client::sWebsockCtxInitialized = false;

    Client::Client(MyMegaApi *api, Listener& listener, uint8_t caps)
: mListener(&listener), mApi(api), mCapabilities(caps)
{
    if (!sWebsockCtxInitialized)
        initWebsocketCtx();
}

void Client::initWebsocketCtx()
{
    assert(!sWebsockCtxInitialized);
    ws_global_init(&sWebsocketContext, services_get_event_loop(), NULL,
        [](struct bufferevent* bev, void* userp)
        {
            marshallCall([bev, userp]()
            {
                //CHATD_LOG_DEBUG("Read event");
                ws_read_callback(bev, userp);
            });
        },
        [](struct bufferevent* bev, short events, void* userp)
        {
            marshallCall([bev, events, userp]()
            {
                //CHATD_LOG_DEBUG("Buffer event 0x%x", events);
                ws_event_callback(bev, events, userp);
            });
        },
        [](int fd, short events, void* userp)
        {
            marshallCall([events, userp]()
            {
                //CHATD_LOG_DEBUG("Timer %p event", userp);
                ws_handle_marshall_timer_cb(0, events, userp);
            });
        });
//        ws_set_log_cb(ws_default_log_cb);
//        ws_set_log_level(LIBWS_TRACE);
    sWebsockCtxInitialized = true;
}

#define checkLibwsCall(call, opname) \
    do {                             \
        int _cls_ret = (call);       \
        if (_cls_ret) throw std::runtime_error("Websocket error " +std::to_string(_cls_ret) + \
        " on operation " #opname);   \
    } while(0)

//Stale event from a previous connect attempt?
#define ASSERT_NOT_ANOTHER_WS(event)    \
    if (ws != self.mWebSocket && self.mWebSocket) {   \
        PRESENCED_LOG_WARNING("Websocket '" event     \
        "' callback: ws param %p is not equal to self.mWebSocket %p, ignoring", \
        ws, self.mWebSocket);                         \
    }

promise::Promise<void>
Client::connect(const std::string& url, Id myHandle, IdRefMap&& currentPeers,
    const Config& config)
{
    mMyHandle = myHandle;
    mConfig = config;
    mCurrentPeers = std::move(currentPeers);
    return reconnect(url);
}

void Client::pushPeers()
{
    Command cmd(OP_ADDPEERS, 4 + mCurrentPeers.size()*8);
    cmd.append<uint32_t>(mCurrentPeers.size());
    for (auto& peer: mCurrentPeers)
    {
        cmd.append<uint64_t>(peer.first);
    }
    if (cmd.dataSize() > 1)
    {
        sendCommand(std::move(cmd));
    }
}

void Client::websockConnectCb(ws_t ws, void* arg)
{
    Client& self = *static_cast<Client*>(arg);
    auto wptr = self.getDelTracker();
    ASSERT_NOT_ANOTHER_WS("connect");
    PRESENCED_LOG_DEBUG("Presenced connected");
    marshallCall([&self, wptr]()
    {
        if (wptr.deleted())
            return;
        self.setConnState(kConnected);
        self.mConnectPromise.resolve();
    });
}

void Client::websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg)
{
    Client& self = *static_cast<Client*>(arg);
    ASSERT_NOT_ANOTHER_WS("message");
    self.mTsLastRecv = time(NULL);
    self.mTsLastPingSent = 0;
    self.handleMessage(StaticBuffer(msg, len));
}

void Client::notifyLoggedIn()
{
    assert(mConnState < kLoggedIn);
    assert(mConnectPromise.succeeded());
    assert(!mLoginPromise.done());
    setConnState(kLoggedIn);
    mLoginPromise.resolve();
}

void Client::websockCloseCb(ws_t ws, int errcode, int errtype, const char *preason,
                                size_t reason_len, void *arg)
{
    auto& self = *static_cast<Client*>(arg);
    auto track = self.getDelTracker();
    ASSERT_NOT_ANOTHER_WS("close/error");
    std::string reason;
    if (preason)
        reason.assign(preason, reason_len);

    //we don't want to initiate websocket reconnect from within a websocket callback
    marshallCall([&self, track, reason, errcode, errtype]()
    {
        if (track.deleted())
            return;
        self.onSocketClose(errcode, errtype, reason);
    });
}

void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    PRESENCED_LOG_WARNING("Socket close, reason: %s", reason.c_str());
    
    mHeartbeatEnabled = false;
    if (mWebSocket)
    {
        ws_destroy(&mWebSocket);
    }
    if (mConnState == kDisconnected)
        return;

    if (mConnState < kLoggedIn) //tell retry controller that the connect attempt failed
    {
        assert(!mLoginPromise.succeeded());
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
        if (!mLoginPromise.done())
        {
            mLoginPromise.reject(reason, errcode, errtype);
        }
    }
    else
    {
        setConnState(kDisconnected);
        reconnect(); //start retry controller
    }
}

std::string Config::toString() const
{
    std::string result;
    result.reserve(64);
    result.append("pres: ").append(mPresence.toString())
          .append(", persist: ").append(mPersist ? "1":"0")
          .append(", aaActive: ").append(mAutoawayActive ? "1":"0")
          .append(", aaTimeout: ").append(std::to_string(mAutoawayTimeout));
    return result;
}

bool Client::setPresence(Presence pres)
{
    if (pres == mConfig.mPresence)
        return true;
    mConfig.mPresence = pres;
    auto ret = sendPrefs();
    signalActivity(true);
    PRESENCED_LOG_DEBUG("setPresence-> %s", pres.toString());
    return ret;
}

bool Client::setPersist(bool enable)
{
    if (enable == mConfig.mPersist)
        return true;
    mConfig.mPersist = enable;
    signalActivity(true);
    return sendPrefs();
}

bool Client::setAutoaway(bool enable, time_t timeout)
{
    if (enable)
    {
        mConfig.mPersist = false;
    }
    mConfig.mAutoawayTimeout = timeout;
    mConfig.mAutoawayActive = enable;
    signalActivity(true);
    return sendPrefs();
}

bool Client::autoAwayInEffect()
{
    return mConfig.mPresence.isValid()    // don't want to change to away from default status
            && !mConfig.mPersist
            && mConfig.mPresence != Presence::kOffline
            && mConfig.mPresence != Presence::kAway
            && mConfig.mAutoawayTimeout
            && mConfig.mAutoawayActive;
}

void Client::signalActivity(bool force)
{
    if (!mConfig.mPresence.isValid())
        return;

    mTsLastUserActivity = time(NULL);
    if (mConfig.mPresence == Presence::kAway)
        sendUserActive(false);
    else if (mConfig.mPresence != Presence::kOffline)
        sendUserActive(true, force);
}

Promise<void>
Client::reconnect(const std::string& url)
{
    assert(!mHeartbeatEnabled);
    try
    {
        if (mConnState >= kConnecting) //would be good to just log and return, but we have to return a promise
            return promise::Error("Already connecting/connected");
        if (!url.empty())
        {
            mUrl.parse(url);
        }
        else
        {
            if (!mUrl.isValid())
                return promise::Error("No valid URL provided and current URL is not valid");
        }

        setConnState(kConnecting);
        return retry("presenced", [this](int no)
        {
            reset();
            mConnectPromise = Promise<void>();
            mLoginPromise = Promise<void>();
            PRESENCED_LOG_DEBUG("Attempting connect...");
            checkLibwsCall((ws_init(&mWebSocket, &Client::sWebsocketContext)), "create socket");
            ws_set_onconnect_cb(mWebSocket, &websockConnectCb, this);
            ws_set_onclose_cb(mWebSocket, &websockCloseCb, this);
            ws_set_onmsg_cb(mWebSocket, &websockMsgCb, this);

            if (mUrl.isSecure)
            {
                ws_set_ssl_state(mWebSocket, LIBWS_SSL_SELFSIGNED);
            }

            auto wptr = weakHandle();
            mApi->call(&::mega::MegaApi::queryDNS, mUrl.host.c_str())
            .then([wptr, this](ReqResult result)
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed, but presenced client was deleted.");
                    return;
                }
                if (!mWebSocket)
                {
                    PRESENCED_LOG_DEBUG("Disconnect called while resolving DNS.");
                    return;
                }
                if (mConnState != kConnecting)
                {
                    PRESENCED_LOG_DEBUG("Connection state changed while resolving DNS.");
                    return;
                }

                string ip = result->getText();
                PRESENCED_LOG_DEBUG("Connecting to presenced using the IP: %s", ip.c_str());
                
                if (ip[0] == '[')
                {
                    struct sockaddr_in6 ipv6addr = { 0 };
                    ip = ip.substr(1, ip.size() - 2);
                    ipv6addr.sin6_family = AF_INET6;
                    ipv6addr.sin6_port = htons(mUrl.port);
                    inet_pton(AF_INET6, ip.c_str(), &ipv6addr.sin6_addr);
                    checkLibwsCall((ws_connect_addr(mWebSocket, mUrl.host.c_str(),
                                                    (struct sockaddr *)&ipv6addr, sizeof(ipv6addr),
                                                    mUrl.port, (mUrl.path).c_str())), "connect");
                }
                else
                {
                    struct sockaddr_in ipv4addr = { 0 };
                    ipv4addr.sin_family = AF_INET;
                    ipv4addr.sin_port = htons(mUrl.port);
                    inet_pton(AF_INET, ip.c_str(), &ipv4addr.sin_addr);
                    checkLibwsCall((ws_connect_addr(mWebSocket, mUrl.host.c_str(),
                                                    (struct sockaddr *)&ipv4addr, sizeof(ipv4addr),
                                                    mUrl.port, (mUrl.path).c_str())), "connect");
                }
            })
            .fail([this](const promise::Error& err)
            {
                if (err.type() == ERRTYPE_MEGASDK)
                {
                    mConnectPromise.reject(err.msg(), err.code(), err.type());
                    mLoginPromise.reject(err.msg(), err.code(), err.type());
                }
            });
            
            return mConnectPromise
            .then([this]()
            {
                mTsLastPingSent = 0;
                mTsLastRecv = time(NULL);
                mHeartbeatEnabled = true;
                return login();
            });
        }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL);
    }
    KR_EXCEPTION_TO_PROMISE(kPromiseErrtype_presenced);
}
bool Client::sendKeepalive(time_t now)
{
    mTsLastPingSent = now ? now : time(NULL);
    return sendCommand(Command(OP_KEEPALIVE));
}

void Client::heartbeat()
{
    // if a heartbeat is received but we are already offline...
    if (!mHeartbeatEnabled)
        return;

    auto now = time(NULL);
    if (autoAwayInEffect())
    {
        if (now - mTsLastUserActivity > mConfig.mAutoawayTimeout)
        {
            sendUserActive(false);
        }
    }

    bool needReconnect = false;
    if (now - mTsLastSend > kKeepaliveSendInterval)
    {
        if (!sendKeepalive(now))
        {
            needReconnect = true;
            PRESENCED_LOG_WARNING("Failed to send keepalive, reconnecting...");
        }
    }
    else if (mTsLastPingSent)
    {
        if (now - mTsLastPingSent > kKeepaliveReplyTimeout)
        {
            PRESENCED_LOG_WARNING("Timed out waiting for KEEPALIVE response, reconnecting...");
            needReconnect = true;
        }
    }
    else if (now - mTsLastRecv >= kKeepaliveSendInterval)
    {
        if (!sendKeepalive(now))
        {
            needReconnect = true;
            PRESENCED_LOG_WARNING("Failed to send keepalive, reconnecting...");
        }
    }
    if (needReconnect)
    {
        setConnState(kDisconnected);
        mHeartbeatEnabled = false;
        reconnect();
    }
}

void Client::disconnect() //should be graceful disconnect
{
    mHeartbeatEnabled = false;
    mTerminating = true;
    reset();
    setConnState(kDisconnected);
}

promise::Promise<void> Client::retryPendingConnection()
{
    if (mUrl.isValid())
    {
        setConnState(kDisconnected);
        mHeartbeatEnabled = false;
        PRESENCED_LOG_WARNING("Retry pending connections...");
        return reconnect();
    }
    return promise::Error("No valid URL provided to retry pending connections");
}

void Client::reset() //immediate disconnect
{
    if (!mWebSocket)
        return;

    ws_close_immediately(mWebSocket);
    ws_destroy(&mWebSocket);
    assert(!mWebSocket);
}

bool Client::sendBuf(Buffer&& buf)
{
    if (!isOnline())
        return false;
//WARNING: ws_send_msg_ex() is destructive to the buffer - it applies the websocket mask directly
//Copy the data to preserve the original
    auto rc = ws_send_msg_ex(mWebSocket, buf.buf(), buf.dataSize(), 1);
    buf.free(); //just in case, as it's content is xor-ed with the websock datamask so it's unusable
    bool result = (!rc && isOnline());
    mTsLastSend = time(NULL);
    return result;
}
bool Client::sendCommand(Command&& cmd)
{
    if (krLoggerWouldLog(krLogChannel_presenced, krLogLevelDebug))
        logSend(cmd);
    bool result = sendBuf(std::move(cmd));
    if (!result)
        PRESENCED_LOG_DEBUG("  Can't send, we are offline");
    return result;
}

bool Client::sendCommand(const Command& cmd)
{
    Buffer buf(cmd.buf(), cmd.dataSize());
    if (krLoggerWouldLog(krLogChannel_presenced, krLogLevelDebug))
        logSend(cmd);
    auto result = sendBuf(std::move(buf));
    if (!result)
        PRESENCED_LOG_DEBUG("  Can't send, we are offline");
    return result;
}
void Client::logSend(const Command& cmd)
{
    char buf[512];
    cmd.toString(buf, 512);
    krLoggerLog(krLogChannel_presenced, krLogLevelDebug, "send %s\n", buf);
}

//only for sent commands
void Command::toString(char* buf, size_t bufsize) const
{
    auto op = opcode();
    switch (op)
    {
        case OP_USERACTIVE:
        {
            auto code = read<uint8_t>(1);
            snprintf(buf, bufsize, "USERACTIVE - %d", code);
            break;
        }
        case OP_PREFS:
        {
            Config config(read<uint16_t>(1));
            snprintf(buf, bufsize, "PREFS - %s", config.toString().c_str());
            break;
        }
        case OP_HELLO:
        {
            uint8_t caps = read<uint8_t>(2);
            snprintf(buf, bufsize, "HELLO - version 0x%02X, caps: (%s,%s)",
                read<uint8_t>(1),
                (caps & karere::kClientCanWebrtc) ? "webrtc" : "nowebrtc",
                (caps & karere::kClientIsMobile) ? "mobile" : "desktop");
            break;
        }
        case OP_ADDPEERS:
        {
            snprintf(buf, bufsize, "ADDPEERS - %u peers", read<uint32_t>(1));
            break;
        }
        case OP_DELPEERS:
        {
            snprintf(buf, bufsize, "DELPEERS - %u peers", read<uint32_t>(1));
            break;
        }
        default:
        {
            snprintf(buf, bufsize, "%s", opcodeName());
            break;
        }
    }
    buf[bufsize-1] = 0; //terminate, just in case
}

void Client::login()
{
    sendCommand(Command(OP_HELLO) + (uint8_t)kProtoVersion+mCapabilities);

    if (mPrefsAckWait)
    {
        sendPrefs();
    }
    sendUserActive((time(NULL) - mTsLastUserActivity) < mConfig.mAutoawayTimeout, true);
    pushPeers();
}

bool Client::sendUserActive(bool active, bool force)
{
    if ((active == mLastSentUserActive) && !force)
        return true;
    bool sent = sendCommand(Command(OP_USERACTIVE) + (uint8_t)(active ? 1 : 0));
    if (!sent)
        return false;
    mLastSentUserActive = active;
    return true;
}

bool Client::sendPrefs()
{
    mPrefsAckWait = true;
    configChanged();
    return sendCommand(Command(OP_PREFS) + mConfig.toCode());
}

void Client::configChanged()
{
    CALL_LISTENER(onPresenceConfigChanged, mConfig, mPrefsAckWait);
}

void Config::fromCode(uint16_t code)
{
    mPresence = (code & 3) + karere::Presence::kOffline;
    mPersist = !!(code & 4);
    mAutoawayActive = !(code & 8);
    mAutoawayTimeout = code >> 4;
    if (mAutoawayTimeout > 600)
        mAutoawayTimeout = (600+(mAutoawayTimeout-600)*60);
}

uint16_t Config::toCode() const
{
    return ((mPresence.code() - karere::Presence::kOffline) & 3)
          | (mPersist ? 4 : 0)
          | (mAutoawayActive ? 0 : 8)
          | (((mAutoawayTimeout > 600)
               ? (600+(mAutoawayTimeout-600)/60)
               : mAutoawayTimeout)
            << 4);
}

Client::~Client()
{
    reset();
    CALL_LISTENER(onDestroy); //we don't delete because it may have its own idea of its lifetime (i.e. it could be a GUI class)
}

#define READ_ID(varname, offset)\
    assert(offset==pos-base); Id varname(buf.read<uint64_t>(pos)); pos+=sizeof(uint64_t)
#define READ_CHATID(offset)\
    assert(offset==pos-base); chatid = buf.read<uint64_t>(pos); pos+=sizeof(uint64_t)

#define READ_32(varname, offset)\
    assert(offset==pos-base); uint32_t varname(buf.read<uint32_t>(pos)); pos+=4
#define READ_16(varname, offset)\
    assert(offset==pos-base); uint16_t varname(buf.read<uint16_t>(pos)); pos+=2
#define READ_8(varname, offset)\
    assert(offset==pos-base); uint8_t varname(buf.read<uint8_t>(pos)); pos+=1

// inbound command processing
void Client::handleMessage(const StaticBuffer& buf)
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
        switch (opcode)
        {
            case OP_KEEPALIVE:
            {
                PRESENCED_LOG_DEBUG("recv KEEPALIVE");
                break;
            }
            case OP_PEERSTATUS:
            {
                READ_8(pres, 0);
                READ_ID(userid, 1);
                // READ_8(webrtc_capability, 7);
                PRESENCED_LOG_DEBUG("recv PEERSTATUS - user '%s' with presence %s",
                    ID_CSTR(userid), Presence::toString(pres));
                CALL_LISTENER(onPresenceChange, userid, pres);
                break;
            }
            case OP_PREFS:
            {
                if (mConnState < kLoggedIn)
                    notifyLoggedIn();
                READ_16(prefs, 0);
                if (mPrefsAckWait && prefs == mConfig.toCode()) //ack
                {
                    PRESENCED_LOG_DEBUG("recv PREFS - server ack to the prefs we sent(0x%x)", prefs);
                }
                else
                {
                    mConfig.fromCode(prefs);
                    if (mPrefsAckWait)
                    {
                        PRESENCED_LOG_DEBUG("recv other PREFS while waiting for our PREFS ack, cancelling our send.\nPrefs: %s",
                          mConfig.toString().c_str());
                    }
                    else
                    {
                        PRESENCED_LOG_DEBUG("recv PREFS from another client: %s", mConfig.toString().c_str());
                    }
                }
                mPrefsAckWait = false;
                configChanged();
                break;
            }
            default:
            {
                PRESENCED_LOG_ERROR("Unknown opcode %d, ignoring all subsequent commands", opcode);
                return;
            }
        }
      }
      catch(BufferRangeError& e)
      {
          PRESENCED_LOG_ERROR("Buffer bound check error while parsing %s:\n\t%s\n\tAborting command processing", Command::opcodeToStr(opcode), e.what());
          return;
      }
      catch(std::exception& e)
      {
          PRESENCED_LOG_ERROR("Exception while processing incoming %s: %s", Command::opcodeToStr(opcode), e.what());
          return;
      }
    }
}

void Client::setConnState(ConnState newState)
{
    if (newState == mConnState)
        return;
    mConnState = newState;
#ifndef LOG_LISTENER_CALLS
    PRESENCED_LOG_DEBUG("Connection state changed to %s", connStateToStr(mConnState));
#endif
    //dont use CALL_LISTENER because we need more intelligent logging
    try
    {
        mListener->onConnStateChange(mConnState);
    }
    catch(...){}
}
void Client::addPeer(karere::Id peer)
{
    int result = mCurrentPeers.insert(peer);
    if (result == 1) //refcount = 1, wasnt there before
    {
        sendCommand(Command(OP_ADDPEERS)+(uint32_t)(1)+peer);
    }
}
void Client::removePeer(karere::Id peer, bool force)
{
    auto it = mCurrentPeers.find(peer);
    if (it == mCurrentPeers.end())
    {
        PRESENCED_LOG_DEBUG("removePeer: Unknown peer %s", peer.toString().c_str());
        return;
    }
    if (--it->second > 0)
    {
        if (!force)
        {
            return;
        }
        else
        {
            PRESENCED_LOG_DEBUG("removePeer: Forcing delete of peer %s with refcount > 0", peer.toString().c_str());
        }
    }
    else //refcount reched zero
    {
        assert(it->second == 0);
    }
    mCurrentPeers.erase(it);
    sendCommand(Command(OP_DELPEERS)+(uint32_t)(1)+peer);
}
}
