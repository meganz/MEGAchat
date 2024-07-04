#include "presenced.h"
#include "chatClient.h"

using namespace std;
using namespace promise;
using namespace karere;

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
    : mApi(api),
      mKarereClient(client),
      mDnsCache(client->mDnsCache),
      mListener(&listener),
      mCapabilities(caps),
      mTsConnSuceeded(time(nullptr))
{
    mApi->sdk.addGlobalListener(this);
}

Promise<void> Client::fetchUrl()
{
    if (mKarereClient->anonymousMode() || mDnsCache.isValidUrl(kPresencedShard))
    {
       return promise::_Void();
    }

    setConnState(kFetchingUrl);
    auto wptr = getDelTracker();
    return mKarereClient->api.call(&::mega::MegaApi::getChatPresenceURL)
        .then(
            [this, wptr, lname = std::string{getLoggingName()}](ReqResult result) -> Promise<void>
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG(
                        "%sPresenced URL request completed, but presenced client was deleted",
                        lname.c_str());
                    return ::promise::_Void();
                }

                if (!result->getLink())
                {
                    PRESENCED_LOG_DEBUG("%sNo Presenced URL received from API", lname.c_str());
                    return ::promise::_Void();
                }

                // Update presenced url in ram and db
                const char* url = result->getLink();
                if (!url || !url[0])
                {
                    return promise::_Void();
                }

                // Add new record to DNS cache
                mDnsCache.addRecord(kPresencedShard, url);
                return promise::_Void();
            });
}

void Client::connect()
{
    assert (mConnState == kConnNew);
    fetchUrl().then(
        [this, lname = std::string{getLoggingName()}]
        {
            reconnect().fail(
                [lname = std::move(lname)](const ::promise::Error& err)
                {
                    PRESENCED_LOG_DEBUG(
                        "%sPresenced::connect(): Error connecting to server after getting URL: %s",
                        lname.c_str(),
                        err.what());
                });
        });
}

void Client::pushPeers()
{
    if (!mLastScsn.isValid())
    {
        PRESENCED_LOG_WARNING("%spushPeers: still not catch-up with API", getLoggingName());
        return;
    }

    size_t numPeers = mContacts.size();
    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) * numPeers;

    Command cmd(OP_SNSETPEERS, static_cast <uint8_t>(totalSize));
    cmd.append<uint64_t>(mLastScsn.val);
    cmd.append<uint32_t>(static_cast<uint32_t>(numPeers));
    for (auto it = mContacts.begin(); it != mContacts.end(); it++)
    {
        cmd.append<uint64_t>(it->first);
    }

    sendCommand(std::move(cmd));
}

void Client::wsConnectCb()
{
    if (mConnState != kConnecting)
    {
        PRESENCED_LOG_WARNING("%sConnection to Presenced has been established, but current "
                              "connection state is %s, instead of connecting (as we expected)",
                              getLoggingName(),
                              connStateToStr(mConnState));
        return;
    }

    time_t now = time(nullptr);
    if (now - mTsConnSuceeded > kMaxConnSucceededTimeframe)
    {
        // reset if last check happened more than kMaxConnSucceededTimeframe seconds ago
        resetConnSuceededAttempts(now);
    }
    else
    {
        if (++mConnSuceeded > kMaxConnSuceeded)
        {
            // We need to refresh URL because we have reached max successful attempts, in kMaxConnSucceededTimeframe period
            PRESENCED_LOG_DEBUG("%sLimit of successful connection attempts (%u), was reached in a "
                                "period of %d seconds:",
                                getLoggingName(),
                                kMaxConnSuceeded,
                                kMaxConnSucceededTimeframe);
            resetConnSuceededAttempts(now);
            retryPendingConnection(true, true); // cancel all retries and fetch new URL
            return;
        }
    }
    setConnState(kConnected);
}

void Client::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    string reason;
    if (preason)
        reason.assign(preason, reason_len);

    if (mConnState == kFetchingUrl || mFetchingUrl)
    {
        PRESENCED_LOG_DEBUG("%swsCloseCb: previous fetch of a fresh URL is still in progress",
                            getLoggingName());
        onSocketClose(errcode, errtype, reason);
        return;
    }

    PRESENCED_LOG_DEBUG("%sFetching a fresh URL", getLoggingName());
    mFetchingUrl = true;
    auto wptr = getDelTracker();
    mApi->call(&::mega::MegaApi::getChatPresenceURL)
        .then(
            [wptr, this, lname = std::string{getLoggingName()}](ReqResult result)
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_ERROR(
                        "%sPresenced URL request completed, but presenced client was deleted",
                        lname.c_str());
                    return;
                }

                mFetchingUrl = false;
                const char* url = result->getLink();
                if (url && url[0] &&
                    (karere::Url(url)).host !=
                        mDnsCache.getUrl(kPresencedShard).host) // hosts do not match
                {
                    // reset mConnSuceeded, to avoid a further succeeded connection attempt, can
                    // trigger another URL re-fetch
                    resetConnSuceededAttempts(time(nullptr));

                    // Update DNSCache record with new URL
                    PRESENCED_LOG_DEBUG("%sUpdate URL in cache, and start a new retry attempt",
                                        lname.c_str());
                    mDnsCache.updateRecord(kPresencedShard, url, true);
                    retryPendingConnection(true);
                }
            });

    onSocketClose(errcode, errtype, reason);
}
    
