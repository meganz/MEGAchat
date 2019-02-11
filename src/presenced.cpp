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
: mApi(api), mKarereClient(client), mListener(&listener), mCapabilities(caps),
  mDNScache(mKarereClient->websocketIO->mDnsCache)
{
    mApi->sdk.addGlobalListener(this);
}

::promise::Promise<void>
Client::connect(const std::string& url, const Config& config)
{
    mConfig = config;
    mUrl.parse(url);

    if (mConnState == kConnNew)
    {
        return reconnect();
    }
    else    // connect() was already called, reconnection is automatic
    {
        PRESENCED_LOG_WARNING("connect() was already called, reconnection is automatic");
        return ::promise::Void();
    }
}

void Client::pushPeers()
{
    if (!mLastScsn.isValid())
    {
        PRESENCED_LOG_WARNING("pushPeers: still not catch-up with API");
        return;
    }

    size_t numPeers = mCurrentPeers.size();
    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) * numPeers;

    Command cmd(OP_SNSETPEERS, totalSize);
    cmd.append<uint64_t>(mLastScsn.val);
    cmd.append<uint32_t>(numPeers);
    for (auto it = mCurrentPeers.begin(); it != mCurrentPeers.end(); it++)
    {
        cmd.append<uint64_t>(it->first);
    }

    sendCommand(std::move(cmd));
}

void Client::wsConnectCb()
{
    setConnState(kConnected);
}

void Client::wsCloseCb(int errcode, int errtype, const char *preason, size_t /*reason_len*/)
{
    onSocketClose(errcode, errtype, preason);
}
    
void Client::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    if (mKarereClient->isTerminated())
    {
        PRESENCED_LOG_WARNING("Socket close but karere client was terminated.");
        return;
    }

    PRESENCED_LOG_WARNING("Socket close on IP %s. Reason: %s", mTargetIp.c_str(), reason.c_str());

    auto oldState = mConnState;
    setConnState(kDisconnected);

    assert(oldState != kDisconnected);

    usingipv6 = !usingipv6;
    mTargetIp.clear();

    if (oldState >= kConnected)
    {
        PRESENCED_LOG_DEBUG("Socket close at state kLoggedIn");

        assert(!mRetryCtrl);
        reconnect(); //start retry controller
    }
    else // (mConState < kConnected) --> tell retry controller that the connect attempt failed
    {
        PRESENCED_LOG_DEBUG("Socket close and state is not kStateConnected (but %s), start retry controller", connStateToStr(oldState));

        assert(mRetryCtrl);
        assert(!mConnectPromise.succeeded());
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
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

    PRESENCED_LOG_DEBUG("setPresence(): %s -> %s", mConfig.mPresence.toString(), pres.toString());

    mConfig.mPresence = pres;
    return sendPrefs();
}

