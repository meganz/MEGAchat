#include "sfu.h"
#include <base/promise.h>
#include <megaapi.h>

namespace sfu
{

std::string Command::COMMAND_IDENTIFIER = "cmd";
std::string AVCommand::COMMAND_NAME = "AV";
std::string AnswerCommand::COMMAND_NAME = "ANSWER";
std::string KeyCommand::COMMAND_NAME = "KEY";
std::string VthumbsCommand::COMMAND_NAME = "VTHUMS";
std::string VthumbsStartCommand::COMMAND_NAME = "VTHUMS_START";
std::string VthumbsStopCommand::COMMAND_NAME = "VTHUMS_STOP";
std::string HiResCommand::COMMAND_NAME = "HIRES";
std::string HiResStartCommand::COMMAND_NAME = "HIRES_START";
std::string HiResStopCommand::COMMAND_NAME = "HIRES_STOP";
std::string SpeakReqsCommand::COMMAND_NAME = "SPEAK_RQ";
std::string SpeakReqDelCommand::COMMAND_NAME = "SPEAK_RQ_DEL";
std::string SpeakOnCommand::COMMAND_NAME = "SPEAK_ON";
std::string SpeakOffCommand::COMMAND_NAME = "SPEAK_OfF";

Command::Command()
{

}

AVCommand::AVCommand(const AvCompleteFunction &complete)
    : mComplete(complete)
{
}

bool AVCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    std::string cidString = cidIterator->value.GetString();
    ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());

    rapidjson::Value::ConstMemberIterator avIterator = command.FindMember("av");
    if (avIterator == command.MemberEnd() || !avIterator->value.IsInt())
    {
        SFU_LOG_ERROR("Received data doesn't have 'av' field");
        return false;
    }

    int av = avIterator->value.GetInt();
    return mComplete(cid, av);
}

AnswerCommand::AnswerCommand(const AnswerCompleteFunction &complete)
    : mComplete(complete)
{
}

bool AnswerCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsString())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'cid' field");
        return false;
    }

    std::string cidString = cidIterator->value.GetString();
    ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());

    rapidjson::Value::ConstMemberIterator modIterator = command.FindMember("mod");
    if (modIterator == command.MemberEnd() || !modIterator->value.IsInt())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'mod' field");
        return false;
    }

    int isModerator = modIterator->value.GetInt();

    rapidjson::Value::ConstMemberIterator sdpIterator = command.FindMember("cid");
    if (sdpIterator == command.MemberEnd() || !sdpIterator->value.IsString())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'sdp' field");
        return false;
    }

    std::string sdpString = sdpIterator->value.GetString();

    rapidjson::Value::ConstMemberIterator peersIterator = command.FindMember("peers");
    if (peersIterator == command.MemberEnd() || !peersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'peers' field");
        return false;
    }

    std::vector<AnswerCommand::Peer> peers;
    parsePeerObject(peers, peersIterator);

    rapidjson::Value::ConstMemberIterator speakersIterator = command.FindMember("speakers");
    if (speakersIterator == command.MemberEnd() || !speakersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'speakers' field");
        return false;
    }

    std::map<karere::Id, AnswerCommand::TrackDescriptor> speakers;
    parseSpeakerObject(speakers, peersIterator);

    rapidjson::Value::ConstMemberIterator vthumbsIterator = command.FindMember("vthumbs");
    if (vthumbsIterator == command.MemberEnd() || !vthumbsIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'vthumbs' field");
        return false;
    }

    std::map<karere::Id, std::string> vthumbs;
    parseVthumsObject(vthumbs, vthumbsIterator);

    return mComplete(cid, sdpString, isModerator, peers, vthumbs, speakers);
}

