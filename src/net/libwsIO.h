#ifndef libwsIO_h
#define libwsIO_h

#include <libws.h>
#include "net/websocketsIO.h"
#include "trackDelete.h"
#include <mega/waiter.h>
#include <functional>

// Websockets network layer implementation based on libws
class LibwsIO : public WebsocketsIO
{
public:
    LibwsIO(::mega::Mutex *mutex, ::mega::Waiter* waiter, ::mega::MegaApi *api, void *ctx = NULL);
    virtual ~LibwsIO();
    
    virtual void addevents(::mega::Waiter*, int);
    virtual std::string getCachedIpFromUrl(const std::string &url, int ipversion);
    virtual void addCachedIpFromUrl(const std::string &url, const std::string &ip, int ipversion);
    virtual void removeCachedIpFromUrl(const std::string &url, int ipversion);
    virtual void cleanCachedIp();
    virtual int64_t getTimestamp();
    virtual void setTimestamp(int64_t ts);

protected:
    ws_base_s wscontext;
    virtual bool wsResolveDNS(const char *hostname, std::function<void(int, std::string, std::string)> f);
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client);
};

class LibwsClient : public WebsocketsClientImpl, public karere::DeleteTrackable
{
public:
    ws_t mWebSocket;
    void *appCtx;

    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason, size_t reason_len, void *arg);
    static void websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg);
    
    LibwsClient(::mega::Mutex *mutex, WebsocketsClient *client, void *ctx);
    virtual ~LibwsClient();
    
    virtual bool wsSendMessage(char *msg, size_t len);
    virtual void wsDisconnect(bool immediate);
    virtual bool wsIsConnected();
};

#endif /* libwsIO_h */
