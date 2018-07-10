#include "webrtc.h"
#include "webrtcPrivate.h"
#include <chatClient.h>
#include <timers.hpp>
#include "rtcCrypto.h"
#include "streamPlayer.h"
#include "rtcStats.h"

#include <regex>

#define SUB_LOG_DEBUG(fmtString,...) RTCM_LOG_DEBUG("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_INFO(fmtString,...) RTCM_LOG_INFO("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_WARNING(fmtString,...) RTCM_LOG_WARNING("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_ERROR(fmtString,...) RTCM_LOG_ERROR("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)
#define SUB_LOG_EVENT(fmtString,...) RTCM_LOG_EVENT("%s: " fmtString, mName.c_str(), ##__VA_ARGS__)

#define CONCAT(a, b) a ## b
#define FIRE_EVENT(type, evName,...)                 \
do {                                                 \
    std::string msg = "event " #evName;      \
    if (strcmp(#evName, "onDestroy") == 0) {         \
        msg.append(" (").append(termCodeFirstArgToString(__VA_ARGS__)) += ')'; \
    }                                                \
    SUB_LOG_DEBUG("%s", msg.c_str());                \
    try                                              \
    {                                                \
        mHandler->evName(__VA_ARGS__);               \
    } catch (std::exception& e) {                    \
        RTCM_LOG_ERROR("%s event handler for '" #evName "' threw exception:\n %s", #type, e.what()); \
    }                                                \
} while(0)
namespace rtcModule
{
using namespace karere;
using namespace std;
using namespace promise;
using namespace chatd;

template <class... Args>
const char* termCodeFirstArgToString(TermCode code, Args...)
{
    return termCodeToStr(code);
}
template <class... Args>
const char* termCodeFirstArgToString(Args...) { return nullptr; }
const char* iceStateToStr(webrtc::PeerConnectionInterface::IceConnectionState);
void setConstraint(webrtc::FakeConstraints& constr, const string &name, const std::string& value,
    bool optional);

struct CallerInfo
{
    Id chatid;
    Id callid;
    chatd::Chat& chat;
    Id callerUser;
    uint32_t callerClient;
};
enum { kErrSetSdp = 0x3e9a5d9e }; //megasdpe
RtMessage::RtMessage(chatd::Chat &aChat, const StaticBuffer& msg)
    :chat(aChat), opcode(msg.read<uint8_t>(0)),
    type(msg.read<uint8_t>(kHdrLen)), chatid(msg.read<uint64_t>(1)),
    userid(msg.read<uint64_t>(9)), clientid(msg.read<uint32_t>(17)),
    payload(nullptr, 0)
{
    auto packetLen = msg.read<uint16_t>(RtMessage::kHdrLen-2)-1;
    payload.assign(msg.readPtr(RtMessage::kPayloadOfs, packetLen), packetLen);
}

RtcModule::RtcModule(karere::Client& client, IGlobalHandler& handler,
  IRtcCrypto* crypto, const char* iceServers)
: IRtcModule(client, handler, crypto, crypto->anonymizeId(client.myHandle())),
  mIceServerProvider(client.api, "turn"),
  mStaticIceSever(iceServers)
{
    if (!artc::isInitialized())
    {
        artc::init(client.appCtx);
        RTCM_LOG_DEBUG("WebRTC stack initialized before first use");
    }
    mPcConstraints.SetMandatoryReceiveAudio(true);
    mPcConstraints.SetMandatoryReceiveVideo(true);
    mPcConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);

  //preload ice servers to make calls faster
    initInputDevices();
}

void RtcModule::init()
{
    StaticProvider iceServerStatic(mStaticIceSever);
    setIceServers(iceServerStatic);
    auto wptr = weakHandle();
    mIceServerProvider.fetchServers()
    .then([wptr, this]()
    {
        if (wptr.deleted())
            return;
        setIceServers(mIceServerProvider);
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Gelb failed with error '%s', using static server list", err.what());
    });

    mClient.chatd->setRtcHandler(this);
}

IRtcModule* create(karere::Client &client, IGlobalHandler &handler, IRtcCrypto* crypto, const char* iceServers)
{
    return new RtcModule(client, handler, crypto, iceServers);
}

template <class T>
T RtcModule::random() const
{
    T result;
    crypto().random((char*)&result, sizeof(result));
    return result;
}
template <class T>
void RtcModule::random(T& result) const
{
    return crypto().random((char*)&result, sizeof(T));
}

void RtcModule::initInputDevices()
{
    auto& devices = mDeviceManager.inputDevices();
    if (!devices.audio.empty())
        selectAudioInDevice(devices.audio[0].name);
    if (!devices.video.empty())
        selectVideoInDevice(devices.video[0].name);
    RTCM_LOG_INFO("Input devices on this system:");
    for (const auto& dev: devices.audio)
        RTCM_LOG_INFO("\tAudio: %s [id=%s]", dev.name.c_str(), dev.id.c_str());
    for (const auto& dev: devices.video)
        RTCM_LOG_INFO("\tVideo: %s [id=%s]", dev.name.c_str(), dev.id.c_str());
}
const cricket::Device* RtcModule::getDevice(const string& name, const artc::DeviceList& devices)
{
    for (size_t i=0; i<devices.size(); i++)
    {
        auto device = &devices[i];
        if (device->name == name)
            return device;
    }
    return nullptr;
}

bool RtcModule::selectDevice(const std::string& devname,
            const artc::DeviceList& devices, string& selected)
{
    if (devices.empty())
    {
        selected.clear();
        return devname.empty();
    }
    if (devname.empty())
    {
        selected = devices[0].name;
        return true;
    }

    if (!getDevice(devname, devices))
    {
        selected = devices[0].name;
        return false;
    }
    else
    {
        selected = devname;
        return true;
    }
}

bool RtcModule::selectAudioInDevice(const string &devname)
{
    return selectDevice(devname, mDeviceManager.inputDevices().audio, mAudioInDeviceName);
}

void RtcModule::loadDeviceList()
{
    mDeviceManager.enumInputDevices();
}
bool RtcModule::selectVideoInDevice(const string &devname)
{
    return selectDevice(devname, mDeviceManager.inputDevices().video, mVideoInDeviceName);
}

void RtcModule::onDisconnect(chatd::Connection& conn)
{
    // notify all relevant calls
    for (auto chatid: conn.chatIds())
    {
        auto it = mCalls.find(chatid);
        if (it == mCalls.end())
        {
            continue;
        }
        auto& call = it->second;
        if (call->state() < Call::kStateTerminating)
        {
            call->destroy(TermCode::kErrNetSignalling, false);
        }
    }
}

int RtcModule::setIceServers(const ServerList &servers)
{
    if (servers.empty())
        return 0;

    webrtc::PeerConnectionInterface::IceServers rtcServers;
    for (auto& server: servers)
    {
        webrtc::PeerConnectionInterface::IceServer rtcServer;
        rtcServer.uri = server->url;        

        if (!server->user.empty())
            rtcServer.username = server->user;
        else
            rtcServer.username = KARERE_TURN_USERNAME;

        if (!server->pass.empty())
            rtcServer.password = server->pass;
        else
            rtcServer.password = KARERE_TURN_PASSWORD;

        KR_LOG_DEBUG("Adding ICE server: '%s'", rtcServer.uri.c_str());
        rtcServers.push_back(rtcServer);
    }

    mIceServers.swap(rtcServers);
    return (int)(mIceServers.size());
}

void RtcModule::handleMessage(chatd::Chat& chat, const StaticBuffer& msg)
{
    // (opcode.1 chatid.8 userid.8 clientid.4 len.2) (type.1 data.(len-1))
    //              ^                                          ^
    //          header.hdrlen                             payload.len
    try
    {
        RtMessage packet(chat, msg);
        auto it = mCalls.find(packet.chatid);
        if (it == mCalls.end())
        {
            RTCM_LOG_WARNING("Received %s for a chat that doesn't currently have a call, ignoring",
                packet.typeStr());
            return;
        }
        it->second->handleMessage(packet);
    }
    catch (std::exception& e)
    {
        RTCM_LOG_ERROR("rtMsgHandler: exception: %s", e.what());
    }
}

void RtcModule::handleCallData(Chat &chat, Id chatid, Id userid, uint32_t clientid, const StaticBuffer &msg)
{
    // this is the only command that is not handled by an existing call
    // PayLoad: <callid> <state> <AV flags> [if (state == kCallDataEnd) <termCode>]
    karere::Id callid = msg.read<karere::Id>(0);
    uint8_t state = msg.read<uint8_t>(sizeof(karere::Id));
    AvFlags avFlagsRemote = msg.read<uint8_t>(sizeof(karere::Id) + sizeof(uint8_t));

    if (userid == chat.client().karereClient->myHandle()
        && clientid == chat.connection().clientId())
    {
        RTCM_LOG_WARNING("Ignoring CALLDATA sent back to sender");
        return;
    }

    updatePeerAvState(chatid, callid, userid, clientid, avFlagsRemote);
    if (state == Call::CallDataState::kCallDataRinging)
    {
        handleCallDataRequest(chat, userid, clientid, callid, avFlagsRemote);
    }
    else if (state == Call::CallDataState::kCallDataInProgress)
    {
        auto itCall = mCalls.find(chatid);
        if (itCall == mCalls.end())
        {
            RTCM_LOG_DEBUG("Ingoring kCallDataInProgress CALLDATA for unknown call");
            return;
        }

        if (chat.isGroup() && itCall->second->state() == Call::kStateRingIn)
        {
            itCall->second->destroy(TermCode::kAnswerTimeout, false);
        }
    }
    else if (state == Call::CallDataState::kCallDataJoin &&
             userid == chat.client().karereClient->myHandle() &&
             clientid != chat.connection().clientId())
    {
        auto itCall = mCalls.find(chatid);
        if (itCall != mCalls.end() && itCall->second->state() == Call::kStateRingIn)
        {
            itCall->second->destroy(TermCode::kAnsElsewhere, false);
        }
    }
}

template <class... Args>
void RtcModule::cmdEndpoint(uint8_t type, const RtMessage& info, Args... args)
{
    cmdEndpoint(info.chat, type, info.chatid, info.userid, info.clientid, args...);
}

template <class... Args>
void RtcModule::cmdEndpoint(chatd::Chat &chat, uint8_t type, Id chatid, Id userid, uint32_t clientid, Args... args)
{
    assert(chatid);
    assert(userid);
    assert(clientid);
    RtMessageComposer msg(OP_RTMSG_ENDPOINT, type, chatid, userid, clientid);
    msg.payloadAppend(args...);
    if (!chat.sendCommand(std::move(msg)))
    {
        RTCM_LOG_ERROR("cmdEndpoint: Send error trying to send command OP_RTMSG_ENDPOINT");
    }
}

void RtcModule::removeCall(Call& call)
{
    auto chatid = call.mChat.chatId();
    auto it = mCalls.find(chatid);
    if (it == mCalls.end())
    {
        RTCM_LOG_WARNING("Call with chatid %s not found", chatid.toString().c_str());
        return;
    }
    
    if (&call != it->second.get() || it->second->id() != call.id()) {
        RTCM_LOG_DEBUG("removeCall: Call has been replaced, not removing");
        return;
    }
    mCalls.erase(chatid);
}
std::shared_ptr<artc::LocalStreamHandle>
RtcModule::getLocalStream(AvFlags av, std::string& errors)
{
    const auto& devices = mDeviceManager.inputDevices();
    if (devices.video.empty() || mVideoInDeviceName.empty())
    {
        mVideoInput.reset();
    }
    else if (!mVideoInput || mVideoInput.mediaOptions().device.name != mVideoInDeviceName)
    try
    {
        auto device = getDevice(mVideoInDeviceName, devices.video);
        if (!device)
        {
            device = &devices.video[0];
            errors.append("Configured video input device '").append(mVideoInDeviceName)
                  .append("' not present, using default device\n");
        }
        auto opts = std::make_shared<artc::MediaGetOptions>(*device, mMediaConstraints);

        mVideoInput = mDeviceManager.getUserVideo(opts);
    }
    catch(exception& e)
    {
        mVideoInput.reset();
        errors.append("Error getting video device: ")
              .append(e.what()?e.what():"Unknown error")+='\n';
    }

    if (devices.audio.empty() || mAudioInDeviceName.empty())
    {
        mAudioInput.reset();
    }
    else if (!mAudioInput || mAudioInput.mediaOptions().device.name != mAudioInDeviceName)
    try
    {
        auto device = getDevice(mAudioInDeviceName, devices.audio);
        if (!device)
        {
            errors.append("Configured audio input device '").append(mAudioInDeviceName)
                  .append("' not present, using default device\n");
            device = &devices.audio[0];
        }
        mAudioInput = mDeviceManager.getUserAudio(
                std::make_shared<artc::MediaGetOptions>(*device, mMediaConstraints));
    }
    catch(exception& e)
    {
        mAudioInput.reset();
        errors.append("Error getting audio device: ")
              .append(e.what()?e.what():"Unknown error")+='\n';
    }
    if (!mAudioInput && !mVideoInput)
        return std::make_shared<artc::LocalStreamHandle>(nullptr, nullptr);

    std::shared_ptr<artc::LocalStreamHandle> localStream =
        std::make_shared<artc::LocalStreamHandle>(
            mAudioInput?mAudioInput.getTrack():nullptr,
            mVideoInput?mVideoInput.getTrack():nullptr);
    localStream->setAv(av);
    return localStream;
}
void RtcModule::getAudioInDevices(std::vector<std::string>& devices) const
{
    for (auto& dev:mDeviceManager.inputDevices().audio)
        devices.push_back(dev.name);
}

void RtcModule::getVideoInDevices(std::vector<std::string>& devices) const
{
    for(auto& dev:mDeviceManager.inputDevices().video)
        devices.push_back(dev.name);
}

std::shared_ptr<Call> RtcModule::startOrJoinCall(karere::Id chatid, AvFlags av,
    ICallHandler& handler, Id callid)
{
    karere::Id id = callid.isValid() ? callid : (karere::Id)random<uint64_t>();

    auto& chat = mClient.chatd->chats(chatid);
    auto callIt = mCalls.find(chatid);
    if (callIt != mCalls.end())
    {
        RTCM_LOG_WARNING("There is already a call in this chatroom, destroying it");
        callIt->second->hangup();
        mCalls.erase(callIt);
    }
    auto call = std::make_shared<Call>(*this, chat, id,
        chat.isGroup(), callid.isValid(), &handler, 0, 0);

    mCalls[chatid] = call;
    handler.setCall(call.get());
    call->startOrJoin(av);
    return call;
}
bool RtcModule::isCaptureActive() const
{
    return (mAudioInput || mVideoInput);
}

ICall& RtcModule::joinCall(karere::Id chatid, AvFlags av, ICallHandler& handler, karere::Id callid)
{
    return *startOrJoinCall(chatid, av, handler, callid);
}
ICall& RtcModule::startCall(karere::Id chatid, AvFlags av, ICallHandler& handler)
{
    return *startOrJoinCall(chatid, av, handler);
}

void RtcModule::onClientLeftCall(Id chatid, Id userid, uint32_t clientid)
{
    auto itCall = mCalls.find(chatid);
    if (itCall != mCalls.end())
    {
        itCall->second->onClientLeftCall(userid, clientid);
    }

    auto itHandler = mCallHandlers.find(chatid);
    if (itHandler != mCallHandlers.end())
    {
        bool isCallEmpty = itHandler->second->removeParticipant(userid, clientid);
        if (isCallEmpty)
        {
            removeCall(chatid);
        }
    }
    else
    {
        RTCM_LOG_DEBUG("onClientLeftCall: Try to remove a peer from a call and there isn't a call in the chatroom (%s)", chatid.toString().c_str());
    }
}

void RtcModule::onShutdown()
{
    RTCM_LOG_DEBUG("Shutting down....");
    hangupAll(TermCode::kAppTerminating);
    RTCM_LOG_DEBUG("Shutdown complete");
}

void RtcModule::hangupAll(TermCode code)
{
    for (auto callIt = mCalls.begin(); callIt != mCalls.end();)
    {
        auto& call = callIt->second;
        callIt++;
        
        if (call->state() == Call::kStateRingIn)
        {
            assert(call->mSessions.empty());
        }

        call->hangup(code);
    }
}

void RtcModule::stopCallsTimers(int shard)
{
    for (auto callIt = mCalls.begin(); callIt != mCalls.end();)
    {
        auto& call = callIt->second;
        callIt++;

        if (call->chat().connection().shardNo() == shard)
        {
            call->stopIncallPingTimer(false);
        }
    }
}

template <class... Args>
void RtcModule::sendCommand(Chat &chat, uint8_t opcode, uint8_t command, Id chatid, Id userid, uint32_t clientid, Args... args)
{
    RtMessageComposer message(opcode, command, chatid, userid, clientid);
    message.payloadAppend(args...);
    if (!chat.sendCommand(std::move(message)))
    {
        RTCM_LOG_ERROR("cmdEndpoint: Send error trying to send command: RTCMD_CALL_REQ_DECLINE");
    }
    return;
}
void RtcModule::setMediaConstraint(const string& name, const string &value, bool optional)
{
    rtcModule::setConstraint(mMediaConstraints, name, value, optional);
}
void RtcModule::setPcConstraint(const string& name, const string &value, bool optional)
{
    rtcModule::setConstraint(mPcConstraints, name, value, optional);
}

bool RtcModule::isCallInProgress() const
{
    bool callInProgress = false;

    for (auto& item: mCalls)
    {
        auto& call = item.second;
        if (call->state() == Call::kStateInProgress || call->state() == Call::kStateReqSent)
        {
            callInProgress = true;
            break;
        }
    }

    return callInProgress;
}

void RtcModule::updatePeerAvState(Id chatid, Id callid, Id userid, uint32_t clientid, AvFlags av)
{
    ICallHandler *callHandler = NULL;

    auto it = mCallHandlers.find(chatid);
    if (it != mCallHandlers.end())  // already known call, update flags
    {
        callHandler = it->second;
    }
    else    // unknown call, create call handler and set flags
    {
        callHandler = mHandler.onGroupCallActive(chatid, callid);
        addCallHandler(chatid, callHandler);
    }

    callHandler->addParticipant(userid, clientid, av);
}

void RtcModule::removeCall(Id chatid)
{
    auto itCall = mCalls.find(chatid);
    if (itCall != mCalls.end())
    {
        itCall->second->destroy(TermCode::kErrPeerOffline, false);
    }

    //TODO: Study behaviour for calls where user is not active
}

void RtcModule::addCallHandler(Id chatid, ICallHandler *callHandler)
{
    auto it = mCallHandlers.find(chatid);
    if (it != mCallHandlers.end())
    {
        delete it->second;
        mCallHandlers.erase(it);
    }

    mCallHandlers[chatid] = callHandler;
}

ICallHandler *RtcModule::findCallHandler(Id chatid)
{
    auto it = mCallHandlers.find(chatid);
    if (it != mCallHandlers.end())
    {
        return it->second;
    }

    return NULL;
}

int RtcModule::numCalls() const
{
    return mCallHandlers.size();
}

std::vector<Id> RtcModule::chatsWithCall() const
{
    std::vector<Id> chats;
    for (auto it = mCallHandlers.begin(); it != mCallHandlers.end(); it++)
    {
        chats.push_back(it->first);
    }

    return chats;
}

void RtcModule::handleCallDataRequest(Chat &chat, Id userid, uint32_t clientid, Id callid, AvFlags avFlagsRemote)
{
    karere::Id chatid = chat.chatId();
    karere::Id myHandle = chat.client().karereClient->myHandle();
    AvFlags avFlags(false, false);
    bool answerAutomatic = false;

    if (userid == myHandle)
    {
        RTCM_LOG_WARNING("Ignoring call request from another client of our user");
        return;
    }

    auto itCallHandler = mCallHandlers.find(chatid);
    auto itCall = mCalls.find(chatid);
    if (itCall != mCalls.end())
    {
        // Two calls at same time in same chat --> resolution of concurrent calls
        shared_ptr<Call> existingCall = itCall->second;
        if (existingCall->state() >= Call::kStateJoining || existingCall->isJoiner())
        {
            RTCM_LOG_DEBUG("hadleCallDataRequest: Receive a CALLDATA with other call in progress");
            bool isCallToSameUser = (userid == existingCall->caller());
            existingCall->sendBusy(isCallToSameUser);
            return;
        }
        else if (mClient.myHandle() > userid)
        {
            RTCM_LOG_DEBUG("hadleCallDataRequest: Waiting for the other peer hangup its incoming call and answer our call");
            return;
        }

        // hang up existing call and answer automatically incoming call
        avFlags = existingCall->sentAv();
        answerAutomatic = true;
        existingCall->hangup();
        mCalls.erase(itCall);
    }
    else if (chat.isGroup() && itCallHandler != mCallHandlers.end() && itCallHandler->second->isParticipating(myHandle))
    {
        // Other client of our user is participanting in the call
        RTCM_LOG_DEBUG("hadleCallDataRequest: Ignoring call request: We are already in the group call from another client");
        return ;
    }

    auto ret = mCalls.emplace(chatid, std::make_shared<Call>(*this, chat, callid, chat.isGroup(),
                                                             true, nullptr, userid, clientid));
    assert(ret.second);
    auto& call = ret.first->second;
    call->mHandler = mHandler.onIncomingCall(*call, avFlagsRemote);
    assert(call->mHandler);
    assert(call->state() == Call::kStateRingIn);
    sendCommand(chat, OP_RTMSG_ENDPOINT, RTCMD_CALL_RINGING, chatid, userid, clientid, callid);
    if (!answerAutomatic)
    {
        auto wcall = call->weakHandle();
        setTimeout([wcall]() mutable
        {
            if (!wcall.isValid() || (wcall->state() != Call::kStateRingIn))
                return;
            static_cast<Call*>(wcall.weakPtr())->destroy(TermCode::kAnswerTimeout, false);
        }, kCallAnswerTimeout+4000, mClient.appCtx); // local timeout a bit longer that the caller
    }
    else
    {
        call->answer(avFlags);
    }
}

void setConstraint(webrtc::FakeConstraints& constr, const string &name, const std::string& value,
    bool optional)
{
    if (optional)
    {
        //TODO: why webrtc has no SetOptional?
        auto& optional = (webrtc::MediaConstraintsInterface::Constraints&)(constr.GetOptional());
        auto it = optional.begin();
        for (; it != optional.end(); it++)
        {
            if (it->key == name)
            {
                it->value = value;
                break;
            }
        }
        if (it == optional.end())
        {
            constr.AddOptional(name, value);
        }
    }
    else
    {
        constr.SetMandatory(name, value);
    }
}
Call::Call(RtcModule& rtcModule, chatd::Chat& chat, karere::Id callid, bool isGroup,
    bool isJoiner, ICallHandler* handler, Id callerUser, uint32_t callerClient)
: ICall(rtcModule, chat, callid, isGroup, isJoiner, handler,
    callerUser, callerClient), mName("call["+chat.chatId().toString()+"]") // the joiner is actually the answerer in case of new call
{
    if (isJoiner)
    {
        mState = kStateRingIn;
        mCallerUser = callerUser;
        mCallerClient = callerClient;
    }
    else
    {
        mState = kStateInitial;
        assert(!callerUser);
        assert(!callerClient);
    }
}

void Call::handleMessage(RtMessage& packet)
{
    switch (packet.type)
    {
        case RTCMD_CALL_TERMINATE:
            msgCallTerminate(packet);
            return;
        case RTCMD_SESSION:
            msgSession(packet);
            return;
        case RTCMD_JOIN:
            msgJoin(packet);
            return;
        case RTCMD_CALL_RINGING:
            msgRinging(packet);
            return;
        case RTCMD_CALL_REQ_DECLINE:
            msgCallReqDecline(packet);
            return;
        case RTCMD_CALL_REQ_CANCEL:
            msgCallReqCancel(packet);
            return;
    }
    auto& data = packet.payload;
    assert(data.dataSize() >= 8); // must start with sid.8
    auto sid = data.read<uint64_t>(0);
    auto sessIt = mSessions.find(sid);
    if (sessIt != mSessions.end())
    {
        sessIt->second->handleMessage(packet);
    }
    else    // new session
    {
        if (packet.type == RTCMD_SESS_TERMINATE)
        {
            TermCode reason = static_cast<TermCode>(data.read<uint8_t>(8));
            if (!Session::isTermRetriable(reason) && cancelSessionRetryTimer(packet.userid, packet.clientid))
            {
                SUB_LOG_DEBUG("Peer terminates session willingfully, but have scheduled a retry because of error. Aborting retry");
                destroyIfNoSessionsOrRetries(reason);
            }
        }
        else
        {
            SUB_LOG_WARNING("Received %s for an existing call but non-existing session %s",
                packet.typeStr(), base64urlencode(&sid, sizeof(sid)).c_str());
        }
    }
}

void Call::setState(uint8_t newState)
{
    auto oldState = mState;
    if (oldState == newState)
        return;

    sStateDesc.assertStateChange(oldState, newState);
    mState = newState;

    SUB_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(newState));
    FIRE_EVENT(CALL, onStateChange, mState);
}

void Call::getLocalStream(AvFlags av, std::string& errors)
{
    // getLocalStream currently never fails - if there is error, stream is a string with the error message
    mLocalStream = mManager.getLocalStream(av, errors);
    if (!errors.empty())
    {
        SUB_LOG_WARNING("There were some errors getting local stream: %s", errors.c_str());
    }
    setState(Call::kStateHasLocalStream);
    IVideoRenderer* renderer = NULL;
    FIRE_EVENT(SESSION, onLocalStreamObtained, renderer);
    mLocalPlayer.reset(new artc::StreamPlayer(renderer, mManager.mClient.appCtx));
    if (mLocalStream && mLocalStream->video())
    {
        mLocalPlayer->attachVideo(mLocalStream->video());
    }

    mLocalPlayer->enableVideo(av.video());
}

void Call::msgCallTerminate(RtMessage& packet)
{
    if (packet.payload.dataSize() < 1)
    {
        SUB_LOG_ERROR("Ignoring CALL_TERMINATE without reason code");
        return;
    }
    auto code = packet.payload.read<uint8_t>(0);
    bool isCallParticipant = false;
    if (mSessions.size() > 0)
    {
        for (auto sessionIt = mSessions.begin(); sessionIt != mSessions.end();)
        {
            auto& session = sessionIt->second;
            sessionIt++;

            if (session->mPeer == packet.userid && session->mPeerClient == packet.clientid)
            {
                isCallParticipant = true;
                break;
            }
        }
    }
    else if (mState <= kStateJoining && mCallerUser == packet.userid && mCallerClient == packet.clientid)
    {
        isCallParticipant = true;
    }
    else
    {
        EndpointId endpointId(packet.userid, packet.clientid);
        auto itSessionRety = mSessRetries.find(endpointId);
        if (itSessionRety != mSessRetries.end())
        {
            // no session to this peer at the moment, but we are in the process of reconnecting to them
            isCallParticipant = true;
        }
    }

    if (!isCallParticipant)
    {
        SUB_LOG_WARNING("Received CALL_TERMINATE from a client that is not in the call, ignoring");
        return;
    }

    destroy(static_cast<TermCode>(code | TermCode::kPeer), false);
}
void Call::msgCallReqDecline(RtMessage& packet)
{
    // callid.8 termcode.1
    assert(packet.payload.dataSize() >= 9);
    packet.callid = packet.payload.read<uint64_t>(0);
    TermCode code = static_cast<TermCode>(packet.payload.read<uint8_t>(8));
    if (code == TermCode::kCallRejected)
    {
        handleReject(packet);
    }
    else if (code == TermCode::kBusy || code == TermCode::kErrAlready)
    {
        handleBusy(packet);
    }
    else
    {
        SUB_LOG_WARNING("Ingoring CALL_REQ_DECLINE with unexpected termnation code %s", termCodeToStr(code));
    }
}

void Call::msgCallReqCancel(RtMessage& packet)
{
    if (mState >= Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected CALL_REQ_CANCEL while in state %s", stateToStr(mState));
        return;
    }

    assert(packet.payload.dataSize() >= 9);
    auto callid = packet.payload.read<uint64_t>(0);
    if (callid != mId)
    {
        SUB_LOG_WARNING("Ignoring CALL_REQ_CANCEL for an unknown request id");
        return;
    }

    assert(mCallerUser);
    assert(mCallerClient);
    // CALL_REQ_CANCEL callid.8 reason.1
    if (mCallerUser != packet.userid || mCallerClient != packet.clientid)
    {
        SUB_LOG_WARNING("Ignoring CALL_REQ_CANCEL from a client that did not send the call request");
        return;
    }

    auto term = packet.payload.read<uint8_t>(8);
    destroy(static_cast<TermCode>(term | TermCode::kPeer), false);
}

void Call::handleReject(RtMessage& packet)
{
    if (packet.callid != mId)
    {
        SUB_LOG_WARNING("Ingoring unexpected call id");
        return;
    }

    if (packet.userid != mChat.client().userId())
    {
        if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
        {
            SUB_LOG_WARNING("Ingoring unexpected CALL_REQ_DECLINE while in state %s", stateToStr(mState));
            return;
        }
        if (mIsGroup)
        {
            // in groupcalls, a peer declining a call should not finish the call
            SUB_LOG_WARNING("Ignoring CALL_REQ_DECLINE. A peer of the group call has declined the request");
            return;
        }
        if (!mSessions.empty())
        {
            // in both 1on1 calls and groupcalls, receiving a request-decline should not destroy the call
            // if there's already a session (in 1on1, it may happen when the answerer declined from a 3rd client, but
            // already answered)
            SUB_LOG_WARNING("Ignoring CALL_REQ_DECLINE. There are active sessions already, so the call is in progress.");
            return;
        }
        destroy(static_cast<TermCode>(TermCode::kCallRejected | TermCode::kPeer), false);
    }
    else // Call has been rejected by other client from same user
    {
        assert(packet.clientid != mChat.connection().clientId());

        if (mState != Call::kStateRingIn)
        {
            SUB_LOG_WARNING("Ingoring unexpected CALL_REQ_DECLINE while in state %s", stateToStr(mState));
            return;
        }

        destroy(static_cast<TermCode>(TermCode::kRejElsewhere), false);
    }
}

void Call::msgRinging(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected RINGING");
        return;
    }
    if (!mRingOutUsers)
    {
        mRingOutUsers.reset(new std::set<Id>());
        mRingOutUsers->insert(packet.userid);
        clearCallOutTimer();
        auto wptr = weakHandle();
        mCallOutTimer = setTimeout([wptr, this] {
            if (wptr.deleted() || mState != Call::kStateReqSent)
            {
                return;
            }
            hangup(TermCode::kAnswerTimeout); // TODO: differentiate whether peer has sent us RINGING or not
        }, RtcModule::kCallAnswerTimeout, mManager.mClient.appCtx);
    }
    else
    {
        mRingOutUsers->insert(packet.userid);
    }
    FIRE_EVENT(CALL, onRingOut, packet.userid);
}

