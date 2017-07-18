#ifndef websocketsIO_h
#define websocketsIO_h

#include <iostream>
#include <mega/waiter.h>
#include <mega/thread.h>

class WebsocketsClient;
class WebsocketsClientImpl;

// Generic websockets network layer
class WebsocketsIO : public mega::EventTrigger
{
public:
    WebsocketsIO(::mega::Mutex *mutex, void *ctx);
    virtual ~WebsocketsIO();
    
protected:
    ::mega::Mutex *mutex;
    void *appCtx;
    
    // This function is protected to prevent a wrong direct usage
    // It must be only used from WebsocketClient
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
    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl);
    bool wsSendMessage(char *msg, uint64_t len);
    void wsDisconnect(bool immediate);
    bool wsIsConnected();
    
    virtual void wsConnectCb() = 0;
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len) = 0;
    virtual void wsHandleMsgCb(char *data, uint64_t len) = 0;
};


class WebsocketsClientImpl
{
protected:
    WebsocketsClient *client;
    ::mega::Mutex *mutex;
    
public:
    WebsocketsClientImpl(::mega::Mutex *mutex, WebsocketsClient *client);
    void wsConnectCb();
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len);
    void wsHandleMsgCb(char *data, uint64_t len);
    
    virtual bool wsSendMessage(char *msg, uint64_t len) = 0;
    virtual void wsDisconnect(bool immediate) = 0;
    virtual bool wsIsConnected() = 0;
};

#endif /* websocketsIO_h */
