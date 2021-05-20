#include <sfu.h>
#include <base/promise.h>
#include <megaapi.h>
#include <mega/base64.h>

#include<rapidjson/writer.h>

namespace sfu
{
// sfu->client commands
std::string Command::COMMAND_IDENTIFIER = "a";
std::string AVCommand::COMMAND_NAME = "AV";
std::string AnswerCommand::COMMAND_NAME = "ANSWER";
std::string KeyCommand::COMMAND_NAME = "KEY";
std::string VthumbsCommand::COMMAND_NAME = "VTHUMBS";
std::string VthumbsStartCommand::COMMAND_NAME = "VTHUMB_START";
std::string VthumbsStopCommand::COMMAND_NAME = "VTHUMB_STOP";
std::string HiResCommand::COMMAND_NAME = "HIRES";
std::string HiResStartCommand::COMMAND_NAME = "HIRES_START";
std::string HiResStopCommand::COMMAND_NAME = "HIRES_STOP";
std::string SpeakReqsCommand::COMMAND_NAME = "SPEAK_REQS";
std::string SpeakReqDelCommand::COMMAND_NAME = "SPEAK_RQ_DEL";
std::string SpeakOnCommand::COMMAND_NAME = "SPEAK_ON";
std::string SpeakOffCommand::COMMAND_NAME = "SPEAK_OFF";
std::string StatCommand::COMMAND_NAME = "STAT";
std::string PeerJoinCommand::COMMAND_NAME = "PEERJOIN";
std::string PeerLeftCommand::COMMAND_NAME = "PEERLEFT";
std::string ErrorCommand::COMMAND_NAME = "ERR";
std::string ModeratorCommand::COMMAND_NAME = "MOD"; // only for testing purposes

const std::string Sdp::endl = "\r\n";

CommandsQueue::CommandsQueue():
    isSending(false)
{
}

bool CommandsQueue::sending()
{
    return isSending;
}

void CommandsQueue::setSending(bool sending)
{
    isSending = sending;
}

void CommandsQueue::push(const std::string& command)
{
    commands.push_back(command);
}

std::string CommandsQueue::pop()
{
    if (commands.empty())
    {
        return std::string();
    }

    std::string command = std::move(commands.front());
    commands.pop_front();
    return command;
}

bool CommandsQueue::isEmpty()
{
    return commands.empty();
}

void CommandsQueue::clear()
{
    commands.clear();
}

Peer::Peer()
    : mCid(0), mPeerid(::karere::Id::inval()), mAvFlags(0)
{
}

Peer::Peer(Cid_t cid, karere::Id peerid, unsigned avFlags)
    : mCid(cid), mPeerid(peerid), mAvFlags(avFlags)
{
}

Peer::Peer(const Peer &peer)
    : mCid(peer.mCid)
    , mPeerid(peer.mPeerid)
    , mAvFlags(peer.mAvFlags)
{

}

void Peer::init(Cid_t cid, karere::Id peerid, unsigned avFlags)
{
    mCid = cid;
    mPeerid = peerid;
    mAvFlags = avFlags;
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

karere::AvFlags Peer::getAvFlags() const
{
    return mAvFlags;
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
    mCurrentkeyId = keyid;
    mKeyMap[mCurrentkeyId] = key;
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

Command::~Command()
{
}

Command::Command()
{
}

void Command::parseSpeakerObject(SpeakersDescriptor &speaker, rapidjson::Value::ConstMemberIterator &it) const
{
    rapidjson::Value::ConstMemberIterator audioIterator = it->value.FindMember("audio");
    if (audioIterator == it->value.MemberEnd() || !audioIterator->value.IsString())
    {
         SFU_LOG_ERROR("Command::parseSpeakerObject: invalid 'audio' value");
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

bool Command::parseTrackDescriptor(TrackDescriptor &trackDescriptor, rapidjson::Value::ConstMemberIterator &it) const
{
    rapidjson::Value::ConstMemberIterator ivIterator = it->value.FindMember("iv");
    if (ivIterator == it->value.MemberEnd() || !ivIterator->value.IsString())
    {
         SFU_LOG_ERROR("parseTrackDescriptor: 'iv' field not found");
         return false;
    }

    std::string ivString = ivIterator->value.GetString();


    rapidjson::Value::ConstMemberIterator midIterator = it->value.FindMember("mid");
    if (midIterator == it->value.MemberEnd() || !midIterator->value.IsUint())
    {
         SFU_LOG_ERROR("parseTrackDescriptor: 'mid' field not found");
         return false;
    }

    rapidjson::Value::ConstMemberIterator reuseIterator = it->value.FindMember("r");
    if (reuseIterator != it->value.MemberEnd() && reuseIterator->value.IsUint())
    {
        // parse reuse flag in case it's found in trackDescriptor
        trackDescriptor.mReuse = reuseIterator->value.GetUint();
    }

    trackDescriptor.mMid = midIterator->value.GetUint();
    trackDescriptor.mIv = hexToBinary(ivString);
    return true;
}

uint64_t Command::hexToBinary(const std::string &hex)
{
    uint64_t value = 0;
    unsigned int bufferSize = hex.length() >> 1;
    assert(bufferSize <= 8);
    std::unique_ptr<uint8_t []> buffer = std::unique_ptr<uint8_t []>(new uint8_t[bufferSize]);
    unsigned int binPos = 0;
    for (unsigned int i = 0; i< hex.length(); binPos++)
    {
        buffer[binPos] = (hexDigitVal(hex[i++])) << 4 | hexDigitVal(hex[i++]);
    }

    memcpy(&value, buffer.get(), bufferSize);

    return value;
}

uint8_t Command::hexDigitVal(char value)
{
    if (value <= 57)
    { // ascii code if '9'
        return value - 48; // ascii code of '0'
    }
    else if (value >= 97)
    { // 'a'
        return 10 + value - 97;
    }
    else
    {
        return 10 + value - 65; // 'A'
    }
}

std::string Command::binaryToHex(uint64_t value)
{
    std::vector<std::string> hexDigits = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"};
    std::string result;
    uint8_t intermediate[8];
    memcpy(intermediate, &value, 8);
    for (unsigned int i = 0; i < sizeof(value); i++)
    {
        uint8_t firstPart = (intermediate[i] >> 4) & 0x0f;
        uint8_t secondPart = intermediate[i] & 0x0f;
        result.append(hexDigits[firstPart]);
        result.append(hexDigits[secondPart]);
    }

    return result;
}

AVCommand::AVCommand(const AvCompleteFunction &complete)
    : mComplete(complete)
{
}

bool AVCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    Cid_t cid = cidIterator->value.GetUint();
    rapidjson::Value::ConstMemberIterator avIterator = command.FindMember("av");
    if (avIterator == command.MemberEnd() || !avIterator->value.IsInt())
    {
        SFU_LOG_ERROR("Received data doesn't have 'av' field");
        return false;
    }

    unsigned av = avIterator->value.GetUint();
    return mComplete(cid, av);
}

AnswerCommand::AnswerCommand(const AnswerCompleteFunction &complete)
    : mComplete(complete)
{
}

bool AnswerCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'cid' field");
        return false;
    }

    Cid_t cid = cidIterator->value.GetUint();
    rapidjson::Value::ConstMemberIterator sdpIterator = command.FindMember("sdp");
    if (sdpIterator == command.MemberEnd() || !sdpIterator->value.IsObject())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'sdp' field");
        return false;
    }

    Sdp sdp(sdpIterator->value);

    rapidjson::Value::ConstMemberIterator tsIterator = command.FindMember("t");
    if (tsIterator == command.MemberEnd() || !tsIterator->value.IsUint64())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 't' field");
        return false;
    }

