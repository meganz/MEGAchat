#ifndef websocketsIO_h
#define websocketsIO_h

#include <thread>
#include <iostream>
#include <functional>
#include <vector>
#include <mega/waiter.h>
#include <mega/thread.h>
#include "base/logger.h"
#include "sdkApi.h"
#include "buffer.h"
#include "db.h"
#include "url.h"

// TLS session resumption can be disabled by turning this off
#define WEBSOCKETS_TLS_SESSION_CACHE_ENABLED 1

// WEBSOCKETS LOG
#define WEBSOCKETS_LOG_VERBOSE(fmtString, ...) \
KARERE_LOG_VERBOSE(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_websockets, fmtString, ##__VA_ARGS__)
#define WEBSOCKETS_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_websockets, fmtString, ##__VA_ARGS__)

// DNSCACHE LOG
#define DNSCACHE_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_dnscache, fmtString, ##__VA_ARGS__)
#define DNSCACHE_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_dnscache, fmtString, ##__VA_ARGS__)
#define DNSCACHE_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_dnscache, fmtString, ##__VA_ARGS__)
#define DNSCACHE_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_dnscache, fmtString, ##__VA_ARGS__)

class WebsocketsClient;
class WebsocketsClientImpl;

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
struct CachedSession
{
    std::string             hostname;    // host.domain
    int                     port = 0;    // 443 usually
    std::shared_ptr<Buffer> blob;        // session data

    void saveToStorage(bool save) { disconnectAction = save ? MEGA_SAVE : MEGA_IGNORE; }
    bool saveToStorage() const { return disconnectAction == MEGA_SAVE; }
    void dropFromStorage(bool drop) { disconnectAction = drop ? MEGA_DROP : MEGA_IGNORE; }
    bool dropFromStorage() const { return disconnectAction == MEGA_DROP; }

private:
    enum
    {
        MEGA_IGNORE   // WinBase.h:   #define IGNORE              0       // Ignore signal
        , MEGA_SAVE
        , MEGA_DROP
    };

    // This can be used to set a default action for TLS session info.
    //
    // Typically, session info should initially be ignored, until
    // a successful connection will explicitly request it to be saved.
    //
    // It can also be initialized to "drop", in order to remove a
    // saved session from storage, upon a failed connection attempt.
    // In case of connection failure, LWS doesn't offer enough data
    // in the cb to identify the originally used session info.
    int disconnectAction = MEGA_IGNORE;
};
#endif // WEBSOCKETS_TLS_SESSION_CACHE_ENABLED

class DNScache
{
struct DNSrecord;
public:
    // reference to db-layer interface
    SqliteDb &mDb;

    DNScache(SqliteDb &db, int chatdVersion);
    void loadFromDb();
    void addRecord(int shard, const std::string &url, std::shared_ptr<Buffer> sess = nullptr, bool saveToDb = true);
    void removeRecord(int shard);
    void updateRecord(int shard, const std::string &url, bool saveToDb);
    bool hasRecord(int shard);
    bool isValidUrl(int shard);
    // the record for the given shard must exist
    bool setIp(int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    // the record for the given shard must exist (to load from DB)
    bool setIp(int shard, std::string ipv4, std::string ipv6);
    bool getIp(int shard, std::string &ipv4, std::string &ipv6);
    bool invalidateIps(int shard);
    void connectDone(int shard, const std::string &ip);
    bool isMatch(int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    bool isMatch(int shard, const std::string &ipv4, const std::string &ipv6);
    time_t age(int shard);
    const karere::Url &getUrl(int shard);

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    bool updateTlsSession(const CachedSession &sess);
    std::vector<CachedSession> getTlsSessions();
#endif

    // DNS cache methods to manage SFU records
    bool isSfuValidShard(int shard) const;
    bool setSfuIp(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    bool addSfuRecord(const std::string &host, std::shared_ptr<Buffer> sess = nullptr, bool saveToDb = true, int shard = kInvalidShard);

    // DNS cache methods to manage records based on host instead of shard
    bool addRecordByHost(const std::string &host, std::shared_ptr<Buffer> sess = nullptr, bool saveToDb = true, int shard = kInvalidShard);
    DNSrecord* getRecordByHost(const std::string &host);
    void connectDoneByHost(const std::string &host, const std::string &ip);
    bool getIpByHost(const std::string &host, std::string &ipv4, std::string &ipv6);
    bool isMatchByHost(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);

private:
    struct DNSrecord
    {
        karere::Url mUrl;
        std::string ipv4;
        std::string ipv6;
        ::mega::m_time_t resolveTs = 0;       // can be used to invalidate IP addresses by age
        ::mega::m_time_t connectIpv4Ts = 0;   // can be used for heuristics based on last successful connection
        ::mega::m_time_t connectIpv6Ts = 0;   // can be used for heuristics based on last successful connection
        std::shared_ptr<Buffer> tlsBlob; // tls session data

        // ctor to use when URL is available (ie. chatd and presenced)
        DNSrecord(const std::string &url) : mUrl(url) {}

        // ctor to use when URL is not available, but hostname exclusively (only for SFU records -> protocol="wss")
        DNSrecord(const std::string &host, std::shared_ptr<Buffer> sess):
            mUrl(host, "wss"), tlsBlob(sess && !sess->empty() ? sess : nullptr) {}

        bool isHostMatch(const std::string &host) const { return mUrl.host == host; }
    };

    // DNS cache methods to manage SFU records
    int calculateNextSfuShard();
    bool addSfuRecordWithIp(const std::string &host, std::shared_ptr<Buffer> sess, bool saveToDb, int shard, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    bool hasRecordByHost(const std::string &host) const;
    bool setIpByHost(const std::string &host, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6);
    bool updateCurrentShardForSfuFromDb(); // retrieves the min SFU shard from DB and updates mCurrentShardForSfu

    // class members
    static constexpr int kInvalidShard = INT_MIN; // invalid shard value
    enum: int8_t { kSfuShardStart = -20,  kSfuShardEnd = -128};
    std::map<int, DNSrecord> mRecords; // Maps shard to DNSrecord
    int mChatdVersion;
    void removeRecordsByShards(const std::set<int> &removeElements);

    /* SFU servers are not distributed in shards, but dns_cache primary key is shard number,
     * so for this purpose, we'll use a variable whose possible values are between
     * kSfuShardStart and kSfuShardEnd.
     */
    int mCurrentShardForSfu;
};

// Generic websockets network layer
class WebsocketsIO : public ::mega::EventTrigger
{
public:
    using Mutex = std::recursive_mutex;
    using MutexGuard = std::lock_guard<Mutex>;

    WebsocketsIO(Mutex &mutex, ::mega::MegaApi *megaApi, void *ctx);
    ~WebsocketsIO() override;

    // apart from the lambda function to be executed, since it needs to be executed on a marshall call,
    // the appCtx is also required for some callbacks, so Msg wraps them both
    struct Msg
    {
        void *appCtx;
        std::function<void (int, const std::vector<std::string>&, const std::vector<std::string>&)> *cb;
        Msg(void *ctx, std::function<void (int, const std::vector<std::string>&, const std::vector<std::string>&)> func)
            : appCtx(ctx), cb(new std::function<void (int, const std::vector<std::string>&, const std::vector<std::string>&)>(func))
        {}
        ~Msg()
        {
            delete cb;
        }
    };

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    virtual bool hasSessionCache() const { return false; }
    virtual void restoreSessions(std::vector<CachedSession> &&) { }
#endif

protected:
    MyMegaApi mApi;
    Mutex &mutex;
    void *appCtx;

    // This function is protected to prevent a wrong direct usage
    // It must be only used from WebsocketClient
    virtual bool wsResolveDNS(const char *hostname, std::function<void(int status, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)> f) = 0;
    virtual WebsocketsClientImpl *wsConnect(const char *ip, const char *host,
                                           int port, const char *path, bool ssl,
                                           WebsocketsClient *client) = 0;
    virtual int wsGetNoNameErrorCode() = 0;   // depends on the implementation
    friend WebsocketsClient;
};


// Abstract class that allows to manage a websocket connection.
// It's needed to subclass this class in order to receive callbacks

class WebsocketsClient
{
private:
    WebsocketsClientImpl *ctx;
    std::thread::id thread_id;

    // chatd/presenced use binary protocol, while SFU use text-based protocol (JSON)
    bool mWriteBinary = true;

public:
    WebsocketsClient(bool writeBinary = true);
    virtual ~WebsocketsClient();
    bool wsResolveDNS(WebsocketsIO *websocketIO, const char *hostname, std::function<void(int, const std::vector<std::string>&, const std::vector<std::string>&)> f);
    bool wsConnect(WebsocketsIO *websocketIO, const char *ip,
                   const char *host, int port, const char *path, bool ssl);
    int wsGetNoNameErrorCode(WebsocketsIO *websocketIO);
    bool wsSendMessage(char *msg, size_t len);  // returns true on success, false if error
    void wsDisconnect(bool immediate);
    bool wsIsConnected();
    void wsCloseCbPrivate(int errcode, int errtype, const char *preason, size_t reason_len);

    bool isWriteBinary() const;

    virtual void wsConnectCb() = 0;
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len) = 0;
    virtual void wsHandleMsgCb(char *data, size_t len) = 0;
    virtual void wsSendMsgCb(const char *data, size_t len) = 0;

    // Called after sending a message through the socket
    // (it may be implemented by clients that require messages to be sent individually and sequentially)
    virtual void wsProcessNextMsgCb() {}
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    virtual bool wsSSLsessionUpdateCb(const CachedSession &) { return false; }
#endif

    /* Public key pinning, by default this flag is enabled (true), it only should be disabled for testing purposes */
    static bool publicKeyPinning;
};

class WebsocketsClientImpl
{
protected:
    WebsocketsClient *client;
    WebsocketsIO::Mutex &mutex;
    bool disconnecting;

public:
    WebsocketsClientImpl(WebsocketsIO::Mutex &mutex, WebsocketsClient *client);
    virtual ~WebsocketsClientImpl();
    void wsConnectCb();
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len);
    void wsHandleMsgCb(char *data, size_t len);
    void wsSendMsgCb(const char *data, size_t len);
    void wsProcessNextMsgCb();
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    bool wsSSLsessionUpdateCb(const CachedSession &sess);
#endif

    virtual bool wsSendMessage(char *msg, size_t len) = 0;
    virtual void wsDisconnect(bool immediate) = 0;
    virtual bool wsIsConnected() = 0;
};

#endif /* websocketsIO_h */