void Call::clearCallOutTimer()
{
    if (!mCallOutTimer) {
        return;
    }
    cancelTimeout(mCallOutTimer, mManager.mClient.appCtx);
    mCallOutTimer = 0;
}

void Call::handleBusy(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected BUSY when in state %s", stateToStr(mState));
        return;
    }
    if (!mIsGroup && mSessions.empty())
    {
        destroy(static_cast<TermCode>(TermCode::kBusy | TermCode::kPeer), false);
    } else
    {
        SUB_LOG_WARNING("Ignoring incoming BUSY for a group call or one with already existing session(s)");
    }
}

void Call::msgSession(RtMessage& packet)
{
    if (mState != Call::kStateJoining && mState != Call::kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected SESSION");
        return;
    }

    setState(Call::kStateInProgress);

    Id sid = packet.payload.read<uint64_t>(8);
    if (mSessions.find(sid) != mSessions.end())
    {
        SUB_LOG_ERROR("Received SESSION with sid of an existing session (%s), ignoring", sid.toString().c_str());
        return;
    }
    auto sess = std::make_shared<Session>(*this, packet);
    mSessions[sid] = sess;
    notifyCallStarting(*sess);
    sess->sendOffer();

    cancelSessionRetryTimer(sess->mPeer, sess->mPeerClient);
}