void AnswerCommand::parsePeerObject(std::vector<Peer> &peers, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        if (it->value[j].IsObject())
        {
            rapidjson::Value::ConstMemberIterator cidIterator = it->value[j].FindMember("cid");
            if (cidIterator == it->value.MemberEnd() || !cidIterator->value.IsString())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'cid' value");
                 return;
            }

            std::string cidString = cidIterator->value.GetString();
            ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());

            rapidjson::Value::ConstMemberIterator userIdIterator = it->value[j].FindMember("userId");
            if (userIdIterator == it->value.MemberEnd() || !userIdIterator->value.IsString())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'userId' value");
                 return;
            }

            std::string userIdString = userIdIterator->value.GetString();
            ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());

            rapidjson::Value::ConstMemberIterator avIterator = it->value[j].FindMember("av");
            if (avIterator == it->value.MemberEnd() || !avIterator->value.IsInt())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'av' value");
                 return;
            }

            int av = avIterator->value.GetInt();

            rapidjson::Value::ConstMemberIterator modIterator = it->value[j].FindMember("mod");
            if (modIterator == it->value.MemberEnd() || !modIterator->value.IsInt())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'mod' value");
                 return;
            }

            int mod = modIterator->value.GetInt();


            peers.push_back(AnswerCommand::Peer(cid, userId, av, mod));
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid value at array 'peers'");
            return;
        }
    }
}

void AnswerCommand::parseSpeakerObject(std::map<karere::Id, AnswerCommand::TrackDescriptor> &speakers, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        if (it->value[j].IsObject())
        {
            karere::Id cid;
            rapidjson::Value::ConstMemberIterator audioIterator = it->value[j].FindMember("audio");
            if (audioIterator == it->value.MemberEnd() || !audioIterator->value.IsString())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'audio' value");
                 return;
            }

            std::string audio = audioIterator->value.GetString();

            std::string video;
            rapidjson::Value::ConstMemberIterator videoIterator = it->value[j].FindMember("video");
            if (videoIterator != it->value.MemberEnd() || videoIterator->value.IsString())
            {
                 video = videoIterator->value.GetString();
            }

            speakers.insert(std::pair<karere::Id, AnswerCommand::TrackDescriptor>(cid, AnswerCommand::TrackDescriptor(audio, video)));
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid value at array 'peers'");
            return;
        }
    }
}

void AnswerCommand::parseVthumsObject(std::map<karere::Id, std::string> &vthumbs, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());

}

AnswerCommand::Peer::Peer(karere::Id cid, karere::Id peerid, int avFlags, int mod)
    : mCid(cid), mPeerid(peerid), mAvFlags(avFlags), mMod(mod)
{
}

karere::Id AnswerCommand::Peer::getCid() const
{
    return mCid;
}

karere::Id AnswerCommand::Peer::getPeerid() const
{
    return mPeerid;
}

int AnswerCommand::Peer::getAvFlags() const
{
    return mAvFlags;
}

int AnswerCommand::Peer::getMod() const
{
    return mMod;
}

AnswerCommand::TrackDescriptor::TrackDescriptor(const std::string &audioDescriptor, const std::string &videoDescriptor)
    : mAudioDescriptor(audioDescriptor), mVideoDescriptor(videoDescriptor)
{
}

std::string AnswerCommand::TrackDescriptor::getAudioDescriptor() const
{
    return mAudioDescriptor;
}

std::string AnswerCommand::TrackDescriptor::getVideoDescriptor() const
{
    return mVideoDescriptor;
}

KeyCommand::KeyCommand(const KeyCompleteFunction &complete)
    : mComplete(complete)
{

}

bool KeyCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator idIterator = command.FindMember("id");
    if (idIterator == command.MemberEnd() || !idIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'id' field");
        return false;
    }

    uint64_t id = idIterator->value.GetUint64();

    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    std::string cidString = cidIterator->value.GetString();
    ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());

    rapidjson::Value::ConstMemberIterator keyIterator = command.FindMember("key");
    if (keyIterator == command.MemberEnd() || !keyIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'key' field");
        return false;
    }

    std::string key = keyIterator->value.GetString();

    return mComplete(id, cid, key);
}

VthumbsCommand::VthumbsCommand(const VtumbsCompleteFunction &complete)
    : mComplete(complete)
{
}

