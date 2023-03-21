#ifndef KARERE_DISABLE_WEBRTC
#include <sfu.h>
#include <base/promise.h>
#include <megaapi.h>
#include <mega/base64.h>

#include<rapidjson/writer.h>

namespace sfu
{

// notifications SFU -> client
std::string Command::COMMAND_IDENTIFIER     = "a";
std::string Command::ERROR_IDENTIFIER       = "err";
std::string Command::WARN_IDENTIFIER        = "warn";
std::string Command::ERROR_MESSAGE          = "msg";

const std::string AVCommand::COMMAND_NAME             = "AV";
const std::string AnswerCommand::COMMAND_NAME         = "ANSWER";
const std::string KeyCommand::COMMAND_NAME            = "KEY";
const std::string VthumbsCommand::COMMAND_NAME        = "VTHUMBS";
const std::string VthumbsStartCommand::COMMAND_NAME   = "VTHUMB_START";
const std::string VthumbsStopCommand::COMMAND_NAME    = "VTHUMB_STOP";
const std::string HiResCommand::COMMAND_NAME          = "HIRES";
const std::string HiResStartCommand::COMMAND_NAME     = "HIRES_START";
const std::string HiResStopCommand::COMMAND_NAME      = "HIRES_STOP";
const std::string SpeakReqsCommand::COMMAND_NAME      = "SPEAK_REQS";
const std::string SpeakReqDelCommand::COMMAND_NAME    = "SPEAK_RQ_DEL";
const std::string SpeakOnCommand::COMMAND_NAME        = "SPEAK_ON";
const std::string SpeakOffCommand::COMMAND_NAME       = "SPEAK_OFF";
const std::string PeerJoinCommand::COMMAND_NAME       = "PEERJOIN";
const std::string PeerLeftCommand::COMMAND_NAME       = "PEERLEFT";
const std::string ByeCommand::COMMAND_NAME            = "BYE";
const std::string ModAddCommand::COMMAND_NAME         = "MOD_ADD";
const std::string ModDelCommand::COMMAND_NAME         = "MOD_DEL";
const std::string HelloCommand::COMMAND_NAME          = "HELLO";
const std::string WrDumpCommand::COMMAND_NAME         = "WR_DUMP";
const std::string WrEnterCommand::COMMAND_NAME        = "WR_ENTER";
const std::string WrLeaveCommand::COMMAND_NAME        = "WR_LEAVE";
const std::string WrAllowCommand::COMMAND_NAME        = "WR_ALLOW";
const std::string WrDenyCommand::COMMAND_NAME         = "WR_DENY";
const std::string WrAllowReqCommand::COMMAND_NAME     = "WR_ALLOW_REQ";
const std::string WrUsersAllowCommand::COMMAND_NAME   = "WR_USERS_ALLOW";
const std::string WrUsersDenyCommand::COMMAND_NAME    = "WR_USERS_DENY";

const std::string Sdp::endl = "\r\n";

// commands client -> SFU
const std::string SfuConnection::CSFU_JOIN         = "JOIN";
const std::string SfuConnection::CSFU_SENDKEY      = "KEY";
const std::string SfuConnection::CSFU_AV           = "AV";
const std::string SfuConnection::CSFU_GET_VTHUMBS  = "GET_VTHUMBS";
const std::string SfuConnection::CSFU_DEL_VTHUMBS  = "DEL_VTHUMBS";
const std::string SfuConnection::CSFU_GET_HIRES    = "GET_HIRES";
const std::string SfuConnection::CSFU_DEL_HIRES    = "DEL_HIRES";
const std::string SfuConnection::CSFU_HIRES_SET_LO = "HIRES_SET_LO";
const std::string SfuConnection::CSFU_LAYER        = "LAYER";
const std::string SfuConnection::CSFU_SPEAK_RQ     = "SPEAK_RQ";
const std::string SfuConnection::CSFU_SPEAK_RQ_DEL = "SPEAK_RQ_DEL";
const std::string SfuConnection::CSFU_SPEAK_DEL    = "SPEAKER_DEL";
const std::string SfuConnection::CSFU_BYE          = "BYE";
const std::string SfuConnection::CSFU_WR_PUSH      = "WR_PUSH";
const std::string SfuConnection::CSFU_WR_ALLOW     = "WR_ALLOW";
const std::string SfuConnection::CSFU_WR_KICK      = "WR_KICK";


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

std::string CommandsQueue::pop()
{
    if (empty())
    {
        return std::string();
    }

    std::string command = std::move(front());
    pop_front();
    return command;
}

Peer::Peer(const karere::Id peerid, const unsigned avFlags, const std::vector<std::string>* ivs, const Cid_t cid, const bool isModerator)
    : mCid(cid),
      mPeerid(peerid),
      mAvFlags(static_cast<uint8_t>(avFlags)),
      mIvs(ivs ? *ivs : std::vector<std::string>()),
      mIsModerator(isModerator)
{
}

Peer::Peer(const Peer &peer)
    : mCid(peer.mCid)
    , mPeerid(peer.mPeerid)
    , mAvFlags(peer.mAvFlags)
    , mEphemeralKeyPair(peer.mEphemeralKeyPair ? peer.mEphemeralKeyPair->copy() : nullptr)
    , mIvs(peer.mIvs)
    , mKeyEncryptIv(peer.mKeyEncryptIv)
    , mKeyDecryptIv(peer.mKeyDecryptIv)
    , mIsModerator(peer.mIsModerator)
{
}

void Peer::setCid(Cid_t cid)
{
    mCid = cid;
}

Cid_t Peer::getCid() const
{
    return mCid;
}

karere::Id Peer::getPeerid() const
{
    return mPeerid;
}

bool Peer::hasAnyKey() const
{
    return !mKeyMap.empty();
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
    mCurrentkeyId = keyid;
    mKeyMap[mCurrentkeyId] = key;
}

void Peer::resetKeys()
{
    mCurrentkeyId = 0;
    mKeyMap.clear();
}

void Peer::setEphemeralKeyPair(const rtcModule::X25519KeyPair* keypair)
{
    if (!keypair)
    {
        mEphemeralKeyPair.reset();
        return;
    }

    mEphemeralKeyPair.reset(new rtcModule::X25519KeyPair(*keypair));
}

const rtcModule::X25519KeyPair* Peer::getEphemeralKeyPair() const
{
    return mEphemeralKeyPair.get();
}

const std::vector<std::string>& Peer::getIvs() const
{
    return mIvs;
}

void Peer::setIvs(const std::vector<std::string>& ivs)
{
    mIvs = ivs;
}

bool Peer::makeKeyEncryptIv(const std::string& vthumbIv, const std::string& hiresIv)
{
    mKeyEncryptIv.clear();
    uint8_t firstPartLen  = static_cast<uint8_t>(mediaKeyIv::lenIvFirstPart);
    uint8_t secondPartLen = static_cast<uint8_t>(mediaKeyIv::lenIvSecondPart);
    if (vthumbIv.size() == firstPartLen && hiresIv.size() == firstPartLen)
    {
        SFU_LOG_WARNING("makeKeyEncryptIv: ill-formed IV's");
        assert(false);
        return false;
    }

    // First 8 bytes are taken from the vthumb track IV
    std::vector<byte> first = sfu::Command::hexToByteArray(vthumbIv);
    std::copy(first.begin(), first.begin() + firstPartLen, std::back_inserter(mKeyEncryptIv));

    // The rest 4 bytes are the first from the hi-res video track IV
    std::vector<byte> second = sfu::Command::hexToByteArray(hiresIv);
    std::copy(second.begin(), second.begin() + secondPartLen, std::back_inserter(mKeyEncryptIv));
    return (mKeyEncryptIv.size() == rtcModule::KEY_ENCRYPT_IV_LENGTH);
}

bool Peer::makeKeyDecryptIv(const std::string& vthumbIv, const std::string& hiresIv)
{
    mKeyDecryptIv.clear();
    uint8_t firstPartLen  = static_cast<uint8_t>(mediaKeyIv::lenIvFirstPart);
    uint8_t secondPartLen = static_cast<uint8_t>(mediaKeyIv::lenIvSecondPart);
    if (vthumbIv.size() == firstPartLen && hiresIv.size() == firstPartLen)
    {
        SFU_LOG_WARNING("makeKeyDecryptIv: ill-formed IV's");
        assert(false);
        return false;
    }

    // First 8 bytes are taken from the vthumb track IV
    std::vector<byte> first = sfu::Command::hexToByteArray(vthumbIv);
    std::copy(first.begin(), first.begin() + firstPartLen, std::back_inserter(mKeyDecryptIv));

    // The rest 4 bytes are the first from the hi-res video track IV
    std::vector<byte> second = sfu::Command::hexToByteArray(hiresIv);
    std::copy(second.begin(), second.begin() + secondPartLen, std::back_inserter(mKeyDecryptIv));
    return (mKeyDecryptIv.size() == rtcModule::KEY_ENCRYPT_IV_LENGTH);
}

const std::vector<byte>& Peer::getKeyEncryptIv() const
{
    return mKeyEncryptIv;
}

const std::vector<byte>& Peer::getKeyDecryptIv() const
{
    return mKeyDecryptIv;
}

void Peer::setAvFlags(karere::AvFlags flags)
{
    mAvFlags = flags;
}

bool Peer::isModerator() const
{
    return mIsModerator;
}

void Peer::setModerator(bool isModerator)
{
    mIsModerator = isModerator;
}

Command::~Command()
{
}

Command::Command(SfuInterface& call)
    : mCall(call)
{
}

bool Command::parseUsersMap(std::map<karere::Id, bool>& wrUsers, const rapidjson::Value& obj) const
{
    assert(obj.IsObject());
    rapidjson::Value::ConstMemberIterator usersIterator = obj.FindMember("users");
    if (usersIterator != obj.MemberEnd())
    {
        const rapidjson::Value& objUsers = usersIterator->value;
        if (!objUsers.IsObject())
        {
            SFU_LOG_ERROR("parseUsersMap: 'users' is not an object");
            return false;
        }

        for (rapidjson::Value::ConstMemberIterator m = objUsers.MemberBegin(); m != objUsers.MemberEnd(); m++)
        {
            if (!m->name.IsString() || !m->value.IsUint())
            {
                SFU_LOG_ERROR("parseUsersMap: 'users' ill-formed");
                return false;
            }

            std::string userIdString = m->name.GetString();
            uint64_t userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
            bool allow = m->value.GetUint();
            wrUsers[userId] = allow;
        }
    }
    return true;
}

void Command::parseUsersArray(std::set<karere::Id> &moderators, rapidjson::Value::ConstMemberIterator &it) const
{
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        if (!it->value[j].IsString())
        {
            SFU_LOG_ERROR("parse users array: invalid user handle value");
            return;
        }
        std::string userIdString = it->value[j].GetString();
        moderators.emplace(::mega::MegaApi::base64ToUserHandle(userIdString.c_str()));
    }
}