void Call::notifyCallStarting(Session& sess)
{
    if (!mCallStartingSignalled)
    {
        mCallStartingSignalled = true;
        FIRE_EVENT(CALL, onCallStarting);
    }
}

void Call::msgJoin(RtMessage& packet)
{
    if (mState == kStateRingIn && packet.userid == mManager.mClient.myHandle())
    {
        destroy(TermCode::kAnsElsewhere, false);
    }
    else if (mState == Call::kStateInProgress || mState == Call::kStateReqSent)
    {
        packet.callid = packet.payload.read<uint64_t>(0);
        assert(packet.callid);
        if (mState == Call::kStateReqSent)
        {
            setState(Call::kStateInProgress);
            // Send OP_CALLDATA with call inProgress
            if (!chat().isGroup())
            {
                sendCallData(CallDataState::kCallDataInProgress);
            }
        }
        // create session to this peer
        auto sess = std::make_shared<Session>(*this, packet);
        mSessions[sess->mSid] = sess;
        notifyCallStarting(*sess);
        sess->createRtcConn();
        sess->sendCmdSession(packet);

        cancelSessionRetryTimer(sess->mPeer, sess->mPeerClient);
    }
    else
    {
        SUB_LOG_WARNING("Ignoring unexpected JOIN");
        return;
    }
}
promise::Promise<void> Call::gracefullyTerminateAllSessions(TermCode code)
{
    SUB_LOG_DEBUG("gracefully term all sessions");
    std::vector<promise::Promise<void>> promises;
    for (auto it = mSessions.begin(); it != mSessions.end();)
    {
        std::shared_ptr<Session> session = it++->second;
        promises.push_back(session->terminateAndDestroy(code));
    }
    return promise::when(promises)
    .fail([](const promise::Error& err)
    {
        assert(false); // terminateAndDestroy() should never fail
    });
}

Promise<void> Call::waitAllSessionsTerminated(TermCode code, const std::string& msg)
{
    // if the peer initiated the call termination, we must wait for
    // all sessions to go away and remove the call
    if (mSessions.empty())
    {
        return promise::_Void();
    }

    for (auto& item: mSessions)
    {
        item.second->setState(Session::kStateTerminating);
    }
    auto wptr = weakHandle();

    struct Ctx
    {
        int count = 0;
        Promise<void> pms;
    };
    auto ctx = std::make_shared<Ctx>();
    mDestroySessionTimer = setInterval([wptr, this, ctx, code, msg]()
    {
        if (wptr.deleted())
            return;
        if (++ctx->count > 7)
        {
            cancelInterval(mDestroySessionTimer, mManager.mClient.appCtx);
            mDestroySessionTimer = 0;
            SUB_LOG_WARNING("Timed out waiting for all sessions to terminate, force closing them");
            for (auto itSessions = mSessions.begin(); itSessions != mSessions.end();)
            {
                auto session = itSessions->second;
                itSessions++;
                session->destroy(code, msg);
            }

            ctx->pms.resolve();
            return;
        }
        if (!mSessions.empty())
            return;
        cancelInterval(mDestroySessionTimer, mManager.mClient.appCtx);
        mDestroySessionTimer = 0;
        ctx->pms.resolve();
    }, 200, mManager.mClient.appCtx);
    return ctx->pms;
}