bool VthumbsCommand::processCommand(const rapidjson::Document &command)
{
    ///TODO
    return false;
}

VthumbsStartCommand::VthumbsStartCommand(const VtumbsStartCompleteFunction &complete)
    : mComplete(complete)
{

}

bool VthumbsStartCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("VthumbsStartCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

VthumbsStopCommand::VthumbsStopCommand(const VtumbsStopCompleteFunction &complete)
    : mComplete(complete)
{

}

bool VthumbsStopCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("VthumbsStopCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

HiResCommand::HiResCommand(const HiresCompleteFunction &complete)
    : mComplete(complete)
{
}

bool HiResCommand::processCommand(const rapidjson::Document &command)
{
    std::map<karere::Id, std::string> tracks;

    return mComplete(tracks);
}

HiResStartCommand::HiResStartCommand(const HiResStartCompleteFunction &complete)
    : mComplete(complete)
{

}

bool HiResStartCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("VthumbsStartNotificationCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

HiResStopCommand::HiResStopCommand(const HiResStopCompleteFunction &complete)
    : mComplete(complete)
{

}

bool HiResStopCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("VthumbsStopNotificationCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

SpeakReqsCommand::SpeakReqsCommand(const SpeakReqsCompleteFunction &complete)
    : mComplete(complete)
{
}

bool SpeakReqsCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.IsArray())
    {
        SFU_LOG_ERROR("SpeakReqsCommand::processCommand - it isn't array");
        return false;
    }

    std::vector<karere::Id> speakRequest;
    for (unsigned int j = 0; j < command.Capacity(); ++j)
    {
        if (command[j].IsString())
        {
            std::string cidString = command[j].GetString();
            ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());
            speakRequest.push_back(cid);
        }
        else
        {
            SFU_LOG_ERROR("SpeakReqsCommand::processCommand - it isn't array");
            return false;
        }
    }

    return mComplete(speakRequest);
}

SpeakReqDelCommand::SpeakReqDelCommand(const SpeakReqDelCompleteFunction &complete)
    : mComplete(complete)
{
}

bool SpeakReqDelCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    std::string cidString = cidIterator->value.GetString();
    ::mega::MegaHandle cid = ::mega::MegaApi::base64ToUserHandle(cidString.c_str());

    return mComplete(cid);
}

SpeakOnCommand::SpeakOnCommand(const SpeakOnCompleteFunction &complete)
    : mComplete(complete)
{

}

