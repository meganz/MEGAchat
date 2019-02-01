#include "chatd.h"
#include "chatClient.h"
#include "chatdICrypto.h"
#include "base64url.h"
#include <algorithm>
#include <random>
#include <regex>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

using namespace std;
using namespace promise;
using namespace karere;

#if WIN32
#include <mega/utils.h>
using ::mega::mega_snprintf;   // enables the calls to snprintf below which are #defined
#endif


#define CHATD_LOG_LISTENER_CALLS

#define ID_CSTR(id) id.toString().c_str()

// logging for a specific chatid - prepends the chatid and calls the normal logging macro
#define CHATID_LOG_DEBUG(fmtString,...) CHATD_LOG_DEBUG("[shard %d]: %s: " fmtString, mConnection.shardNo(), ID_CSTR(chatId()), ##__VA_ARGS__)
#define CHATID_LOG_WARNING(fmtString,...) CHATD_LOG_WARNING("[shard %d]: %s: " fmtString, mConnection.shardNo(), ID_CSTR(chatId()), ##__VA_ARGS__)
#define CHATID_LOG_ERROR(fmtString,...) CHATD_LOG_ERROR("[shard %d]: %s: " fmtString, mConnection.shardNo(), ID_CSTR(chatId()), ##__VA_ARGS__)

// logging for a specific shard - prepends the shard number and calls the normal logging macro
#define CHATDS_LOG_DEBUG(fmtString,...) CHATD_LOG_DEBUG("[shard %d]: " fmtString, shardNo(), ##__VA_ARGS__)
#define CHATDS_LOG_WARNING(fmtString,...) CHATD_LOG_WARNING("[shard %d]: " fmtString, shardNo(), ##__VA_ARGS__)
#define CHATDS_LOG_ERROR(fmtString,...) CHATD_LOG_ERROR("[shard %d]: " fmtString, shardNo(), ##__VA_ARGS__)

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

