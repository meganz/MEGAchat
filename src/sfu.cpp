#ifndef KARERE_DISABLE_WEBRTC
#include <sfu.h>
#include <base/promise.h>
#include <megaapi.h>
#include <mega/base64.h>

#include <rapidjson/writer.h>

#include <memory>


namespace sfu
{
const std::string Sdp::endl = "\r\n";

// SFU -> client (different types of notifications)
const std::string Command::COMMAND_IDENTIFIER           = "a";              // Command sent from SFU
const std::string Command::ERROR_IDENTIFIER             = "err";            // Error sent from SFU
const std::string Command::WARN_IDENTIFIER              = "warn";           // Warning sent from SFU
const std::string Command::DENY_IDENTIFIER              = "deny";           // Notifies that a command previously sent to SFU has been denied

// SFU -> client (commands)
const std::string AVCommand::COMMAND_NAME               = "AV";             // Notifies changes in Av flags for a peer
const std::string AnswerCommand::COMMAND_NAME           = "ANSWER";         // SFU response to JOIN command
const std::string KeyCommand::COMMAND_NAME              = "KEY";            // Notifies about a new media key for a peer
const std::string VthumbsCommand::COMMAND_NAME          = "VTHUMBS";        // Notifies that SFU started sending some peer video thumbnail tracks over the specified slots.
const std::string VthumbsStartCommand::COMMAND_NAME     = "VTHUMB_START";   // Instruct client to start sending the video thumbnail tracks
const std::string VthumbsStopCommand::COMMAND_NAME      = "VTHUMB_STOP";    // Instruct client to stop sending the video thumbnail tracks
const std::string HiResCommand::COMMAND_NAME            = "HIRES";          // Notifies that SFU started sending some peer video hires tracks over the specified slots.
const std::string HiResStartCommand::COMMAND_NAME       = "HIRES_START";    // Instruct client to start sending the video hires tracks
const std::string HiResStopCommand::COMMAND_NAME        = "HIRES_STOP";     // Instruct client to stop sending the video hires tracks
const std::string SpeakReqDelCommand::COMMAND_NAME      = "SPEAKRQ_DEL";    // Notifies that a speak request has been removed from speak request list
const std::string SpeakerAddCommand::COMMAND_NAME       = "SPEAKER_ADD";    // Notifies that an user has been added to active speakers list
const std::string SpeakReqCommand::COMMAND_NAME         = "SPEAKRQ";        // Notifies that one or more speak requests have been added to the pending list, waiting for approval
const std::string SpeakerDelCommand::COMMAND_NAME       = "SPEAKER_DEL";    // Notifies that an user has been removed from active speakers list
const std::string PeerJoinCommand::COMMAND_NAME         = "PEERJOIN";       // Notifies that a peer has joined to the call
const std::string PeerLeftCommand::COMMAND_NAME         = "PEERLEFT";       // Notifies that a peer has left the call
const std::string ByeCommand::COMMAND_NAME              = "BYE";            // Notifies that SFU disconnects a client from the call
const std::string MutedCommand::COMMAND_NAME            = "MUTED";          // Notifies that our audio has been muted remotely by a host user
const std::string ModAddCommand::COMMAND_NAME           = "MOD_ADD";        // Notifies that a moderator has been added to the moderator list
const std::string ModDelCommand::COMMAND_NAME           = "MOD_DEL";        // Notifies that a moderator has been removed from the moderator list
const std::string HelloCommand::COMMAND_NAME            = "HELLO";          // First command received after connecting to the SFU
const std::string WrDumpCommand::COMMAND_NAME           = "WR_DUMP";        // Notifies the current waiting room status
const std::string WrEnterCommand::COMMAND_NAME          = "WR_ENTER";       // Notifies moderators about user(s) that entered/were pushed in the waiting room
const std::string WrLeaveCommand::COMMAND_NAME          = "WR_LEAVE";       // Notifies moderators about user(s) who left the waiting room (either entered the call or disconnected)
const std::string WrAllowCommand::COMMAND_NAME          = "WR_ALLOW";       // Notifies that our user permission to enter the call has been granted (from waiting room)
const std::string WrDenyCommand::COMMAND_NAME           = "WR_DENY";        // Notifies that our user permission to enter the call has been denied (from waiting room)
const std::string WrUsersAllowCommand::COMMAND_NAME     = "WR_USERS_ALLOW"; // Notifies moderators that the specified user(s) were granted to enter the call.
const std::string WrUsersDenyCommand::COMMAND_NAME      = "WR_USERS_DENY";  // Notifies moderators that the specified user(s) have been denied to enter the call
const std::string WillEndCommand::COMMAND_NAME          = "WILL_END";       // Notify that call will end due to duration restrictions
const std::string ClimitsCommand::COMMAND_NAME          = "CLIMITS";        // Notify that the limits of the call has been changed
const std::string RaiseHandAddCommand::COMMAND_NAME     = "RHAND_ADD";      // Notify that a user raised their hand. Params: `user` - the userid of the user
const std::string RaiseHandDelCommand::COMMAND_NAME     = "RHAND_DEL";      // Notify that a user lowered their hand. Params: `user` - the userid of the user

// client -> SFU (commands)
const std::string SfuConnection::CSFU_JOIN              = "JOIN";           // Command sent to JOIN a call after connect to SFU (or receive WR_ALLOW if we are in a waiting room)
const std::string SfuConnection::CSFU_SENDKEY           = "KEY";            // Command sent to broadcast a new media key encrypted for all call participants
const std::string SfuConnection::CSFU_AV                = "AV";             // Command sent to modify the current AV flags
const std::string SfuConnection::CSFU_GET_VTHUMBS       = "GET_VTHUMBS";    // Command sent to request the low-res video thumbnails of specified peers
const std::string SfuConnection::CSFU_DEL_VTHUMBS       = "DEL_VTHUMBS";    // Command sent to instruct the SFU to stop sending the video thumbnail tracks of the specified peers
const std::string SfuConnection::CSFU_GET_HIRES         = "GET_HIRES";      // Command sent to request the hi-resolution track of a single peer
const std::string SfuConnection::CSFU_DEL_HIRES         = "DEL_HIRES";      // Command sent to instruct the SFU to stop sending the hi-res track of the specified peer
const std::string SfuConnection::CSFU_HIRES_SET_LO      = "HIRES_SET_LO";   // Command sent to instruct the SFU to send a lower spatial SVC layer of the hi-res stream of the specified peer
const std::string SfuConnection::CSFU_LAYER             = "LAYER";          // Command sent to select the SVC spatial and layers for all hi-res video tracks that the client receives
const std::string SfuConnection::CSFU_SPEAKRQ           = "SPEAKRQ";       // Command sent to request a client to become an active speaker
const std::string SfuConnection::CSFU_SPEAKRQ_DEL       = "SPEAKRQ_DEL";    // Command sent to cancel a pending speak request
const std::string SfuConnection::CSFU_SPEAKER_ADD       = "SPEAKER_ADD";    // Command sent to add an user to speakers list
const std::string SfuConnection::CSFU_SPEAKER_DEL       = "SPEAKER_DEL";    // Command sent to remove an user from speakers list
const std::string SfuConnection::CSFU_BYE               = "BYE";            // Command sent to disconnect orderly from the call
const std::string SfuConnection::CSFU_WR_PUSH           = "WR_PUSH";        // Command sent to push all clients of sent peerId's (that are in the call) to the waiting room
const std::string SfuConnection::CSFU_WR_ALLOW          = "WR_ALLOW";       // Command sent to grant the specified users the permission to enter the call from the waiting room
const std::string SfuConnection::CSFU_WR_KICK           = "WR_KICK";        // Command sent to disconnects all clients of the specified users, regardless of whether they are in the call or in the waiting room
const std::string SfuConnection::CSFU_MUTE              = "MUTE";           // Command sent to mute specific or all clients in a call
const std::string SfuConnection::CSFU_SETLIMIT          = "SETLIM";         // Command sent to set limits to call (duration, max participants ...)
                                                                            //      - SETLIM command is a temporal feature provided by SFU for testing purposes,
                                                                            //        and it's availability depends on SFU's release plan management
const std::string SfuConnection::CSFU_RHAND_ADD       = "RHAND";            // Command sent to raise hand to speak (no speak permission involved in this command)
const std::string SfuConnection::CSFU_RHAND_DEL       = "RHAND_DEL";        // Command sent to lower hand to speak (no speak permission involved in this command)

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

Peer::Peer(const karere::Id& peerid, const sfu::SfuProtocol sfuProtoVersion, const unsigned avFlags, const std::vector<std::string>* ivs, const Cid_t cid, const bool isModerator)
    : mCid(cid),
      mPeerid(peerid),
      mAvFlags(static_cast<uint8_t>(avFlags)),
      mIvs(ivs ? *ivs : std::vector<std::string>()),
      mIsModerator(isModerator),
      mSfuPeerProtoVersion(sfuProtoVersion)
{
}

Peer::Peer(const Peer& peer)
    : mCid(peer.mCid)
    , mPeerid(peer.mPeerid)
    , mAvFlags(peer.mAvFlags)
    , mIvs(peer.mIvs)
    , mIsModerator(peer.mIsModerator)
    , mEphemeralPubKeyDerived(peer.getEphemeralPubKeyDerived())
    , mSfuPeerProtoVersion(peer.getPeerSfuVersion())
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

const karere::Id& Peer::getPeerid() const
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

const std::vector<std::string>& Peer::getIvs() const
{
    return mIvs;
}

void Peer::setIvs(const std::vector<std::string>& ivs)
{
    mIvs = ivs;
}

std::string Peer::getEphemeralPubKeyDerived() const
{
    return mEphemeralPubKeyDerived;
}

bool Peer::setEphemeralPubKeyDerived(const std::string& key)
{
    if (getPeerSfuVersion() == sfu::SfuProtocol::SFU_PROTO_V1)
    {
        // we shouldn't receive any peer with protocol v1
        SFU_LOG_WARNING("setEphemeralPubKeyDerived: unexpected SFU protocol version [%u] for user: %s, cid: %u",
                        static_cast<std::underlying_type<sfu::SfuProtocol>::type>(getPeerSfuVersion()),
                        getPeerid().toString().c_str(), getCid());
        assert(false);
        return false;
    }

    if (!sfu::isKnownSfuVersion(getPeerSfuVersion()))
    {
        // important: upon an unkown peers's SFU protocol version, native client should act as if they are the latest known version
        SFU_LOG_WARNING("setEphemeralPubKeyDerived: unknown SFU protocol version [%u] for user: %s, cid: %u",
                         static_cast<std::underlying_type<sfu::SfuProtocol>::type>(getPeerSfuVersion()),
                         getPeerid().toString().c_str(), getCid());
    }

    if (key.empty() && getPeerSfuVersion() > sfu::SfuProtocol::SFU_PROTO_V0)
    {
        SFU_LOG_WARNING("setEphemeralPubKeyDerived: Empty ephemeral key for PeerId: %s Cid: %u",
                        getPeerid().toString().c_str() ,getCid());
        return false;
    }

    // peers that uses sfu protocol V0, doesn't provide an ephemeral key
    mEphemeralPubKeyDerived = key;
    return true;
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

bool Command::parseWrUsersMap(sfu::WrUserList& wrUsers, const rapidjson::Value& obj) const
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
            WrState state =  m->value.GetUint() ? WrState::WR_ALLOWED : WrState::WR_NOT_ALLOWED;
            WrRoomUser user { userId, state };
            wrUsers.emplace_back(user);
        }
    }
    return true;
}