Promise<void> Call::destroy(TermCode code, bool weTerminate, const string& msg)
{
    if (mState == Call::kStateDestroyed)
    {
        return promise::_Void();
    }
    else if (mState == Call::kStateTerminating)
    {
        assert(!mDestroyPromise.done());
        return mDestroyPromise;
    }
    if (!msg.empty())
    {
        SUB_LOG_DEBUG("Destroying call due to: %s", msg.c_str());
    }

    mTermCode = code;
    mPredestroyState = mState;
    setState(Call::kStateTerminating);
    clearCallOutTimer();

    Promise<void> pms((promise::Empty())); //non-initialized promise
    if (weTerminate)
    {
        switch (mPredestroyState)
        {
        case kStateReqSent:
            cmdBroadcast(RTCMD_CALL_REQ_CANCEL, mId, code);
            pms = promise::_Void();
            break;
        case kStateRingIn:
            cmdBroadcast(RTCMD_CALL_REQ_DECLINE, mId, code);
            pms = promise::_Void();
            break;
        default:
            if (!mIsGroup)
            {
                cmdBroadcast(RTCMD_CALL_TERMINATE, code);
            }

            // if we initiate the call termination, we must initiate the
            // session termination handshake
            pms = gracefullyTerminateAllSessions(code);
            break;
        }
    }
    else
    {
        pms = waitAllSessionsTerminated(code);
    }

    mDestroyPromise = pms;
    auto wptr = weakHandle();
    auto retPms = pms.then([wptr, this, code, msg]()
    {
        if (wptr.deleted())
            return;

        if (code == TermCode::kAnsElsewhere || code == TermCode::kErrAlready || code == TermCode::kAnswerTimeout)
        {
            SUB_LOG_DEBUG("Not posting termination CALLDATA because term code is kAnsElsewhere or kErrAlready");
        }
        else if (mState == kStateRingIn)
        {
            SUB_LOG_DEBUG("Not sending CALLDATA because we were passively ringing in a group call");
        }
        else
        {
            sendCallData(CallDataState::kCallDataEnd);
        }

        assert(mSessions.empty());
        stopIncallPingTimer();
        mLocalPlayer.reset();
        setState(Call::kStateDestroyed);
        FIRE_EVENT(CALL, onDestroy, static_cast<TermCode>(code & ~TermCode::kPeer),
            !!(code & 0x80), msg);// jscs:ignore disallowImplicitTypeConversion
        mManager.removeCall(*this);
    });
    return retPms;
}
template <class... Args>
bool Call::cmdBroadcast(uint8_t type, Args... args)
{
    RtMessageComposer msg(chatd::OP_RTMSG_BROADCAST, type, mChat.chatId(), 0, 0);
    msg.payloadAppend(args...);
    if (mChat.sendCommand(std::move(msg)))
    {
        return true;
    }
    auto wptr = weakHandle();
    marshallCall([wptr, this]()
    {
        if (wptr.deleted())
            return;
        destroy(TermCode::kErrNetSignalling, false);
    }, mManager.mClient.appCtx);
    return false;
}

bool Call::broadcastCallReq()
{
    if (mState >= Call::kStateTerminating)
    {
        SUB_LOG_WARNING("broadcastCallReq: Call terminating/destroyed");
        return false;
    }
    assert(mState == Call::kStateHasLocalStream);

    if (!sendCallData(CallDataState::kCallDataRinging))
    {
        return false;
    }

    setState(Call::kStateReqSent);
    startIncallPingTimer();
    auto wptr = weakHandle();
    mCallOutTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted())
            return;

        if (mState == Call::kStateReqSent)
        {
            destroy(TermCode::kRingOutTimeout, true);
        }
        else if (chat().isGroup() && mState == Call::kStateInProgress)
        {
            // In group calls we don't stop ringing even when call is answered, but stop it
            // after some time (we use the same kAnswerTimeout duration in order to share the timer)
            sendCallData(CallDataState::kCallDataInProgress);
        }
    }, RtcModule::kRingOutTimeout, mManager.mClient.appCtx);
    return true;
}

void Call::startIncallPingTimer()
{
    assert(!mInCallPingTimer);

    sendInCallCommand();
    auto wptr = weakHandle();
    mInCallPingTimer = setInterval([this, wptr]()
    {
        if (wptr.deleted())
            return;

        sendInCallCommand();
    }, RtcModule::kIncallPingInterval, mManager.mClient.appCtx);
}

void Call::asyncDestroy(TermCode code, bool weTerminate)
{
    auto wptr = weakHandle();
    marshallCall([wptr, this, code, weTerminate]()
    {
        if (wptr.deleted())
            return;
        destroy(code, weTerminate);
    }, mManager.mClient.appCtx);
}

void Call::stopIncallPingTimer(bool endCall)
{
    if (mInCallPingTimer)
    {
        cancelInterval(mInCallPingTimer, mManager.mClient.appCtx);
        mInCallPingTimer = 0;
    }

    if (endCall)
    {
        mChat.sendCommand(Command(OP_ENDCALL) + mChat.chatId() + uint64_t(0) + uint32_t(0));
    }
}

void Call::removeSession(Session& sess, TermCode reason)
{
    // TODO: For group calls we would need to revise this
    if (mState == kStateTerminating)
    {
        mSessions.erase(sess.mSid);
        return;
    }

    if (!Session::isTermRetriable(reason))
    {
        destroyIfNoSessionsOrRetries(reason);
    }

    // If we want to terminate the call (no matter if initiated by us or peer), we first
    // set the call's state to kTerminating. If that is not set, then it's only the session
    // that terminates for a reason that is not fatal to the call,
    // and can try re-establishing the session
    if (cancelSessionRetryTimer(sess.mPeer, sess.mPeerClient))
    {
        SUB_LOG_DEBUG("removeSession: Trying to remove a session for which there is a scheduled retry, the retry should not be there");
        assert(false);
    }

    if (!sess.isCaller())
    {
        SUB_LOG_DEBUG("Session to %s failed, re-establishing it...", sess.sessionId().toString().c_str());
        auto wptr = weakHandle();
        karere::Id peerid = sess.mPeer;
        uint32_t clientid = sess.mPeerClient;
        marshallCall([wptr, this, peerid, clientid]()
        {
            if (wptr.deleted())
                return;

            rejoin(peerid, clientid);

        }, mManager.mClient.appCtx);
    }
    else
    {
        // Else wait for peer to re-join...
        SUB_LOG_DEBUG("Session to %s failed, expecting peer to re-establish it...", sess.sessionId().toString().c_str());
    }

    // set a timeout for the session recovery
    EndpointId endpointId(sess.mPeer, sess.mPeerClient);
    auto wptr = weakHandle();
    mSessRetries[endpointId] = setTimeout([this, wptr, endpointId]()
    {
        if (wptr.deleted())
            return;

        mSessRetries.erase(endpointId);

        if (mState >= kStateTerminating) // call already terminating
        {
           return; //timer is not relevant anymore
        }

        if (hasNoSessionsOrPendingRetries())
        {
            SUB_LOG_DEBUG("Timed out waiting for peer to rejoin, terminating call");
            hangup(kErrSessRetryTimeout);
        }

    }, RtcModule::kSessSetupTimeout, mManager.mClient.appCtx);

    mSessions.erase(sess.mSid);
}
bool Call::startOrJoin(AvFlags av)
{
    std::string errors;

    manager().updatePeerAvState(mChat.chatId(), mId, mChat.client().karereClient->myHandle(), mChat.connection().clientId(), av);
    if (mIsJoiner)
    {
        getLocalStream(av, errors);
        return join();
    }
    else
    {
        getLocalStream(av, errors);
        return broadcastCallReq();
    }
}
template <class... Args>
bool Call::cmd(uint8_t type, Id userid, uint32_t clientid, Args... args)
{
    assert(userid);
    uint8_t opcode = clientid ? OP_RTMSG_ENDPOINT : OP_RTMSG_USER;
    RtMessageComposer msg(opcode, type, mChat.chatId(), userid, clientid);
    msg.payloadAppend(args...);
    return mChat.sendCommand(std::move(msg));
}

bool Call::join(Id userid)
{
    assert(mState == Call::kStateHasLocalStream);
    // JOIN:
    // chatid.8 userid.8 clientid.4 dataLen.2 type.1 callid.8 anonId.8
    // if userid is not specified, join all clients in the chat, otherwise
    // join a specific user (used when a session gets broken)
    setState(Call::kStateJoining);
    bool sent = userid
            ? cmd(RTCMD_JOIN, userid, 0, mId, mManager.mOwnAnonId)
            : cmdBroadcast(RTCMD_JOIN, mId, mManager.mOwnAnonId);
    if (!sent)
    {
        asyncDestroy(TermCode::kErrNetSignalling, true);
        return false;
    }

    sendCallData(CallDataState::kCallDataJoin);
    startIncallPingTimer();
    // we have session setup timeout timer, but in case we don't even reach a session creation,
    // we need another timer as well
    auto wptr = weakHandle();
    setTimeout([wptr, this]()
    {
        if (wptr.deleted())
            return;
        if (mState <= Call::kStateJoining)
        {
            destroy(TermCode::kErrSessSetupTimeout, true);
        }
    }, RtcModule::kSessSetupTimeout, mManager.mClient.appCtx);
    return true;
}

bool Call::rejoin(karere::Id userid, uint32_t clientid)
{
    assert(mState == Call::kStateInProgress);
    // JOIN:
    // chatid.8 userid.8 clientid.4 dataLen.2 type.1 callid.8 anonId.8
    // if userid is not specified, join all clients in the chat, otherwise
    // join a specific user (used when a session gets broken)
    bool sent = cmd(RTCMD_JOIN, userid, clientid, mId, mManager.mOwnAnonId);
    if (!sent)
    {
        asyncDestroy(TermCode::kErrNetSignalling, true);
        return false;
    }

    return true;
}

void Call::sendInCallCommand()
{
    if (!mChat.sendCommand(Command(OP_INCALL) + mChat.chatId() + uint64_t(0) + uint32_t(0)))
    {
        asyncDestroy(TermCode::kErrNetSignalling, true);
    }
}

bool Call::sendCallData(CallDataState state)
{
    uint16_t payLoadLen = sizeof(mId) + sizeof(uint8_t) + sizeof(uint8_t);
    if (state == CallDataState::kCallDataEnd)
    {
         payLoadLen += sizeof(uint8_t);
    }

    Command command = Command(chatd::OP_CALLDATA) + mChat.chatId();
    command.write<uint64_t>(9, 0);
    command.write<uint32_t>(17, 0);
    command.write<uint16_t>(21, payLoadLen);
    command.write<uint64_t>(23, mId);
    command.write<uint8_t>(31, state);
    command.write<uint8_t>(32, sentAv().value());
    if (state == CallDataState::kCallDataEnd)
    {
        command.write<uint8_t>(33, convertTermCodeToCallDataCode());
    }

    if (!mChat.sendCommand(std::move(command)))
    {
        auto wptr = weakHandle();
        marshallCall([wptr, this]()
        {
            if (wptr.deleted())
                return;
            destroy(TermCode::kErrNetSignalling, true);
        }, mManager.mClient.appCtx);

        return false;
    }

    return true;
}

void Call::destroyIfNoSessionsOrRetries(TermCode reason)
{
    if (hasNoSessionsOrPendingRetries())
    {
        SUB_LOG_DEBUG("Everybody left, terminating call");
        destroy(reason, false, "Everybody left");
    }
}