void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    if (mKarereClient->isTerminated())
    {
        PRESENCED_LOG_WARNING("%sSocket close but karere client was terminated.", getLoggingName());
        return;
    }

    if (mConnState == kDisconnected)
    {
        PRESENCED_LOG_DEBUG("%sonSocketClose: we are already in kDisconnected state",
                            getLoggingName());
        if (!mRetryCtrl)
        {
            PRESENCED_LOG_ERROR("%sThere's no retry controller instance when calling onSocketClose "
                                "in kDisconnected state",
                                getLoggingName());
            mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99013, "There's no retry controller instance when calling onSocketClose in kDisconnected state", false, static_cast<const char*>(nullptr));
            reconnect(); //start retry controller
        }
        return;
    }

    PRESENCED_LOG_WARNING("%sSocket close on IP %s. Reason: %s",
                          getLoggingName(),
                          mTargetIp.c_str(),
                          reason.c_str());

    if (mConnState == kFetchingUrl)
    {
        PRESENCED_LOG_DEBUG("%sSocket close while fetching URL. Ignoring...", getLoggingName());
        // it should happen only when the cached URL becomes invalid (wsResolveDNS() returns UV_EAI_NONAME
        // and it will reconnect automatically once the URL is fetched again
        return;
    }

    auto oldState = mConnState;
    setConnState(kDisconnected);

    assert(oldState != kDisconnected);

    usingipv6 = !usingipv6;
    mTargetIp.clear();

    if (oldState >= kConnected)
    {
        PRESENCED_LOG_DEBUG("%sSocket close at state kLoggedIn", getLoggingName());

        assert(!mRetryCtrl);
        reconnect(); //start retry controller
    }
    else // oldState is kResolving or kConnecting
         // -> tell retry controller that the connect attempt failed
    {
        PRESENCED_LOG_DEBUG(
            "%sSocket close and state is not kStateConnected (but %s), start retry controller",
            getLoggingName(),
            connStateToStr(oldState));

        assert(mRetryCtrl);
        assert(!mConnectPromise.succeeded());
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
    }
}

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
bool Client::wsSSLsessionUpdateCb(const CachedSession &sess)
{
    // update the session's data in the DNS cache
    return mDnsCache.updateTlsSession(sess);
}
#endif

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

    PRESENCED_LOG_DEBUG("%ssetPresence(): %s -> %s",
                        getLoggingName(),
                        mConfig.mPresence.toString(),
                        pres.toString());

    mConfig.mPresence = pres;
    return sendPrefs();
}

bool Client::setPersist(bool enable)
{
    if (enable == mConfig.mPersist)
        return true;

    PRESENCED_LOG_DEBUG("%ssetPersist(): %d -> %d",
                        getLoggingName(),
                        (int)mConfig.mPersist,
                        (int)enable);

    mConfig.mPersist = enable;
    return sendPrefs();
}

bool Client::setLastGreenVisible(bool enable)
{
    if (enable == mConfig.mLastGreenVisible)
        return true;

    mConfig.mLastGreenVisible = enable;
    return sendPrefs();
}

bool Client::requestLastGreen(const Id& userid)
{
    // Avoid send OP_LASTGREEN if user is ex-contact or has never been a contact
    if (isExContact(userid) || !isContact(userid))
    {
        return false;
    }

    // Reset user last green or insert an entry in the map if not exists
    mPeersLastGreen[userid.val] = 0;

    return sendCommand(Command(OP_LASTGREEN) + userid);
}

time_t Client::getLastGreen(const Id& userid)
{
    std::map<uint64_t, time_t>::iterator it = mPeersLastGreen.find(userid.val);
    if (it != mPeersLastGreen.end())
    {
        return it->second;
    }
    return 0;
}

bool Client::updateLastGreen(const Id& userid, time_t lastGreen)
{
    time_t &auxLastGreen = mPeersLastGreen[userid.val];
    if (lastGreen >= auxLastGreen)
    {
        auxLastGreen = lastGreen;
        return true;
    }
    return false;
}

void Client::resetConnSuceededAttempts(const time_t &t)
{
    mTsConnSuceeded = t;
    mConnSuceeded = 0;
}

bool Client::setAutoaway(bool enable, time_t timeout)
{
    if (enable == mConfig.mAutoawayActive && timeout == mConfig.mAutoawayTimeout)
        return true;

    if (enable)
    {
        mConfig.mPersist = false;
        mConfig.mPresence = Presence::kOnline;
    }

    mConfig.mAutoawayTimeout = timeout;
    mConfig.mAutoawayActive = enable;
    return sendPrefs();
}

void Client::signalActivity()
{
    if (!mConfig.autoAwayInEffect())
    {
        if (!mConfig.mPresence.isValid())
        {
            PRESENCED_LOG_DEBUG("%ssignalActivity(): the current configuration is not yet "
                                "received, cannot be changed",
                                getLoggingName());
        }
        else if (!mConfig.mAutoawayActive)
        {
            PRESENCED_LOG_WARNING(
                "%ssignalActivity(): autoaway is disabled, no need to signal user's activity",
                getLoggingName());
        }
        else if (mConfig.mPresence != Presence::kOnline)
        {
            PRESENCED_LOG_WARNING(
                "%ssignalActivity(): configured status is not online, autoaway shouldn't be used",
                getLoggingName());
        }
        else if (mConfig.mPersist)
        {
            PRESENCED_LOG_WARNING("%ssignalActivity(): configured status is persistent, no need to "
                                  "signal user's activity",
                                  getLoggingName());
        }
        return;
    }
    else if (mKarereClient->isInBackground())
    {
        PRESENCED_LOG_WARNING(
            "%ssignalActivity(): app is in background, no need to signal user's activity",
            getLoggingName());
        return;
    }

    mTsLastUserActivity = time(NULL);
    sendUserActive(true);
}

