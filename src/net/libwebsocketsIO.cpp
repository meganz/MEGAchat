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
        0, nullptr, 0,
    },
    {} /* terminator */
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
    
    eventloop = libuvWaiter->eventloop();
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.pcontext = &wscontext;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.foreign_loops = (void**)&(eventloop);
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
    info.options |= LWS_SERVER_OPTION_LIBUV;
    info.options |= LWS_SERVER_OPTION_UV_NO_SIGSEGV_SIGFPE_SPIN;
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    info.tls_session_timeout = TLS_SESSION_TIMEOUT; // default was 300 (seconds); (default cache size is 10, should be fine)
#else
    info.options |= LWS_SERVER_OPTION_DISABLE_TLS_SESSION_CACHE;
#endif

    // For extra log messages add the following levels:
    // LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_USER | LLL_THREAD
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    wscontext = lws_create_context(&info);
    mLwsContextThread = this_thread::get_id();

    WEBSOCKETS_LOG_DEBUG("Libwebsockets is using libuv");
}

LibwebsocketsIO::~LibwebsocketsIO()
{
    verifyLwsContextThread();
    lws_context_destroy(wscontext);
}

void LibwebsocketsIO::verifyLwsContextThread() const
{
    assert(mLwsContextThread == std::this_thread::get_id());
}

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
void LibwebsocketsIO::restoreSessions(vector<CachedSession> &&sessions)
{
    // This call will typically be made from a thread that is not the owner of LWS context. In which
    // case, loading cached sessions cannot be done from here. Save them to be available before the
    // first connection.
    if (sessions.empty())
        return;

    std::lock_guard<std::mutex> sessionLock{mTlsSessionsMutex};
    mTlsSessionsToRestore.insert(mTlsSessionsToRestore.end(),
                                 std::make_move_iterator(sessions.begin()),
                                 std::make_move_iterator(sessions.end()));
}

void LibwebsocketsIO::restoreTlsSessions()
{
    verifyLwsContextThread();

    lws_vhost *vh = lws_get_vhost_by_name(wscontext, DEFAULT_VHOST);
    if (!vh) // should never happen, as "default vhost is created along with the context"
    {
        WEBSOCKETS_LOG_ERROR("Missing default vhost for current LWS context");
        return;
    }

    std::lock_guard<std::mutex> sessionLock{mTlsSessionsMutex};
    for (auto& s: mTlsSessionsToRestore)
    {
        if (LwsCache::load(vh, &s))
        {
            WEBSOCKETS_LOG_DEBUG("TLS session loaded into LWS cache for %s:%d",
                                 s.hostname.c_str(), s.port);
        }
        else
        {
            WEBSOCKETS_LOG_ERROR("TLS session failed to load into LWS cache for %s:%d",
                                 s.hostname.c_str(), s.port);
        }
    }
    mTlsSessionsToRestore.clear();
}
#endif // WEBSOCKETS_TLS_SESSION_CACHE_ENABLED

void LibwebsocketsIO::addevents(::mega::Waiter*, int)
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
    verifyLwsContextThread();

    uv_getaddrinfo_t *h = new uv_getaddrinfo_t();
    Msg *msg = new Msg(appCtx, f);
    h->data = msg;
    return uv_getaddrinfo(eventloop, h, onDnsResolved, hostname, NULL, NULL);
}

WebsocketsClientImpl *LibwebsocketsIO::wsConnect(const char *ip, const char *host, int port, const char *path, bool ssl, WebsocketsClient *client)
{
    verifyLwsContextThread();

    restoreTlsSessions(); // load sessions cached in earlier runs, if any

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
    verifyLwsContextThread();

    return UV__EAI_NONAME;
}

LibwebsocketsClient::LibwebsocketsClient(WebsocketsIO::Mutex& mutex, WebsocketsClient* client):
    WebsocketsClientImpl(mutex, client),
    wsi{nullptr},
    disconnecting{false}
{}

LibwebsocketsClient::~LibwebsocketsClient()
{
    doWsDisconnect(true); // do not call wsDisconnect() virtual function during destruction
}

void LibwebsocketsClient::verifyLwsThread() const
{
    assert(mLwsThread == std::this_thread::get_id());
}

