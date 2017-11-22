#include "net/libwebsocketsIO.h"
#include "waiter/libuvWaiter.h"

#include <mega/http.h>
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

LibwebsocketsIO::LibwebsocketsIO(::mega::Mutex *mutex, ::mega::Waiter* waiter, void *ctx) : WebsocketsIO(mutex, ctx)
{
    struct lws_context_creation_info info;
    memset( &info, 0, sizeof(info) );
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
    info.options |= LWS_SERVER_OPTION_LIBUV;
    
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    wscontext = lws_create_context(&info);

    ::mega::LibuvWaiter *libuvWaiter = dynamic_cast<::mega::LibuvWaiter *>(waiter);
    if (!libuvWaiter)
    {
        WEBSOCKETS_LOG_ERROR("Fatal error: NULL or invalid waiter object");
        exit(0);
    }
    lws_uv_initloop(wscontext, libuvWaiter->eventloop, 0);
    WEBSOCKETS_LOG_DEBUG("Libwebsockets is using libuv");
}

LibwebsocketsIO::~LibwebsocketsIO()
{
    lws_context_destroy(wscontext);
}

void LibwebsocketsIO::addevents(::mega::Waiter* waiter, int)
{    

}

WebsocketsClientImpl *LibwebsocketsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    LibwebsocketsClient *libwebsocketsClient = new LibwebsocketsClient(mutex, client);
    
    std::string cip = ip;
    if (cip[0] == '[')
    {
        // remove brackets in IPv6 addresses
        cip = cip.substr(1, cip.size() - 2);
    }
    
    struct lws_client_connect_info i;
    memset(&i, 0, sizeof(i));
    i.context = wscontext;
    i.address = cip.c_str();
    i.port = port;
    i.ssl_connection = ssl ? 2 : 0;
    string urlpath = "/";
    urlpath.append(path);
    i.path = urlpath.c_str();
    i.host = host;
    i.ietf_version_or_minus_one = -1;
    i.userdata = libwebsocketsClient;
    
    libwebsocketsClient->wsi = lws_client_connect_via_info(&i);
    if (!libwebsocketsClient->wsi)
    {
        delete libwebsocketsClient;
        return NULL;
    }
    return libwebsocketsClient;
}

LibwebsocketsClient::LibwebsocketsClient(::mega::Mutex *mutex, WebsocketsClient *client) : WebsocketsClientImpl(mutex, client)
{
    wsi = NULL;
}

LibwebsocketsClient::~LibwebsocketsClient()
{
    wsDisconnect(true);
}

void LibwebsocketsClient::appendMessageFragment(char *data, size_t len, size_t remaining)
{
    if (!recbuffer.size() && remaining)
    {
        recbuffer.reserve(len + remaining);
    }
    recbuffer.append(data, len);
}

bool LibwebsocketsClient::hasFragments()
{
    return recbuffer.size();
}

const char *LibwebsocketsClient::getMessage()
{
    return recbuffer.data();
}

size_t LibwebsocketsClient::getMessageLength()
{
    return recbuffer.size();
}

void LibwebsocketsClient::resetMessage()
{
    recbuffer.clear();
}

bool LibwebsocketsClient::wsSendMessage(char *msg, size_t len)
{
    assert(wsi);
    
    if (!wsi)
    {
        return false;
    }
    
    if (!sendbuffer.size())
    {
        sendbuffer.reserve(LWS_PRE + len);
        sendbuffer.resize(LWS_PRE);
    }
    sendbuffer.append(msg, len);
    return lws_callback_on_writable(wsi) > 0;
}

void LibwebsocketsClient::wsDisconnect(bool immediate)
{
    if (!wsi)
    {
        return;
    }

    if (immediate)
    {
        struct lws *dwsi = wsi;
        wsi = NULL;
        lws_set_wsi_user(dwsi, NULL);
        WEBSOCKETS_LOG_DEBUG("Pointer detached from libwebsockets");
        
        if (!disconnecting)
        {
            lws_callback_on_writable(dwsi);
            WEBSOCKETS_LOG_DEBUG("Requesting a forced disconnection to libwebsockets");
        }
        else
        {
            disconnecting = false;
            WEBSOCKETS_LOG_DEBUG("Already disconnecting from libwebsockets");
        }
    }
    else
    {
        if (!disconnecting)
        {
            disconnecting = true;
            lws_callback_on_writable(wsi);
            WEBSOCKETS_LOG_DEBUG("Requesting a graceful disconnection to libwebsockets");
        }
        else
        {
            WEBSOCKETS_LOG_WARNING("Ignoring graceful disconnect. Already disconnecting gracefully");
        }
    }
}

bool LibwebsocketsClient::wsIsConnected()
{
    return wsi != NULL;
}