    // call start ts (ms)
    uint64_t ts = tsIterator->value.GetUint64();

    std::vector<Peer> peers;
    rapidjson::Value::ConstMemberIterator peersIterator = command.FindMember("peers");
    if (peersIterator != command.MemberEnd() && peersIterator->value.IsArray())
    {
        parsePeerObject(peers, peersIterator);
    }

    std::map<Cid_t, TrackDescriptor> speakers;
    rapidjson::Value::ConstMemberIterator speakersIterator = command.FindMember("speakers");
    if (speakersIterator != command.MemberEnd() && speakersIterator->value.IsObject())
    {
        parseTracks(peers, speakers, speakersIterator, true);
    }

    std::map<Cid_t, TrackDescriptor> vthumbs;
    rapidjson::Value::ConstMemberIterator vthumbsIterator = command.FindMember("vthumbs");
    if (vthumbsIterator != command.MemberEnd() && vthumbsIterator->value.IsObject())
    {
        parseTracks(peers, vthumbs, vthumbsIterator);
    }

    return mComplete(cid, sdp, ts, peers, vthumbs, speakers);
}

void AnswerCommand::parsePeerObject(std::vector<Peer> &peers, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        if (it->value[j].IsObject())
        {
            rapidjson::Value::ConstMemberIterator cidIterator = it->value[j].FindMember("cid");
            if (cidIterator == it->value[j].MemberEnd() || !cidIterator->value.IsUint())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'cid' value");
                 return;
            }

            Cid_t cid = cidIterator->value.GetUint();

            rapidjson::Value::ConstMemberIterator userIdIterator = it->value[j].FindMember("userId");
            if (userIdIterator == it->value[j].MemberEnd() || !userIdIterator->value.IsString())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'userId' value");
                 return;
            }

            std::string userIdString = userIdIterator->value.GetString();
            ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());

            rapidjson::Value::ConstMemberIterator avIterator = it->value[j].FindMember("av");
            if (avIterator == it->value[j].MemberEnd() || !avIterator->value.IsUint())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'av' value");
                 return;
            }

            unsigned av = avIterator->value.GetUint();
            peers.push_back(Peer(cid, userId, av));
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid value at array 'peers'");
            return;
        }
    }
}

