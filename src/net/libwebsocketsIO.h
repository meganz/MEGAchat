#ifndef libwebsocketsIO_h
#define libwebsocketsIO_h

#include <libwebsockets.h>
#include <openssl/ssl.h>
#include <iostream>
#include <functional>

#include "net/websocketsIO.h"

// Websockets network layer implementation based on libwebsocket
class LibwebsocketsIO : public WebsocketsIO
{
public:
    struct lws_context *wscontext;
    uv_loop_t* eventloop;

    LibwebsocketsIO(Mutex &mutex, ::mega::Waiter* waiter, ::mega::MegaApi *api, void *ctx);
    virtual ~LibwebsocketsIO();
    
    void addevents(::mega::Waiter*, int) override;
    
protected:
    bool wsResolveDNS(const char *hostname, std::function<void(int, const std::vector<std::string>&, const std::vector<std::string>&)> f) override;
    WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client) override;
    int wsGetNoNameErrorCode() override;
};

class LibwebsocketsClient : public WebsocketsClientImpl
{
public:
    LibwebsocketsClient(WebsocketsIO::Mutex &mutex, WebsocketsClient *client);
    virtual ~LibwebsocketsClient();
    
protected:
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
    struct lws *wsi;
    static int wsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t len);
};


#endif /* libwebsocketsIO_h */
