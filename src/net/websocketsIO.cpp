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

DNScache::DNScache(SqliteDb &db)
    : mDb(db)
{

}

void DNScache::addRecordToDb(const std::string &url, int shard)
{
    mDb.query("insert or replace into dns_cache(url, shard, ipv4, ipv6) values(?,?,?,?)", url, shard, "", "");
}

void DNScache::removeRecord(const std::string &host, int shard)
{
    mRecords.erase(host);
    mDb.query("delete from dns_cache where url like '%' || ? || '%' and shard=?", host, shard);
}

const DNScache::DNSrecord* DNScache::setIp(const std::string &host, int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    if (!isMatch(host, ipsv4, ipsv6))
    {
        DNSrecord record;
        record.ipv4 = ipsv4.empty() ? "" : ipsv4.front();
        record.ipv6 = ipsv6.empty() ? "" : ipsv6.front();
        record.resolveTs = time(NULL);
        mDb.query("update dns_cache set ipv4=?, ipv6=? where url like '%' || ? || '%' and shard=?", record.ipv4, record.ipv6, host, shard);
        return &(mRecords[host] = record);
    }
    return nullptr;
}

bool DNScache::setIp(const std::string &host, const std::string &ipv4, const std::string &ipv6)
{
    if (!isMatch(host, ipv4, ipv6))
    {
        DNSrecord record;
        record.ipv4 = ipv4;
        record.ipv6 = ipv6;
        record.resolveTs = time(NULL);
        mRecords[host] = record;
        return true;
    }
    return false;
}


bool DNScache::getIp(const std::string &host, std::string &ipv4, std::string &ipv6)
{
    auto it = mRecords.find(host);
    if (it == mRecords.end())
    {
        return false;
    }

    ipv4 = it->second.ipv4;
    ipv6 = it->second.ipv6;
    return true;
}

void DNScache::connectDone(const std::string &host, const std::string &ip)
{
    auto it = mRecords.find(host);
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

time_t DNScache::age(const std::string &host)
{
    auto it = mRecords.find(host);
    if (it != mRecords.end())
    {
        return it->second.resolveTs;
    }

    return 0;
}

bool DNScache::isMatch(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
{
    bool match = false;
    auto it = mRecords.find(host);
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

bool DNScache::isMatch(const std::string &host, const std::string &ipv4, const std::string &ipv6)
{
    bool match = false;
    auto it = mRecords.find(host);
    if (it != mRecords.end())
    {
        match = (it->second.ipv4 == ipv4) && (it->second.ipv6 == ipv6);
    }

    return match;
}
