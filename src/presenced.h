#ifndef __PRESENCED_H__
#define __PRESENCED_H__

#include <stdint.h>
#include <string>
#include <buffer.h>
#include <base/promise.h>
#include <base/timers.hpp>
#include <karereId.h>
#include <url.h>
#include <base/trackDelete.h>
#include <net/websocketsIO.h>
#include <base/retryHandler.h>

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

    // capabilities
    bool canWebRtc() { return mPres & kClientCanWebrtc; }
    bool isMobile() { return mPres & kClientIsMobile; }
    bool canLastGreen() { return mPres & kClientSupportLastGreen; }

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
enum {
    kKeepaliveSendInterval = 25,
    kKeepaliveReplyTimeout = 15,
    kConnectTimeout = 30
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
      * @brief (Deprecated)
      * C->S
      * Client must send all of the peers it wants to see its status when the connection is
      * (re-)established. This command is sent after OP_HELLO and every time the user wants
      * to subscribe to the status of a new peer or contact.
      *
      * <numberOfPeers> <peerHandle1>...<peerHandleN>
      */
    OP_ADDPEERS = 4,

     /**
     * @brief (Deprecated)
     * C->S
     * This command is sent when the client doesn't want a peer to see its status
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
      *     bits 4-14 timeout (pseudo floating point, calculated as:
      *
      *         autoawaytimeout = prefs >> 4;
      *         if (autoawaytimeout > 600)
      *         {
      *             autoawaytimeout = (autoawaytimeout - 600) * 60 + 600;
      *         }
      *
      *     bit 15: flag to enable/disable visibility of last-green timestamp
      */
    OP_PREFS = 7,

    /**
      * @brief
      * C->S
      * This command is sent when the client wants to add a peer to see its status. The list
      * is established with command OP_SNSETPEERS
      *
      * The sn parameter is the sequence-number as provided by API, in order to avoid race-conditions
      * between different clients sending outdated list of users. If presenced receives an outdated
      * list, the command will be discarded.
      *
      * <sn.8> <numberOfPeers.4> <peerHandle1.8>...<peerHandleN.8>
      */
    OP_SNADDPEERS = 8,

     /**
       * @brief
       * C->S
       * This command is sent when the client doesn't want a peer to see its status
       * anymore. In example, the contact relationship is broken or a non-contact doesn't participate
       * in any groupchat any longer.
       *
       * The sn parameter is the sequence-number as provided by API, in order to avoid race-conditions
       * between different clients sending outdated list of users. If presenced receives an outdated
       * list, the command will be discarded.
       *
       * <sn.8> <1.4> <peerHandle.8>
       */
    OP_SNDELPEERS = 9,

    /**
      * @brief
      * C->S
      * This command is sent when the client wants to know the last time that a user has been green
      *
      * <peerHandle.8>
      *
      * S->C
      * This command is sent by server as answer of a previous request from the client.
      * There will be no reply if the user was not ever seen by presenced
      * Maximun time value is 65535 minutes
      *
      * <peerHandle.8><minutes.2>
      */
   OP_LASTGREEN = 10,

    /**
      * @brief
      * C->S
      * Client must send all of the peers it wants to see its status when the connection is
      * (re-)established. This command is sent after OP_HELLO and OP_PREF (if it would be necessary)
      *
      * The sn parameter is the sequence-number as provided by API, in order to avoid race-conditions
      * between different clients sending outdated list of users. If presenced receives an outdated
      * list, the command will be discarded.
      *
      * <sn.8> <numberOfPeers.4> <peerHandle1.8>...<peerHandleN.8>
      */
    OP_SNSETPEERS = 12
};

class Config
{
protected:
    karere::Presence mPresence = karere::Presence::kInvalid;
    bool mPersist = false;
    bool mAutoawayActive = false;
    time_t mAutoawayTimeout = 0;
    bool mLastGreenVisible = false;

public:
    enum { kMaxAutoawayTimeout = 87420 };   // (in seconds, 1.447 minutes + 600 seconds)
    enum { kLastGreenVisibleMask = 0x8000 }; // mask for bit 15 in prefs

    Config(karere::Presence pres=karere::Presence::kInvalid,
          bool persist=false, bool aaEnabled=true, time_t aaTimeout=600, bool lastGreenVisible = false)
        : mPresence(pres), mPersist(persist), mAutoawayActive(aaEnabled),
          mAutoawayTimeout(aaTimeout), mLastGreenVisible(lastGreenVisible){}
    explicit Config(uint16_t code) { fromCode(code); }

