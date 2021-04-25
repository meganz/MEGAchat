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

LibwebsocketsIO::LibwebsocketsIO(Mutex &mutex, ::mega::Waiter* waiter, ::mega::MegaApi *api, void *ctx) : WebsocketsIO(mutex, api, ctx)
{
    ::mega::LibuvWaiter *libuvWaiter = dynamic_cast<::mega::LibuvWaiter *>(waiter);
    if (!libuvWaiter)
    {
        WEBSOCKETS_LOG_ERROR("Fatal error: NULL or invalid waiter object");
        assert(false);
        abort();
    }

    struct lws_context_creation_info info;
    memset( &info, 0, sizeof(info) );
    
    const char *lwsversion = lws_get_library_version();
    if (lwsversion)
    {
        WEBSOCKETS_LOG_DEBUG("Libwebsockets version: %s", lwsversion);        
    }
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.pcontext = &wscontext;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
    info.options |= LWS_SERVER_OPTION_LIBUV;
    info.options |= LWS_SERVER_OPTION_UV_NO_SIGSEGV_SIGFPE_SPIN;
    info.foreign_loops = (void**)&(libuvWaiter->eventloop);
    info.tls_session_timeout = TLS_SESSION_TIMEOUT; // default: 300 (seconds); (default cache size is 10, should be fine)
    
    // For extra log messages add the following levels:
    // LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_USER | LLL_THREAD
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    wscontext = lws_create_context(&info);

    eventloop = libuvWaiter->eventloop;
    WEBSOCKETS_LOG_DEBUG("Libwebsockets is using libuv");
}

LibwebsocketsIO::~LibwebsocketsIO()
{
    lws_context_destroy(wscontext);
}

void LibwebsocketsIO::restoreSessions(vector<CachedSession> &&sessions)
{
    if (sessions.empty())  return;

    lws_vhost *vh = lws_get_vhost_by_name(wscontext, DEFAULT_VHOST);
    if (!vh) // should never happen, as "default vhost is created along with the context"
    {
        WEBSOCKETS_LOG_ERROR("Missing default vhost for current LWS context");
        return;
    }

    for (auto& s : sessions)
    {
        if (LwsCache::load(vh, &s))
        {
            WEBSOCKETS_LOG_DEBUG("Loaded TLS session into LWS cache for %s:%d",
                                 s.hostname.c_str(), s.port);
        }
        else
        {
            WEBSOCKETS_LOG_ERROR("Failed to load TLS session into LWS cache for %s:%d",
                                 s.hostname.c_str(), s.port);
        }
    }
}

void LibwebsocketsIO::addevents(::mega::Waiter* waiter, int)
{    

}

static void onDnsResolved(uv_getaddrinfo_t *req, int status, struct addrinfo *res)
{
    vector<string> ipsv4, ipsv6;
    struct addrinfo *hp = res;
    while (hp)
    {
        char straddr[INET6_ADDRSTRLEN];
        straddr[0] = 0;

        if (hp->ai_family == AF_INET)
        {
            sockaddr_in *addr = (sockaddr_in *)hp->ai_addr;
            inet_ntop(hp->ai_family, &addr->sin_addr, straddr, sizeof(straddr));
            if (straddr[0])
            {
                ipsv4.push_back(straddr);
            }
        }
        else if (hp->ai_family == AF_INET6)
        {
            sockaddr_in6 *addr = (sockaddr_in6 *)hp->ai_addr;
            inet_ntop(hp->ai_family, &addr->sin6_addr, straddr, sizeof(straddr));
            if (straddr[0])
            {
                ipsv6.push_back(string("[") + straddr + "]");
            }
        }

        hp = hp->ai_next;
    }

    if (status < 0)
    {
        WEBSOCKETS_LOG_ERROR("Failed to resolve DNS. Reason: %s (%d)", uv_strerror(status), status);
    }

    WebsocketsIO::Msg *msg = static_cast<WebsocketsIO::Msg*>(req->data);
    karere::marshallCall([msg, status, ipsv4, ipsv6]()
    {
        (*msg->cb)(status, ipsv4, ipsv6);
        delete msg;
    }, msg->appCtx);

    uv_freeaddrinfo(res);
    delete req;
}

