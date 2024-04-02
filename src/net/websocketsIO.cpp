#include "net/websocketsIO.h"
#include <mega/utils.h>

bool WebsocketsClient::publicKeyPinning = true; // needs to be defined here

WebsocketsIO::WebsocketsIO(Mutex &m, ::mega::MegaApi *megaApi, void *ctx)
    : mApi(*megaApi, ctx, false), mutex(m)
{
    this->appCtx = ctx;
}

WebsocketsIO::~WebsocketsIO()
{
    
}

WebsocketsClientImpl::WebsocketsClientImpl(WebsocketsIO::Mutex &m, WebsocketsClient *client)
    : mutex(m)
{
    this->client = client;
    this->disconnecting = false;
}

WebsocketsClientImpl::~WebsocketsClientImpl()
{

}

void WebsocketsClientImpl::wsConnectCb()
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Connection established");
    client->wsConnectCb();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    WebsocketsIO::MutexGuard lock(this->mutex);

    if (disconnecting)
    {
        WEBSOCKETS_LOG_DEBUG("Connection closed gracefully");
    }
    else
    {
        WEBSOCKETS_LOG_DEBUG("Connection closed by server");
    }

    client->wsCloseCbPrivate(errcode, errtype, preason, reason_len);
}

void WebsocketsClientImpl::wsHandleMsgCb(char *data, size_t len)
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Received %lu bytes", len);
    client->wsHandleMsgCb(data, len);
}

void WebsocketsClientImpl::wsSendMsgCb(const char *data, size_t len)
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Sent %lu bytes", len);
    client->wsSendMsgCb(data, len);
}

void WebsocketsClientImpl::wsProcessNextMsgCb()
{
    client->wsProcessNextMsgCb();
}

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
bool WebsocketsClientImpl::wsSSLsessionUpdateCb(const CachedSession &sess)
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    return client->wsSSLsessionUpdateCb(sess);
}
#endif

WebsocketsClient::WebsocketsClient(bool writeBinary)
    : ctx(nullptr)
    , mWriteBinary(writeBinary)
{ }

WebsocketsClient::~WebsocketsClient()
{
    delete ctx;
    ctx = NULL;
}

bool WebsocketsClient::wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void (int, const std::vector<std::string>&, const std::vector<std::string>&)> f)
{
    return websocketIO->wsResolveDNS(hostname, f);
}

bool WebsocketsClient::wsConnect(WebsocketsIO *websocketIO, const char *ip, const char *host, int port, const char *path, bool ssl)
{
    thread_id = std::this_thread::get_id();

    WEBSOCKETS_LOG_DEBUG("Connecting to %s (%s)  port %d  path: %s   ssl: %d", host, ip, port, path, ssl);

    assert(!ctx);
    if (ctx)
    {
        WEBSOCKETS_LOG_ERROR("Valid context at connect()");
        websocketIO->mApi.sdk.sendEvent(99010, "A valid previous context existed upon new wsConnect", false, static_cast<const char*>(nullptr));
        delete ctx;
    }

    ctx = websocketIO->wsConnect(ip, host, port, path, ssl, this);
    if (!ctx)
    {
        WEBSOCKETS_LOG_WARNING("Immediate error in wsConnect");
    }
    return ctx != NULL;
}

int WebsocketsClient::wsGetNoNameErrorCode(WebsocketsIO *websocketIO)
{
    return websocketIO->wsGetNoNameErrorCode();
}

bool WebsocketsClient::wsSendMessage(char *msg, size_t len)
{
    assert (ctx);
    if (!ctx)
    {
        WEBSOCKETS_LOG_ERROR("Trying to send a message without a previous initialization");
        assert(false);
        return false;
    }

    assert(thread_id == std::this_thread::get_id());    
    
    WEBSOCKETS_LOG_DEBUG("Sending %lu bytes", len);
    bool result = ctx->wsSendMessage(msg, len);
    if (!result)
    {
        WEBSOCKETS_LOG_WARNING("Immediate error in wsSendMessage");
    }
    return result;
}

void WebsocketsClient::wsDisconnect(bool immediate)
{
    WEBSOCKETS_LOG_DEBUG("Disconnecting. Immediate: %d", immediate);
    
    if (!ctx)
    {
        return;
    }

    assert(thread_id == std::this_thread::get_id());

    ctx->wsDisconnect(immediate);

    if (immediate)
    {
        delete ctx;
        ctx = NULL;
    }
}

