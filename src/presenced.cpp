#include "presenced.h"
#include <base/cservices.h>
#include <gcmpp.h>
#include <retryHandler.h>
#include <libws_log.h>
#include <event2/dns.h>
#include <event2/dns_compat.h>

using namespace std;
using namespace promise;
using namespace karere;

#define ID_CSTR(id) id.toString().c_str()

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

Client::Client(Listener* listener, uint8_t flags)
: mListener(listener), mFlags(flags)
{
    if (!sWebsockCtxInitialized)
        initWebsocketCtx();
    CALL_LISTENER(onOwnPresence, Presence::kOffline);
}

void Client::initWebsocketCtx()
{
    assert(!sWebsockCtxInitialized);
    ws_global_init(&sWebsocketContext, services_get_event_loop(), services_dns_eventbase,
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
    if (ws != self.mWebSocket) {       \
        PRESENCED_LOG_WARNING("Websocket '" event "' callback: ws param is not equal to self->mWebSocket, ignoring"); \
    }

promise::Promise<void>
Client::connect(const std::string& url, Presence pres, bool force,
                karere::SetOfIds&& currentPeers)
{
    mCurrentPeers = std::move(currentPeers);
    return reconnect(url)
    .then([this, pres, force]()
    {
        setPresence(pres, force);
    });
}

void Client::syncPeers()
{
    Command cmd(OP_ADDPEERS, mCurrentPeers.size()*8);
    for (auto& peer: mCurrentPeers)
    {
        cmd.append<uint64_t>(peer);
    }
    if (cmd.dataSize() > 1)
    {
        sendCommand(std::move(cmd));
    }
}

void Client::websockConnectCb(ws_t ws, void* arg)
{
    Client& self = *static_cast<Client*>(arg);
    ASSERT_NOT_ANOTHER_WS("connect");
    assert(!self.mConnectPromise.done());
    self.setConnState(kStateConnected);
    self.mConnectPromise.resolve();
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
        track.throwIfDeleted();
        self.onSocketClose(errcode, errtype, reason);
    });
}

void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    PRESENCED_LOG_WARNING("Socket close, reason: %s", reason.c_str());
    if (errtype == WS_ERRTYPE_DNS)
    {
        PRESENCED_LOG_WARNING("->DNS error: forcing libevent to re-read /etc/resolv.conf");
        //if we didn't have our network interface up at app startup, and resolv.conf is
        //genereated dynamically, dns may never work unless we re-read the resolv.conf file
#ifndef _WIN32
        evdns_base_clear_host_addresses(services_dns_eventbase);
        evdns_base_resolv_conf_parse(services_dns_eventbase,
            DNS_OPTIONS_ALL & (~DNS_OPTION_SEARCH), "/etc/resolv.conf");
#else
        evdns_config_windows_nameservers();
#endif
    }
    disableInactivityTimer();
    if (mTerminating)
        return;

    if (mState == kStateConnecting) //tell retry controller that the connect attempt failed
    {
        assert(!mConnectPromise.done());
        mConnectPromise.reject(reason, errcode, errtype);
    }
    else
    {
        setConnState(kStateDisconnected);
        reconnect(); //start retry controller
    }
}

void Client::disableInactivityTimer()
{
    if (mInactivityTimer)
    {
        cancelInterval(mInactivityTimer);
        mInactivityTimer = 0;
    }
}

void Client::setPresence(Presence pres, bool force)
{
    if (force)
    {
        sendCommand(Command(OP_STATUSOVERRIDE)+pres.code());
    }
    else
    {
        //FIXME
        mPingCode = mFlags;
        if (pres.code() == Presence::kOnline)
            mPingCode |= 0x01;
        else if (pres.code() == Presence::kBusy)
            mPingCode |= 0x02;
        pingWithPresence();
    }
}