void LibwebsocketsClient::appendMessageFragment(char *data, size_t len, size_t remaining)
{
    verifyLwsThread();
    if (!recbuffer.size() && remaining)
    {
        recbuffer.reserve(len + remaining);
    }
    recbuffer.append(data, len);
}

bool LibwebsocketsClient::hasFragments()
{
    verifyLwsThread();
    return recbuffer.size();
}

const char *LibwebsocketsClient::getMessage()
{
    verifyLwsThread();
    return recbuffer.data();
}

size_t LibwebsocketsClient::getMessageLength()
{
    verifyLwsThread();
    return recbuffer.size();
}

void LibwebsocketsClient::resetMessage()
{
    verifyLwsThread();
    recbuffer.clear();
}

bool LibwebsocketsClient::wsSendMessage(char *msg, size_t len)
{
    verifyLwsThread();
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

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    if (ssl)
    {
        mTlsSession.hostname = host;
        mTlsSession.port = port;
    }
#endif

    wsi = lws_client_connect_via_info(&i);

    mLwsThread = this_thread::get_id();

    return wsi != nullptr;
}

void LibwebsocketsClient::wsDisconnect(bool immediate)
{
    doWsDisconnect(immediate);
}

void LibwebsocketsClient::doWsDisconnect(bool immediate)
{
    verifyLwsThread();

    if (!isConnected())
        return;

    if (disconnecting)
    {
        if (immediate)
        {
            removeConnection();
            disconnecting = false;
            WEBSOCKETS_LOG_DEBUG("Requesting a forced disconnection to libwebsockets while "
                                 "graceful disconnection in progress");
        }
        else
        {
            WEBSOCKETS_LOG_WARNING(
                "Ignoring graceful disconnect. Already disconnecting gracefully");
        }
    }
    else
    {
        const auto lwsContext = lws_get_context(wsi);
        markAsDisconnecting();
        if (immediate)
        {
            removeConnection();
            WEBSOCKETS_LOG_DEBUG("Requesting a forced disconnection to libwebsockets");
        }
        else
        {
            disconnecting = true;
            WEBSOCKETS_LOG_DEBUG("Requesting a graceful disconnection to libwebsockets");
        }

        // TODO (Future investigation / rework / refactoring):
        // The call to verifyLwsThread() above ensures that this gets executed in the context of the
        // thread that owns LWS. Before commit d7e39db115f7398b4fccd42475f82ddb64032318,
        // lws_callback_on_writable() was called directly from here, which, considering the above,
        // was fine. After that commit, the latter was defered to be called from the handler of
        // LWS_CALLBACK_EVENT_WAIT_CANCELLED, which is now triggered by the call below.
        //
        // One thing to consider is whether calling lws_callback_on_writable() when disconnecting is
        // needed at all. Even if it truly is, the changes in the commit mentioned above were
        // probably not needed (although there's no obvious harm to them either, just made the code
        // slightly more complicated).
        lws_cancel_service(lwsContext);
    }
}

bool LibwebsocketsClient::wsIsConnected()
{
    return isConnected();
}

bool LibwebsocketsClient::isConnected() const
{
    verifyLwsThread();
    return wsi != nullptr;
}

void LibwebsocketsClient::removeConnection()
{
    if (!isConnected())
        return;
    lws_set_wsi_user(std::exchange(wsi, nullptr), NULL);
    WEBSOCKETS_LOG_DEBUG("Pointer detached from libwebsockets");
}

std::mutex LibwebsocketsClient::accessDisconnectingWsiMtx{};
std::set<struct lws*> LibwebsocketsClient::disconnectingWsiSet{};

void LibwebsocketsClient::markAsDisconnecting()
{
    if (!isConnected())
        return;
    std::lock_guard g(accessDisconnectingWsiMtx);
    disconnectingWsiSet.insert(wsi);
}

const char *LibwebsocketsClient::getOutputBuffer()
{
    verifyLwsThread();
    return sendbuffer.size() ? sendbuffer.data() + LWS_PRE : NULL;
}

size_t LibwebsocketsClient::getOutputBufferLength()
{
    verifyLwsThread();
    return sendbuffer.size() ? sendbuffer.size() - LWS_PRE : 0;
}