    karere::Presence presence() const { return mPresence; }
    bool persist() const { return mPersist; }
    bool autoawayActive() const { return mAutoawayActive; }
    time_t autoawayTimeout() const { return mAutoawayTimeout; }
    bool lastGreenVisible() const { return mLastGreenVisible;}

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

class Client: public karere::DeleteTrackable, public WebsocketsClient,
        public ::mega::MegaGlobalListener
{
public:
    enum ConnState
    {
        kConnNew = 0,
        kDisconnected,
        kResolving,
        kConnecting,
        kConnected,
        kLoggedIn
    };
    enum: uint16_t { kProtoVersion = 0x0001 };

protected:
    MyMegaApi *mApi;
    karere::Client *mKarereClient;
    Listener* mListener;
    uint8_t mCapabilities;

    /** Current state of the connection */
    ConnState mConnState = kConnNew;

    /** When enabled, hearbeat() method is called periodically */
    bool mHeartbeatEnabled = false;

    /** URL retrieved from API to establish the connection */
    karere::Url mUrl;

    /** DNS cache to store resolved IPs */
    DNScache &mDNScache;

    /** Target IP address being used for the reconnection in-flight */
    std::string mTargetIp;

    /** ip version to try first (both are tried) */
    bool usingipv6 = false;

    /** RetryController that manages the reconnection's attempts */
    std::unique_ptr<karere::rh::IRetryController> mRetryCtrl;

    /** Input promise for the RetryController
     *  - If it fails: a new attempt is schedulled
     *  - If it success: the reconnection is taken as done */
    promise::Promise<void> mConnectPromise;

    /** Handler of the timeout for the connection establishment */
    megaHandle mConnectTimer = 0;

    /** True if last USERACTIVE was 1 (active), false if it was 0 (idle) */
    bool mLastSentUserActive = false;

    /** Timestamp of the last USERACTIVE sent to presenced */
    time_t mTsLastUserActivity = 0;

    /** Timestamp of the last KEEPALIVE sent to presenced */
    time_t mTsLastPingSent = 0;

    /** Timestamp of the last received data from presenced */
    time_t mTsLastRecv = 0;

    /** Timestamp of the last sent data to presenced */
    time_t mTsLastSend = 0;

    /** Configuration of presence for the user */
    Config mConfig;

    /** True if a new configuration (PREFS) has been sent, but not yet acknowledged */
    bool mPrefsAckWait = false;

    /** List of peers that are allowed to see our presence's status
     * (currently, it includes contacts and any user in our groupchats, except ex-contacts) */
    IdRefMap mCurrentPeers;

    /** Map of userids (key) and last green (value) of any contact or any user in our groupchats, except ex-contacts */
    std::map<uint64_t, time_t> mPeersLastGreen;

    /** Map of chatids (key) and the list of peers (value) in every chat (updated only from API) */
    std::map<uint64_t, karere::SetOfIds> mChatMembers;

    /** Map of userid of contacts (key) and their visibility (value) (updated only from API)
     * @note: ex-contacts are included.
     */
    std::map<uint64_t, int> mContacts;

    /** Sequence-number for the list of peers and contacts above (initialized upon completion of catch-up phase) */
    karere::Id mLastScsn = karere::Id::inval();

    void setConnState(ConnState newState);

    virtual void wsConnectCb();
    virtual void wsCloseCb(int errcode, int errtype, const char *preason, size_t /*preason_len*/);
    virtual void wsHandleMsgCb(char *data, size_t len);
    
    void onSocketClose(int ercode, int errtype, const std::string& reason);
    promise::Promise<void> reconnect();
    void abortRetryController();
    void handleMessage(const StaticBuffer& buf); // Destroys the buffer content
    bool sendCommand(Command&& cmd);
    bool sendCommand(const Command& cmd);    
    bool sendBuf(Buffer&& buf);
    void logSend(const Command& cmd);

    void login();
    bool sendUserActive(bool active, bool force=false);
    bool sendKeepalive(time_t now=0);

    // config management
    bool sendPrefs();
    void configChanged();

    // peers management
    void addPeer(karere::Id peer);
    void removePeer(karere::Id peer, bool force=false);
    void pushPeers();
    bool isExContact(uint64_t userid);

    // mega::MegaGlobalListener interface, called by worker thread
    virtual void onChatsUpdate(::mega::MegaApi*, ::mega::MegaTextChatList* rooms);
    virtual void onUsersUpdate(::mega::MegaApi*, ::mega::MegaUserList* users);
    virtual void onEvent(::mega::MegaApi* api, ::mega::MegaEvent* event);
    
public:
    Client(MyMegaApi *api, karere::Client *client, Listener& listener, uint8_t caps);

    // config management
    const Config& config() const { return mConfig; }
    bool isConfigAcknowledged() { return mPrefsAckWait; }
    bool setPresence(karere::Presence pres);
    /** @brief Enables or disables autoaway
     * @param timeout The timeout in seconds after which the presence will be
     *  set to away
     */
    bool setAutoaway(bool enable, time_t timeout);
    bool autoAwayInEffect();
    bool setPersist(bool enable);
    bool setLastGreenVisible(bool enable);
    bool requestLastGreen(karere::Id userid);

    // connection's management
    bool isOnline() const { return (mConnState >= kConnected); }
    promise::Promise<void>
    connect(const std::string& url, const Config& Config);
    void disconnect();
    void doConnect();
    void retryPendingConnection(bool disconnect);

    /** @brief Performs server ping and check for network inactivity.
     * Must be called externally in order to have all clients
     * perform pings at a single moment, to reduce mobile radio wakeup frequency */
    void heartbeat();

    /** Tells presenced that there's user's activity (notified by the app) */
    void signalActivity();

    /** @brief Updates user last green if it's more recent than the current value.*/
    bool updateLastGreen(karere::Id userid, time_t lastGreen);
    time_t getLastGreen(karere::Id userid);

    ~Client();
};

class Listener
{
public:
    virtual void onConnStateChange(Client::ConnState state) = 0;
    virtual void onPresenceChange(karere::Id userid, karere::Presence pres) = 0;
    virtual void onPresenceConfigChanged(const Config& Config, bool pending) = 0;
    virtual void onPresenceLastGreenUpdated(karere::Id userid) = 0;
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
        case OP_SNADDPEERS: return "SNADDPEERS";
        case OP_SNDELPEERS: return "SNDELPEERS";
        case OP_LASTGREEN: return "LASTGREEN";
        default: return "(invalid)";
    }
}
static inline const char* connStateToStr(Client::ConnState state)
{
    switch (state)
    {
    case Client::kDisconnected: return "Disconnected";
    case Client::kResolving: return "Resolving";
    case Client::kConnecting: return "Connecting";
    case Client::kConnected: return "Connected";
    case Client::kLoggedIn: return "Logged-in";
    case Client::kConnNew: return "New";
    default: return "(invalid)";
    }
}
}

#endif