void Command::parseUsersArray(std::set<karere::Id>& users, rapidjson::Value::ConstMemberIterator& it) const
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
        users.emplace(::mega::MegaApi::base64ToUserHandle(userIdString.c_str()));
    }
}

bool Command::parseUsersArrayInOrder(std::vector<karere::Id>& users, rapidjson::Value::ConstMemberIterator& it, const bool allowDuplicates) const
{
    std::set<karere::Id> duplicatedUsers;
    assert(it->value.IsArray());
    for (unsigned int j = 0; j < it->value.Capacity(); ++j)
    {
        if (!it->value[j].IsString())
        {
            SFU_LOG_ERROR("parse users array: invalid user handle value");
            users.clear(); // clear users list as it's ill-formed
            return false;
        }
        std::string userIdString = it->value[j].GetString();
        const karere::Id uh = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
        users.push_back(uh);
        if (!allowDuplicates && !duplicatedUsers.emplace(uh).second)
        {
            SFU_LOG_ERROR("parse users array: duplicated users");
            users.clear();
            return false;
        }
    }
    return true;
}

uint64_t Command::parseHandle(const rapidjson::Document &command, const std::string& paramName, const uint64_t defaultValue) const
{
    uint64_t h = defaultValue;
    rapidjson::Value::ConstMemberIterator handleIterator = command.FindMember(paramName.c_str());
    if (handleIterator != command.MemberEnd() && handleIterator->value.IsString())
    {
        std::string handleString = handleIterator->value.GetString();
        h = ::mega::MegaApi::base64ToUserHandle(handleString.c_str());
    }
    return h;
}

void Command::parseTracks(const rapidjson::Document& command, const std::string& arrayName, std::map<Cid_t, TrackDescriptor>& tracks) const
{
    // expected track array format: [[3 (cid), 2(mid), 1(reuse)]...]
    rapidjson::Value::ConstMemberIterator it = command.FindMember(arrayName.c_str());
    if (it != command.MemberEnd())
    {
        if (!it->value.IsArray())
        {
            SFU_LOG_ERROR("Received ill-formed tracks");
            assert(false);
            return;
        }

        for (unsigned int i = 0; i < it->value.Capacity(); ++i)
        {
            Cid_t cid = 0;
            TrackDescriptor td;
            if (!it->value[i].IsArray())
            {
                if (!it->value[i].IsUint())
                {
                    SFU_LOG_ERROR("Received ill-formed track");
                    assert(false);
                    return;
                }
                cid = it->value[i].GetUint();
                td.mMid = TrackDescriptor::invalidMid;
            }
            else
            {
                const auto& arr = it->value[i].GetArray();
                for (unsigned int j = 0; j < arr.Capacity(); ++j)
                {
                    if (j==0) { cid = arr[0].GetUint(); }
                    if (j==1) { td.mMid = arr[1].GetUint(); }
                    if (j==2) { td.mReuse = arr[2].GetUint(); }
                }
            }
            tracks[cid] = td; // add entry to map <cid, trackDescriptor>
        }
    }
}