bool Call::hasNoSessionsOrPendingRetries() const
{
    return mSessions.empty() && mSessRetries.empty();
}

uint8_t Call::convertTermCodeToCallDataCode()
{
    uint8_t codeToChatd;
    switch (mTermCode & ~TermCode::kPeer)
    {
        case kUserHangup:
        {
            switch (mPredestroyState)
            {
                case kStateRingIn:
                    assert(false);  // it should be kCallRejected
                case kStateReqSent:
                    codeToChatd = kCallDataReasonCancelled;
                    break;

                case kStateInProgress:
                    codeToChatd = kCallDataReasonEnded;
                    break;

                default:
                    codeToChatd = kCallDataReasonFailed;
                    break;
            }
            break;
        }

        case kCallRejected:
            codeToChatd = kCallDataReasonRejected;
            break;

        case kAnsElsewhere:
            SUB_LOG_ERROR("Can't generate a history call ended message for local kAnsElsewhere code");
            assert(false);
            break;
        case kRejElsewhere:
            SUB_LOG_ERROR("Can't generate a history call ended message for local kRejElsewhere code");
            assert(false);
            break;

        case kAnswerTimeout:
        case kRingOutTimeout:
            codeToChatd = kCallDataReasonNoAnswer;
            break;

        case kAppTerminating:
            codeToChatd = (mPredestroyState == kStateInProgress) ? kCallDataReasonEnded : kCallDataReasonFailed;
            break;

        case kBusy:
            assert(!isJoiner());
            codeToChatd = kCallDataReasonRejected;
            break;

        default:
            if (!isTermError(mTermCode))
            {
                SUB_LOG_ERROR("convertTermCodeToCallDataCode: Don't know how to translate term code %s, returning FAILED",
                              termCodeToStr(mTermCode));
            }
            codeToChatd = kCallDataReasonFailed;
            break;
    }

    return codeToChatd;
}

bool Call::cancelSessionRetryTimer(karere::Id userid, uint32_t clientid)
{
    EndpointId endpointId(userid, clientid);
    auto itSessionRetry = mSessRetries.find(endpointId);
    if (itSessionRetry != mSessRetries.end())
    {
        megaHandle timerHandle = itSessionRetry->second;
        cancelTimeout(timerHandle, mManager.mClient.appCtx);
        mSessRetries.erase(itSessionRetry);
        return true;
    }

    return false;
}

bool Call::answer(AvFlags av)
{
    if (mState != Call::kStateRingIn)
    {
        SUB_LOG_WARNING("answer: Not in kRingIn state, nothing to answer");
        return false;
    }
    assert(mIsJoiner);
    assert(mHandler);
    if (mHandler->callParticipants() >= RtcModule::kMaxCallReceivers)
    {
        SUB_LOG_WARNING("answer: It's not possible join to the call, there are too many participants");
        return false;
    }

    return startOrJoin(av);
}

void Call::hangup(TermCode reason)
{
    switch (mState)
    {
    case kStateReqSent:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kUserHangup;
        }
        else
        {
            assert(reason == TermCode::kUserHangup ||
                   reason == TermCode::kAppTerminating ||
                   reason == TermCode::kAnswerTimeout ||
                   reason == TermCode::kRingOutTimeout);
        }

        destroy(reason, true);
        return;

    case kStateRingIn:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kCallRejected;
        }
        else
        {
            assert(reason == TermCode::kCallRejected && "Hangup reason can only be undefined or kCallRejected when hanging up call in state kRingIn");
        }
        assert(mSessions.empty());
        destroy(reason, true);
        return;

    case kStateJoining:
    case kStateInProgress:
    case kStateHasLocalStream:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kUserHangup;
        }
        else
        {
            assert(reason == TermCode::kUserHangup || reason == TermCode::kAppTerminating || isTermError(reason));
        }
        break;

    case kStateTerminating:
    case kStateDestroyed:
        SUB_LOG_DEBUG("hangup: Call already terminating/terminated");
        return;

    default:
        reason = TermCode::kUserHangup;
        SUB_LOG_WARNING("Don't know what term code to send in state %s", stateStr());
        break;
    }

    // in any state, we just have to send CALL_TERMINATE and that's all
    destroy(reason, true);
}
Call::~Call()
{
    if (mState != Call::kStateDestroyed)
    {
        stopIncallPingTimer();
        if (mDestroySessionTimer)
        {
            cancelInterval(mDestroySessionTimer, mManager.mClient.appCtx);
            mDestroySessionTimer = 0;
        }

        mLocalPlayer.reset();
        setState(Call::kStateDestroyed);
        FIRE_EVENT(CALL, onDestroy, TermCode::kErrInternal, false, "Callback from Call::dtor");// jscs:ignore disallowImplicitTypeConversion
        SUB_LOG_DEBUG("Forced call to onDestroy from call dtor");
    }

    SUB_LOG_DEBUG("Destroyed");
}
void Call::onClientLeftCall(Id userid, uint32_t clientid)
{
    if (mState == kStateRingIn && userid == mCallerUser && clientid == mCallerClient) // caller went offline
    {
        destroy(TermCode::kUserHangup, false);
        return;
    }

    if (mIsGroup)
    {
        for (auto& item: mSessions)
        {
            auto sess = item.second;
            if (sess->mPeer == userid && sess->mPeerClient == clientid)
            {
                marshallCall([sess]()
                {
                    sess->terminateAndDestroy(static_cast<TermCode>(TermCode::kErrPeerOffline | TermCode::kPeer));
                }, mManager.mClient.appCtx);
                return;
            }
        }
    }
    else if (mState >= kStateInProgress)
    {
        destroy(TermCode::kErrPeerOffline, userid == mChat.client().karereClient->myHandle());
    }
}
bool Call::changeLocalRenderer(IVideoRenderer* renderer)
{
    if (!mLocalPlayer)
        return false;
    mLocalPlayer->changeRenderer(renderer);
    return true;
}

void Call::notifySessionConnected(Session& sess)
{
    if (mCallStartedSignalled)
        return;
    mCallStartedSignalled = true;
    FIRE_EVENT(CALL, onCallStarted);
}

AvFlags Call::muteUnmute(AvFlags av)
{
    if (!mLocalStream)
        return AvFlags(0);

    AvFlags oldAv = mLocalStream->effectiveAv();
    mLocalStream->setAv(av);
    av = mLocalStream->effectiveAv();
    if (av == oldAv)
    {
        return oldAv;
    }

    mLocalPlayer->enableVideo(av.video());
    for (auto& item: mSessions)
    {
        item.second->sendAv(av);
    }

    manager().updatePeerAvState(mChat.chatId(), mId, mChat.client().karereClient->myHandle(), mChat.connection().clientId(), av);
    sendCallData(CallDataState::kCallDataMute);

    return av;
}

std::map<Id, AvFlags> Call::avFlagsRemotePeers() const
{
    std::map<Id, AvFlags> peerFlags;

    for (auto& item: mSessions)
    {
        peerFlags[item.first] = item.second->receivedAv();
    }

    return peerFlags;
}

std::map<Id, uint8_t> Call::sessionState() const
{    
    std::map<Id, uint8_t> sessionState;

    for (auto& item: mSessions)
    {
        sessionState[item.first] = item.second->getState();
    }

    return sessionState;
}

void Call::sendBusy(bool isCallToSameUser)
{
    // Broadcast instead of send only to requestor, so that all other our clients know we rejected the call
    cmdBroadcast(RTCMD_CALL_REQ_DECLINE, mId, isCallToSameUser ? TermCode::kErrAlready : TermCode::kBusy);
}

