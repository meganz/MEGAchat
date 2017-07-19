#include "net/websocketsIO.h"

WebsocketsIO::WebsocketsIO(::mega::Mutex *mutex, void *ctx)
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
}

class ScopedLock
{
    ::mega::Mutex *m;
    
public:
    ScopedLock(::mega::Mutex *mutex) : m(mutex)
    {
        m->lock();
    }
    ~ScopedLock()
    {
        m->unlock();
    }
};

void WebsocketsClientImpl::wsConnectCb()
{
    ScopedLock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Connection established");
    client->wsConnectCb();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    ScopedLock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Connection closed");
    client->wsCloseCb(errcode, errtype, preason, reason_len);
}

void WebsocketsClientImpl::wsHandleMsgCb(char *data, size_t len)
{
    ScopedLock(this->mutex);
    WEBSOCKETS_LOG_DEBUG("Received %d bytes", len);
    client->wsHandleMsgCb(data, len);
}

WebsocketsClient::WebsocketsClient()
{
    ctx = NULL;
    thread_id = 0;
}

bool WebsocketsClient::wsConnect(WebsocketsIO *websocketIO, const char *ip, const char *host, int port, const char *path, bool ssl)
{
    thread_id = pthread_self();
    
    WEBSOCKETS_LOG_DEBUG("Connecting to %s (%s)  port %d  path: %s   ssl: %d", host, ip, port, path, ssl);
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