#define CALL_LISTENER_FH(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
        if (mListener)                                                                         \
        {                                                                                       \
          CHATD_LOG_DEBUG("Calling FilteredHistoryListener::" #methodName "()");                       \
          mListener->methodName(__VA_ARGS__);                                                   \
        }                                                                                       \
      } catch(std::exception& e) {                                                              \
          CHATD_LOG_WARNING("Exception thrown from FilteredHistoryListener::" #methodName "():\n%s", e.what());\
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

#define CALL_DB_FH(methodName,...)                                                           \
    do {                                                                                        \
      try {                                                                                     \
          CHATD_LOG_DB_CALL("Calling DbInterface::" #methodName "()");                               \
          mDb->methodName(__VA_ARGS__);                                                   \
      } catch(std::exception& e) {                                                              \
          CHATD_LOG_ERROR("Exception thrown from DbInterface::" #methodName "():\n%s", e.what());\
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

Client::Client(karere::Client *aKarereClient) :
    mMyHandle(aKarereClient->myHandle()),
    mApi(&aKarereClient->api),
    mKarereClient(aKarereClient)
{
    mRichPrevAttrCbHandle = mKarereClient->userAttrCache().getAttr(mMyHandle, ::mega::MegaApi::USER_ATTR_RICH_PREVIEWS, this,
       [](::Buffer *buf, void* userp)
       {
            Client *client = static_cast<Client*>(userp);
            client->mRichLinkState = kRichLinkNotDefined;

            // if user changed the option to do/don't generate rich links...
            if (buf && !buf->empty())
            {
                char tmp[2];
                base64urldecode(buf->buf(), buf->size(), tmp, 2);
                switch(*tmp)
                {
                case '1':
                    client->mRichLinkState = kRichLinkEnabled;
                    break;

                case '0':
                    client->mRichLinkState = kRichLinkDisabled;
                    break;

                default:
                    CHATD_LOG_WARNING("Unexpected value for user attribute USER_ATTR_RICH_PREVIEWS - value: %c", *tmp);
                    break;
                }
            }

            // proceed to prepare rich-links or to discard them
            switch (client->mRichLinkState)
            {
            case kRichLinkEnabled:
                for (auto& chat: client->mChatForChatId)
                {
                    chat.second->requestPendingRichLinks();
                }
                break;

            case kRichLinkDisabled:
                for (auto& chat: client->mChatForChatId)
                {
                    chat.second->removePendingRichLinks();
                }
                break;

            case kRichLinkNotDefined:
                break;
            }
       });

    // initialize the most recent message for each user
    SqliteStmt stmt1(mKarereClient->db, "SELECT DISTINCT userid FROM history");
    while (stmt1.step())
    {
        karere::Id userid = stmt1.uint64Col(0);
        if (userid == Id::COMMANDER())
        {
            continue;
        }

        SqliteStmt stmt2(mKarereClient->db, "SELECT MAX(ts) FROM history WHERE userid = ?");
        stmt2 << userid.val;
        if (stmt2.step())
        {
            mLastMsgTs[userid] = stmt2.uintCol(0);
        }
    }
}

Chat& Client::createChat(Id chatid, int shardNo, const std::string& url,
    Listener* listener, const karere::SetOfIds& users, ICrypto* crypto, uint32_t chatCreationTs, bool isGroup)
{
    auto chatit = mChatForChatId.find(chatid);
    if (chatit != mChatForChatId.end())
    {
        CHATD_LOG_WARNING("Client::createChat: Chat with chatid %s already exists, returning existing instance", ID_CSTR(chatid));
        return *chatit->second;
    }

    // instantiate a Connection object for this shard if needed
    Connection* conn;
    auto it = mConnections.find(shardNo);
    if (it == mConnections.end())
    {
        conn = new Connection(*this, shardNo);
        mConnections.emplace(std::piecewise_construct,
            std::forward_as_tuple(shardNo), std::forward_as_tuple(conn));
    }
    else
    {
        conn = it->second.get();
    }

    if (!url.empty())
    {
        conn->mUrl.parse(url);
        conn->mUrl.path.append("/").append(std::to_string(Client::chatdVersion));
    }
    // map chatid to this shard
    mConnectionForChatId[chatid] = conn;

    // always update the URL to give the API an opportunity to migrate chat shards between hosts
    Chat* chat = new Chat(*conn, chatid, listener, users, chatCreationTs, crypto, isGroup);
    // add chatid to the connection's chatids
    conn->mChatIds.insert(chatid);
    mChatForChatId.emplace(chatid, std::shared_ptr<Chat>(chat));
    return *chat;
}
void Client::sendKeepalive()
{
    for (auto& conn: mConnections)
    {
        conn.second->sendKeepalive(mKeepaliveType);
    }
}

void Client::sendEcho()
{
    for (auto& conn: mConnections)
    {
        if (conn.second->isOnline())
        {
            conn.second->sendEcho();
        }
    }
}

void Client::setKeepaliveType(bool isInBackground)
{
    mKeepaliveType = isInBackground ? OP_KEEPALIVEAWAY : OP_KEEPALIVE;
}

uint8_t Client::keepaliveType()
{
    return mKeepaliveType;
}

std::shared_ptr<Chat> Client::chatFromId(Id chatid) const
{
    auto it = mChatForChatId.find(chatid);
    return (it == mChatForChatId.end()) ? nullptr : it->second;
}

Chat &Client::chats(Id chatid) const
{
    auto it = mChatForChatId.find(chatid);
    if (it == mChatForChatId.end())
    {
        throw std::runtime_error("chatidChat: Unknown chatid "+chatid.toString());
    }
    return *it->second;
}

void Client::notifyUserIdle()
{
    if (mKeepaliveType == OP_KEEPALIVEAWAY)
        return;
    mKeepaliveType = OP_KEEPALIVEAWAY;
    cancelSeenTimers();
    sendKeepalive();
}

void Client::cancelSeenTimers()
{
    for (std::set<megaHandle>::iterator it = mSeenTimers.begin(); it != mSeenTimers.end(); ++it)
   {
        cancelTimeout(*it, mKarereClient->appCtx);
   }
   mSeenTimers.clear();
}

void Client::notifyUserActive()
{
    if (mKeepaliveType == OP_KEEPALIVE)
        return;

    sendEcho();

    mKeepaliveType = OP_KEEPALIVE;
    sendKeepalive();
}

bool Client::isMessageReceivedConfirmationActive() const
{
    return mMessageReceivedConfirmation;
}

::mega::m_time_t Client::getLastMsgTs(Id userid) const
{
    std::map<karere::Id, ::mega::m_time_t>::const_iterator it = mLastMsgTs.find(userid);
    if (it != mLastMsgTs.end())
    {
        return it->second;
    }
    return 0;
}

void Client::setLastMsgTs(Id userid, ::mega::m_time_t lastMsgTs)
{
    mLastMsgTs[userid] = lastMsgTs;
}

uint8_t Client::richLinkState() const
{
    return mRichLinkState;
}

bool Client::areAllChatsLoggedIn()
{
    bool allConnected = true;

    for (map<Id, shared_ptr<Chat>>::iterator it = mChatForChatId.begin(); it != mChatForChatId.end(); it++)
    {
        Chat* chat = it->second.get();
        if (!chat->isLoggedIn() && !chat->isDisabled())
        {
            allConnected = false;
            break;
        }
    }

    if (allConnected)
    {
        CHATD_LOG_DEBUG("We are logged in to all chats");
    }

    return allConnected;
}

void Chat::connect()
{
    // attempt a connection ONLY if this is a new shard.
    if (mConnection.state() == Connection::kStateNew)
    {
        mConnection.setState(Connection::kStateFetchingUrl);
        auto wptr = getDelTracker();
        mChatdClient.mApi->call(&::mega::MegaApi::getUrlChat, mChatId)
        .then([wptr, this](ReqResult result)
        {
            if (wptr.deleted())
            {
                CHATD_LOG_DEBUG("Chatd URL request completed, but chatd client was deleted");
                return;
            }

            const char* url = result->getLink();
            if (!url || !url[0])
            {
                CHATID_LOG_ERROR("No chatd URL received from API");
                return;
            }

            std::string sUrl = url;
            mConnection.mUrl.parse(sUrl);
            mConnection.mUrl.path.append("/").append(std::to_string(Client::chatdVersion));

            mConnection.reconnect()
            .fail([this](const ::promise::Error& err)
            {
                CHATID_LOG_ERROR("Chat::connect(): Error connecting to server after getting URL: %s", err.what());
            });
        });
    }
    else if (mConnection.isOnline())
    {
        login();
    }
}

void Chat::login()
{
    assert(mConnection.isOnline());
    mUserDump.clear();
    setOnlineState(kChatStateJoining);
    // In both cases (join/joinrangehist), don't block history messages being sent to app
    mServerOldHistCbEnabled = false;

    ChatDbInfo info;
    mDbInterface->getHistoryInfo(info);
    mOldestKnownMsgId = info.oldestDbId;
    if (mOldestKnownMsgId) //if we have local history
        joinRangeHist(info);
    else
        join();
}

Connection::Connection(Client& chatdClient, int shardNo)
: mChatdClient(chatdClient), mShardNo(shardNo),
  mDNScache(mChatdClient.mKarereClient->websocketIO->mDnsCache)
{}

void Connection::wsConnectCb()
{
    setState(kStateConnected);
}

void Connection::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    string reason;
    if (preason)
        reason.assign(preason, reason_len);

    onSocketClose(errcode, errtype, reason);
}

void Connection::onSocketClose(int errcode, int errtype, const std::string& reason)
{
    if (mChatdClient.mKarereClient->isTerminated())
    {
        CHATDS_LOG_WARNING("Socket close but karere client was terminated.");
        return;
    }

    CHATDS_LOG_WARNING("Socket close on IP %s. Reason: %s", mTargetIp.c_str(), reason.c_str());
    auto oldState = mState;
    setState(kStateDisconnected);

    assert(oldState != kStateDisconnected);

    usingipv6 = !usingipv6;
    mTargetIp.clear();

    if (oldState == kStateConnected)
    {
        CHATDS_LOG_DEBUG("Socket close at state kStateConnected");

        assert(!mRetryCtrl);
        reconnect(); //start retry controller
    }
    else // (mState < kStateConnected) --> tell retry controller that the connect attempt failed
    {
        CHATDS_LOG_DEBUG("Socket close and state is not kStateConnected (but %s), start retry controller", connStateToStr(oldState));

        assert(mRetryCtrl);
        assert(!mConnectPromise.succeeded());
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
    }
}

bool Connection::sendKeepalive(uint8_t opcode)
{
    CHATDS_LOG_DEBUG("send %s", Command::opcodeToStr(opcode));
    return sendBuf(Command(opcode));
}

void Connection::sendEcho()
{
    if (!mUrl.isValid())    // the connection is not ready yet (i.e. initialization in offline-mode)
    {
        CHATDS_LOG_DEBUG("sendEcho(): connection not initialized yet");
        return;
    }

    if (mEchoTimer) // one is already sent
    {
        CHATDS_LOG_DEBUG("sendEcho(): already sent, waiting for response");
        return;
    }

    if (!isOnline())
    {
        CHATDS_LOG_DEBUG("sendEcho(): connection is down, cannot send");
        return;
    }

    auto wptr = weakHandle();
    mEchoTimer = setTimeout([this, wptr]()
    {
        if (wptr.deleted())
            return;

        mEchoTimer = 0;

        CHATDS_LOG_DEBUG("Echo response not received in %d secs. Reconnecting...", kEchoTimeout);
        mChatdClient.mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99001, "ECHO response timed out");

        setState(kStateDisconnected);
        abortRetryController();
        reconnect();

    }, kEchoTimeout * 1000, mChatdClient.mKarereClient->appCtx);

    CHATDS_LOG_DEBUG("send ECHO");
    sendBuf(Command(OP_ECHO));
}

void Connection::sendCallReqDeclineNoSupport(Id chatid, Id callid)
{
    Command msg(OP_RTMSG_BROADCAST);
    msg.write<uint64_t>(1, chatid.val);
    msg.write<uint64_t>(9, 0);
    msg.write<uint32_t>(17, 0);
    msg.write<uint16_t>(21, 10);        // Payload length -> opCode(1) + callid(8) + termCode(1)
    msg.write<uint8_t>(23, rtcModule::RTCMD_CALL_REQ_DECLINE);          // RTCMD_CALL_REQ_DECLINE
    msg.write<uint64_t>(24, callid.val);
    msg.write<uint8_t>(32, rtcModule::kErrNotSupported);         // Termination code kErrNotSupported = 37
    auto& chat = mChatdClient.chats(chatid);
    chat.sendCommand(std::move(msg));
}

void Connection::setState(State state)
{
    if (mState == state)
    {
        CHATDS_LOG_DEBUG("Tried to change connection state to the current state: %s", connStateToStr(state));
        return;
    }
    else
    {
        CHATDS_LOG_DEBUG("Connection state change: %s --> %s", connStateToStr(mState), connStateToStr(state));
        mState = state;
    }

    if (mState == kStateDisconnected)
    {
        mHeartbeatEnabled = false;

        // if a socket is opened, close it immediately
        if (wsIsConnected())
        {
            wsDisconnect(true);
        }

        // if an ECHO was sent, no need to wait for its response
        if (mEchoTimer)
        {
            cancelTimeout(mEchoTimer, mChatdClient.mKarereClient->appCtx);
            mEchoTimer = 0;
        }

        // if connect-timer is running, it must be reset (kStateResolving --> kStateDisconnected)
        if (mConnectTimer)
        {
            cancelTimeout(mConnectTimer, mChatdClient.mKarereClient->appCtx);
            mConnectTimer = 0;
        }

        if (!mChatdClient.mKarereClient->isTerminated())
        {
            // start a timer to ensure the connection is established after kConnectTimeout. Otherwise, reconnect
            auto wptr = weakHandle();
            mConnectTimer = setTimeout([this, wptr]()
            {
                if (wptr.deleted())
                    return;

                mConnectTimer = 0;

                CHATDS_LOG_DEBUG("Reconnection attempt has not succeed after %d. Reconnecting...", kConnectTimeout);
                mChatdClient.mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99004, "Reconnection timed out");

                retryPendingConnection(true);

            }, kConnectTimeout * 1000, mChatdClient.mKarereClient->appCtx);
        }

        // notify chatrooms that connection is down
        for (auto& chatid: mChatIds)
        {
            auto& chat = mChatdClient.chats(chatid);
            chat.onDisconnect();

            // remove calls (if any)
#ifndef KARERE_DISABLE_WEBRTC
            if (mChatdClient.mKarereClient->rtc)
            {
                mChatdClient.mKarereClient->rtc->removeCall(chatid);
            }
#endif
        }
        // and stop call-timers in this shard
#ifndef KARERE_DISABLE_WEBRTC
        if (mChatdClient.mRtcHandler)
        {
            mChatdClient.mRtcHandler->stopCallsTimers(mShardNo);
        }
#endif
    }
    else if (mState == kStateConnected)
    {
        CHATDS_LOG_DEBUG("Chatd connected to %s", mTargetIp.c_str());

        mDNScache.connectDone(mUrl.host, mTargetIp);
        assert(!mConnectPromise.done());
        mConnectPromise.resolve();
        mRetryCtrl.reset();

        if (mConnectTimer)
        {
            cancelTimeout(mConnectTimer, mChatdClient.mKarereClient->appCtx);
            mConnectTimer = 0;
        }
    }
}

Connection::State Connection::state() const
{
    return mState;
}

bool Connection::isOnline() const
{
    return mState == kStateConnected; //(mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
}

const std::set<Id> &Connection::chatIds() const
{
    return mChatIds;
}

uint32_t Connection::clientId() const
{
    return mClientId;
}

Promise<void> Connection::reconnect()
{
    if (mChatdClient.mKarereClient->isTerminated())
    {
        CHATDS_LOG_WARNING("Reconnect attempt initiated, but karere client was terminated.");
        assert(false);
        return ::promise::Error("Reconnect called when karere::Client is terminated", kErrorAccess, kErrorAccess);
    }

    mChatdClient.mKarereClient->setCommitMode(false);
    assert(!mHeartbeatEnabled);
    assert(!mRetryCtrl);
    try
    {
        if (mState >= kStateResolving) //would be good to just log and return, but we have to return a promise
            throw std::runtime_error(std::string("Already connecting/connected to shard ")+std::to_string(mShardNo));

        if (!mUrl.isValid())
            throw std::runtime_error("Current URL is not valid for shard "+std::to_string(mShardNo));

        setState(kStateResolving);

        // if there were an existing retry in-progress, abort it first or it will kick in after its backoff
        abortRetryController();

        // create a new retry controller and return its promise for reconnection
        auto wptr = weakHandle();
        mRetryCtrl.reset(createRetryController("chatd] [shard "+std::to_string(mShardNo), [this](size_t attemptNo, DeleteTrackable::Handle wptr) -> Promise<void>
        {
            if (wptr.deleted())
            {
                CHATDS_LOG_DEBUG("Reconnect attempt initiated, but chatd client was deleted.");
                return ::promise::_Void();
            }

            setState(kStateDisconnected);
            mConnectPromise = Promise<void>();

            string ipv4, ipv6;
            bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);

            setState(kStateResolving);
            CHATDS_LOG_DEBUG("Resolving hostname %s...", mUrl.host.c_str());

            for (auto& chatid: mChatIds)
            {
                auto& chat = mChatdClient.chats(chatid);
                if (!chat.isDisabled())
                    chat.setOnlineState(kChatStateConnecting);
            }

            auto retryCtrl = mRetryCtrl.get();
            int statusDNS = wsResolveDNS(mChatdClient.mKarereClient->websocketIO, mUrl.host.c_str(),
                         [wptr, cachedIPs, this, retryCtrl, attemptNo](int statusDNS, std::vector<std::string> &ipsv4, std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    CHATDS_LOG_DEBUG("DNS resolution completed but ignored: chatd client was deleted.");
                    return;
                }

                if (mChatdClient.mKarereClient->isTerminated())
                {
                    CHATDS_LOG_DEBUG("DNS resolution completed but karere client was terminated.");
                    return;
                }

                if (!mRetryCtrl)
                {
                    CHATDS_LOG_DEBUG("DNS resolution completed but ignored: connection is already established using cached IP");
                    assert(isOnline());
                    assert(cachedIPs);
                    return;
                }
                if (mRetryCtrl.get() != retryCtrl)
                {
                    CHATDS_LOG_DEBUG("DNS resolution completed but ignored: a newer RetryController has already started");
                    return;
                }
                if (mRetryCtrl->currentAttemptNo() != attemptNo)
                {
                    CHATDS_LOG_DEBUG("DNS resolution completed but ignored: a newer attempt is already started (old: %d, new: %d)",
                                     attemptNo, mRetryCtrl->currentAttemptNo());
                    return;
                }

                if (statusDNS < 0 || (ipsv4.empty() && ipsv6.empty()))
                {
                    if (isOnline() && cachedIPs)
                    {
                        assert(false);  // this case should be handled already at: if (!mRetryCtrl)
                        CHATDS_LOG_WARNING("DNS error, but connection is established. Relaying on cached IPs...");
                        return;
                    }

                    if (statusDNS < 0)
                    {
                        CHATDS_LOG_ERROR("Async DNS error in chatd. Error code: %d", statusDNS);
                    }
                    else
                    {
                        CHATDS_LOG_ERROR("Async DNS error in chatd. Empty set of IPs");
                    }

                    assert(!isOnline());
                    onSocketClose(0, 0, "Async DNS error (chatd)");
                    return;
                }

                if (!cachedIPs) // connect() required initial DNS lookup
                {
                    CHATDS_LOG_DEBUG("Hostname resolved by first time. Connecting...");

                    mDNScache.set(mUrl.host,
                                  ipsv4.size() ? ipsv4.at(0) : "",
                                  ipsv6.size() ? ipsv6.at(0) : "");
                    doConnect();
                    return;
                }

                if (mDNScache.isMatch(mUrl.host, ipsv4, ipsv6))
                {
                    CHATDS_LOG_DEBUG("DNS resolve matches cached IPs.");
                }
                else
                {
                    // update DNS cache
                    bool ret = mDNScache.set(mUrl.host,
                                  ipsv4.size() ? ipsv4.at(0) : "",
                                  ipsv6.size() ? ipsv6.at(0) : "");
                    assert(ret);

                    CHATDS_LOG_WARNING("DNS resolve doesn't match cached IPs. Forcing reconnect...");
                    onSocketClose(0, 0, "DNS resolve doesn't match cached IPs (chatd)");
                }
            });

            // immediate error at wsResolveDNS()
            if (statusDNS < 0)
            {
                CHATDS_LOG_ERROR("Sync DNS error in chatd. Error code: %d", statusDNS);
                string errStr = "Sync DNS error in chatd for shard "+std::to_string(mShardNo);

                assert(!mConnectPromise.done());
                mConnectPromise.reject(errStr, statusDNS, kErrorTypeGeneric);
            }
            else if (cachedIPs) // if wsResolveDNS() failed immediately, very likely there's
            // no network connection, so it's futile to attempt to connect
            {
                doConnect();
            }

            return mConnectPromise
            .then([wptr, this]()
            {
                if (wptr.deleted())
                    return;

                assert(isOnline());
                sendCommand(Command(OP_CLIENTID)+mChatdClient.mKarereClient->myIdentity());
                mTsLastRecv = time(NULL);   // data has been received right now, since connection is established
                mHeartbeatEnabled = true;
                sendKeepalive(mChatdClient.mKeepaliveType);
                rejoinExistingChats();
            });
        }, wptr, mChatdClient.mKarereClient->appCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

        return static_cast<Promise<void>&>(mRetryCtrl->start());
    }
    KR_EXCEPTION_TO_PROMISE(kPromiseErrtype_chatd);
}

void Connection::abortRetryController()
{
    if (!mRetryCtrl)
    {
        return;
    }

    assert(!isOnline());

    CHATDS_LOG_DEBUG("Reconnection was aborted");
    mRetryCtrl->abort();
    mRetryCtrl.reset();
}

void Connection::disconnect()
{
    setState(kStateDisconnected);
}

void Connection::doConnect()
{
    string ipv4, ipv6;
    bool cachedIPs = mDNScache.get(mUrl.host, ipv4, ipv6);
    assert(cachedIPs);
    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;

    setState(kStateConnecting);
    CHATDS_LOG_DEBUG("Connecting to chatd using the IP: %s", mTargetIp.c_str());

    bool rt = wsConnect(mChatdClient.mKarereClient->websocketIO, mTargetIp.c_str(),
              mUrl.host.c_str(),
              mUrl.port,
              mUrl.path.c_str(),
              mUrl.isSecure);

    if (!rt)    // immediate failure --> try the other IP family (if available)
    {
        CHATDS_LOG_DEBUG("Connection to chatd failed using the IP: %s", mTargetIp.c_str());

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
            CHATDS_LOG_DEBUG("Retrying using the IP: %s", mTargetIp.c_str());
            if (wsConnect(mChatdClient.mKarereClient->websocketIO, mTargetIp.c_str(),
                                      mUrl.host.c_str(),
                                      mUrl.port,
                                      mUrl.path.c_str(),
                                      mUrl.isSecure))
            {
                return;
            }
            CHATDS_LOG_DEBUG("Connection to chatd failed using the IP: %s", mTargetIp.c_str());
        }

        onSocketClose(0, 0, "Websocket error on wsConnect (chatd)");
    }
}

void Connection::retryPendingConnection(bool disconnect)
{
    if (mUrl.isValid())
    {
        if (disconnect)
        {
            CHATDS_LOG_WARNING("retryPendingConnection: forced reconnection!");

            setState(kStateDisconnected);
            abortRetryController();
            reconnect();
        }
        else if (mRetryCtrl && mRetryCtrl->state() == rh::State::kStateRetryWait)
        {
            CHATDS_LOG_WARNING("retryPendingConnection: abort backoff and reconnect immediately");

            assert(!isOnline());
            assert(!mHeartbeatEnabled);
            assert(!mEchoTimer);

            mRetryCtrl->restart();
        }
        else
        {
            CHATDS_LOG_WARNING("retryPendingConnection: ignored (currently connecting/connected, no forced disconnect was requested)");
        }
    }
    else
    {
        CHATDS_LOG_WARNING("No valid URL provided to retry pending connections");
    }
}

Connection::~Connection()
{
    disconnect();
}

void Connection::heartbeat()
{
    // if a heartbeat is received but we are already offline...
    if (!mHeartbeatEnabled)
        return;

    if (time(NULL) - mTsLastRecv >= Connection::kIdleTimeout)
    {
        CHATDS_LOG_WARNING("Connection inactive for too long, reconnecting...");

        setState(kStateDisconnected);
        abortRetryController();
        reconnect();
    }
}

int Connection::shardNo() const
{
    return mShardNo;
}

void Client::disconnect()
{
    for (auto& conn: mConnections)
    {
        conn.second->disconnect();
    }
}

void Client::retryPendingConnections(bool disconnect)
{
    for (auto& conn: mConnections)
    {
        conn.second->retryPendingConnection(disconnect);
    }
}

void Client::heartbeat()
{
    for (auto& conn: mConnections)
    {
        conn.second->heartbeat();
    }
}

bool Connection::sendBuf(Buffer&& buf)
{
    if (!isOnline())
        return false;

    bool rc = wsSendMessage(buf.buf(), buf.dataSize());
    buf.free();
    return rc;
}

bool Connection::sendCommand(Command&& cmd)
{
    CHATDS_LOG_DEBUG("send %s", cmd.toString().c_str());
    bool result = sendBuf(std::move(cmd));
    if (!result)
        CHATDS_LOG_DEBUG("Can't send, we are offline");
    return result;
}

bool Chat::sendCommand(Command&& cmd)
{
    CHATID_LOG_DEBUG("send %s", cmd.toString().c_str());
    bool result = mConnection.sendBuf(std::move(cmd));
    if (!result)
        CHATID_LOG_DEBUG("  Can't send, we are offline");
    return result;
}

bool Chat::sendCommand(const Command& cmd)
{
    Buffer buf(cmd.buf(), cmd.dataSize());
    CHATID_LOG_DEBUG("send %s", cmd.toString().c_str());
    auto result = mConnection.sendBuf(std::move(buf));
    if (!result)
        CHATID_LOG_DEBUG("  Can't send, we are offline");
    return result;
}

#ifndef KARERE_DISABLE_WEBRTC
namespace rtcModule { std::string rtmsgCommandToString(const StaticBuffer&); }
#endif

string Command::toString(const StaticBuffer& data)
{
    auto opcode = data.read<uint8_t>(0);
    switch(opcode)
    {
        case OP_NEWMSG:
        case OP_NEWNODEMSG:
        {
            auto& msgcmd = static_cast<const MsgCommand&>(data);
            string tmpString;
            if (opcode == OP_NEWMSG)
                tmpString.append("NEWMSG - msgxid: ");
            else
                tmpString.append("NEWNODEMSG - msgxid: ");
            tmpString.append(ID_CSTR(msgcmd.msgid()));
            tmpString.append(", keyid: ");
            tmpString.append(to_string(msgcmd.keyId()));
            tmpString.append(", ts: ");
            tmpString.append(to_string(msgcmd.ts()));
            return tmpString;
        }
        case OP_MSGUPD:
        {
            auto& msgcmd = static_cast<const MsgCommand&>(data);
            string tmpString;
            tmpString.append("MSGUPD - msgid: ");
            tmpString.append(ID_CSTR(msgcmd.msgid()));
            tmpString.append(", keyid: ");
            tmpString.append(to_string(msgcmd.keyId()));
            tmpString.append(", ts: ");
            tmpString.append(to_string(msgcmd.ts()));
            tmpString.append(", tsdelta: ");
            tmpString.append(to_string(msgcmd.updated()));
            return tmpString;
        }
        case OP_MSGUPDX:
        {
            auto& msgcmd = static_cast<const MsgCommand&>(data);
            string tmpString;
            tmpString.append("MSGUPDX - msgxid: ");
            tmpString.append(ID_CSTR(msgcmd.msgid()));
            tmpString.append(", keyid: ");
            tmpString.append(to_string(msgcmd.keyId()));
            tmpString.append(", ts: ");
            tmpString.append(to_string(msgcmd.ts()));
            tmpString.append(", tsdelta: ");
            tmpString.append(to_string(msgcmd.updated()));
            return tmpString;
        }
        case OP_SEEN:
        {
            Id msgId = data.read<uint64_t>(9);
            string tmpString;
            tmpString.append("SEEN - msgid: ");
            tmpString.append(ID_CSTR(msgId));
            return tmpString;
        }
        case OP_NEWKEY:
        {
            auto& keycmd = static_cast<const KeyCommand&>(data);
            string tmpString;
            tmpString.append("NEWKEY - keyxid: ");
            tmpString.append(to_string(keycmd.keyId()));
            tmpString.append(", localkeyid: ");
            tmpString.append(to_string(keycmd.localKeyid()));
            return tmpString;
        }
        case OP_CLIENTID:
        {
            char tmpbuf[64];
            snprintf(tmpbuf, 63, "0x%" PRIx64, data.read<uint64_t>(1));
            return string("CLIENTID: ")+tmpbuf;
        }
        case OP_INCALL:
        {
            string tmpString;
            karere::Id userId = data.read<uint64_t>(9);
            uint32_t clientId = data.read<uint32_t>(17);
            tmpString.append("INCALL userId: ");
            tmpString.append(ID_CSTR(userId));
            tmpString.append(", clientId: ");
            std::stringstream stream;
            stream << std::hex << clientId;
            tmpString.append(stream.str());
            return tmpString;
        }
        case OP_ENDCALL:
        {
            string tmpString;
            karere::Id userId = data.read<uint64_t>(9);
            uint32_t clientId = data.read<uint32_t>(17);
            tmpString.append("ENDCALL userId: ");
            tmpString.append(ID_CSTR(userId));
            tmpString.append(", clientId: ");
            std::stringstream stream;
            stream << std::hex << clientId;
            tmpString.append(stream.str());
            return tmpString;
        }
#ifndef KARERE_DISABLE_WEBRTC
        case OP_RTMSG_ENDPOINT:
        case OP_RTMSG_USER:
        case OP_RTMSG_BROADCAST:
            return ::rtcModule::rtmsgCommandToString(data);
#endif
        case OP_NODEHIST:
        {
            string tmpString;
            karere::Id chatid = data.read<uint64_t>(1);
            karere::Id msgid = data.read<uint64_t>(9);
            int32_t count = data.read<int32_t>(17);
            tmpString.append("NODEHIST chatid: ");
            tmpString.append(ID_CSTR(chatid));
            tmpString.append(", msgid: ");
            tmpString.append(ID_CSTR(msgid));
            tmpString.append(", count: ");
            tmpString.append(std::to_string(count));
            return tmpString;
        }

        default:
            return opcodeToStr(opcode);
    }
}
string Command::toString() const
{
    return toString(*this);
}

string KeyCommand::toString() const
{
    assert(opcode() == OP_NEWKEY);
    string tmpString;
    tmpString.append("NEWKEY - keyxid: ");
    tmpString.append(to_string(keyId()));
    tmpString.append(", localkeyid: ");
    tmpString.append(to_string(mLocalKeyid));
    return tmpString;
}
// rejoin all open chats after reconnection (this is mandatory)
bool Connection::rejoinExistingChats()
{
    for (auto& chatid: mChatIds)
    {
        try
        {
            Chat& chat = mChatdClient.chats(chatid);
            if (!chat.isDisabled())
                chat.login();
        }
        catch(std::exception& e)
        {
            CHATDS_LOG_ERROR("rejoinExistingChats: Exception: %s", e.what());
            return false;
        }
    }
    return true;
}

// send JOIN
void Chat::join()
{
    //We don't have any local history, otherwise joinRangeHist() would be called instead of this
    //Reset handshake state, as we may be reconnecting
    mServerFetchState = kHistNotFetching;
    sendCommand(Command(OP_JOIN) + mChatId + mChatdClient.mMyHandle + (int8_t)PRIV_NOCHANGE);
    requestHistoryFromServer(-initialHistoryFetchCount);
}

void Chat::onJoinRejected()
{
    CHATID_LOG_WARNING("JOIN was rejected, setting chat offline and disabling it");
    mServerFetchState = kHistNotFetching;
    setOnlineState(kChatStateOffline);
    disable(true);
}

void Chat::onDisconnect()
{
    assert(mFetchRequest.size() <= 2);  // no more than a HIST+NODEHIST in parallel are allowed

    while (mFetchRequest.size())
    {
        FetchType fetchType = mFetchRequest.front();
        mFetchRequest.pop();
        if (fetchType == FetchType::kFetchMessages)
        {
            if (mServerOldHistCbEnabled && (mServerFetchState & kHistFetchingOldFromServer))
            {
                //app has been receiving old history from server, but we are now
                //about to receive new history (if any), so notify app about end of
                //old history
                CALL_LISTENER(onHistoryDone, kHistSourceServer);
            }
        }
        else if (fetchType == kFetchNodeHistory)
        {
            mAttachNodesReceived = 0;
            mAttachNodesRequestedToServer = 0;
            mAttachmentNodes->finishFetchingFromServer();
        }
    }

    mServerFetchState = kHistNotFetching;
    setOnlineState(kChatStateOffline);
}

HistSource Chat::getHistory(unsigned count)
{
    if (isNotifyingOldHistFromServer())
    {
        return kHistSourceServer;
    }
    if ((mNextHistFetchIdx == CHATD_IDX_INVALID) && !empty())
    {
        //start from newest message and go backwards
        mNextHistFetchIdx = highnum();
    }

    Idx countSoFar = 0;
    if (mNextHistFetchIdx != CHATD_IDX_INVALID)
    {
        assert(mNextHistFetchIdx <= highnum());
        auto end = lownum()-1;
        if (mNextHistFetchIdx > end) //we are in the RAM range
        {
            CHATID_LOG_DEBUG("Fetching history(%u) from RAM...", count);
            Idx fetchEnd = mNextHistFetchIdx - count;
            if (fetchEnd < end)
            {
                fetchEnd = end;
            }

            for (Idx i = mNextHistFetchIdx; i > fetchEnd; i--)
            {
                auto& msg = at(i);
                if (msg.isPendingToDecrypt())
                {
                    CHATID_LOG_WARNING("Skipping the load of a message still encrypted. "
                                       "msgid: %s idx: %d", ID_CSTR(msg.id()), i);
                    break;
                }

                CALL_LISTENER(onRecvHistoryMessage, i, msg, getMsgStatus(msg, i), true);
            }
            countSoFar = mNextHistFetchIdx - fetchEnd;
            mNextHistFetchIdx -= countSoFar;
            if (countSoFar >= (int)count)
            {
                CALL_LISTENER(onHistoryDone, kHistSourceRam);
                return kHistSourceRam;
            }
        }
    }

    // more than what is available in RAM is requested
    auto nextSource = getHistoryFromDbOrServer(count - countSoFar);
    if (nextSource == kHistSourceNone) //no history in db and server
    {
        auto source = (countSoFar > 0) ? kHistSourceRam : kHistSourceNone;
        CALL_LISTENER(onHistoryDone, source);
        return source;
    }
    if (nextSource == kHistSourceDb || nextSource == kHistSourceNotLoggedIn)
    {
        CALL_LISTENER(onHistoryDone, nextSource);
    }
    return nextSource;
}

HistSource Chat::getNodeHistory(uint32_t count)
{
    return mAttachmentNodes->getHistory(count);
}

HistSource Chat::getHistoryFromDbOrServer(unsigned count)
{
    if (mHasMoreHistoryInDb)
    {
        CHATID_LOG_DEBUG("Fetching history(%u) from db...", count);
        getHistoryFromDb(count);
        return kHistSourceDb;
    }
    else //have to fetch history from server
    {
        if (mHaveAllHistory)
        {
            CHATID_LOG_DEBUG("getHistoryFromDbOrServer: No more history exists");
            return kHistSourceNone;
        }
        if (mServerFetchState & kHistOldFlag)
        {
            CHATID_LOG_DEBUG("getHistoryFromDbOrServer: Need more history, and server history fetch is already in progress, will get next messages from there");
        }
        else
        {
            if (!isLoggedIn())
                return kHistSourceNotLoggedIn;

            auto wptr = weakHandle();
            marshallCall([wptr, this, count]()
            {
                if (wptr.deleted())
                    return;

                CHATID_LOG_DEBUG("Fetching history (%u messages) from server...", count);
                requestHistoryFromServer(-count);
            }, mChatdClient.mKarereClient->appCtx);
        }
        mServerOldHistCbEnabled = true;
        return kHistSourceServer;
    }
}

void Chat::requestHistoryFromServer(int32_t count)
{
    // the connection must be established, but might not be logged in yet (for a JOIN + HIST)
    assert(mConnection.isOnline());
    mLastServerHistFetchCount = mLastHistDecryptCount = 0;
    mServerFetchState = (count > 0)
        ? kHistFetchingNewFromServer
        : kHistFetchingOldFromServer;

    mFetchRequest.push(FetchType::kFetchMessages);
    sendCommand(Command(OP_HIST) + mChatId + count);
}

void Chat::requestNodeHistoryFromServer(Id oldestMsgid, uint32_t count)
{
    // avoid to access websockets from app's thread --> marshall the request
    auto wptr = weakHandle();
    marshallCall([wptr, this, oldestMsgid, count]()
    {
        if (wptr.deleted())
            return;

        if (!isLoggedIn())
        {
            mAttachmentNodes->finishFetchingFromServer();
            return;
        }

        CHATID_LOG_DEBUG("Fetching node history (%u messages) from server...", count);

        mFetchRequest.push(FetchType::kFetchNodeHistory);
        mAttachNodesRequestedToServer = count;
        assert(mAttachNodesReceived == 0);
        mAttachmentHistDoneReceived = false;
        sendCommand(Command(OP_NODEHIST) + mChatId + oldestMsgid + -count);
    }, mChatdClient.mKarereClient->appCtx);
}

Message *Chat::oldest() const
{
    if (!mBackwardList.empty())
    {
        return mBackwardList.back().get();
    }

    if (!mForwardList.empty())
    {
        return mForwardList.front().get();
    }

    return NULL;
}

Message *Chat::newest() const
{
    if (!mForwardList.empty())
    {
        return mForwardList.back().get();
    }

    if (!mBackwardList.empty())
    {
        return mBackwardList.front().get();
    }

    return NULL;
}

Chat::Chat(Connection& conn, Id chatid, Listener* listener,
    const karere::SetOfIds& initialUsers, uint32_t chatCreationTs,
    ICrypto* crypto, bool isGroup)
    : mChatdClient(conn.mChatdClient), mConnection(conn), mChatId(chatid),
      mListener(listener), mUsers(initialUsers), mCrypto(crypto),
      mLastMsgTs(chatCreationTs), mIsGroup(isGroup)
{
    assert(mChatId);
    assert(mListener);
    assert(mCrypto);
    assert(!mUsers.empty());
    mNextUnsent = mSending.begin();
    //we don't use CALL_LISTENER here because if init() throws, then something is wrong and we should not continue
    mListener->init(*this, mDbInterface);
    CALL_CRYPTO(setUsers, &mUsers);
    assert(mDbInterface);
    initChat();
    mAttachmentNodes = std::unique_ptr<FilteredHistory>(new FilteredHistory(*mDbInterface, *this));
    ChatDbInfo info;
    mDbInterface->getHistoryInfo(info);
    mOldestKnownMsgId = info.oldestDbId;
    mLastSeenId = info.lastSeenId;
    mLastReceivedId = info.lastRecvId;
    mLastSeenIdx = mDbInterface->getIdxOfMsgidFromHistory(mLastSeenId);
    mLastReceivedIdx = mDbInterface->getIdxOfMsgidFromHistory(mLastReceivedId);

    if ((mHaveAllHistory = mDbInterface->haveAllHistory()))
    {
        CHATID_LOG_DEBUG("All backward history of chat is available locally");
        mAttachmentNodes->setHaveAllHistory(true);
    }

    if (!mOldestKnownMsgId)
    {
        //no history in db
        mHasMoreHistoryInDb = false;
        mForwardStart = CHATD_IDX_RANGE_MIDDLE;
        CHATID_LOG_DEBUG("Db has no local history for chat");
        loadAndProcessUnsent();
    }
    else
    {
        assert(info.newestDbIdx != CHATD_IDX_INVALID);
        mHasMoreHistoryInDb = true;
        mForwardStart = info.newestDbIdx + 1;
        CHATID_LOG_DEBUG("Db has local history: %s - %s (middle point: %u)",
            ID_CSTR(info.oldestDbId), ID_CSTR(info.newestDbId), mForwardStart);
        loadAndProcessUnsent();
        getHistoryFromDb(initialHistoryFetchCount); // ensure we have a minimum set of messages loaded and ready
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

Idx Chat::getHistoryFromDb(unsigned count)
{
    assert(mHasMoreHistoryInDb); //we are within the db range
    std::vector<Message*> messages;
    CALL_DB(fetchDbHistory, lownum()-1, count, messages);
    for (auto msg: messages)
    {
        msgIncoming(false, msg, true); //increments mLastHistFetch/DecryptCount, may reset mHasMoreHistoryInDb if this msgid == mLastKnownMsgid
    }
    if (mNextHistFetchIdx == CHATD_IDX_INVALID)
    {
        mNextHistFetchIdx = mForwardStart - 1 - messages.size();
    }
    else
    {
        mNextHistFetchIdx -= messages.size();
    }
    CALL_LISTENER(onHistoryDone, kHistSourceDb);

    // If we haven't yet seen the message with the last-seen msgid, then all messages
    // in the buffer (and in the loaded range) are unseen - so we just loaded
    // more unseen messages
    if ((messages.size() < count) && mHasMoreHistoryInDb)
        throw std::runtime_error(mChatId.toString()+": Db says it has no more messages, but we still haven't seen mOldestKnownMsgId of "+std::to_string((int64_t)mOldestKnownMsgId.val));
    return (Idx)messages.size();
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

void Connection::wsHandleMsgCb(char *data, size_t len)
{
    mTsLastRecv = time(NULL);
    execCommand(StaticBuffer(data, len));
}

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
      Id chatid;
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
                CHATDS_LOG_DEBUG("recv KEEPALIVE");
                sendKeepalive(mChatdClient.mKeepaliveType);
                break;
            }
            case OP_BROADCAST:
            {
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_8(bcastType, 16);
                auto& chat = mChatdClient.chats(chatid);
                chat.handleBroadcast(userid, bcastType);
                break;
            }
            case OP_JOIN:
            {
                READ_CHATID(0);
                READ_ID(userid, 8);
                Priv priv = (Priv)buf.read<int8_t>(pos);
                pos++;
                CHATDS_LOG_DEBUG("%s: recv JOIN - user '%s' with privilege level %d",
                                ID_CSTR(chatid), ID_CSTR(userid), priv);

                if (userid == Id::COMMANDER())
                {
                    CHATDS_LOG_ERROR("recv JOIN for API user");
                    assert(false);
                    break;
                }

                auto& chat =  mChatdClient.chats(chatid);
                if (priv == PRIV_NOTPRESENT)
                    chat.onUserLeave(userid);
                else
                    chat.onUserJoin(userid, priv);
                break;
            }
            case OP_OLDMSG:
            case OP_NEWMSG:
            case OP_MSGUPD:
            {
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                READ_32(ts, 24);
                READ_16(updated, 28);
                READ_32(keyid, 30);
                READ_32(msglen, 34);
                const char* msgdata = buf.readPtr(pos, msglen);
                pos += msglen;

                CHATDS_LOG_DEBUG("%s: recv %s - msgid: '%s', from user '%s' with keyid %u, ts %u, tsdelta %u",
                    ID_CSTR(chatid), Command::opcodeToStr(opcode), ID_CSTR(msgid),
                    ID_CSTR(userid), keyid, ts, updated);

                std::unique_ptr<Message> msg(new Message(msgid, userid, ts, updated, msgdata, msglen, false, keyid));
                msg->setEncrypted(Message::kEncryptedPending);
                Chat& chat = mChatdClient.chats(chatid);
                if (opcode == OP_MSGUPD)
                {
                    chat.onMsgUpdated(msg.release());
                }
                else
                {
                    if (!chat.isFetchingNodeHistory() || opcode == OP_NEWMSG)
                    {
                        chat.msgIncoming((opcode == OP_NEWMSG), msg.release(), false);
                    }
                    else
                    {
                        chat.msgNodeHistIncoming(msg.release());
                    }
                }
                break;
            }
            case OP_SEEN:
            {
                READ_CHATID(0);
                READ_ID(msgid, 8);
                CHATDS_LOG_DEBUG("%s: recv SEEN - msgid: '%s'", ID_CSTR(chatid), ID_CSTR(msgid));
                mChatdClient.chats(chatid).onLastSeen(msgid);
                break;
            }
            case OP_RECEIVED:
            {
                READ_CHATID(0);
                READ_ID(msgid, 8);
                CHATDS_LOG_DEBUG("%s: recv RECEIVED - msgid: '%s'", ID_CSTR(chatid), ID_CSTR(msgid));
                mChatdClient.chats(chatid).onLastReceived(msgid);
                break;
            }
            case OP_RETENTION:
            {
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_32(period, 16);
                CHATDS_LOG_DEBUG("%s: recv RETENTION by user '%s' to %u second(s)",
                                ID_CSTR(chatid), ID_CSTR(userid), period);
                break;
            }
            case OP_MSGID:
            {
                READ_ID(msgxid, 0);
                READ_ID(msgid, 8);
                CHATDS_LOG_DEBUG("recv MSGID: '%s' -> '%s'", ID_CSTR(msgxid), ID_CSTR(msgid));
                mChatdClient.onMsgAlreadySent(msgxid, msgid);
                break;
            }
            case OP_NEWMSGID:
            {
                READ_ID(msgxid, 0);
                READ_ID(msgid, 8);
                CHATDS_LOG_DEBUG("recv NEWMSGID: '%s' -> '%s'", ID_CSTR(msgxid), ID_CSTR(msgid));
                mChatdClient.msgConfirm(msgxid, msgid);
                break;
            }
/*            case OP_RANGE:
            {
                READ_CHATID(0);
                READ_ID(oldest, 8);
                READ_ID(newest, 16);
                CHATDS_LOG_DEBUG("%s: recv RANGE - (%s - %s)",
                                ID_CSTR(chatid), ID_CSTR(oldest), ID_CSTR(newest));
                auto& msgs = mClient.chats(chatid);
                if (msgs.onlineState() == kChatStateJoining)
                    msgs.initialFetchHistory(newest);
                break;
            }
*/
            case OP_REJECT:
            {
                READ_CHATID(0);
                READ_ID(id, 8);
                READ_8(op, 16);
                READ_8(reason, 17);
                CHATDS_LOG_WARNING("%s: recv REJECT of %s: id='%s', reason: %hu",
                    ID_CSTR(chatid), Command::opcodeToStr(op), ID_CSTR(id), reason);
                auto& chat = mChatdClient.chats(chatid);
                if (op == OP_NEWMSG || op == OP_NEWNODEMSG) // the message was rejected
                {
                    chat.msgConfirm(id, Id::null());
                }
                else if ((op == OP_MSGUPD) || (op == OP_MSGUPDX))
                {
                    chat.rejectMsgupd(id, reason);
                }
                else if (op == OP_JOIN)
                {
                    chat.onJoinRejected();
                }
                else if (op == OP_RANGE && reason == 1)
                {
                    chat.clearHistory();
                    // we were notifying NEWMSGs in result of JOINRANGEHIST, but after reload we start receiving OLDMSGs
                    chat.mServerOldHistCbEnabled = mChatdClient.mKarereClient->isChatRoomOpened(chatid);
                    chat.getHistoryFromDbOrServer(chat.initialHistoryFetchCount);
                }
                else if (op == OP_NEWKEY)
                {
                    chat.onKeyReject();
                }
                else if (op == OP_HIST)
                {
                    chat.onHistReject();
                }
                else
                {
                    chat.rejectGeneric(op, reason);
                }
                break;
            }
            case OP_HISTDONE:
            {
                READ_CHATID(0);
                CHATDS_LOG_DEBUG("%s: recv HISTDONE - history retrieval finished", ID_CSTR(chatid));
                Chat &chat = mChatdClient.chats(chatid);
                chat.onHistDone();
                break;
            }
            case OP_NEWKEYID:
            {
                READ_CHATID(0);
                READ_32(keyxid, 8);
                READ_32(keyid, 12);
                CHATDS_LOG_DEBUG("%s: recv NEWKEYID: %u -> %u", ID_CSTR(chatid), keyxid, keyid);
                mChatdClient.chats(chatid).keyConfirm(keyxid, keyid);
                break;
            }
            case OP_NEWKEY:
            {
                READ_CHATID(0);
                READ_32(keyid, 8);
                READ_32(totalLen, 12);
                const char* keys = buf.readPtr(pos, totalLen);
                pos+=totalLen;
                CHATDS_LOG_DEBUG("%s: recv NEWKEY %u", ID_CSTR(chatid), keyid);
                mChatdClient.chats(chatid).onNewKeys(StaticBuffer(keys, totalLen));
                break;
            }
            case OP_INCALL:
            {
                // opcode.1 chatid.8 userid.8 clientid.4
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_32(clientid, 16);
                CHATDS_LOG_DEBUG("%s: recv INCALL userid %s, clientid: %x", ID_CSTR(chatid), ID_CSTR(userid), clientid);
                auto& chat = mChatdClient.chats(chatid);
                // TODO: remove this block once the groucalls are fully supported by clients
                if ((chat.isGroup() && !mChatdClient.mKarereClient->areGroupCallsEnabled()))
                {
                    CHATDS_LOG_DEBUG("Groupcalls are disabled, ignoring INCALL command");
                    break;
                }
                chat.onInCall(userid, clientid);
                break;
            }
            case OP_ENDCALL:
            {
                // opcode.1 chatid.8 userid.8 clientid.4
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_32(clientid, 16);
                CHATDS_LOG_DEBUG("%s: recv ENDCALL userid: %s, clientid: %x", ID_CSTR(chatid), ID_CSTR(userid), clientid);
                auto& chat = mChatdClient.chats(chatid);
                // TODO: remove this block once the groucalls are fully supported by clients
                if ((chat.isGroup() && !mChatdClient.mKarereClient->areGroupCallsEnabled()))
                {
                    CHATDS_LOG_DEBUG("Groupcalls are disabled, ignoring ENDCALL command");
                    break;
                }
                chat.onEndCall(userid, clientid);
                break;
            }
            case OP_CALLDATA:
            {
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_32(clientid, 16);
                READ_16(payloadLen, 20);
                CHATDS_LOG_DEBUG("%s: recv CALLDATA userid: %s, clientid: %x, PayloadLen: %d", ID_CSTR(chatid), ID_CSTR(userid), clientid, payloadLen);
                pos += payloadLen; // payload bytes will be consumed by handleCallData(), but does not update `pos` pointer

#ifndef KARERE_DISABLE_WEBRTC
                if (mChatdClient.mRtcHandler && userid != mChatdClient.mKarereClient->myHandle())
                {
                    StaticBuffer cmd(buf.buf() + 23, payloadLen);
                    auto& chat = mChatdClient.chats(chatid);                    
                    // TODO: remove this block once the groucalls are fully supported by clients
                    if ((chat.isGroup() && !mChatdClient.mKarereClient->areGroupCallsEnabled()))
                    {
                        CHATDS_LOG_DEBUG("Groupcalls are disabled, ignoring CALLDATA command");
                        break;
                    }
                    mChatdClient.mRtcHandler->handleCallData(chat, chatid, userid, clientid, cmd);
                }
#else
                READ_ID(callid, 22);
                READ_8(state, 30);
                if (state == rtcModule::kCallDataRinging) // Ringing state
                {
                    sendCallReqDeclineNoSupport(chatid, callid);
                }

                pos += payloadLen - 9;  // 9 -> callid(8) + state(1)
#endif

                break;
            }
            case OP_RTMSG_ENDPOINT:
            case OP_RTMSG_USER:
            case OP_RTMSG_BROADCAST:
            {
                //opcode.1 chatid.8 userid.8 clientid.4 len.2 data.len
                READ_CHATID(0);
                size_t cmdstart = pos - 9; //pos points after opcode
                (void)cmdstart; //disable unused var warning if webrtc is disabled
                READ_ID(userid, 8);
                READ_32(clientid, 16);
                (void)clientid; //disable unused var warning if webrtc is enabled
                READ_16(payloadLen, 20);
                pos += payloadLen; //skip the payload
#ifndef KARERE_DISABLE_WEBRTC
                auto& chat = mChatdClient.chats(chatid);
                StaticBuffer cmd(buf.buf() + cmdstart, 23 + payloadLen);
                CHATDS_LOG_DEBUG("%s: recv %s", ID_CSTR(chatid), ::rtcModule::rtmsgCommandToString(cmd).c_str());
                if (mChatdClient.mRtcHandler)
                {
                    mChatdClient.mRtcHandler->handleMessage(chat, cmd);
                }
#else
                CHATDS_LOG_DEBUG("%s: recv %s userid: %s, clientid: %x", ID_CSTR(chatid), Command::opcodeToStr(opcode), ID_CSTR(userid), clientid);
#endif
                break;
            }
            case OP_CLIENTID:
            {
                // clientid.4 reserved.4
                READ_32(clientid, 0);
                mClientId = clientid;
                CHATDS_LOG_DEBUG("recv CLIENTID - %x", clientid);
                break;
            }
            case OP_ECHO:
            {
                CHATDS_LOG_DEBUG("recv ECHO");
                if (mEchoTimer)
                {
                    CHATDS_LOG_DEBUG("Socket is still alive");
                    cancelTimeout(mEchoTimer, mChatdClient.mKarereClient->appCtx);
                    mEchoTimer = 0;
                }
                break;
            }
            case OP_ADDREACTION:
            {
                //TODO: to be implemented
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                READ_32(reaction, 24);
                CHATDS_LOG_DEBUG("%s: recv ADDREACTION from user %s to message %s reaction %d",
                                ID_CSTR(chatid), ID_CSTR(userid), ID_CSTR(msgid), reaction);
                break;
            }
            case OP_DELREACTION:
            {
                //TODO: to be implemented
                READ_CHATID(0);
                READ_ID(userid, 8);
                READ_ID(msgid, 16);
                READ_32(reaction, 24);
                CHATDS_LOG_DEBUG("%s: recv DELREACTION from user %s to message %s reaction %d",
                                ID_CSTR(chatid), ID_CSTR(userid), ID_CSTR(msgid), reaction);
                break;
            }
            case OP_SYNC:
            {
                READ_CHATID(0);
                CHATDS_LOG_DEBUG("%s: recv SYNC", ID_CSTR(chatid));
                mChatdClient.mKarereClient->onSyncReceived(chatid);
                break;
            }
            case OP_CALLTIME:
            {
                READ_CHATID(0);
                READ_32(duration, 8);
                CHATDS_LOG_DEBUG("%s: recv CALLTIME: %d", ID_CSTR(chatid), duration);
#ifndef KARERE_DISABLE_WEBRTC
                if (mChatdClient.mRtcHandler)
                {
                    auto& chat = mChatdClient.chats(chatid);
                    if (!chat.isGroup() || (chat.isGroup() && mChatdClient.mKarereClient->areGroupCallsEnabled()))
                    {
                        mChatdClient.mRtcHandler->handleCallTime(chatid, duration);
                    }
                    else
                    {
                        CHATDS_LOG_DEBUG("Skip command");
                    }
                }
#endif
                break;
            }
            default:
            {
                CHATDS_LOG_ERROR("Unknown opcode %d, ignoring all subsequent commands", opcode);
                return;
            }
        }
      }
      catch(BufferRangeError& e)
      {
            CHATDS_LOG_ERROR("%s: Buffer bound check error while parsing %s:\n\t%s\n\tAborting command processing", ID_CSTR(chatid), Command::opcodeToStr(opcode), e.what());
            return;
      }
      catch(std::exception& e)
      {
            CHATDS_LOG_ERROR("%s: Exception while processing incoming %s: %s", ID_CSTR(chatid), Command::opcodeToStr(opcode), e.what());
      }
    }
}