AvFlags Call::sentAv() const
{
    return mLocalStream ? mLocalStream->effectiveAv() : AvFlags(0);
}
/** Protocol flow:
    C(aller): broadcast CALLDATA payloadLen.2 callid.8 callState.1 avflags.1
       => state: CallState.kReqSent
    A(nswerer): send RINGING
       => state: CallState.kRingIn
    C: may send CALL_REQ_CANCEL callid.8 reason.1 if caller aborts the call request.
       The reason is normally Term.kUserHangup or Term.kAnswerTimeout
    A: may broadcast CALL_REQ_DECLINE callid.8 reason.1 if answerer rejects the call or if
       the user is currently in another call: if that call is in the same chatroom, then the reason
       is kErrAlready, otherwise - kBusy.
       In group calls, CALL_REQ_DECLINE is ignored by the caller, as a group call request can't be rejected by
       a single user. In 1on1 rooms, the caller should abort the call by broadcasting CALL_REQ_CANCEL,
       with the reason from the received CALL_REQ_DECLINE, and the kPeer bit set. All other clients should stop
       ringing when they receive the CALL_REQ_CANCEL.
    == (from here on we can join an already ongoing group call) ==
    A: broadcast JOIN callid.8 anonId.8
        => state: CallState.kCallDataJoining
        => isJoiner = true
        Other clients of user A will receive the answering client's JOIN (if call
        was answered), or CALL_REQ_DECLINE (in case the call was rejected), and will know that the call has
        been handled by another client of that user. They will then dismiss the "incoming call"
        dialog and stop ringing.
        In 1on1 calls, when the call initiator receives the first JOIN, it broadcasts a CALLDATA packet
        with type kNotRinging, signifying that the call was answered, so the other clients can stop ringing.
        Note: In case of joining an ongoing call, the callid is generated locally
          and does not match the callid if the ongoing call, as the protocol
          has no way of conveying the callid of the ongoing call. The responders
          to the JOIN will use the callid of the JOIN, and not the original callid.
          The callid is just used to match command/responses in the call setup handshake.
          Once that handshake is complete, only session ids are used for the actual 1on1 sessions.

    C: send SESSION callid.8 sid.8 anonId.8 encHashKey.32 actualCallId.8
        => call state: CallState.kInProgress
        => sess state: SessState.kWaitSdpOffer
    A: send SDP_OFFER sid.8 encHashKey.32 fprHash.32 avflags.1 sdpLen.2 sdpOffer.sdpLen
        => call state: CallState.kInProress
        => sess state: SessState.kWaitSdpAnswer
    C: send SDP_ANSWER sid.8 fprHash.32 avflags.1 sdpLen.2 sdpAnswer.sdpLen
        => state: SessState.kWaitMedia
    A and C: exchange ICE_CANDIDATE sid.8 midLen.1 mid.midLen mLineIdx.1 candLen.2 iceCand.candLen

    Once the webrtc session is connected (ICE connection state changes to 'connected'),
    and this is the first connected session for that client, then the call is locally considered
    as started, and the GUI transitions to the in-call state.

    A or C: may send MUTE sid.8 avState.1 (if user mutes/unmutes audio/video).
        Webrtc does not have an in-band notification of stream muting
        (needed to update the GUI), so we do it out-of-band.
        The client that sends the MUTE also sends a CALLDATA packet with type kMute and
        its new audio and video send state, in order to notify clients that are not in
        the call, so they can count the available audio/video slots within a group call.
        At a later stage, we may want to stop sending the MUTE command at all and only
        use the CALLDATA (which ws introduced at a later stage).

    A or C: send SESS_TERMINATE sid.8 reason.1
        => state: SessState.kTerminating
    C or A: send SESS_TERMINATE_ACK sid.8
        => state: Sess.kDestroyed
    A or C: close webrtc connection upont receipt of terminate ack,
        or after a timeout of 1-2 seconds. The terminate ack mechanism prevents
        the peer from thinking that the webrtc connection was closed
        due to error
        => state: Sess.kDestroyed
@note avflags of caller are duplicated in CALLDATA and SDP_ANSWER
The first is purely informative, to make the callee aware what type of
call the caller is requesting - audio or video.
This is not available when joining an existing call.

== INCALL, ENDCALL protocol ==
Each client that is involved in a call, periodically sends an INCALL ping packet to the server.
The server keeps track of all actively pinging clients, and dumps a list of them to clients that log
into that chatroom, and sends deltas in realtime when new clients join or leave. When a client leaves a call,
it sends an ENDCALL. If it times out to send an INCALL ping, the server behaves as if that client sent
an ENDCALL (i.e. considers that client went out of the call). This mechanism allows to keep track of
whether there is an ongoing call in a chatroom and who is participating in it at any moment.
This is how clients know whether they can join an ongoing group call.

== CALLDATA protocol ==
- Overview -
CALLDATA is a special packet that can contain arbitrary data, is sent by clients, and is stored on the server
per-client. Actually the server stores only the last such packet sent by each client. When a client
sends such a packet, the server also broadcasts it to all other clients in that chatroom. Upon login
to a chatroom, the server dumps the CALLDATA blobs stored for all clients (i.e. the last ones they sent).
After that, any new CALLDATA a client sends is received by everyone because of the broadcast.

- Call request -
The CALLDATA mechanism was initually introduced to allow for call requests to be received
via mobile PUSH. When the call request is broadcast, a mobile client is likely to not be running.
It would be started by the OS upon receipt by the PUSH event, but it would miss the realtime
call request message. Therefore, the call request needed to be persisted on the server.
This was realized by the server keeping track of the last CALLDATA packet received by each client,
and dumping these packets upon login to a chatroom (even before any history fetch is done).
This would allow for the call initiator to persist a CALLDATA packet on the server, that signifies
a call request (having a type kRinging). When the call is answered or aborted, the caller is responsible
for either overwriting the kRinging CALLDATA with one that signals the call request is not there anymore -
usually a type kNotRinging CALLDATA (when the call was answered), or send an ENDCALL to the server if it
aborts the call for some reason. A client sending an ENDCALL (or timing out the INCALL ping) results in its
CALLDATA not being sent by the server to clients that log into that chatroom.

- Call end -
Later, the CALLDATA mechanism was extended to facilitate posting history messages about call events -
particularly about the end of a call. The posting of these call management messages is done by the server
and it needs to figure out the reason for the call termination, in order to display "Call was not answered",
"Call ended normally", "Call failed", "Call request was canceled", etc. To do that, clients send another type
of CALLDATA (kTerminated) to the server with a reason code. When a client just sends an ENDCALL or times
out the INCALL ping, the server assumes it left the call due to some error. When all clients leave the call,
the server combines the reasons they reported and posts a call-ended management messages in the message history.

- Audio/video slots -
In group calls, the number of audio and video senders and receivers has to be kept track of,
in order to not overload client with too many streams. This means that a client may be disallowed
to join a call with camera enabled, with microphone enabled, or even disallow join at all, even without
sending any media. This is because of the mesh configuration of group calls - the bandwidth and processing
requirement grows exponentially with the number of stream senders, but also linearly with the number of passive
receivers (a stream sender is also a receiver). Therefore, all CALLDATA packets that a client sends include
its current audio/video send state. This allows even clients that are not in the call to keep track of how many
people are sending and receiving audio/video, and disallow the user to enable camera/mic or even join the call.
There is a dedicated CALLDATA type kMute that clients send for the specific purpose of updating the a/v send state.
It is send when the client mutes/unmutes camera or mic. Currently this CALLDATA is sent together with the MUTE
message, but in the future we may want to only rely on the CALLDATA packet.

*/
Session::Session(Call& call, RtMessage& packet)
:ISession(call, packet.userid, packet.clientid), mManager(call.mManager)
{
    // Packet can be RTCMD_JOIN or RTCMD_SESSION
    call.mManager.random(mOwnSdpKey);
    mHandler = call.callHandler()->onNewSession(*this);
    printf("============== own sdp key: %s\n", StaticBuffer(mOwnSdpKey.data, sizeof(mOwnSdpKey.data)).toString().c_str());
    if (packet.type == RTCMD_JOIN)
    {
        // peer will send offer
        // JOIN callid.8 anonId.8
        mIsJoiner = false;
        mSid = call.mManager.random<uint64_t>();
        setState(kStateWaitSdpOffer);
        mPeerAnonId = packet.payload.read<uint64_t>(8);
    }
    else
    {
        // SESSION callid.8 sid.8 anonId.8 encHashKey.32
        assert(packet.type == RTCMD_SESSION);
        mIsJoiner = true;
        mSid = packet.payload.read<uint64_t>(8);
        setState(kStateWaitSdpAnswer);
        assert(packet.payload.dataSize() >= 56);
        mPeerAnonId = packet.payload.read<uint64_t>(16);
        SdpKey encKey;
        packet.payload.read(24, encKey);
        call.mManager.crypto().decryptKeyFrom(mPeer, encKey, mPeerSdpKey);
    }

    mName = "sess[" + mSid.toString() + "]";
    auto wptr = weakHandle();
    mSetupTimer = setTimeout([wptr, this] {
        if (wptr.deleted())
            return;
        if (mState < kStateInProgress) {
            terminateAndDestroy(TermCode::kErrSessSetupTimeout);
        }
    }, RtcModule::kSessSetupTimeout, call.mManager.mClient.appCtx);
}

void Session::sendCmdSession(RtMessage& joinPacket)
{
    SdpKey encKey;
    mCall.mManager.crypto().encryptKeyTo(mPeer, mOwnSdpKey, encKey);
    // SESSION callid.8 sid.8 anonId.8 encHashKey.32
   mCall.mManager.cmdEndpoint(RTCMD_SESSION, joinPacket,
        joinPacket.callid,
        mSid,
        mCall.mManager.mOwnAnonId,
        encKey,
        mCall.id()
    );
}

void Session::setState(uint8_t newState)
{
    auto oldState = mState;
    if (oldState == newState)
        return;

    sStateDesc.assertStateChange(oldState, newState);
    mState = newState;
    SUB_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(mState));
    FIRE_EVENT(SESSION, onSessStateChange, mState);
}

webrtc::FakeConstraints* Session::pcConstraints()
{
    return &mCall.mManager.mPcConstraints;
}

string Session::getDeviceInfo() const
{
    // UserAgent Format
    // MEGA<app>/<version> (platform) Megaclient/<version>
    std::string userAgent = mCall.mChat.mClient.karereClient->api.sdk.getUserAgent();

    std::string androidId = "MEGAAndroid";
    std::string iosId = "MEGAiOS";
    std::string testChatId = "MEGAChatTest";
    std::string syncId = "MEGAsync";

    std::string deviceType = "n";
    std::string version = "0";

    size_t endTypePosition = std::string::npos;
    size_t idPosition;
    if ((idPosition = userAgent.find(androidId)) != std::string::npos)
    {
        deviceType = "na";
        endTypePosition = idPosition + androidId.size() + 1; // remove '/'
    }
    else if ((idPosition = userAgent.find(iosId)) != std::string::npos)
    {
        deviceType = "ni";
        endTypePosition = idPosition + iosId.size() + 1;  // remove '/'
    }
    else if ((idPosition = userAgent.find(testChatId)) != std::string::npos)
    {
        deviceType = "nct";
    }
    else if ((idPosition = userAgent.find(syncId)) != std::string::npos)
    {
        deviceType = "nsync";
        endTypePosition = idPosition + syncId.size() + 1;  // remove '/'
    }


    size_t endVersionPosition = userAgent.find(" (");
    if (endVersionPosition != std::string::npos &&
            endTypePosition != std::string::npos &&
            endVersionPosition > endTypePosition)
    {
        version = userAgent.substr(endTypePosition, endVersionPosition - endTypePosition);
    }

    return deviceType + ":" + version;
}

void Session::handleMessage(RtMessage& packet)
{
    switch (packet.type)
    {
        case RTCMD_SDP_OFFER:
            msgSdpOfferSendAnswer(packet);
            return;
        case RTCMD_SDP_ANSWER:
            msgSdpAnswer(packet);
            return;
        case RTCMD_ICE_CANDIDATE:
            msgIceCandidate(packet);
            return;
        case RTCMD_SESS_TERMINATE:
            msgSessTerminate(packet);
            return;
        case RTCMD_SESS_TERMINATE_ACK:
            msgSessTerminateAck(packet);
            return;
        case RTCMD_MUTE:
            msgMute(packet);
            return;
        default:
            SUB_LOG_WARNING("Don't know how to handle", packet.typeStr());
            return;
    }
}

void Session::createRtcConn()
{
    mRtcConn = artc::myPeerConnection<Session>(mCall.mManager.mIceServers,
        *this, pcConstraints());
    if (mCall.mLocalStream)
    {
        if (!mRtcConn->AddStream(*mCall.mLocalStream))
        {
            RTCM_LOG_ERROR("mRtcConn->AddStream() returned false");
        }
    }
    mStatRecorder.reset(new stats::Recorder(*this, 1, 5));
    mStatRecorder->start();
}
//PeerConnection events
void Session::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    setState(kStateInProgress);
    if (mRemotePlayer)
    {
        SUB_LOG_ERROR("onRemoteStreamAdded: Session already has a remote player, ignoring event");
        return;
    }

    IVideoRenderer* renderer = NULL;
    FIRE_EVENT(SESSION, onRemoteStreamAdded, renderer);
    assert(renderer);
    mRemotePlayer.reset(new artc::StreamPlayer(renderer, mManager.mClient.appCtx));
    mRemotePlayer->setOnMediaStart([this]()
    {
        FIRE_EVENT(SESS, onVideoRecv);
    });
    mRemotePlayer->attachToStream(stream);
    mRemotePlayer->enableVideo(mPeerAv.video());
}
void Session::onRemoveStream(artc::tspMediaStream stream)
{
    if (stream != mRemoteStream) //we can't throw here because we are in a callback
    {
        KR_LOG_ERROR("onRemoveStream: Stream is not the remote stream that we have");
        return;
    }
    if(mRemotePlayer)
    {
        mRemotePlayer->detachFromStream();
        mRemotePlayer.reset();
    }
    mRemoteStream.release();
    FIRE_EVENT(SESSION, onRemoteStreamRemoved);
}

void Session::onIceCandidate(std::shared_ptr<artc::IceCandText> cand)
{
    // mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    if (!cand)
        return;
    
    RtMessageComposer msg(OP_RTMSG_ENDPOINT, RTCMD_ICE_CANDIDATE,
        mCall.mChat.chatId(), mPeer, mPeerClient, 10+cand->candidate.size());
    msg.payloadAppend(mSid);
    msg.payloadAppend(static_cast<uint8_t>(cand->sdpMLineIndex));
    auto& mid = cand->sdpMid;
    if (!mid.empty())
    {
        msg.payloadAppend(static_cast<uint8_t>(mid.size()), mid);
    }
    else
    {
        msg.payloadAppend(static_cast<uint8_t>(0));
    }
    msg.payloadAppend(static_cast<uint16_t>(cand->candidate.size()),
        cand->candidate);
    mCall.mChat.sendCommand(std::move(msg));
}

void Session::onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    SUB_LOG_DEBUG("ICE connstate changed to %s", iceStateToStr(state));
    if (mState >= Session::kStateTerminating)
        return;
