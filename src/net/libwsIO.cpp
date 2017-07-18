#include "libwsIO.h"
#include <arpa/inet.h>
#include <libws_log.h>
#include <gcmpp.h>

#include "waiter/libeventWaiter.h"

using namespace std;

LibwsIO::LibwsIO(::mega::Mutex *mutex, void *ctx) : WebsocketsIO(mutex, ctx)
{
    initialized = false;
}

LibwsIO::~LibwsIO()
{
    // ws_global_destroy() is not consistent with ws_global_init()
    // ws_global_init() expect wscontext to be externally allocated, but
    // ws_global_destroy() expect to delete it using free()
}

void LibwsIO::addevents(::mega::Waiter* waiter, int)
{
    if (!initialized)
    {
        ::mega::LibeventWaiter *libeventWaiter = dynamic_cast<::mega::LibeventWaiter *>(waiter);
        if (!libeventWaiter)
        {
            // Currently,LibwsIO is not compatible with waiters different than LibeventWaiter
            exit(0);
        }
        
        ws_global_init(&wscontext, libeventWaiter->eventloop, NULL,
        [](struct bufferevent* bev, void* userp)
        {
            ws_read_callback(bev, userp);
        },
        [](struct bufferevent* bev, short events, void* userp)
        {
            ws_event_callback(bev, events, userp);
        },
        [](int fd, short events, void* userp)
        {
            ws_handle_marshall_timer_cb(0, events, userp);
        });
        //ws_set_log_level(LIBWS_TRACE);
        initialized = true;
    }
}

WebsocketsClientImpl *LibwsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    int result;
    LibwsClient *libwsClient = new LibwsClient(mutex, client, appCtx);
    
    result = ws_init(&libwsClient->mWebSocket, &wscontext);
    if (result)
    {
        delete libwsClient;
        return NULL;
    }
    
    ws_set_onconnect_cb(libwsClient->mWebSocket, &LibwsClient::websockConnectCb, libwsClient);
    ws_set_onclose_cb(libwsClient->mWebSocket, &LibwsClient::websockCloseCb, libwsClient);
    ws_set_onmsg_cb(libwsClient->mWebSocket, &LibwsClient::websockMsgCb, libwsClient);
    
    if (ssl)
    {
        ws_set_ssl_state(libwsClient->mWebSocket, LIBWS_SSL_ON);
    }
    
    if (ip[0] == '[')
    {
        string ipv6 = ip;
        struct sockaddr_in6 ipv6addr = { 0 };
        ipv6 = ipv6.substr(1, ipv6.size() - 2);
        ipv6addr.sin6_family = AF_INET6;
        ipv6addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ipv6.c_str(), &ipv6addr.sin6_addr);
        result = ws_connect_addr(libwsClient->mWebSocket, host,
                        (struct sockaddr *)&ipv6addr, sizeof(ipv6addr),
                        port, path);
    }
    else
    {
        struct sockaddr_in ipv4addr = { 0 };
        ipv4addr.sin_family = AF_INET;
        ipv4addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &ipv4addr.sin_addr);
        result = ws_connect_addr(libwsClient->mWebSocket, host,
                        (struct sockaddr *)&ipv4addr, sizeof(ipv4addr),
                        port, path);
    }
    
    if (result)
    {
        delete libwsClient;
        return NULL;
    }
    return libwsClient;
}

LibwsClient::LibwsClient(::mega::Mutex *mutex, WebsocketsClient *client, void *ctx) : WebsocketsClientImpl(mutex, client)
{
    this->appCtx = ctx;
}

LibwsClient::~LibwsClient()
{
    wsDisconnect(true);
}

void LibwsClient::websockConnectCb(ws_t ws, void* arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    assert (ws == self->mWebSocket);

    auto wptr = self->getDelTracker();
    karere::marshallCall([self, wptr]()
    {
        if (wptr.deleted())
            return;

        self->wsConnectCb();
    }, self->appCtx);
}

void LibwsClient::websockCloseCb(ws_t ws, int errcode, int errtype, const char *preason, size_t reason_len, void *arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    assert (ws == self->mWebSocket);

    std::string reason;
    if (preason)
        reason.assign(preason, reason_len);

    auto wptr = self->getDelTracker();
    karere::marshallCall([self, wptr, reason, errcode, errtype]()
    {
        if (wptr.deleted())
            return;
        
        if (self->mWebSocket)
        {
            ws_destroy(&self->mWebSocket);
        }
        
        self->wsCloseCb(errcode, errtype, reason.data(), reason.size());
    }, self->appCtx);
}

void LibwsClient::websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    assert (ws == self->mWebSocket);

    string data;
    data.assign(msg, (size_t)len);
    
    auto wptr = self->getDelTracker();
    karere::marshallCall([self, wptr, data]()
    {
        if (wptr.deleted())
            return;
        
        self->wsHandleMsgCb((char *)data.data(), data.size());
    }, self->appCtx);
}
                         
bool LibwsClient::wsSendMessage(char *msg, uint64_t len)
{
    assert (mWebSocket);
    
    if (!mWebSocket)
    {
        return false;
    }
    
    return !ws_send_msg_ex(mWebSocket, msg, len, 1);
}

void LibwsClient::wsDisconnect(bool immediate)
{
    if (!mWebSocket)
    {
        return;
    }
    
    if (immediate)
    {
        ws_close_immediately(mWebSocket);
        ws_destroy(&mWebSocket);
        assert(!mWebSocket);
    }
    else
    {
        ws_close(mWebSocket);
    }
}

bool LibwsClient::wsIsConnected()
{
    return mWebSocket;
}