void Client::signalInactivity()
{
    if (!mConfig.autoAwayInEffect())
    {
        if (!mConfig.mPresence.isValid())
        {
            PRESENCED_LOG_DEBUG(
                "%ssignalInactivity(): the current configuration is not yet received",
                getLoggingName());
        }
        else if (!mConfig.mAutoawayActive)
        {
            PRESENCED_LOG_WARNING(
                "%ssignalInactivity(): autoaway is disabled, no need to signal user's inactivity",
                getLoggingName());
        }
        else if (mConfig.mPresence != Presence::kOnline)
        {
            PRESENCED_LOG_WARNING("%ssignalInactivity(): configured status is not online, no need "
                                  "to signal user's inactivity",
                                  getLoggingName());
        }
        else if (mConfig.mPersist)
        {
            PRESENCED_LOG_WARNING("%ssignalInactivity(): configured status is persistent, no need "
                                  "to signal user's inactivity",
                                  getLoggingName());
        }
        return;
    }
    else if (!mKarereClient->isInBackground())
    {
        PRESENCED_LOG_WARNING(
            "%ssignalInactivity(): app is not in background, no need to signal user's inactivity",
            getLoggingName());
        return;
    }

    sendUserActive(false);
}

void Client::notifyUserStatus()
{
    if (mKarereClient->isInBackground())
    {
        signalInactivity();
    }
    else
    {
        signalActivity();
    }
}

bool Client::isSignalActivityRequired()
{
    return !mKarereClient->isInBackground() && mConfig.autoAwayInEffect();
}

void Client::abortRetryController()
{
    if (!mRetryCtrl)
    {
        return;
    }

    assert(!isOnline());

    PRESENCED_LOG_DEBUG("%sReconnection was aborted", getLoggingName());
    mRetryCtrl->abort();
    mRetryCtrl.reset();
}

