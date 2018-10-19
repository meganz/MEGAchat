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
: mListener(&listener), karereClient(client), mApi(api), mCapabilities(caps), usingipv6(false),
  mDNScache(karereClient->websocketIO->mDnsCache)
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
    std::vector<karere::Id> peers;

    for (auto& peer: mCurrentPeers)
    {
        peers.push_back(peer.first);
    }

    updatePeers(peers, true);
}

void Client::wsConnectCb()
{
    PRESENCED_LOG_DEBUG("Presenced connected to %s", mTargetIp.c_str());
    mDNScache.connectDone(mUrl.host, mTargetIp);
    setConnState(kConnected);
    assert(!mConnectPromise.done());
    mConnectPromise.resolve();
    mRetryCtrl.reset();
}

void Client::wsCloseCb(int errcode, int errtype, const char *preason, size_t /*reason_len*/)
{
    onSocketClose(errcode, errtype, preason);
}
    
void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    PRESENCED_LOG_WARNING("Socket close on IP %s. Reason: %s", mTargetIp.c_str(), reason.c_str());
    
    mHeartbeatEnabled = false;
    auto oldState = mConnState;
    setConnState(kDisconnected);

    if (oldState == kDisconnected)
        return;

    usingipv6 = !usingipv6;
    mTargetIp.clear();

    if (oldState < kLoggedIn) //tell retry controller that the connect attempt failed
    {
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
    }
    else
    {
        PRESENCED_LOG_DEBUG("Socket close at state kLoggedIn");
        reconnect(); //start retry controller
    }
}

