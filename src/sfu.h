#ifndef SFU_H
#define SFU_H

#include <thread>
#include <base/retryHandler.h>
#include <net/websocketsIO.h>
#include <karereId.h>
#include <rapidjson/document.h>
#include "rtcCrypto.h"

#define SFU_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
namespace sfu
{
// NOTE: This queue, must be always managed from a single thread.
// The classes that instanciates it, are responsible to ensure that.
// In case we need to access to it from another thread, we would need to implement
// a synchronization mechanism (like a mutex).
class CommandsQueue
{
protected:
    std::deque<std::string> commands;
    bool isSending = false;

public:
    CommandsQueue();
    bool sending();
    void setSending(bool sending);
    void push(const std::string &);
    std::string pop();
    bool isEmpty();
    void clear();
};

class Peer
{
public:
    Peer(); //default ctor
    Peer(Cid_t cid, karere::Id peerid, unsigned avFlags);
    Peer(const Peer& peer);
    Cid_t getCid() const;
    karere::Id getPeerid() const;
    Keyid_t getCurrentKeyId() const;
    karere::AvFlags getAvFlags() const;
    std::string getKey(Keyid_t keyid) const;
    void addKey(Keyid_t keyid, const std::string& key);
    void setAvFlags(karere::AvFlags flags);
    void init(Cid_t cid, karere::Id peerid, unsigned avFlags);
protected:
    Cid_t mCid = 0;
    karere::Id mPeerid;
    karere::AvFlags mAvFlags = 0;
    Keyid_t mCurrentkeyId = 0; // we need to know the current keyId for frame encryption
    std::map<Keyid_t, std::string> mKeyMap;

};

class TrackDescriptor
{
public:
    IvStatic_t mIv = 0;
    uint32_t mMid;
    bool mReuse = false;
};

class SpeakersDescriptor
{
public:
    SpeakersDescriptor();
    SpeakersDescriptor(const std::string& audioDescriptor, const std::string& videoDescriptor);
    std::string getAudioDescriptor() const;
    std::string getVideoDescriptor() const;
    void setDescriptors(const std::string& audioDescriptor, const std::string& videoDescriptor);
    IvStatic_t mIv;
    std::string mMid;

protected:
    std::string mAudioDescriptor;
    std::string mVideoDescriptor;
};

class SdpTrack
{
public:
    std::string mType;
    uint64_t mMid;
    std::string mDir;
    std::string mSid;
    std::string mId;
    std::vector<std::string> mSsrcg;
    std::vector<std::pair<uint64_t, std::string>> mSsrcs;
};

class Sdp
{
public:
    Sdp(const std::string& sdp);
    Sdp(const rapidjson::Value& sdp);
    std::string unCompress();
    void toJson(rapidjson::Document& json) const;

public:
    unsigned int createTemplate(const std::string& type, const std::vector<std::string> lines, unsigned int position);
    unsigned int addTrack(const std::vector<std::string>& lines, unsigned int position);
    unsigned int nextMline(const std::vector<std::string>& lines, unsigned int position);
    std::string nextWord(const std::string& line, unsigned int start, unsigned int &charRead);
    SdpTrack parseTrack(const rapidjson::Value &value) const;
    std::string unCompressTrack(const SdpTrack &track, const std::string& tpl);
    std::map<std::string, std::string> mData;
    std::vector<SdpTrack> mTracks;
    static const std::string endl;
};


class SfuInterface
{
public:
    // SFU -> Client
    virtual bool handleAvCommand(Cid_t cid, unsigned av) = 0;
    virtual bool handleAnswerCommand(Cid_t cid, Sdp &spd, uint64_t, const std::vector<Peer>&peers, const std::map<Cid_t, TrackDescriptor>&vthumbs, const std::map<Cid_t, TrackDescriptor>&speakers) = 0;
    virtual bool handleKeyCommand(Keyid_t keyid, Cid_t cid, const std::string& key) = 0;
    virtual bool handleVThumbsCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleVThumbsStartCommand() = 0;
    virtual bool handleVThumbsStopCommand() = 0;
    virtual bool handleHiResCommand(const std::map<Cid_t, TrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleHiResStartCommand() = 0;
    virtual bool handleHiResStopCommand() = 0;
    virtual bool handleSpeakReqsCommand(const std::vector<Cid_t>&) = 0;
    virtual bool handleSpeakReqDelCommand(Cid_t cid) = 0;
    virtual bool handleSpeakOnCommand(Cid_t cid, TrackDescriptor speaker) = 0;
    virtual bool handleSpeakOffCommand(Cid_t cid) = 0;
    virtual bool handleStatCommand() = 0;
    virtual bool handlePeerJoin(Cid_t cid, uint64_t userid, int av) = 0;
    virtual bool handlePeerLeft(Cid_t cid) = 0;
    virtual bool handleError(unsigned int , const std::string) = 0;
    virtual bool handleModerator(Cid_t cid, bool moderator) = 0;
    virtual void onSfuConnected() = 0;
    virtual bool error(unsigned int) = 0;
};