Promise<void>
Client::reconnect()
{
    if (mKarereClient->isTerminated())
    {
        PRESENCED_LOG_WARNING("%sReconnect attempt initiated, but karere client was terminated.",
                              getLoggingName());
        assert(false);
        return ::promise::Error("Reconnect called when karere::Client is terminated", kErrorAccess, kErrorAccess);
    }

    assert(!mHeartbeatEnabled);
    assert(!mRetryCtrl);
    try
    {
        if (mConnState >= kResolving) //would be good to just log and return, but we have to return a promise
            return ::promise::Error(std::string("Already connecting/connected"));

        if (!mDnsCache.isValidUrl(kPresencedShard))
            return ::promise::Error("Current URL is not valid");

        setConnState(kResolving);

        // if there were an existing retry in-progress, abort it first or it will kick in after its backoff
        abortRetryController();

        // create a new retry controller and return its promise for reconnection
        auto wptr = weakHandle();
        mRetryCtrl.reset(createRetryController(
            "presenced",
            [this,
             lname = std::string{getLoggingName()}](size_t attemptId,
                                                    DeleteTrackable::Handle wptr) -> Promise<void>
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG(
                        "%sReconnect attempt initiated, but presenced client was deleted.",
                        lname.c_str());
                    return ::promise::_Void();
                }

                setConnState(kDisconnected);
                mConnectPromise = Promise<void>();

                const std::string& host = mDnsCache.getUrl(kPresencedShard).host;

                string ipv4, ipv6;
                bool cachedIPs = mDnsCache.getIp(kPresencedShard, ipv4, ipv6);

                setConnState(kResolving);
                PRESENCED_LOG_DEBUG("%sResolving hostname %s...", lname.c_str(), host.c_str());

                auto retryCtrl = mRetryCtrl.get();
                int statusDNS = wsResolveDNS(
                    mKarereClient->websocketIO,
                    host.c_str(),
                    [wptr, cachedIPs, this, retryCtrl, attemptId, lname](
                        int statusDNS,
                        const std::vector<std::string>& ipsv4,
                        const std::vector<std::string>& ipsv6)
                    {
                        if (wptr.deleted())
                        {
                            PRESENCED_LOG_DEBUG(
                                "%sDNS resolution completed, but presenced client was deleted.",
                                lname.c_str());
                            return;
                        }

                        if (mKarereClient->isTerminated())
                        {
                            PRESENCED_LOG_DEBUG(
                                "%sDNS resolution completed but karere client was terminated.",
                                lname.c_str());
                            return;
                        }

                        if (!mRetryCtrl)
                        {
                            if (isOnline())
                            {
                                PRESENCED_LOG_DEBUG(
                                    "%sDNS resolution completed but ignored: connection is "
                                    "already established using cached IP",
                                    lname.c_str());
                                assert(cachedIPs);
                            }
                            else
                            {
                                PRESENCED_LOG_DEBUG("%sDNS resolution completed but ignored: "
                                                    "connection was aborted",
                                                    lname.c_str());
                            }
                            return;
                        }
                        if (mRetryCtrl.get() != retryCtrl)
                        {
                            PRESENCED_LOG_DEBUG("%sDNS resolution completed but ignored: a newer "
                                                "retry has already started",
                                                lname.c_str());
                            return;
                        }
                        if (mRetryCtrl->currentAttemptId() != attemptId)
                        {
                            PRESENCED_LOG_DEBUG(
                                "%sDNS resolution completed but ignored: a newer attempt "
                                "is already started (old: %lu, new: %lu)",
                                lname.c_str(),
                                attemptId,
                                mRetryCtrl->currentAttemptId());
                            return;
                        }

                        if (statusDNS < 0 || (ipsv4.empty() && ipsv6.empty()))
                        {
                            if (isOnline() && cachedIPs)
                            {
                                assert(false); // this case should be handled already at: if
                                               // (!mRetryCtrl)
                                PRESENCED_LOG_WARNING("%sDNS error, but connection is established. "
                                                      "Relaying on cached IPs...",
                                                      lname.c_str());
                                return;
                            }

                            if (statusDNS < 0)
                            {
                                PRESENCED_LOG_ERROR(
                                    "%sAsync DNS error in presenced. Error code: %d",
                                    lname.c_str(),
                                    statusDNS);
                            }
                            else
                            {
                                PRESENCED_LOG_ERROR(
                                    "%sAsync DNS error in presenced. Empty set of IPs",
                                    lname.c_str());
                            }

                            assert(!isOnline());
                            if (statusDNS == wsGetNoNameErrorCode(mKarereClient->websocketIO))
                            {
                                retryPendingConnection(true, true);
                            }
                            else if (mConnState == kResolving)
                            {
                                onSocketClose(0, 0, "Async DNS error (presenced)");
                            }
                            // else in case kConnecting let the connection attempt progress
                            return;
                        }

                        if (!cachedIPs) // connect required DNS lookup
                        {
                            PRESENCED_LOG_DEBUG("%sHostname resolved by first time. Connecting...",
                                                lname.c_str());
                            mDnsCache.setIp(kPresencedShard, ipsv4, ipsv6);
                            doConnect();
                            return;
                        }

                        if (mDnsCache.isMatch(kPresencedShard, ipsv4, ipsv6))
                        {
                            PRESENCED_LOG_DEBUG("%sDNS resolve matches cached IPs.", lname.c_str());
                        }
                        else
                        {
                            PRESENCED_LOG_WARNING(
                                "%sDNS resolve doesn't match cached IPs. Forcing reconnect...",
                                lname.c_str());
                            mDnsCache.setIp(kPresencedShard, ipsv4, ipsv6);
                            retryPendingConnection(true);
                        }
                    });

                // immediate error at wsResolveDNS()
                if (statusDNS < 0)
                {
                    string errStr = "Immediate DNS error in presenced. Error code: " +
                                    std::to_string(statusDNS);
                    PRESENCED_LOG_ERROR("%s%s", getLoggingName(), errStr.c_str());

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

                return mConnectPromise.then(
                    [wptr, this]()
                    {
                        if (wptr.deleted())
                            return;

                        assert(isOnline());
                        mTsLastPingSent = 0;
                        mTsLastRecv = time(NULL);
                        mHeartbeatEnabled = true;
                        login();
                    });
            },
            wptr,
            mKarereClient->appCtx,
            nullptr // cancel function
            ,
            KARERE_RECONNECT_ATTEMPT_TIMEOUT // initial attempt timeout (increases exponentially)
            ,
            KARERE_RECONNECT_MAX_ATTEMPT_TIMEOUT // maximum attempt timeout
            ,
            0 // max number of attempts
            ,
            KARERE_RECONNECT_DELAY_MAX // max single wait between attempts
            ,
            0)); // initial single wait between attempts  (increases exponentially)

        return static_cast<Promise<void>&>(mRetryCtrl->start());
    }
    KR_EXCEPTION_TO_PROMISE(kPromiseErrtype_presenced);
}
    
bool Client::sendKeepalive(time_t now)
{
    mTsLastPingSent = now ? now : time(NULL);
    return sendCommand(Command(OP_KEEPALIVE));
}

bool Client::isExContact(uint64_t userid)
{
    auto it = mContacts.find(userid);
    if (it == mContacts.end() || (it != mContacts.end() && it->second != ::mega::MegaUser::VISIBILITY_HIDDEN))
    {
        return false;
    }

    return true;
}

bool Client::isContact(uint64_t userid)
{
    return (mContacts.find(userid) != mContacts.end());
}

