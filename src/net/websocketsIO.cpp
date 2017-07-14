#include "websocketsIO.h"

WebsocketsIO::WebsocketsIO()
{
    
}

WebsocketsIO::~WebsocketsIO()
{
    
}

WebsocketsClientImpl::WebsocketsClientImpl(WebsocketsClient *client)
{
    this->client = client;
}

void WebsocketsClientImpl::wsConnectCb()
{
    client->wsConnectCb();
}

void WebsocketsClientImpl::wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len)
{
    client->wsCloseCb(errcode, errtype, preason, reason_len);
}

void WebsocketsClientImpl::wsHandleMsgCb(char *data, uint64_t len)
{
    client->wsHandleMsgCb(data, len);
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
