#include "sfu.h"
#include "base/promise.h"
#include "megaapi.h"

#include<rapidjson/writer.h>

namespace sfu
{
// sfu->client commands
std::string Command::COMMAND_IDENTIFIER = "cmd";
std::string AVCommand::COMMAND_NAME = "AV";
std::string AnswerCommand::COMMAND_NAME = "ANSWER";
std::string KeyCommand::COMMAND_NAME = "KEY";
std::string VthumbsCommand::COMMAND_NAME = "VTHUMBS";
std::string VthumbsStartCommand::COMMAND_NAME = "VTHUMB_START";
std::string VthumbsStopCommand::COMMAND_NAME = "VTHUMB_STOP";
std::string HiResCommand::COMMAND_NAME = "HIRES";
std::string HiResStartCommand::COMMAND_NAME = "HIRES_START";
std::string HiResStopCommand::COMMAND_NAME = "HIRES_STOP";
std::string SpeakReqsCommand::COMMAND_NAME = "SPEAK_RQ";
std::string SpeakReqDelCommand::COMMAND_NAME = "SPEAK_RQ_DEL";
std::string SpeakOnCommand::COMMAND_NAME = "SPEAK_ON";
std::string SpeakOffCommand::COMMAND_NAME = "SPEAK_OfF";

Peer::Peer()
    : mCid(0), mPeerid(::karere::Id::inval()), mAvFlags(0), mModerator(0)
{
}

Peer::Peer(Cid_t cid, karere::Id peerid, int avFlags, int mod)
    : mCid(cid), mPeerid(peerid), mAvFlags(avFlags), mModerator(mod)
{
}

Peer::Peer(const Peer &peer)
    : mCid(peer.mCid)
    , mPeerid(peer.mPeerid)
    , mAvFlags(peer.mAvFlags)
    , mModerator(peer.mModerator)
{

}

void Peer::init(Cid_t cid, karere::Id peerid, int avFlags, int mod)
{
    mCid = cid;
    mPeerid = peerid;
    mAvFlags = avFlags;
    mModerator = mod;
}

Cid_t Peer::getCid() const
{
    return mCid;
}

karere::Id Peer::getPeerid() const
{
    return mPeerid;
}

Keyid_t Peer::getCurrentKeyId() const
{
    return mCurrentkeyId;
}

int Peer::getAvFlags() const
{
    return mAvFlags;
}

int Peer::getModerator() const
{
    return mModerator;
}


std::string Peer::getKey(Keyid_t keyid) const
{
    std::string key;
    auto it = mKeyMap.find(keyid);
    if (it != mKeyMap.end())
    {
        key = it->second;
    }
    return key;
}

void Peer::addKey(Keyid_t keyid, const std::string &key)
{
    assert(mKeyMap.find(keyid) == mKeyMap.end());
    mKeyMap[keyid] = key;
}

void Peer::setAvFlags(karere::AvFlags flags)
{
    mAvFlags = flags;
}

SpeakersDescriptor::SpeakersDescriptor()
{
}

SpeakersDescriptor::SpeakersDescriptor(const std::string &audioDescriptor, const std::string &videoDescriptor)
    : mAudioDescriptor(audioDescriptor), mVideoDescriptor(videoDescriptor)
{
}

std::string SpeakersDescriptor::getAudioDescriptor() const
{
    return mAudioDescriptor;
}

std::string SpeakersDescriptor::getVideoDescriptor() const
{
    return mVideoDescriptor;
}

void SpeakersDescriptor::setDescriptors(const std::string &audioDescriptor, const std::string &videoDescriptor)
{
    mAudioDescriptor = audioDescriptor;
    mVideoDescriptor = videoDescriptor;
}

Command::Command()
{
}

void Command::parseSpeakerObject(SpeakersDescriptor &speaker, rapidjson::Value::ConstMemberIterator &it) const
{
    rapidjson::Value::ConstMemberIterator audioIterator = it->value.FindMember("audio");
    if (audioIterator == it->value.MemberEnd() || !audioIterator->value.IsString())
    {
         SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'audio' value");
         return;
    }

    std::string audio = audioIterator->value.GetString();

    std::string video;
    rapidjson::Value::ConstMemberIterator videoIterator = it->value.FindMember("video");
    if (videoIterator != it->value.MemberEnd() || videoIterator->value.IsString())
    {
         video = videoIterator->value.GetString();
    }

    speaker.setDescriptors(audio, video);
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

    Cid_t cid = cidIterator->value.GetUint();

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

    std::vector<Peer> peers;
    parsePeerObject(peers, peersIterator);

    rapidjson::Value::ConstMemberIterator speakersIterator = command.FindMember("speakers");
    if (speakersIterator == command.MemberEnd() || !speakersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'speakers' field");
        return false;
    }

    std::map<Cid_t, SpeakersDescriptor> speakers;
    parseSpeakersObject(speakers, speakersIterator);

    rapidjson::Value::ConstMemberIterator vthumbsIterator = command.FindMember("vthumbs");
    if (vthumbsIterator == command.MemberEnd() || !vthumbsIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'vthumbs' field");
        return false;
    }

    std::map<Cid_t, VideoTrackDescriptor> vthumbs;
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


            peers.push_back(Peer(cid, userId, av, mod));
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid value at array 'peers'");
            return;
        }
    }
}