std::optional<SfuInterface::CallLimits> Command::buildCallLimits(const rapidjson::Value& jsonObject)
{
    SfuInterface::CallLimits result{};
    rapidjson::Value::ConstMemberIterator durIterator = jsonObject.FindMember("dur");
    if (durIterator != jsonObject.MemberEnd())
    {
        if (durIterator->value.IsDouble())
        {
            // convert from minutes into seconds
            result.durationInSecs = static_cast<int>(durIterator->value.GetDouble() * 60);
        }
        else if (durIterator->value.IsUint())
        {
            // convert from minutes into seconds
            result.durationInSecs = static_cast<int>(durIterator->value.GetUint() * 60);
        }
        else
        {
            SFU_LOG_ERROR("buildCallLimits: Received param 'lim[dur]' has an unexpected format");
            assert(false);
            return std::nullopt;
        }
    }
    auto parseUserLims = [&jsonObject, this](const char* label) -> std::optional<int>
    {
        rapidjson::Value::ConstMemberIterator labelIterator = jsonObject.FindMember(label);
        if (labelIterator == jsonObject.MemberEnd())
        {
            return ::sfu::kCallLimitDisabled;
        }
        if (labelIterator->value.IsUint())
        {
            return static_cast<int>(labelIterator->value.GetUint());
        }
        SFU_LOG_ERROR("parseUserLims: Received param 'lim[%s]' has an unexpected format", label);
        assert(false);
        return std::nullopt;
    };
    auto usrNum = parseUserLims("usr");
    auto uclntNum = parseUserLims("uclnt");
    auto clntNum = parseUserLims("clnt");
    if (!usrNum || !clntNum || !uclntNum)
    {
        return std::nullopt;
    }
    result.numUsers = *usrNum;
    result.numClients = *clntNum;
    result.numClientsPerUser = *uclntNum;
    return result;
}

std::optional<SfuInterface::CallLimits> Command::parseCallLimits(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator limIterator = command.FindMember("lim");
    if (limIterator == command.MemberEnd())
    {
        return SfuInterface::CallLimits{};
    }
    // If limIterator not found it means all values are unlimited for this call (defaults for
    // CallLimits)
    if (!limIterator->value.IsObject())
    {
        SFU_LOG_ERROR("parseCallLimits: Received param 'lim' has an unexpected format");
        assert(false);
        return std::nullopt;
    }
    const rapidjson::Value& limObject = limIterator->value;
    return buildCallLimits(limObject);
}

uint64_t Command::hexToBinary(const std::string &hex)
{
    uint64_t value = 0;
    unsigned int bufferSize = static_cast<unsigned int>(hex.length()) >> 1;
    assert(bufferSize <= 8);
    std::unique_ptr<uint8_t []> buffer(new uint8_t[bufferSize]);
    unsigned int binPos = 0;
    for (unsigned int i = 0; i< hex.length(); binPos++)
    {
        // compiler doesn't guarantees the order "++" operation performed in relation to the second access of variable i (better to split in two operations)
        buffer[binPos] = static_cast<uint8_t>((hexDigitVal(hex[i++])) << 4);
        buffer[binPos] = static_cast<uint8_t>(buffer[binPos] | hexDigitVal(hex[i++]));
    }

    memcpy(&value, buffer.get(), bufferSize);

    return value;
}

std::vector<mega::byte> Command::hexToByteArray(const std::string &hex)
{
    unsigned int bufferSize = static_cast<unsigned int>(hex.length()) >> 1;
    std::vector<mega::byte> res(bufferSize);
    unsigned int binPos = 0;
    for (unsigned int i = 0; i< hex.length(); binPos++)
    {
        // compiler doesn't guarantees the order "++" operation performed in relation to the second access of variable i (better to split in two operations)
        res[binPos] = static_cast<uint8_t>((hexDigitVal(hex[i]) << 4) | hexDigitVal(hex[i+1]));
        i += 2;
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

    rapidjson::Value::ConstMemberIterator amidIterator = command.FindMember("amid");
    uint32_t audioMid = TrackDescriptor::invalidMid;
    if (amidIterator != command.MemberEnd() && amidIterator->value.IsInt()) // It's optional
    {
        audioMid = amidIterator->value.GetUint();
    }

    return mComplete(cid, av, audioMid);
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

    std::shared_ptr<Sdp> sdp(new Sdp(sdpIterator->value));

    rapidjson::Value::ConstMemberIterator tsIterator = command.FindMember("t"); // time elapsed since the start of the call
    if (tsIterator == command.MemberEnd() || !tsIterator->value.IsUint64())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 't' field");
        return false;
    }

    // offset ts when we join within the call respect the call start (ms)
    uint64_t callJoinOffset = tsIterator->value.GetUint64();
    std::vector<Peer> peers;
    std::map<Cid_t, std::string> keystrmap;
    std::map<Cid_t, uint32_t> amidmap;
    rapidjson::Value::ConstMemberIterator peersIterator = command.FindMember("peers");
    if (peersIterator != command.MemberEnd() && peersIterator->value.IsArray())
    {
        parsePeerObject(peers, keystrmap, amidmap, peersIterator);
    }

    // lists with the user handles of all non-moderator users that have been given speak permission
    std::set<karere::Id> speakers;
    rapidjson::Value::ConstMemberIterator spkIterator = command.FindMember("speakers");
    if (spkIterator != command.MemberEnd() && spkIterator->value.IsArray())
    {
        parseUsersArray(speakers, spkIterator);
    }

    // lists with the user handles of users that have pending speak requests
    std::vector<karere::Id> speakReqs;
    rapidjson::Value::ConstMemberIterator spkReqIterator = command.FindMember("spkrqs");
    if (spkReqIterator != command.MemberEnd() && spkReqIterator->value.IsArray())
    {
        if (bool parseSucceed = parseUsersArrayInOrder(speakReqs, spkReqIterator, false /*allowDuplicates=*/);
            !parseSucceed)
        {
            SFU_LOG_ERROR("AnswerCommand::processCommand: 'spkrqs' wrong format");
            assert(false);
            return false;
        }
    }

    // lists with the user handles of all users that have raised hand to speak
    std::vector<karere::Id> raiseHands;
    rapidjson::Value::ConstMemberIterator rhIterator = command.FindMember("rhands");
    if (rhIterator != command.MemberEnd() && rhIterator->value.IsArray())
    {
        if (auto parseSucceed = parseUsersArrayInOrder(raiseHands, rhIterator, false /*allowDuplicates=*/); !parseSucceed)
        {
            SFU_LOG_ERROR("AnswerCommand::processCommand: 'rhands' wrong format");
            assert(false);
            return false;
        }
    }

    std::map<Cid_t, TrackDescriptor> vthumbs;
    parseTracks(command, "vthumbs", vthumbs);

    return mComplete(cid, sdp, callJoinOffset, peers, keystrmap, vthumbs, speakers, speakReqs, raiseHands, amidmap);
}

