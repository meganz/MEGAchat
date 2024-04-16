#ifndef KARERE_DISABLE_WEBRTC
#ifndef SFU_H
#define SFU_H
#include <thread>
#include <optional>
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
 *
 * - Version 2 (contains all features from V1):
 *      + Change AES-GCM by AES-CBC with Zero iv
 *      + Waiting rooms
 *
 * - Version 3 (contains all features from V2):
 *      + Speak requests (raise hand to speak)
 */
enum class SfuProtocol: uint32_t
{
    SFU_PROTO_V0       = 0,
    SFU_PROTO_V1       = 1,
    SFU_PROTO_V2       = 2,
    SFU_PROTO_V3       = 3,
    SFU_PROTO_V4       = 4,            // currently for testing purposes
    SFU_PROTO_FIRST    = SFU_PROTO_V0,
    SFU_PROTO_PROD     = SFU_PROTO_V3, // protocol version used in production
    SFU_PROTO_LAST     = SFU_PROTO_V4, // last known protocol version by sfu
    SFU_PROTO_INVAL    = UINT32_MAX,
};

// returns true if provided version as param is a valid protocol version in SFU
static bool isKnownSfuVersion(sfu::SfuProtocol v)       { return v >= SfuProtocol::SFU_PROTO_V0
                                                            && v <= SfuProtocol::SFU_PROTO_LAST; }

// enum for user status in waiting room
enum class WrState: int
{
    WR_UNKNOWN      = -1,   // client unknown joining status
    WR_NOT_ALLOWED  = 0,    // client is not allowed to join call (must remains in waiting room)
    WR_ALLOWED      = 1,    // client is allowed to join call (needs to send JOIN command to SFU)
};

// struct that represents an user in waiting room
struct WrRoomUser
{
public:
    karere::Id mWrUserid = karere::Id::inval();
    WrState mWrState   = WrState::WR_UNKNOWN;
};

// typedef for waiting room user list
// SFU provides Waiting room participants list in order they joined to the call
typedef std::vector<WrRoomUser> WrUserList;


// Default value for when we don't receive information about a specific limit in HELLO or CLIMITS
// commands. It can be thought as an unlimited value.
static constexpr int kCallLimitDisabled = -1;

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
    bool setEphemeralPubKeyDerived(const std::string& key);

    // returns derived peer's ephemeral key if available
    std::string getEphemeralPubKeyDerived() const;

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
    struct CallLimits
    {
        int durationInSecs = ::sfu::kCallLimitDisabled;
        int numUsers = ::sfu::kCallLimitDisabled;
        int numClients = ::sfu::kCallLimitDisabled;
        int numClientsPerUser = ::sfu::kCallLimitDisabled;

        bool operator==(const CallLimits& other)
        {
            return durationInSecs == other.durationInSecs && numUsers == other.numUsers &&
                   numClients == other.numClients && numClientsPerUser == other.numClientsPerUser;
        }

        bool operator!=(const CallLimits& other)
        {
            return !(*this == other);
        }
    };

    // SFU -> Client commands
    virtual bool handleAvCommand(Cid_t cid, unsigned av, uint32_t amid) = 0;   // audio/video/on-hold flags
    virtual bool handleAnswerCommand(Cid_t cid, std::shared_ptr<Sdp> spd, uint64_t, std::vector<Peer>& peers, const std::map<Cid_t, std::string>& keystrmap,
                                     const std::map<Cid_t, TrackDescriptor>& vthumbs,
                                     const std::set<karere::Id>& speakers, const std::set<karere::Id>& speakReqs,
                                     const std::vector<karere::Id>& raiseHands,
                                     const std::map<Cid_t, uint32_t>& amidmap) = 0;
    virtual bool handleKeyCommand(const Keyid_t& keyid, const Cid_t& cid, const std::string& key) = 0;
    virtual bool handleVThumbsCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleVThumbsStartCommand() = 0;
    virtual bool handleVThumbsStopCommand() = 0;
    virtual bool handleHiResCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleHiResStartCommand() = 0;
    virtual bool handleHiResStopCommand() = 0;
    virtual bool handleSpeakerAddDelCommand(const uint64_t userid, const bool add) = 0;
    virtual bool handleSpeakReqAddDelCommand(const uint64_t userid, const bool add) = 0;
    virtual bool handleRaiseHandAddCommand(const uint64_t userid) = 0;
    virtual bool handleRaiseHandDelCommand(const uint64_t userid) = 0;
    virtual bool handleModAdd(uint64_t userid) = 0;
    virtual bool handleModDel(uint64_t userid) = 0;
    virtual bool handleHello(const Cid_t cid, const unsigned int nAudioTracks,
                             const std::set<karere::Id>& mods, const bool wr, const bool allowed,
                             bool speakRequest, const sfu::WrUserList& wrUsers, const CallLimits& callLimits) = 0;

    virtual bool handleWrDump(const sfu::WrUserList& users) = 0;
    virtual bool handleWrEnter(const sfu::WrUserList& users) = 0;
    virtual bool handleWrLeave(const karere::Id& /*user*/) = 0;
    virtual bool handleWrAllow(const Cid_t& cid) = 0;
    virtual bool handleWrDeny() = 0;
    virtual bool handleWrUsersAllow(const std::set<karere::Id>& users) = 0;
    virtual bool handleWrUsersDeny(const std::set<karere::Id>& users) = 0;
    virtual bool handleMutedCommand(const unsigned av, const Cid_t cidPerf) = 0;
    virtual bool handleWillEndCommand(const unsigned int endsIn) = 0;
    virtual bool handleClimitsCommand(const sfu::SfuInterface::CallLimits& callLimits) = 0;
    // called when the connection to SFU is established
    virtual bool handlePeerJoin(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string> &ivs) = 0;
    virtual bool handlePeerLeft(Cid_t cid, unsigned termcode) = 0;
    virtual bool handleBye(const unsigned termCode, const bool wr, const std::string& errMsg) = 0;
    virtual void onSfuDisconnected() = 0;
    virtual void onByeCommandSent() = 0;

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
    void parseUsersArray(std::set<karere::Id>& users, rapidjson::Value::ConstMemberIterator& it) const;
    bool parseUsersArrayInOrder(std::vector<karere::Id>& users, rapidjson::Value::ConstMemberIterator& it, const bool allowDuplicates) const;
    void parseTracks(const rapidjson::Document &command, const std::string& arrayName, std::map<Cid_t, TrackDescriptor>& tracks) const;