void AnswerCommand::parseSpeakersObject(std::map<Cid_t, SpeakersDescriptor> &speakers, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        Cid_t cid;
        rapidjson::Value::ConstMemberIterator cidIterator = it->value[j].FindMember("cid");
        if (cidIterator == it->value.MemberEnd() || !cidIterator->value.IsUint())
        {
             SFU_LOG_ERROR("parseSpeakersObject: invalid 'cid' value");
             return;
        }

        rapidjson::Value::ConstMemberIterator speakerIterator = it->value[j].FindMember("speaker");
        if (speakerIterator == it->value[j].MemberEnd() || !speakerIterator->value.IsArray())
        {
            SFU_LOG_ERROR("parseSpeakersObject: Received data doesn't have 'speaker' field");
            return;
        }

        SpeakersDescriptor speakerDescriptor;
        parseSpeakerObject(speakerDescriptor, speakerIterator);

        speakers.insert(std::pair<karere::Id, SpeakersDescriptor>(cid, speakerDescriptor));

    }
}

void AnswerCommand::parseVthumsObject(std::map<Cid_t, VideoTrackDescriptor> &vthumbs, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());

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

    // TODO: check if it's necessary to add new data type to Rapid json impl
    Keyid_t id = static_cast<Keyid_t>(idIterator->value.GetUint());

    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    Cid_t cid = cidIterator->value.GetUint();

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
    std::map<Cid_t, VideoTrackDescriptor> tracks;

    return mComplete(tracks);
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
    std::map<Cid_t, VideoTrackDescriptor> tracks;

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

    std::vector<Cid_t> speakRequest;
    for (unsigned int j = 0; j < command.Capacity(); ++j)
    {
        if (command[j].IsString())
        {
            Cid_t cid = command[j].GetUint();
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

    Cid_t cid = cidIterator->value.GetUint();

    return mComplete(cid);
}

SpeakOnCommand::SpeakOnCommand(const SpeakOnCompleteFunction &complete)
    : mComplete(complete)
{

}

bool SpeakOnCommand::processCommand(const rapidjson::Document &command)
{    
    Cid_t cid = 0;
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator != command.MemberEnd() && cidIterator->value.IsUint())
    {
        cid = cidIterator->value.GetUint();

        rapidjson::Value::ConstMemberIterator speakerIterator = command.FindMember("speaker");
        if (speakerIterator == command.MemberEnd() || !speakerIterator->value.IsArray())
        {
            SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'speakers' field");
            return false;
        }

        SpeakersDescriptor speaker;
        parseSpeakerObject(speaker, speakerIterator);
        return mComplete(cid, speaker);
    }

    return false;
}

SpeakOffCommand::SpeakOffCommand(const SpeakOffCompleteFunction &complete)
    : mComplete(complete)
{

}

bool SpeakOffCommand::processCommand(const rapidjson::Document &command)
{    
    Cid_t cid = 0;
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator != command.MemberEnd() && cidIterator->value.IsUint())
    {
        cid = cidIterator->value.GetUint();
    }

    return mComplete(cid);
}

