#ifndef libwebsocketsIO_h
#define libwebsocketsIO_h

#include <libwebsockets.h>
#include <openssl/ssl.h>
#include <iostream>

#include "websocketsIO.h"

// Websockets network layer implementatiuon based on libwebsocket
class LibwebsocketsIO : public WebsocketsIO
{
public:
    struct lws_context *wscontext;
    LibwebsocketsIO(::mega::Mutex *mutex);
    virtual ~LibwebsocketsIO();
    
    virtual void addevents(::mega::Waiter*, int);
    
protected:
    bool initialized;
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client);
};

class LibwebsocketsClient : public WebsocketsClientImpl
{
public:
    LibwebsocketsClient(::mega::Mutex *mutex, WebsocketsClient *client);
    
protected:
    std::string recbuffer;
    std::string sendbuffer;
    struct lws *wsi;

    void AppendMessageFragment(char *data, size_t len, size_t remaining);
    bool HasFragments();
    const char *GetMessage();
    size_t GetMessageLength();
    void ResetMessage();
    const char *getOutputBuffer();
    size_t getOutputBufferLength();
    void ResetOutputBuffer();
    
    virtual void wsSendMessage(char *msg, uint64_t len);
    
public:
    static int wsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t len);
};


#endif /* libwebsocketsIO_h */
