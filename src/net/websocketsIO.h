#ifndef websocketsIO_h
#define websocketsIO_h

#include <iostream>
#include <mega/waiter.h>

class WebsocketsClient;
class WebsocketsClientImpl;

// Generic websockets network layer
class WebsocketsIO : public mega::EventTrigger
{
public:
    WebsocketsIO();
    virtual ~WebsocketsIO();
    
protected:
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
    
public:
    WebsocketsClient();
    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl);
    void wsSendMessage(char *msg, uint64_t len);
    
    virtual void wsConnectCb() = 0;
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len) = 0;
    virtual void wsHandleMsgCb(char *data, uint64_t len) = 0;
};


class WebsocketsClientImpl
{
protected:
    WebsocketsClient *client;
    
public:
    WebsocketsClientImpl(WebsocketsClient *client);
    void wsConnectCb();
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len);
    void wsHandleMsgCb(char *data, uint64_t len);
    
    virtual void wsSendMessage(char *msg, uint64_t len) = 0;
};

#endif /* websocketsIO_h */