void AnswerCommand::parsePeerObject(std::vector<Peer> &peers, std::map<Cid_t, std::string>& keystrmap, std::map<Cid_t, uint32_t>& amidmap, rapidjson::Value::ConstMemberIterator &it) const
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
                 // clients with SFU protocol = 0 won't send ephemeral pubkey
                 keystrmap.emplace(cid, pubkeyIterator->value.GetString());
            }

            rapidjson::Value::ConstMemberIterator sfuvIterator = it->value[j].FindMember("v");
            if (sfuvIterator == it->value[j].MemberEnd() || !sfuvIterator->value.IsUint())
            {
                SFU_LOG_ERROR("AnswerCommand::parsePeerObject: Received data doesn't have 'v' field");
                return;
            }
            unsigned int sfuVersion = sfuvIterator->value.GetUint();

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

            rapidjson::Value::ConstMemberIterator amidIterator = it->value[j].FindMember("amid");
            if (amidIterator != it->value[j].MemberEnd() && amidIterator->value.IsUint()) // It's optional
            {
                amidmap[cid] = amidIterator->value.GetUint();
            }

            unsigned av = avIterator->value.GetUint();
            // default initialization of isModerator. It gets updated for every peer at handleAnwerCommand with mod list received at HELLO command later in the run flow
            Peer peer(userId, static_cast<sfu::SfuProtocol>(sfuVersion), av, &ivs, cid, false /*isModerator*/);
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

    unsigned int auxid = idIterator->value.GetUint();
    if (auxid > maxKeyId)
    {
        SFU_LOG_ERROR("KeyCommand: keyId exceeds max allowed value (%u): %u", maxKeyId, auxid);
        return false;
    }
    Keyid_t id = static_cast<Keyid_t>(auxid);

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

bool VthumbsStartCommand::processCommand(const rapidjson::Document &)
{
    return mComplete();
}

