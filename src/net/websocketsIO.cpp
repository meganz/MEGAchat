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
    WEBSOCKETS_LOG_DEBUG("Received %d bytes", len);
    client->wsHandleMsgCb(data, len);
}

void WebsocketsClientImpl::wsSendMsgCb(const char *data, size_t len)
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Sent %d bytes", len);
    client->wsSendMsgCb(data, len);
}

void WebsocketsClientImpl::wsProcessNextMsgCb()
{
    client->wsProcessNextMsgCb();
}

bool WebsocketsClientImpl::wsSSLsessionUpdateCb(const CachedSession &sess)
{
    WebsocketsIO::MutexGuard lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("TLS session updated for %s:%d",
                         sess.hostname.c_str(), sess.port);
    return client->wsSSLsessionUpdateCb(sess);
}

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
        websocketIO->mApi.sdk.sendEvent(99010, "A valid previous context existed upon new wsConnect");
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
    
    WEBSOCKETS_LOG_DEBUG("Sending %d bytes", len);
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
    DNSrecord record;
    record.mUrl.parse(url);
    if (shard >= 0) // only chatd needs to append the protocol version
    {
        record.mUrl.path.append("/").append(std::to_string(mChatdVersion));
    }
    if (sess && !sess->empty())
    {
        record.tlsBlob = sess;
    }
    mRecords[shard] = record;

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
    assert(hasRecord(shard));   // The record for this shard should already exist
    assert(!url.empty());

    DNSrecord &record = mRecords[shard];
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

void DNScache::updateCurrentShardForSfuFromDb()
{
    SqliteStmt stmt(mDb, "SELECT MIN(shard) FROM dns_cache WHERE shard <= ? AND shard >= ?");
    stmt << kSfuShardStart << kSfuShardEnd;
    if (stmt.step())
    {
        assert(stmt.intCol(0) <= kSfuShardStart && stmt.intCol(0) >= kSfuShardEnd);
        mCurrentShardForSfu = stmt.intCol(0);
    }
}

void DNScache::loadFromDb()
{
    SqliteStmt stmt(mDb, "select shard, url, ipv4, ipv6, sess_data from dns_cache");
    while (stmt.step())
    {
        int shard = stmt.intCol(0);
        std::string url = stmt.stringCol(1);
        if (url.size())
        {
            // get tls session data
            auto blobBuff = std::make_shared<Buffer>();
            int columnbytes = sqlite3_column_bytes(stmt, 4);
            if (columnbytes)
            {
                blobBuff->reserve(columnbytes);
                stmt.blobCol(4, *blobBuff);
            }

            // if the record is for chatd, need to add the protocol version to the URL
            addRecord(shard, url, blobBuff, false);
            setIp(shard, stmt.stringCol(2), stmt.stringCol(3));
        }
        else
        {
            assert(!url.empty());  // there shouldn't be emtpy urls in cache
            mDb.query("delete from dns_cache where shard=?", shard);
        }
    }

    // retrieve min SFU shard from DB and update mCurrentShardForSfu
    updateCurrentShardForSfuFromDb();
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

bool DNScache::updateTlsSession(const CachedSession &sess)
{
    // find the dns record that corresponds to this session
    for (const auto &i : mRecords)
    {
        const DNSrecord &r = i.second;
        if (r.mUrl.host == sess.hostname && r.mUrl.port == sess.port)
        {
            // update session data for that connection
            if (sess.dropFromStorage())
            {
                mDb.query("update dns_cache set sess_data=? where shard=?", Buffer(), i.first);
            }
            else if (sess.saveToStorage())
            {
                if (!sess.blob)
                    return false;

                mDb.query("update dns_cache set sess_data=? where shard=?", *sess.blob, i.first);
            }

            return true;
        }
    }

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

// DNS cache methods to manage records based on host instead of shard
bool DNScache::addRecordByHost(const std::string &host, std::shared_ptr<Buffer> sess, bool saveToDb, int shard)
{
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

    if (saveToDb)
    {
        if (mCurrentShardForSfu <= kSfuShardEnd)
        {
            /* in case we have reached kSfuShardEnd, we need to reset mCurrentShardForSfu and remove
             * the current record in cache for that shard value
             */
            mCurrentShardForSfu = kSfuShardStart;
            removeRecord(mCurrentShardForSfu);
        }
        DNSrecord record(host, sess); // add record in DNS cache based on host instead full URL
        mRecords[mCurrentShardForSfu] = record;

        /* For every starting/joining meeting attempt, it's mandatory to send mcms/mcmj command to API.
         * In both cases API returns the SFU server URL where we have to connect to, so we don't
         * need to store full URL(URL+path) in dns cache, as we have already stored in memory.
         */
        mDb.query("insert or replace into dns_cache(shard, url) values(?,?)", mCurrentShardForSfu, host);
        mCurrentShardForSfu--; // decrement mCurrentShardForSfu
    }
    else // loading records from DB to rebuilt DNS cache (in RAM) from scratch
    {
        // use shard param as key and don't update mCurrentShardForSfu, this must be done outside this method
        DNSrecord record(host, sess); // add record in DNS cache based on host instead full URL
        mRecords[shard] = record;
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
        /* Important: This is a corner case
         * This method is called twice:
         * 1) Upon DNS resolution succeed but there's not cached IP's for this host yet
         *      - this case it's not problematic
         * 2) Upon DNS resolution succeed, and returned IP's by DNS doesn't match with stored in cache
         *      - In case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
         *        mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones
         *
         *        The case described above could happens, as multiple calls are allowed and all calls shares
         *        the same DNS cache
         *
         * The solution to this corner case, is add the host to DNS cache again.
        */
        DNSCACHE_LOG_WARNING("setIpByHost: host %s not found in DNS cache, that record could be overwritten. Adding it again", host.c_str());
        addRecordByHost(host);
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
