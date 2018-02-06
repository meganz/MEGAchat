#ifndef __PRESENCED_H__
#define __PRESENCED_H__

#include <stdint.h>
#include <string>
#include <buffer.h>
#include <base/promise.h>
#include <base/timers.hpp>
#include <karereId.h>
#include "url.h"
#include <base/trackDelete.h>
#include "net/websocketsIO.h"
#include <mega/backofftimer.h>
#include "sdkApi.h"

#define PRESENCED_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_presenced, fmtString, ##__VA_ARGS__)

struct ws_s;
struct ws_base_s;
typedef struct ws_s *ws_t;

class MyMegaApi;

enum: uint32_t { kPromiseErrtype_presenced = 0x339a92e5 }; //should resemble 'megapres'
namespace karere
{
class Client;    
class Presence
{
public:
    typedef uint8_t Code;
    enum: Code
    {
        kClear = 0,
        kOffline = 1,
        kAway = 2,
        kOnline = 3,
        kBusy = 4,
        kLast = kBusy,
        kInvalid = 0xff,
        kFlagsMask = 0xf0
    };
    Presence(Code pres = kInvalid): mPres(pres){}
    Code code() const { return mPres & ~kFlagsMask; }
    Code status() const { return code(); }
    operator Code() const { return code(); }
    Code raw() const { return mPres; }
    bool isValid() const { return mPres != kInvalid; }
    inline static const char* toString(Code pres);
    const char* toString() const { return toString(mPres); }
    bool canWebRtc() { return mPres & kClientCanWebrtc; }
    bool isMobile() { return mPres & kClientIsMobile; }
protected:
    Code mPres;
};

inline const char* Presence::toString(Code pres)
{
    switch (pres & ~kFlagsMask)
    {
        case kOffline: return "Offline";
        case kOnline: return "Online";
        case kAway: return "Away";
        case kBusy: return "Busy";
        case kClear: return "kClear";
        case kInvalid: return "kInvalid";
        default: return "(unknown)";
    }
}
}

namespace presenced
{
enum { kKeepaliveSendInterval = 25, kKeepaliveReplyTimeout = 15 };
enum: karere::Presence::Code
{
    kPresFlagsMask = 0xf0,
    kPresFlagInProgress = 0x10 // used internally
};
enum: uint8_t
{
    /**
      * @brief
      * This command is used to know connection status. It is client responsibility to start
      * KEEPALIVE interaction. Client sends a KEEPALIVE every 25 seconds and server answers immediately
      */
    OP_KEEPALIVE = 0,

    /**
      * @brief
      * C->S
      * After establishing a connection, the client identifies itself with an OPCODE_HELLO,
      * followed by a byte indicating the version of presenced protocol supported and the
      * client's capabilities: an OR of push-enabled device (0x40) and/or WebRTC capabilities (0x80).
      *
      * <protocolVersion> + <clientCapabilities>
      *
      * After this command, client sends OP_USERACTIVE and OP_ADDPEERS (with all peers and contacts)
      */
    OP_HELLO = 1,

    /**
      * @brief
      * C->S
      * This command is sent when the local user's activity status changes (including right
      * after connecting to presenced).
      * The only supported flag is in bit 0: CLIENT_USERACTIVE. All other bits must always be 0.
      *
      * <activeState> 1: active | 0: inactive
      *
      * @note This command is also known as OP_SETFLAGS.
      */
    OP_USERACTIVE = 3,

    /**
      * @brief
      * C->S
      * Client must send all of the peers it wants to see its status when the connection is
      * (re-)established. This command is sent after OP_HELLO and every time the user wants
      * to subscribe to the status of a new peer or contact.
      *
      * <numberOfPeers> <peerHandle1>...<peerHandleN>
      */
    OP_ADDPEERS = 4,

    /**
      * @brief
      * C->S
      * This command is sent when the client doesn't want to know the status of a peer or a contact
      * anymore. In example, the contact relationship is broken or a non-contact doesn't participate
      * in any groupchat any longer.
      *
      * <1> <peerHandle>
      */
    OP_DELPEERS = 5,