void Chat::onNewKeys(StaticBuffer&& keybuf)
{
    size_t pos = 0;
    size_t size = keybuf.dataSize();
    while ((pos + 14) < size)
    {
        Id userid(keybuf.read<uint64_t>(pos));          pos += 8;
        KeyId keyid = keybuf.read<KeyId>(pos);          pos += 4;
        uint16_t keylen = keybuf.read<uint16_t>(pos);   pos += 2;
        const char *key = keybuf.readPtr(pos, keylen);  pos += keylen;

        CHATID_LOG_DEBUG("sending key %d for user %s with length %zu to crypto module",
                         keyid, userid.toString().c_str(), keybuf.dataSize());
        CALL_CRYPTO(onKeyReceived, keyid, userid, mChatdClient.myHandle(), key, keylen);
    }

    if (pos != size)
    {
        CHATID_LOG_ERROR("onNewKeys: unexpected size of received NEWKEY");
        assert(false);
    }
}

void Chat::onHistDone()
{
    FetchType fetchType = mFetchRequest.front();
    mFetchRequest.pop();
    if (fetchType == FetchType::kFetchMessages)
    {
        // We may be fetching from memory and db because of a resetHistFetch()
        // while fetching from server. In that case, we don't notify about
        // fetched messages and onHistDone()

        if (isFetchingFromServer()) //HISTDONE is received for new history or after JOINRANGEHIST
        {
            onFetchHistDone();
        }
        if (isJoining())
        {
            onJoinComplete();
        }
    }
    else if (fetchType == FetchType::kFetchNodeHistory)
    {
        if (!mDecryptionAttachmentsHalted)
        {
            attachmentHistDone();
        }

        mAttachmentHistDoneReceived = true;
    }
    else
    {
        CHATID_LOG_WARNING("onHistDone: unknown type of fetch");
        assert(false);
    }
}