bool SpeakOnCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("SpeakOnCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

SpeakOffCommand::SpeakOffCommand(const SpeakOffCompleteFunction &complete)
    : mComplete(complete)
{

}

bool SpeakOffCommand::processCommand(const rapidjson::Document &command)
{
    if (!command.Empty())
    {
        SFU_LOG_ERROR("SpeakOffCommand::processCommand - it isn't empty");
        return false;
    }

    return mComplete();
}

SfuConnection::SfuConnection(const std::string &sfuUrl, karere::Client& karereClient, karere::Id cid)
    : mSfuUrl(sfuUrl)
    , mKarereClient(karereClient)
    , mCid(cid)
{
    mCommands[AVCommand::COMMAND_NAME] = mega::make_unique<AVCommand>(std::bind(&SfuConnection::handleAvCommand, this,  std::placeholders::_1, std::placeholders::_2));
    mCommands[AnswerCommand::COMMAND_NAME] = mega::make_unique<AnswerCommand>(std::bind(&SfuConnection::handleAnswerCommand, this,  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    mCommands[KeyCommand::COMMAND_NAME] = mega::make_unique<KeyCommand>(std::bind(&SfuConnection::handleKeyCommand, this,  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    mCommands[VthumbsCommand::COMMAND_NAME] = mega::make_unique<VthumbsCommand>(std::bind(&SfuConnection::handleVThumbsCommand, this,  std::placeholders::_1));
    mCommands[VthumbsStartCommand::COMMAND_NAME] = mega::make_unique<VthumbsStartCommand>(std::bind(&SfuConnection::handleVThumbsStartCommand, this));
    mCommands[VthumbsStopCommand::COMMAND_NAME] = mega::make_unique<VthumbsStopCommand>(std::bind(&SfuConnection::handleVThumbsStopCommand, this));
    mCommands[HiResCommand::COMMAND_NAME] = mega::make_unique<HiResCommand>(std::bind(&SfuConnection::handleHiResCommand, this, std::placeholders::_1));
    mCommands[HiResStartCommand::COMMAND_NAME] = mega::make_unique<HiResStartCommand>(std::bind(&SfuConnection::handleHiResStartCommand, this));
    mCommands[HiResStopCommand::COMMAND_NAME] = mega::make_unique<HiResStopCommand>(std::bind(&SfuConnection::handleHiResStopCommand, this));
    mCommands[SpeakReqsCommand::COMMAND_NAME] = mega::make_unique<SpeakReqsCommand>(std::bind(&SfuConnection::handleSpeakReqsCommand, this,  std::placeholders::_1));
    mCommands[SpeakReqDelCommand::COMMAND_NAME] = mega::make_unique<SpeakReqDelCommand>(std::bind(&SfuConnection::handleSpeakReqDelCommand, this,  std::placeholders::_1));
    mCommands[SpeakOnCommand::COMMAND_NAME] = mega::make_unique<SpeakOnCommand>(std::bind(&SfuConnection::handleSpeakOnCommand, this));
    mCommands[SpeakOffCommand::COMMAND_NAME] = mega::make_unique<SpeakOffCommand>(std::bind(&SfuConnection::handleSpeakOffCommand, this));

}

promise::Promise<void> SfuConnection::connect()
{
    assert (mConnState == kConnNew);
    return reconnect()
    .fail([](const ::promise::Error& err)
    {
        SFU_LOG_DEBUG("SfuConnection::connect(): Error connecting to server after getting URL: %s", err.what());
    });
}

void SfuConnection::disconnect()
{
    setConnState(kDisconnected);
}

void SfuConnection::doConnect()
{
    const karere::Url url(mSfuUrl);
    assert (url.isValid());

    std::string ipv4 = mIpsv4.size() ? mIpsv4[0] : "";
    std::string ipv6 = mIpsv6.size() ? mIpsv6[0] : "";

    setConnState(kConnecting);
    SFU_LOG_DEBUG("Connecting to sfu using the IP: %s", mTargetIp.c_str());

    bool rt = wsConnect(mKarereClient.websocketIO, mTargetIp.c_str(),
          url.host.c_str(),
          url.port,
          url.path.c_str(),
          url.isSecure);

    if (!rt)    // immediate failure --> try the other IP family (if available)
    {
        SFU_LOG_DEBUG("Connection to sfu failed using the IP: %s", mTargetIp.c_str());

        std::string oldTargetIp = mTargetIp;
        mTargetIp.clear();
        if (oldTargetIp == ipv6 && ipv4.size())
        {
            mTargetIp = ipv4;
        }
        else if (oldTargetIp == ipv4 && ipv6.size())
        {
            mTargetIp = ipv6;
        }

        if (mTargetIp.size())
        {
            SFU_LOG_DEBUG("Retrying using the IP: %s", mTargetIp.c_str());
            if (wsConnect(mKarereClient.websocketIO, mTargetIp.c_str(),
                          url.host.c_str(),
                          url.port,
                          url.path.c_str(),
                          url.isSecure))
            {
                return;
            }
            SFU_LOG_DEBUG("Connection to sfu failed using the IP: %s", mTargetIp.c_str());
        }
        else
        {
            // do not close the socket, which forces a new retry attempt and turns the DNS response obsolete
            // Instead, let the DNS request to complete, in order to refresh IPs
            SFU_LOG_DEBUG("Empty cached IP. Waiting for DNS resolution...");
            return;
        }

        onSocketClose(0, 0, "Websocket error on wsConnect (sfu)");
    }
}

void SfuConnection::retryPendingConnection(bool disconnect)
{
    if (mConnState == kConnNew)
    {
        SFU_LOG_WARNING("retryPendingConnection: no connection to be retried yet. Call connect() first");
        return;
    }

    if (disconnect)
    {
        SFU_LOG_WARNING("retryPendingConnection: forced reconnection!");

        setConnState(kDisconnected);
        abortRetryController();
        reconnect();
    }
    else if (mRetryCtrl && mRetryCtrl->state() == karere::rh::State::kStateRetryWait)
    {
        SFU_LOG_WARNING("retryPendingConnection: abort backoff and reconnect immediately");

        assert(!isOnline());
        mRetryCtrl->restart();
    }
    else
    {
        SFU_LOG_WARNING("retryPendingConnection: ignored (currently connecting/connected, no forced disconnect was requested)");
    }
}

karere::Id SfuConnection::getCid() const
{
    return mCid;
}

bool SfuConnection::handleIncomingData(const char* data, size_t len)
{
    std::string receivedData(data, len);
    size_t bracketPosition = receivedData.find('}');
    std::string commandString = receivedData;
    if (bracketPosition < len)
    {
        commandString = commandString.substr(0, bracketPosition + 1);
    }

    rapidjson::StringStream stringStream(commandString.c_str());
    rapidjson::Document document;
    document.ParseStream(stringStream);

    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        SFU_LOG_ERROR("Failure at: Parser json error");
        return false;
    }

    rapidjson::Value::ConstMemberIterator jsonIterator = document.FindMember(Command::COMMAND_IDENTIFIER.c_str());
    if (jsonIterator == document.MemberEnd() || !jsonIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cmd' field");
        return false;
    }

    std::string command = jsonIterator->value.GetString();
    auto commandIterator = mCommands.find(command);
    if (commandIterator == mCommands.end())
    {
        SFU_LOG_ERROR("Command is not defined yet");
        return false;
    }

    bool processCommandResult = mCommands[command]->processCommand(document);

    if (commandString.length() < len)
    {
        size_t previousCommandSize = commandString.length();
        processCommandResult = handleIncomingData(&data[previousCommandSize], len - previousCommandSize);
    }

    return processCommandResult;
}

bool SfuConnection::handleAvCommand(karere::Id cid, int av)
{
    return true;
}

bool SfuConnection::handleAnswerCommand(karere::Id, const std::string &, int, const std::vector<AnswerCommand::Peer>&, const std::map<karere::Id, std::string>&, const std::map<karere::Id, AnswerCommand::TrackDescriptor>&)
{
    return true;
}

bool SfuConnection::handleKeyCommand(uint64_t, karere::Id, const std::string &)
{
    return true;
}

bool SfuConnection::handleVThumbsCommand(const std::map<karere::Id, std::string> &)
{
    return true;
}

bool SfuConnection::handleVThumbsStartCommand()
{
    return true;
}

bool SfuConnection::handleVThumbsStopCommand()
{
    return true;
}

bool SfuConnection::handleHiResCommand(const std::map<karere::Id, std::string>&)
{
    return true;
}

bool SfuConnection::handleHiResStartCommand()
{
    return true;
}

bool SfuConnection::handleHiResStopCommand()
{
    return true;
}

bool SfuConnection::handleSpeakReqsCommand(const std::vector<karere::Id>&)
{
    return true;
}

bool SfuConnection::handleSpeakReqDelCommand(karere::Id)
{
    return true;
}

bool SfuConnection::handleSpeakOnCommand()
{
    return true;
}

bool SfuConnection::handleSpeakOffCommand()
{
    return true;
}
void SfuConnection::setConnState(SfuConnection::ConnState newState)
{
    if (newState == mConnState)
    {
        SFU_LOG_DEBUG("Tried to change connection state to the current state: %s", connStateToStr(newState));
        return;
    }
    else
    {
        SFU_LOG_DEBUG("Connection state change: %s --> %s", connStateToStr(mConnState), connStateToStr(newState));
        mConnState = newState;
    }

    if (newState == kDisconnected)
    {
        // if a socket is opened, close it immediately
        if (wsIsConnected())
        {
            wsDisconnect(true);
        }

    }
    else if (mConnState == kConnected)
    {
        SFU_LOG_DEBUG("Sfu connected to %s", mTargetIp.c_str());

        assert(!mConnectPromise.done());
        mConnectPromise.resolve();
        mRetryCtrl.reset();
    }
}

void SfuConnection::wsConnectCb()
{
    setConnState(kConnected);
}

void SfuConnection::wsCloseCb(int errcode, int errtype, const char *preason, size_t preason_len)
{
    std::string reason;
    if (preason)
        reason.assign(preason, preason_len);

    onSocketClose(errcode, errtype, reason);
}

void SfuConnection::wsHandleMsgCb(char *data, size_t len)
{
    handleIncomingData(data, len);
}

void SfuConnection::onSocketClose(int errcode, int errtype, const std::string &reason)
{
    if (mKarereClient.isTerminated())
    {
        SFU_LOG_WARNING("Socket close but karere client was terminated.");
        return;
    }

    SFU_LOG_WARNING("Socket close on IP %s. Reason: %s", mTargetIp.c_str(), reason.c_str());

    auto oldState = mConnState;
    setConnState(kDisconnected);

    assert(oldState != kDisconnected);

    usingipv6 = !usingipv6;
    mTargetIp.clear();

    if (oldState >= kConnected)
    {
        SFU_LOG_DEBUG("Socket close at state kLoggedIn");

        assert(!mRetryCtrl);
        reconnect(); //start retry controller
    }
    else // (mConState < kConnected) --> tell retry controller that the connect attempt failed
    {
        SFU_LOG_DEBUG("Socket close and state is not kStateConnected (but %s), start retry controller", connStateToStr(oldState));

        assert(mRetryCtrl);
        assert(!mConnectPromise.succeeded());
        if (!mConnectPromise.done())
        {
            mConnectPromise.reject(reason, errcode, errtype);
        }
    }
}

promise::Promise<void> SfuConnection::reconnect()
{
    assert(!mRetryCtrl);
    try
    {
        if (mConnState >= kResolving) //would be good to just log and return, but we have to return a promise
            return ::promise::Error(std::string("Already connecting/connected"));

        setConnState(kResolving);

        // if there were an existing retry in-progress, abort it first or it will kick in after its backoff
        abortRetryController();

        // create a new retry controller and return its promise for reconnection
        auto wptr = weakHandle();
        mRetryCtrl.reset(createRetryController("sfu", [this](size_t attemptNo, DeleteTrackable::Handle wptr) -> promise::Promise<void>
        {
            if (wptr.deleted())
            {
                SFU_LOG_DEBUG("Reconnect attempt initiated, but sfu client was deleted.");
                return ::promise::_Void();
            }

            setConnState(kDisconnected);
            mConnectPromise = promise::Promise<void>();

            karere::Url url(mSfuUrl);

            setConnState(kResolving);
            SFU_LOG_DEBUG("Resolving hostname %s...", url.host.c_str());

            auto retryCtrl = mRetryCtrl.get();
            int statusDNS = wsResolveDNS(mKarereClient.websocketIO, url.host.c_str(),
                         [wptr, this, retryCtrl, attemptNo](int statusDNS, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    SFU_LOG_DEBUG("DNS resolution completed, but sfu client was deleted.");
                    return;
                }

                if (mKarereClient.isTerminated())
                {
                    SFU_LOG_DEBUG("DNS resolution completed but karere client was terminated.");
                    return;
                }

                if (!mRetryCtrl)
                {
                    SFU_LOG_DEBUG("DNS resolution completed but ignored: connection is already established using cached IP");
                    assert(isOnline());
                    return;
                }
                if (mRetryCtrl.get() != retryCtrl)
                {
                    SFU_LOG_DEBUG("DNS resolution completed but ignored: a newer retry has already started");
                    return;
                }
                if (mRetryCtrl->currentAttemptNo() != attemptNo)
                {
                    SFU_LOG_DEBUG("DNS resolution completed but ignored: a newer attempt is already started (old: %d, new: %d)",
                                     attemptNo, mRetryCtrl->currentAttemptNo());
                    return;
                }

                if (statusDNS < 0 || (ipsv4.empty() && ipsv6.empty()))
                {
                    if (isOnline())
                    {
                        assert(false);  // this case should be handled already at: if (!mRetryCtrl)
                        SFU_LOG_WARNING("DNS error, but connection is established. Relaying on cached IPs...");
                        return;
                    }

                    if (statusDNS < 0)
                    {
                        SFU_LOG_ERROR("Async DNS error in sfu. Error code: %d", statusDNS);
                    }
                    else
                    {
                        SFU_LOG_ERROR("Async DNS error in sfu. Empty set of IPs");
                    }

                    assert(!isOnline());
                    if (statusDNS == wsGetNoNameErrorCode(mKarereClient.websocketIO))
                    {
                        retryPendingConnection(true);
                    }
                    else
                    {
                        onSocketClose(0, 0, "Async DNS error (sfu connection)");
                    }
                    return;
                }

                if (mIpsv4.empty() && mIpsv6.empty()) // connect required DNS lookup
                {
                    SFU_LOG_DEBUG("Hostname resolved by first time. Connecting...");
                    mIpsv4 = ipsv4;
                    mIpsv6 = ipsv6;
                    doConnect();
                    return;
                }

                if (mIpsv4 != ipsv4 && mIpsv6 != ipsv6)
                {
                    SFU_LOG_DEBUG("DNS resolve matches cached IPs.");
                }
                else
                {
                    SFU_LOG_WARNING("DNS resolve doesn't match cached IPs. Forcing reconnect...");
                    mIpsv4 = ipsv4;
                    mIpsv6 = ipsv6;
                    onSocketClose(0, 0, "DNS resolve doesn't match cached IPs (sfu)");
                }
            });

            // immediate error at wsResolveDNS()
            if (statusDNS < 0)
            {
                std::string errStr = "Immediate DNS error in sfu. Error code: " + std::to_string(statusDNS);
                SFU_LOG_ERROR("%s", errStr.c_str());

                assert(mConnState == kResolving);
                assert(!mConnectPromise.done());

                // reject promise, so the RetryController starts a new attempt
                mConnectPromise.reject(errStr, statusDNS, promise::kErrorTypeGeneric);
            }
            else if (mIpsv4.size() || mIpsv6.size()) // if wsResolveDNS() failed immediately, very likely there's
            // no network connetion, so it's futile to attempt to connect
            {
                doConnect();
            }

            return mConnectPromise
            .then([wptr, this]()
            {
                if (wptr.deleted())
                    return;

                assert(isOnline());
            });

        }, wptr, mKarereClient.appCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

        return static_cast<promise::Promise<void>&>(mRetryCtrl->start());
    }
    KR_EXCEPTION_TO_PROMISE(0);
}

void SfuConnection::abortRetryController()
{
    if (!mRetryCtrl)
    {
        return;
    }

    assert(!isOnline());

    SFU_LOG_DEBUG("Reconnection was aborted");
    mRetryCtrl->abort();
    mRetryCtrl.reset();
}

SfuClient::SfuClient(karere::Client &karereClient)
    : mKarereClient(karereClient)
{

}

promise::Promise<void> SfuClient::startCall(karere::Id chatid, const std::string &sfuUrl, karere::Id cid)
{
    assert(mConnections.find(chatid) == mConnections.end());
    mConnections[chatid] = mega::make_unique<SfuConnection>(sfuUrl, mKarereClient, cid);
    return mConnections[chatid]->connect();
}

void SfuClient::endCall(karere::Id chatid)
{
    mConnections[chatid]->disconnect();
    mConnections.erase(chatid);
}

}