VthumbsStopCommand::VthumbsStopCommand(const VtumbsStopCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool VthumbsStopCommand::processCommand(const rapidjson::Document &)
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

bool HiResStartCommand::processCommand(const rapidjson::Document &)
{
    return mComplete();
}

HiResStopCommand::HiResStopCommand(const HiResStopCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{

}

bool HiResStopCommand::processCommand(const rapidjson::Document &)
{
    return mComplete();
}

SpeakerAddCommand::SpeakerAddCommand(const SpeakerAddCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool SpeakerAddCommand::processCommand(const rapidjson::Document &command)
{
    uint64_t uh = karere::Id::inval();
    rapidjson::Value::ConstMemberIterator userIterator = command.FindMember("user");
    if (userIterator != command.MemberEnd() && userIterator->value.IsString())
    {
        uh = ::mega::MegaApi::base64ToUserHandle(userIterator->value.GetString());
    }
    return mComplete(uh, true);
}

SpeakerDelCommand::SpeakerDelCommand(const SpeakerDelCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool SpeakerDelCommand::processCommand(const rapidjson::Document &command)
{
    uint64_t uh = karere::Id::inval();
    rapidjson::Value::ConstMemberIterator userIterator = command.FindMember("user");
    if (userIterator != command.MemberEnd() && userIterator->value.IsString())
    {
        uh = ::mega::MegaApi::base64ToUserHandle(userIterator->value.GetString());
    }
    return mComplete(uh, false);
}

SpeakReqCommand::SpeakReqCommand(const SpeakReqsCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool SpeakReqCommand::processCommand(const rapidjson::Document &command)
{
    uint64_t uh = karere::Id::inval();
    rapidjson::Value::ConstMemberIterator userIterator = command.FindMember("user");
    if (userIterator == command.MemberEnd() || !userIterator->value.IsString())
    {
        SFU_LOG_ERROR("SpeakReqCommand: Received data doesn't have 'user' field");
        return false;
    }

    uh = ::mega::MegaApi::base64ToUserHandle(userIterator->value.GetString());
    return mComplete(uh, true);
}

SpeakReqDelCommand::SpeakReqDelCommand(const SpeakReqDelCompleteFunction &complete, SfuInterface &call)
    : Command(call)
    , mComplete(complete)
{
}

bool SpeakReqDelCommand::processCommand(const rapidjson::Document &command)
{
    uint64_t uh = karere::Id::inval();
    rapidjson::Value::ConstMemberIterator userIterator = command.FindMember("user");
    if (userIterator == command.MemberEnd() || !userIterator->value.IsString())
    {
        SFU_LOG_ERROR("SpeakReqDelCommand: Received data doesn't have 'user' field");
        return false;
    }

    uh = ::mega::MegaApi::base64ToUserHandle(userIterator->value.GetString());
    return mComplete(uh, false);
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

    rapidjson::Value::ConstMemberIterator sfuvIterator = command.FindMember("v");
    if (sfuvIterator == command.MemberEnd() || !sfuvIterator->value.IsUint())
    {
        SFU_LOG_ERROR("PeerJoinCommand: Received data doesn't have 'v' field");
        return false;
    }

    std::string pubkeyStr;
    rapidjson::Value::ConstMemberIterator pubkeyIterator = command.FindMember("pubk");
    if (pubkeyIterator != command.MemberEnd() && pubkeyIterator->value.IsString())
    {
         // clients with SFU protocol = 0 won't send ephemeral pubkey
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
    unsigned int sfuVersion = sfuvIterator->value.GetUint();
    return mComplete(cid, userid, static_cast<sfu::SfuProtocol>(sfuVersion), av, pubkeyStr, ivs);
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

void SfuConnection::setMyCid(const Cid_t& cid)
{
    mMyCid = cid;
}

Cid_t SfuConnection::getMyCid() const
{
    return mMyCid;
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

void SfuConnection::connect()
{
    assert (mConnState == kConnNew);
    doReconnect(false /*initialBackoff*/);
}

void SfuConnection::doReconnect(const bool applyInitialBackoff)
{
    if (avoidReconnect())
    {
        SFU_LOG_DEBUG("Avoid reconnect to SFU, as we are destroying call");
        return;
    }

    auto wptr = weakHandle();
    const auto reconnectFunc = [this, wptr]()
    {
        if (wptr.deleted()) { return; }

        reconnect().fail(
            [wptr](const ::promise::Error& err)
            {
                if (wptr.deleted())
                {
                    return;
                }
                SFU_LOG_DEBUG("SfuConnection::reconnect(): Error connecting to SFU server: %s",
                              err.what());
            });
    };

    cancelConnectTimer(); // cancel connect timer in case is set

    if (!applyInitialBackoff || !getInitialBackoff())
    {
        reconnectFunc(); // start reconnection attempt immediately
    }
    else
    {
        /* A SFU connection attempt is considered as succeeded, just when client receives ANSWER command.
         * RetryController algorithm already manages failed connection attempts (adding an exponential backoff),
         * but in case LWS connection to SFU succeeded but client gets disconnected before receiving ANSWER command,
         * we also need to add a backoff to prevent hammering SFU (which triggers DDOS protection)
         */
        mConnectTimer = karere::setTimeout([this, reconnectFunc, wptr]()
        {
            if (wptr.deleted()) { return; }
            mConnectTimer = 0;
            reconnectFunc();
        }, getInitialBackoff() * 100, mAppCtx);
    }
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

    std::string urlPath = mSfuUrl.path;
    if (getMyCid() != K_INVALID_CID) // add current cid for reconnection
    {
        urlPath.append("&cid=").append(std::to_string(getMyCid()));
    }

    bool rt = wsConnect(&mWebsocketIO, mTargetIp.c_str(),
          mSfuUrl.host.c_str(),
          mSfuUrl.port,
          urlPath.c_str(),
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
                          urlPath.c_str(),
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
    const auto oldConnState = mConnState;
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
        doReconnect(oldConnState >= kConnected /*initialBackoff*/);

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
        if (isSendingByeCommand())
        {
            mCall.onByeCommandSent();
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
        setIsSendingBye(true);
    }

    assert(!command.empty());
    SFU_LOG_DEBUG("Send command: %s", command.c_str());
    std::unique_ptr<char[]> buffer(mega::MegaApi::strdup(command.c_str()));
    bool rc = wsSendMessage(buffer.get(), command.length());

    if (!rc)
    {
        mSendPromise.reject("Socket is not ready");
        if (isSendingByeCommand())
        {
             // if wsSendMessage failed inmediately trying to send BYE command, call onSendByeCommand in order to
             // execute the expected action (retry, remove or disconnect call) that triggered the BYE command sent
             mCommandsQueue.setSending(false);
             mCall.onByeCommandSent();
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
    commands[AVCommand::COMMAND_NAME] = std::make_unique<AVCommand>(std::bind(&sfu::SfuInterface::handleAvCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), call);
    commands[AnswerCommand::COMMAND_NAME] = std::make_unique<AnswerCommand>(std::bind(&sfu::SfuInterface::handleAnswerCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9, std::placeholders::_10), call);
    commands[KeyCommand::COMMAND_NAME] = std::make_unique<KeyCommand>(std::bind(&sfu::SfuInterface::handleKeyCommand, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), call);
    commands[VthumbsCommand::COMMAND_NAME] = std::make_unique<VthumbsCommand>(std::bind(&sfu::SfuInterface::handleVThumbsCommand, &call, std::placeholders::_1), call);
    commands[VthumbsStartCommand::COMMAND_NAME] = std::make_unique<VthumbsStartCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStartCommand, &call), call);
    commands[VthumbsStopCommand::COMMAND_NAME] = std::make_unique<VthumbsStopCommand>(std::bind(&sfu::SfuInterface::handleVThumbsStopCommand, &call), call);
    commands[HiResCommand::COMMAND_NAME] = std::make_unique<HiResCommand>(std::bind(&sfu::SfuInterface::handleHiResCommand, &call, std::placeholders::_1), call);
    commands[HiResStartCommand::COMMAND_NAME] = std::make_unique<HiResStartCommand>(std::bind(&sfu::SfuInterface::handleHiResStartCommand, &call), call);
    commands[HiResStopCommand::COMMAND_NAME] = std::make_unique<HiResStopCommand>(std::bind(&sfu::SfuInterface::handleHiResStopCommand, &call), call);
    commands[SpeakerAddCommand::COMMAND_NAME] = std::make_unique<SpeakerAddCommand>(std::bind(&sfu::SfuInterface::handleSpeakerAddDelCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[SpeakerDelCommand::COMMAND_NAME] = std::make_unique<SpeakerDelCommand>(std::bind(&sfu::SfuInterface::handleSpeakerAddDelCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[SpeakReqCommand::COMMAND_NAME] = std::make_unique<SpeakReqCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqAddDelCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[SpeakReqDelCommand::COMMAND_NAME] = std::make_unique<SpeakReqDelCommand>(std::bind(&sfu::SfuInterface::handleSpeakReqAddDelCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[PeerJoinCommand::COMMAND_NAME] = std::make_unique<PeerJoinCommand>(std::bind(&sfu::SfuInterface::handlePeerJoin, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6), call);
    commands[PeerLeftCommand::COMMAND_NAME] = std::make_unique<PeerLeftCommand>(std::bind(&sfu::SfuInterface::handlePeerLeft, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[ByeCommand::COMMAND_NAME] = std::make_unique<ByeCommand>(std::bind(&sfu::SfuInterface::handleBye, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), call);
    commands[ModAddCommand::COMMAND_NAME] = std::make_unique<ModAddCommand>(std::bind(&sfu::SfuInterface::handleModAdd, &call, std::placeholders::_1), call);
    commands[ModDelCommand::COMMAND_NAME] = std::make_unique<ModDelCommand>(std::bind(&sfu::SfuInterface::handleModDel, &call, std::placeholders::_1), call);
    commands[HelloCommand::COMMAND_NAME] = std::make_unique<HelloCommand>(std::bind(&sfu::SfuInterface::handleHello, &call, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8), call);
    commands[WrDumpCommand::COMMAND_NAME] = std::make_unique<WrDumpCommand>(std::bind(&sfu::SfuInterface::handleWrDump, &call, std::placeholders::_1), call);
    commands[WrEnterCommand::COMMAND_NAME] = std::make_unique<WrEnterCommand>(std::bind(&sfu::SfuInterface::handleWrEnter, &call, std::placeholders::_1), call);
    commands[WrLeaveCommand::COMMAND_NAME] = std::make_unique<WrLeaveCommand>(std::bind(&sfu::SfuInterface::handleWrLeave, &call, std::placeholders::_1), call);
    commands[WrAllowCommand::COMMAND_NAME] = std::make_unique<WrAllowCommand>(std::bind(&sfu::SfuInterface::handleWrAllow, &call, std::placeholders::_1), call);
    commands[WrDenyCommand::COMMAND_NAME] = std::make_unique<WrDenyCommand>(std::bind(&sfu::SfuInterface::handleWrDeny, &call), call);
    commands[WrUsersAllowCommand::COMMAND_NAME] = std::make_unique<WrUsersAllowCommand>(std::bind(&sfu::SfuInterface::handleWrUsersAllow, &call, std::placeholders::_1), call);
    commands[WrUsersDenyCommand::COMMAND_NAME] = std::make_unique<WrUsersDenyCommand>(std::bind(&sfu::SfuInterface::handleWrUsersDeny, &call, std::placeholders::_1), call);
    commands[MutedCommand::COMMAND_NAME] = std::make_unique<MutedCommand>(std::bind(&sfu::SfuInterface::handleMutedCommand, &call, std::placeholders::_1, std::placeholders::_2), call);
    commands[WillEndCommand::COMMAND_NAME] = std::make_unique<WillEndCommand>(std::bind(&sfu::SfuInterface::handleWillEndCommand, &call, std::placeholders::_1), call);
    commands[ClimitsCommand::COMMAND_NAME] = std::make_unique<ClimitsCommand>(std::bind(&sfu::SfuInterface::handleClimitsCommand, &call, std::placeholders::_1), call);
    commands[RaiseHandAddCommand::COMMAND_NAME] = std::make_unique<RaiseHandAddCommand>(std::bind(&sfu::SfuInterface::handleRaiseHandAddCommand, &call, std::placeholders::_1), call);
    commands[RaiseHandDelCommand::COMMAND_NAME] = std::make_unique<RaiseHandDelCommand>(std::bind(&sfu::SfuInterface::handleRaiseHandDelCommand, &call, std::placeholders::_1), call);
}

bool SfuConnection::parseSfuData(const char* data, rapidjson::Document& jsonDoc, SfuData& parsedData)
{
    SFU_LOG_DEBUG("Data received: %s", data);
    rapidjson::StringStream stringStream(data);
    jsonDoc.ParseStream(stringStream);

    if (jsonDoc.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        parsedData.msg = "Failure at: Parser json error";
        return false;
    }

    // command received {"a": "command", ...}
    rapidjson::Value::ConstMemberIterator jsonCommandIterator = jsonDoc.FindMember(Command::COMMAND_IDENTIFIER.c_str());
    if (jsonCommandIterator != jsonDoc.MemberEnd() && jsonCommandIterator->value.IsString())
    {
        parsedData.notificationType = SfuData::SFU_COMMAND;
        parsedData.notification = jsonCommandIterator->value.GetString();
        return true;
    }

    // warn received {"warn": "message"}
    rapidjson::Value::ConstMemberIterator jsonWarnIterator = jsonDoc.FindMember(Command::WARN_IDENTIFIER.c_str());
    if (jsonWarnIterator != jsonDoc.MemberEnd() && jsonWarnIterator->value.IsString())
    {
        parsedData.notificationType = SfuData::SFU_WARN;
        parsedData.msg = jsonWarnIterator->value.GetString();
        return true;
    }

    // deny received {"deny": "command", "msg": "message"}
    rapidjson::Value::ConstMemberIterator jsonDenyIterator = jsonDoc.FindMember(Command::DENY_IDENTIFIER.c_str());
    if (jsonDenyIterator != jsonDoc.MemberEnd() && jsonDenyIterator->value.IsString())
    {
        parsedData.notificationType = SfuData::SFU_DENY;
        parsedData.notification = jsonDenyIterator->value.GetString();

        rapidjson::Value::ConstMemberIterator jsonMsgIterator = jsonDoc.FindMember("msg");
        if (jsonMsgIterator != jsonDoc.MemberEnd() && jsonMsgIterator->value.IsString())
        {
            parsedData.msg = jsonMsgIterator->value.GetString();
        }
        return true;
    }

    // err received {"err": "errCode", "msg": "message"}
    rapidjson::Value::ConstMemberIterator jsonErrIterator = jsonDoc.FindMember(Command::ERROR_IDENTIFIER.c_str());
    if (jsonErrIterator != jsonDoc.MemberEnd() && jsonErrIterator->value.IsInt())
    {
        parsedData.notificationType = SfuData::SFU_ERROR;
        parsedData.errCode = jsonErrIterator->value.GetInt();
        parsedData.msg = "Unknown reason";

        rapidjson::Value::ConstMemberIterator jsonMsgIterator = jsonDoc.FindMember("msg");
        if (jsonMsgIterator != jsonDoc.MemberEnd() && jsonMsgIterator->value.IsString())
        {
            parsedData.msg = jsonMsgIterator->value.GetString();
        }

        return true;
    }

    assert(false);
    parsedData.msg = "Invalid Received data doesn't match without any of expected SFU notifications format ('a'/'err'/'warn')";
    return false;
}

bool SfuConnection::handleIncomingData(const char *data, size_t len)
{
    rapidjson::Document jsonDoc;
    SfuData outdata;
    if (!parseSfuData(data, jsonDoc, outdata))
    {
        // error parsing incoming data from SFU
        SFU_LOG_ERROR("%s", outdata.msg.c_str());
        return false;
    }

    switch (outdata.notificationType)
    {
        case SfuData::SFU_ERROR:
                mCall.error(static_cast<unsigned int>(outdata.errCode), outdata.msg);
                break;
        case SfuData::SFU_WARN:
                SFU_LOG_WARNING("%s", outdata.msg.c_str());
                break;
        case SfuData::SFU_DENY:
                mCall.processDeny(outdata.notification, outdata.msg);
                break;
        case SfuData::SFU_COMMAND: {
                const std::string& command = outdata.notification;
                auto commandIterator = mCommands.find(command);
                if (commandIterator == mCommands.end())
                {
                    SFU_LOG_ERROR("Command is not defined yet");
                    return false;
                }

                SFU_LOG_DEBUG("Received Command: %s, Bytes: %lu", command.c_str(), len);
                bool processCommandResult = mCommands[command]->processCommand(jsonDoc);

                if (!processCommandResult)
                {
                    SFU_LOG_WARNING("Error processing command: %s");
                }
                else if (command == AnswerCommand::COMMAND_NAME)
                {
                    setConnState(SfuConnection::kJoined);
                }

                return processCommandResult;
                }
        default: {
                assert (false);
                SFU_LOG_ERROR("Invalid data received from SFU");
                return false;
                }
    }

    return true;
}

bool SfuConnection::joinSfu(const Sdp &sdp, const std::map<std::string, std::string> &ivs,
                            std::string& ephemeralKey, int avFlags, Cid_t prevCid, int vthumbs, const bool hasRaisedHand)

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

    if (hasRaisedHand)
    {
        json.AddMember("rh", 1, json.GetAllocator());
    }

    if (prevCid != K_INVALID_CID)
    {
        // when reconnecting, send the SFU the CID of the previous connection, so it can kill it instantly
        json.AddMember("cid", prevCid, json.GetAllocator());
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

bool SfuConnection::sendSpeakerAddDel(const karere::Id& user, const bool add)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    const std::string& cmd = add ? SfuConnection::CSFU_SPEAKER_ADD.c_str() : SfuConnection::CSFU_SPEAKER_DEL.c_str();
    cmdValue.SetString(cmd.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    if (user.isValid())
    {
        rapidjson::Value auxValue(rapidjson::kStringType);
        auxValue.SetString(user.toString().c_str(), static_cast<rapidjson::SizeType>(user.toString().length()), json.GetAllocator());
        json.AddMember(rapidjson::Value("user"), auxValue, json.GetAllocator());
    }
    // else => own user

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::raiseHandToSpeak(const bool add)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    const std::string& cmd = add ? SfuConnection::CSFU_RHAND_ADD : SfuConnection::CSFU_RHAND_DEL;
    cmdValue.SetString(cmd.c_str(), json.GetAllocator());
    json.AddMember(
        rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(),
                         static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())),
        cmdValue,
        json.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendSpeakReqAddDel(const karere::Id& user, const bool add)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    const std::string& cmd = add ? SfuConnection::CSFU_SPEAKRQ.c_str() : SfuConnection::CSFU_SPEAKRQ_DEL.c_str();
    cmdValue.SetString(cmd.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    if (user.isValid())
    {
        rapidjson::Value auxValue(rapidjson::kStringType);
        auxValue.SetString(user.toString().c_str(), static_cast<rapidjson::SizeType>(user.toString().length()), json.GetAllocator());
        json.AddMember(rapidjson::Value("user"), auxValue, json.GetAllocator());
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

void SfuConnection::clearInitialBackoff()       { mInitialBackoff = 0; }
void SfuConnection::incrementInitialBackoff()   { ++mInitialBackoff; }
unsigned int SfuConnection::getInitialBackoff() const
{
    // returns initial backoff in milliseconds
    if (!mInitialBackoff)                     { return mInitialBackoff; }
    if (mInitialBackoff >= maxInitialBackoff) { return maxInitialBackoff; }
    return 10 * mInitialBackoff;
}


bool SfuConnection::sendWrCommand(const std::string& commandStr, const std::set<karere::Id>& users, const bool all)
{
    if (users.empty() && !all)
    {
        SFU_LOG_WARNING("%s: invalid arguments provided", commandStr.c_str());
        assert(false);
        return false;
    }

    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(commandStr.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());
    addWrUsersArray(users, all, json);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendWrPush(const std::set<karere::Id>& users, const bool all)
{
    return sendWrCommand(SfuConnection::CSFU_WR_PUSH, users, all);
}

bool SfuConnection::sendWrAllow(const std::set<karere::Id>& users, const bool all)
{
    return sendWrCommand(SfuConnection::CSFU_WR_ALLOW, users, all);
}

bool SfuConnection::sendWrKick(const std::set<karere::Id>& users)
{
    return sendWrCommand(SfuConnection::CSFU_WR_KICK, users);
}

bool SfuConnection::sendSetLimit(const uint32_t callDurSecs, const uint32_t numUsers, const uint32_t numClientsPerUser, const uint32_t numClients, const uint32_t divider)
{
    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_SETLIMIT.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    if (callDurSecs != callLimitNotPresent)
    {
        const double callDurMin = callDurSecs != callLimitReset ? static_cast<double>(callDurSecs) / 60.0 : static_cast<double>(callLimitReset);
        rapidjson::Value durVal(rapidjson::kNumberType);
        durVal.SetDouble(callDurMin);
        json.AddMember(rapidjson::Value("dur"), durVal, json.GetAllocator());
    }

    if (numUsers != callLimitNotPresent)
    {
        rapidjson::Value numUsersVal(rapidjson::kNumberType);
        numUsersVal.SetUint(static_cast<unsigned int>(numUsers));
        json.AddMember(rapidjson::Value("usr"), numUsersVal, json.GetAllocator());
    }

    if (numClients != callLimitNotPresent)
    {
        rapidjson::Value numClientsVal(rapidjson::kNumberType);
        numClientsVal.SetUint(static_cast<unsigned int>(numClients));
        json.AddMember(rapidjson::Value("clnt"), numClientsVal, json.GetAllocator());
    }

    if (numClientsPerUser != callLimitNotPresent && numClientsPerUser <= callLimitUsersPerClient)
    {
        rapidjson::Value numClientsPerUserVal(rapidjson::kNumberType);
        numClientsPerUserVal.SetUint(static_cast<unsigned int>(numClientsPerUser));
        json.AddMember(rapidjson::Value("uclnt"), numClientsPerUserVal, json.GetAllocator());
    }

    if (divider != callLimitNotPresent)
    {
        rapidjson::Value numClientsVal(rapidjson::kNumberType);
        numClientsVal.SetUint(static_cast<unsigned int>(divider));
        json.AddMember(rapidjson::Value("div"), numClientsVal, json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::sendMute(const Cid_t& cid, const unsigned av)
{
    if (av != karere::AvFlags::kAudio)
    {
        // remove this checkup when video mute is implemented by SFU
        SFU_LOG_WARNING("sendMute: Av flags not supported by SFU for MUTE command");
        assert(false);
        return false;
    }

    rapidjson::Document json(rapidjson::kObjectType);
    rapidjson::Value cmdValue(rapidjson::kStringType);
    cmdValue.SetString(SfuConnection::CSFU_MUTE.c_str(), json.GetAllocator());
    json.AddMember(rapidjson::Value(Command::COMMAND_IDENTIFIER.c_str(), static_cast<rapidjson::SizeType>(Command::COMMAND_IDENTIFIER.length())), cmdValue, json.GetAllocator());

    rapidjson::Value avValue(rapidjson::kNumberType);
    avValue.SetUint(av);
    json.AddMember(rapidjson::Value("av"), avValue, json.GetAllocator());

    if (cid != K_INVALID_CID)
    {
        json.AddMember("cid", cid, json.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    std::string command(buffer.GetString(), buffer.GetSize());
    return sendCommand(command);
}

bool SfuConnection::avoidReconnect() const
{
    return mAvoidReconnect;
}

void SfuConnection::setAvoidReconnect(const bool avoidReconnect)
{
    mAvoidReconnect = avoidReconnect;
}

bool SfuConnection::addWrUsersArray(const std::set<karere::Id>& users, const bool all, rapidjson::Document& json)
{
    if (users.empty())
    {
        if (!all)
        {
            SFU_LOG_WARNING("addWrUsersArray: empty user list and all param is false");
            assert(false);
            return false;
        }
        const std::string userStr = "*";
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

void SfuConnection::cancelConnectTimer()
{
    if (mConnectTimer)
    {
        karere::cancelTimeout(mConnectTimer, mAppCtx);
        mConnectTimer = 0;
    }
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
        cancelConnectTimer();

        // start a timer to ensure the connection is established after kConnectTimeout. Otherwise, reconnect
        auto wptr = weakHandle();
        mConnectTimer = karere::setTimeout([this, wptr]()
        {
            if (wptr.deleted())
                return;

            SFU_LOG_DEBUG("Reconnection attempt has not succeed after %u. Reconnecting...", kConnectTimeout);
            mConnectTimer = 0;
            retryPendingConnection(true);
        }, kConnectTimeout * 1000, mAppCtx);
    }
    else if (mConnState == kConnected)
    {
        SFU_LOG_DEBUG("Sfu connected to %s", mTargetIp.c_str());
        /* Increment InitialBackoff as we have completed LWS conenction to SFU, but this connection attempt
         * can't be considered succeeded, until we have received ANSWER command.
         * In case of disconnection before receiving ANSWER command, next connection attempt will have a backoff to avoid hammering SFU
         */
        incrementInitialBackoff();
        mDnsCache.connectDoneByHost(mSfuUrl.host, mTargetIp);
        assert(!mConnectPromise.done());
        mConnectPromise.resolve();
        mRetryCtrl.reset();
        cancelConnectTimer();  // cancel connect timer in case is set
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
            doReconnect(false /*initialBackoff*/); // start retry controller
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
        doReconnect(true /*initialBackoff*/); //start retry controller
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
        mRetryCtrl.reset(createRetryController("sfu", [this](size_t attemptId, DeleteTrackable::Handle wptr) -> promise::Promise<void>
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
                         [wptr, cachedIpsByHost, this, retryCtrl, attemptId, ipv4, ipv6](int statusDNS, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
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
                        assert(mDnsCache.getRecordByHost(mSfuUrl.host) != nullptr);
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
                if (mRetryCtrl->currentAttemptId() != attemptId)
                {
                    SFU_LOG_DEBUG("DNS resolution completed but ignored: a newer attempt is already started (old: %lu, new: %lu)",
                                     attemptId, mRetryCtrl->currentAttemptId());
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
    : mRtcCryptoMeetings(rRtcCryptoMeetings)
    , mWebsocketIO(websocketIO)
    , mAppCtx(appCtx)
{

}

SfuConnection* SfuClient::createSfuConnection(const karere::Id& chatid, karere::Url&& sfuUrl, SfuInterface &call, DNScache &dnsCache)
{
    assert(mConnections.find(chatid) == mConnections.end());
    mConnections[chatid] = std::make_unique<SfuConnection>(std::move(sfuUrl), mWebsocketIO, mAppCtx, call, dnsCache);
    SfuConnection* sfuConnection = mConnections[chatid].get();
    sfuConnection->connect();
    return sfuConnection;
}

void SfuClient::closeSfuConnection(const karere::Id& chatid)
{
    mConnections[chatid]->disconnect();
    mConnections.erase(chatid);
}

std::shared_ptr<rtcModule::RtcCryptoMeetings> SfuClient::getRtcCryptoMeetings()
{
    return mRtcCryptoMeetings;
}

void SfuClient::addVersionToUrl(karere::Url& sfuUrl, const sfu::SfuProtocol sfuVersion)
{
    std::string app;
    if (sfuUrl.path.back() != '?')  // if last URL char is '?' just add version, otherwise:
    {
        app = sfuUrl.path.find("?") != std::string::npos
                 ? "&"  // url already has parameters
                 : "?"; // add ? as append character
    }

    sfuUrl.path.append(app).append("v=").append(std::to_string(static_cast<unsigned int>(sfuVersion)));
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

    bool wr = false;
    rapidjson::Value::ConstMemberIterator wrIterator = command.FindMember("wr");
    if (wrIterator != command.MemberEnd() && wrIterator->value.IsUint())
    {
        wr = wrIterator->value.GetUint();
    }

    std::string errMsg = "Unknown reason";
    rapidjson::Value::ConstMemberIterator jsonErrMsgIterator = command.FindMember(Command::ERROR_IDENTIFIER.c_str());
    if (jsonErrMsgIterator != command.MemberEnd() && jsonErrMsgIterator->value.IsString())
    {
        errMsg = jsonErrMsgIterator->value.GetString();
    }

    return mComplete(reasonIterator->value.GetUint() /*termcode */, wr, errMsg);
}

MutedCommand::MutedCommand(const MutedCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool MutedCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator avIterator = command.FindMember("av");
    if (avIterator == command.MemberEnd() || !avIterator->value.IsInt())
    {
        SFU_LOG_ERROR("Received data doesn't have 'av' field");
        return false;
    }
    unsigned av = avIterator->value.GetUint();

    Cid_t cidPerf = K_INVALID_CID;
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("by");
    if (cidIterator != command.MemberEnd() && cidIterator->value.IsUint())
    {
        cidPerf = cidIterator->value.GetUint();  // optional
    }
    return mComplete(av, cidPerf);
}


WillEndCommand::WillEndCommand(const WillEndCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WillEndCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator inIterator = command.FindMember("in");
    if (inIterator == command.MemberEnd() || !inIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'in' field");
        assert(false);
        return false;
    }
    unsigned int in = inIterator->value.GetUint();
    return mComplete(in);
}

ClimitsCommand::ClimitsCommand(const ClimitsCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool ClimitsCommand::processCommand(const rapidjson::Document& command)
{
    std::optional<SfuInterface::CallLimits> callLimitsOpt = buildCallLimits(command);
    if (!callLimitsOpt)
    {
        return false;
    }
    return mComplete(*callLimitsOpt);
}

RaiseHandAddCommand::RaiseHandAddCommand(const RaiseHandAddCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool RaiseHandAddCommand::processCommand(const rapidjson::Document& command)
{
    // if user no present, this command is about own user (provide Id::null)
    return mComplete(parseHandle(command, "user", karere::Id::null().val));
}

RaiseHandDelCommand::RaiseHandDelCommand(const RaiseHandDelCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool RaiseHandDelCommand::processCommand(const rapidjson::Document& command)
{
    // if user no present, this command is about own user (provide Id::null)
    return mComplete(parseHandle(command, "user", karere::Id::null().val));
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
        SFU_LOG_ERROR("HelloCommand: Received data doesn't have 'cid' field");
        assert(false);
        return false;
    }
    Cid_t cid = cidIterator->value.GetUint();

    rapidjson::Value::ConstMemberIterator srIterator = command.FindMember("sr");
    bool speakRequest = false;
    if (srIterator != command.MemberEnd() && srIterator->value.IsUint())
    {
        speakRequest = cidIterator->value.GetUint();
    }

    std::optional<SfuInterface::CallLimits> callLimitsOpt = parseCallLimits(command);
    if (!callLimitsOpt)
    {
        return false;
    }

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

    // parse moderators list
    std::set<karere::Id> moderators;
    rapidjson::Value::ConstMemberIterator modsIterator = command.FindMember("mods");
    if (modsIterator != command.MemberEnd() && modsIterator->value.IsArray())
    {
        parseUsersArray(moderators, modsIterator);
    }

    bool wr = false;
    bool allowed = false;
    sfu::WrUserList wrUserList;
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

        if (!parseWrUsersMap(wrUserList, obj))
        {
            assert(false);
            SFU_LOG_ERROR("HelloCommand: users array in wr is ill-formed");
            return false;
        }
    }
    return mComplete(cid, nAudioTracks, moderators, wr, allowed, speakRequest, wrUserList, *callLimitsOpt);
}

WrDumpCommand::WrDumpCommand(const WrDumpCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrDumpCommand::processCommand(const rapidjson::Document& command)
{
    sfu::WrUserList wrUsers;
    if (!parseWrUsersMap(wrUsers, command.GetObject()))
    {
        SFU_LOG_ERROR("WrDumpCommand: users array is ill-formed");
        assert(false);
        return false;
    }
    return mComplete(wrUsers);
}

WrEnterCommand::WrEnterCommand(const WrEnterCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrEnterCommand::processCommand(const rapidjson::Document& command)
{
    sfu::WrUserList wrUsers;
    if (!parseWrUsersMap(wrUsers, command.GetObject()))
    {
        SFU_LOG_ERROR("WrEnterCommand: users array is ill-formed");
        assert(false);
        return false;
    }
    return mComplete(wrUsers);
}

WrLeaveCommand::WrLeaveCommand(const WrLeaveCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrLeaveCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator reasonIterator = command.FindMember("user");
    if (reasonIterator == command.MemberEnd() || !reasonIterator->value.IsString())
    {
        SFU_LOG_ERROR("WrLeaveCommand: Received data doesn't have 'user' field");
        assert(false);
        return false;
    }
    std::string userIdString = reasonIterator->value.GetString();
    ::mega::MegaHandle userId = ::mega::MegaApi::base64ToUserHandle(userIdString.c_str());
    return mComplete(userId /*userid*/);
}

WrAllowCommand::WrAllowCommand(const WrAllowCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrAllowCommand::processCommand(const rapidjson::Document& command)
{
    rapidjson::Value::ConstMemberIterator cidIterator = command.FindMember("cid");
    if (cidIterator == command.MemberEnd() || !cidIterator->value.IsUint())
    {
        SFU_LOG_ERROR("Received data doesn't have 'cid' field");
        assert(false);
        return false;
    }
    Cid_t cid = cidIterator->value.GetUint();
    return mComplete(cid);
}

WrDenyCommand::WrDenyCommand(const WrDenyCommandFunction& complete, SfuInterface& call)
    : Command(call)
    , mComplete(complete)
{
}

bool WrDenyCommand::processCommand(const rapidjson::Document&)
{
    return mComplete();
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
        assert(false);
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
        assert(false);
        return false;
    }

    parseUsersArray(users, usersIterator);
    return mComplete(users);
}
}
#endif