bool AnswerCommand::parseTracks(const std::vector<Peer> &peers, std::map<Cid_t, TrackDescriptor>& tracks, rapidjson::Value::ConstMemberIterator &it, bool audio) const
{
    for (const Peer& peer : peers)
    {
        std::string cid = std::to_string(peer.getCid());
        rapidjson::Value::ConstMemberIterator iterator = it->value.FindMember(cid.c_str());
        if (iterator == it->value.MemberEnd() || !iterator->value.IsObject())
        {
             SFU_LOG_ERROR("parseTracks: invalid 'cid' value");
             return false;
        }

        if (audio)
        {
            rapidjson::Value::ConstMemberIterator audioIterator = iterator->value.FindMember("audio");
            if (audioIterator == iterator->value.MemberEnd() || !audioIterator->value.IsObject())
            {
                 SFU_LOG_ERROR("parseTracks: invalid 'audio' value");
                 return false;
            }

            iterator = audioIterator;
        }

        TrackDescriptor track;
        bool valid = parseTrackDescriptor(track, iterator);
        if (valid)
        {
            tracks[peer.getCid()] = track;
        }
        else
        {
            return false;
        }
    }

     return true;
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

void AnswerCommand::parseVthumsObject(std::map<Cid_t, TrackDescriptor> &vthumbs, rapidjson::Value::ConstMemberIterator &it) const
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
    if (idIterator == command.MemberEnd() || !idIterator->value.IsUint())
    {
        SFU_LOG_ERROR("KeyCommand: Received data doesn't have 'id' field");
        return false;
    }

    // TODO: check if it's necessary to add new data type to Rapid json impl
    Keyid_t id = static_cast<Keyid_t>(idIterator->value.GetUint());

    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("from");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("KeyCommand: Received data doesn't have 'from' field");
        return false;
    }

    Cid_t cid = cidIterator->value.GetUint();

    rapidjson::Value::ConstMemberIterator keyIterator = command.FindMember("key");
    if (keyIterator == command.MemberEnd() || !keyIterator->value.IsString())
    {
        SFU_LOG_ERROR("KeyCommand: Received data doesn't have 'key' field");
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
    Cid_t cid = 0;
    std::map<Cid_t, TrackDescriptor> tracks;
    rapidjson::Value::ConstMemberIterator it = command.FindMember("tracks");
    if (it != command.MemberEnd())
    {
        assert(it->value.IsObject());
        for (auto itMember = it->value.MemberBegin(); itMember != it->value.MemberEnd(); ++itMember)
        {
            assert(itMember->name.IsString() && itMember->value.IsObject());
            cid = static_cast<Cid_t>(atoi(itMember->name.GetString()));
            TrackDescriptor td;
            parseTrackDescriptor(td, itMember);
            tracks[cid] = td; // add entry to map <cid, trackDescriptor>
        }
    }
    return mComplete(tracks);
}

VthumbsStartCommand::VthumbsStartCommand(const VtumbsStartCompleteFunction &complete)
    : mComplete(complete)
{

}

bool VthumbsStartCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

VthumbsStopCommand::VthumbsStopCommand(const VtumbsStopCompleteFunction &complete)
    : mComplete(complete)
{

}

bool VthumbsStopCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

HiResCommand::HiResCommand(const HiresCompleteFunction &complete)
    : mComplete(complete)
{
}

bool HiResCommand::processCommand(const rapidjson::Document &command)
{
    Cid_t cid = 0;
    std::map<Cid_t, TrackDescriptor> tracks;
    rapidjson::Value::ConstMemberIterator it = command.FindMember("tracks");
    if (it != command.MemberEnd())
    {
        assert(it->value.IsObject());
        for (auto itMember = it->value.MemberBegin(); itMember != it->value.MemberEnd(); ++itMember)
        {
            assert(itMember->name.IsString() && itMember->value.IsObject());
            cid = static_cast<Cid_t>(atoi(itMember->name.GetString()));
            TrackDescriptor td;
            parseTrackDescriptor(td, itMember);
            tracks[cid] = td; // add entry to map <cid, trackDescriptor>
        }
    }

    return mComplete(tracks);
}

HiResStartCommand::HiResStartCommand(const HiResStartCompleteFunction &complete)
    : mComplete(complete)
{

}

bool HiResStartCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

HiResStopCommand::HiResStopCommand(const HiResStopCompleteFunction &complete)
    : mComplete(complete)
{

}

bool HiResStopCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

SpeakReqsCommand::SpeakReqsCommand(const SpeakReqsCompleteFunction &complete)
    : mComplete(complete)
{
}