bool WebsocketsClient::wsIsConnected()
{
    if (!ctx)
    {
        return false;
    }
    
    assert(thread_id == std::this_thread::get_id());

    return ctx->wsIsConnected();
}

void WebsocketsClient::wsCloseCbPrivate(int errcode, int errtype, const char *preason, size_t reason_len)
{
    if (!ctx)   // immediate disconnect ocurred before the marshall is executed (only applies to libws)
    {
        return;
    }

    delete ctx;
    ctx = NULL;

    WEBSOCKETS_LOG_DEBUG("Socket was closed gracefully or by server");

    wsCloseCb(errcode, errtype, preason, reason_len);
}

bool WebsocketsClient::isWriteBinary() const
{
    return mWriteBinary;
}

DNScache::DNScache(SqliteDb &db, int chatdVersion)
    : mDb(db),
      mChatdVersion(chatdVersion),
      mCurrentShardForSfu(kSfuShardStart)
{

}

void DNScache::addRecord(int shard, const std::string &url, std::shared_ptr<Buffer> sess, bool saveToDb)
{    
    if (hasRecord(shard))
    {
        assert(!hasRecord(shard));
        return;
    }

    assert(!url.empty());

    auto ret = mRecords.emplace(std::make_pair(shard, DNSrecord(url))); // add record in DNS cache based on URL
    DNSrecord &record = ret.first->second;
    if (shard >= 0) // only chatd needs to append the protocol version
    {
        record.mUrl.path.append("/").append(std::to_string(mChatdVersion));
    }
    if (sess && !sess->empty())
    {
        record.tlsBlob = sess;
    }

    if (saveToDb)
    {
        mDb.query("insert or replace into dns_cache(shard, url) values(?,?)", shard, url);
    }
}

void DNScache::removeRecord(int shard)
{
    mRecords.erase(shard);
    mDb.query("delete from dns_cache where shard=?", shard);
}

void DNScache::updateRecord(int shard, const std::string &url, bool saveToDb)
{
    if (!hasRecord(shard))
    {
        assert(hasRecord(shard));   // The record for this shard should already exist
        WEBSOCKETS_LOG_ERROR("The record for shard %d should already exist. Adding new one", shard);
        addRecord(shard, url);
    }
    assert(!url.empty());

    DNSrecord &record = mRecords.at(shard);
    record.mUrl.parse(url);
    if (shard >= 0) // only chatd needs to append the protocol version
    {
        record.mUrl.path.append("/").append(std::to_string(mChatdVersion));
    }
    record.ipv4.clear();
    record.ipv6.clear();

    if (saveToDb)
    {
        mDb.query("update dns_cache set url=?, ipv4=?, ipv6=? where shard=?",
                  url, record.ipv4, record.ipv6, shard);
    }
}

bool DNScache::hasRecord(int shard)
{
    return mRecords.find(shard) != mRecords.end();
}

bool DNScache::isValidUrl(int shard)
{
    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        return it->second.mUrl.isValid();
    }

    return false;
}

const karere::Url &DNScache::getUrl(int shard)
{
    auto it = mRecords.find(shard);
    assert(it != mRecords.end());
    return it->second.mUrl;
}

void DNScache::removeRecordsByShards(const std::set<int> &removeElements)
{
    if (removeElements.empty())
    {
        return;
    }
    std::string query ("delete from dns_cache where shard in(");
    std::ostringstream os;
    std::fill_n(std::ostream_iterator<std::string>(os), removeElements.size(), "?,");
    query.append(os.str());
    query.pop_back();     // remove last comma => ?,?,?,?,
    query.push_back(')'); // add close bracket for IN clause
    SqliteStmt auxstmt(mDb, query.c_str());
    for (auto shard: removeElements)
    {
        auxstmt << shard;
    }
    auxstmt.step();
}