const char *LibwebsocketsClient::getOutputBuffer()
{
    return sendbuffer.size() ? sendbuffer.data() + LWS_PRE : NULL;
}

size_t LibwebsocketsClient::getOutputBufferLength()
{
    return sendbuffer.size() ? sendbuffer.size() - LWS_PRE : 0;
}

void LibwebsocketsClient::resetOutputBuffer()
{
    sendbuffer.clear();
}

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define X509_STORE_CTX_get0_cert(ctx) (ctx->cert)
#define X509_STORE_CTX_get0_untrusted(ctx) (ctx->untrusted)
#define EVP_PKEY_get0_DSA(_pkey_) ((_pkey_)->pkey.dsa)
#define EVP_PKEY_get0_RSA(_pkey_) ((_pkey_)->pkey.rsa)
#endif

const BIGNUM *RSA_get0_n(const RSA *rsa)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    return rsa->n;
#else
    const BIGNUM *result;
    RSA_get0_key(rsa, &result, NULL, NULL);
    return result;
#endif
}

const BIGNUM *RSA_get0_e(const RSA *rsa)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    return rsa->e;
#else
    const BIGNUM *result;
    RSA_get0_key(rsa, NULL, &result, NULL);
    return result;
#endif
}

const BIGNUM *RSA_get0_d(const RSA *rsa)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    return rsa->d;
#else
    const BIGNUM *result;
    RSA_get0_key(rsa, NULL, NULL, &result);
    return result;
#endif
}

static bool check_public_key(X509_STORE_CTX* ctx)
{
    unsigned char buf[sizeof(APISSLMODULUS1) - 1];
    EVP_PKEY* evp;
    if ((evp = X509_PUBKEY_get(X509_get_X509_PUBKEY(X509_STORE_CTX_get0_cert(ctx)))))
    {
        if (BN_num_bytes(RSA_get0_n(EVP_PKEY_get0_RSA(evp))) == sizeof APISSLMODULUS1 - 1
            && BN_num_bytes(RSA_get0_e(EVP_PKEY_get0_RSA(evp))) == sizeof APISSLEXPONENT - 1)
        {
            BN_bn2bin(RSA_get0_n(EVP_PKEY_get0_RSA(evp)), buf);
            
            if (!memcmp(buf, CHATSSLMODULUS, sizeof CHATSSLMODULUS - 1))
            {
                BN_bn2bin(RSA_get0_e(EVP_PKEY_get0_RSA(evp)), buf);
                if (!memcmp(buf, APISSLEXPONENT, sizeof APISSLEXPONENT - 1))
                {
                    EVP_PKEY_free(evp);
                    return true;
                }
            }
        }
        EVP_PKEY_free(evp);
    }

    return false;
}

int LibwebsocketsClient::wsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                    void *user, void *data, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
        {
            if (check_public_key((X509_STORE_CTX*)user))
            {
                X509_STORE_CTX_set_error((X509_STORE_CTX*)user, X509_V_OK);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                return -1;
            }
            
            client->wsConnectCb();
            break;
        }
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                WEBSOCKETS_LOG_DEBUG("Forced disconnect completed");
                return -1;
            }
            if (client->disconnecting)
            {
                WEBSOCKETS_LOG_DEBUG("Graceful disconnect completed");
                client->disconnecting = false;
            }
            else
            {
                WEBSOCKETS_LOG_DEBUG("Disconnect done by server");
            }
            if (client->wsIsConnected())
            {
                struct lws *dwsi = client->wsi;
                client->wsi = NULL;
                lws_set_wsi_user(dwsi, NULL);
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
                if (client->hasFragments())
                {
                    WEBSOCKETS_LOG_DEBUG("Fragmented data completed");
                    client->appendMessageFragment((char *)data, len, 0);
                    data = (void *)client->getMessage();
                    len = client->getMessageLength();
                }
                
                client->wsHandleMsgCb((char *)data, len);
                client->resetMessage();
            }
            else
            {
                WEBSOCKETS_LOG_DEBUG("Managing fragmented data");
                client->appendMessageFragment((char *)data, len, remaining);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                WEBSOCKETS_LOG_DEBUG("Completing forced disconnect");
                return -1;
            }
            
            if (client->disconnecting)
            {
                WEBSOCKETS_LOG_DEBUG("Completing graceful disconnect");
                return -1;
            }
            
            data = (void *)client->getOutputBuffer();
            len = client->getOutputBufferLength();
            if (len && data)
            {
                lws_write(wsi, (unsigned char *)data, len, LWS_WRITE_BINARY);
                client->resetOutputBuffer();
            }
            break;
        }
        default:
            break;
    }
    
    return 0;
}