void Chat::onFetchHistDone()
{
    assert(isFetchingFromServer());

    //resetHistFetch() may have been called while fetching from server,
    //so state may be fetching-from-ram or fetching-from-db
    bool fetchingOld = (mServerFetchState & kHistOldFlag);
    if (fetchingOld)
    {
        if (mDecryptOldHaltedAt != CHATD_IDX_INVALID)
        {
            mServerFetchState = kHistDecryptingOld;
        }
        else
        {
            mServerFetchState = kHistNotFetching;
            // if app tries to load messages before first join and there's no local history available yet,
            // they received a `HistSource == kSourceNotLoggedIn`. During login, received messages won't be
            // notified, but after login the app can attempt to load messages again and should be notified
            // about messages from the beginning
            if (!mIsFirstJoin)
            {
                mNextHistFetchIdx = lownum()-1;
            }
        }
        if (mLastServerHistFetchCount <= 0)
        {
            //server returned zero messages
            assert((mDecryptOldHaltedAt == CHATD_IDX_INVALID) && (mDecryptNewHaltedAt == CHATD_IDX_INVALID));
            mHaveAllHistory = true;
            mAttachmentNodes->setHaveAllHistory(true);
            CALL_DB(setHaveAllHistory, true);
            CHATID_LOG_DEBUG("Start of history reached");
            //last text msg stuff
            if (mLastTextMsg.isFetching())
            {
                mLastTextMsg.clear();
                notifyLastTextMsg();
            }
        }
    }
    else
    {
        mServerFetchState = (mDecryptNewHaltedAt != CHATD_IDX_INVALID)
            ? kHistDecryptingNew : kHistNotFetching;
    }

    if (mServerFetchState == kHistNotFetching) //if not still decrypting
    {
        if (fetchingOld && mServerOldHistCbEnabled)
        {
            //we are forwarding to the app the history we are receiving from
            //server. Tell app that is complete.
            CALL_LISTENER(onHistoryDone, kHistSourceServer);
        }
        if (mLastSeenIdx == CHATD_IDX_INVALID)
            CALL_LISTENER(onUnreadChanged);
    }

    // handle last text message fetching
    if (mLastTextMsg.isFetching())
    {
        assert(!mHaveAllHistory); //if we reach start of history, mLastTextMsg.state() will be set to kNone
        CHATID_LOG_DEBUG("No text message seen yet, fetching more history from server");
        getHistory(initialHistoryFetchCount);
    }
}

void Chat::loadAndProcessUnsent()
{
    assert(mSending.empty());
    CALL_DB(loadSendQueue, mSending);
    if (mSending.empty())
        return;

    mNextUnsent = mSending.begin();
    replayUnsentNotifications();

    //last text message stuff
    for (auto it = mSending.rbegin(); it!=mSending.rend(); it++)
    {
        if (it->msg->isValidLastMessage())
        {
            onLastTextMsgUpdated(*it->msg);
            return;
        }
    }
}

void Chat::resetListenerState()
{
    resetGetHistory();
    replayUnsentNotifications();
    loadManualSending();
}

void Chat::replayUnsentNotifications()
{
    for (auto it = mSending.begin(); it != mSending.end(); it++)
    {
        auto& item = *it;
        if (item.opcode() == OP_NEWMSG || item.opcode() == OP_NEWNODEMSG)
        {
            CALL_LISTENER(onUnsentMsgLoaded, *item.msg);
        }
        else if (item.opcode() == OP_MSGUPD)
        {
            CHATID_LOG_DEBUG("Adding a pending edit of msgid %s", ID_CSTR(item.msg->id()));
            mPendingEdits[item.msg->id()] = item.msg;
            CALL_LISTENER(onUnsentEditLoaded, *item.msg, false);
        }
        else if (item.opcode() == OP_MSGUPDX)
        {
            //in case of MSGUPDX, when msgModify posted it, it must have updated
            //the text of the original message with that msgxid in the send queue.
            //So we can technically do without the this
            //'else if (item.opcode == OP_MSGUPDX)' case. However, if we don't tell
            //the GUI there is an actual edit pending, (although it may be a dummy one,
            //because the pending NEWMSG in the send queue was updated with the new msg),
            //it will display a normal pending outgoing message without any sign
            //of an edit. Then, when it receives the MSGUPD confirmation, it will
            //suddenly flash an indicator that the message was edited, which may be
            //confusing to the user.
            CHATID_LOG_DEBUG("Adding a pending edit of msgxid %s", ID_CSTR(item.msg->id()));
            CALL_LISTENER(onUnsentEditLoaded, *item.msg, true);
        }
    }
}

void Chat::loadManualSending()
{
    std::vector<ManualSendItem> items;
    CALL_DB(loadManualSendItems, items);
    for (auto& item: items)
    {
        CALL_LISTENER(onManualSendRequired, item.msg, item.rowid, item.reason);
    }
}

Message* Chat::getManualSending(uint64_t rowid, ManualSendReason& reason)
{
    ManualSendItem item;
    CALL_DB(loadManualSendItem, rowid, item);
    reason = item.reason;
    return item.msg;
}

Idx Chat::lastIdxReceivedFromServer() const
{
    return mLastIdxReceivedFromServer;
}

Id Chat::lastIdReceivedFromServer() const
{
    return mLastIdReceivedFromServer;
}

bool Chat::isGroup() const
{
    return mIsGroup;
}

void Chat::clearHistory()
{
    initChat();
    CALL_DB(clearHistory);
    CALL_CRYPTO(onHistoryReload);
    CALL_LISTENER(onHistoryReloaded);
}

void Chat::sendSync()
{
    sendCommand(Command(OP_SYNC) + mChatId);
}

bool Chat::isFetchingNodeHistory() const
{
    return (!mFetchRequest.empty() && (mFetchRequest.front() == FetchType::kFetchNodeHistory));
}

void Chat::setNodeHistoryHandler(FilteredHistoryHandler *handler)
{
    mAttachmentNodes->setHandler(handler);
}

void Chat::unsetHandlerToNodeHistory()
{
    mAttachmentNodes->unsetHandler();
}

Message* Chat::getMsgByXid(Id msgxid)
{
    for (auto& item: mSending)
    {
        if (!item.msg)
            continue;
        //id() of MSGUPD messages is a real msgid, not a msgxid
        if ((item.msg->id() == msgxid) && (item.opcode() != OP_MSGUPD))
        {
            assert(item.msg->isSending());
            return item.msg;
        }
    }
    return nullptr;
}

bool Chat::haveAllHistoryNotified() const
{
    if (!mHaveAllHistory || mHasMoreHistoryInDb)
        return false;

    return (mNextHistFetchIdx < lownum());
}

Message *Chat::getMessageFromNodeHistory(Id msgid) const
{
    return mAttachmentNodes->getMessage(msgid);
}

Idx Chat::getIdxFromNodeHistory(Id msgid) const
{
    return mAttachmentNodes->getMessageIdx(msgid);
}

uint64_t Chat::generateRefId(const ICrypto* aCrypto)
{
    uint64_t ts = time(nullptr);
    uint64_t rand;
    aCrypto->randomBytes(&rand, sizeof(rand));
    return (ts & 0x0000000000ffffff) | (rand << 40);
}
void Chat::onInCall(karere::Id userid, uint32_t clientid)
{
#ifndef KARERE_DISABLE_WEBRTC
    assert(mChatdClient.mRtcHandler);
    if (mChatdClient.mRtcHandler)
    {
        mChatdClient.mRtcHandler->handleInCall(mChatId, userid, clientid);
    }
#endif
}

void Chat::onEndCall(karere::Id userid, uint32_t clientid)
{
#ifndef KARERE_DISABLE_WEBRTC
    assert(mChatdClient.mRtcHandler);
    if (mChatdClient.mRtcHandler)
    {
        mChatdClient.mRtcHandler->onClientLeftCall(mChatId, userid, clientid);
    }
#endif
}

void Chat::initChat()
{
    mBackwardList.clear();
    mForwardList.clear();
    mIdToIndexMap.clear();
    if (mAttachmentNodes)
    {
        mAttachmentNodes->clear();
    }

    mForwardStart = CHATD_IDX_RANGE_MIDDLE;

    mOldestKnownMsgId = 0;
    mLastSeenIdx = CHATD_IDX_INVALID;
    mLastReceivedIdx = CHATD_IDX_INVALID;
    mNextHistFetchIdx = CHATD_IDX_INVALID;
    mLastIdReceivedFromServer = 0;
    mLastIdxReceivedFromServer = CHATD_IDX_INVALID;
    mLastServerHistFetchCount = 0;
    mLastHistDecryptCount = 0;

    mLastTextMsg.clear();
    mEncryptionHalted = false;
    mDecryptNewHaltedAt = CHATD_IDX_INVALID;
    mDecryptOldHaltedAt = CHATD_IDX_INVALID;
    mRefidToIdxMap.clear();

    mHasMoreHistoryInDb = false;
    mHaveAllHistory = false;
}

void Chat::requestRichLink(Message &message)
{
    std::string text = message.toText();
    std::string url;
    if (Message::hasUrl(text, url))
    {
        std::regex expresion("^(http://|https://)(.+)");
        std::string linkRequest = url;
        if (!regex_match(url, expresion))
        {
            linkRequest = std::string("http://") + url;
        }

        auto wptr = weakHandle();
        karere::Id msgId = message.id();
        uint16_t updated = message.updated;
        client().mKarereClient->api.call(&::mega::MegaApi::requestRichPreview, linkRequest.c_str())
        .then([wptr, this, msgId, updated](ReqResult result)
        {
            if (wptr.deleted())
                return;

            const char *requestText = result->getText();
            if (!requestText || (strlen(requestText) == 0))
            {
                CHATID_LOG_ERROR("requestRichLink: API request succeed, but returned an empty metadata for: %s", result->getLink());
                return;
            }

            Idx messageIdx = msgIndexFromId(msgId);
            Message *msg = (messageIdx != CHATD_IDX_INVALID) ? findOrNull(messageIdx) : NULL;
            if (msg && updated == msg->updated)
            {
                std::string text = requestText;
                std::string originalMessage = msg->toText();
                std::string textMessage;
                textMessage.reserve(originalMessage.size());
                for (std::string::size_type i = 0; i < originalMessage.size(); i++)
                {
                    unsigned char character = originalMessage[i];
                    switch (character)
                    {
                        case '\n':
                            textMessage.push_back('\\');
                            textMessage.push_back('n');
                        break;
                        case '\r':
                            textMessage.push_back('\\');
                            textMessage.push_back('r');
                        break;
                        case '\t':
                            textMessage.push_back('\\');
                            textMessage.push_back('t');
                        break;
                        case '\"':
                        case '\\':
                            textMessage.push_back('\\');
                            textMessage.push_back(character);
                        break;
                        default:
                            if (character > 31 && character != 127) // control ASCII characters are removed
                            {
                                textMessage.push_back(character);
                            }
                        break;
                    }
                }

                std::string updateText = std::string("{\"textMessage\":\"") + textMessage + std::string("\",\"extra\":[");
                updateText = updateText + text + std::string("]}");

                rapidjson::StringStream stringStream(updateText.c_str());
                rapidjson::Document document;
                document.ParseStream(stringStream);

                if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
                {
                    API_LOG_ERROR("requestRichLink: Json is not valid");
                    return;
                }

                updateText.insert(updateText.begin(), Message::ContainsMetaSubType::kRichLink);
                updateText.insert(updateText.begin(), Message::kMsgContainsMeta - Message::kMsgOffset);
                updateText.insert(updateText.begin(), 0x0);
                std::string::size_type size = updateText.size();

                if (!msgModify(*msg, updateText.c_str(), size, NULL, Message::kMsgContainsMeta))
                {
                    CHATID_LOG_ERROR("requestRichLink: Message can't be updated with the rich-link (%s)", ID_CSTR(msgId));
                }
            }
            else if (!msg)
            {
                CHATID_LOG_WARNING("requestRichLink: Message not found (%s)", ID_CSTR(msgId));
            }
            else
            {
                CHATID_LOG_DEBUG("requestRichLink: Message has been updated during rich link request (%s)", ID_CSTR(msgId));
            }
        })
        .fail([wptr, this](const ::promise::Error& err)
        {
            if (wptr.deleted())
                return;

            CHATID_LOG_ERROR("Failed to request rich link: request error (%d)", err.code());
        });
    }
}

Message *Chat::removeRichLink(Message &message, const string& content)
{

    Message *msg = msgModify(message, content.c_str(), content.size(), NULL, Message::kMsgNormal);
    if (!msg)
    {
        CHATID_LOG_ERROR("requestRichLink: Message can't be updated with the rich-link (%s)", ID_CSTR(message.id()));
    }
    else
    {
        // prevent to create a new rich-link upon acknowledge of update at onMsgUpdated()
        msg->richLinkRemoved = true;
    }

    return msg;
}

void Chat::requestPendingRichLinks()
{
    for (std::set<karere::Id>::iterator it = mMsgsToUpdateWithRichLink.begin();
         it != mMsgsToUpdateWithRichLink.end();
         it++)
    {
        karere::Id msgid = *it;
        Idx index = msgIndexFromId(msgid);
        if (index != CHATD_IDX_INVALID)     // only confirmed messages have index
        {
            Message *msg = findOrNull(index);
            if (msg)
            {
                requestRichLink(*msg);
            }
            else
            {
                CHATID_LOG_DEBUG("Failed to find message by index, being index retrieved from message id (index: %d, id: %d)", index, msgid);
            }
        }
        else
        {
            CHATID_LOG_DEBUG("Failed to find message by id (id: %d)", msgid);
        }
    }

    mMsgsToUpdateWithRichLink.clear();
}

void Chat::removePendingRichLinks()
{
    mMsgsToUpdateWithRichLink.clear();
}

void Chat::removePendingRichLinks(Idx idx)
{
    for (std::set<karere::Id>::iterator it = mMsgsToUpdateWithRichLink.begin(); it != mMsgsToUpdateWithRichLink.end(); )
    {
        karere::Id msgid = *it;
        it++;
        Idx index = msgIndexFromId(msgid);
        assert(index != CHATD_IDX_INVALID);
        if (index <= idx)
        {
            mMsgsToUpdateWithRichLink.erase(msgid);
        }
    }
}

void Chat::manageRichLinkMessage(Message &message)
{
    std::string url;
    bool hasURL = Message::hasUrl(message.toText(), url);
    bool isMsgQueued = (mMsgsToUpdateWithRichLink.find(message.id()) != mMsgsToUpdateWithRichLink.end());

    if (!isMsgQueued && hasURL)
    {
        mMsgsToUpdateWithRichLink.insert(message.id());
    }
    else if (isMsgQueued && !hasURL)    // another edit removed the previous URL
    {
        mMsgsToUpdateWithRichLink.erase(message.id());
    }
}

void Chat::attachmentHistDone()
{
    assert(mAttachNodesRequestedToServer);
    if (mAttachNodesReceived < mAttachNodesRequestedToServer)
    {
        mAttachmentNodes->setHaveAllHistory(true);
    }

    mAttachNodesReceived = 0;
    mAttachNodesRequestedToServer = 0;
    mAttachmentNodes->finishFetchingFromServer();
}

Message* Chat::msgSubmit(const char* msg, size_t msglen, unsigned char type, void* userp)
{
    if (msglen > kMaxMsgSize)
    {
        CHATID_LOG_WARNING("msgSubmit: Denying sending message because it's too long");
        return NULL;
    }

    // write the new message to the message buffer and mark as in sending state
    auto message = new Message(makeRandomId(), client().myHandle(), time(NULL),
        0, msg, msglen, true, CHATD_KEYID_INVALID, type, userp);
    message->backRefId = generateRefId(mCrypto);

    auto wptr = weakHandle();
    SetOfIds recipients = mUsers;
    marshallCall([wptr, this, message, recipients]()
    {
        if (wptr.deleted())
            return;

        msgSubmit(message, recipients);

    }, mChatdClient.mKarereClient->appCtx);
    return message;
}
void Chat::msgSubmit(Message* msg, SetOfIds recipients)
{
    assert(msg->isSending());
    assert(msg->keyid == CHATD_KEYID_INVALID);

    // last text msg stuff
    if (msg->isValidLastMessage())
    {
        onLastTextMsgUpdated(*msg);
        onMsgTimestamp(msg->ts);
    }

    int opcode = (msg->type == Message::Type::kMsgAttachment) ? OP_NEWNODEMSG : OP_NEWMSG;
    postMsgToSending(opcode, msg, recipients);
}

void Chat::createMsgBackRefs(Chat::OutputQueue::iterator msgit)
{
#ifndef _MSC_VER
    static std::uniform_int_distribution<uint8_t>distrib(0, 0xff);
#else
//MSVC has a bug - no char template argument allowed
    static std::uniform_int_distribution<uint32_t>distrib(0,0xff);
#endif

    static std::random_device rd;

    // mSending is a list, so we don't have random access by index there.
    // Therefore, we copy the relevant part of it to a vector
    std::vector<SendingItem*> sendingIdx;
    sendingIdx.reserve(mSending.size());
    auto next = msgit;
    next++;
    for (auto it = mSending.begin(); it != next; it++)
    {
        sendingIdx.push_back(&(*it));
    }

    Idx maxEnd = size() - sendingIdx.size();
    if (maxEnd <= 0)
    {
        return;
    }

    // We include 7 backreferences, each in a different range of preceding messages
    // The exact message in these ranges is picked randomly
    // The ranges (as backward offsets from the current message's position) are:
    // 1<<0 - 1<<1, 1<<1 - 1<<2, 1<<2 - 1<<3, etc
    Idx rangeStart = 0;
    for (uint8_t i = 0; i < 7; i++)
    {
        Idx rangeEnd = 1 << i;
        if (rangeEnd > maxEnd)
        {
            rangeEnd = maxEnd;
        }

        //backward offset range is [start - end)
        Idx span = (rangeEnd - rangeStart);
        assert(span >= 0);

        // The actual offset of the picked target backreferenced message
        // It is zero-based: idx of 0 means the message preceding the one for which we are creating backrefs.
        Idx idx;
        if (span > 1)
        {
            idx = rangeStart + (distrib(rd) % span);
        }
        else
        {
            idx = rangeStart;
        }

        Message &msg = (idx < (Idx)sendingIdx.size())
                ? *(sendingIdx[sendingIdx.size()-1-idx]->msg)   // msg is from sending queue
                : at(highnum()-(idx-sendingIdx.size()));        // msg is from history buffer

        if (!msg.isManagementMessage()) // management-msgs don't have a valid backrefid
        {
            msgit->msg->backRefs.push_back(msg.backRefId);
        }
        else
        {
            CHATID_LOG_WARNING("Skipping backrefid for a management message: %s", ID_CSTR(msg.id()));
            // TODO: instead of skipping the backrefid for this range, we should try to find another
            // message with a valid backrefid within the current range
        }

        if (rangeEnd == maxEnd)
        {
            return;
        }

        rangeStart = rangeEnd;
    }
}