    /**
      * @brief
      * S->C
      * Server sends own user's status (necessary for synchronization between different clients)
      * and peers status requested by user
      *
      * <status> <peerHandle>
      */
    OP_PEERSTATUS = 6,

    /**
      * @brief
      * C->S
      * Client sends its preferences to server
      *
      * S->C
      * Upon (re-)connection, the client receives the current prefs and needs to update
      * its settings UI accordingly, unless the user has made a recent change, in which
      * case it must send OPCODE_PREFS with the new user preferences.
      *
      * It is broadcast to all connections of a user if one of the clients makes a change.
      *
      * <preferences>
      *
      * Preferences are encoded as a 16-bit little-endian word:
      *     bits 0-1: user status config (offline, away, online, do-not-disturb)
      *     bit 2: override active (presenced uses the configured status if this is set)
      *     bit 3: timeout active (ignored by presenced, relevant for clients)
      *     bits 4-15 timeout (pseudo floating point, calculated as:
      *
      *         autoawaytimeout = prefs >> 4;
      *         if (autoawaytimeout > 600)
      *         {
      *             autoawaytimeout = (autoawaytimeout - 600) * 60 + 600;
      *         }
      */
    OP_PREFS = 7
};

class Config
{
protected:
    karere::Presence mPresence = karere::Presence::kInvalid;
    bool mPersist = false;
    bool mAutoawayActive = false;
    time_t mAutoawayTimeout = 0;
public:
    Config(karere::Presence pres=karere::Presence::kInvalid,
          bool persist=false, bool aaEnabled=true, time_t aaTimeout=600)
        :mPresence(pres), mPersist(persist), mAutoawayActive(aaEnabled),
          mAutoawayTimeout(aaTimeout){}
    explicit Config(uint16_t code) { fromCode(code); }
    karere::Presence presence() const { return mPresence; }
    bool persist() const { return mPersist; }
    bool autoawayActive() const { return mAutoawayActive; }
    time_t autoawayTimeout() const { return mAutoawayTimeout; }
    void fromCode(uint16_t code);
    uint16_t toCode() const;
    std::string toString() const;
    friend class Client;
};

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
public:
    Command(): Buffer(){}
    Command(Command&& other): Buffer(std::forward<Buffer>(other)) {assert(!other.buf() && !other.bufSize() && !other.dataSize());}
    Command(uint8_t opcode, uint8_t reserve=10): Buffer(reserve+1) { write(0, opcode); }
    template<class T>
    Command&& operator+(const T& val)
    {
        append(val);
        return std::move(*this);
    }
    Command&& operator+(karere::Id id)
    {
        append(id.val);
        return std::move(*this);
    }
    uint8_t opcode() const { return read<uint8_t>(0); }
    const char* opcodeName() const { return opcodeToStr(opcode()); }
    void toString(char* buf, size_t bufsize) const;
    static inline const char* opcodeToStr(uint8_t code);
    virtual ~Command(){}
};
struct IdRefMap: public std::map<karere::Id, int>
{
    typedef std::map<karere::Id, int> Base;
    int insert(karere::Id id)
    {
        auto result = Base::insert(std::make_pair(id, 1));
        return result.second
            ? 1 //we just inserted the peer
            : ++result.first->second; //already have that peer
    }
};

class Listener;

class Client: public karere::DeleteTrackable, public WebsocketsClient
{
public:
    enum ConnState
    {
        kConnNew = 0,   // Initial state
        kFetchingURL,   // Request URL has been sent, waiting for response
        kDisconnected,  // Presenced is not connected, but reconnecting or in offline mode
        kResolvingDNS,  // Resolving DNS
        kConnecting,    // Establishing connection through websocket
        kConnected,     // Websocket connection is established
        kLoggedIn       // Connection is logged-in in presenced
    };
    enum: uint16_t { kProtoVersion = 0x0001 };

protected:
    ConnState mConnState = kConnNew;
    Listener* mListener;
    karere::Client *karereClient;
    karere::Url mUrl;
    std::string mIp;
    MyMegaApi *mApi;
    bool mHeartbeatEnabled = false;
    uint8_t mCapabilities;
    karere::Id mMyHandle;
    Config mConfig;
    bool mLastSentUserActive = false;
    time_t mTsLastUserActivity = 0;
    time_t mTsLastPingSent = 0;
    time_t mTsLastRecv = 0;
    time_t mTsLastSend = 0;
    bool mPrefsAckWait = false;
    IdRefMap mCurrentPeers;
    void initWebsocketCtx();
    void setConnState(ConnState newState);

