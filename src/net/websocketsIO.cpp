#include "net/websocketsIO.h"

WebsocketsIO::WebsocketsIO(::mega::Mutex *mutex, ::mega::MegaApi *megaApi, void *ctx)
    : mApi(*megaApi, ctx, false)
{
    this->mutex = mutex;
    this->appCtx = ctx;
}

WebsocketsIO::~WebsocketsIO()
{
    
}

WebsocketsClientImpl::WebsocketsClientImpl(::mega::Mutex *mutex, WebsocketsClient *client)
{
    this->mutex = mutex;
    this->client = client;
    this->disconnecting = false;
}

WebsocketsClientImpl::~WebsocketsClientImpl()
{

}

class ScopedLock
{
    ::mega::Mutex *m;
    
public:
    ScopedLock(::mega::Mutex *mutex) : m(mutex)
    {
        if (m)
        {    
            m->lock();
        }
    }
    ~ScopedLock()
    {
        if (m)
        {
            m->unlock();
        }
    }
};

void WebsocketsClientImpl::wsConnectCb()
{
    ScopedLock lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Connection established");
    client->wsConnectCb();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    ScopedLock lock(this->mutex);

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
    ScopedLock lock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Received %d bytes", len);
    client->wsHandleMsgCb(data, len);
}

WebsocketsClient::WebsocketsClient()
{
    ctx = NULL;
    thread_id = 0;
}

WebsocketsClient::~WebsocketsClient()
{
    delete ctx;
    ctx = NULL;
}

bool WebsocketsClient::wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void (int, std::string, std::string)> f)
{
    return websocketIO->wsResolveDNS(hostname, f);
}

bool WebsocketsClient::wsConnect(WebsocketsIO *websocketIO, const char *ip, const char *host, int port, const char *path, bool ssl)
{
    thread_id = pthread_self();
    
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

    assert (thread_id == pthread_self());
    
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

    assert (thread_id == pthread_self());
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
    
    assert (thread_id == pthread_self());
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

bool DNScache::set(std::string &url, std::string &ipv4, std::string &ipv6)
{
    auto it = mRecords.find(url);
    bool exists = (it != mRecords.end());

    bool match = (exists
                  && (ipv4 == it->second.ipv4)
                  && (ipv6 == it->second.ipv6));

    if (!match)
    {
        DNSrecord record;
        record.ipv4 = ipv4;
        record.ipv6 = ipv6;
        record.resolveTs = time(NULL);

        mRecords[url] = record;
    }

    return match;
}

void DNScache::clear(std::string &url)
{
    mRecords.erase(url);
}

bool DNScache::get(std::string &url, std::string &ipv4, std::string &ipv6)
{
    auto it = mRecords.find(url);
    if (it == mRecords.end())
    {
        return false;
    }

    ipv4 = it->second.ipv4;
    ipv6 = it->second.ipv6;

    return true;
}

void DNScache::connectDone(std::string &url, std::string &ip)
{
    auto it = mRecords.find(url);
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