bool LibwebsocketsIO::wsResolveDNS(const char *hostname, std::function<void (int, const vector<string>&, const vector<string>&)> f)
{
    uv_getaddrinfo_t *h = new uv_getaddrinfo_t();
    Msg *msg = new Msg(appCtx, f);
    h->data = msg;
    return uv_getaddrinfo(eventloop, h, onDnsResolved, hostname, NULL, NULL);
}

WebsocketsClientImpl *LibwebsocketsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    LibwebsocketsClient *libwebsocketsClient = new LibwebsocketsClient(mutex, client);
    
    if (!libwebsocketsClient->connectViaClientInfo(ip, host, port, path, ssl, wscontext))
    {
        delete libwebsocketsClient;
        return NULL;
    }
    return libwebsocketsClient;
}

int LibwebsocketsIO::wsGetNoNameErrorCode()
{
    return UV__EAI_NONAME;
}

LibwebsocketsClient::LibwebsocketsClient(WebsocketsIO::Mutex &mutex, WebsocketsClient *client) : WebsocketsClientImpl(mutex, client)
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
        WEBSOCKETS_LOG_ERROR("Trying to send a message without a valid socket (libwebsockets)");
        assert(false);
        return false;
    }
    
    if (!sendbuffer.size())
    {
        sendbuffer.reserve(LWS_PRE + len);
        sendbuffer.resize(LWS_PRE);
    }
    sendbuffer.append(msg, len);

    if (lws_callback_on_writable(wsi) <= 0)
    {
        WEBSOCKETS_LOG_ERROR("lws_callback_on_writable() failed");
        assert(false);
        return false;
    }
    return true;
}