/*  kIceNew, kIceGathering, kIceWaiting, kIceChecking, kIceConnected,
    kIceCompleted, kIceFailed, kIceClosed
 */

    if (state == webrtc::PeerConnectionInterface::kIceConnectionClosed)
    {
        terminateAndDestroy(TermCode::kErrIceDisconn);
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
    {
        terminateAndDestroy(TermCode::kErrIceFail);
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
    {
        terminateAndDestroy(TermCode::kErrIceDisconn);
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionConnected)
    {
        mTsIceConn = time(NULL);
        mCall.notifySessionConnected(*this);
    }
}

void Session::onIceComplete()
{
    SUB_LOG_DEBUG("onIceComplete");
}
void Session::onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
{
    SUB_LOG_DEBUG("onSignalingStateChange: %d", newState);
}
void Session::onDataChannel(webrtc::DataChannelInterface*)
{}

//end of event handlers

// stats interface
//====
void Session::sendAv(AvFlags av)
{
    cmd(RTCMD_MUTE, av.value());
}
Promise<void> Session::sendOffer()
{
    assert(mIsJoiner); // the joiner sends the SDP offer
    assert(mPeerAnonId);
    createRtcConn();
    auto wptr = weakHandle();
    return mRtcConn.createOffer(pcConstraints())
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> Promise<void>
    {
        if (wptr.deleted())
            return promise::_Void();
    /*  if (self.state !== SessState.kWaitSdpAnswer) {
            return;
        }
    */
        KR_THROW_IF_FALSE(sdp->ToString(&mOwnSdp));
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, this]()
    {
        if (wptr.deleted())
            return;
        SdpKey encKey;
        mCall.mManager.crypto().encryptKeyTo(mPeer, mOwnSdpKey, encKey);
        SdpKey hash;
        mCall.mManager.crypto().mac(mOwnSdp, mPeerSdpKey, hash);

        // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        cmd(RTCMD_SDP_OFFER,
            mCall.mManager.mOwnAnonId,
            encKey,
            hash,
            mCall.mLocalStream->effectiveAv().value(),
            static_cast<uint16_t>(mOwnSdp.size()),
            mOwnSdp
        );
        assert(mState == Session::kStateWaitSdpAnswer);
    })
    .fail([wptr, this](const promise::Error& err)
    {
        if (!wptr.deleted())
            return;
        terminateAndDestroy(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
    });
}

void Session::msgSdpOfferSendAnswer(RtMessage& packet)
{
    // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
    if (mState != Session::kStateWaitSdpOffer)
    {
        SUB_LOG_WARNING("Ignoring unexpected SDP offer while in state %s", stateStr());
        return;
    }
    assert(!mIsJoiner);
    // The peer is likely to send ICE candidates immediately after the offer,
    // but we can't process them until setRemoteDescription is ready, so
    // we have to store them in a queue
    setState(Session::kStateWaitLocalSdpAnswer);
    mPeerAnonId = packet.payload.read<uint64_t>(8);
    SdpKey encKey;
    packet.payload.read(16, encKey);
    mCall.mManager.crypto().decryptKeyFrom(mPeer, encKey, mPeerSdpKey);
    mPeerAv = packet.payload.read<uint8_t>(80);
    uint16_t sdpLen = packet.payload.read<uint16_t>(81);
    assert((int) packet.payload.dataSize() >= 83 + sdpLen);
    packet.payload.read(83, sdpLen, mPeerSdp);
    SdpKey hash;
    packet.payload.read(48, hash); //have to read it to a buffer first, to avoid alignment issues if directly typecasting the buffer pointer
    if (!verifySdpFingerprints(mPeerSdp, hash))
    {
        SUB_LOG_ERROR("Fingerprint verification error, immediately terminating session");
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forge attempt");
        return;
    }
    mungeSdp(mPeerSdp);
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* sdp = webrtc::CreateSessionDescription("offer", mPeerSdp, &error);
    if (!sdp)
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP offer: line="+error.line+"\nError: "+error.description);
        return;
    }
    auto wptr = weakHandle();
    mRtcConn.setRemoteDescription(sdp)
    .fail([this](const promise::Error& err)
    {
        return promise::Error(err.msg(), 1, kErrSetSdp); //we signal 'remote' (i.e. protocol) error with errCode == 1
    })
    .then([this, wptr]() -> Promise<webrtc::SessionDescriptionInterface*>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return promise::Error("Session killed");
        return mRtcConn.createAnswer(pcConstraints());
    })
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> Promise<void>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return promise::Error("Session killed");

        sdp->ToString(&mOwnSdp);
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, this]()
    {
        SdpKey ownFprHash;
        // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
        mCall.mManager.crypto().mac(mOwnSdp, mPeerSdpKey, ownFprHash);
        cmd(
            RTCMD_SDP_ANSWER,
            ownFprHash,
            mCall.mLocalStream->effectiveAv().value(),
            static_cast<uint16_t>(mOwnSdp.size()),
            mOwnSdp
        );
    })
    .fail([wptr, this](const promise::Error& err)
    {
        if (wptr.deleted())
            return;
        // cmd() doesn't throw, so we are here because of other error
        std::string msg;
        if (err.type() == kErrSetSdp && err.code() == 1) {
            msg = "Error accepting remote SDP offer: " + err.msg();
        } else {
            msg = "Error creating SDP answer: " + err.msg();
        }
        terminateAndDestroy(TermCode::kErrSdp, msg);
    });
}

void Session::msgSdpAnswer(RtMessage& packet)
{
    if (mState != Session::kStateWaitSdpAnswer)
    {
        SUB_LOG_WARNING("Ingoring unexpected SDP_ANSWER");
        return;
    }
    // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
    mPeerAv.set(packet.payload.read<uint8_t>(40));
    auto sdpLen = packet.payload.read<uint16_t>(41);
    assert((int)packet.payload.dataSize() >= sdpLen + 43);
    packet.payload.read(43, sdpLen, mPeerSdp);
    SdpKey encKey;
    packet.payload.read(8, encKey);
    if (!verifySdpFingerprints(mPeerSdp, encKey))
    {
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forgery");
        return;
    }
    mungeSdp(mPeerSdp);
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* sdp = webrtc::CreateSessionDescription("answer", mPeerSdp, &error);
    if (!sdp)
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP answer: line="+error.line+"\nError: "+error.description);
        return;
    }
    auto wptr = weakHandle();
    mRtcConn.setRemoteDescription(sdp)
    .then([this, wptr]() -> Promise<void>
    {
        if (mState > Session::kStateInProgress)
            return promise::Error("Session killed");
        setState(Session::kStateInProgress);
        return promise::_Void();
    })
    .fail([wptr, this](const promise::Error& err)
    {
        std::string msg = "Error setting SDP answer: " + err.msg();
        terminateAndDestroy(TermCode::kErrSdp, msg);
    });
}

template<class... Args>
bool Session::cmd(uint8_t type, Args... args)
{
    RtMessageComposer msg(OP_RTMSG_ENDPOINT, type, mCall.mChat.chatId(), mPeer, mPeerClient);
    msg.payloadAppend(mSid, args...);
    if (!mCall.mChat.sendCommand(std::move(msg)))
    {
        if (mState < kStateTerminating)
        {
            mCall.destroy(TermCode::kErrNetSignalling, false);
        }
        return false;
    }
    return true;
}
void Session::asyncDestroy(TermCode code, const std::string& msg)
{
    auto wptr = weakHandle();
    marshallCall([this, wptr, code, msg]()
    {
        if (wptr.deleted())
            return;
        destroy(code, msg);
    }, mManager.mClient.appCtx);
}

Promise<void> Session::terminateAndDestroy(TermCode code, const std::string& msg)
{
    if (mState == Session::kStateTerminating)
        return mTerminatePromise;

    if (mState == kStateDestroyed)
        return promise::_Void();

    if (!msg.empty())
    {
        SUB_LOG_DEBUG("Terminating due to: %s", msg.c_str());
    }
    assert(!mTerminatePromise.done());
    setState(kStateTerminating);
    if (!cmd(RTCMD_SESS_TERMINATE, code))
    {
        if (!mTerminatePromise.done())
        {
            auto pms = mTerminatePromise;
            pms.resolve();
            return pms;
        }
    }
    auto wptr = weakHandle();
    setTimeout([wptr, this]()
    {
        if (wptr.deleted() || mState != Session::kStateTerminating)
            return;

        if (!mTerminatePromise.done())
        {
            SUB_LOG_WARNING("Terminate ack didn't arrive withing timeout, destroying session anyway");
            auto pms = mTerminatePromise;
            pms.resolve();
        }
    }, 1000, mManager.mClient.appCtx);
    auto pms = mTerminatePromise;
    return pms
    .then([wptr, this, code, msg]()
    {
        if (wptr.deleted())
            return;
        destroy(code, msg);
    });
}

void Session::msgSessTerminateAck(RtMessage& packet)
{
    if (mState != kStateTerminating)
    {
        SUB_LOG_WARNING("Ignoring unexpected TERMINATE_ACK");
        return;
    }
    if (!mTerminatePromise.done())
    {
        // resolve() will destroy the session and mTermiatePromise promise itself.
        // although promises are refcounted, mTerminatePromise will point to an
        // invalid shared instance upon return from the destroying handler, i.e.
        // 'this' will be an invalid pointer upon return from the promise handler
        // that destroys the session, resulting in a crash inside the promise lib.
        // Therefore, we need to do the resolve on a copy of the promise object
        // (pointing to the same shared promise instance), that outlives the
        // destruction of mTerminatePromise
        auto pms = mTerminatePromise;
        pms.resolve();
    }
}

void Session::msgSessTerminate(RtMessage& packet)
{
    // sid.8 termcode.1
    assert(packet.payload.dataSize() >= 1);
    cmd(RTCMD_SESS_TERMINATE_ACK);

    if (mState == kStateTerminating)
    {
        // handle terminate as if it were an ack - in both cases the peer is terminating
        msgSessTerminateAck(packet);
    }
    else if (mState == kStateDestroyed)
    {
        SUB_LOG_WARNING("Ignoring SESS_TERMINATE for a dead session");
        return;
    }

    setState(kStateTerminating);
    destroy(static_cast<TermCode>(packet.payload.read<uint8_t>(8) | TermCode::kPeer));
}

/** Terminates a session without the signalling terminate handshake.
  * This should normally not be called directly, but via terminate(),
  * unless there is a network error
  */
void Session::destroy(TermCode code, const std::string& msg)
{
    assert(code != TermCode::kInvalid);
    if (mState >= kStateDestroyed)
    {
        SUB_LOG_ERROR("Session::destroy(): Already destroyed");
        return;
    }
    if (!msg.empty()) {
        SUB_LOG_DEBUG("Destroying session due to:", msg.c_str());
    }

    submitStats(code, msg);

    if (mRtcConn)
    {
        if (mRtcConn->signaling_state() != webrtc::PeerConnectionInterface::kClosed)
        {
            mRtcConn->Close();
        }
        mRtcConn.release();
    }

    mRemotePlayer.reset();
    FIRE_EVENT(SESS, onRemoteStreamRemoved);
    setState(kStateDestroyed);
    FIRE_EVENT(SESS, onSessDestroy, static_cast<TermCode>(code & (~TermCode::kPeer)),
        !!(code & TermCode::kPeer), msg);
    mCall.removeSession(*this, code);
}

void Session::submitStats(TermCode termCode, const std::string& errInfo)
{
    std::string deviceInformation = getDeviceInfo();

    stats::StatSessInfo info(mSid, termCode, errInfo, deviceInformation);
    if (mIsJoiner)
    { // isJoiner means answerer
        info.isCaller = false;
        info.caid = mPeerAnonId;
        info.aaid = mCall.mManager.mOwnAnonId;
    }
    else
    {
        info.isCaller = true;
        info.caid = mCall.mManager.mOwnAnonId;
        info.aaid = mPeerAnonId;
    }

    std::string stats = mStatRecorder->terminate(info);
    mCall.mManager.mClient.api.sdk.sendChatStats(stats.c_str());
    return;
}

// we actually verify the whole SDP, not just the fingerprints
bool Session::verifySdpFingerprints(const std::string& sdp, const SdpKey& peerHash)
{
    SdpKey hash;
    mCall.mManager.crypto().mac(sdp, mOwnSdpKey, hash);
    bool match = true; // constant time compare
    for (unsigned int i = 0; i < sizeof(SdpKey); i++)
    {
        match &= (hash.data[i] == peerHash.data[i]);
    }
    return match;
}