Promise<void>
Client::reconnect(const std::string& url)
{
    try
    {
        if (mState >= kStateConnecting) //would be good to just log and return, but we have to return a promise
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

        setConnState(kStateConnecting);
        return retry("presenced", [this](int no)
        {
            reset();
            mConnectPromise = Promise<void>();
            PRESENCED_LOG_DEBUG("Attempting connect...");
            checkLibwsCall((ws_init(&mWebSocket, &Client::sWebsocketContext)), "create socket");
            ws_set_onconnect_cb(mWebSocket, &websockConnectCb, this);
            ws_set_onclose_cb(mWebSocket, &websockCloseCb, this);
            ws_set_onmsg_cb(mWebSocket,
            [](ws_t ws, char *msg, uint64_t len, int binary, void *arg)
            {
                Client& self = *static_cast<Client*>(arg);
                ASSERT_NOT_ANOTHER_WS("message");
                self.mInactivityBeats = 0;
                self.handleMessage(StaticBuffer(msg, len));
            }, this);

            if (mUrl.isSecure)
            {
                ws_set_ssl_state(mWebSocket, LIBWS_SSL_SELFSIGNED);
            }
            checkLibwsCall((ws_connect(mWebSocket, mUrl.host.c_str(), mUrl.port, (mUrl.path).c_str())), "connect");
            return mConnectPromise;
        }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL)
        .then([this]()
        {
            enableInactivityTimer();
            return login();
        });
    }
    KR_EXCEPTION_TO_PROMISE(PRESENCED);
}

void Client::enableInactivityTimer()
{
    if (mInactivityTimer)
        return;

    mInactivityTimer = setInterval([this]()
    {
        if (mInactivityBeats++ > 3)
        {
            mState = kStateDisconnected;
            disableInactivityTimer();
            PRESENCED_LOG_WARNING("Connection inactive for too long, reconnecting...");
            reconnect();
        }
    }, 10000);
}

void Client::disconnect() //should be graceful disconnect
{
    mTerminating = true;
    if (mWebSocket)
        ws_close(mWebSocket);
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
    auto op = cmd.opcode();
    switch (op)
    {
        case OP_PING:
        {
            auto code = cmd.read<uint8_t>(1);
            const char* flags;
            if ((code & 0x03) == 0)
                flags = "Away";
            else if (code & 0x01)
                flags = "Online";
            else if (code & 0x02)
                flags = "DnD";
            else
                flags = "invalid(both DnD and Online set)";

            krLoggerLog(krLogChannel_presenced, krLogLevelDebug,
                "send PING - %s%s\n", flags, (code & 0x80)?"(mobile)":"");
        }
        case OP_STATUSOVERRIDE:
        {
            krLoggerLog(krLogChannel_presenced, krLogLevelDebug,
                "send STAUSOVERRIDE - presence: %s\n",
                Presence::toString(cmd.read<uint8_t>(1)));
            break;
        }
        case OP_HELLO:
        {
            krLoggerLog(krLogChannel_presenced, krLogLevelDebug,
                "send HELLO - version %0x04X\n",
                cmd.read<uint16_t>(0));
            break;
        }
        default:
        {
            krLoggerLog(krLogChannel_presenced, krLogLevelDebug, "send %s\n",
                cmd.opcodeName());
            break;
        }
    }
}

void Client::login()
{
    sendCommand(Command(OP_HELLO) + (uint16_t)kProtoVersion);
    syncPeers();
}

Client::~Client()
{
    disableInactivityTimer();
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
//        CHATD_LOG_DEBUG("RECV %s", Command::opcodeToStr(opcode));
        switch (opcode)
        {
            case OP_KEEPALIVE:
            {
                //CHATD_LOG_DEBUG("Server heartbeat received");
                pingWithPresence();
                break;
            }
            case OP_PEERSTATUS:
            {
                READ_8(pres, 0);
                READ_ID(userid, 1);
                PRESENCED_LOG_DEBUG("recv PEERSTATUS - user '%s' with presence %s",
                    ID_CSTR(userid), Presence::toString(pres));
                CALL_LISTENER(onPresence, userid, pres);
                break;
            }
            case OP_STATUSOVERRIDE:
            {
                READ_8(pres, 0);
                PRESENCED_LOG_DEBUG("recv STATUSOVERRIDE - presence %s", Presence::toString(pres));

                CALL_LISTENER(onOwnPresence, pres);
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
      }
    }
}

void Client::pingWithPresence()
{
    sendBuf(Command(OP_PING)+mPingCode);
}

void Client::setConnState(State newState)
{
    if (newState == mState)
        return;
    mState = newState;
#ifndef LOG_LISTENER_CALLS
    PRESENCED_LOG_DEBUG("Connection state changed to %s", connStateToStr(mState));
#endif
    mListener->onConnStateChange(mState);
}

}