protected:
    Command(SfuInterface& call);
    bool parseWrUsersMap(sfu::WrUserList& wrUsers, const rapidjson::Value& obj) const;
    static uint8_t hexDigitVal(char value);

    /**
     * @brief Extract the information of the lim field in the document
     *
     * @param command The document to parse
     * @return std::optional<SfuInterface::CallLimits> An empty optional if any filed in the lim
     * object has an unexpected format. Else, the object with the limits. If any field is missing in
     * the lim object, the associated parameter will be set to sfu::kCallLimitDisabled.
     */
    std::optional<SfuInterface::CallLimits> parseCallLimits(const rapidjson::Document& command);

    /**
     * @brief Given a json object, extracts from it all the fields needed to build a CallLimits
     * object.
     *
     * @param jsonObject The json object with the required fields
     * @return std::optional<SfuInterface::CallLimits> An empty optional if any field in the lim
     * object has an unexpected format. Else, the object with the limits. If any field is missing in
     * the lim object, the associated parameter will be set to sfu::kCallLimitDisabled.
     */
    std::optional<SfuInterface::CallLimits> buildCallLimits(const rapidjson::Value& jsonOject);

    SfuInterface& mCall;
};

class AVCommand : public Command
{   // "AV"
public:
    typedef std::function<bool(karere::Id, unsigned, uint32_t)> AvCompleteFunction;
    AVCommand(const AvCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    AvCompleteFunction mComplete;
};

class AnswerCommand : public Command
{   // "ANSWER"
public:
    typedef std::function<bool(Cid_t,
                               std::shared_ptr<Sdp>,
                               uint64_t,
                               std::vector<Peer>&,
                               const std::map<Cid_t, std::string>&,
                               std::map<Cid_t, TrackDescriptor>&,
                               const std::set<karere::Id>&,
                               const std::set<karere::Id>&,
                               const std::vector<karere::Id>&,
                               std::map<Cid_t, uint32_t>&)>
        AnswerCompleteFunction;
    AnswerCommand(const AnswerCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    AnswerCompleteFunction mComplete;

private:
    void parsePeerObject(std::vector<Peer>& peers, std::map<Cid_t, std::string>& keystrmap, std::map<Cid_t, uint32_t>& amidmap, rapidjson::Value::ConstMemberIterator& it) const;
};

class KeyCommand : public Command
{   // "KEY"
public:
    typedef std::function<bool(const Keyid_t&, const Cid_t&, const std::string&)> KeyCompleteFunction;
    KeyCommand(const KeyCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    KeyCompleteFunction mComplete;
    constexpr static Keyid_t maxKeyId = static_cast<Keyid_t>(~0);
};

class VthumbsCommand : public Command
{   // "VTHUMBS"
public:
    typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> VtumbsCompleteFunction;
    VthumbsCommand(const VtumbsCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsCompleteFunction mComplete;
};

class VthumbsStartCommand : public Command
{   // "VTHUMB_START"
public:
    typedef std::function<bool(void)> VtumbsStartCompleteFunction;
    VthumbsStartCommand(const VtumbsStartCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsStartCompleteFunction mComplete;
};

class VthumbsStopCommand : public Command
{   // "VTHUMB_STOP"
public:
    typedef std::function<bool(void)> VtumbsStopCompleteFunction;
    VthumbsStopCommand(const VtumbsStopCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    VtumbsStopCompleteFunction mComplete;
};

class HiResCommand : public Command
{   // "HIRES"
public:
    typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> HiresCompleteFunction;
    HiResCommand(const HiresCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiresCompleteFunction mComplete;
};

class HiResStartCommand : public Command
{   // "HIRES_START"
public:
    typedef std::function<bool(void)> HiResStartCompleteFunction;
    HiResStartCommand(const HiResStartCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiResStartCompleteFunction mComplete;
};

class HiResStopCommand : public Command
{   // "HIRES_STOP"
public:
    typedef std::function<bool(void)> HiResStopCompleteFunction;
    HiResStopCommand(const HiResStopCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HiResStopCompleteFunction mComplete;
};

class SpeakerAddCommand : public Command
{   // "SPEAKER_ADD"
public:
    typedef std::function<bool(const uint64_t, const bool)> SpeakerAddCompleteFunction;
    SpeakerAddCommand(const SpeakerAddCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakerAddCompleteFunction mComplete;
};

class SpeakerDelCommand : public Command
{   // "SPEAKER_DEL"
public:
    typedef std::function<bool(const uint64_t, const bool)> SpeakerDelCompleteFunction;
    SpeakerDelCommand(const SpeakerDelCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakerDelCompleteFunction mComplete;
};

class SpeakReqCommand : public Command
{   // "SPEAKRQ"
public:
    typedef std::function<bool(const uint64_t, const bool)> SpeakReqsCompleteFunction;
    SpeakReqCommand(const SpeakReqsCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakReqsCompleteFunction mComplete;
};

class SpeakReqDelCommand : public Command
{   // "SPEAKRQ_DEL"
public:
    typedef std::function<bool(const uint64_t, const bool)> SpeakReqDelCompleteFunction;
    SpeakReqDelCommand(const SpeakReqDelCompleteFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    SpeakReqDelCompleteFunction mComplete;
};

class PeerJoinCommand : public Command
{   // "PEERJOIN"
public:
    typedef std::function<bool(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string> &ivs)> PeerJoinCommandFunction;
    PeerJoinCommand(const PeerJoinCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    PeerJoinCommandFunction mComplete;
};

class PeerLeftCommand : public Command
{   // "PEERLEFT"
public:
    typedef std::function<bool(Cid_t cid, unsigned termcode)> PeerLeftCommandFunction;
    PeerLeftCommand(const PeerLeftCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    PeerLeftCommandFunction mComplete;
};

class ByeCommand : public Command
{   // "BYE"
public:
    typedef std::function<bool(const unsigned termCode, const bool wr, const std::string& errMsg)> ByeCommandFunction;
    ByeCommand(const ByeCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ByeCommandFunction mComplete;
};

class MutedCommand : public Command
{   // "MUTED"
public:
    typedef std::function<bool(const unsigned av, const Cid_t cidPerf)> MutedCommandFunction;
    MutedCommand(const MutedCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    MutedCommandFunction mComplete;
};

class WillEndCommand : public Command
{
public:
    typedef std::function<bool(int64_t endsIn)> WillEndCommandFunction;
    WillEndCommand(const WillEndCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WillEndCommandFunction mComplete;
};

class ClimitsCommand : public Command
{
public:
    typedef std::function<bool(const SfuInterface::CallLimits& callLimits)> ClimitsCommandFunction;
    ClimitsCommand(const ClimitsCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ClimitsCommandFunction mComplete;
};

class RaiseHandAddCommand : public Command
{
public:
    typedef std::function<bool(const uint64_t userid)> RaiseHandAddCommandFunction;
    RaiseHandAddCommand(const RaiseHandAddCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    RaiseHandAddCommandFunction mComplete;
};

class RaiseHandDelCommand : public Command
{
public:
    typedef std::function<bool(const uint64_t userid)> RaiseHandDelCommandFunction;
    RaiseHandDelCommand(const RaiseHandDelCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    RaiseHandDelCommandFunction mComplete;
};

class ModAddCommand : public Command
{   // "MOD_ADD"
public:
    typedef std::function<bool(uint64_t userid)> ModAddCommandFunction;
    ModAddCommand(const ModAddCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ModAddCommandFunction mComplete;
};

class ModDelCommand : public Command
{   // "MOD_DEL"
public:
    typedef std::function<bool(uint64_t userid)> ModDelCommandFunction;
    ModDelCommand(const ModDelCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    ModDelCommandFunction mComplete;
};

class HelloCommand : public Command
{   // "HELLO"
public:
    typedef std::function<bool(const Cid_t userid,
                               const unsigned int nAudioTracks,
                               const std::set<karere::Id>& mods,
                               const bool wr,
                               const bool speakRequest,
                               const bool allowed,
                               const sfu::WrUserList& wrUsers,
                               const SfuInterface::CallLimits& callLimits)>HelloCommandFunction;