void DNScache::loadFromDb()
{
    /* retrieve min SFU shard from DB and update mCurrentShardForSfu. updateCurrentShardForSfuFromDb
     * returns false in case there are no records with a valid SFU shard (check isSfuValidShard()) */
    bool sfuShardUpdated = updateCurrentShardForSfuFromDb();
    SqliteStmt stmt(mDb, "select shard, url, ipv4, ipv6, sess_data from dns_cache");
    std::set<int> removeElements;
    while (stmt.step())
    {
        int shard = stmt.integralCol<int>(0);
        std::string url = stmt.stringCol(1);
        if (url.size())
        {
            // get tls session data
            auto blobBuff = std::make_shared<Buffer>();
            int columnbytes = stmt.getColumnBytes(4);
            if (columnbytes)
            {
                blobBuff->reserve(columnbytes);
                stmt.blobCol(4, *blobBuff);
            }

            if (isSfuValidShard(shard))
            {
                karere::Url sfuUrl(url);
                assert(sfuUrl.isValid());
                if (!sfuUrl.isValid()
                        || !sfuShardUpdated // this case shouldn't happens, but in that case better to remove SFU records
                        || !addSfuRecordWithIp(sfuUrl.host, blobBuff, false, shard, {stmt.stringCol(2)}, {stmt.stringCol(3)}))
                {
                    DNSCACHE_LOG_ERROR("loadFromDb: invalid SFU record");
                    removeElements.insert(shard);
                    continue;
                }
            }
            else
            {
                // if the record is for chatd, need to add the protocol version to the URL
                addRecord(shard, url, blobBuff, false);
                setIp(shard, stmt.stringCol(2), stmt.stringCol(3));
            }
        }
        else
        {
            assert(!url.empty());  // there shouldn't be emtpy urls in cache
            removeElements.insert(shard);
        }
    }

    // remove wrong records with one query
    removeRecordsByShards(removeElements);
}

bool DNScache::setIp(int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    if (!isMatch(shard, ipsv4, ipsv6))
    {
        auto it = mRecords.find(shard);
        assert (it != mRecords.end());
        it->second.ipv4 = ipsv4.empty() ? "" : ipsv4.front();
        it->second.ipv6 = ipsv6.empty() ? "" : ipsv6.front();
        it->second.resolveTs = ::mega::m_time(nullptr);
        mDb.query("update dns_cache set ipv4=?, ipv6=? where shard=?", it->second.ipv4, it->second.ipv6, shard);
        return true;
    }
    return false;
}

bool DNScache::setIp(int shard, std::string ipv4, std::string ipv6)
{
    if (!isMatch(shard, ipv4, ipv6))
    {
        auto it = mRecords.find(shard);
        assert(it != mRecords.end());
        it->second.ipv4 = ipv4;
        it->second.ipv6 = ipv6;
        it->second.resolveTs = ::mega::m_time(nullptr);
        mDb.query("update dns_cache set ipv4=?, ipv6=? where shard=?", ipv4, ipv6, shard);
        return true;
    }
    return false;
}

bool DNScache::invalidateIps(int shard)
{
    return setIp(shard, "", "");
}

bool DNScache::getIp(int shard, std::string &ipv4, std::string &ipv6)
{
    auto it = mRecords.find(shard);
    if (it == mRecords.end())
    {
        return false;
    }

    assert(it->second.mUrl.isValid());
    ipv4 = it->second.ipv4;
    ipv6 = it->second.ipv6;

    // If both Ip's are empty there's no cached ip's
    return ipv4.size() || ipv6.size();
}

void DNScache::connectDone(int shard, const std::string &ip)
{
    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        if (ip == it->second.ipv4)
        {
            it->second.connectIpv4Ts = ::mega::m_time(nullptr);
        }
        else if (ip == it->second.ipv6)
        {
            it->second.connectIpv6Ts = ::mega::m_time(nullptr);
        }
    }
}

time_t DNScache::age(int shard)
{
    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        return it->second.resolveTs;
    }

    return 0;
}

bool DNScache::isMatch(int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    bool match = false;
    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        std::string ipv4 = it->second.ipv4;
        std::string ipv6 = it->second.ipv6;

        match = ( ((ipv4.empty() && ipsv4.empty()) // don't have IPv4, but it wasn't received either
                   || (std::find(ipsv4.begin(), ipsv4.end(), ipv4) != ipsv4.end())) // IPv4 is contained in `ipsv4`
                  && ((ipv6.empty() && ipsv6.empty())
                      || std::find(ipsv6.begin(), ipsv6.end(), ipv6) != ipsv6.end()));
    }

    return match;
}