void Command::parseTracks(const rapidjson::Document& command, const std::string& arrayName, std::map<Cid_t, TrackDescriptor>& tracks) const
{
    // expected track array format: [[3 (cid), 2(mid), 1(reuse)]...]
    rapidjson::Value::ConstMemberIterator it = command.FindMember(arrayName.c_str());
    if (it != command.MemberEnd())
    {
        if (!it->value.IsArray())
        {
            SFU_LOG_ERROR("Received track ill-formed");
            assert(false);
            return;
        }

        for (unsigned int i = 0; i < it->value.Capacity(); ++i)
        {
            if (!it->value[i].IsArray())
            {
                SFU_LOG_ERROR("Received track ill-formed");
                assert(false);
                return;
            }

            Cid_t cid = 0;
            TrackDescriptor td;
            const auto& arr = it->value[i].GetArray();
            for (unsigned int j = 0; j < arr.Capacity(); ++j)
            {
                if (j==0) { cid = arr[0].GetUint(); }
                if (j==1) { td.mMid = arr[1].GetUint(); }
                if (j==2) { td.mReuse = arr[2].GetUint(); }
            }
            tracks[cid] = td; // add entry to map <cid, trackDescriptor>
        }
    }
}

uint64_t Command::hexToBinary(const std::string &hex)
{
    uint64_t value = 0;
    unsigned int bufferSize = static_cast<unsigned int>(hex.length()) >> 1;
    assert(bufferSize <= 8);
    std::unique_ptr<uint8_t []> buffer = std::unique_ptr<uint8_t []>(new uint8_t[bufferSize]);
    unsigned int binPos = 0;
    for (unsigned int i = 0; i< hex.length(); binPos++)
    {
        // compiler doesn't guarantees the order "++" operation performed in relation to the second access of variable i (better to split in two operations)
        buffer[binPos] = static_cast<uint8_t>((hexDigitVal(hex[i++])) << 4);
        buffer[binPos] |= static_cast<uint8_t>(hexDigitVal(hex[i++]));
    }

    memcpy(&value, buffer.get(), bufferSize);

    return value;
}

std::vector<byte> Command::hexToByteArray(const std::string &hex)
{
    unsigned int bufferSize = static_cast<unsigned int>(hex.length()) >> 1;
    std::vector<byte> res(bufferSize);
    unsigned int binPos = 0;
    for (unsigned int i = 0; i< hex.length(); binPos++)
    {
        // compiler doesn't guarantees the order "++" operation performed in relation to the second access of variable i (better to split in two operations)
        res[binPos] = static_cast<uint8_t>((hexDigitVal(hex[i++])) << 4);
        res[binPos] |= static_cast<uint8_t>(hexDigitVal(hex[i++]));
    }

    return res;
}

uint8_t Command::hexDigitVal(char value)
{
    if (value <= 57)
    { // ascii code if '9'
        return static_cast<uint8_t>(value - 48); // ascii code of '0'
    }
    else if (value >= 97)
    { // 'a'
        return static_cast<uint8_t>(10 + value - 97);
    }
    else
    {
        return static_cast<uint8_t>(10 + value - 65); // 'A'
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

AVCommand::AVCommand(const AvCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

AnswerCommand::AnswerCommand(const AnswerCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

    rapidjson::Value::ConstMemberIterator tsIterator = command.FindMember("t"); // time elapsed since the start of the call
    if (tsIterator == command.MemberEnd() || !tsIterator->value.IsUint64())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 't' field");
        return false;
    }

    // call start ts (ms)
    uint64_t callDuration = tsIterator->value.GetUint64();

    // parse moderators list
    std::set<karere::Id> moderators;
    rapidjson::Value::ConstMemberIterator modsIterator = command.FindMember("mods");
    if (modsIterator != command.MemberEnd() && modsIterator->value.IsArray())
    {
        parseUsersArray(moderators, modsIterator);
    }

    // parse own moderator permission
    bool ownModerator = false;
    rapidjson::Value::ConstMemberIterator modIterator = command.FindMember("mod");
    if (modIterator != command.MemberEnd() && modIterator->value.IsUint())
    {
        ownModerator = modIterator->value.GetUint();
    }

    std::vector<Peer> peers;
    std::map<Cid_t, std::string> keystrmap;
    rapidjson::Value::ConstMemberIterator peersIterator = command.FindMember("peers");
    if (peersIterator != command.MemberEnd() && peersIterator->value.IsArray())
    {
        parsePeerObject(peers, keystrmap, moderators, peersIterator);
    }

    std::map<Cid_t, TrackDescriptor> speakers;
    parseTracks(command, "speakers", speakers);

    std::map<Cid_t, TrackDescriptor> vthumbs;
    parseTracks(command, "vthumbs", vthumbs);

    return mComplete(cid, sdp, callDuration, peers, keystrmap, vthumbs, speakers, moderators, ownModerator);
}

void AnswerCommand::parsePeerObject(std::vector<Peer> &peers, std::map<Cid_t, std::string>& keystrmap, const std::set<karere::Id>& moderators, rapidjson::Value::ConstMemberIterator &it) const
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

            rapidjson::Value::ConstMemberIterator pubkeyIterator = it->value[j].FindMember("pubk");
            if (pubkeyIterator != it->value[j].MemberEnd() && pubkeyIterator->value.IsString())
            {
                 // clients with SFU protocol < 1 won't send ephemeral pubkey
                 keystrmap.emplace(cid, pubkeyIterator->value.GetString());
            }

            std::vector<std::string> ivs;
            rapidjson::Value::ConstMemberIterator ivsIterator = it->value[j].FindMember("ivs");
            if (ivsIterator != it->value[j].MemberEnd() && ivsIterator->value.IsArray())
            {
                for (unsigned int i = 0; i < ivsIterator->value.Capacity(); i++)
                {
                    if (!ivsIterator->value[i].IsString())
                    {
                        SFU_LOG_ERROR("parse invalid ivs");
                        ivs.clear();
                        break;
                    }
                    ivs.emplace_back(ivsIterator->value[i].GetString());
                }
            }

            rapidjson::Value::ConstMemberIterator avIterator = it->value[j].FindMember("av");
            if (avIterator == it->value[j].MemberEnd() || !avIterator->value.IsUint())
            {
                 SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid 'av' value");
                 return;
            }

            bool isModerator = moderators.find(userId) != moderators.end();
            unsigned av = avIterator->value.GetUint();
            Peer peer(userId, av, &ivs, cid, isModerator);
            if (!peer.makeKeyDecryptIv(ivs[kVthumbTrack], ivs[kHiResTrack]))
            {
                SFU_LOG_ERROR("Error generating DecryptIv");
                assert(false);
                return;
            }
            peers.push_back(std::move(peer));
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::parsePeerObject: invalid value at array 'peers'");
            return;
        }
    }
}