void Client::onUsersUpdate(::mega::MegaApi *api, ::mega::MegaUserList *usersUpdated)
{
    if (!mLastScsn.isValid())
    {
        PRESENCED_LOG_DEBUG("%sonUsersUpdate: still catching-up with actionpackets",
                            getLoggingName());
        return;
    }

    const char *buf = api->getSequenceNumber();
    Id scsn(buf, strlen(buf));
    delete [] buf;

    if (!usersUpdated)
    {
        PRESENCED_LOG_DEBUG("%sUser list up to date. scsn: %s",
                            getLoggingName(),
                            scsn.toString().c_str());
        return;
    }

    std::shared_ptr<::mega::MegaUserList> users(usersUpdated->copy());
    auto wptr = weakHandle();
    marshallCall([wptr, this, users, scsn]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (!mLastScsn.isValid())
        {
            PRESENCED_LOG_DEBUG("%sonUsersUpdate (marshall): still catching-up with actionpackets",
                                getLoggingName());
            return;
        }

        mLastScsn = scsn;
        std::vector<karere::Id> addPeerList;
        std::vector<karere::Id> delPeerList;

        for (int i = 0; i < users->size(); i++)
        {
            ::mega::MegaUser *user = users->get(i);
            uint64_t userid = user->getHandle();
            int newVisibility = user->getVisibility();

            if (userid == mKarereClient->myHandle())
            {
                continue;
            }

            auto it = mContacts.find(userid);
            if (it == mContacts.end())
            {
                // new contact
                mContacts[userid] = newVisibility;
                if (newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                {
                    addPeerList.emplace_back(userid);
                }
            }
            else    // existing (ex)contact
            {
                // Update visibility
                int oldVisibility = it->second;
                it->second = newVisibility;

                if (newVisibility == ::mega::MegaUser::VISIBILITY_INACTIVE)
                {
                    // user cancelled the account
                    mContacts.erase(it);
                    if (oldVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                    {
                        // Send delPeer only if an active contact cancelled the account
                        delPeerList.emplace_back(userid);
                    }
                }
                else if (oldVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE && newVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN)
                {
                    // contact to ex-contact
                    delPeerList.emplace_back(userid);
                }
                else if (oldVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN && newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                {
                    // ex-contact to contact
                    addPeerList.emplace_back(userid);
                }
            }
        }

        // Send ADD/DELPEERS
        addPeers(addPeerList);
        removePeers(delPeerList);
    }, mKarereClient->appCtx);
}

void Client::onEvent(::mega::MegaApi *api, ::mega::MegaEvent *event)
{
    if (event->getType() == ::mega::MegaEvent::EVENT_NODES_CURRENT)
    {
        // Prepare list of peers to subscribe to its presence now, when catch-up phase is completed
        std::shared_ptr<::mega::MegaUserList> contacts(api->getContacts());
        std::shared_ptr<::mega::MegaTextChatList> chats(api->getChatList());
        const char *buf = api->getSequenceNumber();
        Id scsn(buf, strlen(buf));
        delete [] buf;

        // reset current status (for the full reload once logged in already)
        mLastScsn = karere::Id::inval();
        mContacts.clear();

        auto wptr = weakHandle();
        marshallCall([wptr, this, contacts, chats, scsn]()
        {
            if (wptr.deleted())
            {
                return;
            }

            assert(!mLastScsn.isValid());
            assert(mContacts.empty());

            mLastScsn = scsn;
            mContacts.clear();

            // initialize the list of contacts
            for (int i = 0; i < contacts->size(); i++)
            {
                ::mega::MegaUser *user = contacts->get(i);
                uint64_t userid = user->getHandle();
                if (userid == mKarereClient->myHandle())
                {
                    continue;
                }

                int visibility = user->getVisibility();
                mContacts[userid] = visibility; // add ex-contacts to identify them
            }

            // finally send to presenced the initial set of peers
            pushPeers();

        }, mKarereClient->appCtx);
    }
}

const char* Client::getLoggingName() const
{
    return mKarereClient ? mKarereClient->getLoggingName() : "";
}

void Client::heartbeat()
{
    // if a heartbeat is received but we are already offline...
    if (!mHeartbeatEnabled)
        return;

    auto now = time(NULL);
    if (mConfig.autoAwayInEffect()
            && mLastSentUserActive
            && (now - mTsLastUserActivity > mConfig.mAutoawayTimeout)
            && !mKarereClient->isCallInProgress())
    {
            sendUserActive(false);
    }

    bool needReconnect = false;
    if (now - mTsLastSend > kKeepaliveSendInterval)
    {
        if (!sendKeepalive(now))
        {
            PRESENCED_LOG_WARNING("%sFailed to send keepalive, reconnecting...", getLoggingName());
            needReconnect = true;
        }
    }
    else if (mTsLastPingSent)
    {
        if (now - mTsLastPingSent > kKeepaliveReplyTimeout)
        {
            PRESENCED_LOG_WARNING("%sTimed out waiting for KEEPALIVE response, reconnecting...",
                                  getLoggingName());
            needReconnect = true;
        }
    }
    else if (now - mTsLastRecv >= kKeepaliveSendInterval)
    {
        if (!sendKeepalive(now))
        {
            PRESENCED_LOG_WARNING("%sFailed to send keepalive, reconnecting...", getLoggingName());
            needReconnect = true;
        }
    }
    if (needReconnect)
    {
        setConnState(kDisconnected);
        abortRetryController();
        reconnect();
    }
}

void Client::disconnect()
{
    setConnState(kDisconnected);
}

void Client::doConnect()
{
    string ipv4, ipv6;
#ifndef NDEBUG
    bool cachedIPs =
#endif
    mDnsCache.getIp(kPresencedShard, ipv4, ipv6);
    assert(cachedIPs);
    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;

    const karere::Url &url = mDnsCache.getUrl(kPresencedShard);
    assert (url.isValid());

    setConnState(kConnecting);
    PRESENCED_LOG_DEBUG("%sConnecting to presenced using the IP: %s",
                        getLoggingName(),
                        mTargetIp.c_str());

    bool rt = wsConnect(mKarereClient->websocketIO, mTargetIp.c_str(),
          url.host.c_str(),
          url.port,
          url.path.c_str(),
          url.isSecure);

    if (!rt)    // immediate failure --> try the other IP family (if available)
    {
        PRESENCED_LOG_DEBUG("%sConnection to presenced failed using the IP: %s",
                            getLoggingName(),
                            mTargetIp.c_str());

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
            PRESENCED_LOG_DEBUG("%sRetrying using the IP: %s", getLoggingName(), mTargetIp.c_str());
            if (wsConnect(mKarereClient->websocketIO, mTargetIp.c_str(),
                          url.host.c_str(),
                          url.port,
                          url.path.c_str(),
                          url.isSecure))
            {
                return;
            }
            PRESENCED_LOG_DEBUG("%sConnection to presenced failed using the IP: %s",
                                getLoggingName(),
                                mTargetIp.c_str());
        }        
        else
        {
            // do not close the socket, which forces a new retry attempt and turns the DNS response obsolete
            // Instead, let the DNS request to complete, in order to refresh IPs
            PRESENCED_LOG_DEBUG("%sEmpty cached IP. Waiting for DNS resolution...",
                                getLoggingName());
            return;
        }

        onSocketClose(0, 0, "Websocket error on wsConnect (presenced)");
    }
}

void Client::retryPendingConnection(bool disconnect, bool refreshURL)
{
    if (mConnState == kConnNew)
    {
        PRESENCED_LOG_WARNING(
            "%sretryPendingConnection: no connection to be retried yet. Call connect() first",
            getLoggingName());
        return;
    }

    if (refreshURL || !mDnsCache.isValidUrl(kPresencedShard))
    {
        if (mConnState == kFetchingUrl || mFetchingUrl)
        {
            PRESENCED_LOG_WARNING(
                "%sretryPendingConnection: previous fetch of a fresh URL is still in progress",
                getLoggingName());
            return;
        }

        PRESENCED_LOG_WARNING("%sretryPendingConnection: fetch a fresh URL for reconnection!",
                              getLoggingName());

        // abort and prevent any further reconnection attempt
        setConnState(kDisconnected);
        abortRetryController();

        // Remove DnsCache record
        mDnsCache.removeRecord(kPresencedShard);

        auto wptr = getDelTracker();
        fetchUrl().then(
            [this, wptr, lname = std::string{getLoggingName()}]
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG(
                        "%sPresenced URL request completed, but presenced client was deleted",
                        lname.c_str());
                    return;
                }

                // reset mConnSuceeded, to avoid a further succeeded connection attempt, can trigger
                // another URL re-fetch
                resetConnSuceededAttempts(time(nullptr));
                retryPendingConnection(true);
            });
    }
    else if (disconnect)
    {
        PRESENCED_LOG_WARNING("%sretryPendingConnection: forced reconnection!", getLoggingName());

        setConnState(kDisconnected);
        abortRetryController();
        reconnect();
    }
    else if (mRetryCtrl && mRetryCtrl->state() == rh::State::kStateRetryWait)
    {
        PRESENCED_LOG_WARNING("%sretryPendingConnection: abort backoff and reconnect immediately",
                              getLoggingName());

        assert(!isOnline());
        assert(!mHeartbeatEnabled);

        mRetryCtrl->restart();
    }
    else
    {
        PRESENCED_LOG_WARNING("%sretryPendingConnection: ignored (currently joining/joined, no "
                              "forced disconnect was requested)",
                              getLoggingName());
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
        PRESENCED_LOG_DEBUG("%s  Can't send, we are offline", getLoggingName());
    return result;
}