bool Client::setPersist(bool enable)
{
    if (enable == mConfig.mPersist)
        return true;

    PRESENCED_LOG_DEBUG("setPersist(): %d -> %d", (int)mConfig.mPersist, (int)enable);

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

bool Client::requestLastGreen(Id userid)
{
    // Avoid send OP_LASTGREEN if user is ex-contact
    if (isExContact(userid))
    {
        return false;
    }

    // Reset user last green or insert an entry in the map if not exists
    mPeersLastGreen[userid.val] = 0;

    return sendCommand(Command(OP_LASTGREEN) + userid);
}

time_t Client::getLastGreen(Id userid)
{
    std::map<uint64_t, time_t>::iterator it = mPeersLastGreen.find(userid.val);
    if (it != mPeersLastGreen.end())
    {
        return it->second;
    }
    return 0;
}

bool Client::updateLastGreen(Id userid, time_t lastGreen)
{
    time_t &auxLastGreen = mPeersLastGreen[userid.val];
    if (lastGreen >= auxLastGreen)
    {
        auxLastGreen = lastGreen;
        return true;
    }
    return false;
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

bool Client::autoAwayInEffect()
{
    return mConfig.mPresence.isValid() && mConfig.mAutoawayActive && mConfig.mPresence == Presence::kOnline;
}

void Client::signalActivity()
{
    if (!autoAwayInEffect())
    {
        if (!mConfig.mPresence.isValid())
        {
            PRESENCED_LOG_DEBUG("signalActivity(): the current configuration is not yet received, cannot be changed");
        }
        else if (!mConfig.mAutoawayActive)
        {
            PRESENCED_LOG_WARNING("signalActivity(): autoaway is disabled, no need to signal user's activity");
        }
        else if (mConfig.mPresence != Presence::kOnline)
        {
            PRESENCED_LOG_WARNING("signalActivity(): configured status is not online, autoaway shouldn't be used");
        }
        return;
    }

    mTsLastUserActivity = time(NULL);
    sendUserActive(true);
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
Client::reconnect()
{
    if (mKarereClient->isTerminated())
    {
        PRESENCED_LOG_WARNING("Reconnect attempt initiated, but karere client was terminated.");
        assert(false);
        return ::promise::Error("Reconnect called when karere::Client is terminated", kErrorAccess, kErrorAccess);
    }

    assert(!mHeartbeatEnabled);
    assert(!mRetryCtrl);
    try
    {
        if (mConnState >= kResolving) //would be good to just log and return, but we have to return a promise
            return ::promise::Error(std::string("Already connecting/connected"));

        if (!mUrl.isValid())
            return ::promise::Error("Current URL is not valid");

        setConnState(kResolving);

        // if there were an existing retry in-progress, abort it first or it will kick in after its backoff
        abortRetryController();

        // create a new retry controller and return its promise for reconnection
        auto wptr = weakHandle();
        mRetryCtrl.reset(createRetryController("presenced", [this](size_t attemptNo, DeleteTrackable::Handle wptr) -> Promise<void>
        {
            if (wptr.deleted())
            {
                PRESENCED_LOG_DEBUG("Reconnect attempt initiated, but presenced client was deleted.");
                return ::promise::_Void();
            }

            setConnState(kDisconnected);
            mConnectPromise = Promise<void>();

            string ipv4, ipv6;
            bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);

            setConnState(kResolving);
            PRESENCED_LOG_DEBUG("Resolving hostname %s...", mUrl.host.c_str());

            auto retryCtrl = mRetryCtrl.get();
            int statusDNS = wsResolveDNS(mKarereClient->websocketIO, mUrl.host.c_str(),
                         [wptr, cachedIPs, this, retryCtrl, attemptNo](int statusDNS, std::vector<std::string> &ipsv4, std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed, but presenced client was deleted.");
                    return;
                }

                if (mKarereClient->isTerminated())
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed but karere client was terminated.");
                    return;
                }

                if (!mRetryCtrl)
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed but ignored: connection is already established using cached IP");
                    assert(isOnline());
                    assert(cachedIPs);
                    return;
                }
                if (mRetryCtrl.get() != retryCtrl)
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed but ignored: a newer retry has already started");
                    return;
                }
                if (mRetryCtrl->currentAttemptNo() != attemptNo)
                {
                    PRESENCED_LOG_DEBUG("DNS resolution completed but ignored: a newer attempt is already started (old: %d, new: %d)",
                                     attemptNo, mRetryCtrl->currentAttemptNo());
                    return;
                }

                if (statusDNS < 0 || (ipsv4.empty() && ipsv6.empty()))
                {
                    if (isOnline() && cachedIPs)
                    {
                        assert(false);  // this case should be handled already at: if (!mRetryCtrl)
                        PRESENCED_LOG_WARNING("DNS error, but connection is established. Relaying on cached IPs...");
                        return;
                    }

                    if (statusDNS < 0)
                    {
                        PRESENCED_LOG_ERROR("Async DNS error in presenced. Error code: %d", statusDNS);
                    }
                    else
                    {
                        PRESENCED_LOG_ERROR("Async DNS error in presenced. Empty set of IPs");
                    }

                    assert(!isOnline());
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

                assert(isOnline());
                mTsLastPingSent = 0;
                mTsLastRecv = time(NULL);
                mHeartbeatEnabled = true;
                login();
            });

        }, wptr, mKarereClient->appCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

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

void Client::onChatsUpdate(::mega::MegaApi *api, ::mega::MegaTextChatList *roomsUpdated)
{
    const char *buf = api->getSequenceNumber();
    Id scsn(buf, strlen(buf));
    delete [] buf;

    if (!roomsUpdated)
    {
        PRESENCED_LOG_DEBUG("Chatroom list up to date. scsn: %s", scsn.toString().c_str());
        return;
    }

    std::shared_ptr<::mega::MegaTextChatList> rooms(roomsUpdated->copy());
    auto wptr = weakHandle();
    marshallCall([wptr, this, rooms, scsn]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (!mLastScsn.isValid())
        {
            PRESENCED_LOG_DEBUG("onChatsUpdate: still catching-up with actionpackets");
            return;
        }

        mLastScsn = scsn;

        for (int i = 0; i < rooms->size(); i++)
        {
            const ::mega::MegaTextChat *room = rooms->get(i);
            uint64_t chatid = room->getHandle();
            const ::mega::MegaTextChatPeerList *peerList = room->getPeerList();

            auto it = mChatMembers.find(chatid);
            if (it == mChatMembers.end())  // new room
            {
                if (!peerList)
                {
                    continue;   // no peers in this chatroom
                }

                for (int j = 0; j < peerList->size(); j++)
                {
                    uint64_t userid = peerList->getPeerHandle(j);
                    mChatMembers[chatid].insert(userid);
                    addPeer(userid);
                }
            }
            else    // existing room
            {
                SetOfIds oldPeerList = mChatMembers[chatid];
                SetOfIds newPeerList;
                if (peerList)
                {
                    for (int j = 0; j < peerList->size(); j++)
                    {
                        newPeerList.insert(peerList->getPeerHandle(j));
                    }
                }

                // check for removals
                for (auto oldIt = oldPeerList.begin(); oldIt != oldPeerList.end(); oldIt++)
                {
                    uint64_t userid = *oldIt;
                    if (!newPeerList.has(userid))
                    {
                        mChatMembers[chatid].erase(userid);
                        removePeer(userid);
                    }
                }

                // check for additions
                for (auto newIt = newPeerList.begin(); newIt != newPeerList.end(); newIt++)
                {
                    uint64_t userid = *newIt;
                    if (!oldPeerList.has(userid))
                    {
                        mChatMembers[chatid].insert(userid);
                        addPeer(userid);
                    }
                }
            }
        }

    }, mKarereClient->appCtx);
}

void Client::onUsersUpdate(::mega::MegaApi *api, ::mega::MegaUserList *usersUpdated)
{
    const char *buf = api->getSequenceNumber();
    Id scsn(buf, strlen(buf));
    delete [] buf;

    if (!usersUpdated)
    {
        PRESENCED_LOG_DEBUG("User list up to date. scsn: %s", scsn.toString().c_str());
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
            PRESENCED_LOG_DEBUG("onUsersUpdate: still catching-up with actionpackets");
            return;
        }

        mLastScsn = scsn;

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
            if (it == mContacts.end())  // new contact
            {
                assert(newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE);
                mContacts[userid] = newVisibility;
                addPeer(userid);
            }
            else    // existing (ex)contact
            {
                int oldVisibility = it->second;
                it->second = newVisibility;

                if (newVisibility == ::mega::MegaUser::VISIBILITY_INACTIVE) // user cancelled the account
                {
                    mContacts.erase(it);
                    removePeer(userid, true);
                }
                else if (oldVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE && newVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN)
                {
                    removePeer(userid, true);
                }
                else if (oldVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN && newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                {
                    addPeer(userid);

                    // update mCurrentPeers's counter in order to consider groupchats. Otherwise, the count=1 will be zeroed if
                    // the user (now contact again) is removed from any groupchat, resulting in an incorrect DELPEERS
                    for (auto it = mChatMembers.begin(); it != mChatMembers.end(); it++)
                    {
                        if (it->second.has(userid))
                        {
                            addPeer(userid);
                        }
                    }
                }
            }
        }

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
        mCurrentPeers.clear();
        mContacts.clear();
        mChatMembers.clear();

        auto wptr = weakHandle();
        marshallCall([wptr, this, contacts, chats, scsn]()
        {
            if (wptr.deleted())
            {
                return;
            }

            assert(!mLastScsn.isValid());
            assert(mCurrentPeers.empty());
            assert(mContacts.empty());
            assert(mChatMembers.empty());

            mLastScsn = scsn;
            mCurrentPeers.clear();
            mContacts.clear();
            mChatMembers.clear();

            // initialize the list of contacts
            for (int i = 0; i < contacts->size(); i++)
            {
                ::mega::MegaUser *user = contacts->get(i);
                uint64_t userid = user->getHandle();
                int visibility = user->getVisibility();

                mContacts[userid] = visibility; // add ex-contacts to identify them
                if (visibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                {
                    mCurrentPeers.insert(userid);
                }
            }

            // initialize chatroom's peers
            for (int i = 0; i < chats->size(); i++)
            {
                const ::mega::MegaTextChat *chat = chats->get(i);
                uint64_t chatid = chat->getHandle();
                const ::mega::MegaTextChatPeerList *peerlist = chat->getPeerList();
                if (!peerlist)
                {
                    continue;   // no peers in this chatroom
                }

                for (int j = 0; j < peerlist->size(); j++)
                {
                    uint64_t userid = peerlist->getPeerHandle(j);
                    if (!isExContact(userid))
                    {
                        mCurrentPeers.insert(userid);
                        mChatMembers[chatid].insert(userid);
                    }
                }
            }

            // finally send to presenced the initial set of peers
            pushPeers();

        }, mKarereClient->appCtx);
    }
}

void Client::heartbeat()
{
    // if a heartbeat is received but we are already offline...
    if (!mHeartbeatEnabled)
        return;

    auto now = time(NULL);
    if (autoAwayInEffect()
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
            PRESENCED_LOG_WARNING("Failed to send keepalive, reconnecting...");
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
    bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);
    assert(cachedIPs);
    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;

    setConnState(kConnecting);
    PRESENCED_LOG_DEBUG("Connecting to presenced using the IP: %s", mTargetIp.c_str());

    bool rt = wsConnect(mKarereClient->websocketIO, mTargetIp.c_str(),
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
            if (wsConnect(mKarereClient->websocketIO, mTargetIp.c_str(),
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
        if (disconnect)
        {
            PRESENCED_LOG_WARNING("retryPendingConnection: forced reconnection!");

            setConnState(kDisconnected);
            abortRetryController();
            reconnect();
        }
        else if (mRetryCtrl && mRetryCtrl->state() == rh::State::kStateRetryWait)
        {
            PRESENCED_LOG_WARNING("retryPendingConnection: abort backoff and reconnect immediately");

            assert(!isOnline());
            assert(!mHeartbeatEnabled);

            mRetryCtrl->restart();
        }
        else
        {
            PRESENCED_LOG_WARNING("retryPendingConnection: ignored (currently connecting/connected, no forced disconnect was requested)");
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
        PRESENCED_LOG_DEBUG("Tried to change user-active to the current state: %d", (int)mLastSentUserActive);
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
    mLastGreenVisible = !(code & Config::kLastGreenVisibleMask);
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
                        if (autoAwayInEffect())
                        {
                            // signal whether the user is active or inactive
                            bool isActive = ((time(NULL) - mTsLastUserActivity) < mConfig.mAutoawayTimeout);
                            sendUserActive(isActive, true);
                        }
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

                // convert the received minutes into a UNIX timestamp
                time_t lastGreenTs = time(NULL) - (lastGreen * 60);
                mPeersLastGreen[userid] = lastGreenTs;

                CALL_LISTENER(onPresenceLastGreenUpdated, userid);
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
    {
        PRESENCED_LOG_DEBUG("Tried to change connection state to the current state: %s", connStateToStr(newState));
        return;
    }
    else
    {
        PRESENCED_LOG_DEBUG("Connection state change: %s --> %s", connStateToStr(mConnState), connStateToStr(newState));
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

                PRESENCED_LOG_DEBUG("Reconnection attempt has not succeed after %d. Reconnecting...", kConnectTimeout);
                mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99005, "Reconnection timed out (presenced)");

                retryPendingConnection(true);

            }, kConnectTimeout * 1000, mKarereClient->appCtx);
        }

        // if disconnected, we don't really know the presence status anymore
        for (auto it = mCurrentPeers.begin(); it != mCurrentPeers.end(); it++)
        {
            CALL_LISTENER(onPresenceChange, it->first, Presence::kInvalid);
        }
        CALL_LISTENER(onPresenceChange, mKarereClient->myHandle(), Presence::kInvalid);
    }
    else if (mConnState == kConnected)
    {
        PRESENCED_LOG_DEBUG("Presenced connected to %s", mTargetIp.c_str());

        mDNScache.connectDone(mUrl.host, mTargetIp);
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
void Client::addPeer(karere::Id peer)
{
    if (isExContact(peer))
    {
        PRESENCED_LOG_WARNING("Not sending ADDPEERS for user %s because it's ex-contact", peer.toString().c_str());
        return;
    }

    assert(mLastScsn.isValid());

    int result = mCurrentPeers.insert(peer);
    if (result == 1) //refcount = 1, wasnt there before
    {
        size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t);

        Command cmd(OP_SNADDPEERS, totalSize);
        cmd.append<uint64_t>(mLastScsn.val);
        cmd.append<uint32_t>(1);
        cmd.append<uint64_t>(peer.val);

        sendCommand(std::move(cmd));
    }
}

void Client::removePeer(karere::Id peer, bool force)
{
    assert(mLastScsn.isValid());

    auto it = mCurrentPeers.find(peer);
    if (it == mCurrentPeers.end())
    {
        PRESENCED_LOG_WARNING("removePeer: Unknown peer %s", peer.toString().c_str());
        return;
    }
    if (--it->second > 0)
    {
        if (!force)
        {
            PRESENCED_LOG_DEBUG("removePeer: decremented number of references for peer %s", peer.toString().c_str());
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

    // Remove peer from mPeersLastGreen map if exists
    mPeersLastGreen.erase(peer.val);


    size_t totalSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t);

    Command cmd(OP_SNDELPEERS, totalSize);
    cmd.append<uint64_t>(mLastScsn.val);
    cmd.append<uint32_t>(1);
    cmd.append<uint64_t>(peer.val);

    sendCommand(std::move(cmd));
}
}