    HelloCommand(const HelloCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    HelloCommandFunction mComplete;
};

class WrDumpCommand: public Command
{   // "WR_DUMP"
public:
    typedef std::function<bool(const sfu::WrUserList& users)>WrDumpCommandFunction;
    WrDumpCommand(const WrDumpCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrDumpCommandFunction mComplete;
};

class WrEnterCommand: public Command
{   // "WR_ENTER"
public:
    typedef std::function<bool(const sfu::WrUserList& users)>WrEnterCommandFunction;
    WrEnterCommand(const WrEnterCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrEnterCommandFunction mComplete;
};

class WrLeaveCommand: public Command
{   // "WR_LEAVE"
public:
    typedef std::function<bool(const karere::Id& user)>WrLeaveCommandFunction;
    WrLeaveCommand(const WrLeaveCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrLeaveCommandFunction mComplete;
};

class WrAllowCommand: public Command
{   // "WR_ALLOW"
public:
    typedef std::function<bool(const Cid_t& cid)>WrAllowCommandFunction;
    WrAllowCommand(const WrAllowCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrAllowCommandFunction mComplete;
};

class WrDenyCommand: public Command
{   // "WR_DENY"
public:
    typedef std::function<bool()>WrDenyCommandFunction;
    WrDenyCommand(const WrDenyCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrDenyCommandFunction mComplete;
};

class WrUsersAllowCommand: public Command
{   // "WR_USERS_ALLOW"
public:
    typedef std::function<bool(const std::set<karere::Id>& users)>WrUsersAllowCommandFunction;
    WrUsersAllowCommand(const WrUsersAllowCommandFunction& complete, SfuInterface& call);
    bool processCommand(const rapidjson::Document& command) override;
    static const std::string COMMAND_NAME;
    WrUsersAllowCommandFunction mComplete;
};

class WrUsersDenyCommand: public Command
{   // "WR_USERS_DENY"
public:
    typedef std::function<bool(const std::set<karere::Id>& users)>WrUsersDenyCommandFunction;
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
    static const std::string CSFU_SPEAKRQ;
    static const std::string CSFU_SPEAKER_ADD;
    static const std::string CSFU_SPEAKER_DEL;
    static const std::string CSFU_SPEAKRQ_DEL;
    static const std::string CSFU_BYE;
    static const std::string CSFU_WR_PUSH;
    static const std::string CSFU_WR_ALLOW;
    static const std::string CSFU_WR_KICK;
    static const std::string CSFU_MUTE;
    static const std::string CSFU_SETLIMIT;
    static const std::string CSFU_RHAND_ADD;
    static const std::string CSFU_RHAND_DEL;

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

