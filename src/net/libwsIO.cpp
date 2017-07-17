#include "libwsIO.h"
#include <arpa/inet.h>
#include <libws_log.h>

#include "waiter/libeventWaiter.h"

using namespace std;

LibwsIO::LibwsIO(::mega::Mutex *mutex) : WebsocketsIO(mutex)
{
    initialized = false;
}

LibwsIO::~LibwsIO()
{
    //FIXME: Free wscontext
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
        
        ws_global_init(&wscontext, libeventWaiter->eventloop, NULL, [](struct bufferevent* bev, void* userp)
                       {
                           //CHATD_LOG_DEBUG("Read event");
                           ws_read_callback(bev, userp);
                       },
                       [](struct bufferevent* bev, short events, void* userp)
                       {
                           //CHATD_LOG_DEBUG("Buffer event 0x%x", events);
                           ws_event_callback(bev, events, userp);
                       },
                       [](int fd, short events, void* userp)
                       {
                           //CHATD_LOG_DEBUG("Timer %p event", userp);
                           ws_handle_marshall_timer_cb(0, events, userp);
                       });
        //ws_set_log_cb(ws_default_log_cb);
        //ws_set_log_level(LIBWS_TRACE);
        
        initialized = true;
    }
}

WebsocketsClientImpl *LibwsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    LibwsClient *libwsClient = new LibwsClient(mutex, client);
    
    ws_init(&libwsClient->mWebSocket, &wscontext);
    ws_set_onconnect_cb(libwsClient->mWebSocket, &LibwsClient::websockConnectCb, libwsClient);
    ws_set_onclose_cb(libwsClient->mWebSocket, &LibwsClient::websockCloseCb, libwsClient);
    ws_set_onmsg_cb(libwsClient->mWebSocket, &LibwsClient::websockMsgCb, libwsClient);
    
    if (ssl)
    {
        ws_set_ssl_state(libwsClient->mWebSocket, LIBWS_SSL_SELFSIGNED);
    }
    
    if (ip[0] == '[')
    {
        string ipv6 = ip;
        struct sockaddr_in6 ipv6addr = { 0 };
        ipv6 = ipv6.substr(1, ipv6.size() - 2);
        ipv6addr.sin6_family = AF_INET6;
        ipv6addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ipv6.c_str(), &ipv6addr.sin6_addr);
        ws_connect_addr(libwsClient->mWebSocket, host,
                        (struct sockaddr *)&ipv6addr, sizeof(ipv6addr),
                        port, path);
    }
    else
    {
        struct sockaddr_in ipv4addr = { 0 };
        ipv4addr.sin_family = AF_INET;
        ipv4addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &ipv4addr.sin_addr);
        ws_connect_addr(libwsClient->mWebSocket, host,
                        (struct sockaddr *)&ipv4addr, sizeof(ipv4addr),
                        port, path);
    }
    
    return libwsClient;
}

LibwsClient::LibwsClient(::mega::Mutex *mutex, WebsocketsClient *client) : WebsocketsClientImpl(mutex, client)
{
    
}

void LibwsClient::websockConnectCb(ws_t ws, void* arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    self->wsConnectCb();
}

void LibwsClient::websockCloseCb(ws_t ws, int errcode, int errtype, const char *preason, size_t reason_len, void *arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    std::string reason;
    if (preason)
        reason.assign(preason, reason_len);

    self->wsCloseCb(errcode, errtype, reason.data(), reason.size());
}

void LibwsClient::websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg)
{
    LibwsClient* self = static_cast<LibwsClient*>(arg);
    string data;
    data.assign(msg, len);
    
    self->wsHandleMsgCb((char *)data.data(), data.size());
}
                         
void LibwsClient::wsSendMessage(char *msg, uint64_t len)
{
    ws_send_msg_ex(mWebSocket, msg, len, 1);
}
