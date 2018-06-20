#include "presenced.h"
#include "chatClient.h"

using namespace std;
using namespace promise;
using namespace karere;

#if WIN32
#include <mega/utils.h>
using ::mega::mega_snprintf;   // enables the calls to snprintf below which are #defined
#endif

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

Client::Client(MyMegaApi *api, karere::Client *client, Listener& listener, uint8_t caps)
: mListener(&listener), karereClient(client), mApi(api), mCapabilities(caps), usingipv6(false)
{}

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

void Client::wsConnectCb()
{
    PRESENCED_LOG_DEBUG("Presenced connected");
    setConnState(kConnected);
    assert(!mConnectPromise.done());
    mConnectPromise.resolve();
}
    
void Client::notifyLoggedIn()
{
    assert(mConnState < kLoggedIn);
    assert(mConnectPromise.succeeded());
    assert(!mLoginPromise.done());
    setConnState(kLoggedIn);
    mLoginPromise.resolve();
}

void Client::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    onSocketClose(errcode, errtype, preason);
}
    
void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    PRESENCED_LOG_WARNING("Socket close, reason: %s", reason.c_str());
    
    mHeartbeatEnabled = false;
    auto oldState = mConnState;
    setConnState(kDisconnected);

    if (oldState == kDisconnected)
        return;

    usingipv6 = !usingipv6;

    if (oldState < kLoggedIn) //tell retry controller that the connect attempt failed
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
            && mConfig.mAutoawayActive
            && !karereClient->isCallInProgress();
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

        setConnState(kResolving);
        auto wptr = weakHandle();
        return retry("presenced", [this](int no, DeleteTrackable::Handle wptr)
        {
            if (wptr.deleted())
            {
                PRESENCED_LOG_DEBUG("Reconnect attempt initiated, but presenced client was deleted.");

                promise::Promise<void> pms = Promise<void>();
                pms.resolve();
                return pms;
            }

            disconnect();
            mConnectPromise = Promise<void>();
            mLoginPromise = Promise<void>();

            setConnState(kResolving);
            PRESENCED_LOG_DEBUG("Resolving hostname %s...", mUrl.host.c_str());

            int status = wsResolveDNS(karereClient->websocketIO, mUrl.host.c_str(),
                         [wptr, this](int status, std::string ipv4, std::string ipv6)
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed, but presenced client was deleted.");
                    return;
                }
                if (mConnState != kResolving)
                {
                    PRESENCED_LOG_DEBUG("Connection state changed while resolving DNS.");
                    return;
                }

                if (status < 0)
                {
                    PRESENCED_LOG_ERROR("Async DNS error in presenced. Error code: %d", status);
                    string errStr = "Async DNS error in presenced. Error code: "+std::to_string(status);
                    if (!mConnectPromise.done())
                    {
                        mConnectPromise.reject(errStr, status, kErrorTypeGeneric);
                    }
                    if (!mLoginPromise.done())
                    {
                        mLoginPromise.reject(errStr, status, kErrorTypeGeneric);
                    }
                    return;
                }

                setConnState(kConnecting);
                string ip = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;
                PRESENCED_LOG_DEBUG("Connecting to presenced using the IP: %s", ip.c_str());
                bool rt = wsConnect(karereClient->websocketIO, ip.c_str(),
                      mUrl.host.c_str(),
                      mUrl.port,
                      mUrl.path.c_str(),
                      mUrl.isSecure);
                if (!rt)
                {
                    string otherip;
                    if (ip == ipv6 && ipv4.size())
                    {
                        otherip = ipv4;
                    }
                    else if (ip == ipv4 && ipv6.size())
                    {
                        otherip = ipv6;
                    }

                    if (otherip.size())
                    {
                        PRESENCED_LOG_DEBUG("Connection to presenced failed. Retrying using the IP: %s", otherip.c_str());
                        if (wsConnect(karereClient->websocketIO, otherip.c_str(),
                                      mUrl.host.c_str(),
                                      mUrl.port,
                                      mUrl.path.c_str(),
                                      mUrl.isSecure))
                        {
                            return;
                        }
                    }

                    onSocketClose(0, 0, "Websocket error on wsConnect (presenced)");
                }
            });

            if (status < 0)
            {
                PRESENCED_LOG_ERROR("Sync DNS error in presenced. Error code: %d", status);
                string errStr = "Sync DNS error in presenced. Error code: "+std::to_string(status);
                if (!mConnectPromise.done())
                {
                    mConnectPromise.reject(errStr, status, kErrorTypeGeneric);
                }
                if (!mLoginPromise.done())
                {
                    mLoginPromise.reject(errStr, status, kErrorTypeGeneric);
                }
            }
            
            return mConnectPromise
            .then([wptr, this]()
            {
                if (wptr.deleted())
                    return;

                mTsLastPingSent = 0;
                mTsLastRecv = time(NULL);
                mHeartbeatEnabled = true;
                login();
            });
        }, wptr, karereClient->appCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL);
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

void Client::disconnect()
{
    setConnState(kDisconnected);
    if (wsIsConnected())
    {
        wsDisconnect(true);
    }

    onSocketClose(0, 0, "terminating");
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

bool Client::sendBuf(Buffer&& buf)
{
    if (!isOnline())
        return false;
    
    bool rc = wsSendMessage(buf.buf(), buf.dataSize());
    buf.free();  //just in case, as it's content is xor-ed with the websock datamask so it's unusable
    mTsLastSend = time(NULL);
    return rc && isOnline();
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
            uint32_t numPeers = read<uint32_t>(1);
            string tmpString;
            tmpString.append("ADDPEERS - ");
            tmpString.append(to_string(numPeers));
            tmpString.append(" peer/s: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(5+i*8);
                tmpString.append(ID_CSTR(peerId));
                if (i + 1 < numPeers)
                    tmpString.append(", ");
            }
            snprintf(buf, bufsize, "%s",tmpString.c_str());
            break;
        }
        case OP_DELPEERS:
        {
            uint32_t numPeers = read<uint32_t>(1);
            string tmpString;
            tmpString.append("DELPEERS - ");
            tmpString.append(to_string(numPeers));
            tmpString.append(" peer/s: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(5+i*8);
                tmpString.append(ID_CSTR(peerId));
                if (i + 1 < numPeers)
                    tmpString.append(", ");
            }
            snprintf(buf, bufsize, "%s",tmpString.c_str());
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
    disconnect();
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

void Client::wsHandleMsgCb(char *data, size_t len)
{
    mTsLastRecv = time(NULL);
    mTsLastPingSent = 0;
    handleMessage(StaticBuffer(data, len));
}

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
    CALL_LISTENER(onConnStateChange, mConnState);

    if (newState == kDisconnected)
    {
        // if disconnected, we don't really know the presence status anymore
        for (auto it = mCurrentPeers.begin(); it != mCurrentPeers.end(); it++)
        {
            CALL_LISTENER(onPresenceChange, it->first, Presence::kInvalid);
        }
        CALL_LISTENER(onPresenceChange, mMyHandle, Presence::kInvalid);
    }
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
