#ifndef libwebsocketsIO_h
#define libwebsocketsIO_h

#include <libwebsockets.h>
#include <openssl/ssl.h>
#include <iostream>
#include <functional>

#include "net/websocketsIO.h"
#include <uv.h>

// Websockets network layer implementation based on libwebsocket
class LibwebsocketsIO : public WebsocketsIO
{
    struct lws_context *wscontext;
    uv_loop_t* eventloop;

public:
    LibwebsocketsIO(Mutex &mutex, ::mega::Waiter* waiter, ::mega::MegaApi *api, void *ctx);
    ~LibwebsocketsIO() override;
    
    void addevents(::mega::Waiter*, int) override;
    
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    bool hasSessionCache() const override { return true; }
    void restoreSessions(std::vector<CachedSession> &&sessions) override;
#endif

private:
    bool wsResolveDNS(const char *hostname, std::function<void(int, const std::vector<std::string>&, const std::vector<std::string>&)> f) override;
    WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client) override;
    int wsGetNoNameErrorCode() override;

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    // Note: While theoretically a LWS context can have multiple vhosts, it's a
    //       feature applicable to servers, and they need to be explicitly created.
    //       Implicitly, as in our case, only the default vhost will be created.
    //
    // Note 2: Unfortunately default vhost's name was not abstracted away in LWS,
    //         and is used verbatim in their sample too...
    static const char constexpr *DEFAULT_VHOST = "default";

    static constexpr int TLS_SESSION_TIMEOUT = 180 * 24 * 3600; // ~6 months, in seconds
#endif
};

class LibwebsocketsClient : public WebsocketsClientImpl
{
public:
    LibwebsocketsClient(WebsocketsIO::Mutex &mutex, WebsocketsClient *client);
    virtual ~LibwebsocketsClient();

    bool connectViaClientInfo(const char *ip, const char *host, int port, const char *path, bool ssl, lws_context *wscontext);

private:
    std::string recbuffer;
    std::string sendbuffer;

    void appendMessageFragment(char *data, size_t len, size_t remaining);
    bool hasFragments();
    const char *getMessage();
    size_t getMessageLength();
    void resetMessage();
    const char *getOutputBuffer();
    size_t getOutputBufferLength();
    void resetOutputBuffer();
    
    bool wsSendMessage(char *msg, size_t len) override;
    void wsDisconnect(bool immediate) override;
    bool wsIsConnected() override;
    
public:
    static int wsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t len);

private:
    struct lws *wsi;
    void doWsDisconnect(bool immediate);
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    void saveTlsSessionToPersistentStorage();
    CachedSession mTlsSession;
#endif
};

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
class LwsCache
{
public:
    static bool dump(lws_vhost *vh, CachedSession *s);
    static bool load(lws_vhost *vh, CachedSession *s);

private:
    static int dumpCb(lws_context *, lws_tls_session_dump *info);
    static int loadCb(lws_context *, lws_tls_session_dump *info);
};
#endif

#endif /* libwebsocketsIO_h */