bool SpeakReqsCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidsIterator = command.FindMember("cids");
    if (cidsIterator == command.MemberEnd() || !cidsIterator->value.IsArray())
    {
        SFU_LOG_ERROR("SpeakReqsCommand::processCommand - Received data doesn't have 'cids' field");
        return false;
    }

    std::vector<Cid_t> speakRequest;
    for (unsigned int j = 0; j < cidsIterator->value.Capacity(); ++j)
    {
        if (cidsIterator->value[j].IsUint())
        {
            Cid_t cid = cidsIterator->value[j].GetUint();
            speakRequest.push_back(cid);
        }
        else
        {
            SFU_LOG_ERROR("SpeakReqsCommand::processCommand - it isn't uint");
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
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("SpeakReqDelCommand: Received data doesn't have 'cid' field");
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

        rapidjson::Value::ConstMemberIterator audioIterator = command.FindMember("audio");
        if (audioIterator == command.MemberEnd() || !audioIterator->value.IsObject())
        {
            SFU_LOG_ERROR("SpeakOnCommand::processCommand: Received data doesn't have 'audio' field");
            return false;
        }

        TrackDescriptor descriptor;
        parseTrackDescriptor(descriptor, audioIterator);
        return mComplete(cid, descriptor);
    }
    else
    {
        return mComplete(cid, sfu::TrackDescriptor());
    }
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

StatCommand::StatCommand(const StatCommandFunction &complete)
    : mComplete(complete)
{

}

bool StatCommand::processCommand(const rapidjson::Document &command)
{
    return true;
}

PeerJoinCommand::PeerJoinCommand(const PeerJoinCommandFunction &complete)
    : mComplete(complete)
{
}

bool PeerJoinCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("PeerJoinCommand: Received data doesn't have 'cid' field");
        return false;
    }

    Cid_t cid = cidIterator->value.GetUint();

    rapidjson::Value::ConstMemberIterator userIdIterator = command.FindMember("userId");
    if (userIdIterator == command.MemberEnd() || !userIdIterator->value.IsString())
    {
        SFU_LOG_ERROR("PeerJoinCommand: Received data doesn't have 'userId' field");
        return false;
    }

    std::string userIdString = userIdIterator->value.GetString();
    uint64_t userid = mega::MegaApi::base64ToUserHandle(userIdString.c_str());

    rapidjson::Value::ConstMemberIterator avIterator = command.FindMember("av");
    if (avIterator == command.MemberEnd() || !avIterator->value.IsUint())
    {
        SFU_LOG_ERROR("PeerJoinCommand: Received data doesn't have 'av' field");
        return false;
    }

    unsigned int av = avIterator->value.GetUint();

    return mComplete(cid, userid, av);

}

ErrorCommand::ErrorCommand(const ErrorCommandFunction &complete)
    : mComplete(complete)
{
}

bool ErrorCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator codeIterator = command.FindMember("code");
    if (codeIterator == command.MemberEnd() || !codeIterator->value.IsUint())
    {
        SFU_LOG_ERROR("ErrorCommand: Received data doesn't have 'code' field");
        return false;
    }

    std::string errorMsg = "";
    unsigned int code = codeIterator->value.GetUint();
    rapidjson::Value::ConstMemberIterator msgIterator = command.FindMember("msg");
    if (msgIterator != command.MemberEnd() && msgIterator->value.IsString())
    {
        errorMsg = msgIterator->value.GetString();
    }
    return mComplete(code, errorMsg);
}

Sdp::Sdp(const std::string &sdp)
{
    size_t pos = 0;
    std::string buffer = sdp;
    std::vector<std::string> tokens;
    while ((pos = buffer.find(endl)) != std::string::npos)
    {
        std::string token = buffer.substr(0, pos);
        tokens.push_back(token);
        buffer.erase(0, pos + endl.size());
    }

    for (const std::string& line : tokens)
    {
        if (line.size() > 2 && line[0] == 'm' && line[1] == '=')
        {
            break;
        }

        mData["cmn"].append(line).append(endl);
    }

    unsigned int i = 0;
    while (i < tokens.size())
    {
        const std::string& line = tokens.at(i);
        std::string type = line.substr(2, 5);
        if (type == "audio" && mData.find("atpl") == mData.end())
        {
            i = createTemplate("atpl", tokens, i);
            if (mData.find("vtpl") != mData.end())
            {
                break;
            }
        }
        else if (type == "video" && mData.find("vtpl") == mData.end())
        {
            i = createTemplate("vtpl", tokens, i);
            if (mData.find("atpl") != mData.end())
            {
                break;
            }
        }
        else
        {
            i = nextMline(tokens, i + 1);
        }
    }

    for (i = nextMline(tokens, 0); i < tokens.size();)
    {
        i = addTrack(tokens, i);
    }
}

Sdp::Sdp(const rapidjson::Value &sdp)
{
    rapidjson::Value::ConstMemberIterator cmnIterator = sdp.FindMember("cmn");
    if (cmnIterator != sdp.MemberEnd() && cmnIterator->value.IsString())
    {
        mData["cmn"] = cmnIterator->value.GetString();
    }

    std::string atpl;
    rapidjson::Value::ConstMemberIterator atplIterator = sdp.FindMember("atpl");
    if (atplIterator != sdp.MemberEnd() && atplIterator->value.IsString())
    {
       mData["atpl"] = atplIterator->value.GetString();
    }

    std::string vtpl;
    rapidjson::Value::ConstMemberIterator vtplIterator = sdp.FindMember("vtpl");
    if (vtplIterator != sdp.MemberEnd() && vtplIterator->value.IsString())
    {
        mData["vtpl"] = vtplIterator->value.GetString();
    }

    rapidjson::Value::ConstMemberIterator tracksIterator = sdp.FindMember("tracks");
    if (tracksIterator != sdp.MemberEnd() && tracksIterator->value.IsArray())
    {
        for (unsigned int i = 0; i < tracksIterator->value.Capacity(); i++)
        {
            mTracks.push_back(parseTrack(tracksIterator->value[i]));
        }
    }
}

std::string Sdp::unCompress()
{
    std::string sdp;
    sdp.append(mData["cmn"]);

    for (const SdpTrack& track : mTracks)
    {
        if (track.mType == "a")
        {
            sdp.append(unCompressTrack(track, mData["atpl"]));
        }
        else if (track.mType == "v")
        {
            sdp.append(unCompressTrack(track, mData["vtpl"]));
        }
    }

    return sdp;
}

void Sdp::toJson(rapidjson::Document& json) const
{
}