Chat::SendingItem* Chat::postMsgToSending(uint8_t opcode, Message* msg, SetOfIds recipients)
{
    // for NEWMSG/NEWNODEMSG, recipients is always current set of participants
    // for MSGXUPD, recipients must always be the same participants than in the pending NEWMSG (and MSGUPDX, if any)
    // for MSGUPD, recipients is not used (the keyid is already confirmed)
    assert(((opcode == OP_NEWMSG || opcode == OP_NEWNODEMSG ) && recipients == mUsers)
           || (opcode == OP_MSGUPDX)    // can use unconfirmed or confirmed key
           || (opcode == OP_MSGUPD && !isLocalKeyId(msg->keyid)));

    mSending.emplace_back(opcode, msg, recipients);
    CALL_DB(addSendingItem, mSending.back());
    if (mNextUnsent == mSending.end())
    {
        mNextUnsent--;
    }
    flushOutputQueue();
    return &mSending.back();
}

bool Chat::sendKeyAndMessage(std::pair<MsgCommand*, KeyCommand*> cmd)
{
    assert(cmd.first);
    if (cmd.second) // if NEWKEY is required for this NEWMSG...
    {
        if (!sendCommand(*cmd.second))
            return false;
    }
    return sendCommand(*cmd.first);
}

bool Chat::msgEncryptAndSend(OutputQueue::iterator it)
{
    if (it->msgCmd)
    {
        sendKeyAndMessage(std::make_pair(it->msgCmd, it->keyCmd));
        return true;
    }

    Message* msg = it->msg;
    uint64_t rowid = it->rowid;
    assert(msg->id());

    //opcode can be NEWMSG, NEWNODEMSG, MSGUPD or MSGUPDX
    if ((it->opcode() == OP_NEWMSG || it->opcode() == OP_NEWNODEMSG) && msg->backRefs.empty())
    {
        createMsgBackRefs(it);  // only for new messages
    }

    if (mEncryptionHalted)
        return false;

    auto msgCmd = new MsgCommand(it->opcode(), mChatId, client().myHandle(),
         msg->id(), msg->ts, msg->updated);

    CHATD_LOG_CRYPTO_CALL("Calling ICrypto::encrypt()");
    auto pms = mCrypto->msgEncrypt(msg, it->recipients, msgCmd);
    // if using current keyid or original keyid from msg, promise is resolved immediately
    if (pms.succeeded())
    {
        MsgCommand *msgCmd = pms.value().first;
        KeyCommand *keyCmd = pms.value().second;
        assert(!keyCmd                                              // no newkey required...
               || (keyCmd && keyCmd->localKeyid() == msg->keyid     // ... or localkeyid is assigned to message
                   && msgCmd->keyId() == CHATD_KEYID_UNCONFIRMED)); // and msgCmd's keyid is unconfirmed

        it->msgCmd = pms.value().first;
        it->keyCmd = pms.value().second;
        CALL_DB(addBlobsToSendingItem, rowid, it->msgCmd, it->keyCmd, msg->keyid);

        sendKeyAndMessage(pms.value());
        return true;
    }
    // else --> new key is required: KeyCommand != NULL in pms.value()

    mEncryptionHalted = true;
    CHATID_LOG_DEBUG("Can't encrypt message immediately, halting output");

    pms.then([this, msg, rowid](std::pair<MsgCommand*, KeyCommand*> result)
    {
        assert(mEncryptionHalted);
        assert(!mSending.empty());

        MsgCommand *msgCmd = result.first;
        KeyCommand *keyCmd = result.second;
        assert(keyCmd);
        assert(keyCmd->localKeyid() == msg->keyid);
        assert(msgCmd->keyId() == CHATD_KEYID_UNCONFIRMED);

        SendingItem &item = mSending.front();
        item.msgCmd = msgCmd;
        item.keyCmd = keyCmd;
        CALL_DB(addBlobsToSendingItem, rowid, item.msgCmd, item.keyCmd, msg->keyid);

        sendKeyAndMessage(result);
        mEncryptionHalted = false;
        flushOutputQueue();
    });

    pms.fail([this, msg, msgCmd](const ::promise::Error& err)
    {
        CHATID_LOG_ERROR("ICrypto::encrypt error encrypting message %s: %s", ID_CSTR(msg->id()), err.what());
        delete msgCmd;
        return err;
    });

    return false;
    //we don't sent a msgStatusChange event to the listener, as the GUI should initialize the
    //message's status with something already, so it's redundant.
    //The GUI should by default show it as sending
}

// Can be called for a message in history or a NEWMSG,MSGUPD,MSGUPDX message in sending queue
Message* Chat::msgModify(Message& msg, const char* newdata, size_t newlen, void* userp, uint8_t newtype)
{
    uint32_t now = time(NULL);
    uint32_t age = now - msg.ts;
    if (!msg.isSending() && age > CHATD_MAX_EDIT_AGE)
    {
        CHATID_LOG_DEBUG("msgModify: Denying edit of msgid %s because message is too old", ID_CSTR(msg.id()));
        return nullptr;
    }
    if (newlen > kMaxMsgSize)
    {
        CHATID_LOG_WARNING("msgModify: Denying edit of msgid %s because message is too long", ID_CSTR(msg.id()));
        return nullptr;
    }

    SetOfIds recipients;    // empty for already confirmed messages, since they already have a keyid
    if (msg.isSending())
    {
        // recipients must be the same from original message/s
        // content of original message/s should be updated with the new content
        // delta of original message/s should be updated to the current timestamp
        // Note that there could be more than one item in the sending queue
        // referencing to the same message that wants to be edited (more than one
        // unconfirmed edit, for both confirmed or unconfirmed messages)

        // find the most-recent item in the queue, which has the most recent timestamp
        SendingItem* item = nullptr;
        for (list<SendingItem>::reverse_iterator loopItem = mSending.rbegin();
             loopItem != mSending.rend(); loopItem++)
        {
            if (loopItem->msg->id() == msg.id())
            {
                item = &(*loopItem);
                break;
            }
        }
        assert(item);

        // avoid same "delta" for different edits
        switch (item->opcode())
        {
            case OP_NEWMSG:
            case OP_NEWNODEMSG:
                if (age == 0)
                {
                    age++;
                }
            break;

            case OP_MSGUPD:
            case OP_MSGUPDX:
                if (item->msg->updated == age)
                {
                    age++;
                }
                break;

            default:
                CHATID_LOG_ERROR("msgModify: unexpected opcode for the msgid %s in the sending queue", ID_CSTR(msg.id()));
                return nullptr;
        }

        // update original content+delta of the message being edited...
        msg.updated = age;
        msg.assign((void*)newdata, newlen);
        // ...and also for all messages with same msgid in the sending queue , trying to avoid sending the original content
        int count = 0;
        for (auto& it: mSending)
        {
            SendingItem &item = it;
            if (item.msg->id() == msg.id())
            {
                item.msg->updated = age;
                item.msg->assign((void*)newdata, newlen);
                count++;
            }
        }
        assert(count);  // an edit of a message in sending always indicates the former message is in the queue
        if (count)
        {
            int countDb = mDbInterface->updateSendingItemsContentAndDelta(msg);
            assert(countDb == count);
            CHATID_LOG_DEBUG("msgModify: updated the content and delta of %d message/s in the sending queue", count);
        }

        // recipients must not change
        recipients = SetOfIds(item->recipients);
    }
    else if (age == 0)  // in the very unlikely case the msg is already confirmed, but edit is done in the same second
    {
        age++;
    }

    auto upd = new Message(msg.id(), msg.userid, msg.ts, age, newdata, newlen,
        msg.isSending(), msg.keyid, newtype, userp);

    auto wptr = weakHandle();
    marshallCall([wptr, this, upd, recipients]()
    {
        if (wptr.deleted())
            return;

        postMsgToSending(upd->isSending() ? OP_MSGUPDX : OP_MSGUPD, upd, recipients);

    }, mChatdClient.mKarereClient->appCtx);

    return upd;
}

void Chat::onLastReceived(Id msgid)
{
    mLastReceivedId = msgid;
    CALL_DB(setLastReceived, msgid);
    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())
    { // we don't have that message in the buffer yet, so we don't know its index
        Idx idx = mDbInterface->getIdxOfMsgidFromHistory(msgid);
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
    if (at(idx).userid != mChatdClient.myHandle())
    {
        CHATID_LOG_WARNING("Last-received pointer points to a message not by us,"
            " possibly the pointer was set incorrectly");
    }
    //notify about messages that become 'received'
    Idx notifyOldest;
    if (mLastReceivedIdx != CHATD_IDX_INVALID) //we have a previous last-received index, notify user about received messages
    {
        if (mLastReceivedIdx > idx)
        {
            CHATID_LOG_ERROR("onLastReceived: Tried to set the index to an older message, ignoring");
            CHATID_LOG_DEBUG("highnum() = %zu, mLastReceivedIdx = %zu, idx = %zu", highnum(), mLastReceivedIdx, idx);
            return;
        }
        notifyOldest = mLastReceivedIdx + 1;
        auto low = lownum();
        if (notifyOldest < low)
        { // mLastReceivedIdx may point to a message in db, older than what we have in RAM
            notifyOldest = low;
        }
        mLastReceivedIdx = idx;
    }
    else
    {
        // No mLastReceivedIdx - notify all messages in RAM
        mLastReceivedIdx = idx;
        notifyOldest = lownum();
    }
    for (Idx i=notifyOldest; i<=mLastReceivedIdx; i++)
    {
        auto& msg = at(i);
        if (msg.userid == mChatdClient.mMyHandle)
        {
            CALL_LISTENER(onMessageStatusChange, i, Message::kDelivered, msg);
        }
    }
}

