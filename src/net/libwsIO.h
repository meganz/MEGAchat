#ifndef libwsIO_h
#define libwsIO_h

#include <libws.h>
#include "websocketsIO.h"
#include <mega/waiter.h>

// Websockets network layer implementatiuon based on libws
class LibwsIO : public WebsocketsIO
{
public:
    LibwsIO();
    virtual ~LibwsIO();
    
    virtual void addevents(::mega::Waiter*, int);

protected:
    bool initialized;
    ws_base_s wscontext;
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client);
};

class LibwsClient : public WebsocketsClientImpl
{
public:
    ws_t mWebSocket = nullptr;

    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason, size_t reason_len, void *arg);
    static void websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg);
    
    LibwsClient(WebsocketsClient *client);
    
    virtual void wsSendMessage(char *msg, uint64_t len);
};

#endif /* libwsIO_h */
