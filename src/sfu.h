#ifndef SFU_H
#define SFU_H

#include <chatClient.h>

#define SFU_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
#define SFU_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_sfu, fmtString, ##__VA_ARGS__)
namespace sfu
{
    class Command
    {
    public:
        virtual bool processCommand(const rapidjson::Document& command) = 0;
        static std::string COMMAND_IDENTIFIER;
    protected:
        Command();
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
        class Peer
        {
        public:
            Peer(karere::Id cid, karere::Id peerid, int avFlags, int mod);
            karere::Id getCid() const;
            karere::Id getPeerid() const;
            int getAvFlags() const;
            int getMod() const;
        protected:
            karere::Id mCid;
            karere::Id mPeerid;
            int mAvFlags;
            int mMod;
        };

        class TrackDescriptor
        {
        public:
            TrackDescriptor(const std::string& audioDescriptor, const std::string& videoDescriptor);
            std::string getAudioDescriptor() const;
            std::string getVideoDescriptor() const;
        protected:
            std::string mAudioDescriptor;
            std::string mVideoDescriptor;
        };

        typedef std::function<bool(karere::Id, const std::string&, int, std::vector<AnswerCommand::Peer>, std::map<karere::Id, std::string>, std::map<karere::Id, AnswerCommand::TrackDescriptor>)> AnswerCompleteFunction;
        AnswerCommand(const AnswerCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        AnswerCompleteFunction mComplete;

    private:
        void parsePeerObject(std::vector<AnswerCommand::Peer>&peers, rapidjson::Value::ConstMemberIterator& it) const;
        void parseSpeakerObject(std::map<karere::Id, TrackDescriptor> &speakers, rapidjson::Value::ConstMemberIterator& it) const;
        void parseVthumsObject(std::map<karere::Id, std::string> &vthumbs, rapidjson::Value::ConstMemberIterator& it) const;
    };

    typedef std::function<bool(uint64_t, karere::Id, const std::string&)> KeyCompleteFunction;
    class KeyCommand : public Command
    {
    public:
        KeyCommand(const KeyCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        KeyCompleteFunction mComplete;
    };

    typedef std::function<bool(const std::map<karere::Id, std::string>&)> VtumbsCompleteFunction;
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

    typedef std::function<bool(const std::map<karere::Id, std::string>&)> HiresCompleteFunction;
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

    typedef std::function<bool(const std::vector<karere::Id>&)> SpeakReqsCompleteFunction;
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

    typedef std::function<bool(void)> SpeakOnCompleteFunction;
    class SpeakOnCommand : public Command
    {
    public:
        SpeakOnCommand(const SpeakOnCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakOnCompleteFunction mComplete;
    };

    typedef std::function<bool(void)> SpeakOffCompleteFunction;
    class SpeakOffCommand : public Command
    {
    public:
        SpeakOffCommand(const SpeakOffCompleteFunction& complete);
        bool processCommand(const rapidjson::Document& command) override;
        static std::string COMMAND_NAME;
        SpeakOffCompleteFunction mComplete;
    };

    class SfuConnection : public karere::DeleteTrackable, public WebsocketsClient
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

        SfuConnection(const std::string& sfuUrl, karere::Client& karereClient, karere::Id cid);
        bool isOnline() const { return (mConnState >= kConnected); }
        promise::Promise<void> connect();
        void disconnect();
        void doConnect();
        void retryPendingConnection(bool disconnect);
        karere::Id getCid() const;
        bool sendCommand(const std::string& command);
        bool handleIncomingData(const char* data, size_t len);
        virtual bool handleAvCommand(karere::Id cid, int av);
        virtual bool handleAnswerCommand(karere::Id, const std::string&, int, const std::vector<AnswerCommand::Peer>&, const std::map<karere::Id, std::string>&, const std::map<karere::Id, AnswerCommand::TrackDescriptor>&);
        virtual bool handleKeyCommand(uint64_t, karere::Id, const std::string&);
        virtual bool handleVThumbsCommand(const std::map<karere::Id, std::string>&);
        virtual bool handleVThumbsStartCommand();
        virtual bool handleVThumbsStopCommand();
        virtual bool handleHiResCommand(const std::map<karere::Id, std::string> &);
        virtual bool handleHiResStartCommand();
        virtual bool handleHiResStopCommand();
        virtual bool handleSpeakReqsCommand(const std::vector<karere::Id>&);
        virtual bool handleSpeakReqDelCommand(karere::Id);
        virtual bool handleSpeakOnCommand();
        virtual bool handleSpeakOffCommand();

        bool joinCall(const std::string& sdp, const std::map<std::string, std::string>& ivs, int avFlags, int speaker, int vthumbs);
        bool sendKey(uint64_t id, const std::string& data);
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

    protected:
        std::string mSfuUrl;
        karere::Client& mKarereClient;

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
        karere::Id mCid;
    };

    class SfuClient
    {
    public:
        SfuClient(karere::Client& karereClient);
        promise::Promise<void> startCall(karere::Id chatid, const std::string& sfuUrl, karere::Id cid);
        void endCall(karere::Id chatid);

    private:
        std::map<karere::Id, std::unique_ptr<SfuConnection>> mConnections;
        karere::Client& mKarereClient;
    };

    static inline const char* connStateToStr(SfuConnection::ConnState state)
    {
        switch (state)
        {
        case SfuConnection::kDisconnected: return "Disconnected";
        case SfuConnection::kResolving: return "Resolving";
        case SfuConnection::kConnecting: return "Connecting";
        case SfuConnection::kConnected: return "Connected";
        case SfuConnection::kLoggedIn: return "Logged-in";
        case SfuConnection::kConnNew: return "New";
        default: return "(invalid)";
        }
    }

}

#endif // SFU_H