bool Client::sendCommand(const Command& cmd)
{
    Buffer buf(cmd.buf(), cmd.dataSize());
    if (krLoggerWouldLog(krLogChannel_presenced, krLogLevelDebug))
        logSend(cmd);
    auto result = sendBuf(std::move(buf));
    if (!result)
        PRESENCED_LOG_DEBUG("%s  Can't send, we are offline", getLoggingName());
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
            snprintf(buf, bufsize, "USERACTIVE - %u", code);
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
            tmpString.append("SNADDPEERS - scsn: ");
            tmpString.append(ID_CSTR(sn));
            tmpString.append(" num_peers: ");
            tmpString.append(to_string(numPeers));
            tmpString.append((numPeers == 1) ? " peer: " :  " peers: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(13+i*8);
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
            tmpString.append("SNDELPEERS - scsn: ");
            tmpString.append(ID_CSTR(sn));
            tmpString.append(" num_peers: ");
            tmpString.append(to_string(numPeers));
            tmpString.append((numPeers == 1) ? " peer: " :  " peers: ");
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(13+i*8);
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
        case OP_SNSETPEERS:
        {
            Id sn = read<uint64_t>(1);
            uint32_t numPeers = read<uint32_t>(9);
            string tmpString;
            tmpString.append("SNSETPEERS - scsn: ");
            tmpString.append(ID_CSTR(sn));
            tmpString.append(" num_peers: ");
            tmpString.append(to_string(numPeers));
            if (numPeers)
            {
                tmpString.append((numPeers == 1) ? " peer: " :  " peers: ");
            }
            for (unsigned int i = 0; i < numPeers; i++)
            {
                Id peerId = read<uint64_t>(13+i*8);
                tmpString.append(ID_CSTR(peerId));
                if (i + 1 < numPeers)
                    tmpString.append(", ");
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
    // login to presenced indicating capabilities of the client
    sendCommand(Command(OP_HELLO) + (uint8_t)kProtoVersion + mCapabilities);

    // if reconnecting and the PREFS's changes are not acknowledge yet... retry
    if (mPrefsAckWait)
    {
        sendPrefs();
    }

    if (mLastScsn.isValid())
    {
        // send the list of peers allowed to see the own presence's status
        pushPeers();
    }
}

bool Client::sendUserActive(bool active, bool force)
{
    if ((active == mLastSentUserActive) && !force)
    {
        PRESENCED_LOG_DEBUG("%sTried to change user-active to the current state: %d",
                            getLoggingName(),
                            (int)mLastSentUserActive);
        return true;
    }

    bool sent = sendCommand(Command(OP_USERACTIVE) + (uint8_t)(active ? 1 : 0));
    if (!sent)
    {
        return false;
    }

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
    CALL_LISTENER(onPresenceChange, mKarereClient->myHandle(), mConfig.mPresence, mPrefsAckWait);
}

bool Config::autoAwayInEffect() const
{
    return mPresence.isValid()
            && mAutoawayActive
            && mAutoawayTimeout
            && mPresence == Presence::kOnline
            && !mPersist;
}

void Config::fromCode(uint16_t code)
{
    mPresence = static_cast<karere::Presence::Code>((code & 3) + karere::Presence::kOffline);
    mPersist = !!(code & 4);
    mAutoawayActive = !(code & 8);
    mAutoawayTimeout = (code & ~Config::kLastGreenVisibleMask) >> 4;
    if (mAutoawayTimeout > 600) // if longer than 10 minutes, use 10m + number of minutes (in seconds)
    {
        mAutoawayTimeout = 600 + (mAutoawayTimeout - 600) * 60;
    }
    mLastGreenVisible = !(code & Config::kLastGreenVisibleMask);
}

uint16_t Config::toCode() const
{
    auto autoawayTimeout = mAutoawayTimeout;
    if (autoawayTimeout > 600)  // if longer than 10 minutes, convert into 10m (in seconds) + number of minutes
    {
        autoawayTimeout = 600 + (mAutoawayTimeout - 600) / 60;
    }

    return static_cast<uint16_t>(((mPresence.code() - karere::Presence::kOffline) & 3)
          | (mPersist ? 4 : 0)
          | (mAutoawayActive ? 0 : 8)
          | (autoawayTimeout << 4)
          | (mLastGreenVisible ? 0 : Config::kLastGreenVisibleMask));
}

Client::~Client()
{
    mApi->sdk.removeGlobalListener(this);

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
                PRESENCED_LOG_DEBUG("%srecv KEEPALIVE", getLoggingName());
                break;
            }
            case OP_PEERSTATUS:
            {
                READ_8(pres, 0);
                READ_ID(userid, 1);
                PRESENCED_LOG_DEBUG("%srecv PEERSTATUS - user '%s' with presence %s",
                                    getLoggingName(),
                                    ID_CSTR(userid),
                                    Presence::toString(pres));
                updatePeerPresence(userid, pres);
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
                    PRESENCED_LOG_DEBUG("%srecv PREFS - server ack to the prefs we sent (%s)",
                                        getLoggingName(),
                                        mConfig.toString().c_str());
                }
                else
                {
                    mConfig.fromCode(prefs);
                    if (mPrefsAckWait)
                    {
                        PRESENCED_LOG_DEBUG("%srecv other PREFS while waiting for our PREFS ack, "
                                            "cancelling our send.\nPrefs: %s",
                                            getLoggingName(),
                                            mConfig.toString().c_str());
                    }
                    else if (loginCompleted)
                    {
                        PRESENCED_LOG_DEBUG("%srecv PREFS from server (initial config): %s",
                                            getLoggingName(),
                                            mConfig.toString().c_str());
                        if (mConfig.autoAwayInEffect())
                        {
                            // signal whether the user is active or inactive
                            bool isActive = !mKarereClient->isInBackground()    // active is not possible in background
                                    && (!mTsLastUserActivity                    // first connection, signal active if not in background
                                        || ((time(NULL) - mTsLastUserActivity) < mConfig.mAutoawayTimeout));    // check autoaway's timeout

                            if (isActive)
                            {
                                mTsLastUserActivity = time(NULL);
                            }
                            sendUserActive(isActive, true);
                        }
                    }
                    else
                    {
                        PRESENCED_LOG_DEBUG("%srecv PREFS from another client: %s",
                                            getLoggingName(),
                                            mConfig.toString().c_str());
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
                PRESENCED_LOG_DEBUG("%srecv LASTGREEN - user '%s' last green %u",
                                    getLoggingName(),
                                    ID_CSTR(userid),
                                    lastGreen);

                // convert the received minutes into a UNIX timestamp
                time_t lastGreenTs = time(NULL) - (lastGreen * 60);
                mPeersLastGreen[userid] = lastGreenTs;

                CALL_LISTENER(onPresenceLastGreenUpdated, userid);
                break;
            }
            default:
            {
                PRESENCED_LOG_ERROR("%sUnknown opcode %d, ignoring all subsequent commands",
                                    getLoggingName(),
                                    opcode);
                return;
            }
        }
      }
      catch(BufferRangeError& e)
      {
          PRESENCED_LOG_ERROR(
              "%sBuffer bound check error while parsing %s:\n\t%s\n\tAborting command processing",
              getLoggingName(),
              Command::opcodeToStr(opcode),
              e.what());
          return;
      }
      catch(std::exception& e)
      {
          PRESENCED_LOG_ERROR("%sException while processing incoming %s: %s",
                              getLoggingName(),
                              Command::opcodeToStr(opcode),
                              e.what());
          return;
      }
    }
}

