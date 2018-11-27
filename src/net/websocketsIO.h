#ifndef websocketsIO_h
#define websocketsIO_h

#include <iostream>
#include <functional>
#include <vector>
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

class DNScache
{
public:
    DNScache() {}
    // returns false if ipv4 and ipv6 for the given url already match the ones in cache, true if not (so they are updated)
    bool set(const std::string &url, const std::string &ipv4, const std::string &ipv6);
    void clear(const std::string &url);
    // returns true if hit in cache, false if there's no record for the given url
    bool get(const std::string &url, std::string &ipv4, std::string &ipv6);
    void connectDone(const std::string &url, const std::string &ip);
    time_t age(const std::string &url);
    bool isMatch(const std::string &url, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    bool isMatch(const std::string &url, const std::string &ipv4, const std::string &ipv6);
private:
    struct DNSrecord
    {
        std::string ipv4;
        std::string ipv6;
        time_t resolveTs = 0;       // can be used to invalidate IP addresses by age
        time_t connectIpv4Ts = 0;   // can be used for heuristics based on last successful connection
        time_t connectIpv6Ts = 0;   // can be used for heuristics based on last successful connection
    };

    std::map<std::string, DNSrecord> mRecords;
};

// Generic websockets network layer
class WebsocketsIO : public ::mega::EventTrigger
{
public:
    WebsocketsIO(::mega::Mutex *mutex, ::mega::MegaApi *megaApi, void *ctx);
    virtual ~WebsocketsIO();

    DNScache mDnsCache;
    
protected:
    ::mega::Mutex *mutex;
    MyMegaApi mApi;
    void *appCtx;
    
    // This function is protected to prevent a wrong direct usage
    // It must be only used from WebsocketClient
    virtual bool wsResolveDNS(const char *hostname, std::function<void(int status, std::vector<std::string> &ipsv4, std::vector<std::string> &ipsv6)> f) = 0;
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
#if defined(_WIN32) && defined(_MSC_VER)
    std::thread::id thread_id;
#else
    pthread_t thread_id;
#endif

public:
    WebsocketsClient();
    virtual ~WebsocketsClient();
    bool wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void(int, std::vector<std::string>&, std::vector<std::string>&)> f);
    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl);
    bool wsSendMessage(char *msg, size_t len);  // returns true on success, false if error
    void wsDisconnect(bool immediate);
    bool wsIsConnected();
    void wsCloseCbPrivate(int errcode, int errtype, const char *preason, size_t reason_len);

    virtual void wsConnectCb() = 0;
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len) = 0;
    virtual void wsHandleMsgCb(char *data, size_t len) = 0;
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