SfuConnection::SfuConnection(const std::string &sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface &call)
    : mSfuUrl(sfuUrl)
    , mWebsocketIO(websocketIO)
    , mAppCtx(appCtx)
    , mCall(call)
{
    mCommands[AVCommand::COMMAND_NAME] = mega::make_unique<AVCommand>(std::bind(&sfu::SfuInterface::handleAvCommand, &call, std::placeholders::_1, std::placeholders::_2));
    mCommands[AnswerCommand::COMMAND_NAME] = mega::make_unique<AnswerCommand>(std::bind(&sfu::SfuInterface::handleAnswerCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    mCommands[KeyCommand::COMMAND_NAME] = mega::make_unique<KeyCommand>(std::bind(&sfu::SfuInterface::handleKeyCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    mCommands[VthumbsCommand::COMMAND_NAME] = mega::make_unique<VthumbsCommand>(std::bind(&sfu::SfuInterface::handleVThumbsCommand, &call, std::placeholders::_1));
    mCommands[VthumbsStartCommand::COMMAND_NAME] = mega::make_unique<VthumbsStartCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStartCommand, &call));
    mCommands[VthumbsStopCommand::COMMAND_NAME] = mega::make_unique<VthumbsStopCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStopCommand, &call));
    mCommands[HiResCommand::COMMAND_NAME] = mega::make_unique<HiResCommand>(std::bind(&sfu::SfuInterface::handleHiResCommand, &call, std::placeholders::_1));
    mCommands[HiResStartCommand::COMMAND_NAME] = mega::make_unique<HiResStartCommand>(std::bind(&sfu::SfuInterface::handleHiResStartCommand, &call));
    mCommands[HiResStopCommand::COMMAND_NAME] = mega::make_unique<HiResStopCommand>(std::bind(&sfu::SfuInterface::handleHiResStopCommand, &call));
    mCommands[SpeakReqsCommand::COMMAND_NAME] = mega::make_unique<SpeakReqsCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqsCommand, &call, std::placeholders::_1));
    mCommands[SpeakReqDelCommand::COMMAND_NAME] = mega::make_unique<SpeakReqDelCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqDelCommand, &call, std::placeholders::_1));
    mCommands[SpeakOnCommand::COMMAND_NAME] = mega::make_unique<SpeakOnCommand>(std::bind(&sfu::SfuInterface::handleSpeakOnCommand, &call, std::placeholders::_1, std::placeholders::_2));
    mCommands[SpeakOffCommand::COMMAND_NAME] = mega::make_unique<SpeakOffCommand>(std::bind(&sfu::SfuInterface::handleSpeakOffCommand, &call, std::placeholders::_1));

}