void Client::setConnState(ConnState newState)
{
    if (newState == mConnState)
    {
        PRESENCED_LOG_DEBUG("%sTried to change connection state to the current state: %s",
                            getLoggingName(),
                            connStateToStr(newState));
        return;
    }
    else
    {
        PRESENCED_LOG_DEBUG("%sConnection state change: %s --> %s",
                            getLoggingName(),
                            connStateToStr(mConnState),
                            connStateToStr(newState));
        mConnState = newState;
    }

    CALL_LISTENER(onConnStateChange, mConnState);

    if (newState == kDisconnected)
    {
        mHeartbeatEnabled = false;

        // if a socket is opened, close it immediately
        if (wsIsConnected())
        {
            wsDisconnect(true);
        }

        // if connect-timer is running, it must be reset (kResolving --> kDisconnected)
        if (mConnectTimer)
        {
            cancelTimeout(mConnectTimer, mKarereClient->appCtx);
            mConnectTimer = 0;
        }

        if (!mKarereClient->isTerminated())
        {
            // start a timer to ensure the connection is established after kConnectTimeout. Otherwise, reconnect
            auto wptr = weakHandle();
            mConnectTimer = setTimeout([this, wptr]()
            {
                if (wptr.deleted())
                    return;

                mConnectTimer = 0;

                PRESENCED_LOG_DEBUG(
                    "%sReconnection attempt has not succeed after %d. Reconnecting...",
                    getLoggingName(),
                    kConnectTimeout);
                mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99005, "Reconnection timed out (presenced)", false, static_cast<const char*>(nullptr));

                retryPendingConnection(true);

            }, kConnectTimeout * 1000, mKarereClient->appCtx);
        }

        // if disconnected, we don't really know the presence status anymore
        for (auto it = mContacts.begin(); it != mContacts.end(); it++)
        {
            updatePeerPresence(it->first, Presence::kUnknown);
        }
        updatePeerPresence(mKarereClient->myHandle(), Presence::kUnknown);
    }
    else if (mConnState == kConnected)
    {
        PRESENCED_LOG_DEBUG("%sPresenced connected to %s", getLoggingName(), mTargetIp.c_str());

        mDnsCache.connectDone(kPresencedShard, mTargetIp);
        assert(!mConnectPromise.done());
        mConnectPromise.resolve();
        mRetryCtrl.reset();

        if (mConnectTimer)
        {
            cancelTimeout(mConnectTimer, mKarereClient->appCtx);
            mConnectTimer = 0;
        }
    }
}
void Client::addPeers(const std::vector<karere::Id> &peers)
{
    if (peers.empty())
        return;

    if (mKarereClient->anonymousMode())
    {
        PRESENCED_LOG_WARNING("%sNot sending ADDPEERS in anonymous mode", getLoggingName());
        return;
    }

    assert(mLastScsn.isValid());
    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + (peers.size() * sizeof(uint64_t));
    Command cmd(OP_SNADDPEERS, static_cast<uint8_t>(totalSize));
    cmd.append<uint64_t>(mLastScsn.val);
    cmd.append<uint32_t>(static_cast<uint32_t>(peers.size()));
    for (size_t i = 0; i < peers.size(); i++)
    {
#ifndef NDEBUG
        auto it = mContacts.find(peers.at(i));
        assert (it != mContacts.end() && it->second == ::mega::MegaUser::VISIBILITY_VISIBLE);
#endif
        cmd.append<uint64_t>(peers.at(i).val);
    }
    sendCommand(std::move(cmd));
}