bool LibwebsocketsClient::connectViaClientInfo(const char *ip, const char *host, int port, const char *path, bool ssl, lws_context *wscontext)
{
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
    i.ssl_connection = ssl ? LCCSCF_USE_SSL : 0;
    string urlpath = "/";
    urlpath.append(path);
    i.path = urlpath.c_str();
    i.host = host;
    i.ietf_version_or_minus_one = -1;
    i.userdata = this;

    if (ssl)
    {
        mTlsSession.hostname = host;
        mTlsSession.port = port;
    }

    wsi = lws_client_connect_via_info(&i);

    return wsi != nullptr;
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

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined (LIBRESSL_VERSION_NUMBER) || defined (OPENSSL_IS_BORINGSSL)
#define X509_STORE_CTX_get0_cert(ctx) (ctx->cert)
#define X509_STORE_CTX_get0_untrusted(ctx) (ctx->untrusted)
#define EVP_PKEY_get0_DSA(_pkey_) ((_pkey_)->pkey.dsa)
#define EVP_PKEY_get0_RSA(_pkey_) ((_pkey_)->pkey.rsa)
#endif

static bool check_public_key(X509_STORE_CTX* ctx)
{
    if (!::WebsocketsClient::publicKeyPinning)
    {
        // if public key pinning is disabled, avoid cert's public key checkups
        WEBSOCKETS_LOG_WARNING("Public key pinning is disabled");
        return true;
    }

    unsigned char buf[sizeof(APISSLMODULUS1) - 1];
    EVP_PKEY* evp;
    if ((evp = X509_PUBKEY_get(X509_get_X509_PUBKEY(X509_STORE_CTX_get0_cert(ctx)))))
    {
        if (EVP_PKEY_id(evp) != EVP_PKEY_RSA)
        {
            WEBSOCKETS_LOG_ERROR("Invalid public key algorithm detected");
            return false;
        }

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

    WEBSOCKETS_LOG_ERROR("Invalid public key");
    return false;
}

int LibwebsocketsClient::wsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                    void *user, void *data, size_t len)
{
//    WEBSOCKETS_LOG_DEBUG("wsCallback() received: %d", reason);

    switch (reason)
    {
        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
        {
            if (check_public_key((X509_STORE_CTX*)user))
            {
                X509_STORE_CTX_set_error((X509_STORE_CTX*)user, X509_V_OK);
            }
            else
            {
                X509_STORE_CTX_set_error((X509_STORE_CTX*)user, X509_V_ERR_APPLICATION_VERIFICATION);
                return -1;
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

            //
            // deal with the TLS session

            CachedSession *s = &client->mTlsSession;
            if (s->hostname.empty()) // filter non-SSL connections, if any
                break;

            if (lws_tls_session_is_reused(wsi))
            {
                WEBSOCKETS_LOG_DEBUG("Reused TLS session for %s:%d",
                                     s->hostname.c_str(), s->port);
                break;
            }

            // save new TLS session to persistent storage
            lws_vhost *vhost = lws_get_vhost(wsi);
            if (!vhost) // should never be null
            {
                WEBSOCKETS_LOG_ERROR("Failed to save TLS session to persistent storage for %s:%d (null default vhost)",
                                     s->hostname.c_str(), s->port);
                break;
            }

            // fill in the session data
            if (LwsCache::dump(vhost, s))
            {
                if (!client->wsSSLsessionUpdateCb(*s))
                {
                    WEBSOCKETS_LOG_ERROR("Failed to save TLS session to persistent storage for %s:%d",
                                         s->hostname.c_str(), s->port);
                }
            }

            else // webrtc ssl lib did not call session-new-cb
            {
                // get the session info ourselves, and push it into the cache
                SSL *nativeSSL = lws_get_ssl(wsi);
                SSL_SESSION *sslSess = SSL_get_session(nativeSSL);
                auto bloblen = i2d_SSL_SESSION(sslSess, nullptr);
                s->blob = make_shared<Buffer>(bloblen);
                auto pp = s->blob->typedBuf<uint8_t>();
                i2d_SSL_SESSION(sslSess, &pp);
                s->blob->setDataSize(bloblen);

                if (LwsCache::load(vhost, s))
                {
                    WEBSOCKETS_LOG_DEBUG("Added TLS session to LWS cache for %s:%d (ssl callback not executed)",
                                         s->hostname.c_str(), s->port);

                    if (!client->wsSSLsessionUpdateCb(*s))
                    {
                        WEBSOCKETS_LOG_ERROR("Failed to save TLS session to persistent storage for %s:%d (ssl callback not executed)",
                                             s->hostname.c_str(), s->port);
                    }
                }
                else
                {
                    WEBSOCKETS_LOG_ERROR("Failed to add TLS session to LWS cache for %s:%d (ssl callback not executed)",
                                         s->hostname.c_str(), s->port);
                }
            }
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

            if (reason == LWS_CALLBACK_CLIENT_CONNECTION_ERROR && data && len)
            {
                std::string buf((const char*) data, len);
                WEBSOCKETS_LOG_DEBUG("Diagnostic: %s", buf.c_str());
            }

            if (client->wsIsConnected())
            {
                struct lws *dwsi = client->wsi;
                client->wsi = NULL;
                lws_set_wsi_user(dwsi, NULL);
            }
            client->wsCloseCb(reason, 0, "closed", 7);
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
                client->wsSendMsgCb((const char *)data, len);
                client->resetOutputBuffer();
            }
            break;
        }
        default:
            break;
    }
    
    return 0;
}


bool LwsCache::dump(lws_vhost *vh, CachedSession *s)
{
    return vh && s &&
            // fill in the session data
            // (dump callback will be called synchronously)
            !lws_tls_session_dump_save(vh, s->hostname.c_str(), (uint16_t)s->port, &dumpCb, s); // 0: success
}

int LwsCache::dumpCb(lws_context *, lws_tls_session_dump *info)
{
    if (!info)  return 1;

    CachedSession *sess = reinterpret_cast<CachedSession*>(info->opaque);
    if (!sess)  return 1;

    sess->blob = make_shared<Buffer>(info->blob_len);
    sess->blob->assign(info->blob, info->blob_len);

    return 0;
}

bool LwsCache::load(lws_vhost *vh, CachedSession *s)
{
    return vh && s &&
            // fill in the session data
            // (dump callback will be called synchronously)
            !lws_tls_session_dump_load(vh, s->hostname.c_str(), (uint16_t)s->port, &loadCb, s); // 0: success
}

int LwsCache::loadCb(lws_context *, lws_tls_session_dump *info)
{
    if (!info)  return 1;

    CachedSession *sess = reinterpret_cast<CachedSession*>(info->opaque);
    if (!sess)  return 1;

    info->blob = malloc(sess->blob->dataSize()); // will be deleted by LWS
    memcpy(info->blob, sess->blob->buf(), sess->blob->dataSize());
    info->blob_len = sess->blob->dataSize();

    return 0;
}