bool SfuConnection::isOnline() const
{
    return (mConnState >= kConnected);
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

    bool rt = wsConnect(&mWebsocketIO, mTargetIp.c_str(),
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
            if (wsConnect(&mWebsocketIO, mTargetIp.c_str(),
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

Cid_t SfuConnection::getCid() const
{
    return mCid;
}

bool SfuConnection::sendCommand(const std::string &command)
{
    if (!isOnline())
        return false;

    // if several data are written to the output buffer to be sent all together, wait for all of them
    if (mSendPromise.done())
    {
        mSendPromise = promise::Promise<void>();
        auto wptr = weakHandle();
        mSendPromise.fail([this, wptr](const promise::Error& err)
        {
            if (wptr.deleted())
                return;

           SFU_LOG_WARNING("Failed to send data. Error: %s", err.what());
        });
    }

    std::unique_ptr<char[]> dfa(mega::MegaApi::strdup(command.c_str()));
    bool rc = wsSendMessage(dfa.get(), command.length());

    if (!rc)
    {
        mSendPromise.reject("Socket is not ready");
    }

    return rc;
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
    if (processCommandResult && command == AnswerCommand::COMMAND_NAME)
    {
        setConnState(SfuConnection::kJoined);
    }

    if (commandString.length() < len)
    {
        size_t previousCommandSize = commandString.length();
        processCommandResult = handleIncomingData(&data[previousCommandSize], len - previousCommandSize);
    }

    return processCommandResult;
}

promise::Promise<void> SfuConnection::getPromiseConnection()
{
    return mConnectPromise;
}

bool SfuConnection::joinSfu(const std::string &sdp, const std::map<int, std::string> &ivs, int avFlags, int speaker, int vthumbs)
{
    rapidjson::Document json(rapidjson::kObjectType);

    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_JOIN.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());


    rapidjson::Value sdpValue(rapidjson::kStringType);
    sdpValue.SetString(sdp.c_str(), sdp.length(), json.GetAllocator());
    json.AddMember(rapidjson::Value("sdp"), sdpValue, json.GetAllocator());

    ///TODO ivs

    rapidjson::Value avValue(rapidjson::kNumberType);
    avValue.SetInt(avFlags);
    json.AddMember(rapidjson::Value("av"), avValue, json.GetAllocator());

    if (speaker)
    {
        rapidjson::Value speakerValue(rapidjson::kNumberType);
        speakerValue.SetInt(avFlags);
        json.AddMember(rapidjson::Value("spk"), speakerValue, json.GetAllocator());
    }

    if (vthumbs > 0)
    {
        rapidjson::Value vThumbsValue(rapidjson::kNumberType);
        vThumbsValue.SetInt(vthumbs);
        json.AddMember(rapidjson::Value("vthumbs"), vThumbsValue, json.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());

    setConnState(SfuConnection::kJoining);

    return sendCommand(command);
}

bool SfuConnection::sendKey(uint64_t id, const std::string &data)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SENDKEY.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value idValue(rapidjson::kNumberType);
    idValue.SetUint64(id);
    json.AddMember(rapidjson::Value("id"), idValue, json.GetAllocator());

    rapidjson::Value dataValue(rapidjson::kStringType);
    dataValue.SetString(data.data(), data.size(), json.GetAllocator());
    json.AddMember(rapidjson::Value("data"), dataValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendAv(int av)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_AV.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value avValue(rapidjson::kNumberType);
    avValue.SetInt(av);
    json.AddMember(rapidjson::Value("av"), avValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendGetVtumbs(const std::vector<karere::Id> &cids)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_GET_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value cidsValue(rapidjson::kArrayType);
    for(karere::Id cid : cids)
    {
        std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
        cidsValue.PushBack(rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    }

    json.AddMember("cids", cidsValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendDelVthumbs(const std::vector<karere::Id> &cids)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_DEL_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value cidsValue(rapidjson::kArrayType);
    for(karere::Id cid : cids)
    {
        std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
        cidsValue.PushBack(rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    }

    json.AddMember("cids", cidsValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendGetHiRes(karere::Id cid, int r, int lo)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_GET_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
    json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    json.AddMember("r", rapidjson::Value(r), json.GetAllocator());
    json.AddMember("lo", rapidjson::Value(lo), json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendDelHiRes(karere::Id cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_DEL_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
    json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendHiResSetLo(karere::Id cid, int lo)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_HIRES_SET_LO.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
    json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    json.AddMember("lo", rapidjson::Value(lo), json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendLayer(int spt, int tmp, int stmp)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_LAYER.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());
    json.AddMember("spt", rapidjson::Value(spt), json.GetAllocator());
    json.AddMember("tmp", rapidjson::Value(tmp), json.GetAllocator());
    json.AddMember("stmp", rapidjson::Value(stmp), json.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakReq(karere::Id cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_RQ.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid.isValid())
    {
        std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
        json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakReqDel(karere::Id cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_RQ_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid.isValid())
    {
        std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
        json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakDel(karere::Id cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid.isValid())
    {
        std::unique_ptr<char[]> cidString = std::unique_ptr<char[]>(::mega::MegaApi::userHandleToBase64(cid.val));
        json.AddMember("cid", rapidjson::Value(cidString.get(), strlen(cidString.get())), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendModeratorRequested(karere::Id cid)
{
    assert(false);
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

void SfuConnection::wsSendMsgCb(const char *, size_t)
{
    assert(!mSendPromise.done());
    mSendPromise.resolve();
}

void SfuConnection::onSocketClose(int errcode, int errtype, const std::string &reason)
{
//    if (mKarereClient.isTerminated())
//    {
//        SFU_LOG_WARNING("Socket close but karere client was terminated.");
//        return;
//    }

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
            int statusDNS = wsResolveDNS(&mWebsocketIO, url.host.c_str(),
                         [wptr, this, retryCtrl, attemptNo](int statusDNS, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    SFU_LOG_DEBUG("DNS resolution completed, but sfu client was deleted.");
                    return;
                }

//                if (mKarereClient.isTerminated())
//                {
//                    SFU_LOG_DEBUG("DNS resolution completed but karere client was terminated.");
//                    return;
//                }

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
                    if (statusDNS == wsGetNoNameErrorCode(&mWebsocketIO))
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

        }, wptr, mAppCtx, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

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

SfuClient::SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings, karere::Client& karereClient)
    : mRtcCryptoMeetings(std::make_shared<rtcModule::RtcCryptoMeetings>(*rRtcCryptoMeetings))
    , mWebsocketIO(websocketIO)
    , mKarereClient(karereClient)
    , mAppCtx(appCtx)
{

}

SfuConnection* SfuClient::generateSfuConnection(karere::Id chatid, const std::string &sfuUrl, SfuInterface &call)
{
    assert(mConnections.find(chatid) == mConnections.end());
    mConnections[chatid] = mega::make_unique<SfuConnection>(sfuUrl, mWebsocketIO, mAppCtx, call);
    SfuConnection* sfuConnection = mConnections[chatid].get();
    sfuConnection->connect();
    return sfuConnection;
}

void SfuClient::closeManagerProtocol(karere::Id chatid)
{
    mConnections[chatid]->disconnect();
    mConnections.erase(chatid);
}

std::shared_ptr<rtcModule::RtcCryptoMeetings> SfuClient::getRtcCryptoMeetings()
{
    return mRtcCryptoMeetings;
}

karere::Client& SfuClient::getKarereClient()
{
    return mKarereClient;
}

}
