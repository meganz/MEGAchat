#include "presenced.h"
#include "chatClient.h"

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

    Client::Client(MyMegaApi *api, karere::Client *client, Listener& listener, uint8_t caps)
: mListener(&listener), karereClient(client), mApi(api), mCapabilities(caps)
{
}

void Client::connect(IdRefMap&& currentPeers, const Config& config)
{
    assert(!mRetryTimerHandle);
    bt.reset();

    mConfig = config;
    mCurrentPeers = std::move(currentPeers);

    // only trigger the connect-steps if haven't started yet or we've been disconnected
    if (mConnState == kConnNew || mConnState == kDisconnected)
    {
        getPresenceURL();
    }
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
    PRESENCED_LOG_INFO("Connection established successfully.");

    assert(mConnState == kConnecting);
    setConnState(kConnected);

    mTsLastPingSent = 0;
    mTsLastRecv = time(NULL);
    mHeartbeatEnabled = true;

    login();
}
    
void Client::notifyLoggedIn()
{
    PRESENCED_LOG_INFO("Login successful");

    assert(mConnState == kConnected);
    setConnState(kLoggedIn);

    bt.reset();
}

void Client::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    onSocketClose(errcode, errtype, std::string(preason, reason_len));
}
    
void Client::onSocketClose(int errcode, int /*errtype*/, const std::string& reason)
{
    PRESENCED_LOG_WARNING("Socket close (%d), reason: %s", errcode, reason.c_str());

    switch (mConnState)
    {
        case kDisconnected:
            return;

        case kConnNew:
        case kFetchingURL:
        case kResolvingDNS:
            PRESENCED_LOG_ERROR("Unexpected connection state on socket close: %s", connStateToStr(mConnState));
            assert(false);
            break;

        case kConnecting:
        case kConnected:
        case kLoggedIn:
            retry();
            break;
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
    bool ret = sendPrefs();
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

    assert(mConnState == kConnected || mConnState == kLoggedIn);

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
            PRESENCED_LOG_WARNING("Failed to send keepalive, reconnecting...");
            needReconnect = true;
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
        retry();
    }
}

void Client::disconnect()
{

    if (karereClient->onlineMode() && mConnState >= kDisconnected)
    {
        PRESENCED_LOG_DEBUG("Disconnecting...");

        resetConnection();
        bt.reset();
    }
    else
    {
        PRESENCED_LOG_DEBUG("disconnect() received, but still not connected");
    }
}

void Client::retryPendingConnection()
{
    if (karereClient->onlineMode() && mConnState >= kDisconnected)
    {
        PRESENCED_LOG_DEBUG("Retrying pending connections...");

        resetConnection();
        bt.reset();

        resolveDNS();
    }
    else
    {
        PRESENCED_LOG_DEBUG("retryPendingConnection() received, but still not connected");
    }
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
    if (mRetryTimerHandle)  // in case we are retrying getPresenceURL()
    {
        cancelTimeout(mRetryTimerHandle, karereClient->appCtx);
    }

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
                assert(mConnState == kConnected || mConnState == kLoggedIn);

                if (mConnState < kLoggedIn)
                    notifyLoggedIn();   // at connection-state level

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
        CALL_LISTENER(onPresenceChange, karereClient->myHandle(), Presence::kInvalid);
    }
}

void Client::getPresenceURL()
{
    if (!mUrl.isValid())
    {
        PRESENCED_LOG_INFO("Requesting URL...");

        assert(mConnState == kConnNew || mConnState == kFetchingURL);
        setConnState(kFetchingURL);

        auto wptr = getDelTracker();
        mApi->call(&::mega::MegaApi::getChatPresenceURL)
        .then([wptr, this](ReqResult result)
        {
            if (wptr.deleted())
            {
                PRESENCED_LOG_DEBUG("Presenced URL request completed, but client was deleted");
                return;
            }

            assert(result->getLink());
            mUrl.parse(result->getLink());
            if (!mUrl.isValid())
            {
                PRESENCED_LOG_ERROR("Received URL is invalid: %s", result->getLink());
                retryGetPresenceURL();
            }
            else
            {
                bt.reset();

                if (!karereClient->onlineMode())    // if we've been disconnected, do nothing
                {
                    PRESENCED_LOG_ERROR("Received presenced URL, but we've been disconnected");
                    assert(mConnState == kFetchingURL);
                    setConnState(kDisconnected);
                    return;
                }

                PRESENCED_LOG_INFO("Received presenced URL: %s", result->getLink());
                resolveDNS();
            }
        })
        .fail([wptr, this](const promise::Error& err)
        {
            if (wptr.deleted())
            {
                PRESENCED_LOG_DEBUG("URL request failed, but client was deleted. Error: %s", err.what());
                return;
            }

            PRESENCED_LOG_ERROR("Error connecting to server, cannot get URL: %s", err.what());
            retryGetPresenceURL();
        });
    }
    else    // we have a URL from previous call (URL is not persisted)
    {
        assert(mConnState == kDisconnected);
        setConnState(kFetchingURL);

        PRESENCED_LOG_INFO("Reusing presenced URL");
        resolveDNS();
    }
}

