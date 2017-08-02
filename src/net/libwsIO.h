#ifndef libwsIO_h
#define libwsIO_h

#include <libws.h>
#include "net/websocketsIO.h"
#include "trackDelete.h"
#include <mega/waiter.h>

// Websockets network layer implementatiuon based on libws
class LibwsIO : public WebsocketsIO
{
public:
    LibwsIO(::mega::Mutex *mutex = NULL, void *ctx = NULL);
    virtual ~LibwsIO();
    
    virtual void addevents(::mega::Waiter*, int);

protected:
    bool initialized;
    ws_base_s wscontext;
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client);
};

class LibwsClient : public WebsocketsClientImpl, public karere::DeleteTrackable
{
public:
    ws_t mWebSocket = nullptr;
    void *appCtx;

    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason, size_t reason_len, void *arg);
    static void websockMsgCb(ws_t ws, char *msg, uint64_t len, int binary, void *arg);
    
    LibwsClient(::mega::Mutex *mutex, WebsocketsClient *client, void *ctx);
    ~LibwsClient();
    
    virtual bool wsSendMessage(char *msg, size_t len);
    virtual void wsDisconnect(bool immediate);
    virtual bool wsIsConnected();
};

#endif /* libwsIO_h */
