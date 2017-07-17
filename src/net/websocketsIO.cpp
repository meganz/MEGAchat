#include "websocketsIO.h"

WebsocketsIO::WebsocketsIO(::mega::Mutex *mutex)
{
    this->mutex = mutex;
}

WebsocketsIO::~WebsocketsIO()
{
    
}

WebsocketsClientImpl::WebsocketsClientImpl(::mega::Mutex *mutex, WebsocketsClient *client)
{
    this->mutex = mutex;
    this->client = client;
}

void WebsocketsClientImpl::wsConnectCb()
{
    mutex->lock();
    client->wsConnectCb();
    mutex->unlock();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    mutex->lock();
    client->wsCloseCb(errcode, errtype, preason, reason_len);
    mutex->unlock();
}

void WebsocketsClientImpl::wsHandleMsgCb(char *data, uint64_t len)
{
    mutex->lock();
    client->wsHandleMsgCb(data, len);
    mutex->unlock();
}

WebsocketsClient::WebsocketsClient()
{
    ctx = NULL;
}

bool WebsocketsClient::wsConnect(WebsocketsIO *websocketIO, const char *ip, const char *host, int port, const char *path, bool ssl)
{
    ctx = websocketIO->wsConnect(ip, host, port, path, ssl, this);
    return ctx != NULL;
}

void WebsocketsClient::wsSendMessage(char *msg, uint64_t len)
{
    ctx->wsSendMessage(msg, len);
}
