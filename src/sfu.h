#ifndef SFU_H
#define SFU_H

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

class Peer
{
public:
    Peer(); //default ctor
    Peer(Cid_t cid, karere::Id peerid, int avFlags, int mod);
    Peer(const Peer& peer);
    Cid_t getCid() const;
    karere::Id getPeerid() const;
    Keyid_t getCurrentKeyId() const;
    int getAvFlags() const;
    int getModerator() const;
    std::string getKey(Keyid_t keyid) const;
    void addKey(Keyid_t keyid, const std::string& key);
    void setAvFlags(karere::AvFlags flags);
    void init(Cid_t cid, karere::Id peerid, int avFlags, int mod);
protected:
    Cid_t mCid;
    karere::Id mPeerid;
    int mAvFlags;
    int mModerator;
    Keyid_t mCurrentkeyId; // we need to know the current keyId for frame encryption
    std::map<Keyid_t, std::string> mKeyMap;

};

class VideoTrackDescriptor
{
public:
    IvStatic_t mIv;
    std::string mMid;
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
    std::map<uint64_t, std::string> mSsrcs;

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
    virtual bool handleAvCommand(Cid_t cid, int av) = 0;
    virtual bool handleAnswerCommand(Cid_t cid, Sdp &spd, int mod, const std::vector<Peer>&peers, const std::map<Cid_t, VideoTrackDescriptor>&vthumbs, const std::map<Cid_t, SpeakersDescriptor>&speakers) = 0;
    virtual bool handleKeyCommand(Keyid_t keyid, Cid_t cid, const std::string& key) = 0;
    virtual bool handleVThumbsCommand(const std::map<Cid_t, VideoTrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleVThumbsStartCommand() = 0;
    virtual bool handleVThumbsStopCommand() = 0;
    virtual bool handleHiResCommand(const std::map<Cid_t, VideoTrackDescriptor>& videoTrackDescriptors) = 0;
    virtual bool handleHiResStartCommand() = 0;
    virtual bool handleHiResStopCommand() = 0;
    virtual bool handleSpeakReqsCommand(const std::vector<Cid_t>&) = 0;
    virtual bool handleSpeakReqDelCommand(Cid_t cid) = 0;
    virtual bool handleSpeakOnCommand(Cid_t cid, SpeakersDescriptor speaker) = 0;
    virtual bool handleSpeakOffCommand(Cid_t cid) = 0;
    virtual bool handleStatCommand() = 0;
};

    class Command
    {
    public:
        virtual bool processCommand(const rapidjson::Document& command) = 0;
        static std::string COMMAND_IDENTIFIER;
    protected:
        Command();
        void parseSpeakerObject(SpeakersDescriptor &speaker, rapidjson::Value::ConstMemberIterator& it) const;
    };

    typedef std::function<bool(karere::Id, int)> AvCompleteFunction;
    class AVCommand : public Command
    {
    public:
        AVCommand(const AvCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        AvCompleteFunction mComplete;
    };

    class AnswerCommand : public Command
    {
    public:
        typedef std::function<bool(Cid_t, sfu::Sdp&, int, std::vector<Peer>, std::map<Cid_t, VideoTrackDescriptor>, std::map<Cid_t, SpeakersDescriptor>)> AnswerCompleteFunction;
        AnswerCommand(const AnswerCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        AnswerCompleteFunction mComplete;

    private:
        void parsePeerObject(std::vector<Peer>&peers, rapidjson::Value::ConstMemberIterator& it) const;
        void parseSpeakersObject(std::map<Cid_t, SpeakersDescriptor> &speakers, rapidjson::Value::ConstMemberIterator& it) const;
        void parseVthumsObject(std::map<Cid_t, VideoTrackDescriptor> &vthumbs, rapidjson::Value::ConstMemberIterator& it) const;
    };