bool DNScache::isMatch(int shard, const std::string &ipv4, const std::string &ipv6)
{
    bool match = false;

    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        match = (it->second.ipv4 == ipv4) && (it->second.ipv6 == ipv6);
    }

    return match;
}

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
bool DNScache::updateTlsSession(const CachedSession &sess)
{
    // find the dns record that corresponds to this session
    for (const auto &i : mRecords)
    {
        const DNSrecord &r = i.second;
        // Match DNS record by hostname and port, or just by hostname
        // if it's a SFU entry (for which port is irrelevant)
        if (r.mUrl.host == sess.hostname && (r.mUrl.port == sess.port ||
                                             isSfuValidShard(i.first)))
        {
            // update session data for that connection
            if (sess.dropFromStorage())
            {
                mDb.query("update dns_cache set sess_data=? where shard=?", Buffer(), i.first);
                WEBSOCKETS_LOG_DEBUG("TLS session info dropped from persistent storage"
                                     " for %s:%d", sess.hostname.c_str(), sess.port);
            }
            else if (sess.saveToStorage())
            {
                if (!sess.blob)
                {
                    WEBSOCKETS_LOG_ERROR("TLS session blob was missing. Persistent storage not updated"
                                         " for %s:%d", sess.hostname.c_str(), sess.port);
                    return false;
                }

                mDb.query("update dns_cache set sess_data=? where shard=?", *sess.blob, i.first);
                WEBSOCKETS_LOG_DEBUG("TLS session info updated in persistent storage"
                                     " for %s:%d", sess.hostname.c_str(), sess.port);
            }

            return true;
        }
    }

    WEBSOCKETS_LOG_ERROR("TLS session info did not match any entry in persistent storage."
                         " Not updated for %s:%d", sess.hostname.c_str(), sess.port);
    return false;
}

std::vector<CachedSession> DNScache::getTlsSessions()
{
    std::vector<CachedSession> sessions;

    for (auto &i : mRecords)
    {
        DNSrecord &r = i.second;
        if (!r.tlsBlob)  continue;

        CachedSession ts;
        ts.hostname = r.mUrl.host;
        ts.port = r.mUrl.port;
        ts.blob = r.tlsBlob;
        r.tlsBlob = nullptr; // no need to keep a copy here
        sessions.emplace_back(std::move(ts));
    }

    return sessions;
}
#endif // WEBSOCKETS_TLS_SESSION_CACHE_ENABLED

bool DNScache::isSfuValidShard(int shard) const
{
    // note that kSfuShardStart and kSfuShardEnd are negative values
    return shard <= kSfuShardStart && shard >= kSfuShardEnd;
}

int DNScache::calculateNextSfuShard()
{
    if (mCurrentShardForSfu > kSfuShardStart || mCurrentShardForSfu <= kSfuShardEnd)
    {
        /* in case we have reached kSfuShardEnd (or mCurrentShardForSfu > kSfuShardStart), we need
         * to reset mCurrentShardForSfu and remove the current record in cache for that shard value */
        assert(mCurrentShardForSfu <= kSfuShardStart); // this assert should never fail
        mCurrentShardForSfu = kSfuShardStart;
        removeRecord(mCurrentShardForSfu);
    }
    return mCurrentShardForSfu--; // return mCurrentShardForSfu and decrement
}

bool DNScache::updateCurrentShardForSfuFromDb()
{
    mCurrentShardForSfu = kSfuShardStart; // reset default value
    SqliteStmt stmt(mDb, "SELECT MIN(shard) FROM dns_cache WHERE shard <= ? AND shard > ?");
    stmt << kSfuShardStart << kSfuShardEnd;
    if (stmt.step() && !stmt.isNullColumn(0))
    {
        assert(isSfuValidShard(stmt.integralCol<int>(0)));
        mCurrentShardForSfu = stmt.integralCol<int>(0);
        return true;
    }
    return false;
}

bool DNScache::setSfuIp(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    DNSrecord *record = getRecordByHost(host);
    if (!record)
    {
        /* Important: This is a corner case
         * This method is called thee times:
         * 1) Upon load DNS cache records from DB
         *      - this case it's not problematic
         * 2) Upon DNS resolution succeed but there's not cached IP's for this host yet
         *      - this case it's not problematic
         * 3) Upon DNS resolution succeed, and returned IP's by DNS doesn't match with stored in cache
         *      - In case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
         *        mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones
         *
         *        The case described above could happens, as multiple calls are allowed and all calls shares
         *        the same DNS cache
         *
         * The solution to this corner case, is add the host to DNS cache again.
        */
        DNSCACHE_LOG_WARNING("setIpByHost: host %s not found in DNS cache, that record could be overwritten. Adding it again", host.c_str());
        addSfuRecord(host);
    }
    return setIpByHost(host, ipsv4, ipsv6);
}