void Client::retryGetPresenceURL()
{
    bt.backoff();
    PRESENCED_LOG_INFO("Retrying in %.1f seconds", (float) bt.backoffdelta() / 10);

    assert(!mRetryTimerHandle);
    auto wptr = getDelTracker();
    mRetryTimerHandle = karere::setTimeout([wptr, this]()
    {
        if (wptr.deleted())
        {
            PRESENCED_LOG_DEBUG("Retry aborted, client was deleted");
            return;
        }

        mRetryTimerHandle = 0;
        getPresenceURL();
    }, bt.backoffdelta() * 10, karereClient->appCtx);
}

void Client::resolveDNS()
{
    PRESENCED_LOG_INFO("Resolving DNS: %s", mUrl.host.c_str());

    assert(mConnState == kFetchingURL || mConnState == kDisconnected);
    setConnState(kResolvingDNS);

    auto wptr = weakHandle();
    mResolveDnsPromise = mApi->call(&::mega::MegaApi::queryDNS, mUrl.host.c_str());
    mResolveDnsPromise
    .then([wptr, this](ReqResult result)
    {
        if (wptr.deleted())
        {
            PRESENCED_LOG_DEBUG("DNS resolution completed, but presenced client was deleted");
            return;
        }

        if (!karereClient->onlineMode()) // if we've been disconnected, do nothing
        {
            PRESENCED_LOG_DEBUG("DNS resolution completed, but we've been disconnected");
            return;
        }

        PRESENCED_LOG_INFO("DNS resolution completed: %s --> %s", mUrl.host.c_str(), result->getText());
        mIp = result->getText();
        doConnect();
    })
    .fail([wptr, this](const promise::Error& err)
    {
        if (wptr.deleted())
        {
            PRESENCED_LOG_DEBUG("DNS resolution failed, but presenced client was deleted. Error: %s", err.what());
            return;
        }

        if (!karereClient->onlineMode())
        {
            PRESENCED_LOG_DEBUG("DNS resolution failed, but we've been disconnected");
            return;
        }

        PRESENCED_LOG_ERROR("Error connecting to server, cannot resolve DNS: %s", err.what());
        retry();
    });
}

void Client::resetConnection()
{
    ConnState oldState = mConnState;
    setConnState(kDisconnected);
    mHeartbeatEnabled = false;
    mIp.clear();

    if (oldState == kResolvingDNS && !mResolveDnsPromise.done())
    {
        mResolveDnsPromise.reject("Request to resolve DNS is obsolete. Result will be ignored");
    }

    if (oldState >= kConnecting && wsIsConnected())
    {
        wsDisconnect(true);
    }

    // immediately cancel any ongoing retry (in example, retrying the login)
    if (mRetryTimerHandle)
    {
        cancelTimeout(mRetryTimerHandle, karereClient->appCtx);
        mRetryTimerHandle = 0;
    }
}

void Client::retry()
{
    resetConnection();

    bt.backoff();
    PRESENCED_LOG_INFO("Retrying in %.1f seconds", (float) bt.backoffdelta() / 10);

    assert(!mRetryTimerHandle);
    auto wptr = getDelTracker();
    mRetryTimerHandle = karere::setTimeout([wptr, this]()
    {
        if (wptr.deleted())
        {
            PRESENCED_LOG_DEBUG("Retry aborted, client was deleted");
            return;
        }

        mRetryTimerHandle = 0;
        resolveDNS();
    }, bt.backoffdelta() * 10, karereClient->appCtx);
}

void Client::doConnect()
{
    PRESENCED_LOG_INFO("Connecting to presenced using the IP: %s", mIp.c_str());

    assert(mConnState == kResolvingDNS);
    setConnState(kConnecting);

    bool rt = wsConnect(karereClient->websocketIO, mIp.c_str(),
              mUrl.host.c_str(),
              mUrl.port,
              mUrl.path.c_str(),
              mUrl.isSecure);
    if (!rt)
    {
        PRESENCED_LOG_ERROR("Immediate error on wsConnect");

#ifndef USE_LIBWEBSOCKETS
        retry();
#endif
        // libwebsockets calls wsCloseCb()-->onSocketClose() on immediate error
    }

    // if connection is successful, wsConnectCb() is called
}

void Client::login()
{
    PRESENCED_LOG_INFO("Logging into presenced...");

    sendCommand(Command(OP_HELLO) + (uint8_t)kProtoVersion+mCapabilities);

    if (mPrefsAckWait)
    {
        sendPrefs();
    }

    sendUserActive((time(NULL) - mTsLastUserActivity) < mConfig.mAutoawayTimeout, true);
    pushPeers();

    // if login is successful, wsHandleMsgCb() --> notifyLoggedIn() is called
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