void Client::removePeers(const std::vector<karere::Id> &peers)
{
    if (peers.empty())
        return;

    if (mKarereClient->anonymousMode())
    {
        PRESENCED_LOG_WARNING("%sNot sending DELPEERS in anonymous mode", getLoggingName());
        return;
    }

    assert(mLastScsn.isValid());
    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + (peers.size() * sizeof(uint64_t));
    Command cmd(OP_SNDELPEERS, static_cast<uint8_t>(totalSize));
    cmd.append<uint64_t>(mLastScsn.val);
    cmd.append<uint32_t>(static_cast<uint32_t>(peers.size()));
    for (size_t i = 0; i < peers.size(); i++)
    {
#ifndef NDEBUG
        auto it = mContacts.find(peers.at(i));
        assert (it == mContacts.end() || it->second == ::mega::MegaUser::VISIBILITY_HIDDEN);
#endif
        mPeersLastGreen.erase(peers.at(i).val); // Remove peer from mPeersLastGreen map if exists
        cmd.append<uint64_t>(peers.at(i).val);
        updatePeerPresence(peers.at(i), Presence::kUnknown);
    }
    sendCommand(std::move(cmd));
}

void Client::updatePeerPresence(const karere::Id& peer, karere::Presence pres)
{
    auto pair = mPeersPresence.emplace(peer, pres);
    if (!pair.second) // Element is already in the map (update value)
    {
        if (pair.first->second == pres)
        {
            return;
        }
        else
        {
            pair.first->second = pres;
        }
    }

    // Do not notify if the peer is ex-contact or has never been contact
    // (except updating to unknown when a contact becomes ex-contact)
    bool exContact = isExContact(peer);
    bool contact = isContact(peer);
    if (peer == mKarereClient->myHandle()
            || (contact && !exContact)
            || (exContact && pres.status() == Presence::kUnknown))
    {
        CALL_LISTENER(onPresenceChange, peer, pres);
    }
}

karere::Presence Client::peerPresence(const karere::Id& peer) const
{
    auto it = mPeersPresence.find(peer);
    if (it == mPeersPresence.end())
    {
        return karere::Presence::kUnknown;
    }
    return it->second;
}
}