    class Command
    {
    public:
        virtual bool processCommand(const rapidjson::Document& command) = 0;
        static std::string COMMAND_IDENTIFIER;
        static std::string ERROR_IDENTIFIER;
        virtual ~Command();
        static std::string binaryToHex(uint64_t value);
        static uint64_t hexToBinary(const std::string& hex);
    protected:
        Command();
        void parseSpeakerObject(SpeakersDescriptor &speaker, rapidjson::Value::ConstMemberIterator& it) const;
        bool parseTrackDescriptor(TrackDescriptor &trackDescriptor, rapidjson::Value::ConstMemberIterator &value) const;
        static uint8_t hexDigitVal(char value);
    };

    typedef std::function<bool(karere::Id, unsigned)> AvCompleteFunction;
    class AVCommand : public Command
    {
    public:
        AVCommand(const AvCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        AvCompleteFunction mComplete;
    };

    class AnswerCommand : public Command
    {
    public:
        typedef std::function<bool(Cid_t, sfu::Sdp&, uint64_t, std::vector<Peer>, std::map<Cid_t, TrackDescriptor>, std::map<Cid_t, TrackDescriptor>)> AnswerCompleteFunction;
        AnswerCommand(const AnswerCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        AnswerCompleteFunction mComplete;

    private:
        void parsePeerObject(std::vector<Peer>&peers, rapidjson::Value::ConstMemberIterator& it) const;
        bool parseTracks(const std::vector<Peer>&peers, std::map<Cid_t, TrackDescriptor> &tracks, rapidjson::Value::ConstMemberIterator& it, bool audio = false) const;
        void parseSpeakersObject(std::map<Cid_t, SpeakersDescriptor> &speakers, rapidjson::Value::ConstMemberIterator& it) const;
        void parseVthumsObject(std::map<Cid_t, TrackDescriptor> &vthumbs, rapidjson::Value::ConstMemberIterator& it) const;
    };