    typedef std::function<bool(Keyid_t, Cid_t, const std::string&)> KeyCompleteFunction;
    class KeyCommand : public Command
    {
    public:
        KeyCommand(const KeyCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        KeyCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::map<Cid_t, VideoTrackDescriptor>&)> VtumbsCompleteFunction;
    class VthumbsCommand : public Command
    {
    public:
        VthumbsCommand(const VtumbsCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        VtumbsCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> VtumbsStartCompleteFunction;
    class VthumbsStartCommand : public Command
    {
    public:
        VthumbsStartCommand(const VtumbsStartCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        VtumbsStartCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> VtumbsStopCompleteFunction;
    class VthumbsStopCommand : public Command
    {
    public:
        VthumbsStopCommand(const VtumbsStopCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        VtumbsStopCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::map<Cid_t, VideoTrackDescriptor>&)> HiresCompleteFunction;
    class HiResCommand : public Command
    {
    public:
        HiResCommand(const HiresCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        HiresCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> HiResStartCompleteFunction;
    class HiResStartCommand : public Command
    {
    public:
        HiResStartCommand(const HiResStartCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        HiResStartCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> HiResStopCompleteFunction;
    class HiResStopCommand : public Command
    {
    public:
        HiResStopCommand(const HiResStopCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        HiResStopCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::vector<Cid_t>&)> SpeakReqsCompleteFunction;
    class SpeakReqsCommand : public Command
    {
    public:
        SpeakReqsCommand(const SpeakReqsCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakReqsCompleteFunction mComplete;
    };

    typedef std::function<bool(karere::Id)> SpeakReqDelCompleteFunction;
    class SpeakReqDelCommand : public Command
    {
    public:
        SpeakReqDelCommand(const SpeakReqDelCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakReqDelCompleteFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid, SpeakersDescriptor speaker)> SpeakOnCompleteFunction;
    class SpeakOnCommand : public Command
    {
    public:
        SpeakOnCommand(const SpeakOnCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakOnCompleteFunction mComplete;
    };

    typedef std::function<bool(Cid_t cid)> SpeakOffCompleteFunction;
    class SpeakOffCommand : public Command
    {
    public:
        SpeakOffCommand(const SpeakOffCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakOffCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> StatCommandFunction;
    class StatCommand : public Command
    {
    public:
        StatCommand(const StatCommandFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        StatCommandFunction mComplete;
    };

    class SfuConnection : public karere::DeleteTrackable, public WebsocketsClient
    {
        // client->sfu commands
        const std::string CSFU_JOIN = "JOIN";
        const std::string CSFU_SENDKEY = "KEY";
        const std::string CSFU_AV = "AV";
        const std::string CSFU_GET_VTHUMBS = "GET_VTHUMBS";
        const std::string CSFU_DEL_VTHUMBS = "DEL_VTHUMBS";
        const std::string CSFU_GET_HIRES = "GET_HIRES";
        const std::string CSFU_DEL_HIRES = "DEL_HIRES";
        const std::string CSFU_HIRES_SET_LO = "HIRES_SET_LO";
        const std::string CSFU_LAYER = "LAYER";
        const std::string CSFU_SPEAK_RQ = "SPEAK_RQ";
        const std::string CSFU_SPEAK_RQ_DEL = "SPEAK_RQ_DEL";
        const std::string CSFU_SPEAK_DEL = "SPEAK_DEL";

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
        bool isOnline() const;
        promise::Promise<void> connect();
        void disconnect();
        void doConnect();
        void retryPendingConnection(bool disconnect);
        bool sendCommand(const std::string& command);
        bool handleIncomingData(const char* data, size_t len);

        promise::Promise<void> getPromiseConnection();
        bool joinSfu(const Sdp& sdp, const std::map<int, uint64_t> &ivs, bool moderator, int avFlags, int speaker = -1, int vthumbs = -1);
        bool sendKey(Keyid_t id, const std::map<Cid_t, std::string>& keys);
        bool sendAv(int av);
        bool sendGetVtumbs(const std::vector<karere::Id>& cids);
        bool sendDelVthumbs(const std::vector<karere::Id>& cids);
        bool sendGetHiRes(karere::Id cid, int r, int lo = -1);
        bool sendDelHiRes(karere::Id cid);
        bool sendHiResSetLo(karere::Id cid, int lo = -1);
        bool sendLayer(int spt, int tmp, int stmp);
        bool sendSpeakReq(karere::Id cid = karere::Id::inval());
        bool sendSpeakReqDel(karere::Id cid = karere::Id::inval());
        bool sendSpeakDel(karere::Id cid = karere::Id::inval());
        bool sendModeratorRequested(karere::Id cid = karere::Id::inval());

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
        promise::Promise<void> mSendPromise;

        void onSocketClose(int errcode, int errtype, const std::string& reason);
        promise::Promise<void> reconnect();
        void abortRetryController();

        std::map<std::string, std::unique_ptr<Command>> mCommands;
        SfuInterface& mCall;
    };

    class SfuClient
    {
    public:
        SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings, const karere::Id& myHandle);
        SfuConnection *generateSfuConnection(karere::Id chatid, const std::string& sfuUrl, SfuInterface& call);
        void closeManagerProtocol(karere::Id chatid);
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
