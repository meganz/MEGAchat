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
    const char* toString() { return toString(mPres); }
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
        case kBusy: return "DnD";
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
    OP_STATUSOVERRIDE = 2, //notifies own presence, sets 'manual' presence
    OP_SETSTATUS = 3,
    OP_ADDPEERS = 4,
    OP_DELPEERS = 5,
    OP_PEERSTATUS = 6 //notifies about presence of user
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
    enum State
    {
        kStateNew = 0,
        kStateDisconnected,
        kStateConnecting,
        kStateConnected
    };
    enum: uint16_t { kProtoVersion = 0x0001 };
protected:
    static ws_base_s sWebsocketContext;
    static bool sWebsockCtxInitialized;
    ws_t mWebSocket = nullptr;
    State mState = kStateNew;
    Listener* mListener;
    karere::Url mUrl;
    uint8_t mPingCode = 0;
    megaHandle mInactivityTimer = 0;
    int mInactivityBeats = 0;
    bool mTerminating = false;
    promise::Promise<void> mConnectPromise;
    uint8_t mFlags;
    karere::Id mMyHandle;
    karere::Presence mDynamicPresence = karere::Presence::kInvalid;
    karere::Presence mForcedPresence = karere::Presence::kInvalid;
    IdRefMap mCurrentPeers;
    void initWebsocketCtx();
    void setConnState(State newState);
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
    uint8_t presenceToDynFlags(karere::Presence pres);
    void setOnlineState(State state);
    void pingWithPresence();
    void pushPeers();
public:
    Client(Listener& listener, uint8_t flags);
    State state() { return mState; }
    bool isOnline() const
    {
        return (mWebSocket && (ws_get_state(mWebSocket) == WS_STATE_CONNECTED));
    }
    bool setPresence(karere::Presence pres, bool force);
    promise::Promise<void>
    connect(const std::string& url, karere::Id myHandle, IdRefMap&& peers,
        karere::Presence forcedPres,
        karere::Presence dynPres=karere::Presence::kOnline);
    void disconnect();
    void reset();
    void addPeer(karere::Id peer);
    void removePeer(karere::Id peer, bool force=false);
    ~Client();
};

class Listener
{
public:
    virtual void onConnStateChange(Client::State state) = 0;
    virtual void onPresence(karere::Id userid, karere::Presence pres) = 0;
    virtual void onOwnPresence(karere::Presence pres) = 0;
    virtual void onDestroy(){}
};

inline const char* Command::opcodeToStr(uint8_t opcode)
{
    switch (opcode)
    {
        case OP_PEERSTATUS: return "PEERSTATUS";
        case OP_SETSTATUS: return "SETSTATUS";
        case OP_KEEPALIVE: return "KEEPALIVE";
        case OP_STATUSOVERRIDE: return "STATUSOVERRIDE";
        case OP_HELLO: return "HELLO";
        case OP_ADDPEERS: return "ADDPEERS";
        case OP_DELPEERS: return "DELPEERS";
        default: return "(invalid)";
    }
}
static inline const char* connStateToStr(Client::State state)
{
    switch (state)
    {
    case Client::kStateDisconnected: return "Disconnected";
    case Client::kStateConnecting: return "Connecting";
    case Client::kStateConnected: return "Connected";
    default: return "(invalid)";
    }
}
}

#endif





