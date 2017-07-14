#include "libwebsocketsIO.h"

#ifdef LWS_USE_LIBUV
#include "waiter/libuvWaiter.h"
#else
#include "waiter/libwebsocketsWaiter.h"
#endif

#include <assert.h>

using namespace std;

static struct lws_protocols protocols[] =
{
    {
        "MEGAchat",
        LibwebsocketsClient::wsCallback,
        0,
        128 * 1024, // Rx buffer size
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

LibwebsocketsIO::LibwebsocketsIO()
{
    struct lws_context_creation_info info;
    memset( &info, 0, sizeof(info) );
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
    
#ifdef LWS_USE_LIBUV
    info.options |= LWS_SERVER_OPTION_LIBUV;
#endif
    
    lws_set_log_level(LLL_ERR | LLL_EXT | LLL_INFO | LLL_USER | LLL_WARN | LLL_COUNT
                      | LLL_DEBUG | LLL_CLIENT | LLL_HEADER | LLL_NOTICE
                      | LLL_PARSER | LLL_LATENCY, NULL);
    
    wscontext = lws_create_context(&info);
    initialized = false;
}

LibwebsocketsIO::~LibwebsocketsIO()
{
    
}

void LibwebsocketsIO::addevents(::mega::Waiter* waiter, int)
{    
    if (!initialized)
    {
#ifdef LWS_USE_LIBUV
        ::mega::LibuvWaiter *libuvWaiter = dynamic_cast<::mega::LibuvWaiter *>(waiter);
        if (!libuvWaiter)
        {
            exit(0);
        }
        lws_uv_initloop(wscontext, libuvWaiter->eventloop, 0);
#else
        ::mega::LibwebsocketsWaiter *websocketsWaiter = dynamic_cast<::mega::LibwebsocketsWaiter *>(waiter);
        if (!websocketsWaiter)
        {
            exit(0);
        }
        websocketsWaiter->wscontext = wscontext;
#endif
        initialized = true;
    }
}


WebsocketsClientImpl *LibwebsocketsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    LibwebsocketsClient *libwebsocketsClient = new LibwebsocketsClient(client);
    
    struct lws_client_connect_info i;
    memset(&i, 0, sizeof(i));
    i.context = wscontext;
    i.address = ip;
    i.port = port;
    i.ssl_connection = ssl ? 2 : 0;
    string urlpath = "/";
    urlpath.append(path);
    i.path = urlpath.c_str();
    i.host = host;
    i.ietf_version_or_minus_one = -1;
    i.userdata = libwebsocketsClient;
    
    lws_client_connect_via_info(&i);
    return libwebsocketsClient;
}

LibwebsocketsClient::LibwebsocketsClient(WebsocketsClient *client) : WebsocketsClientImpl(client)
{
    wsi = NULL;
}

void LibwebsocketsClient::AppendMessageFragment(char *data, size_t len, size_t remaining)
{
    if (!recbuffer.size() && remaining)
    {
        recbuffer.reserve(len + remaining);
    }
    recbuffer.append(data, len);
}

bool LibwebsocketsClient::HasFragments()
{
    return recbuffer.size();
}

const char *LibwebsocketsClient::GetMessage()
{
    return recbuffer.data();
}

size_t LibwebsocketsClient::GetMessageLength()
{
    return recbuffer.size();
}

void LibwebsocketsClient::ResetMessage()
{
    recbuffer.clear();
}

void LibwebsocketsClient::wsSendMessage(char *msg, uint64_t len)
{
    assert(wsi);
    
    if (!sendbuffer.size())
    {
        sendbuffer.reserve(LWS_PRE + len);
        sendbuffer.resize(LWS_PRE);
    }
    sendbuffer.append(msg, len);
    lws_callback_on_writable(wsi);
}

const char *LibwebsocketsClient::getOutputBuffer()
{
    return sendbuffer.size() ? sendbuffer.data() + LWS_PRE : NULL;
}

size_t LibwebsocketsClient::getOutputBufferLength()
{
    return sendbuffer.size() ? sendbuffer.size() - LWS_PRE : 0;
}

void LibwebsocketsClient::ResetOutputBuffer()
{
    sendbuffer.clear();
}

int LibwebsocketsClient::wsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                    void *user, void *data, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
        {
            X509_STORE_CTX_set_error((X509_STORE_CTX*)user, X509_V_OK);
            break;
        }
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                return -1;
            }
            
            client->wsi = wsi;
            client->wsConnectCb();
            break;
        }
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                return -1;
            }
            
            client->wsCloseCb(0, 0, "", 0);
            break;
        }
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                return -1;
            }
            
            const size_t remaining = lws_remaining_packet_payload(wsi);
            if (!remaining && lws_is_final_fragment(wsi))
            {
                if (client->HasFragments())
                {
                    client->AppendMessageFragment((char *)data, len, 0);
                    data = (void *)client->GetMessage();
                    len = client->GetMessageLength();
                }
                
                client->wsHandleMsgCb((char *)data, len);
                client->ResetMessage();
            }
            else
            {
                client->AppendMessageFragment((char *)data, len, remaining);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                return -1;
            }
            
            data = (void *)client->getOutputBuffer();
            len = client->getOutputBufferLength();
            if (len && data)
            {
                lws_write(wsi, (unsigned char *)data, len, LWS_WRITE_BINARY);
                client->ResetOutputBuffer();
            }
            break;
        }
        default:
            break;
    }
    
    return 0;
}