std::string Config::toString() const
{
    std::string result;
    result.reserve(64);
    result.append("pres: ").append(mPresence.toString())
          .append(", persist: ").append(mPersist ? "1" : "0")
          .append(", aaActive: ").append(mAutoawayActive ? "1" : "0")
          .append(", aaTimeout: ").append(std::to_string(mAutoawayTimeout))
          .append(", hideLastGreen: ").append(mLastGreenVisible ? "0" : "1");
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

bool Client::setLastGreenVisible(bool enable)
{
    if (enable == mConfig.mLastGreenVisible)
        return true;
    mConfig.mLastGreenVisible = enable;
    signalActivity(true);
    return sendPrefs();
}

bool Client::requestLastGreen(Id userid)
{
    return sendCommand(Command(OP_LASTGREEN) + userid);
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

void Client::abortRetryController()
{
    if (!mRetryCtrl)
    {
        return;
    }

    assert(!isOnline());

    PRESENCED_LOG_DEBUG("Reconnection was aborted");
    mRetryCtrl->abort();
    mRetryCtrl.reset();
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

        // if there were an existing retry in-progress, abort it first or it will kick in after its backoff
        abortRetryController();

        // create a new retry controller and return its promise for reconnection
        auto wptr = weakHandle();
        mRetryCtrl.reset(createRetryController("presenced", [this](int /*no*/, DeleteTrackable::Handle wptr) -> Promise<void>
        {
            if (wptr.deleted())
            {
                PRESENCED_LOG_DEBUG("Reconnect attempt initiated, but presenced client was deleted.");
                return promise::_Void();
            }

            disconnect();
            mConnectPromise = Promise<void>();

            string ipv4, ipv6;
            bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);

            setConnState(kResolving);
            PRESENCED_LOG_DEBUG("Resolving hostname %s...", mUrl.host.c_str());

            int statusDNS = wsResolveDNS(karereClient->websocketIO, mUrl.host.c_str(),
                         [wptr, cachedIPs, this](int statusDNS, std::vector<std::string> &ipsv4, std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed, but presenced client was deleted.");
                    return;
                }

                if (statusDNS < 0 || (ipsv4.empty() && ipsv6.empty()))
                {
                    string errStr = (statusDNS < 0)
                            ? "Async DNS error in presenced. Error code: "+std::to_string(statusDNS)
                            : "Async DNS in presenced result on empty set of IPs";
                    PRESENCED_LOG_ERROR("%s", errStr.c_str());

                    if (isOnline() && cachedIPs)
                    {
                        PRESENCED_LOG_WARNING("DNS error, but connection is established. Relaying on cached IPs...");
                        return;
                    }

                    // if connection already started, first abort/cancel
                    if (wsIsConnected())
                    {
                        wsDisconnect(true);
                    }
                    onSocketClose(0, 0, "Async DNS error (presenced)");
                    return;
                }

                if (!cachedIPs) // connect required DNS lookup
                {
                    PRESENCED_LOG_DEBUG("Hostname resolved by first time. Connecting...");

                    mDNScache.set(mUrl.host,
                                  ipsv4.size() ? ipsv4.at(0) : "",
                                  ipsv6.size() ? ipsv6.at(0) : "");
                    doConnect();
                    return;
                }

                if (mDNScache.isMatch(mUrl.host, ipsv4, ipsv6))
                {
                    PRESENCED_LOG_DEBUG("DNS resolve matches cached IPs.");
                }
                else
                {
                    // update DNS cache
                    bool ret = mDNScache.set(mUrl.host,
                                             ipsv4.size() ? ipsv4.at(0) : "",
                                             ipsv6.size() ? ipsv6.at(0) : "");
                    assert(!ret);

                    PRESENCED_LOG_WARNING("DNS resolve doesn't match cached IPs. Forcing reconnect...");
                    // if connection already started, first abort/cancel
                    if (wsIsConnected())
                    {
                        wsDisconnect(true);
                    }
                    onSocketClose(0, 0, "DNS resolve doesn't match cached IPs (presenced)");
                }
            });

            // immediate error at wsResolveDNS()
            if (statusDNS < 0)
            {
                string errStr = "Immediate DNS error in presenced. Error code: "+std::to_string(statusDNS);
                PRESENCED_LOG_ERROR("%s", errStr.c_str());

                assert(mConnState == kResolving);
                assert(!mConnectPromise.done());

                // reject promise, so the RetryController starts a new attempt
                mConnectPromise.reject(errStr, statusDNS, kErrorTypeGeneric);
            }
            else if (cachedIPs) // if wsResolveDNS() failed immediately, very likely there's
            // no network connetion, so it's futile to attempt to connect
            {
                doConnect();
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

        }, wptr, karereClient->appCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

        return static_cast<Promise<void>&>(mRetryCtrl->start());
    }

    KR_EXCEPTION_TO_PROMISE(kPromiseErrtype_presenced);
}
    
bool Client::sendKeepalive(time_t now)
{
    mTsLastPingSent = now ? now : time(NULL);
    return sendCommand(Command(OP_KEEPALIVE));
}

void Client::updatePeers(const vector<Id> &peers, bool addOrRemove)
{
    assert(peers.size());
    Id scsn(mApi->sdk.getSequenceNumber());
    size_t numPeers = peers.size();
    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) * numPeers;

    Command cmd(addOrRemove ? OP_SNADDPEERS : OP_SNDELPEERS, totalSize);
    cmd.append<uint64_t>(scsn.val);
    cmd.append<uint32_t>(numPeers);
    for (unsigned int i = 0; i < numPeers; i++)
    {
        cmd.append<uint64_t>(peers[i].val);
    }

    sendCommand(std::move(cmd));
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

void Client::doConnect()
{
    string ipv4, ipv6;
    bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);
    assert(cachedIPs);
    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;

    setConnState(kConnecting);
    PRESENCED_LOG_DEBUG("Connecting to presenced using the IP: %s", mTargetIp.c_str());

    bool rt = wsConnect(karereClient->websocketIO, mTargetIp.c_str(),
          mUrl.host.c_str(),
          mUrl.port,
          mUrl.path.c_str(),
          mUrl.isSecure);

    if (!rt)    // immediate failure --> try the other IP family (if available)
    {
        PRESENCED_LOG_DEBUG("Connection to presenced failed using the IP: %s", mTargetIp.c_str());

        string oldTargetIp = mTargetIp;
        mTargetIp.clear();
        if (oldTargetIp == ipv6 && ipv4.size())
        {
            mTargetIp = ipv4;
        }
        else if (oldTargetIp == ipv4 && ipv6.size())
        {
            mTargetIp = ipv6;
        }

        if (mTargetIp.size())
        {
            PRESENCED_LOG_DEBUG("Retrying using the IP: %s", mTargetIp.c_str());
            if (wsConnect(karereClient->websocketIO, mTargetIp.c_str(),
                          mUrl.host.c_str(),
                          mUrl.port,
                          mUrl.path.c_str(),
                          mUrl.isSecure))
            {
                return;
            }
            PRESENCED_LOG_DEBUG("Connection to presenced failed using the IP: %s", mTargetIp.c_str());
        }

        onSocketClose(0, 0, "Websocket error on wsConnect (presenced)");
    }
}

void Client::retryPendingConnection(bool disconnect)
{
    if (mUrl.isValid())
    {
        if (mRetryCtrl && mRetryCtrl->state() == rh::State::kStateRetryWait)
        {
            PRESENCED_LOG_WARNING("Abort backoff and reconnect immediately");
            mRetryCtrl->restart();
        }
        else if (disconnect)
        {
            setConnState(kDisconnected);
            mHeartbeatEnabled = false;
            abortRetryController();
            PRESENCED_LOG_WARNING("Retry pending connection...");
            reconnect();
        }
    }
    else
    {
        PRESENCED_LOG_WARNING("No valid URL provided to retry pending connections");
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
            snprintf(buf, bufsize, "HELLO - version 0x%02X, caps: (%s,%s,%s)",
                read<uint8_t>(1),
                (caps & karere::kClientCanWebrtc) ? "webrtc" : "nowebrtc",
                (caps & karere::kClientIsMobile) ? "mobile" : "desktop",
                (caps & karere::kClientSupportLastGreen ? "last-green" : "no-last-green"));
            break;
        }
        case OP_SNADDPEERS:
        {
            Id sn = read<uint64_t>(1);
            uint32_t numPeers = read<uint32_t>(9);
            string tmpString;
            tmpString.append("SNADDPEERS - ");
            tmpString.append(ID_CSTR(sn));
            tmpString.append(" - NumPeers: ");
            tmpString.append(to_string(numPeers));
            tmpString.append(" peer/s: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(5+i*8);
                tmpString.append(ID_CSTR(peerId));
                if (i + 1 < numPeers)
                    tmpString.append(", ");
            }
            snprintf(buf, bufsize, "%s", tmpString.c_str());
            break;
        }
        case OP_SNDELPEERS:
        {
            Id sn = read<uint64_t>(1);
            uint32_t numPeers = read<uint32_t>(9);
            string tmpString;
            tmpString.append("SNDELPEERS - ");
            tmpString.append(ID_CSTR(sn));
            tmpString.append(" - NumPeers: ");
            tmpString.append(to_string(numPeers));
            tmpString.append(" peer/s: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(5+i*8);
                tmpString.append(ID_CSTR(peerId));
                if (i + 1 < numPeers)
                    tmpString.append(", ");
            }
            snprintf(buf, bufsize, "%s", tmpString.c_str());
            break;
        }
        case OP_LASTGREEN:
        {
            Id user = read<uint64_t>(1);
            string tmpString;
            tmpString.append("LASTGREEN - ");
            tmpString.append(ID_CSTR(user));
            if (size() > 9)
            {
                uint16_t lastGreen = read<uint16_t>(9);
                tmpString.append(" Last green: ");
                tmpString.append(to_string(lastGreen));
            }
            snprintf(buf, bufsize, "%s", tmpString.c_str());
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
    sendCommand(Command(OP_HELLO) + (uint8_t)kProtoVersion + mCapabilities);

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
    mAutoawayTimeout = (code & ~Config::kLastGreenVisibleMask) >> 4;
    if (mAutoawayTimeout > 600) // if longer than 10 minutes, use 10m + number of minutes (in seconds)
    {
        mAutoawayTimeout = 600 + (mAutoawayTimeout - 600) * 60;
    }
    mLastGreenVisible = (code & ~Config::kLastGreenVisibleMask);
}

uint16_t Config::toCode() const
{
    uint32_t autoawayTimeout = mAutoawayTimeout;
    if (autoawayTimeout > 600)  // if longer than 10 minutes, convert into 10m (in seconds) + number of minutes
    {
        autoawayTimeout = 600 + (mAutoawayTimeout - 600) / 60;
    }

    return ((mPresence.code() - karere::Presence::kOffline) & 3)
          | (mPersist ? 4 : 0)
          | (mAutoawayActive ? 0 : 8)
          | (autoawayTimeout << 4)
          | (mLastGreenVisible ? 0 : Config::kLastGreenVisibleMask);
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
                bool loginCompleted = false;
                if (mConnState < kLoggedIn)
                {
                    loginCompleted = true;
                    setConnState(kLoggedIn);
                }

                READ_16(prefs, 0);
                if (mPrefsAckWait && prefs == mConfig.toCode()) //ack
                {
                    PRESENCED_LOG_DEBUG("recv PREFS - server ack to the prefs we sent (%s)", mConfig.toString().c_str());
                }
                else
                {
                    mConfig.fromCode(prefs);
                    if (mPrefsAckWait)
                    {
                        PRESENCED_LOG_DEBUG("recv other PREFS while waiting for our PREFS ack, cancelling our send.\nPrefs: %s",
                          mConfig.toString().c_str());
                    }
                    else if (loginCompleted)
                    {
                        PRESENCED_LOG_DEBUG("recv PREFS from server (initial config): %s", mConfig.toString().c_str());
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
            case OP_LASTGREEN:
            {
                READ_ID(userid, 0);
                READ_16(lastGreen, 8);
                PRESENCED_LOG_DEBUG("recv LASTGREEN - user '%s' last green %d", ID_CSTR(userid), lastGreen);
                CALL_LISTENER(onPresenceLastGreenUpdated, userid, lastGreen);
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
        std::vector<karere::Id> peers;
        peers.push_back(peer);
        updatePeers(peers, true);
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
    std::vector<karere::Id> peers;
    peers.push_back(peer);
    updatePeers(peers, false);
}
}