void Session::msgIceCandidate(RtMessage& packet)
{
    assert(!mPeerSdp.empty());
    // sid.8 mLineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
    auto mLineIdx = packet.payload.read<uint8_t>(8);
    auto midLen = packet.payload.read<uint8_t>(9);
    if (midLen > packet.payload.dataSize() - 11)
        throw new Error("Invalid ice candidate packet: midLen spans beyond data length");

    std::string mid;
    if (midLen)
    {
        packet.payload.read(10, midLen, mid);
    }
    auto candLen = packet.payload.read<uint16_t>(10 + midLen);
    assert((int) packet.payload.dataSize() >= 12 + midLen + candLen);
    std::string strCand;
    packet.payload.read(midLen + 12, candLen, strCand);

    webrtc::SdpParseError err;
    std::unique_ptr<webrtc::IceCandidateInterface> cand(webrtc::CreateIceCandidate(mid, mLineIdx, strCand, &err));
    if (!cand)
        throw runtime_error("Error parsing ICE candidate:\nline: '"+err.line+"'\nError:" +err.description);
/*
    if (!cand)
    {
        SUB_LOG_ERROR("NULL ice candidate");
        return;
    }
*/
    if (!mRtcConn->AddIceCandidate(cand.get()))
    {
        terminateAndDestroy(TermCode::kErrProtocol);
    }
}

void Session::msgMute(RtMessage& packet)
{
    auto oldAv = mPeerAv;
    mPeerAv.set(packet.payload.read<uint8_t>(8));
    mRemotePlayer->enableVideo(mPeerAv.video());

    FIRE_EVENT(SESS, onPeerMute, mPeerAv, oldAv);
}

void Session::mungeSdp(std::string& sdp)
{
    try
    {
        auto& maxbr = mCall.chat().isGroup() ? mCall.mManager.maxBr : mCall.mManager.maxGroupBr;
        if (maxbr)
        {
            SUB_LOG_WARNING("mungeSdp: Limiting peer's send video send bitrate to %d kbps", maxbr);
            sdpSetVideoBw(sdp, maxbr);
        }
    }
    catch(std::exception& e)
    {
        SUB_LOG_ERROR("mungeSdp: Exception: %s", e.what());
        throw;
    }
}
Session::~Session()
{
    SUB_LOG_DEBUG("Destroyed");
}

bool Session::isTermRetriable(TermCode reason)
{
    return (reason & 0x7f) != TermCode::kErrPeerOffline;
}

#define RET_ENUM_NAME(name) case name: return #name

const char* ICall::stateToStr(uint8_t state)
{
    switch(state)
    {
        RET_ENUM_NAME(kStateInitial);
        RET_ENUM_NAME(kStateHasLocalStream);
        RET_ENUM_NAME(kStateReqSent);
        RET_ENUM_NAME(kStateRingIn);
        RET_ENUM_NAME(kStateJoining);
        RET_ENUM_NAME(kStateInProgress);
        RET_ENUM_NAME(kStateTerminating);
        RET_ENUM_NAME(kStateDestroyed);
        default: return "(invalid call state)";
    }
}

const char* ISession::stateToStr(uint8_t state)
{
    switch(state)
    {
        RET_ENUM_NAME(kStateInitial);
        RET_ENUM_NAME(kStateWaitSdpOffer);
        RET_ENUM_NAME(kStateWaitSdpAnswer);
        RET_ENUM_NAME(kStateWaitLocalSdpAnswer);
        RET_ENUM_NAME(kStateInProgress);
        RET_ENUM_NAME(kStateTerminating);
        RET_ENUM_NAME(kStateDestroyed);
        default: return "(invalid session state)";
    }
}

void StateDesc::assertStateChange(uint8_t oldState, uint8_t newState) const
{
    if (oldState >= transMap.size())
        throw std::runtime_error(std::string("assertStateChange: Invalid old state ")+toStrFunc(oldState));
    auto allowed = transMap[oldState];
    if (newState >= transMap.size())
        throw std::runtime_error(std::string("assertStateChange: Invalid new state ")+toStrFunc(newState));
    for (auto a: allowed)
    {
        if (newState == a)
            return;
    }
    throw std::runtime_error(std::string("assertStateChange: Invalid state transition ")+toStrFunc(oldState)+" -> "+toStrFunc(newState));
}

const StateDesc Call::sStateDesc = {
    .transMap = {
        { kStateReqSent, kStateHasLocalStream, kStateTerminating }, //for kStateInitial
        { kStateJoining, kStateReqSent, kStateTerminating }, //for kStateHasLocalStream
        { kStateInProgress, kStateTerminating },             //for kStateReqSent
        { kStateHasLocalStream, kStateInProgress,            //for kStateRingIn
          kStateTerminating },
        { kStateInProgress, kStateTerminating },             //for kStateJoining
        { kStateTerminating },                               //for kStateInProgress,
        { kStateDestroyed },                                 //for kStateTerminating,
        {}                                                   //for kStateDestroyed
    },
    .toStrFunc = Call::stateToStr
};

const StateDesc Session::sStateDesc = {
    .transMap = {
        { kStateWaitSdpOffer, kStateWaitSdpAnswer},
        { kStateWaitLocalSdpAnswer, kStateTerminating }, //for kSWaitSdpOffer
        { kStateInProgress, kStateTerminating },         //for kStateWaitLocalSdpAnswer
        { kStateInProgress, kStateTerminating },               //for kStateWaitSdpAnswer
        { kStateTerminating },                           //for kStateInProgress
        { kStateDestroyed },                             //for kStateTerminating
        {}                                               //for kStateDestroyed
    },
    .toStrFunc = Session::stateToStr
};

const char* rtcmdTypeToStr(uint8_t type)
{
    switch(type)
    {
        RET_ENUM_NAME(RTCMD_CALL_RINGING);
        RET_ENUM_NAME(RTCMD_CALL_REQ_DECLINE);
        RET_ENUM_NAME(RTCMD_CALL_REQ_CANCEL);
        RET_ENUM_NAME(RTCMD_CALL_TERMINATE); // hangup existing call, cancel call request. Works on an existing call
        RET_ENUM_NAME(RTCMD_JOIN); // join an existing/just initiated call. There is no call yet, so the command identifies a call request
        RET_ENUM_NAME(RTCMD_SESSION); // join was accepter and the receiver created a session to joiner
        RET_ENUM_NAME(RTCMD_SDP_OFFER); // joiner sends an SDP offer
        RET_ENUM_NAME(RTCMD_SDP_ANSWER); // joinee answers with SDP answer
        RET_ENUM_NAME(RTCMD_ICE_CANDIDATE); // both parties exchange ICE candidates
        RET_ENUM_NAME(RTCMD_SESS_TERMINATE); // initiate termination of a session
        RET_ENUM_NAME(RTCMD_SESS_TERMINATE_ACK); // acknowledge the receipt of SESS_TERMINATE, so the sender can safely stop the stream and
        // it will not be detected as an error by the receiver
        RET_ENUM_NAME(RTCMD_MUTE);
        default: return "(invalid RTCMD)";
    }
}
const char* termCodeToStr(uint8_t code)
{
    switch(code)
    {
        RET_ENUM_NAME(kUserHangup);
        RET_ENUM_NAME(kCallRejected);
        RET_ENUM_NAME(kAnsElsewhere);
        RET_ENUM_NAME(kRejElsewhere);
        RET_ENUM_NAME(kAnswerTimeout);
        RET_ENUM_NAME(kRingOutTimeout);
        RET_ENUM_NAME(kAppTerminating);
        RET_ENUM_NAME(kCallGone);
        RET_ENUM_NAME(kBusy);
        RET_ENUM_NAME(kNormalHangupLast);
        RET_ENUM_NAME(kErrApiTimeout);
        RET_ENUM_NAME(kErrFprVerifFailed);
        RET_ENUM_NAME(kErrProtoTimeout);
        RET_ENUM_NAME(kErrProtocol);
        RET_ENUM_NAME(kErrInternal);
        RET_ENUM_NAME(kErrLocalMedia);
        RET_ENUM_NAME(kErrNoMedia);
        RET_ENUM_NAME(kErrNetSignalling);
        RET_ENUM_NAME(kErrIceDisconn);
        RET_ENUM_NAME(kErrIceFail);
        RET_ENUM_NAME(kErrSdp);
        RET_ENUM_NAME(kErrPeerOffline);
        RET_ENUM_NAME(kErrSessRetryTimeout);
        RET_ENUM_NAME(kInvalid);
        RET_ENUM_NAME(kNotFinished);
        default: return "(invalid term code)";
    }
}
#define RET_ICE_CONN(name) \
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnection##name: return #name

const char* iceStateToStr(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    switch (state)
    {
        RET_ICE_CONN(New);
        RET_ICE_CONN(Checking);
        RET_ICE_CONN(Connected);
        RET_ICE_CONN(Completed);
        RET_ICE_CONN(Failed);
        RET_ICE_CONN(Disconnected);
        RET_ICE_CONN(Closed);
        default: return "(invalid ICE connection state)";
    }
}
std::string rtmsgCommandToString(const StaticBuffer& buf)
{
    //opcode.1 chatid.8 userid.8 clientid.4 len.2 type.1 data.(len-1)
    auto opcode = buf.read<uint8_t>(0);
    Id chatid = buf.read<uint64_t>(1);
    Id userid = buf.read<uint64_t>(9);
    auto clientid = buf.read<uint32_t>(17);
    auto dataLen = buf.read<uint16_t>(21);
    auto type = buf.read<uint8_t>(23);
    std::string result = Command::opcodeToStr(opcode);
    result.append(": ").append(rtcmdTypeToStr(type));
    result.append(" chatid: ").append(chatid.toString())
          .append(" userid: ").append(userid.toString())
          .append(" clientid: ").append(std::to_string(clientid));
    StaticBuffer data(buf.buf()+23, dataLen);
    switch (type)
    {
        case RTCMD_CALL_REQ_DECLINE:
        case RTCMD_CALL_REQ_CANCEL:
            result.append(" reason: ").append(termCodeToStr(data.read<uint8_t>(8)));
        case RTCMD_CALL_RINGING:
            result.append(" callid: ").append(Id(data.read<uint64_t>(0)).toString());
            break;
        case RTCMD_CALL_TERMINATE:
            result.append(" reason: ").append(termCodeToStr(data.read<uint8_t>(0)));
            break;
        case RTCMD_JOIN:
            result.append(" anonId: ").append(Id(data.read<uint64_t>(0)).toString());
            break;
        case RTCMD_SESSION:
            result.append(" callid: ").append(Id(data.read<uint64_t>(0)).toString())
            .append(" sid: ").append(Id(data.read<uint64_t>(8)).toString())
            .append(" ownAnonId: ").append(Id(data.read<uint64_t>(16)).toString());
            break;
        case RTCMD_SDP_OFFER:
        case RTCMD_SDP_ANSWER:
        case RTCMD_ICE_CANDIDATE:
        case RTCMD_SESS_TERMINATE:
        case RTCMD_SESS_TERMINATE_ACK:
        case RTCMD_MUTE:
            result.append(" sid: ").append(Id(buf.read<uint64_t>(0)).toString());
            break;
    }
    return result;
}
void Session::sdpSetVideoBw(std::string& sdp, int maxbr)
{
    std::regex expresion("m=video.*", std::regex::icase);
    std::smatch m;
    if (!regex_search(sdp, m, expresion))
    {
        SUB_LOG_WARNING("setVideoQuality: m=video line not found in local SDP");
        return;
    }

    assert(m.size());
    std::string line = "\r\nb=AS:" + std::to_string(maxbr);
    sdp.insert(m.position(0) + m.length(0), line);
}
void globalCleanup()
{
    if (!artc::isInitialized())
        return;
    artc::cleanup();
}
}
