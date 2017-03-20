#ifndef __PRESENCED_H__
#define __PRESENCED_H__

#include <libws.h>
#include <stdint.h>
#include <string>
#include <buffer.h>
#include <base/promise.h>
#include <base/timers.hpp>
#include <karereId.h>
#include "url.h"
#include <base/trackDelete.h>

#define PRESENCED_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_presenced, fmtString, ##__VA_ARGS__)
#define PRESENCED_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_presenced, fmtString, ##__VA_ARGS__)

enum: uint32_t { kPromiseErrtype_presenced = 0x339a92e5 }; //should resemble 'megapres'
namespace karere
{
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
    Presence(Code pres=kOffline): mPres(pres){}
    Code code() const { return mPres & ~kFlagsMask; }
    Code status() const { return code(); }
    operator Code() const { return code(); }
    Code raw() const { return mPres; }
    bool isValid() const { return mPres != kInvalid; }
    inline static const char* toString(Code pres);
    const char* toString() const { return toString(mPres); }
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
enum: karere::Presence::Code
{
    kPresFlagsMask = 0xf0,
    kPresFlagInProgress = 0x10 // used internally
};
enum: uint8_t
{
    OP_KEEPALIVE = 0,
    OP_HELLO = 1,
    OP_USERACTIVE = 3,
    OP_ADDPEERS = 4,
    OP_DELPEERS = 5,
    OP_PEERSTATUS = 6, //notifies about presence of user
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
    Config(uint16_t code) { fromCode(code); }
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

class Client: public karere::DeleteTrackable
{
public:
    enum ConnState
    {
        kConnNew = 0,
        kDisconnected,
        kConnecting,
        kConnected
    };
    enum: uint16_t { kProtoVersion = 0x0001 };
protected:
    static ws_base_s sWebsocketContext;
    static bool sWebsockCtxInitialized;
    ws_t mWebSocket = nullptr;
    ConnState mConnState = kConnNew;
    Listener* mListener;
    karere::Url mUrl;
    bool mHeartbeatEnabled = false;
    uint8_t mHeartBeats = 0;
    bool mPacketReceived = true; //used for connection activity detection
    bool mTerminating = false;
    promise::Promise<void> mConnectPromise;
    uint8_t mCapabilities;
    karere::Id mMyHandle;
    Config mConfig;
    bool mLastSentUserActive = false;
    time_t mTsLastUserActivity = time_t(NULL);
    bool mPrefsAckWait = false;
    IdRefMap mCurrentPeers;
    void initWebsocketCtx();
    void setConnState(ConnState newState);
    static void websockConnectCb(ws_t ws, void* arg);
    static void websockCloseCb(ws_t ws, int errcode, int errtype, const char *reason,
        size_t reason_len, void *arg);
    void onSocketClose(int ercode, int errtype, const std::string& reason);
    promise::Promise<void> reconnect(const std::string& url=std::string());
    void enableInactivityTimer();
    void disableInactivityTimer();
    void handleMessage(const StaticBuffer& buf); // Destroys the buffer content
    bool sendCommand(Command&& cmd);
    bool sendCommand(const Command& cmd);
    void login();
    bool sendBuf(Buffer&& buf);
    void logSend(const Command& cmd);
    bool sendUserActive(bool active, bool force=false);
    bool sendPrefs();
    void setOnlineConfig(Config Config);
    void pingWithPresence();
    void pushPeers();
    void configChanged();
    std::string prefsString() const;
public:
    Client(Listener& listener, uint8_t caps);
    const Config& config() const { return mConfig; }
    bool isConfigAcknowledged() { return mPrefsAckWait; }
    bool isOnline() const
    {
        return (mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
    }
    bool setPresence(karere::Presence pres);
    bool setPersist(bool enable);
    bool setAutoaway(bool enable, uint16_t timeout);
    promise::Promise<void>
    connect(const std::string& url, karere::Id myHandle, IdRefMap&& peers,
        const Config& Config);
    void disconnect();
    void reset();
    /** @brief Performs server ping and check for network inactivity.
     * Must be called externally in order to have all clients
     * perform pings at a single moment, to reduce mobile radio wakeup frequency */
    void heartbeat();
    void signalActivity(bool force = false);
    bool checkEnableAutoaway();
    void addPeer(karere::Id peer);
    void removePeer(karere::Id peer, bool force=false);
    ~Client();
};

class Listener
{
public:
    virtual void onConnStateChange(Client::ConnState state) = 0;
    virtual void onOwnPresence(karere::Presence pres) = 0;
    virtual void onPeerPresence(karere::Id userid, karere::Presence pres) = 0;
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
    case Client::kConnecting: return "Connecting";
    case Client::kConnected: return "Connected";
    case Client::kConnNew: return "New";
    default: return "(invalid)";
    }
}
}

#endif





