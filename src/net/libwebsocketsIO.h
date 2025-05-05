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
    std::thread::id mLwsContextThread;
    uv_loop_t* eventloop;

public:
    LibwebsocketsIO(Mutex &mutex, ::mega::Waiter* waiter, ::mega::MegaApi *api, void *ctx);
    ~LibwebsocketsIO() override;

    void verifyLwsContextThread() const;

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
    void verifyLwsThread() const;
    std::thread::id mLwsThread;
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
    /**
     * @brief Pointer to the WebSocket connection.
     *
     * @important The lifetime of this structure is controlled internally by lws. `wsi` is valid
     * after calling `connectViaClientInfo` and is invalidated when `wsCallback` is called with
     * `LWS_CALLBACK_CLIENT_CLOSED` or `LWS_CALLBACK_CLIENT_CONNECTION_ERROR`.
     */
    struct lws* wsi;

    /**
     * @brief Indicates that this connection (`wsi`) is being closed gracefully.
     */
    bool disconnecting;

    /**
     * @brief Implementation of the `wsDisconnect` virtual method. Used to close the lws connection
     * (invalidate `wsi`). This can be done in two ways:
     * - Immediately (`immediate == true`): Assumes the current connection (`wsi`) is invalid as
     * soon as the method is called, so no more operations must be performed on it. This is done by
     * setting `wsi` to `nullptr` and the userdata in the lws* (internally managed by lws) also to
     * `NULL`. Further callbacks on this lws struct will exit early by checking the user pointer.
     * - Gracefully (`immediate == false`): In this case, the object remains valid until lws
     * confirms the `wsi` has been invalidated (when the corresponding callback is received). This
     * sets `disconnecting` to true, which is reset to false once the connection is closed.
     *
     * @note Reminder: the protocol to close a connection follows these steps:
     * 1. Call `lws_cancel_service`: This is the only thread-safe interface, callable outside
     * `wsCallback`. It triggers a callback of type `LWS_CALLBACK_EVENT_WAIT_CANCELLED`.
     * 2. Inside `callback[LWS_CALLBACK_EVENT_WAIT_CANCELLED]`, queue a
     *    `callback[LWS_CALLBACK_CLIENT_WRITEABLE]`.
     * 3. Inside `callback[LWS_CALLBACK_CLIENT_WRITEABLE]`, return `-1`. This tells lws to close the
     *    connection, and a `callback[LWS_CALLBACK_CLIENT_CLOSED]` will be received.
     * 4. Once `callback[LWS_CALLBACK_CLIENT_CLOSED]` is received, the connection can no longer be
     * used.
     * @param immediate See above
     */
    void doWsDisconnect(bool immediate);

    /**
     * @brief Private implementation of `wsIsConnected`, defined to avoid calling a virtual method
     * inside the destructor.
     */
    bool isConnected() const;

    /**
     * @brief Sets the user data of the `wsi` member to `nullptr` and then sets `wsi` to `nullptr`.
     */
    void removeConnection();

    /**
     * @brief These static members avoid calling `lws_callback_on_writable` outside `wsCallback` by
     * using `lws_cancel_service` instead (which requires the context as an argument). We use this
     * `std::set` as a queue for connections that need to call `lws_callback_on_writable` once the
     * `LWS_CALLBACK_EVENT_WAIT_CANCELLED` callback is received.
     *
     * The mutex protects access to the set.
     */
    static std::mutex accessDisconnectingWsiMtx;
    static std::set<struct lws*> disconnectingWsiSet;

    /**
     * @brief Adds the current `wsi` to the `disconnectingWsiSet`.
     */
    void markAsDisconnecting();

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
