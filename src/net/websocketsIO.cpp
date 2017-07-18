#include "websocketsIO.h"

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
    client->wsConnectCb();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    ScopedLock(this->mutex);
    client->wsCloseCb(errcode, errtype, preason, reason_len);
}

void WebsocketsClientImpl::wsHandleMsgCb(char *data, uint64_t len)
{
    ScopedLock(this->mutex);
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
    ctx = websocketIO->wsConnect(ip, host, port, path, ssl, this);
    return ctx != NULL;
}

bool WebsocketsClient::wsSendMessage(char *msg, uint64_t len)
{
    assert (ctx);
    if (!ctx)
    {
        return false;
    }

    assert (thread_id == pthread_self());
    return ctx->wsSendMessage(msg, len);
}

void WebsocketsClient::wsDisconnect(bool immediate)
{
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