bool DNScache::addSfuRecord(const std::string &host, std::shared_ptr<Buffer> sess, bool saveToDb, int shard)
{
    if (!saveToDb && !isSfuValidShard(shard))
    {
        assert(saveToDb && isSfuValidShard(shard));
        DNSCACHE_LOG_ERROR("addSfuRecord: invalid shard value");
        return false;
    }

    // if saveToDb is true, we need to calculate next sfu shard value, otherwise use shard param
    int sfuShard = saveToDb ? calculateNextSfuShard() : shard;

    /* For every starting/joining meeting attempt, it's mandatory to send mcms/mcmj command to API.
     * In both cases API returns the SFU server URL where we have to connect to, so we don't
     * need to store full URL(URL+path) in dns cache, as we have already stored in memory.
     */
    return addRecordByHost(host, sess, saveToDb, sfuShard);
}

bool DNScache::addSfuRecordWithIp(const std::string &host, std::shared_ptr<Buffer> sess, bool saveToDb, int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    if (addSfuRecord(host, sess, saveToDb, shard))
    {
        return setIpByHost(host, ipsv4, ipsv6);
    }

    return false;
}

// DNS cache methods to manage records based on host instead of shard
bool DNScache::addRecordByHost(const std::string &host, std::shared_ptr<Buffer> sess, bool saveToDb, int shard)
{
    assert(shard != kInvalidShard);
    if (host.empty())
    {
        assert(!host.empty());
        DNSCACHE_LOG_ERROR("addRecordByHost: empty host");
        return false;
    }

    if (hasRecordByHost(host) && saveToDb)
    {
        assert(!hasRecordByHost(host));
        DNSCACHE_LOG_ERROR("addRecordByHost: we already have a record in DNS cache for that host");
        return false;
    }

    mRecords.emplace(std::make_pair(shard, DNSrecord(host,sess)));    // add record in DNS cache based on host instead full URL
    if (saveToDb)
    {
        mDb.query("insert or replace into dns_cache(shard, url) values(?,?)", shard, host);
    }

    return true;
}

bool DNScache::hasRecordByHost(const std::string &host) const
{
    return const_cast<DNScache*>(this)->getRecordByHost(host) != nullptr;
}

DNScache::DNSrecord* DNScache::getRecordByHost(const std::string &host)
{
    for (auto it = mRecords.begin(); it != mRecords.end(); it++)
    {
        if (it->second.isHostMatch(host))
        {
           return &it->second;
        }
    }
    return nullptr;
}

void DNScache::connectDoneByHost(const std::string &host, const std::string &ip)
{
    DNSrecord *record = getRecordByHost(host);
    if (!record)
    {
        /* in case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
         * mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones */
        return;
    }

    if (ip == record->ipv4)
    {
        record->connectIpv4Ts = ::mega::m_time(nullptr);
    }
    else if (ip == record->ipv6)
    {
        record->connectIpv6Ts = ::mega::m_time(nullptr);
    }
}

bool DNScache::getIpByHost(const std::string &host, std::string &ipv4, std::string &ipv6)
{
    DNSrecord *record = getRecordByHost(host);
    if (!record)
    {
        /* in case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
         * mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones */
        return false;
    }

    ipv4 = record->ipv4;
    ipv6 = record->ipv6;
    return ipv4.size() || ipv6.size();
}

bool DNScache::setIpByHost(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    if (isMatchByHost(host, ipsv4, ipsv6))
    {
        return false; // if there's a match in cache, returns
    }

    DNSrecord *record = getRecordByHost(host);
    if (!record)
    {
        assert(record);
        DNSCACHE_LOG_WARNING("setIpByHost: host %s not found in DNS cache.", host.c_str());
        return false;
    }

    record->ipv4 = ipsv4.empty() ? "" : ipsv4.front();
    record->ipv6 = ipsv6.empty() ? "" : ipsv6.front();
    record->resolveTs = ::mega::m_time(nullptr);
    mDb.query("update dns_cache set ipv4=?, ipv6=? where url=?", record->ipv4, record->ipv6, host);
    return true;
}

bool DNScache::isMatchByHost(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    bool match = false;
    DNSrecord *record = getRecordByHost(host);
    if (record)
    {
        const std::string &ipv4 = record->ipv4;
        const std::string &ipv6 = record->ipv6;

        match = ( ((ipv4.empty() && ipsv4.empty()) // don't have IPv4, but it wasn't received either
                   || (std::find(ipsv4.begin(), ipsv4.end(), ipv4) != ipsv4.end())) // IPv4 is contained in `ipsv4`
                  && ((ipv6.empty() && ipsv6.empty())
                      || std::find(ipsv6.begin(), ipsv6.end(), ipv6) != ipsv6.end()));
    }

    return match;
}
