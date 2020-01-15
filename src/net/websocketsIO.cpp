#include "net/websocketsIO.h"

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

WebsocketsClient::WebsocketsClient()
{
    ctx = NULL;
#if !defined(_WIN32) || !defined(_MSC_VER)
    thread_id = 0;
#endif
}

WebsocketsClient::~WebsocketsClient()
{
    delete ctx;
    ctx = NULL;
}

bool WebsocketsClient::wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void (int, std::vector<std::string>&, std::vector<std::string>&)> f)
{
    return websocketIO->wsResolveDNS(hostname, f);
}

bool WebsocketsClient::wsConnect(WebsocketsIO *websocketIO, const char *ip, const char *host, int port, const char *path, bool ssl)
{
#if defined(_WIN32) && defined(_MSC_VER)
    thread_id = std::this_thread::get_id();
#else
    thread_id = pthread_self();
#endif

    WEBSOCKETS_LOG_DEBUG("Connecting to %s (%s)  port %d  path: %s   ssl: %d", host, ip, port, path, ssl);

    assert(!ctx);
    if (ctx)
    {
        WEBSOCKETS_LOG_ERROR("Valid context at connect()");
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

#if defined(_WIN32) && defined(_MSC_VER)
    assert(thread_id == std::this_thread::get_id());
#else
    assert(thread_id == pthread_self());
#endif
    
    
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

#if defined(_WIN32) && defined(_MSC_VER)
    assert(thread_id == std::this_thread::get_id());
#else
    assert(thread_id == pthread_self());
#endif
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
    
#if defined(_WIN32) && defined(_MSC_VER)
    assert(thread_id == std::this_thread::get_id());
#else
    assert(thread_id == pthread_self());
#endif

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

DNScache::DNScache(SqliteDb &db, int chatdVersion)
    : mDb(db),
      mChatdVersion(chatdVersion)
{

}

void DNScache::addRecord(int shard, const std::string &url, bool saveToDb)
{    
    if (hasRecord(shard))
    {
        return;
    }

    // Parse Url to construct a karere::Url
    ::karere::Url auxurl;
    auxurl.parse(url);
    if (shard >= 0) // only chatd needs to append the protocol version
    {
        auxurl.path.append("/").append(std::to_string(mChatdVersion));
    }

    DNSrecord record;
    record.mUrl = auxurl;
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

void DNScache::loadFromDb()
{
    SqliteStmt stmt(mDb, "select shard, url, ipv4, ipv6 from dns_cache");
    while (stmt.step())
    {
        int shard = stmt.intCol(0);
        std::string url = stmt.stringCol(1);
        if (url.size())
        {
            // if the record is for chatd, need to add the protocol version to the URL
            addRecord(shard, url, false);
            setIp(shard, stmt.stringCol(2), stmt.stringCol(3));
        }
        else
        {
            assert(false);  // there shouldn't be emtpy urls in cache
            mDb.query("delete from dns_cache where shard=?", shard);
        }
    }
}

bool DNScache::setIp(int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    if (!isMatch(shard, ipsv4, ipsv6))
    {
        auto it = mRecords.find(shard);
        assert (it != mRecords.end());
        it->second.ipv4 = ipsv4.empty() ? "" : ipsv4.front();
        it->second.ipv6 = ipsv6.empty() ? "" : ipsv6.front();
        it->second.resolveTs = time(NULL);
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
        it->second.resolveTs = time(NULL);
        mDb.query("update dns_cache set ipv4=?, ipv6=? where shard=?", ipv4, ipv6, shard);
        return true;
    }
    return false;
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
    return true;
}

void DNScache::connectDone(int shard, const std::string &ip)
{
    auto it = mRecords.find(shard);
    if (it != mRecords.end())
    {
        if (ip == it->second.ipv4)
        {
            it->second.connectIpv4Ts = time(NULL);
        }
        else if (ip == it->second.ipv6)
        {
            it->second.connectIpv6Ts = time(NULL);
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