    typedef std::function<bool(Keyid_t, Cid_t, const std::string&)> KeyCompleteFunction;
    class KeyCommand : public Command
    {
    public:
        KeyCommand(const KeyCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        KeyCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> VtumbsCompleteFunction;
    class VthumbsCommand : public Command
    {
    public:
        VthumbsCommand(const VtumbsCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        VtumbsCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> VtumbsStartCompleteFunction;
    class VthumbsStartCommand : public Command
    {
    public:
        VthumbsStartCommand(const VtumbsStartCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        VtumbsStartCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> VtumbsStopCompleteFunction;
    class VthumbsStopCommand : public Command
    {
    public:
        VthumbsStopCommand(const VtumbsStopCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        VtumbsStopCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::map<Cid_t, TrackDescriptor>&)> HiresCompleteFunction;
    class HiResCommand : public Command
    {
    public:
        HiResCommand(const HiresCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        HiresCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> HiResStartCompleteFunction;
    class HiResStartCommand : public Command
    {
    public:
        HiResStartCommand(const HiResStartCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        HiResStartCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> HiResStopCompleteFunction;
    class HiResStopCommand : public Command
    {
    public:
        HiResStopCommand(const HiResStopCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        HiResStopCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::vector<Cid_t>&)> SpeakReqsCompleteFunction;
    class SpeakReqsCommand : public Command
    {
    public:
        SpeakReqsCommand(const SpeakReqsCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        SpeakReqsCompleteFunction mComplete;
    };

    typedef std::function<bool(karere::Id)> SpeakReqDelCompleteFunction;
    class SpeakReqDelCommand : public Command
    {
    public:
        SpeakReqDelCommand(const SpeakReqDelCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        SpeakReqDelCompleteFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid, TrackDescriptor speaker)> SpeakOnCompleteFunction;
    class SpeakOnCommand : public Command
    {
    public:
        SpeakOnCommand(const SpeakOnCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        SpeakOnCompleteFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid)> SpeakOffCompleteFunction;
    class SpeakOffCommand : public Command
    {
    public:
        SpeakOffCommand(const SpeakOffCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        SpeakOffCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> StatCommandFunction;
    class StatCommand : public Command
    {
    public:
        StatCommand(const StatCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        StatCommandFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid, uint64_t userid, int av)> PeerJoinCommandFunction;
    class PeerJoinCommand : public Command
    {
    public:
        PeerJoinCommand(const PeerJoinCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        PeerJoinCommandFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid)> PeerLeftCommandFunction;
    class PeerLeftCommand : public Command
    {
    public:
        PeerLeftCommand(const PeerLeftCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        PeerLeftCommandFunction mComplete;
    };

    typedef std::function<bool(unsigned int , const std::string)> ErrorCommandFunction;
    class ErrorCommand : public Command
    {
    public:
        ErrorCommand(const ErrorCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        ErrorCommandFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid, bool moderator)> ModeratorCommandFunction;
    class ModeratorCommand : public Command
    {
    public:
        ModeratorCommand(const ModeratorCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static const std::string COMMAND_NAME;
        ModeratorCommandFunction mComplete;
    };

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

    public:
        enum ConnState
        {
            kConnNew = 0,
            kDisconnected,
            kResolving,
            kConnecting,
            kConnected,
            kJoining,
            kJoined,
        };

        SfuConnection(const std::string& sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface& call);
        ~SfuConnection();
        bool isOnline() const;
        bool isDisconnected() const;
        promise::Promise<void> connect();
        void disconnect(bool withoutReconnection = false);
        void doConnect();
        void retryPendingConnection(bool disconnect);
        bool sendCommand(const std::string& command);
        bool handleIncomingData(const char* data, size_t len);
        void addNewCommand(const std::string &command);
        void processNextCommand(bool resetSending = false);
        void clearCommandsQueue();
        void checkThreadId();

        promise::Promise<void> getPromiseConnection();
        bool joinSfu(const Sdp& sdp, const std::map<std::string, std::string> &ivs, int avFlags, int speaker = -1, int vthumbs = -1);
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

    protected:
        std::string mSfuUrl;
        //karere::Client& mKarereClient;
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

        /** Input promise for the RetryController
         *  - If it fails: a new attempt is schedulled
         *  - If it success: the reconnection is taken as done */
        promise::Promise<void> mConnectPromise;
        std::vector<std::string> mIpsv4;
        std::vector<std::string> mIpsv6;

        void setConnState(ConnState newState);

        void wsConnectCb() override;
        void wsCloseCb(int errcode, int errtype, const char *preason, size_t preason_len) override;
        void wsHandleMsgCb(char *data, size_t len) override;
        void wsSendMsgCb(const char *, size_t) override;
        void wsProcessNextMsgCb() override;
        promise::Promise<void> mSendPromise;

        void onSocketClose(int errcode, int errtype, const std::string& reason);
        promise::Promise<void> reconnect();
        void abortRetryController();

        std::map<std::string, std::unique_ptr<Command>> mCommands;
        SfuInterface& mCall;
        CommandsQueue mCommandsQueue;
        std::thread::id mMainThreadId; // thread id to ensure that CommandsQueue is accessed from a single thread
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
        SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rtcCryptoMeetings, const karere::Id& myHandle);

        SfuConnection *createSfuConnection(karere::Id chatid, const std::string& sfuUrl, SfuInterface& call);
        void closeSfuConnection(karere::Id chatid);
        void retryPendingConnections(bool disconnect);

        std::shared_ptr<rtcModule::RtcCryptoMeetings>  getRtcCryptoMeetings();
        const karere::Id& myHandle();

    private:
        std::shared_ptr<rtcModule::RtcCryptoMeetings> mRtcCryptoMeetings;
        std::map<karere::Id, std::unique_ptr<SfuConnection>> mConnections;
        WebsocketsIO& mWebsocketIO;
        karere::Id mMyHandle;
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