KeyCommand::KeyCommand(const KeyCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

VthumbsCommand::VthumbsCommand(const VtumbsCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool VthumbsCommand::processCommand(const rapidjson::Document &command)
{
    std::map<Cid_t, TrackDescriptor> tracks;
    parseTracks(command, "tracks", tracks);
    return mComplete(tracks);
}

VthumbsStartCommand::VthumbsStartCommand(const VtumbsStartCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool VthumbsStartCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

VthumbsStopCommand::VthumbsStopCommand(const VtumbsStopCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool VthumbsStopCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

HiResCommand::HiResCommand(const HiresCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool HiResCommand::processCommand(const rapidjson::Document &command)
{
    std::map<Cid_t, TrackDescriptor> tracks;
    parseTracks(command, "tracks", tracks);
    return mComplete(tracks);
}

HiResStartCommand::HiResStartCommand(const HiResStartCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool HiResStartCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

HiResStopCommand::HiResStopCommand(const HiResStopCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool HiResStopCommand::processCommand(const rapidjson::Document &command)
{
    return mComplete();
}

SpeakReqsCommand::SpeakReqsCommand(const SpeakReqsCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

SpeakReqDelCommand::SpeakReqDelCommand(const SpeakReqDelCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

SpeakOnCommand::SpeakOnCommand(const SpeakOnCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

        std::map<Cid_t, TrackDescriptor> speakers;
        parseTracks(command, "speakers", speakers);
        if (speakers.size() != 1)
        {
            SFU_LOG_ERROR("SpeakOnCommand::processCommand: Received data contains unexpected number of audio tracks");
            return false;
        }
        else
        {
            return mComplete(speakers.begin()->first, speakers.begin()->second);
        }
    }
    else
    {
        return mComplete(cid, sfu::TrackDescriptor());
    }
}

SpeakOffCommand::SpeakOffCommand(const SpeakOffCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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


PeerJoinCommand::PeerJoinCommand(const PeerJoinCommandFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

    std::string pubkeyStr;
    rapidjson::Value::ConstMemberIterator pubkeyIterator = command.FindMember("pubk");
    if (pubkeyIterator != command.MemberEnd() && pubkeyIterator->value.IsString())
    {
         // clients with SFU protocol < 1 won't send ephemeral pubkey
         pubkeyStr = pubkeyIterator->value.GetString();
    }

    std::vector<std::string> ivs;
    rapidjson::Value::ConstMemberIterator ivsIterator = command.FindMember("ivs");
    if (ivsIterator != command.MemberEnd() && ivsIterator->value.IsArray())
    {
        for (unsigned int i = 0; i < ivsIterator->value.Capacity(); i++)
        {
            if (!ivsIterator->value[i].IsString())
            {
                SFU_LOG_ERROR("parse invalid ivs");
                ivs.clear();
                break;
            }
            ivs.emplace_back(ivsIterator->value[i].GetString());
        }
    }

    int av = static_cast<int>(avIterator->value.GetUint());
    return mComplete(cid, userid, av, pubkeyStr, ivs);
}

Sdp::Sdp(const std::string &sdp, int64_t mungedTrackIndex)
{
    size_t pos = 0;
    std::string buffer = sdp;
    std::vector<std::string> lines;
    while ((pos = buffer.find(Sdp::endl)) != std::string::npos)
    {
        std::string line = buffer.substr(0, pos);
        lines.push_back(line);
        buffer.erase(0, pos + Sdp::endl.size());
    }

    for (const std::string& line : lines)
    {
        if (line.size() > 2 && line[0] == 'm' && line[1] == '=')
        {
            // "cmn" precedes any "m=" line in the session-description provided by WebRTC
            assert(mData.find("cmn") != mData.end());
            break;
        }

        mData["cmn"].append(line).append(Sdp::endl);
    }

    unsigned int i = 0;
    while (i < lines.size())
    {
        const std::string& line = lines.at(i);
        std::string type = line.substr(2, 5);
        if (type == "audio" && mData.find("atpl") == mData.end())
        {
            i = createTemplate("atpl", lines, i);   // can consume more than one line -> update `i`
            if (mData.find("vtpl") != mData.end())
            {
                // if "vtpl" is already added to data, we are done
                break;
            }
        }
        else if (type == "video" && mData.find("vtpl") == mData.end())
        {
            i = createTemplate("vtpl", lines, i);
            if (mData.find("atpl") != mData.end())  // TODO: why do we break here?
            {
                // if "atpl" is already added to data, we are done
                break;
            }
        }
        else
        {
            // find next line starting with "m"
            i = nextMline(lines, i + 1);
        }
    }

    for (i = nextMline(lines, 0); i < lines.size();)
    {
        i = addTrack(lines, i);
    }

    if (mungedTrackIndex != -1) // track requires to be munged
    {
        assert(mTracks.size() > static_cast<size_t>(mungedTrackIndex));
        // modify SDP (hack to enable SVC) for hi-res track to enable SVC multicast
        mungeSdpForSvc(mTracks.at(static_cast<size_t>(mungedTrackIndex)));
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
        // TODO: check whether we should use Size() instead of Capacity() (also check other usages of Capacity())
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

    for (const Sdp::Track& track : mTracks)
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

unsigned int Sdp::createTemplate(const std::string& type, const std::vector<std::string> lines, unsigned int position)
{
    std::string temp = lines[position++];
    temp.append(Sdp::endl);

    unsigned int i = position;
    for (; i < lines.size(); i++)
    {
        const std::string& line = lines[i];
        char lineType = line[0];
        if (lineType == 'm')
        {
            break;
        }

        if (lineType != 'a')
        {
            temp.append(line).append(Sdp::endl);
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

        temp.append(line).append(Sdp::endl);
    }

    mData[type] = temp;

    return i;
}

void Sdp::mungeSdpForSvc(Sdp::Track &track)
{
    std::pair<uint64_t, std::string> vidSsrc1 = track.mSsrcs.at(0);
    std::pair<uint64_t, std::string> fidSsrc1 = track.mSsrcs.at(1);
    uint64_t id = vidSsrc1.first;

    std::pair<uint64_t, std::string> vidSsrc2 = std::pair<uint64_t, std::string>(++id, vidSsrc1.second);
    std::pair<uint64_t, std::string> vidSsrc3 = std::pair<uint64_t, std::string>(++id, vidSsrc1.second);
    id = fidSsrc1.first;

    std::pair<uint64_t, std::string> fidSsrc2 = std::pair<uint64_t, std::string>(++id, fidSsrc1.second);
    std::pair<uint64_t, std::string> fidSsrc3 = std::pair<uint64_t, std::string>(++id, fidSsrc1.second);

    track.mSsrcs.clear();
    track.mSsrcs.emplace_back(vidSsrc1);
    track.mSsrcs.emplace_back(fidSsrc1);
    track.mSsrcs.emplace_back(vidSsrc2);
    track.mSsrcs.emplace_back(vidSsrc3);
    track.mSsrcs.emplace_back(fidSsrc2);
    track.mSsrcs.emplace_back(fidSsrc3);

    std::string Ssrcg1 = "SIM ";
    Ssrcg1.append(std::to_string(vidSsrc1.first))
            .append(" ")
            .append(std::to_string(vidSsrc2.first))
            .append(" ")
            .append(std::to_string(vidSsrc3.first));

    std::string Ssrcg3 = "FID ";
    Ssrcg3.append(std::to_string(vidSsrc2.first))
            .append(" ")
            .append(std::to_string(fidSsrc2.first));

    std::string Ssrcg2 = track.mSsrcg[0];

    std::string Ssrcg4 = "FID ";
    Ssrcg4.append(std::to_string(vidSsrc3.first))
            .append(" ")
            .append(std::to_string(fidSsrc3.first));

    track.mSsrcg.clear();
    track.mSsrcg.emplace_back(Ssrcg1);
    track.mSsrcg.emplace_back(Ssrcg2);
    track.mSsrcg.emplace_back(Ssrcg3);
    track.mSsrcg.emplace_back(Ssrcg4);
}

unsigned int Sdp::addTrack(const std::vector<std::string>& lines, unsigned int position)
{
    std::string type = lines[position++].substr(2, 5);
    Sdp::Track track;
    if (type == "audio")
    {
        track.mType = "a";
    }
    else if (type == "video")
    {
        track.mType = "v";
    }

    unsigned int i = position;
    std::set<uint64_t> ssrcsIds;
    for (; i < lines.size(); i++)
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
            unsigned int pos = static_cast<unsigned int>(subLine.find(" "));
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

Sdp::Track Sdp::parseTrack(const rapidjson::Value &value) const
{
    Sdp::Track track;

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

std::string Sdp::unCompressTrack(const Sdp::Track& track, const std::string &tpl)
{
    std::string sdp = tpl;

    sdp.append("a=mid:").append(std::to_string(track.mMid)).append(Sdp::endl);
    sdp.append("a=").append(track.mDir).append(Sdp::endl);
    if (track.mId.size())
    {
        sdp.append("a=msid:").append(track.mSid).append(" ").append(track.mId).append(Sdp::endl);
    }

    if (track.mSsrcs.size())
    {
        for (const auto& ssrc : track.mSsrcs)
        {
            sdp.append("a=ssrc:").append(std::to_string(ssrc.first)).append(" cname:").append(ssrc.second.length() ? ssrc.second : track.mSid).append(Sdp::endl);
            sdp.append("a=ssrc:").append(std::to_string(ssrc.first)).append(" msid:").append(track.mSid).append(" ").append(track.mId).append(Sdp::endl);
        }

        if (track.mSsrcg.size())
        {
            for (const std::string& grp : track.mSsrcg)
            {
                sdp.append("a=ssrc-group:").append(grp).append(Sdp::endl);
            }
        }
    }

    return sdp;
}

SfuConnection::SfuConnection(karere::Url&& sfuUrl, WebsocketsIO& websocketIO, void* appCtx, sfu::SfuInterface &call, DNScache& dnsCache)
    : WebsocketsClient(false)
    , mSfuUrl(std::move(sfuUrl))
    , mWebsocketIO(websocketIO)
    , mAppCtx(appCtx)
    , mCall(call)
    , mMainThreadId(std::this_thread::get_id())
    , mDnsCache(dnsCache)
{
    setCallbackToCommands(mCall, mCommands);
}

SfuConnection::~SfuConnection()
{
    if (mConnState != kDisconnected)
    {
        disconnect();
    }
}

void SfuConnection::setIsSendingBye(bool sending)
{
    mIsSendingBye = sending;
}

bool SfuConnection::isJoined() const
{
    return (mConnState == kJoined);
}

bool SfuConnection::isSendingByeCommand() const
{
    return mIsSendingBye;
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

void SfuConnection::disconnect(bool withoutReconnection)
{
    setConnState(kDisconnected);
    if (withoutReconnection)
    {
        // It isn't required check mConnectTimer because it's set at setConnState(kDisconnected);
        karere::cancelTimeout(mConnectTimer, mAppCtx);
        mConnectTimer = 0;
        abortRetryController();
    }
}

void SfuConnection::doConnect(const std::string &ipv4, const std::string &ipv6)
{
    assert (mSfuUrl.isValid());
    if (ipv4.empty() && ipv6.empty())
    {
        SFU_LOG_ERROR("Trying to connect sfu (%s) using empty Ip's (ipv4 and ipv6)", mSfuUrl.host.c_str());
        onSocketClose(0, 0, "sfu doConnect error, empty Ip's (ipv4 and ipv6)");
    }

    mTargetIp = (usingipv6 && ipv6.size()) ? ipv6 : ipv4;
    setConnState(kConnecting);
    SFU_LOG_DEBUG("Connecting to sfu using the IP: %s", mTargetIp.c_str());

    bool rt = wsConnect(&mWebsocketIO, mTargetIp.c_str(),
          mSfuUrl.host.c_str(),
          mSfuUrl.port,
          mSfuUrl.path.c_str(),
          mSfuUrl.isSecure);

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
                          mSfuUrl.host.c_str(),
                          mSfuUrl.port,
                          mSfuUrl.path.c_str(),
                          mSfuUrl.isSecure))
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
    /* mSfuUrl must always be valid, however we could not find the host in DNS cache
     * as in case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
     * mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones
     */
    assert(mSfuUrl.isValid());
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
        SFU_LOG_WARNING("retryPendingConnection: ignored (currently joining/joined, no forced disconnect was requested)");
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
    checkThreadId();    // Check that commandsQueue is always accessed from a single thread

    mCommandsQueue.push_back(command);   // push command in the queue
    processNextCommand();
}

void SfuConnection::processNextCommand(bool resetSending)
{
    checkThreadId(); // Check that commandsQueue is always accessed from a single thread

    if (resetSending)
    {
        // upon wsSendMsgCb we need to reset isSending flag
        mCommandsQueue.setSending(false);
        if (mIsSendingBye)
        {
            mCall.onSendByeCommand();
            return; // we have sent BYE command to SFU, following commands will be ignored
        }
    }

    if (mCommandsQueue.empty() || mCommandsQueue.sending())
    {
        std::string msg = "processNextCommand: skip processing next command";
        if (mCommandsQueue.empty())     { msg.append(", mCommandsQueue is empty"); }
        if (mCommandsQueue.sending())   { msg.append(", sending is true"); }
        SFU_LOG_DEBUG("%s", msg.c_str());
        return;
    }

    mCommandsQueue.setSending(true);
    std::string command = mCommandsQueue.pop();

    // mCommandsQueue is a sequencial queue, so new commands just can be processed if previous commands have already been sent
    if (command.find("{\"a\":\"BYE\",\"rsn\":") != std::string::npos)
    {
        // set mIsSendingBye flag true, to indicate that we are going to send BYE command
        mIsSendingBye = true;
    }

    assert(!command.empty());
    SFU_LOG_DEBUG("Send command: %s", command.c_str());
    std::unique_ptr<char[]> buffer(mega::MegaApi::strdup(command.c_str()));
    bool rc = wsSendMessage(buffer.get(), command.length());

    if (!rc)
    {
        mSendPromise.reject("Socket is not ready");
        if (mIsSendingBye)
        {
             // if wsSendMessage failed inmediately trying to send BYE command, call onSendByeCommand in order to
             // execute the expected action (retry, remove or disconnect call) that triggered the BYE command sent
             mCommandsQueue.setSending(false);
             mCall.onSendByeCommand();
             return;
        }
        processNextCommand(true);
    }
}

void SfuConnection::clearCommandsQueue()
{
    checkThreadId(); // Check that commandsQueue is always accessed from a single thread
    SFU_LOG_WARNING("SfuConnection: clearing commands queue");
    setIsSendingBye(false);
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

const karere::Url& SfuConnection::getSfuUrl()
{
    return mSfuUrl;
}

void SfuConnection::setCallbackToCommands(sfu::SfuInterface &call, std::map<std::string, std::unique_ptr<sfu::Command>>& commands)
{
    commands[AVCommand::COMMAND_NAME] = mega::make_unique<AVCommand>(std::bind(&sfu::SfuInterface::handleAvCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[AnswerCommand::COMMAND_NAME] = mega::make_unique<AnswerCommand>(std::bind(&sfu::SfuInterface::handleAnswerCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9), call);
    commands[KeyCommand::COMMAND_NAME] = mega::make_unique<KeyCommand>(std::bind(&sfu::SfuInterface::handleKeyCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), call);
    commands[VthumbsCommand::COMMAND_NAME] = mega::make_unique<VthumbsCommand>(std::bind(&sfu::SfuInterface::handleVThumbsCommand, &call, std::placeholders::_1), call);
    commands[VthumbsStartCommand::COMMAND_NAME] = mega::make_unique<VthumbsStartCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStartCommand, &call), call);
    commands[VthumbsStopCommand::COMMAND_NAME] = mega::make_unique<VthumbsStopCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStopCommand, &call), call);
    commands[HiResCommand::COMMAND_NAME] = mega::make_unique<HiResCommand>(std::bind(&sfu::SfuInterface::handleHiResCommand, &call, std::placeholders::_1), call);
    commands[HiResStartCommand::COMMAND_NAME] = mega::make_unique<HiResStartCommand>(std::bind(&sfu::SfuInterface::handleHiResStartCommand, &call), call);
    commands[HiResStopCommand::COMMAND_NAME] = mega::make_unique<HiResStopCommand>(std::bind(&sfu::SfuInterface::handleHiResStopCommand, &call), call);
    commands[SpeakReqsCommand::COMMAND_NAME] = mega::make_unique<SpeakReqsCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqsCommand, &call, std::placeholders::_1), call);
    commands[SpeakReqDelCommand::COMMAND_NAME] = mega::make_unique<SpeakReqDelCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqDelCommand, &call, std::placeholders::_1), call);
    commands[SpeakOnCommand::COMMAND_NAME] = mega::make_unique<SpeakOnCommand>(std::bind(&sfu::SfuInterface::handleSpeakOnCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[SpeakOffCommand::COMMAND_NAME] = mega::make_unique<SpeakOffCommand>(std::bind(&sfu::SfuInterface::handleSpeakOffCommand, &call, std::placeholders::_1), call);
    commands[PeerJoinCommand::COMMAND_NAME] = mega::make_unique<PeerJoinCommand>(std::bind(&sfu::SfuInterface::handlePeerJoin, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5), call);
    commands[PeerLeftCommand::COMMAND_NAME] = mega::make_unique<PeerLeftCommand>(std::bind(&sfu::SfuInterface::handlePeerLeft, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[ByeCommand::COMMAND_NAME] = mega::make_unique<ByeCommand>(std::bind(&sfu::SfuInterface::handleBye, &call, std::placeholders::_1), call);
    commands[ModAddCommand::COMMAND_NAME] = mega::make_unique<ModAddCommand>(std::bind(&sfu::SfuInterface::handleModAdd, &call, std::placeholders::_1), call);
    commands[ModDelCommand::COMMAND_NAME] = mega::make_unique<ModDelCommand>(std::bind(&sfu::SfuInterface::handleModDel, &call, std::placeholders::_1), call);
    commands[HelloCommand::COMMAND_NAME] = mega::make_unique<HelloCommand>(std::bind(&sfu::SfuInterface::handleHello, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7), call);
    commands[WrDumpCommand::COMMAND_NAME] = mega::make_unique<WrDumpCommand>(std::bind(&sfu::SfuInterface::handleWrDump, &call, std::placeholders::_1), call);
    commands[WrEnterCommand::COMMAND_NAME] = mega::make_unique<WrEnterCommand>(std::bind(&sfu::SfuInterface::handleWrEnter, &call, std::placeholders::_1), call);
    commands[WrLeaveCommand::COMMAND_NAME] = mega::make_unique<WrLeaveCommand>(std::bind(&sfu::SfuInterface::handleWrLeave, &call, std::placeholders::_1), call);
    commands[WrAllowCommand::COMMAND_NAME] = mega::make_unique<WrAllowCommand>(std::bind(&sfu::SfuInterface::handleWrAllow, &call), call);
    commands[WrDenyCommand::COMMAND_NAME] = mega::make_unique<WrDenyCommand>(std::bind(&sfu::SfuInterface::handleWrDeny, &call), call);
    commands[WrAllowReqCommand::COMMAND_NAME] = mega::make_unique<WrAllowReqCommand>(std::bind(&sfu::SfuInterface::handleWrAllowReq, &call, std::placeholders::_1), call);
    commands[WrUsersAllowCommand::COMMAND_NAME] = mega::make_unique<WrUsersAllowCommand>(std::bind(&sfu::SfuInterface::handleWrUsersAllow, &call, std::placeholders::_1), call);
    commands[WrUsersDenyCommand::COMMAND_NAME] = mega::make_unique<WrUsersDenyCommand>(std::bind(&sfu::SfuInterface::handleWrUsersDeny, &call, std::placeholders::_1), call);
}

bool SfuConnection::parseSfuData(const char* data, rapidjson::Document& document, std::string& command, std::string& warnMsg, std::string& errMsg, int32_t& errCode)
{
    SFU_LOG_DEBUG("Data received: %s", data);
    rapidjson::StringStream stringStream(data);
    document.ParseStream(stringStream);

    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        errMsg = "Failure at: Parser json error";
        return false;
    }

    rapidjson::Value::ConstMemberIterator jsonIterator = document.FindMember(Command::COMMAND_IDENTIFIER.c_str());
    rapidjson::Value::ConstMemberIterator jsonErrIterator = document.FindMember(Command::ERROR_IDENTIFIER.c_str());
    rapidjson::Value::ConstMemberIterator jsonWarnIterator = document.FindMember(Command::WARN_IDENTIFIER.c_str());
    if ((jsonIterator == document.MemberEnd() || !jsonIterator->value.IsString())
            && (jsonErrIterator == document.MemberEnd())
            && (jsonWarnIterator == document.MemberEnd()))
    {
        errMsg = "Received data doesn't have 'a' field";
        return false;
    }

    if (jsonErrIterator != document.MemberEnd() && jsonErrIterator->value.IsInt())
    {
        errMsg = "Unknown reason";
        rapidjson::Value::ConstMemberIterator jsonErrMsgIterator = document.FindMember(Command::ERROR_MESSAGE.c_str());
        if (jsonErrMsgIterator != document.MemberEnd() && jsonErrMsgIterator->value.IsString())
        {
            errMsg = jsonErrMsgIterator->value.GetString();
        }
        errCode = jsonErrIterator->value.GetInt();
        return true;
    }

    if (jsonWarnIterator != document.MemberEnd() && jsonWarnIterator->value.IsString())
    {
        warnMsg = jsonWarnIterator->value.GetString();
        return true;
    }

    command = jsonIterator->value.GetString();
    return true;
}

bool SfuConnection::handleIncomingData(const char *data, size_t len)
{
    // init errCode to invalid value, to check if a valid errCode has been returned by SFU
    int32_t errCode = INT32_MIN;
    std::string command;
    std::string warnMsg;
    std::string errMsg;
    rapidjson::Document document;

    if (!parseSfuData(data, document, command, warnMsg, errMsg, errCode))
    {
        // error parsing incoming data from SFU
        SFU_LOG_ERROR("%s", errMsg.c_str());
        return false;
    }

    if (errCode != INT32_MIN)
    {
        // process errCode returned by SFU
        mCall.error(static_cast<unsigned int>(errCode), errMsg);
        return true;
    }

    if (!warnMsg.empty())
    {
        SFU_LOG_WARNING("%s", warnMsg.c_str());
        return true;
    }

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

bool SfuConnection::joinSfu(const Sdp &sdp, const std::map<std::string, std::string> &ivs, std::string& ephemeralKey, int avFlags, Cid_t prevCid, int speaker, int vthumbs)
{
    rapidjson::Document json(rapidjson::kObjectType);

    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_JOIN.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    rapidjson::Value sdpValue(rapidjson::kObjectType);
    auto data = sdp.data();
    for (const auto& data : data)
    {
        rapidjson::Value dataValue(rapidjson::kStringType);
        dataValue.SetString(data.second.c_str(),  static_cast<rapidjson::SizeType>(data.second.length()));
        sdpValue.AddMember(rapidjson::Value(data.first.c_str(), static_cast<rapidjson::SizeType>(data.first.length())), dataValue, json.GetAllocator());
    }

    rapidjson::Value tracksValue(rapidjson::kArrayType);
    auto tracks = sdp.tracks();
    for (const Sdp::Track& track : tracks)
    {
        if (track.mType != "a" && track.mType != "v")
        {
            // skip any other (unknown) type of track. Only audio and video are supported
            continue;
        }

        rapidjson::Value dataValue(rapidjson::kObjectType);
        dataValue.AddMember("t", rapidjson::Value(track.mType.c_str(), static_cast<rapidjson::SizeType>(track.mType.length())), json.GetAllocator());
        dataValue.AddMember("mid", rapidjson::Value(track.mMid), json.GetAllocator());
        dataValue.AddMember("dir", rapidjson::Value(track.mDir.c_str(), static_cast<rapidjson::SizeType>(track.mDir.length())), json.GetAllocator());
        if (track.mSid.length())
        {
            dataValue.AddMember("sid", rapidjson::Value(track.mSid.c_str(), static_cast<rapidjson::SizeType>(track.mSid.length())), json.GetAllocator());
        }

        if (track.mId.length())
        {
            dataValue.AddMember("id", rapidjson::Value(track.mId.c_str(), static_cast<rapidjson::SizeType>(track.mId.length())), json.GetAllocator());
        }

        if (track.mSsrcg.size())
        {
            rapidjson::Value ssrcgValue(rapidjson::kArrayType);
            for (const auto& element : track.mSsrcg)
            {
                ssrcgValue.PushBack(rapidjson::Value(element.c_str(), static_cast<rapidjson::SizeType>(element.length())), json.GetAllocator());
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
                elementValue.AddMember("cname", rapidjson::Value(element.second.c_str(), static_cast<rapidjson::SizeType>(element.second.size())), json.GetAllocator());

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
        ivsValue.AddMember(rapidjson::Value(iv.first.c_str(), static_cast<rapidjson::SizeType>(iv.first.size())), rapidjson::Value(iv.second.c_str(), static_cast<rapidjson::SizeType>(iv.second.size())), json.GetAllocator());
    }

    rapidjson::Value pubkey(rapidjson::kStringType);
    pubkey.SetString(ephemeralKey.c_str(), static_cast<unsigned int>(ephemeralKey.length()), json.GetAllocator());
    json.AddMember(rapidjson::Value("pubk"), pubkey, json.GetAllocator());

    json.AddMember("ivs", ivsValue, json.GetAllocator());
    json.AddMember("av", avFlags, json.GetAllocator());
    if (prevCid) // cid 0 is invalid
    {
        // when reconnecting, send the SFU the CID of the previous connection, so it can kill it instantly
        json.AddMember("cid", prevCid, json.GetAllocator());
    }

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
    if (keys.empty())
    {
        return true;
    }

    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_SENDKEY.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    rapidjson::Value idValue(rapidjson::kNumberType);
    idValue.SetUint(id);
    json.AddMember(rapidjson::Value("id"), idValue, json.GetAllocator());

    rapidjson::Value dataValue(rapidjson::kArrayType);
    for (const auto& key : keys)
    {
        rapidjson::Value keyValue(rapidjson::kArrayType);
        keyValue.PushBack(rapidjson::Value(key.first), json.GetAllocator());
        keyValue.PushBack(rapidjson::Value(key.second.c_str(), static_cast<rapidjson::SizeType>(key.second.length())), json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_AV.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_GET_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_DEL_VTHUMBS.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_GET_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    json.AddMember("cid", rapidjson::Value(cid), json.GetAllocator());
    if (r)
    {
        // avoid sending r flag if it's zero (it's useless and it could generate issues at SFU)
        json.AddMember("r", rapidjson::Value(r), json.GetAllocator());
    }
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
    cmdValue.SetString(SfuConnection::CSFU_DEL_HIRES.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_HIRES_SET_LO.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_LAYER.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
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
    cmdValue.SetString(SfuConnection::CSFU_SPEAK_RQ.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_SPEAK_RQ_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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
    cmdValue.SetString(SfuConnection::CSFU_SPEAK_DEL.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

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

bool SfuConnection::sendBye(int termCode)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_BYE.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
    json.AddMember("rsn", rapidjson::Value(termCode), json.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendWrPush(const std::set<karere::Id>& users, const bool all)
{
    if (users.empty() && !all)
    {
        assert(false);
        SFU_LOG_WARNING("sendWrPush: invalid arguments provided");
        return false;
    }
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_WR_PUSH.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
    addWrUsersArray(users, all, json);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendWrAllow(const std::set<karere::Id>& users, const bool all)
{
    if (users.empty() && !all)
    {
        assert(false);
        SFU_LOG_WARNING("sendWrAllow: invalid arguments provided");
        return false;
    }
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_WR_ALLOW.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
    addWrUsersArray(users, all, json);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendWrKick(const std::set<karere::Id>& users)
{
    if (users.empty())
    {
        assert(false);
        SFU_LOG_WARNING("sendWrKick: invalid arguments provided");
        return false;
    }
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_WR_KICK.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
    addWrUsersArray(users, false, json);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::addWrUsersArray(const std::set<karere::Id>& users, const bool all, rapidjson::Document& json)
{
    assert(!users.empty() || all);
    if (users.empty())
    {
        std::string userStr = "*";
        rapidjson::Value nameValue(rapidjson::kStringType);
        nameValue.SetString(userStr.c_str(), static_cast<rapidjson::SizeType>(userStr.length()), json.GetAllocator());
        json.AddMember(rapidjson::Value("users"), nameValue, json.GetAllocator());
    }
    else
    {
        rapidjson::Value usersArray(rapidjson::kArrayType);
        for (const auto& user: users)
        {
            rapidjson::Value auxValue(rapidjson::kStringType);
            auxValue.SetString(user.toString().c_str(), static_cast<rapidjson::SizeType>(user.toString().length()), json.GetAllocator());
            usersArray.PushBack(auxValue, json.GetAllocator());
        }
        json.AddMember(rapidjson::Value("users"), usersArray, json.GetAllocator());
    }
    return true;
}

void SfuConnection::setConnState(SfuConnection::ConnState newState)
{
    if (newState == mConnState)
    {
        SFU_LOG_DEBUG("Tried to change connection state to the current state: %s", connStateToStr(newState));
        return;
    }
    else if(newState == SfuConnection::ConnState::kConnected && mConnState > newState)
    {
        SFU_LOG_DEBUG("Tried to change connection state to kConnected but current state is: %s", connStateToStr(mConnState));
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

        // if connect-timer is running, it must be reset (kResolving --> kDisconnected)
        if (mConnectTimer)
        {
            karere::cancelTimeout(mConnectTimer, mAppCtx);
            mConnectTimer = 0;
        }

        // start a timer to ensure the connection is established after kConnectTimeout. Otherwise, reconnect
        auto wptr = weakHandle();
        mConnectTimer = karere::setTimeout([this, wptr]()
        {
            if (wptr.deleted())
                return;

            SFU_LOG_DEBUG("Reconnection attempt has not succeed after %d. Reconnecting...", kConnectTimeout);
            mConnectTimer = 0;
            retryPendingConnection(true);
        }, kConnectTimeout * 1000, mAppCtx);
    }
    else if (mConnState == kConnected)
    {
        SFU_LOG_DEBUG("Sfu connected to %s", mTargetIp.c_str());

        mDnsCache.connectDoneByHost(mSfuUrl.host, mTargetIp);
        assert(!mConnectPromise.done());
        mConnectPromise.resolve();
        mRetryCtrl.reset();

        if (mConnectTimer)
        {
            karere::cancelTimeout(mConnectTimer, mAppCtx);
            mConnectTimer = 0;
        }
    }
}

void SfuConnection::wsConnectCb()
{
    if (mConnState != kConnecting)
    {
        SFU_LOG_WARNING("Connection to SFU has been established, but current connection state is %s, instead of connecting (as we expected)"
                           , connStateToStr(mConnState));
        return;
    }

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

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
bool SfuConnection::wsSSLsessionUpdateCb(const CachedSession &sess)
{
    // update the session's data in the DNS cache
    return mDnsCache.updateTlsSession(sess);
}
#endif

void SfuConnection::onSocketClose(int errcode, int errtype, const std::string &reason)
{
    if (mConnState == kDisconnected)
    {
        SFU_LOG_DEBUG("onSocketClose: we are already in kDisconnected state");
        if (!mRetryCtrl)
        {
            SFU_LOG_ERROR("There's no retry controller instance when calling onSocketClose in kDisconnected state");
            reconnect(); // start retry controller
        }
        return;
    }

    SFU_LOG_WARNING("Socket close on IP %s. Reason: %s", mTargetIp.c_str(), reason.c_str());
    mCall.onSfuDisconnected();
    setIsSendingBye(false); // reset mIsSendingBye as we are already disconnected from SFU
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
    else // oldState is kResolving or kConnecting
         // -> tell retry controller that the connect attempt failed
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

        /* mSfuUrl must always be valid, however we could not find the host in DNS cache
         * as in case we have reached max SFU records (abs(kSfuShardEnd - kSfuShardStart)),
         * mCurrentShardForSfu will be reset, so oldest records will be overwritten by new ones
         */
        if (!mSfuUrl.isValid())
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

            std::string ipv4, ipv6;
            bool cachedIpsByHost = mDnsCache.getIpByHost(mSfuUrl.host, ipv4, ipv6);

            setConnState(kResolving);
            SFU_LOG_DEBUG("Resolving hostname %s...", mSfuUrl.host.c_str());

            auto retryCtrl = mRetryCtrl.get();
            int statusDNS = wsResolveDNS(&mWebsocketIO, mSfuUrl.host.c_str(),
                         [wptr, cachedIpsByHost, this, retryCtrl, attemptNo, ipv4, ipv6](int statusDNS, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
            {
                if (wptr.deleted())
                {
                    SFU_LOG_DEBUG("DNS resolution completed, but sfu client was deleted.");
                    return;
                }

                if (!mRetryCtrl)
                {
                    if (isOnline())
                    {
                        SFU_LOG_DEBUG("DNS resolution completed but ignored: connection is already established using cached IP");
                        assert(cachedIpsByHost);
                    }
                    else
                    {
                        SFU_LOG_DEBUG("DNS resolution completed but ignored: connection was aborted");
                    }
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
                    if (isOnline() && cachedIpsByHost)
                    {
                        assert(false);  // this case should be handled already at: if (!mRetryCtrl)
                        SFU_LOG_ERROR_NO_STATS("DNS error, but connection is established. Relaying on cached IPs...");
                        return;
                    }

                    if (statusDNS < 0)
                    {
                        /* don't send log error to SFU stats server, if DNS error is:
                         *  - UV__EAI_AGAIN  (-3001)
                         *  - UV__EAI_NODATA (-3007)
                         *  - UV__EAI_NONAME (-3008)
                         */
                        (statusDNS == 3001 || statusDNS == 3007 || statusDNS == 3008)
                            ? SFU_LOG_ERROR_NO_STATS("Async DNS error in sfu. Error code: %d", statusDNS)
                            : SFU_LOG_ERROR("Async DNS error in sfu. Error code: %d", statusDNS);
                    }
                    else
                    {
                        SFU_LOG_ERROR_NO_STATS("Async DNS error in sfu. Empty set of IPs");
                    }

                    assert(!isOnline());
                    if (statusDNS == wsGetNoNameErrorCode(&mWebsocketIO))
                    {
                        retryPendingConnection(true);
                    }
                    else if (mConnState == kResolving)
                    {
                        onSocketClose(0, 0, "Async DNS error (sfu connection)");
                    }
                    // else in case kConnecting let the connection attempt progress
                    return;
                }

                if (!cachedIpsByHost) // connect required DNS lookup
                {
                    SFU_LOG_DEBUG("Hostname resolved and there was no previous cached Ip's for this host. Connecting...");
                    mDnsCache.setSfuIp(mSfuUrl.host, ipsv4, ipsv6);
                    const std::string &resolvedIpv4 = ipsv4.empty() ? "" : ipsv4.front();
                    const std::string &resolvedIpv6 = ipsv6.empty() ? "" : ipsv6.front();
                    doConnect(resolvedIpv4, resolvedIpv6);
                    return;
                }

                if (mDnsCache.isMatchByHost(mSfuUrl.host, ipsv4, ipsv6))
                {
                    if (!ipv4.empty() && !ipsv4.empty() && !ipv6.empty() && !ipsv6.empty()
                                     && std::find(ipsv4.begin(), ipsv4.end(), ipv4) == ipsv4.end()
                                     && std::find(ipsv6.begin(), ipsv6.end(), ipv6) == ipsv6.end())
                    {
                       /* If there are multiple calls trying to reconnect in parallel against the same SFU server, and
                        * IP's have been changed for that moment, first DNS resolution attempt that finishes,
                        * will update IP's in cache (and will call onsocket close), but for the rest of calls,
                        * when DNS resolution ends (with same IP's returned) returned IP's will already match in cache.
                        *
                        * In this case Ip's used for that reconnection attempt are outdated, so we need to force reconnect
                        */
                        SFU_LOG_WARNING("DNS resolve matches cached IPs, but Ip's used for this reconnection attempt are outdated. Forcing reconnect...");
                        onSocketClose(0, 0, "Outdated Ip's. Forcing reconnect... (sfu)");
                    }
                    else
                    {
                        SFU_LOG_DEBUG("DNS resolve matches cached IPs, let current attempt finish.");
                    }
                }
                else
                {
                    // update DNS cache
                    mDnsCache.setSfuIp(mSfuUrl.host, ipsv4, ipsv6);
                    SFU_LOG_WARNING("DNS resolve doesn't match cached IPs. Forcing reconnect...");
                    retryPendingConnection(true);
                }
            });

            // immediate error at wsResolveDNS()
            if (statusDNS < 0)
            {
                std::string errStr = "Immediate DNS error in sfu. Error code: " + std::to_string(statusDNS);
                SFU_LOG_ERROR_NO_STATS("%s", errStr.c_str());

                assert(mConnState == kResolving);
                assert(!mConnectPromise.done());

                // reject promise, so the RetryController starts a new attempt
                mConnectPromise.reject(errStr, statusDNS, promise::kErrorTypeGeneric);
            }
            else if (cachedIpsByHost) // if wsResolveDNS() failed immediately, very likely there's
            // no network connetion, so it's futile to attempt to connect
            {
                // this connect attempt is made in parallel with DNS resolution, use cached IP's
                SFU_LOG_DEBUG("Connection attempt (with Cached Ip's) in parallel to DNS resolution");
                doConnect(ipv4, ipv6);
            }

            return mConnectPromise
            .then([wptr, this]()
            {
                if (wptr.deleted())
                    return;

                assert(isOnline());
                mCall.onSfuConnected();
            });
        }, wptr, mAppCtx
                     , nullptr                              // cancel function
                     , KARERE_RECONNECT_ATTEMPT_TIMEOUT     // initial attempt timeout (increases exponentially)
                     , KARERE_RECONNECT_MAX_ATTEMPT_TIMEOUT // maximum attempt timeout
                     , 0                                    // max number of attempts
                     , KARERE_RECONNECT_DELAY_MAX           // max single wait between attempts
                     , 0));                                 // initial single wait between attempts  (increases exponentially)


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

SfuClient::SfuClient(WebsocketsIO& websocketIO, void* appCtx, rtcModule::RtcCryptoMeetings *rRtcCryptoMeetings)
    : mRtcCryptoMeetings(std::make_shared<rtcModule::RtcCryptoMeetings>(*rRtcCryptoMeetings))
    , mWebsocketIO(websocketIO)
    , mAppCtx(appCtx)
{

}

SfuConnection* SfuClient::createSfuConnection(karere::Id chatid, karere::Url&& sfuUrl, SfuInterface &call, DNScache &dnsCache)
{
    assert(mConnections.find(chatid) == mConnections.end());
    mConnections[chatid] = mega::make_unique<SfuConnection>(std::move(sfuUrl), mWebsocketIO, mAppCtx, call, dnsCache);
    SfuConnection* sfuConnection = mConnections[chatid].get();
    sfuConnection->connect();
    return sfuConnection;
}

void SfuClient::closeSfuConnection(karere::Id chatid)
{
    mConnections[chatid]->disconnect();
    mConnections.erase(chatid);
}

std::shared_ptr<rtcModule::RtcCryptoMeetings> SfuClient::getRtcCryptoMeetings()
{
    return mRtcCryptoMeetings;
}

void SfuClient::addVersionToUrl(karere::Url& sfuUrl, Cid_t myCid)
{
    std::string app;
    if (sfuUrl.path.back() != '?') // if last URL char is '?' => just add version
    {
        app = !sfuUrl.path.find("?")
                 ? "&"  // url already has parameters
                 : "?"; // add ? as append character
    }

    sfuUrl.path.append(app).append("v=").append(std::to_string(getSfuVersion()));

    if (myCid != 0) // in case of reconenct add cid (if valid)
    {
        sfuUrl.path.append("&cid=").append(std::to_string(myCid));;
    }
}

unsigned int SfuClient::getSfuVersion()
{
    return mSfuVersion;
}

void SfuClient::retryPendingConnections(bool disconnect)
{
    for (auto it = mConnections.begin(); it != mConnections.end(); it++)
    {
        it->second->retryPendingConnection(disconnect);
    }
}

PeerLeftCommand::PeerLeftCommand(const PeerLeftCommandFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
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

    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("rsn");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'rsn' field");
        return false;
    }

    ::mega::MegaHandle cid = (cidIterator->value.GetUint64());
    unsigned termcode = reasonIterator->value.GetUint();
    return mComplete(static_cast<Cid_t>(cid), termcode);
}

ByeCommand::ByeCommand(const ByeCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool ByeCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("rsn");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'rsn' field");
        return false;
    }

    return mComplete(reasonIterator->value.GetUint() /*termcode */);
}

ModAddCommand::ModAddCommand(const ModAddCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool ModAddCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("user");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsString())
    {
        SFU_LOG_ERROR("MOD_ADD: Received data doesn't have 'user' field");
        return false;
    }
    std::string userIdString = reasonIterator->value.GetString();
    ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
    return mComplete(userId /*userid*/);
}

ModDelCommand::ModDelCommand(const ModDelCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool ModDelCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("user");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsString())
    {
        SFU_LOG_ERROR("MOD_DEL: Received data doesn't have 'user' field");
        return false;
    }
    std::string userIdString = reasonIterator->value.GetString();
    ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
    return mComplete(userId /*userid*/);
}

HelloCommand::HelloCommand(const HelloCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool HelloCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        assert(false);
        SFU_LOG_ERROR("HelloCommand: Received data doesn't have 'cid' field");
        return false;
    }
    Cid_t cid = cidIterator->value.GetUint();

    unsigned int nAudioTracks = 0;
    rapidjson::Value::ConstMemberIterator naIterator = command.FindMember("na");
    if (naIterator != command.MemberEnd() && cidIterator->value.IsUint())
    {
        nAudioTracks = naIterator->value.GetUint();
    }
    else
    {
        SFU_LOG_ERROR("HelloCommand: Received data doesn't have 'na' field");
    }

    unsigned int nVideoTracks = 0;
    rapidjson::Value::ConstMemberIterator nvIterator = command.FindMember("nv");
    if (nvIterator != command.MemberEnd() && cidIterator->value.IsUint())
    {
        nVideoTracks = nvIterator->value.GetUint();
    }
    else
    {
        SFU_LOG_ERROR("HelloCommand: Received data doesn't have 'nv' field");
    }

    // parse moderators list
    std::set<karere::Id> moderators;
    rapidjson::Value::ConstMemberIterator modsIterator = command.FindMember("mods");
    if (modsIterator != command.MemberEnd() && modsIterator->value.IsArray())
    {
        parseUsersArray(moderators, modsIterator);
    }

    bool wr = false;
    bool allowed = false;
    std::map<karere::Id, bool> wrUsers;
    rapidjson::Value::ConstMemberIterator wrIterator = command.FindMember("wr");
    if (wrIterator != command.MemberEnd())
    {
        if (!wrIterator->value.IsObject())
        {
            assert(false);
            SFU_LOG_ERROR("HelloCommand: Received wr is not an object");
            return false;
        }

        wr = true;
        const rapidjson::Value& obj = wrIterator->value;
        assert(obj.IsObject());
        rapidjson::Value::ConstMemberIterator allowIterator = obj.FindMember("allow");
        if (allowIterator == obj.MemberEnd() || !allowIterator->value.IsUint())
        {
             assert(false);
             SFU_LOG_ERROR("HelloCommand: 'allow' field not found in wr object");
             return false;
        }
        allowed = allowIterator->value.GetUint();

        if (!parseUsersMap(wrUsers, obj))
        {
            assert(false);
            SFU_LOG_ERROR("HelloCommand: users array in wr is ill-formed");
            return false;
        }
    }
    return mComplete(cid, nAudioTracks, nVideoTracks, moderators, wr, allowed, wrUsers);
}

WrDumpCommand::WrDumpCommand(const WrDumpCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrDumpCommand::processCommand(const rapidjson::Document& command)
{
    std::map<karere::Id, bool> users;
    rapidjson::Value::ConstMemberIterator usersIterator = command.FindMember("users");
    if (!parseUsersMap(users, usersIterator->value))
    {
        assert(false);
        SFU_LOG_ERROR("WrDumpCommand: users array is ill-formed");
        return false;
    }
    return mComplete(users);
}

WrEnterCommand::WrEnterCommand(const WrEnterCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrEnterCommand::processCommand(const rapidjson::Document& command)
{
    std::map<karere::Id, bool> users;
    rapidjson::Value::ConstMemberIterator usersIterator = command.FindMember("users");
    if (!parseUsersMap(users, usersIterator->value))
    {
        assert(false);
        SFU_LOG_ERROR("WrEnterCommand: users array is ill-formed");
        return false;
    }
    return mComplete(users);
}

WrLeaveCommand::WrLeaveCommand(const WrLeaveCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrLeaveCommand::processCommand(const rapidjson::Document& command)
{
    std::set<karere::Id> users;
    rapidjson::Value::ConstMemberIterator usersIterator = command.FindMember("users");
    if (usersIterator == command.MemberEnd() || !usersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("WrLeaveCommand: Received data doesn't have 'users' array");
        return false;
    }

    parseUsersArray(users, usersIterator);
    return mComplete(users);
}

WrAllowCommand::WrAllowCommand(const WrAllowCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrAllowCommand::processCommand(const rapidjson::Document& /*command*/)
{
    return mComplete();
}

WrDenyCommand::WrDenyCommand(const WrDenyCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrDenyCommand::processCommand(const rapidjson::Document& /*command*/)
{
    return mComplete();
}

WrAllowReqCommand::WrAllowReqCommand(const WrAllowReqCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrAllowReqCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("user");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsString())
    {
        SFU_LOG_ERROR("WrAllowReqCommand: Received data doesn't have 'user' field");
        return false;
    }
    std::string userIdString = reasonIterator->value.GetString();
    ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
    return mComplete(userId /*userid*/);
}

WrUsersAllowCommand::WrUsersAllowCommand(const WrUsersAllowCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrUsersAllowCommand::processCommand(const rapidjson::Document& command)
{
    std::set<karere::Id> users;
    rapidjson::Value::ConstMemberIterator usersIterator = command.FindMember("users");
    if (usersIterator == command.MemberEnd() || !usersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("WrUsersAllowCommand: Received data doesn't have 'users' array");
        return false;
    }

    parseUsersArray(users, usersIterator);
    return mComplete(users);
}

WrUsersDenyCommand::WrUsersDenyCommand(const WrUsersDenyCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrUsersDenyCommand::processCommand(const rapidjson::Document& command)
{
    std::set<karere::Id> users;
    rapidjson::Value::ConstMemberIterator usersIterator = command.FindMember("users");
    if (usersIterator == command.MemberEnd() || !usersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("WrUsersDenyCommand: Received data doesn't have 'users' array");
        return false;
    }

    parseUsersArray(users, usersIterator);
    return mComplete(users);
}
}
#endif