void LibwebsocketsClient::resetOutputBuffer()
{
    verifyLwsThread();
    sendbuffer.clear();
}

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined (LIBRESSL_VERSION_NUMBER)
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

    unsigned char buf[sizeof(CHATSSLMODULUS) - 1];
    EVP_PKEY* evp;
    if ((evp = X509_PUBKEY_get(X509_get_X509_PUBKEY(X509_STORE_CTX_get0_cert(ctx)))))
    {
        if (EVP_PKEY_id(evp) != EVP_PKEY_RSA)
        {
            WEBSOCKETS_LOG_ERROR("Invalid public key algorithm detected");
            return false;
        }

        // CONNECT TO CHATD/PRESENCED
        if ((BN_num_bytes(RSA_get0_e(EVP_PKEY_get0_RSA(evp))) == sizeof CHATSSLEXPONENT - 1)
                && ((BN_num_bytes(RSA_get0_n(EVP_PKEY_get0_RSA(evp))) == sizeof CHATSSLMODULUS  - 1)
                    || (BN_num_bytes(RSA_get0_n(EVP_PKEY_get0_RSA(evp))) == sizeof CHATSSLMODULUS2 - 1)))
        {
            BN_bn2bin(RSA_get0_n(EVP_PKEY_get0_RSA(evp)), buf);

            if (!memcmp(buf,CHATSSLMODULUS, sizeof CHATSSLMODULUS - 1)            // check main key
                || !memcmp(buf,CHATSSLMODULUS2, sizeof CHATSSLMODULUS2 - 1))      // check backup key
            {
                BN_bn2bin(RSA_get0_e(EVP_PKEY_get0_RSA(evp)), buf);
                if (!memcmp(buf, CHATSSLEXPONENT, sizeof CHATSSLEXPONENT - 1))
                {
                    EVP_PKEY_free(evp);
                    return true;
                }
            }
        }

        // CONNECT TO SFU
        if ((BN_num_bytes(RSA_get0_e(EVP_PKEY_get0_RSA(evp))) == sizeof SFUSSLEXPONENT - 1)
                && ((BN_num_bytes(RSA_get0_n(EVP_PKEY_get0_RSA(evp))) == sizeof SFUSSLMODULUS  - 1)
                    || (BN_num_bytes(RSA_get0_n(EVP_PKEY_get0_RSA(evp))) == sizeof SFUSSLMODULUS2 - 1)))
        {
            BN_bn2bin(RSA_get0_n(EVP_PKEY_get0_RSA(evp)), buf);

            if (!memcmp(buf,SFUSSLMODULUS, sizeof SFUSSLMODULUS - 1)            // check main key
                || !memcmp(buf,SFUSSLMODULUS2, sizeof SFUSSLMODULUS2 - 1))      // check backup key
            {
                BN_bn2bin(RSA_get0_e(EVP_PKEY_get0_RSA(evp)), buf);
                if (!memcmp(buf, SFUSSLEXPONENT, sizeof SFUSSLEXPONENT - 1))
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

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
            //
            // deal with the TLS session

            const CachedSession& s = client->mTlsSession;

            if (s.hostname.empty()) // filter non-SSL connections, if any
            {
                break;
            }

            if (lws_tls_session_is_reused(wsi))
            {
                WEBSOCKETS_LOG_DEBUG("TLS session reused for %s:%d",
                                     s.hostname.c_str(), s.port);
                break;
            }

            // Session info was new (not reused). Store it persistently even if it may not be reusable,
            // as the only way to know that (SSL_SESSION_is_resumable) was unreliable.
            client->saveTlsSessionToPersistentStorage();
#endif // WEBSOCKETS_TLS_SESSION_CACHE_ENABLED

            break;
        }

        case LWS_CALLBACK_TIMER:
        {
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (wsi && client && client->mTlsSession.saveToStorage())
            {
                WEBSOCKETS_LOG_DEBUG("TLS session retrying to save to persistent storage for %s:%d",
                                     client->mTlsSession.hostname.c_str(), client->mTlsSession.port);
                client->saveTlsSessionToPersistentStorage();
            }
#endif
            break;
        }

        case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
        {
            lock_guard g(accessDisconnectingWsiMtx);
            while (!disconnectingWsiSet.empty())
            {
                auto firstEl = disconnectingWsiSet.begin();
                lws_callback_on_writable(*firstEl);
                disconnectingWsiSet.erase(firstEl);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            {
                lock_guard g(accessDisconnectingWsiMtx);
                disconnectingWsiSet.erase(wsi);
            }
            LibwebsocketsClient* client = (LibwebsocketsClient*)user;
            if (!client)
            {
                WEBSOCKETS_LOG_DEBUG("Forced disconnect completed");
                break;
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

            client->removeConnection();
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
                enum lws_write_protocol writeProtocol = client->client->isWriteBinary() ?
                            LWS_WRITE_BINARY : LWS_WRITE_TEXT;

                lws_write(wsi, (unsigned char *)data, len, writeProtocol);
                client->wsSendMsgCb((const char *)data, len);
                client->resetOutputBuffer();

                // This cb will only be implemented in those clients that require messages to be sent individually
                client->wsProcessNextMsgCb();
            }
            break;
        }
        default:
            break;
    }
    
    return 0;
}


#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
void LibwebsocketsClient::saveTlsSessionToPersistentStorage()
{
    if (!wsIsConnected())
    {
        return;
    }

    // Allow later retry should it fail now
    mTlsSession.saveToStorage(true);
    // Schedule a retry now, just in case this will return early
    lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC); // 5 seconds

    // Attempt to get it from LWS cache first
    bool fromLWS = false;
    if (LwsCache::dump(lws_get_vhost(wsi), &mTlsSession))
    {
        fromLWS = true;
        WEBSOCKETS_LOG_DEBUG("TLS session info taken from LWS cache for %s:%d",
                             mTlsSession.hostname.c_str(), mTlsSession.port);
    }
    else
    {
        // Get it from raw OpenSSL/BoringSSL when it hasn't reached LWS. It may contain
        // invalid data in this case, so it will also retry later.
        SSL *nativeSSL = lws_get_ssl(wsi);
        SSL_SESSION *sslSess = SSL_get_session(nativeSSL);
        if (!sslSess) // should never happen
        {
            WEBSOCKETS_LOG_ERROR("TLS session was NULL for %s:%d; try again later to store it",
                                 mTlsSession.hostname.c_str(), mTlsSession.port);
            return;
        }
        // SSL_SESSION_is_resumable() returned 0 for all sessions, resumable or not.
        // It cannot be trusted, so don't use it to filter sessions.

        // Serialize session data
        auto bloblen = i2d_SSL_SESSION(sslSess, nullptr);
        mTlsSession.blob = make_shared<Buffer>(bloblen);
        uint8_t *pp = mTlsSession.blob->typedBuf<uint8_t>();
        i2d_SSL_SESSION(sslSess, &pp);
        mTlsSession.blob->setDataSize(bloblen);
        WEBSOCKETS_LOG_DEBUG("TLS session info taken from raw BoringSSL, will try again later"
                             " from LWS cache for %s:%d", mTlsSession.hostname.c_str(), mTlsSession.port);
    }

    // Store session info. Consider it valid when taken from LWS, so don't retry later.
    if (wsSSLsessionUpdateCb(mTlsSession) && fromLWS)
    {
        mTlsSession.saveToStorage(false);
        lws_set_timer_usecs(wsi, LWS_SET_TIMER_USEC_CANCEL); // cancel scheduled retry
    }

    mTlsSession.blob = nullptr; // stored or not, don't keep it in memory
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
            // (load callback will be called synchronously)
            !lws_tls_session_dump_load(vh, s->hostname.c_str(), (uint16_t)s->port, &loadCb, s); // 0: success
}

int LwsCache::loadCb(lws_context *, lws_tls_session_dump *info)
{
    if (!info)  return 1;

    CachedSession *sess = reinterpret_cast<CachedSession*>(info->opaque);
    if (!sess || !sess->blob)  return 1;

    info->blob = malloc(sess->blob->dataSize()); // will be deleted by LWS
    memcpy(info->blob, sess->blob->buf(), sess->blob->dataSize());
    info->blob_len = sess->blob->dataSize();

    return 0;
}
#endif // WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