    static constexpr uint32_t callLimitNotPresent = 0xFFFFFFFF;     // No limit present (the param won't be modified)
    static constexpr uint32_t callLimitReset = 0;                   // Value used for reset call limit like duration or max participants
    static constexpr unsigned int callLimitUsersPerClient = 4;      // Maximum number of clients with which a single user can join a call
    static constexpr unsigned int maxInitialBackoff = 100;          // (in milliseconds) max initial backoff for SFU connection attempt
    static constexpr uint8_t kConnectTimeout = 30;                  // (in seconds) timeout reconnection to succeeed
    static constexpr uint8_t kNoMediaPathTimeout = 6;               // (in seconds) disconnect call upon no UDP connectivity after this period
    SfuConnection(karere::Url&& sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface& call, DNScache &dnsCache);
    ~SfuConnection();
    void setIsSendingBye(bool sending);
    void setMyCid(const Cid_t& cid);
    Cid_t getMyCid() const;
    bool isSendingByeCommand() const;
    bool isOnline() const;
    bool isJoined() const;
    bool isDisconnected() const;
    void connect();
    void doReconnect(const bool applyInitialBackoff);
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

    // Important: SFU V2 or greater doesn't accept audio flag enabled upon JOIN command
    bool joinSfu(const Sdp& sdp, const std::map<std::string, std::string> &ivs, std::string& ephemeralKey,
                 int avFlags, Cid_t prevCid, int vthumbs = -1);