void Chat::onLastSeen(Id msgid)
{
    Idx idx = CHATD_IDX_INVALID;

    auto it = mIdToIndexMap.find(msgid);
    if (it == mIdToIndexMap.end())  // msgid not loaded in RAM
    {
        idx = mDbInterface->getIdxOfMsgidFromHistory(msgid);   // return CHATD_IDX_INVALID if not found in DB
    }
    else    // msgid is in RAM
    {
        idx = it->second;

        if (at(idx).userid == mChatdClient.mMyHandle)
        {
            CHATID_LOG_WARNING("Last-seen points to a message by us, possibly the pointer was not set properly");
        }
    }

    if (idx == CHATD_IDX_INVALID)   // msgid is unknown locally (during initialization, or very old msg)
    {
        if (mLastSeenIdx == CHATD_IDX_INVALID)  // don't have a previous idx yet --> initialization
        {
            CHATID_LOG_DEBUG("onLastSeen: Setting last seen msgid to %s", ID_CSTR(msgid));
            mLastSeenId = msgid;
            CALL_DB(setLastSeen, msgid);

            return;
        }
    }
    // else --> msgid was found locally
    assert(mLastSeenId.isValid());

    if (idx == mLastSeenIdx)
    {
        return; // we are up to date
    }

    if (mLastSeenIdx != CHATD_IDX_INVALID && idx < mLastSeenIdx) // msgid is older than the locally seen pointer --> update chatd
    {
        // it means the SEEN sent to chatd was not applied remotely (network issue), but it was locally
        CHATID_LOG_WARNING("onLastSeen: chatd last seen message is older than local last seen message. Updating chatd...");
        sendCommand(Command(OP_SEEN) + mChatId + mLastSeenId);
        return;
    }

    CHATID_LOG_DEBUG("setMessageSeen: Setting last seen msgid to %s", ID_CSTR(msgid));
    mLastSeenId = msgid;
    CALL_DB(setLastSeen, msgid);

    if (idx != CHATD_IDX_INVALID)   // if msgid is known locally, notify the unread count
    {
        Idx oldLastSeenIdx = mLastSeenIdx;
        mLastSeenIdx = idx;

        //notify about messages that have become 'seen'
        Idx  notifyOldest = oldLastSeenIdx + 1;
        Idx low = lownum();
        if (notifyOldest < low) // consider only messages in RAM
        {
            notifyOldest = low;
        }

        for (Idx i = notifyOldest; i <= mLastSeenIdx; i++)
        {
            auto& msg = at(i);
            if (msg.userid != mChatdClient.mMyHandle)
            {
                CALL_LISTENER(onMessageStatusChange, i, Message::kSeen, msg);
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
    if (msg.userid == mChatdClient.mMyHandle)
    {
        CHATID_LOG_DEBUG("Asked to mark own message %s as seen, ignoring", ID_CSTR(msg.id()));
        return false;
    }

    auto wptr = weakHandle();
    karere::Id id = msg.id();
    megaHandle seenTimer = karere::setTimeout([this, wptr, idx, id, seenTimer]()
    {
        if (wptr.deleted())
          return;

        mChatdClient.mSeenTimers.erase(seenTimer);

        if ((mLastSeenIdx != CHATD_IDX_INVALID) && (idx <= mLastSeenIdx))
            return;

        CHATID_LOG_DEBUG("setMessageSeen: Setting last seen msgid to %s", ID_CSTR(id));
        sendCommand(Command(OP_SEEN) + mChatId + id);

        Idx notifyStart;
        if (mLastSeenIdx == CHATD_IDX_INVALID)
        {
            notifyStart = lownum()-1;
        }
        else
        {
            Idx lowest = lownum()-1;
            notifyStart = (mLastSeenIdx < lowest) ? lowest : mLastSeenIdx;
        }
        mLastSeenIdx = idx;
        Idx highest = highnum();
        Idx notifyEnd = (mLastSeenIdx > highest) ? highest : mLastSeenIdx;

        for (Idx i=notifyStart+1; i<=notifyEnd; i++)
        {
            auto& m = at(i);
            if (m.userid != mChatdClient.mMyHandle)
            {
                CALL_LISTENER(onMessageStatusChange, i, Message::kSeen, m);
            }
        }
        mLastSeenId = id;
        CALL_DB(setLastSeen, mLastSeenId);
        CALL_LISTENER(onUnreadChanged);
    }, kSeenTimeout, mChatdClient.mKarereClient->appCtx);

    mChatdClient.mSeenTimers.insert(seenTimer);

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
    {
        if (mHaveAllHistory)
        {
            return mDbInterface->getUnreadMsgCountAfterIdx(mLastSeenIdx);
        }
        else
        {
            return -mDbInterface->getUnreadMsgCountAfterIdx(CHATD_IDX_INVALID);
        }
    }
    else if (mLastSeenIdx < lownum())
    {
        return mDbInterface->getUnreadMsgCountAfterIdx(mLastSeenIdx);
    }

    Idx first = mLastSeenIdx+1;
    unsigned count = 0;
    auto last = highnum();
    for (Idx i=first; i<=last; i++)
    {
        auto& msg = at(i);
        if (msg.isValidUnread(mChatdClient.myHandle()))
        {
            count++;
        }
    }
    return count;
}

void Chat::flushOutputQueue(bool fromStart)
{
    if (fromStart)
        mNextUnsent = mSending.begin();

    while (mNextUnsent != mSending.end())
    {
        //kickstart encryption
        //return true if we encrypted at least one message
        if (!msgEncryptAndSend(mNextUnsent++))
            return;
    }
}

void Chat::moveItemToManualSending(OutputQueue::iterator it, ManualSendReason reason)
{
    CALL_DB(deleteSendingItem, it->rowid);
    CALL_DB(saveItemToManualSending, *it, reason);
    CALL_LISTENER(onManualSendRequired, it->msg, it->rowid, reason); //GUI should put this message at end of that list of messages requiring 'manual' resend
    it->msg = nullptr; //don't delete the Message object, it will be owned by the app
    mSending.erase(it);
}

void Chat::removeManualSend(uint64_t rowid)
{
    try
    {
        ManualSendReason reason;
        Message *msg = getManualSending(rowid, reason);
        bool updateLastMsg = (mLastTextMsg.idx() == CHATD_IDX_INVALID) // if not confirmed yet...
                ? (msg->id() == mLastTextMsg.xid()) // ...and it's the msgxid about to be removed
                : (msg->id() == mLastTextMsg.id()); // or was confirmed and the msgid is about to be removed (a pending edit)
        if (updateLastMsg)
        {
            findAndNotifyLastTextMsg();
        }
        delete msg;

        mDbInterface->deleteManualSendItem(rowid);
    }
    catch(std::runtime_error& e)
    {
        CHATID_LOG_ERROR("removeManualSend: Unknown manual send id: %s", e.what());
    }
}

// after a reconnect, we tell the chatd the oldest and newest buffered message
void Chat::joinRangeHist(const ChatDbInfo& dbInfo)
{
    assert(dbInfo.oldestDbId && dbInfo.newestDbId);
    mServerFetchState = kHistFetchingNewFromServer;
    CHATID_LOG_DEBUG("Sending JOINRANGEHIST based on app db: %s - %s",
            dbInfo.oldestDbId.toString().c_str(), dbInfo.newestDbId.toString().c_str());

    mFetchRequest.push(FetchType::kFetchMessages);
    sendCommand(Command(OP_JOINRANGEHIST) + mChatId + dbInfo.oldestDbId + at(highnum()).id());
}

Client::~Client()
{
    cancelSeenTimers();
    mKarereClient->userAttrCache().removeCb(mRichPrevAttrCbHandle);
}

const Id Client::myHandle() const
{
    return mMyHandle;
}

void Client::msgConfirm(Id msgxid, Id msgid)
{
    // TODO: maybe it's more efficient to keep a separate mapping of msgxid to messages?
    for (auto& chat: mChatForChatId)
    {
        if (chat.second->msgConfirm(msgxid, msgid) != CHATD_IDX_INVALID)
            return;
    }
    CHATD_LOG_DEBUG("msgConfirm: No chat knows about message transaction id %s", ID_CSTR(msgxid));
}

//called when MSGID is received
bool Client::onMsgAlreadySent(Id msgxid, Id msgid)
{
    for (auto& chat: mChatForChatId)
    {
        if (chat.second->msgAlreadySent(msgxid, msgid))
            return true;
    }
    return false;
}

bool Chat::msgAlreadySent(Id msgxid, Id msgid)
{
    auto msg = msgRemoveFromSending(msgxid, msgid);
    if (!msg)
        return false; // message does not belong to our chat

    CHATID_LOG_DEBUG("message is sending status was already received by server '%s' -> '%s'", ID_CSTR(msgxid), ID_CSTR(msgid));
    CALL_LISTENER(onMessageRejected, *msg, 0);
    delete msg;
    return true;
}

Message* Chat::msgRemoveFromSending(Id msgxid, Id msgid)
{
    // as msgConfirm() is tried on all chatids, it's normal that we don't have
    // the message, so no error logging of error, just return invalid index
    if (mSending.empty())
        return nullptr;

    SendingItem& item = mSending.front();
    if (item.opcode() == OP_MSGUPDX)
    {
        CHATID_LOG_DEBUG("msgConfirm: sendQueue doesnt start with NEWMSG or MSGUPD, but with MSGUPDX");
        return nullptr;
    }
    Id msgxidOri = item.msg->id();
    if ((item.opcode() == OP_NEWMSG || item.opcode() == OP_NEWNODEMSG) && (msgxidOri != msgxid))
    {
        CHATID_LOG_DEBUG("msgConfirm: sendQueue starts with NEWMSG, but the msgxid is different"
                         " (sent msgxid: '%s', received '%s')", ID_CSTR(msgxidOri), ID_CSTR(msgxid));
        return nullptr;
    }

    if (mNextUnsent == mSending.begin())
        mNextUnsent++; //because we remove the first element

    if (!msgid) // message was rejected by chatd
    {
        moveItemToManualSending(mSending.begin(), (mOwnPrivilege < PRIV_FULL)
            ? kManualSendNoWriteAccess
            : kManualSendGeneralReject); //deletes item
        return nullptr;
    }

    Message *msg = item.msg;
    item.msg = nullptr; // avoid item.msg to be deleted in SendingItem dtor
    assert(msg);
    assert(msg->isSending());

    CALL_DB(deleteSendingItem, item.rowid);
    mSending.pop_front(); //deletes item

    return msg; // gives the ownership
}

// msgid can be 0 in case of rejections
Idx Chat::msgConfirm(Id msgxid, Id msgid)
{
    Message* msg = msgRemoveFromSending(msgxid, msgid);
    if (!msg)
        return CHATD_IDX_INVALID;

    CHATID_LOG_DEBUG("recv NEWMSGID: '%s' -> '%s'", ID_CSTR(msgxid), ID_CSTR(msgid));

    // update msgxid to msgid
    msg->setId(msgid, false);

    // the keyid should be already confirmed by this time
    assert(!msg->isLocalKeyid());

    // add message to history
    push_forward(msg);
    auto idx = mIdToIndexMap[msgid] = highnum();
    CALL_DB(addMsgToHistory, *msg, idx);

    assert(msg->backRefId);
    if (!mRefidToIdxMap.emplace(msg->backRefId, idx).second)
    {
        CALL_LISTENER(onMsgOrderVerificationFail, *msg, idx, "A message with that backrefId "+std::to_string(msg->backRefId)+" already exists");
    }

    //update any following MSGUPDX-s referring to this msgxid
    int count = 0;
    for (auto& item: mSending)
    {
        if (item.msg->id() == msgxid)
        {
            assert(item.opcode() == OP_MSGUPDX);
            item.msg->setId(msgid, false);
            item.setOpcode(OP_MSGUPD);
            count++;
        }
    }
    if (count)
    {
        int countDb = mDbInterface->updateSendingItemsMsgidAndOpcode(msgxid, msgid);
        assert(countDb == count);
        CHATD_LOG_DEBUG("msgConfirm: updated opcode MSGUPDx to MSGUPD and the msgxid=%u to msgid=%u of %d message/s in the sending queue", msgxid, msgid, count);
    }

    CALL_LISTENER(onMessageConfirmed, msgxid, *msg, idx);

    // last text message stuff
    if (msg->isValidLastMessage())
    {
        if (mLastTextMsg.idx() == CHATD_IDX_INVALID)
        {
            if (mLastTextMsg.xid() != msgxid) //it's another message
            {
                onLastTextMsgUpdated(*msg, idx);
            }
            else
            { //it's the same message - set its index, and don't notify again
                mLastTextMsg.confirm(idx, msgid);
                if (!mLastTextMsg.mIsNotified)
                    notifyLastTextMsg();
            }
        }
        else if (idx > mLastTextMsg.idx())
        {
            onLastTextMsgUpdated(*msg, idx);
        }
        else if (idx == mLastTextMsg.idx() && !mLastTextMsg.mIsNotified)
        {
            notifyLastTextMsg();
        }
    }

    if (msg->type == Message::kMsgNormal)
    {
        if (mChatdClient.richLinkState() == Client::kRichLinkEnabled)
        {
            requestRichLink(*msg);
        }
        else if (mChatdClient.richLinkState() == Client::kRichLinkNotDefined)
        {
            manageRichLinkMessage(*msg);
        }
    }

    if (msg->type == Message::kMsgAttachment)
    {
        mAttachmentNodes->addMessage(*msg, true, false);
    }

    return idx;
}

void Chat::keyConfirm(KeyId keyxid, KeyId keyid)
{
    if (keyxid != CHATD_KEYID_UNCONFIRMED)
    {
        CHATID_LOG_ERROR("keyConfirm: Key transaction id != 0xfffffffe");
        return;
    }

    if (mSending.empty())
    {
        CHATID_LOG_ERROR("keyConfirm: Sending queue is empty");
        return;
    }

    const SendingItem &item = mSending.front();
    assert(item.keyCmd);  // first message in sending queue should send a NEWKEY
    KeyId localKeyid = item.keyCmd->localKeyid();
    assert(item.msg->keyid == localKeyid);

    CALL_CRYPTO(onKeyConfirmed, localKeyid, keyid);

    // update keyid of all messages using this confirmed new key
    int count = 0;
    for (auto& item: mSending)
    {
        Message *msg = item.msg;
        if (msg->keyid == localKeyid)
        {
            msg->keyid = keyid;
            count++;
        }
    }
    assert(count);  // a confirmed key should always indicate that a new message was sent
    if (count)
    {
        int countDb = mDbInterface->updateSendingItemsKeyid(localKeyid, keyid);
        assert(countDb == count);
        CHATID_LOG_DEBUG("keyConfirm: updated the localkeyid=%u to keyid=%u of %d message/s in the sending queue", localKeyid, keyid, count);
    }
}

void Chat::onKeyReject()
{
    CALL_CRYPTO(onKeyRejected);
}

void Chat::onHistReject()
{
    CHATID_LOG_WARNING("HIST was rejected, setting chat offline and disabling it");
    assert(false);  // chatd should not REJECT a HIST, it indicates a more critical issue
    mServerFetchState = kHistNotFetching;
    setOnlineState(kChatStateOffline);
    disable(true);
}

void Chat::rejectMsgupd(Id id, uint8_t serverReason)
{
    if (mSending.empty())
    {
        throw std::runtime_error("rejectMsgupd: Send queue is empty");
    }

    auto& front = mSending.front();
    auto opcode = front.opcode();
    if (opcode != OP_MSGUPD && opcode != OP_MSGUPDX)
    {
        throw std::runtime_error(std::string("rejectMsgupd: Front of send queue does not match - expected opcode MSGUPD or MSGUPDX, actual opcode: ")
        +Command::opcodeToStr(opcode));
    }

    auto& msg = *front.msg;
    if (msg.id() != id)
    {
        std::string errorMsg = "rejectMsgupd: Message msgid/msgxid does not match the one at the front of send queue. Rejected: '";
        errorMsg.append(id.toString());
        errorMsg.append("' Sent: '");
        errorMsg.append(msg.id().toString());
        errorMsg.append("'");

        throw std::runtime_error(errorMsg);
    }

    // Update with contains meta has been rejected by server. We don't notify
    if (msg.type == Message::kMsgContainsMeta)
    {
        CHATID_LOG_DEBUG("Message can't be update with meta contained. Reason: %d", serverReason);
        CALL_DB(deleteSendingItem, mSending.front().rowid);
        mSending.pop_front();
        return;
    }

    /* Server reason:
        0 - insufficient privs or not in chat
        1 - message is not your own or you are outside the time window
        2 - message did not change (with same content)
    */
    if (serverReason == 2)
    {
        CALL_LISTENER(onEditRejected, msg, kManualSendEditNoChange);
        CALL_DB(deleteSendingItem, mSending.front().rowid);
        mSending.pop_front();
    }
    else
    {
        moveItemToManualSending(mSending.begin(), (serverReason == 0)
            ? kManualSendNoWriteAccess : kManualSendTooOld);
    }
}

void Chat::rejectGeneric(uint8_t /*opcode*/, uint8_t /*reason*/)
{
    //TODO: Implement
}

void Chat::onMsgUpdated(Message* cipherMsg)
{
//first, if it was us who updated the message confirm the update by removing any
//queued msgupds from sending, even if they are not the same edit (i.e. a received
//MSGUPD from another client with out user will cancel any pending edit by our client
    time_t updateTs = 0;
    bool richLinkRemoved = false;

    if (cipherMsg->userid == client().myHandle())
    {
        for (auto it = mSending.begin(); it != mSending.end(); )
        {
            auto& item = *it;
            if (((item.opcode() != OP_MSGUPD) && (item.opcode() != OP_MSGUPDX))
                || (item.msg->id() != cipherMsg->id()))
            {
                it++;
                continue;
            }
            //erase item
            CALL_DB(deleteSendingItem, item.rowid);
            auto erased = it;
            it++;
            mPendingEdits.erase(cipherMsg->id());
            updateTs = item.msg->updated;
            richLinkRemoved = item.msg->richLinkRemoved;
            mSending.erase(erased);
        }
    }
    mCrypto->msgDecrypt(cipherMsg)
    .fail([this, cipherMsg](const ::promise::Error& err) -> ::promise::Promise<Message*>
    {
        assert(cipherMsg->isPendingToDecrypt());

        int type = err.type();
        switch (type)
        {
            case SVCRYPTO_EEXPIRED:
                return ::promise::Error("Strongvelope was deleted, ignore message", EINVAL, SVCRYPTO_EEXPIRED);

            case SVCRYPTO_ENOMSG:
                return ::promise::Error("History was reloaded, ignore message", EINVAL, SVCRYPTO_ENOMSG);

            case SVCRYPTO_ENOKEY:
                //we have a normal situation where a message was sent just before a user joined, so it will be undecryptable
                CHATID_LOG_WARNING("No key to decrypt message %s, possibly message was sent just before user joined", ID_CSTR(cipherMsg->id()));
                assert(mChatdClient.chats(mChatId).isGroup());
                assert(cipherMsg->keyid < 0xffff0001);   // a confirmed keyid should never be the transactional keyxid
                cipherMsg->setEncrypted(Message::kEncryptedNoKey);
                break;

            case SVCRYPTO_ESIGNATURE:
                CHATID_LOG_ERROR("Signature verification failure for message: %s", ID_CSTR(cipherMsg->id()));
                cipherMsg->setEncrypted(Message::kEncryptedSignature);
                break;

            case SVCRYPTO_ENOTYPE:
                CHATID_LOG_WARNING("Unknown type of management message: %d (msgid: %s)", cipherMsg->type, ID_CSTR(cipherMsg->id()));
                cipherMsg->setEncrypted(Message::kEncryptedNoType);
                break;

            case SVCRYPTO_EMALFORMED:
            default:
                CHATID_LOG_ERROR("Malformed message: %s", ID_CSTR(cipherMsg->id()));
                cipherMsg->setEncrypted(Message::kEncryptedMalformed);
                break;
        }

        return cipherMsg;
    })
    .then([this, updateTs, richLinkRemoved](Message* msg)
    {
        assert(!msg->isPendingToDecrypt()); //either decrypted or error
        if (!msg->empty() && msg->type == Message::kMsgNormal && (*msg->buf() == 0))
        {
            if (msg->dataSize() < 2)
                CHATID_LOG_ERROR("onMsgUpdated: Malformed special message received - starts with null char received, but its length is 1. Assuming type of normal message");
            else
                msg->type = msg->buf()[1] + Message::Type::kMsgOffset;
        }

        //update in memory, if loaded
        auto msgit = mIdToIndexMap.find(msg->id());
        Idx idx;
        if (msgit != mIdToIndexMap.end())   // message is loaded in RAM
        {
            idx = msgit->second;
            auto& histmsg = at(idx);
            unsigned char histType = histmsg.type;

            if ( (msg->type == Message::kMsgTruncate
                  && histmsg.type == msg->type
                  && histmsg.ts == msg->ts)
                    || (msg->type != Message::kMsgTruncate
                        && histmsg.updated == msg->updated) )
            {
                CHATID_LOG_DEBUG("Skipping replayed MSGUPD");
                delete msg;
                return;
            }

            if (!msg->empty() && msg->type == Message::kMsgNormal
                             && !richLinkRemoved        // user have not requested to remove rich-link preview (generate it)
                             && updateTs && (updateTs == msg->updated)) // message could have been updated by another client earlier/later than our update's attempt
            {
                if (client().richLinkState() == Client::kRichLinkEnabled)
                {
                    requestRichLink(*msg);
                }
                else if (mChatdClient.richLinkState() == Client::kRichLinkNotDefined)
                {
                    manageRichLinkMessage(*msg);
                }
            }

            //update in db
            CALL_DB(updateMsgInHistory, msg->id(), *msg);

            // update in RAM
            histmsg.assign(*msg);     // content
            histmsg.updated = msg->updated;
            histmsg.type = msg->type;
            histmsg.userid = msg->userid;
            histmsg.setEncrypted(msg->isEncrypted());
            if (msg->type == Message::kMsgTruncate)
            {
                histmsg.ts = msg->ts;   // truncates update the `ts` instead of `update`
            }

            if (idx > mNextHistFetchIdx)
            {
                // msg.ts is zero - chatd doesn't send the original timestamp
                CALL_LISTENER(onMessageEdited, histmsg, idx);
            }
            else
            {
                CHATID_LOG_DEBUG("onMessageEdited() skipped for not-loaded-yet (by the app) message");
            }

            if (msg->isDeleted())
            {
                if (!msg->isOwnMessage(client().myHandle()))
                {
                    CALL_LISTENER(onUnreadChanged);
                }

                if (histType == Message::kMsgAttachment)
                {
                    mAttachmentNodes->deleteMessage(*msg);
                }
            }

            if (msg->type == Message::kMsgTruncate)
            {
                handleTruncate(*msg, idx);
            }
            else if (mLastTextMsg.idx() == idx) //last text msg stuff
            {
                //our last text message was edited
                if (histmsg.isValidLastMessage()) //same message, but with updated contents
                {
                    onLastTextMsgUpdated(histmsg, idx);
                }
                else //our last text msg is not valid anymore, find another one
                {
                    findAndNotifyLastTextMsg();
                }
            }
        }
        else    // message not loaded in RAM
        {
            CHATID_LOG_DEBUG("onMsgUpdated(): update for message not loaded");

            // check if message in DB is outdated
            uint16_t delta = 0;
            CALL_DB(getMessageDelta, msg->id(), &delta);

            if (delta < msg->updated)
            {
                //update in db
                CALL_DB(updateMsgInHistory, msg->id(), *msg);
            }

            if (msg->isDeleted()) // previous type is unknown, so cannot check for attachment type here
            {
                mAttachmentNodes->deleteMessage(*msg);
            }
        }

        delete msg;
    })
    .fail([this, cipherMsg](const ::promise::Error& err)
    {
        if (err.type() == SVCRYPTO_ENOMSG)
        {
            CHATID_LOG_WARNING("Msg has been deleted during decryption process");

            //if (err.type() == SVCRYPTO_ENOMSG)
                //TODO: If a message could be deleted individually, decryption process should be restarted again
                // It isn't a possibilty with actual implementation
        }
        else
        {
            CHATID_LOG_WARNING("Message %s can't be decrypted: Failure type %s (%d)",
                               ID_CSTR(cipherMsg->id()), err.what(), err.type());
            delete cipherMsg;
        }
    });
}
void Chat::handleTruncate(const Message& msg, Idx idx)
{
// chatd may re-send a MSGUPD at login, if there are no newer messages in the
// chat. We have to be prepared to handle this, i.e. handleTruncate() must
// be idempotent.
// However, handling the SEEN and RECEIVED pointers in in a replayed truncate
// is a bit tricky, because if they point to the truncate point (i.e. idx)
// normally we would set them in a way that makes the truncate management message
// at the truncation point unseen. But in case of a replay, we don't want it
// to be unseen, as this will reset the unread message count to '1+' every time
// the client connects, until someoone posts a new message in the chat.
// To avoid this, we have to detect the replay. But if we detect it, we can actually
// avoid the whole replay (even the idempotent part), and just bail out.

    CHATID_LOG_DEBUG("Truncating chat history before msgid %s, idx %d, fwdStart %d", ID_CSTR(msg.id()), idx, mForwardStart);
    CALL_CRYPTO(resetSendKey);      // discard current key, if any
    CALL_DB(truncateHistory, msg);
    if (idx != CHATD_IDX_INVALID)   // message is loaded in RAM
    {
        //GUI must detach and free any resources associated with
        //messages older than the one specified
        CALL_LISTENER(onHistoryTruncated, msg, idx);

        deleteMessagesBefore(idx);
        removePendingRichLinks(idx);

        // update last-seen pointer
        if (mLastSeenIdx != CHATD_IDX_INVALID)
        {
            if (mLastSeenIdx <= idx)
            {
                //if we haven't seen even messages before the truncation point,
                //now we will have not seen any message after the truncation
                mLastSeenIdx = CHATD_IDX_INVALID;
                mLastSeenId = 0;
                CALL_DB(setLastSeen, 0);
            }
        }

        // update last-received pointer
        if (mLastReceivedIdx != CHATD_IDX_INVALID)
        {
            if (mLastReceivedIdx <= idx)
            {
                mLastReceivedIdx = CHATD_IDX_INVALID;
                mLastReceivedId = 0;
                CALL_DB(setLastReceived, 0);
            }
        }

        if (mChatdClient.isMessageReceivedConfirmationActive() && mLastIdxReceivedFromServer <= idx)
        {
            mLastIdxReceivedFromServer = CHATD_IDX_INVALID;
            mLastIdReceivedFromServer = karere::Id::null();
            // TODO: the update of those variables should be persisted
        }
    }

    // since we have the truncate message in local history (otherwise chatd wouldn't have sent us
    // the truncate), now we know we have all history and what's the oldest msgid.
    CALL_DB(setHaveAllHistory, true);
    mHaveAllHistory = true;
    mOldestKnownMsgId = msg.id();

    // if truncate was received for a message not loaded in RAM, we may have more history in DB
    mHasMoreHistoryInDb = at(lownum()).id() != mOldestKnownMsgId;

    CALL_LISTENER(onUnreadChanged);
    findAndNotifyLastTextMsg();

    // Find an attachment newer than truncate (lownum) in order to truncate node-history
    // (if no more attachments in history buffer, node-history will be fully cleared)
    Id attachmentTruncateFromId = Id::inval();
    for (Idx i = lownum(); i < highnum(); i++)
    {
        if (at(i).type == Message::kMsgAttachment)
        {
            attachmentTruncateFromId = at(i).id();
            break;
        }
    }
    mAttachmentNodes->truncateHistory(attachmentTruncateFromId);
    if (mDecryptionAttachmentsHalted)
    {
        while (!mAttachmentsPendingToDecrypt.empty())
        {
            mAttachmentsPendingToDecrypt.pop();
        }
        mTruncateAttachment = true; // --> indicates the message being decrypted must be discarded
    }
}

Id Chat::makeRandomId()
{
    static std::uniform_int_distribution<uint64_t>distrib(0, 0xffffffffffffffff);
    static std::random_device rd;
    return distrib(rd);
}

void Chat::deleteMessagesBefore(Idx idx)
{
    //delete everything before idx, but not including idx
    if (idx > mForwardStart)
    {
        mBackwardList.clear();
        auto delCount = idx-mForwardStart;
        mForwardList.erase(mForwardList.begin(), mForwardList.begin()+delCount);
        mForwardStart += delCount;
    }
    else
    {
        mBackwardList.erase(mBackwardList.begin()+mForwardStart-idx, mBackwardList.end());
    }
}

Message::Status Chat::getMsgStatus(const Message& msg, Idx idx) const
{
    assert(idx != CHATD_IDX_INVALID);
    if (msg.userid == mChatdClient.mMyHandle)
    {
        if (msg.isSending())
            return Message::kSending;

        // Check if we have an unconfirmed edit
        for (auto& item: mSending)
        {
            if (item.msg->id() == msg.id())
            {
                auto op = item.opcode();
                if (op == OP_MSGUPD || op == OP_MSGUPDX)
                    return Message::kSending;
            }
        }
        if (idx <= mLastReceivedIdx)
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
/* We have 3 stages:
 - add to history buffer, allocating an index
 - decrypt - may happen asynchronous if crypto needs to fetch stuff from network.
 Also the message may be undecryptable - in this case continue as normal, but message's
 isEncrypted() flag will be set to true, so GUI can decide how to show it
 - add message to history db (including its isEncrypted() state(), handle received
 and seen pointers, call old/new message user callbacks. This may be executed for
 messages that are actually encrypted - app must be prepared for that
*/
Idx Chat::msgIncoming(bool isNew, Message* message, bool isLocal)
{
    assert((isLocal && !isNew) || !isLocal);
    auto msgid = message->id();
    assert(msgid);
    Idx idx;

    if (isNew)
    {
        auto it = mIdToIndexMap.find(message->id());
        if (it != mIdToIndexMap.end())  // message already received
        {
            CHATID_LOG_WARNING("Ignoring duplicated NEWMSG: msgid %s, idx %d", ID_CSTR(it->first), it->second);
            return it->second;
        }

        push_forward(message);
        idx = highnum();
        if (!mOldestKnownMsgId)
            mOldestKnownMsgId = msgid;
    }
    else
    {
        if (!isLocal)
        {
            //history message older than the oldest we have
            assert(isFetchingFromServer());
            assert(message->isPendingToDecrypt());
            mLastServerHistFetchCount++;
            if (mHasMoreHistoryInDb)
            { //we have db history that is not loaded, so we determine the index
              //by the db, and don't add the message to RAM
                idx = mDbInterface->getOldestIdx()-1;
            }
            else
            {
                //all history is in RAM, determine the index from RAM
                push_back(message);
                idx = lownum();
            }
            //shouldn't we update this only after we save the msg to db?
            mOldestKnownMsgId = msgid;
        }
        else //local history message - load from DB to RAM
        {
            push_back(message);
            idx = lownum();
            if (msgid == mOldestKnownMsgId)
            //we have just processed the oldest message from the db
                mHasMoreHistoryInDb = false;
        }
    }
    mIdToIndexMap[msgid] = idx;
    handleLastReceivedSeen(msgid);
    msgIncomingAfterAdd(isNew, isLocal, *message, idx);
    return idx;
}

bool Chat::msgIncomingAfterAdd(bool isNew, bool isLocal, Message& msg, Idx idx)
{
    if (isLocal)
    {
        if (msg.isEncrypted() != Message::kEncryptedNoType)
        {
            msgIncomingAfterDecrypt(isNew, true, msg, idx);
        }
        else    // --> unknown management msg type, we may want to try to decode it again
        {
            Message *message = &msg;
            mCrypto->msgDecrypt(message)
            .fail([this, message](const ::promise::Error& err) -> ::promise::Promise<Message*>
            {
                assert(message->isEncrypted() == Message::kEncryptedNoType);
                int type = err.type();
                switch (type)
                {
                    case SVCRYPTO_EEXPIRED:
                        return ::promise::Error("Strongvelope was deleted, ignore message", EINVAL, SVCRYPTO_EEXPIRED);

                    case SVCRYPTO_ENOTYPE:
                        CHATID_LOG_WARNING("Retry to decrypt unknown type of management message failed (not yet supported): %d (msgid: %s)", message->type, ID_CSTR(message->id()));
                        break;

                    default:
                        CHATID_LOG_ERROR("Retry to decrypt type of management message failed. Malformed message: %s", ID_CSTR(message->id()));
                        message->setEncrypted(Message::kEncryptedMalformed);
                        break;
                }
                return message;
            })
            .then([this, isNew, idx](Message* message)
            {
                if (message->isEncrypted() != Message::kEncryptedNoType)
                {
                    CALL_DB(updateMsgInHistory, message->id(), *message);   // update 'data' & 'is_encrypted'
                }
                msgIncomingAfterDecrypt(isNew, true, *message, idx);
            })
            .fail([this, message](const ::promise::Error& err)
            {
                CHATID_LOG_WARNING("Retry to decrypt unknown type of management message failed. (msgid: %s, failure type %s (%d))",
                                   ID_CSTR(message->id()), err.what(), err.type());
            });
        }
        return true;    // decrypt was not done immediately, but none checks the returned value in this codepath
    }
    else
    {
        assert(msg.isPendingToDecrypt()); //no decrypt attempt was made
    }

    try
    {
        mCrypto->handleLegacyKeys(msg);
    }
    catch(std::exception& e)
    {
        CHATID_LOG_WARNING("handleLegacyKeys threw error: %s\n"
            "Queued messages for decrypt: %d - %d. Ignoring", e.what(),
            mDecryptOldHaltedAt, idx);
        msg.setEncrypted(Message::kEncryptedNoKey);
        return true;
    }

    if (!msg.isPendingToDecrypt() && msg.isEncrypted() != Message::kEncryptedNoType)
    {
        CHATID_LOG_DEBUG("Message already decrypted or undecryptable: %s, bailing out", ID_CSTR(msg.id()));
        return true;
    }

    if (isNew)
    {
        if (mDecryptNewHaltedAt != CHATD_IDX_INVALID)
        {
            CHATID_LOG_DEBUG("Decryption of new messages is halted, message queued for decryption");
            return false;
        }
    }
    else
    {
        if (mDecryptOldHaltedAt != CHATD_IDX_INVALID)
        {
            CHATID_LOG_DEBUG("Decryption of old messages is halted, message queued for decryption");
            return false;
        }
    }
    CHATD_LOG_CRYPTO_CALL("Calling ICrypto::decrypt()");
    auto pms = mCrypto->msgDecrypt(&msg);
    if (pms.succeeded())
    {
        assert(!msg.isEncrypted());
        msgIncomingAfterDecrypt(isNew, false, msg, idx);
        return true;
    }

    CHATID_LOG_DEBUG("Decryption could not be done immediately, halting for next messages");
    if (isNew)
        mDecryptNewHaltedAt = idx;
    else
        mDecryptOldHaltedAt = idx;

    auto message = &msg;
    pms.fail([this, message](const ::promise::Error& err) -> ::promise::Promise<Message*>
    {
        assert(message->isPendingToDecrypt());

        int type = err.type();
        switch (type)
        {
            case SVCRYPTO_EEXPIRED:
                return ::promise::Error("Strongvelope was deleted, ignore message", EINVAL, SVCRYPTO_EEXPIRED);

            case SVCRYPTO_ENOMSG:
                return ::promise::Error("History was reloaded, ignore message", EINVAL, SVCRYPTO_ENOMSG);

            case SVCRYPTO_ENOKEY:
                //we have a normal situation where a message was sent just before a user joined, so it will be undecryptable
                CHATID_LOG_WARNING("No key to decrypt message %s, possibly message was sent just before user joined", ID_CSTR(message->id()));
                assert(mChatdClient.chats(mChatId).isGroup());
                assert(message->keyid < 0xffff0001);   // a confirmed keyid should never be the transactional keyxid
                message->setEncrypted(Message::kEncryptedNoKey);
                break;

            case SVCRYPTO_ESIGNATURE:
                CHATID_LOG_ERROR("Signature verification failure for message: %s", ID_CSTR(message->id()));
                message->setEncrypted(Message::kEncryptedSignature);
                break;

            case SVCRYPTO_ENOTYPE:
                CHATID_LOG_WARNING("Unknown type of management message: %d (msgid: %s)", message->type, ID_CSTR(message->id()));
                message->setEncrypted(Message::kEncryptedNoType);
                break;

            case SVCRYPTO_EMALFORMED:
            default:
                CHATID_LOG_ERROR("Malformed message: %s", ID_CSTR(message->id()));
                message->setEncrypted(Message::kEncryptedMalformed);
                break;
        }

        return message;
    })
    .then([this, isNew, isLocal, idx](Message* message)
    {
#ifndef NDEBUG
        if (isNew)
            assert(mDecryptNewHaltedAt == idx);
        else
            assert(mDecryptOldHaltedAt == idx);
#endif
        msgIncomingAfterDecrypt(isNew, false, *message, idx);
        if (isNew)
        {
            // Decrypt the rest - try to decrypt immediately (synchromously),
            // so that order is guaranteed. Bail out of the loop at the first
            // message that can't be decrypted immediately(msgIncomingAfterAdd()
            // returns false). Will continue when the delayed decrypt finishes

            auto first = mDecryptNewHaltedAt + 1;
            mDecryptNewHaltedAt = CHATD_IDX_INVALID;
            auto last = highnum();
            for (Idx i = first; i <= last; i++)
            {
                if (!msgIncomingAfterAdd(isNew, false, at(i), i))
                    break;
            }
            if ((mServerFetchState == kHistDecryptingNew) &&
                (mDecryptNewHaltedAt == CHATD_IDX_INVALID)) //all messages decrypted
            {
                mServerFetchState = kHistNotFetching;
            }
        }
        else
        {
            // Old history
            // Decrypt the rest synchronously, bail out on first that can't
            // decrypt synchonously.
            // Local messages are always decrypted, this is handled
            // at the start of this func

            assert(!isLocal);
            auto first = mDecryptOldHaltedAt - 1;
            mDecryptOldHaltedAt = CHATD_IDX_INVALID;
            auto last = lownum();
            for (Idx i = first; i >= last; i--)
            {
                if (!msgIncomingAfterAdd(isNew, false, at(i), i))
                    break;
            }
            if ((mServerFetchState == kHistDecryptingOld) &&
                (mDecryptOldHaltedAt == CHATD_IDX_INVALID))
            {
                mServerFetchState = kHistNotFetching;
                if (mServerOldHistCbEnabled)
                {
                    CALL_LISTENER(onHistoryDone, kHistSourceServer);
                }
            }
        }
    })
    .fail([this, message](const ::promise::Error& err)
    {
        if (err.type() == SVCRYPTO_ENOMSG)
        {
            CHATID_LOG_WARNING("Msg has been deleted during decryption process");

            //if (err.type() == SVCRYPTO_ENOMSG)
                //TODO: If a message could be deleted individually, decryption process should be restarted again
                // It isn't a possibilty with actual implementation
        }
        else
        {
            CHATID_LOG_WARNING("Message %s can't be decrypted: Failure type %s (%d)",
                               ID_CSTR(message->id()), err.what(), err.type());
        }
    });

    return false; //decrypt was not done immediately
}

// Save to history db, handle received and seen pointers, call new/old message user callbacks
void Chat::msgIncomingAfterDecrypt(bool isNew, bool isLocal, Message& msg, Idx idx)
{
    assert(idx != CHATD_IDX_INVALID);
    if (!isNew)
    {
        mLastHistDecryptCount++;
    }
    auto msgid = msg.id();
    if (!isLocal)
    {
        assert(!msg.isPendingToDecrypt()); //either decrypted or error
        if (!msg.empty() && msg.type == Message::kMsgNormal && (*msg.buf() == 0)) //'special' message - attachment etc
        {
            if (msg.dataSize() < 2)
                CHATID_LOG_ERROR("Malformed special message received - starts with null char received, but its length is 1. Assuming type of normal message");
            else
                msg.type = msg.buf()[1] + Message::Type::kMsgOffset;
        }

        verifyMsgOrder(msg, idx);
        CALL_DB(addMsgToHistory, msg, idx);

        if (mChatdClient.isMessageReceivedConfirmationActive() && !isGroup() &&
                (msg.userid != mChatdClient.mMyHandle) && // message is not ours
                ((mLastIdxReceivedFromServer == CHATD_IDX_INVALID) ||   // no local history
                 (idx > mLastIdxReceivedFromServer)))   // newer message than last received
        {
            mLastIdxReceivedFromServer = idx;
            mLastIdReceivedFromServer = msgid;
            // TODO: the update of those variables should be persisted

            sendCommand(Command(OP_RECEIVED) + mChatId + msgid);
        }
    }
    if (msg.backRefId && !mRefidToIdxMap.emplace(msg.backRefId, idx).second)
    {
        CALL_LISTENER(onMsgOrderVerificationFail, msg, idx, "A message with that backrefId "+std::to_string(msg.backRefId)+" already exists");
    }

    auto status = getMsgStatus(msg, idx);
    if (isNew)
    {
        // update in memory the timestamp of the most recent message from this user
        if (msg.ts > mChatdClient.getLastMsgTs(msg.userid))
        {
            mChatdClient.setLastMsgTs(msg.userid, msg.ts);
            mChatdClient.mKarereClient->updateAndNotifyLastGreen(msg.userid);
        }

        CALL_LISTENER(onRecvNewMessage, idx, msg, status);
    }
    else
    {
        // old message
        // local messages are obtained on-demand, so if isLocal,
        // then always send to app
        bool isChatRoomOpened = mChatdClient.mKarereClient->isChatRoomOpened(mChatId);
        if (isLocal || (mServerOldHistCbEnabled && isChatRoomOpened))
        {
            CALL_LISTENER(onRecvHistoryMessage, idx, msg, status, isLocal);
        }
    }
    if (msg.type == Message::kMsgTruncate)
    {
        if (isNew)
        {
            handleTruncate(msg, idx);
        }
        return;
    }

    if (isNew || (mLastSeenIdx == CHATD_IDX_INVALID))
        CALL_LISTENER(onUnreadChanged);

    //handle last text message
    if (msg.isValidLastMessage())
    {
        if (!mLastTextMsg.isValid()  // we don't have any last-text-msg yet, just use any
                || (mLastTextMsg.idx() == CHATD_IDX_INVALID) //current last-text-msg is a pending send, always override it
                || (idx > mLastTextMsg.idx())) //we have a newer message
        {
            onLastTextMsgUpdated(msg, idx);
        }
        else if (idx == mLastTextMsg.idx() && !mLastTextMsg.mIsNotified)
        { //we have already updated mLastTextMsg because app called
          //lastTextMessage() from the onRecvXXX callback, but we haven't done
          //onLastTextMessageUpdated() with it
            notifyLastTextMsg();
        }
    }

    if (msg.type == Message::Type::kMsgAttachment)
    {
        mAttachmentNodes->addMessage(msg, isNew, false);
    }
}

bool Chat::msgNodeHistIncoming(Message *msg)
{
    mAttachNodesReceived++;
    if (!mDecryptionAttachmentsHalted)
    {
        auto pms = mCrypto->msgDecrypt(msg);
        if (pms.succeeded())
        {
            assert(!msg->isEncrypted());
            mAttachmentNodes->addMessage(*msg, false, false);
            delete msg;

            return true;
        }
        else
        {
            mDecryptionAttachmentsHalted = true;
            pms.then([this](Message* msg)
            {
                if (!mTruncateAttachment)
                {
                    mAttachmentNodes->addMessage(*msg, false, false);
                }
                delete msg;
                mTruncateAttachment = false;
                bool decrypt = true;
                mDecryptionAttachmentsHalted = false;
                while (!mAttachmentsPendingToDecrypt.empty() && decrypt)
                {
                    decrypt = msgNodeHistIncoming(mAttachmentsPendingToDecrypt.front());
                    mAttachmentsPendingToDecrypt.pop();
                }

                if (mAttachmentsPendingToDecrypt.empty() && decrypt && mAttachmentHistDoneReceived)
                {
                    attachmentHistDone();
                }
            })
            .fail([this, msg](const ::promise::Error& /*err*/)
            {
                assert(msg->isPendingToDecrypt());
                delete msg;
                mTruncateAttachment = false;
                bool decrypt = true;
                mDecryptionAttachmentsHalted = false;
                while (mAttachmentsPendingToDecrypt.size() && decrypt)
                {
                    decrypt = msgNodeHistIncoming(mAttachmentsPendingToDecrypt.front());
                    mAttachmentsPendingToDecrypt.pop();
                }

                if (!mAttachmentsPendingToDecrypt.empty() && decrypt && mAttachmentHistDoneReceived)
                {
                    attachmentHistDone();
                }
            });
        }
    }
    else
    {
        mAttachmentsPendingToDecrypt.push(msg);
    }

    return false;
}

void Chat::onMsgTimestamp(uint32_t ts)
{
    if (ts == mLastMsgTs)
        return;

    if (ts < mLastMsgTs)
    {
        CHATID_LOG_WARNING("onMsgTimestamp: moving last-ts to an older ts (last-msg was deleted or history was truncated)");
    }

    mLastMsgTs = ts;
    CALL_LISTENER(onLastMessageTsUpdated, ts);
}

void Chat::verifyMsgOrder(const Message& msg, Idx idx)
{
    for (auto refid: msg.backRefs)
    {
        auto it = mRefidToIdxMap.find(refid);
        if (it == mRefidToIdxMap.end())
            continue;
        Idx targetIdx = it->second;
        if (targetIdx >= idx)
        {
            CALL_LISTENER(onMsgOrderVerificationFail, msg, idx, "Message order verification failed, possible history tampering");
            client().mKarereClient->api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99000, "order tampering native");
            return;
        }
    }
}

void Chat::handleLastReceivedSeen(Id msgid)
{
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
}

void Chat::onUserJoin(Id userid, Priv priv)
{
    if (mOnlineState < kChatStateJoining)
        throw std::runtime_error("onUserJoin received while not joining and not online");

    if (userid == client().myHandle())
    {
        mOwnPrivilege = priv;
    }

    if (isJoining())
    {
        mUserDump.insert(userid);
    }

    if (isLoggedIn() || !mIsFirstJoin)
    {
        mUsers.insert(userid);
        CALL_CRYPTO(onUserJoin, userid);
        CALL_LISTENER(onUserJoin, userid, priv);
    }
}

void Chat::onUserLeave(Id userid)
{
    if (mOnlineState < kChatStateJoining)
        throw std::runtime_error("onUserLeave received while not joining and not online");

    if (userid == client().myHandle())
    {
        mOwnPrivilege = PRIV_NOTPRESENT;

#ifndef KARERE_DISABLE_WEBRTC
        if (mChatdClient.mRtcHandler)
        {
            mChatdClient.mRtcHandler->onKickedFromChatRoom(mChatId);
        }
    }
    else
    {
        if (mChatdClient.mRtcHandler)
        {
            // the call will usually be terminated by the kicked user, but just in case
            // the client doesn't do it properly, we notify the user left the call
            uint32_t clientid = mChatdClient.mRtcHandler->clientidFromPeer(mChatId, userid);
            if (clientid)
            {
                onEndCall(userid, clientid);
            }
        }
#endif
    }



    if (isLoggedIn() || !mIsFirstJoin)
    {
        mUsers.erase(userid);
        CALL_CRYPTO(onUserLeave, userid);
        CALL_LISTENER(onUserLeave, userid);
    }
}

void Chat::onJoinComplete()
{
    if (mUsers != mUserDump)
    {
        mUsers.swap(mUserDump);
        CALL_CRYPTO(setUsers, &mUsers);
    }
    mUserDump.clear();
    mEncryptionHalted = false;
    setOnlineState(kChatStateOnline);
    flushOutputQueue(true); //flush encrypted messages

    if (mIsFirstJoin)
    {
        mIsFirstJoin = false;
        if (!mLastTextMsg.isValid())
        {
            CHATID_LOG_DEBUG("onJoinComplete: Haven't received a text message during join, getting last text message on-demand");
            findAndNotifyLastTextMsg();
        }
    }

#ifndef KARERE_DISABLE_WEBRTC
    if (mChatdClient.mKarereClient->rtc)
    {
        mChatdClient.mKarereClient->rtc->removeCallWithoutParticipants(mChatId);
    }
#endif
}

void Chat::resetGetHistory()
{
    mNextHistFetchIdx = CHATD_IDX_INVALID;
    mServerOldHistCbEnabled = false;
}

void Chat::setOnlineState(ChatState state)
{
    if (state == mOnlineState)
        return;

    CHATID_LOG_DEBUG("Online state change: %s --> %s", chatStateToStr(mOnlineState), chatStateToStr(state));

    mOnlineState = state;
    CALL_CRYPTO(onOnlineStateChange, state);
    mListener->onOnlineStateChange(state);  // avoid log message, we already have the one above

    if (state == kChatStateOnline && mChatdClient.areAllChatsLoggedIn())
    {
        mChatdClient.mKarereClient->setCommitMode(true);

        if (!mChatdClient.mKarereClient->mSyncPromise.done())
        {
            CHATID_LOG_DEBUG("Pending pushReceived is completed now");
            if (mChatdClient.mKarereClient->mSyncTimer)
            {
                cancelTimeout(mChatdClient.mKarereClient->mSyncTimer, mChatdClient.mKarereClient->appCtx);
                mChatdClient.mKarereClient->mSyncTimer = 0;
            }
            mChatdClient.mKarereClient->mSyncPromise.resolve();
        }
    }
}

void Chat::onLastTextMsgUpdated(const Message& msg, Idx idx)
{
    //idx == CHATD_IDX_INVALID when we notify about a message in the send queue
    //either (msg.isSending() && idx-is-invalid) or (!msg.isSending() && index-is-valid)
    assert(!((idx == CHATD_IDX_INVALID) ^ msg.isSending()));
    assert(!msg.empty() || msg.isManagementMessage());
    assert(msg.type != Message::kMsgRevokeAttachment);
    mLastTextMsg.assign(msg, idx);
    notifyLastTextMsg();
}

void Chat::notifyLastTextMsg()
{
    CALL_LISTENER(onLastTextMessageUpdated, mLastTextMsg);
    mLastTextMsg.mIsNotified = true;

    // upon deletion of lastMessage and/or truncate, need to find the new suitable
    // lastMessage through the history. In that case, we need to notify also the
    // message's timestamp to reorder the list of chats
    Message *lastMsg = findOrNull(mLastTextMsg.idx());
    if (lastMsg)
    {
        onMsgTimestamp(lastMsg->ts);
    }
}

uint8_t Chat::lastTextMessage(LastTextMsg*& msg)
{
    if (mLastTextMsg.isValid())
    {
        msg = &mLastTextMsg;
        return LastTextMsgState::kHave;
    }

    if (mLastTextMsg.isFetching() || !findLastTextMsg())
    {
        msg = nullptr;
        return LastTextMsgState::kFetching;
    }

    if (mLastTextMsg.isValid()) // findLastTextMsg() may have found it locally
    {
        msg = &mLastTextMsg;
        return LastTextMsgState::kHave;
    }

    msg = nullptr;
    return mLastTextMsg.state();
}

bool Chat::findLastTextMsg()
{
    if (!mSending.empty())
    {
        for (auto it = mSending.rbegin(); it!= mSending.rend(); it++)
        {
            assert(it->msg);
            auto& msg = *it->msg;
            if (msg.isValidLastMessage())
            {
                mLastTextMsg.assign(msg, CHATD_IDX_INVALID);
                CHATID_LOG_DEBUG("lastTextMessage: Text message found in send queue");
                return true;
            }
        }
    }
    if (!empty())
    {
        //check in ram
        auto low = lownum();
        for (Idx i=highnum(); i >= low; i--)
        {
            auto& msg = at(i);
            if (msg.isValidLastMessage())
            {
                mLastTextMsg.assign(msg, i);
                CHATID_LOG_DEBUG("lastTextMessage: Text message found in RAM");
                return true;
            }
        }
        //check in db
        CALL_DB(getLastTextMessage, lownum()-1, mLastTextMsg);
        if (mLastTextMsg.isValid())
        {
            CHATID_LOG_DEBUG("lastTextMessage: Text message found in DB");
            return true;
        }
    }
    if (mHaveAllHistory)
    {
        CHATID_LOG_DEBUG("lastTextMessage: No text message in whole history");
        assert(!mLastTextMsg.isValid());
        return true;
    }

    CHATID_LOG_DEBUG("lastTextMessage: No text message found locally");

    // prevent access to websockets from app's thread
    auto wptr = weakHandle();
    marshallCall([wptr, this]()
    {
        if (wptr.deleted())
            return;

        // since this codepath is marshalled, it could happen the last
        // text message is already found before the marshall is executed
        if (mLastTextMsg.isValid())
            return;

        if (isFetchingFromServer())
        {
            mLastTextMsg.setState(LastTextMsgState::kFetching);
        }
        else if (isLoggedIn())
        {
            CHATID_LOG_DEBUG("lastTextMessage: fetching history from server");

            mServerOldHistCbEnabled = false;
            requestHistoryFromServer(-initialHistoryFetchCount);
            mLastTextMsg.setState(LastTextMsgState::kFetching);
        }

    }, mChatdClient.mKarereClient->appCtx);

    return false;
}

void Chat::findAndNotifyLastTextMsg()
{
    auto wptr = weakHandle();
    marshallCall([wptr, this]() //prevent re-entrancy
    {
        if (wptr.deleted())
            return;

        if (!findLastTextMsg())
            return;

        notifyLastTextMsg();
    }, mChatdClient.mKarereClient->appCtx);
}

void Chat::sendTypingNotification()
{
    sendCommand(Command(OP_BROADCAST) + mChatId + karere::Id::null() +(uint8_t)Command::kBroadcastUserTyping);
}

void Chat::sendStopTypingNotification()
{
    sendCommand(Command(OP_BROADCAST) + mChatId + karere::Id::null() +(uint8_t)Command::kBroadcastUserStopTyping);
}

void Chat::handleBroadcast(karere::Id from, uint8_t type)
{
    if (type == Command::kBroadcastUserTyping)
    {
        CHATID_LOG_DEBUG("recv BROADCAST kBroadcastUserTyping");
        CALL_LISTENER(onUserTyping, from);
    }
    else if (type == Command::kBroadcastUserStopTyping)
    {
        CHATID_LOG_DEBUG("recv BROADCAST kBroadcastUserStopTyping");
        CALL_LISTENER(onUserStopTyping, from);
    }
    else
    {
        CHATID_LOG_WARNING("recv BROADCAST <unknown_type>");
    }
}

void Client::leave(Id chatid)
{
    auto conn = mConnectionForChatId.find(chatid);
    if (conn == mConnectionForChatId.end())
    {
        CHATD_LOG_ERROR("Client::leave: Unknown chat %s", ID_CSTR(chatid));
        return;
    }
    conn->second->mChatIds.erase(chatid);
    mConnectionForChatId.erase(conn);
    mChatForChatId.erase(chatid);
}

IRtcHandler* Client::setRtcHandler(IRtcHandler *handler)
{
    auto old = mRtcHandler;
    mRtcHandler = handler;
    return old;
}

#define RET_ENUM_NAME(name) case OP_##name: return #name

const char* Command::opcodeToStr(uint8_t code)
{
    switch(code)
    {
        RET_ENUM_NAME(KEEPALIVE);
        RET_ENUM_NAME(JOIN);
        RET_ENUM_NAME(OLDMSG);
        RET_ENUM_NAME(NEWMSG);
        RET_ENUM_NAME(MSGUPD);
        RET_ENUM_NAME(SEEN);
        RET_ENUM_NAME(RECEIVED);
        RET_ENUM_NAME(RETENTION);
        RET_ENUM_NAME(HIST);
        RET_ENUM_NAME(RANGE);
        RET_ENUM_NAME(NEWMSGID);
        RET_ENUM_NAME(REJECT);
        RET_ENUM_NAME(BROADCAST);
        RET_ENUM_NAME(HISTDONE);
        RET_ENUM_NAME(NEWKEY);
        RET_ENUM_NAME(NEWKEYID);
        RET_ENUM_NAME(JOINRANGEHIST);
        RET_ENUM_NAME(MSGUPDX);
        RET_ENUM_NAME(MSGID);
        RET_ENUM_NAME(CLIENTID);
        RET_ENUM_NAME(RTMSG_BROADCAST);
        RET_ENUM_NAME(RTMSG_USER);
        RET_ENUM_NAME(RTMSG_ENDPOINT);
        RET_ENUM_NAME(INCALL);
        RET_ENUM_NAME(ENDCALL);
        RET_ENUM_NAME(KEEPALIVEAWAY);
        RET_ENUM_NAME(CALLDATA);
        RET_ENUM_NAME(ECHO);
        RET_ENUM_NAME(ADDREACTION);
        RET_ENUM_NAME(DELREACTION);
        RET_ENUM_NAME(SYNC);
        RET_ENUM_NAME(NEWNODEMSG);
        RET_ENUM_NAME(NODEHIST);
        default: return "(invalid opcode)";
    };
}

const char* Message::statusNames[] =
{
  "Sending", "SendingManual", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

bool Message::hasUrl(const string &text, string &url)
{
    std::string::size_type position = 0;
    std::string partialString;
    while (position < text.size())
    {
        char character = text[position];
        if ((character >= 33 && character <= 126)
                && character != '"'
                && character != '\''
                && character != '\\'
                && character != '<'
                && character != '>'
                && character != '{'
                && character != '}'
                && character != '|')
        {
            partialString.push_back(character);
        }
        else
        {
            if (!partialString.empty())
            {
                removeUnnecessaryFirstCharacters(partialString);
                removeUnnecessaryLastCharacters(partialString);
                if (parseUrl(partialString))
                {
                    url = partialString;
                    return true;
                }
            }

            partialString.clear();
        }

        position ++;
    }

    if (!partialString.empty())
    {
        removeUnnecessaryFirstCharacters(partialString);
        removeUnnecessaryLastCharacters(partialString);
        if (parseUrl(partialString))
        {
            url = partialString;
            return true;
        }
    }

    return false;
}

bool Message::parseUrl(const std::string &url)
{
    if (url.find('.') == std::string::npos)
    {
        return false;
    }

    if (isValidEmail(url))
    {
        return false;
    }

    std::string urlToParse = url;
    std::string::size_type position = urlToParse.find("://");
    if (position != std::string::npos)
    {
        std::regex expresion("^(http://|https://)(.+)");
        if (regex_match(urlToParse, expresion))
        {
            urlToParse = urlToParse.substr(position + 3);
        }
        else
        {
            return false;
        }
    }

    if (urlToParse.find("mega.co.nz/#!") != std::string::npos || urlToParse.find("mega.co.nz/#F!") != std::string::npos ||
            urlToParse.find("mega.nz/#!") != std::string::npos || urlToParse.find("mega.nz/#F!") != std::string::npos)
    {
        return false;
    }

    std::regex regularExpresion("^(WWW.|www.)?[a-z0-9A-Z-._~:/?#@!$&'()*+,;=]+[.][a-zA-Z]{2,5}(:[0-9]{1,5})?([a-z0-9A-Z-._~:/?#@!$&'()*+,;=]*)?$");


    return regex_match(urlToParse, regularExpresion);
}

Chat::SendingItem::SendingItem(uint8_t aOpcode, Message *aMsg, const SetOfIds &aRcpts, uint64_t aRowid)
    : mOpcode(aOpcode), msg(aMsg), recipients(aRcpts), rowid(aRowid)
{

}

Chat::SendingItem::~SendingItem()
{
    delete msg;
    delete msgCmd;
    delete keyCmd;
}

Chat::ManualSendItem::ManualSendItem(Message *aMsg, uint64_t aRowid, uint8_t aOpcode, ManualSendReason aReason)
    :msg(aMsg), rowid(aRowid), opcode(aOpcode), reason(aReason)
{

}

Chat::ManualSendItem::ManualSendItem()
    :msg(nullptr), rowid(0), opcode(0), reason(kManualSendInvalidReason)
{

}

void Message::removeUnnecessaryLastCharacters(string &buf)
{
    if (!buf.empty())
    {
        char lastCharacter = buf.back();
        while (!buf.empty() && (lastCharacter == '.' || lastCharacter == ',' || lastCharacter == ':'
                               || lastCharacter == '?' || lastCharacter == '!' || lastCharacter == ';'))
        {
            buf.erase(buf.size() - 1);

            if (!buf.empty())
            {
                lastCharacter = buf.back();
            }
        }
    }
}

void Message::removeUnnecessaryFirstCharacters(string &buf)
{
    if (!buf.empty())
    {
        char firstCharacter = buf.front();
        while (!buf.empty() && (firstCharacter == '.' || firstCharacter == ',' || firstCharacter == ':'
                               || firstCharacter == '?' || firstCharacter == '!' || firstCharacter == ';'))
        {
            buf.erase(0, 1);

            if (!buf.empty())
            {
                firstCharacter = buf.front();
            }
        }
    }
}

bool Message::isValidEmail(const string &buf)
{
    std::regex regularExpresion("^[a-z0-9A-Z._%+-]+@[a-z0-9A-Z.-]+[.][a-zA-Z]{2,6}");
    return regex_match(buf, regularExpresion);
}

FilteredHistory::FilteredHistory(DbInterface &db, Chat &chat)
    : mDb(&db), mChat(&chat), mListener(NULL)
{
    init();
    CALL_DB_FH(getNodeHistoryInfo, mNewestIdx, mOldestIdxInDb);
    mOldestIdx = (mNewestIdx < 0) ? 0 : mNewestIdx;
}

void FilteredHistory::addMessage(Message &msg, bool isNew, bool isLocal)
{
    if (msg.size()) // protect against deleted node-attachment messages
    {
        msg.type = msg.buf()[1] + Message::Type::kMsgOffset;
        assert(msg.type == Message::Type::kMsgAttachment);
    }

    Id msgid = msg.id();
    if (isNew)
    {
        mBuffer.emplace_front(new Message(msg));
        mIdToMsgMap[msgid] =  mBuffer.begin();
        mNewestIdx++;
        CALL_DB_FH(addMsgToNodeHistory, msg, mNewestIdx);
        CALL_LISTENER_FH(onReceived, mBuffer.front().get(), mNewestIdx);
    }
    else    // from DB or from NODEHIST/HIST
    {
        if (mIdToMsgMap.find(msgid) == mIdToMsgMap.end())  // if it doesn't exist
        {
            mBuffer.emplace_back(isLocal ? &msg : new Message(msg));    // if it's local (from DB), take the ownership
            mIdToMsgMap[msgid] = --mBuffer.end();
            mOldestIdx--;
            if (!isLocal)
            {
                CALL_DB_FH(addMsgToNodeHistory, msg, mOldestIdx);
                mOldestIdxInDb = (mOldestIdx < mOldestIdxInDb) ? mOldestIdx : mOldestIdxInDb;  // avoid update if already in cache
            }

            // I can receive an old message but we don't have to notify because it was not requested by the app
            if (mListener && (mFetchingFromServer || isLocal))
            {
                CALL_LISTENER_FH(onLoaded, mBuffer.back().get(), mOldestIdx);
                mNextMsgToNotify = mBuffer.end();
            }
        }
    }
}

void FilteredHistory::deleteMessage(const Message &msg)
{
    auto it = mIdToMsgMap.find(msg.id());
    if (it != mIdToMsgMap.end())
    {
        // Remove message's content and modify updated field, it is the same that delete a file
        (*it->second)->free();
        (*it->second)->updated = msg.updated;
        (*it->second)->type = msg.type;
        // Only it's necessary notify messages that are loaded in RAM
        CALL_LISTENER_FH(onDeleted, msg.id());
    }

    CALL_DB_FH(deleteMsgFromNodeHistory, msg);
}

void FilteredHistory::truncateHistory(Id id)
{
    if (id.isValid())
    {
        auto it = mIdToMsgMap.find(id);
        if (it != mIdToMsgMap.end())
        {
            // id is a message in the history, we want to remove from the next message until the oldest
            for (auto itLoop = ++it->second; itLoop != mBuffer.end(); itLoop++)
            {
                mIdToMsgMap.erase((*itLoop)->id());

                // if next message to notify was truncated, point to the end of the buffer
                if (itLoop == mNextMsgToNotify)
                {
                    mNextMsgToNotify = mBuffer.end();
                }
            }
            mBuffer.erase(it->second, mBuffer.end());
        }

        CALL_DB_FH(truncateNodeHistory, id);
        CALL_DB_FH(getNodeHistoryInfo, mNewestIdx, mOldestIdxInDb);
        CALL_LISTENER_FH(onTruncated, id);
        mOldestIdx = (mOldestIdx < mOldestIdxInDb) ? mOldestIdxInDb : mOldestIdx;
    }
    else    // full-history truncated or no remaining attachments
    {
        if (!mBuffer.empty())
        {
            CALL_LISTENER_FH(onTruncated, (*mBuffer.begin())->id());
        }

        clear();
    }

    mHaveAllHistory = true;
}

void FilteredHistory::clear()
{
    mBuffer.clear();
    mIdToMsgMap.clear();
    CALL_DB_FH(clearNodeHistory);
    init();
}

HistSource FilteredHistory::getHistory(uint32_t count)
{
    // Get messages from RAM
    if (mNextMsgToNotify != mBuffer.end())
    {
        uint32_t msgsLoadedFromRam = 0;
        auto it = mNextMsgToNotify;
        while ((it != mBuffer.end()) && (msgsLoadedFromRam < count))
        {
            Idx index = mNewestIdx - std::distance(mBuffer.begin(), it);
            CALL_LISTENER_FH(onLoaded,  it->get(), index);
            msgsLoadedFromRam++;

            it++;
            mNextMsgToNotify = it;
        }

        if (msgsLoadedFromRam)
        {
            CALL_LISTENER_FH(onLoaded, NULL, 0); // All messages requested has been returned or no more messages from this source
            return HistSource::kHistSourceRam;
        }
    }

    // Get messages from DB
    if (mOldestIdx > mOldestIdxInDb)  // more messages available in DB
    {
        // First time we want messages from newest. If already have messages, we want to load from the oldest message
        Idx indexValue = mBuffer.empty() ? mNewestIdx : mOldestIdx - 1;

        std::vector<chatd::Message*> messages;
        CALL_DB_FH(fetchDbNodeHistory, indexValue, count, messages);
        if (messages.size())
        {
            for (unsigned int i = 0; i < messages.size(); i++)
            {
                addMessage(*messages[i], false, true);   // takes ownership of Message*
            }

            CALL_LISTENER_FH(onLoaded, NULL, 0);  // All messages requested has been returned or no more messages from this source
            return HistSource::kHistSourceDb;
        }
    }

    // Get messages from Server
    if (!mHaveAllHistory && mChat->isLoggedIn())
    {
        if (!mFetchingFromServer)
        {
            const Message *msgNode = !mBuffer.empty() ? mBuffer.back().get() : NULL;
            const Message *msgText = mChat->oldest();
            Id oldestMsgid = Id::inval();
            if (msgNode && msgText)
            {
                oldestMsgid = (msgNode->ts >= msgText->ts) ? msgNode->id() : msgText->id();
            }
            else if (msgNode)
            {
                oldestMsgid = msgNode->id();
            }
            else if (msgText)
            {
                oldestMsgid = msgText->id();
            }
            // else --> no history at all, use invalid id

            mFetchingFromServer = true;
            mChat->requestNodeHistoryFromServer(oldestMsgid, count);
        }
        return HistSource::kHistSourceServer;
    }

    // No more messages in history, or not logged-in to load more messages from server
    HistSource hist = mHaveAllHistory ? HistSource::kHistSourceNone : HistSource::kHistSourceNotLoggedIn;
    CALL_LISTENER_FH(onLoaded, NULL, 0); // No more messages

    return hist;
}

void FilteredHistory::setHaveAllHistory(bool haveAllHistory)
{
    mHaveAllHistory = haveAllHistory;
    // TODO: persist this variable in DB (chat_vars::have_all_history)
}

void FilteredHistory::setHandler(FilteredHistoryHandler *handler)
{
    if (mListener)
        throw std::runtime_error("App node history handler is already set, remove it first");

    mNextMsgToNotify = mBuffer.begin();
    mListener = handler;
}

void FilteredHistory::unsetHandler()
{
    mListener = NULL;
}

void FilteredHistory::finishFetchingFromServer()
{
    assert(mFetchingFromServer);
    CALL_LISTENER_FH(onLoaded, NULL, 0);
    mFetchingFromServer = false;
}

Message *FilteredHistory::getMessage(Id id)
{
    Message *msg = NULL;
    auto msgItetrator = mIdToMsgMap.find(id);
    if (msgItetrator != mIdToMsgMap.end())
    {
        msg = msgItetrator->second->get();
    }

    return msg;
}

Idx FilteredHistory::getMessageIdx(Id id)
{
    return mDb->getIdxOfMsgidFromNodeHistory(id);
}

void FilteredHistory::init()
{
    mNewestIdx = -1;
    mOldestIdx = 0;
    mOldestIdxInDb = 0;
    mNextMsgToNotify = mBuffer.begin();
    mHaveAllHistory = false;
}

} // end chatd namespace