unsigned int Sdp::createTemplate(const std::string& type, const std::vector<std::string> lines, unsigned int position)
{
    std::string temp = lines[position++];
    temp.append(endl);

    unsigned int i = 0;
    for (i = position; i < lines.size(); i++)
    {
        const std::string& line = lines[i];
        char lineType = line[0];
        if (lineType == 'm')
        {
            break;
        }

        if (lineType != 'a')
        {
            temp.append(line).append(endl);
            continue;
        }

        unsigned int bytesRead = 0;
        std::string name = nextWord(line, 2, bytesRead);
        if (name == "recvonly")
        {
            return nextMline(lines, i);
        }

        if (name == "sendrecv" || name == "sendonly" || name == "ssrc-group" || name == "ssrc" || name == "mid" || name == "msid")
        {
            continue;
        }

        temp.append(line).append(endl);
    }

    mData[type] = temp;

    return i;
}

unsigned int Sdp::addTrack(const std::vector<std::string>& lines, unsigned int position)
{
    std::string type = lines[position++].substr(2, 5);
    SdpTrack track;
    if (type == "audio")
    {
        track.mType = "a";
    }
    else if (type == "video")
    {
        track.mType = "v";
    }

    unsigned int i = 0;
    std::set<uint64_t> ssrcsIds;
    for (i = position; i < lines.size(); i++)
    {
        std::string line = lines[i];
        char lineType = line[0];
        if (lineType == 'm')
        {
            break;
        }

        if (lineType != 'a')
        {
            continue;
        }

        unsigned int bytesRead = 0;
        std::string name = nextWord(line, 2, bytesRead);
        if (name == "sendrecv" || name == "recvonly" || name == "sendonly")
        {
            track.mDir = name;
        }
        else if (name == "mid")
        {
            track.mMid = std::stoull(line.substr(6));
        }
        else if (name == "msid")
        {
            std::string subLine = line.substr(7);
            unsigned int pos = subLine.find(" ");
            track.mSid = subLine.substr(0, pos);
            track.mId = subLine.substr(pos + 1, subLine.length());
        }
        else if (name == "ssrc-group")
        {
            track.mSsrcg.push_back(line.substr(13));
        }
        else if (name == "ssrc")
        {
            unsigned int bytesRead = 0;
            std::string ret = nextWord(line, 7, bytesRead);
            uint64_t id = std::stoull(ret);
            if (ssrcsIds.find(id) == ssrcsIds.end())
            {
                ret = nextWord(line, bytesRead + 1, bytesRead);
                ret = nextWord(line, bytesRead + 1, bytesRead);
                track.mSsrcs.push_back(std::pair<uint64_t, std::string>(id, ret));
                ssrcsIds.insert(id);
            }
        }
    }

    mTracks.push_back(track);
    return i;
}

unsigned int Sdp::nextMline(const std::vector<std::string>& lines, unsigned int position)
{
    for (unsigned int i = position; i < lines.size(); i++)
    {
        if (lines[i][0] == 'm')
        {
            return i;
        }
    }

    return position;
}

std::string Sdp::nextWord(const std::string& line, unsigned int start, unsigned int& charRead)
{
    unsigned int i = 0;
    for (i = start; i < line.size(); i++)
    {
        uint8_t ch = static_cast<uint8_t>(line[i]);
        if ((ch >= 97 && ch <= 122) || // a - z
                (ch >= 65 && ch <= 90) ||  // A - Z
                (ch >= 48 && ch <= 57) ||  // 0 - 9
                (ch == 45) || (ch == 43) || (ch == 47) || (ch == 95))
        { // - + /
            continue;
        }

        break;
    }

    charRead = i;
    return line.substr(start, i - start);

}

SdpTrack Sdp::parseTrack(const rapidjson::Value &value) const
{
    SdpTrack track;

    rapidjson::Value::ConstMemberIterator typeIterator = value.FindMember("t");
    if (typeIterator != value.MemberEnd() && typeIterator->value.IsString())
    {
        track.mType = typeIterator->value.GetString();
    }

    rapidjson::Value::ConstMemberIterator midIterator = value.FindMember("mid");
    if (midIterator != value.MemberEnd() && midIterator->value.IsUint64())
    {
        track.mMid = midIterator->value.GetUint64();
    }

    rapidjson::Value::ConstMemberIterator sidIterator = value.FindMember("sid");
    if (sidIterator != value.MemberEnd() && sidIterator->value.IsString())
    {
        track.mSid = sidIterator->value.GetString();
    }

    rapidjson::Value::ConstMemberIterator idIterator = value.FindMember("id");
    if (idIterator != value.MemberEnd() && idIterator->value.IsString())
    {
        track.mId = idIterator->value.GetString();
    }

    rapidjson::Value::ConstMemberIterator dirIterator = value.FindMember("dir");
    if (dirIterator != value.MemberEnd() && dirIterator->value.IsString())
    {
        track.mDir = dirIterator->value.GetString();
    }

    rapidjson::Value::ConstMemberIterator ssrcgIterator = value.FindMember("ssrcg");
    if (ssrcgIterator != value.MemberEnd() && ssrcgIterator->value.IsArray())
    {
        for (unsigned int i = 0; i < ssrcgIterator->value.Size(); i++)
        {
            if (ssrcgIterator->value[i].IsString())
            {
                track.mSsrcg.push_back(ssrcgIterator->value[i].GetString());
            }
        }
    }

    rapidjson::Value::ConstMemberIterator ssrcsIterator = value.FindMember("ssrcs");
    if (ssrcsIterator != value.MemberEnd() && ssrcsIterator->value.IsArray())
    {
        for (unsigned int i = 0; i < ssrcsIterator->value.Size(); i++)
        {
            if (ssrcsIterator->value[i].IsObject())
            {
                rapidjson::Value::ConstMemberIterator ssrcsIdIterator = ssrcsIterator->value[i].FindMember("id");
                if (ssrcsIdIterator != ssrcsIterator->value[i].MemberEnd() && ssrcsIdIterator->value.IsUint64())
                {
                    uint64_t id = ssrcsIdIterator->value.GetUint64();
                    std::string cname;
                    rapidjson::Value::ConstMemberIterator ssrcsCnameIterator = ssrcsIterator->value[i].FindMember("cname");
                    if (ssrcsCnameIterator != ssrcsIterator->value[i].MemberEnd() && ssrcsCnameIterator->value.IsString())
                    {
                        cname = ssrcsCnameIterator->value.GetString();
                    }

                    track.mSsrcs.push_back(std::pair<uint64_t, std::string>(id, cname));
                }
            }
        }
    }

    return  track;
}