    bool sendKey(Keyid_t id, const std::map<Cid_t, std::string>& keys);
    bool sendAv(unsigned av);
    bool sendGetVtumbs(const std::vector<Cid_t>& cids);
    bool sendDelVthumbs(const std::vector<Cid_t>& cids);
    bool sendGetHiRes(Cid_t cid, int r, int lo = -1);
    bool sendDelHiRes(const std::vector<Cid_t>& cids);
    bool sendHiResSetLo(Cid_t cid, int lo = -1);
    bool sendLayer(int spt, int tmp, int stmp);
    bool sendSpeakerAddDel(const karere::Id& user, const bool add);
    bool sendSpeakReqAddDel(const karere::Id& user, const bool add);
    bool raiseHandToSpeak(const bool add);
    bool sendBye(int termCode);
    void clearInitialBackoff();
    void incrementInitialBackoff();
    unsigned int getInitialBackoff() const;

    // Waiting room related commands
    bool sendWrCommand(const std::string& commandStr, const std::set<karere::Id>& users, const bool all = false);
    bool sendWrPush(const std::set<karere::Id>& users, const bool all);
    bool sendWrAllow(const std::set<karere::Id>& users, const bool all);
    bool sendWrKick(const std::set<karere::Id>& users);
    bool sendSetLimit(const uint32_t callDurSecs, const uint32_t numUsers, const uint32_t numClientsPerUser, const uint32_t numClients, const uint32_t divider);
    bool sendMute(const Cid_t& cid, const unsigned av);
    bool addWrUsersArray(const std::set<karere::Id>& users, const bool all, rapidjson::Document& json);
    bool avoidReconnect() const;
    void setAvoidReconnect(const bool avoidReconnect);

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

    /** Cancels connect timer in case is set **/
    void cancelConnectTimer();

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

    // This flag is set true when BYE command is being sent to SFU
    bool mIsSendingBye = false;

    Cid_t mMyCid = K_INVALID_CID;

    std::map<std::string, std::unique_ptr<Command>> mCommands;
    SfuInterface& mCall;
    CommandsQueue mCommandsQueue;
    std::thread::id mMainThreadId; // thread id to ensure that CommandsQueue is accessed from a single thread
    DNScache &mDnsCache;

    /* Initial backoff for retry controller (in milliseconds)
     * A connection to SFU can be considered succeeded, just when client receives ANSWER command.
     * Extend lifetime of retry controller far away than LWS connection, doesn't make sense for this particular scenario.
     * The best solution is adding a initial backoff that will start in 0 and will be incremented when we establish LWS connection.
     *
     * If connection is dropped down before receiving the ANSWER command, the next attempt will be delayed.
     */
     unsigned int mInitialBackoff = 0;

     // This flag prevents to start a new reconnection attempt, if we are currently destroying the call
     bool mAvoidReconnect = false;
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
    void addVersionToUrl(karere::Url& sfuUrl, const sfu::SfuProtocol sfuVersion);

private:
    std::shared_ptr<rtcModule::RtcCryptoMeetings> mRtcCryptoMeetings;
    std::map<karere::Id, std::unique_ptr<SfuConnection>> mConnections;
    WebsocketsIO& mWebsocketIO;
    void* mAppCtx;
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
