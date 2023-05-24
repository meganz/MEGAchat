#ifndef KARERE_DISABLE_WEBRTC
#ifndef SFU_H
#define SFU_H
#include <thread>
#include <base/retryHandler.h>
#include <net/websocketsIO.h>
#include <karereId.h>
#include <rapidjson/document.h>
#include "rtcCrypto.h"
#include <base/timers.hpp>

#define SFU_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_ERROR_NO_STATS(fmtString,...) KARERE_LOG_ERROR(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_sfu, fmtString, ##__VA_ARGS__); \
    char logLine[300]; \
    snprintf(logLine, 300, fmtString, ##__VA_ARGS__); \
    mCall.logError(logLine);

namespace sfu
{
/** SFU Protocol Versions:
 * - Version 0: initial version
 *
 * - Version 1 (never released for native clients):
 *      + Forward secrecy (ephemeral X25519 EC key pair for each session)
 *      + Dynamic audio routing
 *      + Waiting rooms
 *
 * - Version 2 (contains all features from V1):
 *      + Change AES-GCM by AES-CBC with Zero iv
 */
enum class SfuProtocol: uint32_t
{
    SFU_PROTO_INVAL    = UINT32_MAX,
    SFU_PROTO_V0       = 0,
    SFU_PROTO_V1       = 1,
    SFU_PROTO_V2       = 2,
};

// own client SFU protocol version
constexpr sfu::SfuProtocol MY_SFU_PROTOCOL_VERSION = SfuProtocol::SFU_PROTO_V2;

// returns true if provided version as param is equal to SFU current version
static bool isCurrentSfuVersion(sfu::SfuProtocol v) { return v == SfuProtocol::SFU_PROTO_V2; }

// returns true if provided version as param is SFU version V0 (forward secrecy is not supported)
static bool isInitialSfuVersion(sfu::SfuProtocol v) { return v == SfuProtocol::SFU_PROTO_V0; }

// returns true if provided version as param is a valid SFU version
static bool isValidSfuVersion(sfu::SfuProtocol v) { return v != SfuProtocol::SFU_PROTO_INVAL; }

// NOTE: This queue, must be always managed from a single thread.
// The classes that instantiates it, are responsible to ensure that.
// In case we need to access to it from another thread, we would need to implement
// a synchronization mechanism (like a mutex).
class CommandsQueue : public std::deque<std::string>
{
protected:
    bool isSending = false;

public:
    CommandsQueue();
    bool sending();
    void setSending(bool sending);
    std::string pop();
};

class Peer
{
public:
    Peer(const karere::Id& peerid, const sfu::SfuProtocol sfuProtoVersion, const unsigned avFlags, const std::vector<std::string>* ivs = nullptr, const Cid_t cid = 0, const bool isModerator = false);
    Peer(const Peer& peer);

    Cid_t getCid() const;
    void setCid(Cid_t cid);    // called from handleAnswerCommand() only for setting cid of Call::mMyPeer

    const karere::Id& getPeerid() const;

    karere::AvFlags getAvFlags() const;
    void setAvFlags(karere::AvFlags flags);

    bool isModerator() const;
    void setModerator(bool isModerator);

    bool hasAnyKey() const;
    Keyid_t getCurrentKeyId() const;
    std::string getKey(Keyid_t keyid) const;
    void addKey(Keyid_t keyid, const std::string& key);
    void resetKeys();
    const std::vector<std::string>& getIvs() const;
    void setIvs(const std::vector<std::string>& ivs);
    void setEphemeralPubKeyDerived(const std::string& key);

    // returns derived peer's ephemeral key if available
    std::string getEphemeralPubKeyDerived() const;

    // returns a promise that will be resolved/rejected when peer's ephemeral key is verified and derived
    const promise::Promise<void>& getEphemeralPubKeyPms() const;

    // returns the SFU protocol version used by the peer
    sfu::SfuProtocol getPeerSfuVersion() const { return mSfuPeerProtoVersion; }

protected:
    Cid_t mCid = K_INVALID_CID;
    karere::Id mPeerid;
    karere::AvFlags mAvFlags = karere::AvFlags::kEmpty;
    Keyid_t mCurrentkeyId = 0; // we need to know the current keyId for frame encryption
    std::map<Keyid_t, std::string> mKeyMap;

    // initialization vector
    std::vector<std::string> mIvs;

    /*
     * Moderator role for this call
     *
     * The information about moderator role is only updated from SFU.
     *  1) ANSWER command: When user receives Answer call, SFU will provide a list with current moderators for this call,
     *     independently if those users currently has answered or not the call
     *  2) ADDMOD command: informs that a peer has been granted with moderator role
     *  3) DELMOD command: informs that a peer has been removed it's moderator role
     *
     *  Participants with moderator role can:
     *  - End groupal calls for all participants
     *  - Approve/reject speaker requests
     */
    bool mIsModerator = false;

    // peer ephemeral key derived
    std::string mEphemeralPubKeyDerived;

    // this promise is resolved/rejected when peer's ephemeral key is verified and derived
    mutable promise::Promise<void> mEphemeralKeyPms;

    // SFU protocol version used by the peer
    sfu::SfuProtocol mSfuPeerProtoVersion = sfu::SfuProtocol::SFU_PROTO_INVAL;
};

class TrackDescriptor
{
public:
    static constexpr uint32_t invalidMid = UINT32_MAX;
    uint32_t mMid = invalidMid;
    bool mReuse = false;
};

class Sdp
{
public:
    struct Track
    {
        // TODO: document what is each variable
        std::string mType;  // "a" for audio, "v" for video
        uint64_t mMid;
        std::string mDir;   // direction of track (sendrecv, recvonly, sendonly)
        std::string mSid;
        std::string mId;
        std::vector<std::string> mSsrcg;
        std::vector<std::pair<uint64_t, std::string>> mSsrcs;
    };

    // ctor from session-description provided by WebRTC (string format)
    Sdp(const std::string& sdp, int64_t mungedTrackIndex = -1);

    // ctor from session-description from SFU (JSON format)
    Sdp(const rapidjson::Value& sdp);

    // restores the original (webrtc) session-description string from a striped (JSON) string (which got condensed for saving bandwidth)
    std::string unCompress();

    const std::vector<Track>& tracks() const { return mTracks; }
    const std::map<std::string, std::string>& data() const { return mData; }

private:
    // process 'lines' of (webrtc) session description from 'position', for 'type' (atpl, vtpl) and adds them to 'mData'
    // it returns the final position after reading lines
    unsigned int createTemplate(const std::string& type, const std::vector<std::string> lines, unsigned int position);

    // Enable SVC by modifying SDP message, generated using createOffer, and before providing it to setLocalDescription.
    void mungeSdpForSvc(Sdp::Track &track);

    // process 'lines' of (webrtc) session description from 'position' and adds them to 'mTracks'
    unsigned int addTrack(const std::vector<std::string>& lines, unsigned int position);

    // returns the position of the next line starting with "m"
    unsigned int nextMline(const std::vector<std::string>& lines, unsigned int position);
    std::string nextWord(const std::string& line, unsigned int start, unsigned int &charRead);

    // returns the Track represented by a JSON string
    Track parseTrack(const rapidjson::Value &value) const;

    // convenience method to uncompress each track from JSON session-description (see unCompress() )
    std::string unCompressTrack(const Track &track, const std::string& tpl);

    // maps id ("cmn", "atpl", "vtpl") to the corresponding session description
    std::map<std::string, std::string> mData;

    // array of tracks for audio and video
    std::vector<Track> mTracks;

    static const std::string endl;
};


/**
 * @brief The SfuInterface class
 *
 * Defines the handlers that should be implemented in order to manage the different
 * commands received by the client from the SFU server.
 */
class SfuInterface
{
public:
    // SFU -> Client commands
    virtual bool handleAvCommand(Cid_t cid, unsigned av, uint32_t amid) = 0;   // audio/video/on-hold flags
    virtual bool handleAnswerCommand(Cid_t cid, std::shared_ptr<Sdp> spd, uint64_t, std::vector<Peer>& peers, const std::map<Cid_t, std::string>& keystrmap, const std::map<Cid_t, TrackDescriptor>& vthumbs, const std::map<Cid_t, TrackDescriptor>& speakers, std::set<karere::Id>& moderators, bool ownMod) = 0;
    virtual bool handleKeyCommand(const Keyid_t& keyid, const Cid_t& cid, const std::string& key) = 0;
    virtual bool handleVThumbsCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleVThumbsStartCommand() = 0;
    virtual bool handleVThumbsStopCommand() = 0;
    virtual bool handleHiResCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleHiResStartCommand() = 0;
    virtual bool handleHiResStopCommand() = 0;
    virtual bool handleSpeakReqsCommand(const std::vector<Cid_t>&) = 0;
    virtual bool handleSpeakReqDelCommand(Cid_t cid) = 0;
    virtual bool handleSpeakOnCommand(Cid_t cid) = 0;
    virtual bool handleSpeakOffCommand(Cid_t cid) = 0;
    virtual bool handleModAdd (uint64_t userid) = 0;
    virtual bool handleModDel (uint64_t userid) = 0;
    virtual bool handleHello (const Cid_t userid, const unsigned int nAudioTracks, const unsigned int nVideoTracks,
                                       const std::set<karere::Id>& mods, const bool wr, const bool allowed,
                                       const std::map<karere::Id, bool>& wrUsers) = 0;
    virtual bool handleWrDump(const std::map<karere::Id, bool>& users) = 0;
    virtual bool handleWrEnter(const std::map<karere::Id, bool>& users) = 0;
    virtual bool handleWrLeave(const karere::Id& /*user*/) = 0;
    virtual bool handleWrAllow(const Cid_t& cid, const std::set<karere::Id>& mods) = 0;
    virtual bool handleWrDeny(const std::set<karere::Id>& mods) = 0;
    virtual bool handleWrUsersAllow(const std::set<karere::Id>& users) = 0;
    virtual bool handleWrUsersDeny(const std::set<karere::Id>& users) = 0;

    // called when the connection to SFU is established
    virtual bool handlePeerJoin(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string> &ivs) = 0;
    virtual bool handlePeerLeft(Cid_t cid, unsigned termcode) = 0;
    virtual bool handleBye(const unsigned& termCode, const bool& wr, const std::string& errMsg) = 0;
    virtual void onSfuDisconnected() = 0;
    virtual void onSendByeCommand() = 0;

    // handle errors at higher level (connection to SFU -> {err:<code>} )
    virtual bool error(unsigned int, const std::string&) = 0;

    // process Deny notification from SFU
    virtual bool processDeny(const std::string& cmd, const std::string& msg) = 0;

    // send error to server, for debugging purposes
    virtual void logError(const char* error) = 0;
};

class Command
{
public:
    virtual bool processCommand(const rapidjson::Document& command) = 0;
    static const std::string COMMAND_IDENTIFIER;
    static const std::string ERROR_IDENTIFIER;
    static const std::string WARN_IDENTIFIER;
    static const std::string DENY_IDENTIFIER;
    virtual ~Command();
    static std::string binaryToHex(uint64_t value);
    static uint64_t hexToBinary(const std::string& hex);
    static std::vector<mega::byte> hexToByteArray(const std::string &hex);
    void parseUsersArray(std::set<karere::Id> &moderators, rapidjson::Value::ConstMemberIterator &it) const;
    void parseTracks(const rapidjson::Document &command, const std::string& arrayName, std::map<Cid_t, TrackDescriptor>& tracks) const;

protected:
    Command(SfuInterface& call);
    bool parseUsersMap(std::map<karere::Id, bool> &wrUsers, const rapidjson::Value &obj) const;
    static uint8_t hexDigitVal(char value);

    SfuInterface& mCall;
};

typedef std::function<bool(karere::Id, unsigned, uint32_t)> AvCompleteFunction;
class AVCommand : public Command
{
public:
    AVCommand(const AvCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    AvCompleteFunction mComplete;
};

class AnswerCommand : public Command
{
public:
    typedef std::function<bool(Cid_t, std::shared_ptr<Sdp>, uint64_t, std::vector<Peer>&, const std::map<Cid_t, std::string>& keystrmap, std::map<Cid_t, TrackDescriptor>, std::map<Cid_t, TrackDescriptor>, std::set<karere::Id>&, bool)> AnswerCompleteFunction;
    AnswerCommand(const AnswerCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    AnswerCompleteFunction mComplete;

private:
    void parsePeerObject(std::vector<Peer>& peers, std::map<Cid_t, std::string>& keystrmap, const std::set<karere::Id>& moderators, rapidjson::Value::ConstMemberIterator& it) const;
};

typedef std::function<bool(const Keyid_t&, const Cid_t&, const std::string&)> KeyCompleteFunction;
class KeyCommand : public Command
{
public:
    KeyCommand(const KeyCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    KeyCompleteFunction mComplete;
    constexpr static Keyid_t maxKeyId = static_cast<Keyid_t>(~0);
};

typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> VtumbsCompleteFunction;
class VthumbsCommand : public Command
{
public:
    VthumbsCommand(const VtumbsCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsCompleteFunction mComplete;
};

typedef std::function<bool(void)> VtumbsStartCompleteFunction;
class VthumbsStartCommand : public Command
{
public:
    VthumbsStartCommand(const VtumbsStartCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsStartCompleteFunction mComplete;
};

typedef std::function<bool(void)> VtumbsStopCompleteFunction;
class VthumbsStopCommand : public Command
{
public:
    VthumbsStopCommand(const VtumbsStopCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsStopCompleteFunction mComplete;
};

typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> HiresCompleteFunction;
class HiResCommand : public Command
{
public:
    HiResCommand(const HiresCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiresCompleteFunction mComplete;
};

typedef std::function<bool(void)> HiResStartCompleteFunction;
class HiResStartCommand : public Command
{
public:
    HiResStartCommand(const HiResStartCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiResStartCompleteFunction mComplete;
};

typedef std::function<bool(void)> HiResStopCompleteFunction;
class HiResStopCommand : public Command
{
public:
    HiResStopCommand(const HiResStopCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiResStopCompleteFunction mComplete;
};

typedef std::function<bool(const std::vector<Cid_t>&)> SpeakReqsCompleteFunction;
class SpeakReqsCommand : public Command
{
public:
    SpeakReqsCommand(const SpeakReqsCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakReqsCompleteFunction mComplete;
};

typedef std::function<bool(karere::Id)> SpeakReqDelCompleteFunction;
class SpeakReqDelCommand : public Command
{
public:
    SpeakReqDelCommand(const SpeakReqDelCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakReqDelCompleteFunction mComplete;
};

typedef std::function<bool(Cid_t cid)> SpeakOnCompleteFunction;
class SpeakOnCommand : public Command
{
public:
    SpeakOnCommand(const SpeakOnCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakOnCompleteFunction mComplete;
};

typedef std::function<bool(Cid_t cid)> SpeakOffCompleteFunction;
class SpeakOffCommand : public Command
{
public:
    SpeakOffCommand(const SpeakOffCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakOffCompleteFunction mComplete;
};

typedef std::function<bool(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string> &ivs)> PeerJoinCommandFunction;
class PeerJoinCommand : public Command
{
public:
    PeerJoinCommand(const PeerJoinCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    PeerJoinCommandFunction mComplete;
};

typedef std::function<bool(Cid_t cid, unsigned termcode)> PeerLeftCommandFunction;
class PeerLeftCommand : public Command
{
public:
    PeerLeftCommand(const PeerLeftCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    PeerLeftCommandFunction mComplete;
};

class ByeCommand : public Command
{
public:
    typedef std::function<bool(const unsigned& termCode, const bool& wr, const std::string& errMsg)> ByeCommandFunction;
    ByeCommand(const ByeCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ByeCommandFunction mComplete;
};

typedef std::function<bool(uint64_t userid)> ModAddCommandFunction;
class ModAddCommand : public Command
{
public:
    ModAddCommand(const ModAddCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ModAddCommandFunction mComplete;
};

typedef std::function<bool(uint64_t userid)> ModDelCommandFunction;
class ModDelCommand : public Command
{
public:
    ModDelCommand(const ModDelCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ModDelCommandFunction mComplete;
};

class HelloCommand : public Command
{
public:
    typedef std::function<bool(const Cid_t userid,
                               const unsigned int nAudioTracks,
                               const unsigned int nVideoTracks,
                               const std::set<karere::Id>& mods,
                               const bool wr,
                               const bool allowed,
                               const std::map<karere::Id, bool>& wrUsers)>HelloCommandFunction;

    HelloCommand(const HelloCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HelloCommandFunction mComplete;
};

typedef std::function<bool(const std::map<karere::Id, bool>& users)>WrDumpCommandFunction;
class WrDumpCommand: public Command
{
public:
    WrDumpCommand(const WrDumpCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrDumpCommandFunction mComplete;
};

typedef std::function<bool(const std::map<karere::Id, bool>& users)>WrEnterCommandFunction;
class WrEnterCommand: public Command
{
public:
    WrEnterCommand(const WrEnterCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrEnterCommandFunction mComplete;
};

typedef std::function<bool(const karere::Id& user)>WrLeaveCommandFunction;
class WrLeaveCommand: public Command
{
public:
    WrLeaveCommand(const WrLeaveCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrLeaveCommandFunction mComplete;
};

typedef std::function<bool(const Cid_t& cid, const std::set<karere::Id>& mods)>WrAllowCommandFunction;
class WrAllowCommand: public Command
{
public:
    WrAllowCommand(const WrAllowCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrAllowCommandFunction mComplete;
};

typedef std::function<bool(const std::set<karere::Id>& mods)>WrDenyCommandFunction;
class WrDenyCommand: public Command
{
public:
    WrDenyCommand(const WrDenyCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrDenyCommandFunction mComplete;
};

typedef std::function<bool(const std::set<karere::Id>& users)>WrUsersAllowCommandFunction;
class WrUsersAllowCommand: public Command
{
public:
    WrUsersAllowCommand(const WrUsersAllowCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrUsersAllowCommandFunction mComplete;
};

typedef std::function<bool(const std::set<karere::Id>& users)>WrUsersDenyCommandFunction;
class WrUsersDenyCommand: public Command
{
public:
    WrUsersDenyCommand(const WrUsersDenyCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrUsersDenyCommandFunction mComplete;
};

/**
 * @brief This class allows to handle a connection to the SFU
 *
 * Each call requires its own connection to the SFU in order to handle
 * call signalling.
 *
 * It implements the interface to communicate via websockets
 * in text-mode using JSON protocol (compared with binary protocol used by
 * chatd and presenced).
 *
 * Additionally, the JSON commands are sent to the SFU sequeniatlly. In other
 * words, commands are sent one by one, never combined in a single packet.
 * In consequence, this class maintains a queue of commands.
 *
 * TODO: integrate the DNS cache within the SfuConnection -> IPs and TLS sessions speed up connections significantly
 */
class SfuConnection : public karere::DeleteTrackable, public WebsocketsClient
{
    // client->sfu commands
    static const std::string CSFU_JOIN;
    static const std::string CSFU_SENDKEY;
    static const std::string CSFU_AV;
    static const std::string CSFU_GET_VTHUMBS;
    static const std::string CSFU_DEL_VTHUMBS;
    static const std::string CSFU_GET_HIRES;
    static const std::string CSFU_DEL_HIRES;
    static const std::string CSFU_HIRES_SET_LO;
    static const std::string CSFU_LAYER;
    static const std::string CSFU_SPEAK_RQ;
    static const std::string CSFU_SPEAK_RQ_DEL;
    static const std::string CSFU_SPEAK_DEL;
    static const std::string CSFU_BYE;
    static const std::string CSFU_WR_PUSH;
    static const std::string CSFU_WR_ALLOW;
    static const std::string CSFU_WR_KICK;

public:
    struct SfuData
    {
        public:
            enum
            {
                SFU_INVALID         = -1,
                SFU_COMMAND         = 0,
                SFU_ERROR           = 1,
                SFU_WARN            = 2,
                SFU_DENY            = 3,
            };

            int32_t notificationType = SFU_INVALID;
            std::string notification;
            std::string msg;
            int32_t errCode;
    };

    enum ConnState
    {
        kConnNew = 0,
        kDisconnected,
        kResolving,
        kConnecting,
        kConnected,
        kJoining,       // after sending JOIN
        kJoined,        // after receiving ANSWER
    };

    static constexpr uint8_t kConnectTimeout = 30;           // (in seconds) timeout reconnection to succeeed
    static constexpr uint8_t kNoMediaPathTimeout = 6;        // (in seconds) disconnect call upon no UDP connectivity after this period
    SfuConnection(karere::Url&& sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface& call, DNScache &dnsCache);
    ~SfuConnection();
    void setIsSendingBye(bool sending);
    void setMyCid(const Cid_t& cid);
    Cid_t getMyCid() const;
    bool isSendingByeCommand() const;
    bool isOnline() const;
    bool isJoined() const;
    bool isDisconnected() const;
    promise::Promise<void> connect();
    void disconnect(bool withoutReconnection = false);
    void doConnect(const std::string &ipv4, const std::string &ipv6);
    void retryPendingConnection(bool disconnect);
    bool sendCommand(const std::string& command);
    static bool parseSfuData(const char* data, rapidjson::Document& jsonDoc, SfuData& outdata);
    static void setCallbackToCommands(sfu::SfuInterface &call, std::map<std::string, std::unique_ptr<sfu::Command>>& commands);
    bool handleIncomingData(const char *data, size_t len);
    void addNewCommand(const std::string &command);
    void processNextCommand(bool resetSending = false);
    void clearCommandsQueue();
    void checkThreadId();
    const karere::Url& getSfuUrl();

    bool joinSfu(const Sdp& sdp, const std::map<std::string, std::string> &ivs, std::string& ephemeralKey, int avFlags, Cid_t prevCid, int speaker = -1, int vthumbs = -1);
    bool sendKey(Keyid_t id, const std::map<Cid_t, std::string>& keys);
    bool sendAv(unsigned av);
    bool sendGetVtumbs(const std::vector<Cid_t>& cids);
    bool sendDelVthumbs(const std::vector<Cid_t>& cids);
    bool sendGetHiRes(Cid_t cid, int r, int lo = -1);
    bool sendDelHiRes(const std::vector<Cid_t>& cids);
    bool sendHiResSetLo(Cid_t cid, int lo = -1);
    bool sendLayer(int spt, int tmp, int stmp);
    bool sendSpeakReq(Cid_t cid = 0);
    bool sendSpeakReqDel(Cid_t cid = 0);
    bool sendSpeakDel(Cid_t cid = 0);
    bool sendBye(int termCode);

    // Waiting room related commands
    bool sendWrPush(const std::set<karere::Id>& users, const bool& all);
    bool sendWrAllow(const std::set<karere::Id>& users, const bool& all);
    bool sendWrKick(const std::set<karere::Id>& users);
    bool addWrUsersArray(const std::set<karere::Id>& users, const bool& all, rapidjson::Document& json);

protected:
    // mSfuUrl is provided in class ctor and is returned in answer of mcmc/mcmj commands
    karere::Url mSfuUrl;
    WebsocketsIO& mWebsocketIO;
    void* mAppCtx;


    /** Current state of the connection */
    ConnState mConnState = kConnNew;

    /** Target IP address being used for the reconnection in-flight */
    std::string mTargetIp;

    /** ip version to try first (both are tried) */
    bool usingipv6 = false;

    /** RetryController that manages the reconnection's attempts */
    std::unique_ptr<karere::rh::IRetryController> mRetryCtrl;

    /** Handler of the timeout for the connection establishment */
    megaHandle mConnectTimer = 0;

    /** Input promise for the RetryController
     *  - If it fails: a new attempt is schedulled
     *  - If it success: the reconnection is taken as done */
    promise::Promise<void> mConnectPromise;
    void setConnState(ConnState newState);

    void wsConnectCb() override;
    void wsCloseCb(int errcode, int errtype, const char *preason, size_t preason_len) override;
    void wsHandleMsgCb(char *data, size_t len) override;
    void wsSendMsgCb(const char *, size_t) override;
    void wsProcessNextMsgCb() override;
#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
    bool wsSSLsessionUpdateCb(const CachedSession &sess) override;
#endif
    promise::Promise<void> mSendPromise;

    void onSocketClose(int errcode, int errtype, const std::string& reason);
    promise::Promise<void> reconnect();
    void abortRetryController();

    // This flag is set true when BYE command is sent to SFU
    bool mIsSendingBye = false;

    Cid_t mMyCid = K_INVALID_CID;

    std::map<std::string, std::unique_ptr<Command>> mCommands;
    SfuInterface& mCall;
    CommandsQueue mCommandsQueue;
    std::thread::id mMainThreadId; // thread id to ensure that CommandsQueue is accessed from a single thread
    DNScache &mDnsCache;
};

/**
 * @brief The SfuClient class
 *
 * This class is used to handle the connections to the SFU for each call. It allows
 * to handle multiple calls in different chatrooms at the same time, each of them using
 * a different connection.
 */
class SfuClient
{
public:
    SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rtcCryptoMeetings);

    SfuConnection *createSfuConnection(const karere::Id& chatid, karere::Url&& sfuUrl, SfuInterface& call, DNScache &dnsCache);
    void closeSfuConnection(const karere::Id& chatid); // does NOT retry the connection afterwards (used for errors/disconnects)
    void retryPendingConnections(bool disconnect);

    std::shared_ptr<rtcModule::RtcCryptoMeetings>  getRtcCryptoMeetings();
    void addVersionToUrl(karere::Url& sfuUrl);

private:
    std::shared_ptr<rtcModule::RtcCryptoMeetings> mRtcCryptoMeetings;
    std::map<karere::Id, std::unique_ptr<SfuConnection>> mConnections;
    WebsocketsIO& mWebsocketIO;
    void* mAppCtx;

   /** SFU Protocol Versions:
     * - Version 0: initial version
     *
     * - Version 1 (never released for native clients):
     *      + Forward secrecy (ephemeral X25519 EC key pair for each session)
     *      + Dynamic audio routing
     *      + Waiting rooms
     *
     * - Version 2 (contains all features from V1):
     *      + Change AES-GCM by AES-CBC with Zero iv
     */
     static const unsigned int mSfuVersion = 1;
};

static inline const char* connStateToStr(SfuConnection::ConnState state)
{
    switch (state)
    {
    case SfuConnection::kDisconnected: return "Disconnected";
    case SfuConnection::kResolving: return "Resolving";
    case SfuConnection::kConnecting: return "Connecting";
    case SfuConnection::kConnected: return "Connected";
    case SfuConnection::kJoining: return "Joining";
    case SfuConnection::kJoined: return "Joined";
    case SfuConnection::kConnNew: return "New";
    default: return "(invalid)";
    }
}

}

#endif // SFU_H
#endif