std::string Sdp::unCompressTrack(const SdpTrack& track, const std::string &tpl)
{
    std::string sdp = tpl;

    sdp.append("a=mid:").append(std::to_string(track.mMid)).append(endl);
    sdp.append("a=").append(track.mDir).append(endl);
    if (track.mId.size())
    {
        sdp.append("a=msid:").append(track.mSid).append(" ").append(track.mId).append(endl);
    }

    if (track.mSsrcs.size())
    {
        for (const auto& ssrc : track.mSsrcs)
        {
            sdp.append("a=ssrc:").append(std::to_string(ssrc.first)).append(" cname:").append(ssrc.second.length() ? ssrc.second : track.mSid).append(endl);
            sdp.append("a=ssrc:").append(std::to_string(ssrc.first)).append(" msid:").append(track.mSid).append(" ").append(track.mId).append(endl);
        }

        if (track.mSsrcg.size())
        {
            for (const std::string& grp : track.mSsrcg)
            {
                sdp.append("a=ssrc-group:").append(grp).append(endl);
            }
        }
    }

    return sdp;
}

SfuConnection::SfuConnection(const std::string &sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface &call)
    : WebsocketsClient(false)
    , mSfuUrl(sfuUrl)
    , mWebsocketIO(websocketIO)
    , mAppCtx(appCtx)
    , mCall(call)
    , mMainThreadId(std::this_thread::get_id())
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
    mCommands[StatCommand::COMMAND_NAME] = mega::make_unique<StatCommand>(std::bind(&sfu::SfuInterface::handleStatCommand, &call));
    mCommands[PeerJoinCommand::COMMAND_NAME] = mega::make_unique<PeerJoinCommand>(std::bind(&sfu::SfuInterface::handlePeerJoin, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    mCommands[PeerLeftCommand::COMMAND_NAME] = mega::make_unique<PeerLeftCommand>(std::bind(&sfu::SfuInterface::handlePeerLeft, &call, std::placeholders::_1));
    mCommands[ErrorCommand::COMMAND_NAME] = mega::make_unique<ErrorCommand>(std::bind(&sfu::SfuInterface::handleError, &call, std::placeholders::_1, std::placeholders::_2));
    mCommands[ModeratorCommand::COMMAND_NAME] = mega::make_unique<ModeratorCommand>(std::bind(&sfu::SfuInterface::handleModerator, &call, std::placeholders::_1, std::placeholders::_2));
}

SfuConnection::~SfuConnection()
{
    if (mConnState != kDisconnected)
    {
        disconnect();
    }
}

bool SfuConnection::isOnline() const
{
    return (mConnState >= kConnected);
}

bool SfuConnection::isDisconnected() const
{
    return (mConnState <= kDisconnected);
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

    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;

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
            usingipv6 = false;
        }
        else if (oldTargetIp == ipv4 && ipv6.size())
        {
            mTargetIp = ipv6;
            usingipv6 = true;
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

bool SfuConnection::sendCommand(const std::string &command)
{
    if (!isOnline())
        return false;

    // if several data are written to the output buffer to be sent all together, wait for all of them
    if (mSendPromise.done())
    {
        mSendPromise = promise::Promise<void>();
        auto wptr = weakHandle();
        mSendPromise.fail([wptr](const promise::Error& err)
        {
            if (wptr.deleted())
                return;

           SFU_LOG_WARNING("Failed to send data. Error: %s", err.what());
        });
    }

    addNewCommand(command);
    return true;
}

void SfuConnection::addNewCommand(const std::string &command)
{
    checkThreadId();                // Check that commandsQueue is always accessed from a single thread
    mCommandsQueue.push(command);   // push command in the queue
    processNextCommand();
}

void SfuConnection::processNextCommand(bool resetSending)
{
    checkThreadId(); // Check that commandsQueue is always accessed from a single thread

    if (resetSending)
    {
        // upon wsSendMsgCb we need to reset isSending flag
        mCommandsQueue.setSending(false);
    }

    if (mCommandsQueue.isEmpty() || mCommandsQueue.sending())
    {
        return;
    }

    mCommandsQueue.setSending(true);
    std::string command = mCommandsQueue.pop();
    assert(!command.empty());
    SFU_LOG_DEBUG("Send command: %s", command.c_str());
    std::unique_ptr<char[]> buffer(mega::MegaApi::strdup(command.c_str()));
    bool rc = wsSendMessage(buffer.get(), command.length());

    if (!rc)
    {
        mSendPromise.reject("Socket is not ready");
        processNextCommand(true);
    }
}

void SfuConnection::clearCommandsQueue()
{
    checkThreadId(); // Check that commandsQueue is always accessed from a single thread
    mCommandsQueue.clear();
    mCommandsQueue.setSending(false);
}

void SfuConnection::checkThreadId()
{
    if (mMainThreadId != std::this_thread::get_id())
    {
        SFU_LOG_ERROR("Current thread id doesn't match with expected");
        assert(false);
    }
}

bool SfuConnection::handleIncomingData(const char* data, size_t len)
{
    SFU_LOG_DEBUG("Data received: %s", data);
    rapidjson::StringStream stringStream(data);
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
        SFU_LOG_ERROR("Received data doesn't have 'a' field");
        return false;
    }

    std::string command = jsonIterator->value.GetString();
    auto commandIterator = mCommands.find(command);
    if (commandIterator == mCommands.end())
    {
        SFU_LOG_ERROR("Command is not defined yet");
        return false;
    }

    SFU_LOG_DEBUG("Received Command: %s, Bytes: %d", command.c_str(), len);
    bool processCommandResult = mCommands[command]->processCommand(document);
    if (processCommandResult && command == AnswerCommand::COMMAND_NAME)
    {
        setConnState(SfuConnection::kJoined);
    }

    return processCommandResult;
}

promise::Promise<void> SfuConnection::getPromiseConnection()
{
    return mConnectPromise;
}

bool SfuConnection::joinSfu(const Sdp &sdp, const std::map<std::string, std::string> &ivs, int avFlags, int speaker, int vthumbs)
{
    rapidjson::Document json(rapidjson::kObjectType);

    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_JOIN.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value sdpValue(rapidjson::kObjectType);
    for (const auto& data : sdp.mData)
    {
        rapidjson::Value dataValue(rapidjson::kStringType);
        dataValue.SetString(data.second.c_str(), data.second.length());
        sdpValue.AddMember(rapidjson::Value(data.first.c_str(), data.first.length()), dataValue, json.GetAllocator());
    }

    rapidjson::Value tracksValue(rapidjson::kArrayType);
    for(const SdpTrack& track : sdp.mTracks)
    {
        if (track.mType != "a" && track.mType != "v")
        {
            continue;
        }

        rapidjson::Value dataValue(rapidjson::kObjectType);
        dataValue.AddMember("t", rapidjson::Value(track.mType.c_str(), track.mType.length()), json.GetAllocator());
        dataValue.AddMember("mid", rapidjson::Value(track.mMid), json.GetAllocator());
        dataValue.AddMember("dir", rapidjson::Value(track.mDir.c_str(), track.mDir.length()), json.GetAllocator());
        if (track.mSid.length())
        {
            dataValue.AddMember("sid", rapidjson::Value(track.mSid.c_str(), track.mSid.length()), json.GetAllocator());
        }

        if (track.mId.length())
        {
            dataValue.AddMember("id", rapidjson::Value(track.mId.c_str(), track.mId.length()), json.GetAllocator());
        }

        if (track.mSsrcg.size())
        {
            rapidjson::Value ssrcgValue(rapidjson::kArrayType);
            for (const auto& element : track.mSsrcg)
            {
                ssrcgValue.PushBack(rapidjson::Value(element.c_str(), element.length()), json.GetAllocator());
            }

            dataValue.AddMember("ssrcg", ssrcgValue, json.GetAllocator());
        }

        if (track.mSsrcs.size())
        {
            rapidjson::Value ssrcsValue(rapidjson::kArrayType);
            for (const auto& element : track.mSsrcs)
            {
                rapidjson::Value elementValue(rapidjson::kObjectType);
                elementValue.AddMember("id", rapidjson::Value(element.first), json.GetAllocator());
                elementValue.AddMember("cname", rapidjson::Value(element.second.c_str(), element.second.size()), json.GetAllocator());

                ssrcsValue.PushBack(elementValue, json.GetAllocator());
            }

            dataValue.AddMember("ssrcs", ssrcsValue, json.GetAllocator());
        }

        tracksValue.PushBack(dataValue, json.GetAllocator());
    }

    sdpValue.AddMember("tracks", tracksValue, json.GetAllocator());

    json.AddMember("sdp", sdpValue, json.GetAllocator());

    rapidjson::Value ivsValue(rapidjson::kObjectType);
    for (const auto& iv : ivs)
    {
        ivsValue.AddMember(rapidjson::Value(iv.first.c_str(), iv.first.size()), rapidjson::Value(iv.second.c_str(), iv.second.size()), json.GetAllocator());
    }

    json.AddMember("ivs", ivsValue, json.GetAllocator());
    json.AddMember("av", avFlags, json.GetAllocator());

    if (speaker)
    {
        rapidjson::Value speakerValue(rapidjson::kNumberType);
        speakerValue.SetInt(speaker);
        json.AddMember("spk", speakerValue, json.GetAllocator());
    }

    if (vthumbs > 0)
    {
        rapidjson::Value vThumbsValue(rapidjson::kNumberType);
        vThumbsValue.SetInt(vthumbs);
        json.AddMember("vthumbs", vThumbsValue, json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());

    setConnState(SfuConnection::kJoining);

    return sendCommand(command);
}

bool SfuConnection::sendKey(Keyid_t id, const std::map<Cid_t, std::string>& keys)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SENDKEY.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value idValue(rapidjson::kNumberType);
    idValue.SetUint(id);
    json.AddMember(rapidjson::Value("id"), idValue, json.GetAllocator());

    rapidjson::Value dataValue(rapidjson::kArrayType);
    for (const auto& key : keys)
    {
        rapidjson::Value keyValue(rapidjson::kArrayType);
        keyValue.PushBack(rapidjson::Value(key.first), json.GetAllocator());
        keyValue.PushBack(rapidjson::Value(key.second.c_str(), key.second.length()), json.GetAllocator());

        dataValue.PushBack(keyValue, json.GetAllocator());
    }

    json.AddMember(rapidjson::Value("data"), dataValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendAv(unsigned av)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_AV.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value avValue(rapidjson::kNumberType);
    avValue.SetUint(av);
    json.AddMember(rapidjson::Value("av"), avValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendGetVtumbs(const std::vector<Cid_t> &cids)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_GET_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value cidsValue(rapidjson::kArrayType);
    for(Cid_t cid : cids)
    {
        cidsValue.PushBack(rapidjson::Value(cid), json.GetAllocator());
    }

    json.AddMember("cids", cidsValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendDelVthumbs(const std::vector<Cid_t> &cids)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_DEL_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value cidsValue(rapidjson::kArrayType);
    for(Cid_t cid : cids)
    {
        cidsValue.PushBack(rapidjson::Value(cid), json.GetAllocator());
    }

    json.AddMember("cids", cidsValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendGetHiRes(Cid_t cid, int r, int lo)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_GET_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
    json.AddMember("r", rapidjson::Value(r), json.GetAllocator());
    json.AddMember("lo", rapidjson::Value(lo), json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendDelHiRes(const std::vector<Cid_t> &cids)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_DEL_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    rapidjson::Value cidsValue(rapidjson::kArrayType);
    for(Cid_t cid : cids)
    {
        cidsValue.PushBack(rapidjson::Value(cid), json.GetAllocator());
    }
    json.AddMember("cids", cidsValue, json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendHiResSetLo(Cid_t cid, int lo)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_HIRES_SET_LO.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
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

bool SfuConnection::sendSpeakReq(Cid_t cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_RQ.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid)
    {
        json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakReqDel(Cid_t cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_RQ_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid)
    {
        json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakDel(Cid_t cid)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(CSFU_SPEAK_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), Command::COMMAND_IDENTIFIER.length()), cmdValue, json.GetAllocator());

    if (cid)
    {
        json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
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
    if (!mSendPromise.done())
    {
        mSendPromise.resolve();
    }
}

void SfuConnection::wsProcessNextMsgCb()
{
    processNextCommand(true);
}

void SfuConnection::onSocketClose(int errcode, int errtype, const std::string &reason)
{
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

        if (mSfuUrl.empty())
            return ::promise::Error("SFU reconnect: Current URL is not valid");

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

                if (mIpsv4 == ipsv4 && mIpsv6 == ipsv6)
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
                mCall.handleSfuConnected();
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

SfuClient::SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings, const karere::Id& myHandle)
    : mRtcCryptoMeetings(std::make_shared<rtcModule::RtcCryptoMeetings>(*rRtcCryptoMeetings))
    , mWebsocketIO(websocketIO)
    , mMyHandle(myHandle)
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

const karere::Id& SfuClient::myHandle()
{
    return mMyHandle;
}

void SfuClient::reconnectAllToSFU(bool disconnect)
{
    for (auto it = mConnections.begin(); it != mConnections.end(); it++)
    {
        it->second->retryPendingConnection(disconnect);
    }
}

PeerLeftCommand::PeerLeftCommand(const PeerLeftCommandFunction &complete)
    : mComplete(complete)
{
}

bool PeerLeftCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        return false;
    }

    ::mega::MegaHandle cid = (cidIterator->value.GetUint64());
    return mComplete(cid);
}

ModeratorCommand::ModeratorCommand(const ModeratorCommandFunction &complete)
    : mComplete(complete)
{
}

bool ModeratorCommand::processCommand(const rapidjson::Document &command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    Cid_t cid = 0;
    if (cidIterator != command.MemberEnd() || cidIterator->value.IsUint())
    {
        cid = (cidIterator->value.GetUint64());
    }

    rapidjson::Value::ConstMemberIterator modeIterator = command.FindMember("mod");
    if (modeIterator == command.MemberEnd() || !modeIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'mod' field");
        return false;
    }

    bool moderator = modeIterator->value.GetUint();

    return mComplete(cid, moderator);
}

}
