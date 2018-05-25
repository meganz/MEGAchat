#ifndef websocketsIO_h
#define websocketsIO_h

#include <iostream>
#include <functional>
#include <mega/waiter.h>
#include <mega/thread.h>
#include "base/logger.h"
#include "sdkApi.h"

#define WEBSOCKETS_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_websockets, fmtString, ##__VA_ARGS__)

class WebsocketsClient;
class WebsocketsClientImpl;

// Generic websockets network layer
class WebsocketsIO : public mega::EventTrigger
{
public:
    enum ipVersion { kIpv4 = 0, kIpv6 = 1 };
    WebsocketsIO(::mega::Mutex *mutex, ::mega::MegaApi *megaApi, void *ctx);
    virtual ~WebsocketsIO();
    
protected:
    ::mega::Mutex *mutex;
    MyMegaApi mApi;
    void *appCtx;
    int64_t ts;
    virtual std::string getCachedIpFromUrl(const std::string &url, int ipversion) = 0;
    virtual void addCachedIpFromUrl(const std::string &url, const std::string &ip, int ipVersion) = 0;
    virtual void removeCachedIpFromUrl(const std::string &url, int ipVersion) = 0;
    virtual void cleanCachedIp() = 0;
    virtual int64_t getTimestamp() = 0;
    virtual void setTimestamp(int64_t ts) = 0;

    // This function is protected to prevent a wrong direct usage
    // It must be only used from WebsocketClient
    virtual bool wsResolveDNS(const char *hostname, std::function<void(int status, std::string ipv4, std::string ipv6)> f) = 0;
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client) = 0;
    friend WebsocketsClient;
};


// Abstract class that allows to manage a websocket connection.
// It's needed to subclass this class in order to receive callbacks
class WebsocketsClient
{
private:
    WebsocketsClientImpl *ctx;
    pthread_t thread_id;

public:
    WebsocketsClient();
    virtual ~WebsocketsClient();
    bool wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void(int, std::string, std::string)> f);
    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl);
    bool wsSendMessage(char *msg, size_t len);  // returns true on success, false if error
    void wsDisconnect(bool immediate);
    bool wsIsConnected();
    void wsCloseCbPrivate(int errcode, int errtype, const char *preason, size_t reason_len);
    virtual std::string getCachedIpFromUrl(WebsocketsIO *websocketIO, const std::string &url, int ipversion);
    virtual void addCachedIpFromUrl(WebsocketsIO *websocketIO, const std::string &url, const std::string &ip, int ipVersion);
    virtual void removeCachedIpFromUrl(WebsocketsIO *websocketIO, const std::string &url, int ipVersion);
    virtual void cleanCachedIp(WebsocketsIO *websocketIO);
    virtual void wsConnectCb() = 0;
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len) = 0;
    virtual void wsHandleMsgCb(char *data, size_t len) = 0;
    virtual int64_t getTimestamp(WebsocketsIO *websocketIO);
    virtual void setTimestamp(WebsocketsIO *websocketIO, int64_t ts);
};


class WebsocketsClientImpl
{
protected:
    WebsocketsClient *client;
    ::mega::Mutex *mutex;
    bool disconnecting;
    
public:
    WebsocketsClientImpl(::mega::Mutex *mutex, WebsocketsClient *client);
    virtual ~WebsocketsClientImpl();
    void wsConnectCb();
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len);
    void wsHandleMsgCb(char *data, size_t len);
    
    virtual bool wsSendMessage(char *msg, size_t len) = 0;
    virtual void wsDisconnect(bool immediate) = 0;
    virtual bool wsIsConnected() = 0;
};

#endif /* websocketsIO_h */