    megaHandle mRetryTimerHandle = 0;
    ::mega::BackoffTimer bt;
    void resetConnection();
    void getPresenceURL();
    void retryGetPresenceURL();
    void resolveDNS();
    ApiPromise mResolveDnsPromise;
    void doConnect();
    void login();
    void retry();

    virtual void wsConnectCb();
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t reason_len);
    virtual void wsHandleMsgCb(char *data, size_t len);
    
    void onSocketClose(int ercode, int errtype, const std::string& reason);
    void enableInactivityTimer();
    void disableInactivityTimer();
    void notifyLoggedIn();
    void handleMessage(const StaticBuffer& buf); // Destroys the buffer content
    bool sendCommand(Command&& cmd);
    bool sendCommand(const Command& cmd);
    bool sendBuf(Buffer&& buf);
    void logSend(const Command& cmd);
    bool sendUserActive(bool active, bool force=false);
    bool sendPrefs();
    void setOnlineConfig(Config Config);
    void pushPeers();
    void configChanged();
    std::string prefsString() const;
    bool sendKeepalive(time_t now=0);
    
public:
    Client(MyMegaApi *api, karere::Client *client, Listener& listener, uint8_t caps);
    const Config& config() const { return mConfig; }
    bool isConfigAcknowledged() { return mPrefsAckWait; }
    bool isOnline() const { return (mConnState >= kConnected); }
    bool setPresence(karere::Presence pres);
    bool setPersist(bool enable);

    /** @brief Enables or disables autoaway
     * @param timeout The timeout in seconds after which the presence will be
     *  set to away
     */
    bool setAutoaway(bool enable, time_t timeout);
    void connect(karere::Id myHandle, IdRefMap&& peers, const Config& config);
    void disconnect();
    void retryPendingConnection();
    /** @brief Performs server ping and check for network inactivity.
     * Must be called externally in order to have all clients
     * perform pings at a single moment, to reduce mobile radio wakeup frequency */
    void heartbeat();
    void signalActivity(bool force = false);
    bool autoAwayInEffect();
    void addPeer(karere::Id peer);
    void removePeer(karere::Id peer, bool force=false);
    ~Client();
};

class Listener
{
public:
    virtual void onConnStateChange(Client::ConnState state) = 0;
    virtual void onPresenceChange(karere::Id userid, karere::Presence pres) = 0;
    virtual void onPresenceConfigChanged(const Config& Config, bool pending) = 0;
    virtual void onDestroy(){}
};

inline const char* Command::opcodeToStr(uint8_t opcode)
{
    switch (opcode)
    {
        case OP_PEERSTATUS: return "PEERSTATUS";
        case OP_USERACTIVE: return "USERACTIVE";
        case OP_KEEPALIVE: return "KEEPALIVE";
        case OP_PREFS: return "PREFS";
        case OP_HELLO: return "HELLO";
        case OP_ADDPEERS: return "ADDPEERS";
        case OP_DELPEERS: return "DELPEERS";
        default: return "(invalid)";
    }
}
static inline const char* connStateToStr(Client::ConnState state)
{
    switch (state)
    {
    case Client::kDisconnected: return "Disconnected";
    case Client::kResolvingDNS: return "Resolving DNS";
    case Client::kConnecting: return "Connecting";
    case Client::kConnected: return "Connected";
    case Client::kLoggedIn: return "Logged-in";
    case Client::kConnNew: return "New";
    case Client::kFetchingURL: return "Fetching URL";
    default: return "(invalid)";
    }
}
}

#endif
