#ifdef WIN32
#include <WinSock2.h> // for htonll, needed in webrtc\rtc_base\byteorder.h
#endif

#include "webrtc.h"
#include "webrtcPrivate.h"
#include <chatClient.h>
#include <timers.hpp>
#include "rtcCrypto.h"
#include "streamPlayer.h"
#include "rtcStats.h"

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
  mStaticIceServers(iceServers),
  mIceServerProvider(client.api, "turn"),
  mManager(*this)
{
    if (!artc::isInitialized())
    {
        artc::init(client.appCtx);
        RTCM_LOG_DEBUG("WebRTC stack initialized before first use");
    }

    //preload ice servers to make calls faster
    initInputDevices();

    mWebRtcLogger.reset(new WebRtcLogger(mKarereClient.api, mOwnAnonId.toString(), getDeviceInfo()));
}

void RtcModule::init()
{
    // set cached ICE servers, or static/hardcoded if not available
    StaticProvider iceServersStatic;
    string jsonCachedTurnServers = getCachedTurnServers();
    if (jsonCachedTurnServers.size())
    {
        iceServersStatic.setServers(jsonCachedTurnServers.c_str());
    }
    else
    {
        iceServersStatic.setServers(mStaticIceServers);
    }
    setIceServers(iceServersStatic);

    // fetch ICE servers URLs from Load Balancer (GeLB)
    auto wptr = weakHandle();
    mIceServerProvider.fetchServers()
    .then([wptr, this]()
    {
        if (wptr.deleted())
            return;

        // replace cache ICE servers URLs with received ones
        int shard = TURNSERVER_SHARD;
        for (const std::shared_ptr<TurnServerInfo>& serverInfo : mIceServerProvider)
        {
            std::string fullUrl = serverInfo->url;
            if (fullUrl.size())
            {
                // Example: "turn:example.url.co:3478?transport=udp"
                size_t posInitialColon = fullUrl.find(":") + 1;
                std::string urlString = fullUrl.substr(posInitialColon, fullUrl.size() - posInitialColon);
                karere::Url url(urlString);

                if (!mKarereClient.mDnsCache.isValidUrl(shard) || url != mKarereClient.mDnsCache.getUrl(shard))
                {
                    if (mKarereClient.mDnsCache.isValidUrl(shard))
                    {
                        mKarereClient.mDnsCache.removeRecord(shard);
                    }

                    mKarereClient.mDnsCache.addRecord(shard, urlString);
                }

                shard--;
            }
        }

        // discard any obsolete cached URL (not returned by GeLB)
        int maxShard = TURNSERVER_SHARD - MAX_TURN_SERVERS;
        while (mKarereClient.mDnsCache.isValidUrl(shard) && shard > maxShard)
        {
            mKarereClient.mDnsCache.removeRecord(shard);
            shard--;
        }

        // finally, update the IPs corresponding to the received URLs
        if (mIceServerProvider.size())
        {
            refreshTurnServerIp();
        }
    })
    .fail([](const ::promise::Error& err)
    {
        KR_LOG_ERROR("Gelb failed with error '%s', using static server list", err.what());
    });

    mKarereClient.mChatdClient->setRtcHandler(this);
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
    std::set<std::pair<std::string, std::string>> videoDevices = loadDeviceList();
    if (!videoDevices.empty())
    {
        mVideoDeviceSelected = videoDevices.begin()->second;
        RTCM_LOG_DEBUG("Video device selected -> %s", videoDevices.begin()->second.c_str());
    }
}

void RtcModule::removeCallRetry(karere::Id chatid, bool retry)
{
    auto retryCalltimerIt = mRetryCallTimers.find(chatid);
    if (retryCalltimerIt != mRetryCallTimers.end())
    {
        cancelTimeout(retryCalltimerIt->second, mKarereClient.appCtx);
        mRetryCallTimers.erase(retryCalltimerIt);

        if (!retry)
        {
            auto callHandlerIt = mCallHandlers.find(chatid);
            assert(callHandlerIt != mCallHandlers.end());
            callHandlerIt->second->setReconnectionFailed();
        }
    }

    mRetryCall.erase(chatid);
}

string RtcModule::getCachedTurnServers()
{
    int i = 0;
    vector<string> urls;
    while (mKarereClient.mDnsCache.isValidUrl(TURNSERVER_SHARD - i) && i < MAX_TURN_SERVERS)
    {
        string ipv4;
        string ipv6;
        karere::Url turnServerUrl = mKarereClient.mDnsCache.getUrl(TURNSERVER_SHARD - i);
        if (mKarereClient.mDnsCache.getIp(TURNSERVER_SHARD - i, ipv4, ipv6))
        {
            if (ipv4.size())
            {
                urls.push_back(buildTurnServerUrl(ipv4, turnServerUrl.port, turnServerUrl.path));
            }

            if(ipv6.size())
            {
                urls.push_back(buildTurnServerUrl(ipv6, turnServerUrl.port, turnServerUrl.path));
            }
        }
        else    // no cached IP available for this URL
        {
            urls.push_back(buildTurnServerUrl(turnServerUrl.host, turnServerUrl.port, turnServerUrl.path));

        }
        i++;
    }

    string json;
    if (urls.size())
    {
        json.insert(json.begin(), '[');
        for (const string& url : urls)
        {
            if (json.size() > 2)
            {
                json.append(", ");
            }

            json.append("{\"host\":\"")
                    .append(url)
                    .append("\"}");
        }

        json.append("]");
    }

    return json;
}

string RtcModule::buildTurnServerUrl(const string &host, int port, const string &path) const
{
    string url("turn:");
    url.append(host);
    if (port > 0)
    {
        url.append(":")
                .append(std::to_string(port));
    }

    if (path.size())
    {
        url.append(path);
    }

    return url;
}

bool RtcModule::selectAudioInDevice(const string &devname)
{
    return false;
}

std::set<std::pair<std::string, std::string>> RtcModule::loadDeviceList() const
{
    return artc::VideoManager::getVideoDevices();
}

string RtcModule::getVideoDeviceSelected()
{
    std::set<std::pair<std::string, std::string>> videoDevices = loadDeviceList();
    for (const std::pair<std::string, std::string> &device : videoDevices)
    {
        if (mVideoDeviceSelected == device.second)
        {
            return device.first;
        }
    }

    return std::string();
}

bool RtcModule::selectVideoInDevice(const string &devname)
{
    std::set<std::pair<std::string, std::string>> videoDevices = loadDeviceList();
    for (const std::pair<std::string, std::string> &device : videoDevices)
    {
        if (devname == device.first)
        {
            mVideoDeviceSelected = device.second;
            for (auto callIt : mCalls)
            {
                if (callIt.second->state() >= Call::kStateHasLocalStream && callIt.second->sentFlags().video())
                {
                    callIt.second->changeVideoInDevice();
                }
            }

            return true;
        }
    }

    return false;
}

int RtcModule::setIceServers(const ServerList &servers)
{
    if (servers.empty())
        return 0;

    webrtc::PeerConnectionInterface::IceServers rtcServers;
    for (auto& server: servers)
    {
        webrtc::PeerConnectionInterface::IceServer rtcServer = createIceServer(*server);
        KR_LOG_DEBUG("setIceServers::Adding ICE server: '%s'", rtcServer.uri.c_str());
        rtcServers.push_back(rtcServer);
    }

    mIceServers.swap(rtcServers);
    return (int)(mIceServers.size());
}

void RtcModule::addIceServers(const ServerList &servers)
{
    if (servers.empty())
        return;

    for (auto& server: servers)
    {
        bool serverFound = false;
        for (unsigned int i = 0; i < mIceServers.size(); i++)
        {
            if (mIceServers[i].uri == server->url)
            {
                serverFound = true;
                break;
            }
        }

        if (!serverFound)
        {
            webrtc::PeerConnectionInterface::IceServer rtcServer = createIceServer(*server);
            KR_LOG_DEBUG("addIceServers::Adding ICE server: '%s'", rtcServer.uri.c_str());
            mIceServers.push_back(rtcServer);
        }
    }
}

webrtc::PeerConnectionInterface::IceServer RtcModule::createIceServer(const TurnServerInfo &serverInfo)
{
    webrtc::PeerConnectionInterface::IceServer rtcServer;
    rtcServer.uri = serverInfo.url;

    if (!serverInfo.user.empty())
        rtcServer.username = serverInfo.user;
    else
        rtcServer.username = KARERE_TURN_USERNAME;

    if (!serverInfo.pass.empty())
        rtcServer.password = serverInfo.pass;
    else
        rtcServer.password = KARERE_TURN_PASSWORD;

    return rtcServer;
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
            RTCM_LOG_DEBUG("Received %s for a chat that doesn't currently have a call, ignoring", packet.typeStr());
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
    uint8_t flag = msg.read<uint8_t>(sizeof(karere::Id) + sizeof(uint8_t));
    AvFlags avFlagsRemote = flag & ~Call::kFlagRinging;
    bool ringing = flag & Call::kFlagRinging;
    RTCM_LOG_DEBUG("Handle CALLDATA: callid -> %s - state -> %d  - ringing -> %d", callid.toString().c_str(), state, ringing);

    if (userid == chat.client().mKarereClient->myHandle()
        && clientid == chat.connection().clientId())
    {
        RTCM_LOG_ERROR("Ignoring CALLDATA sent back to sender");
        return;
    }

    //Compatibility
    if (state == Call::CallDataState::kCallDataRinging || state == Call::CallDataState::kCallDataSessionKeepRinging)
    {
        ringing = true;
    }
    else if (state == Call::CallDataState::kCallDataEnd)
    {
        auto itCall = mCalls.find(chatid);
        if (itCall == mCalls.end())
        {
            return;
        }

        if (!chat.isGroup() && itCall->second->state() < Call::kStateTerminating)
        {
            itCall->second->destroy(static_cast<TermCode>(TermCode::kUserHangup | TermCode::kPeer), "Terminating when there isn't session");
            return;
        }

        EndpointId endPointId(userid, clientid);
        if (chat.isGroup() && itCall->second->mSessRetries.find(endPointId) != itCall->second->mSessRetries.end())
        {
            itCall->second->cancelSessionRetryTimer(userid, clientid);
            itCall->second->destroyIfNoSessionsOrRetries(static_cast<TermCode>(TermCode::kUserHangup | TermCode::kPeer));
            return;
        }

        // Peer will be removed from call participants with OP_ENDCALL
        RTCM_LOG_DEBUG("Ignoring kCallDataEnd CALLDATA: %s", callid.toString().c_str());
        return;
    }

    auto itCall = mCalls.find(chatid);
    if (itCall != mCalls.end())
    {
        if (itCall->second->state() == Call::kStateReqSent && state == Call::CallDataState::kCallDataRinging)
        {
            RTCM_LOG_DEBUG("Call collision resolution. Receive a CALLDATA(kCallDataRinging) with different callid in state kStateReqSent");
            updatePeerAvState(chatid, callid, userid, clientid, avFlagsRemote);
            handleCallDataRequest(chat, userid, clientid, callid, avFlagsRemote);
            return;
        }

        if (itCall->second->id() != callid)
        {
            RTCM_LOG_ERROR("Ignoring CALLDATA because its callid is different than the call that we have in that chatroom");
            return;
        }

        updatePeerAvState(chatid, callid, userid, clientid, avFlagsRemote);

        if (itCall->second->state() == Call::kStateRingIn && itCall->second->isCaller(userid, clientid) && !ringing)
        {
            if (!chat.isGroup())
            {
                RTCM_LOG_ERROR("Received not-terminating CALLDATA with ringing flag to fasle for 1on1 in state kStateRingIn. "
                                 "The call should have already been taken out of this state by a RTMSG");
            }

            itCall->second->destroy(TermCode::kAnswerTimeout, false, "Force destroy the call by CALLDATA, the call should be destroyed by RTMSG");
        }
    }
    else
    {
        updatePeerAvState(chatid, callid, userid, clientid, avFlagsRemote);
        if (ringing)
        {
            auto itCallHandler = mCallHandlers.find(chatid);
            // itCallHandler is created at updatePeerAvState
            assert(itCallHandler != mCallHandlers.end());
            if (!itCallHandler->second->isParticipating(mKarereClient.myHandle()) && !itCallHandler->second->hasBeenNotifiedRinging())
            {
                handleCallDataRequest(chat, userid, clientid, callid, avFlagsRemote);
            }
        }
    }

    if (state == Call::CallDataState::kCallDataSession || state == Call::CallDataState::kCallDataSessionKeepRinging)
    {
        auto itCallHandler = mCallHandlers.find(chatid);
        if (itCallHandler != mCallHandlers.end() && !itCallHandler->second->getInitialTimeStamp())
        {
            itCallHandler->second->setInitialTimeStamp(time(NULL));
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

void RtcModule::getAudioInDevices(std::vector<std::string>& /*devices*/) const
{

}

void RtcModule::getVideoInDevices(std::set<std::string>& deviceNameIds) const
{
    std::set<std::pair<std::string, std::string>> devices = loadDeviceList();
    for (const std::pair<std::string, std::string> &device : devices)
    {
        deviceNameIds.insert(device.first);
    }
}

std::shared_ptr<Call> RtcModule::startOrJoinCall(karere::Id chatid, AvFlags av,
    ICallHandler& handler, Id callid)
{
    karere::Id id = callid.isValid() ? callid : (karere::Id)random<uint64_t>();
    auto& chat = mKarereClient.mChatdClient->chats(chatid);
    auto callIt = mCalls.find(chatid);
    if (callIt != mCalls.end())
    {
        if (callIt->second->state() == Call::kStateRingIn)
        {
            callIt->second->answer(av);
            return callIt->second;
        }
        else
        {
            RTCM_LOG_ERROR("There is already a call in this chatroom, destroying it");
            removeCall(*callIt->second);
        }
    }

    bool callRecovered = (mRetryCall.find(chatid) != mRetryCall.end());
    auto call = std::make_shared<Call>(*this, chat, id,
        chat.isGroup(), callid.isValid(), &handler, 0, 0, callRecovered);

    mCalls[chatid] = call;
    handler.setCall(call.get());
    call->startOrJoin(av);
    return call;
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

void RtcModule::launchCallRetry(Id chatid, AvFlags av, bool isActiveRetry)
{
    auto itRetryCall = mRetryCall.find(chatid);
    if (itRetryCall != mRetryCall.end())
    {
        if (isActiveRetry && !itRetryCall->second.second)
        {
            RTCM_LOG_DEBUG("Stop reconnection call in pasive mode and launch in active");
            auto itRetryTimerHandle = mRetryCallTimers.find(chatid);
            assert(itRetryTimerHandle != mRetryCallTimers.end());
            cancelTimeout(itRetryTimerHandle->second, mKarereClient.appCtx);
            mRetryCallTimers.erase(itRetryTimerHandle);

        }
        else
        {
            RTCM_LOG_DEBUG("Avoid launch other reconnection call. In same mode or pasive %s", isActiveRetry ? "Active" : "Pasive");
            return;
        }
    }

    mRetryCall[chatid] = std::pair<karere::AvFlags, bool>(av, isActiveRetry);

    if (mRetryCallTimers.find(chatid) != mRetryCallTimers.end())
    {
        assert(false);
        mKarereClient.api.call(&::mega::MegaApi::sendEvent, 99011, "mRetryCallTimers shouldn't have an element with that chatid");
        return;
    }

    auto itHandler = mCallHandlers.find(chatid);
    assert(itHandler != mCallHandlers.end());
    itHandler->second->onReconnectingState(true);

    if (isActiveRetry)
    {
        RTCM_LOG_DEBUG("Launch reconnection call at active mode");

        auto wptr = weakHandle();
        mRetryCallTimers[chatid] = setTimeout([this, wptr, chatid]()
        {
            if (wptr.deleted())
                return;

            mRetryCallTimers.erase(chatid);
            mRetryCall.erase(chatid);
            auto itHandler = mCallHandlers.find(chatid);
            assert(itHandler != mCallHandlers.end());
            itHandler->second->setReconnectionFailed();
            Chat& chat = mManager.mKarereClient.mChatdClient->chats(chatid);
            itHandler->second->removeParticipant(mManager.mKarereClient.myHandle(), chat.connection().clientId());
            removeCallWithoutParticipants(chatid);
        }, kRetryCallTimeout, mKarereClient.appCtx);
    }
}

WebRtcLogger *RtcModule::getWebRtcLogger()
{
    return mWebRtcLogger.get();
}

string RtcModule::getDeviceInfo()
{
    // UserAgent Format
    // MEGA<app>/<version> (platform) Megaclient/<version>
    std::string userAgent = mKarereClient.api.sdk.getUserAgent();

    std::string androidId = "MEGAAndroid";
    std::string iosId = "MEGAiOS";
    std::string testChatId = "MEGAChatTest";
    std::string syncId = "MEGAsync";
    std::string qtAppId = "MEGAChatQtApp";
    std::string megaClcId = "MEGAclc";

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
    else if ((idPosition = userAgent.find(qtAppId)) != std::string::npos)
    {
        deviceType = "nqtApp";
    }
    else if ((idPosition = userAgent.find(megaClcId)) != std::string::npos)
    {
        deviceType = "nclc";
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

RtcModule::~RtcModule()
{
    mCalls.clear();
    mWebRtcLogger.reset();
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

void RtcModule::handleInCall(karere::Id chatid, karere::Id userid, uint32_t clientid)
{
    auto& chat = mKarereClient.mChatdClient->chats(chatid);

    // Our own flags are set in `Call::startOrJoin and happend before this`
    if (userid != mKarereClient.myHandle() || clientid != chat.connection().clientId())
    {
        updatePeerAvState(chatid, Id::inval(), userid, clientid, AvFlags(false, false));
    }

}

void RtcModule::handleCallTime(karere::Id chatid, uint32_t duration)
{
    auto callHandlerIt = mCallHandlers.find(chatid);
    if (callHandlerIt == mCallHandlers.end())
    {
        ICallHandler *callHandler = mHandler.onGroupCallActive(chatid, karere::Id::inval(), duration);
        addCallHandler(chatid, callHandler);
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

bool RtcModule::isCallInProgress(Id chatid) const
{
    bool callInProgress = false;

    if (chatid.isValid())
    {
        auto it = mCalls.find(chatid);
        if (it != mCalls.end())
        {
            callInProgress = it->second->isInProgress();
        }
    }
    else    // find a call in progress in any chatroom
    {
        for (auto it: mCalls)
        {
            if (it.second->isInProgress())
            {
                callInProgress = true;
                break;
            }
        }
    }

    return callInProgress;
}

bool RtcModule::isCallActive(Id chatid) const
{
    if (chatid.isValid())
    {
        return (mCallHandlers.find(chatid) != mCallHandlers.end());
    }
    else
    {
        return !mCallHandlers.empty();
    }
}

void RtcModule::updatePeerAvState(Id chatid, Id callid, Id userid, uint32_t clientid, AvFlags av)
{
    ICallHandler *callHandler = NULL;

    auto it = mCallHandlers.find(chatid);
    if (it != mCallHandlers.end())  // already known call, update flags
    {
        callHandler = it->second;
        if (callid != Id::inval())
        {
            callHandler->setCallId(callid);
        }
    }
    else    // unknown call, create call handler and set flags
    {
        callHandler = mHandler.onGroupCallActive(chatid, callid);
        addCallHandler(chatid, callHandler);
    }

    callHandler->addParticipant(userid, clientid, av);

    Call *call  = static_cast<Call *>(callHandler->getCall());
    if (call)
    {
        call->updateAvFlags(userid, clientid, av);
    }
}

void RtcModule::removeCall(Id chatid, bool retry)
{
    auto itCall = mCalls.find(chatid);
    Promise<void> pms = promise::_Void();
    if (itCall != mCalls.end())
    {
        if (retry && (itCall->second->state() == Call::kStateJoining || itCall->second->state() == Call::kStateInProgress))
        {
            launchCallRetry(chatid, itCall->second->sentFlags());
        }

        RTCM_LOG_DEBUG("Remove Call on state disconnected: %s", chatid.toString().c_str());
        pms = itCall->second->destroy(TermCode::kErrPeerOffline, false, "Destroy call by network disconnection, call retry has been launched if necessary");
    }

    auto wptr = weakHandle();
    pms.then([wptr, this, chatid, retry]()
    {
        if (wptr.deleted())
            return;

        auto itHandler = mCallHandlers.find(chatid);
        if (itHandler != mCallHandlers.end())
        {
            if (retry || itHandler->second->callParticipants())
            {
                bool reconnectionState = false;
                if (mRetryCall.find(chatid) != mRetryCall.end())
                {
                    reconnectionState = true;
                }

                itHandler->second->removeAllParticipants(reconnectionState);
            }

            if (mRetryCall.find(chatid) == mRetryCall.end())
            {
                delete itHandler->second;
                mCallHandlers.erase(itHandler);
            }
        }
    });
}

void RtcModule::removeCallWithoutParticipants(Id chatid)
{
    auto itHandler = mCallHandlers.find(chatid);
    if (itHandler != mCallHandlers.end())
    {
        if (!itHandler->second->callParticipants() && mRetryCall.find(chatid) == mRetryCall.end())
        {
            delete itHandler->second;
            mCallHandlers.erase(itHandler);
        }
    }
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

void RtcModule::abortCallRetry(Id chatid)
{
    removeCallRetry(chatid, false);
    auto itHandler = mCallHandlers.find(chatid);
    if (itHandler != mCallHandlers.end())
    {
        itHandler->second->onReconnectingState(false);
        Chat& chat = mManager.mKarereClient.mChatdClient->chats(chatid);
        itHandler->second->removeParticipant(mManager.mKarereClient.myHandle(), chat.connection().clientId());
    }

    removeCallWithoutParticipants(chatid);
}

void RtcModule::refreshTurnServerIp()
{
    if (mIceServerProvider.busy())
    {
        RTCM_LOG_WARNING("Turn server URLs not available yet. Fetching...");
        return;
    }

    mDnsRequestId++;

    int index = 0;
    while (mKarereClient.mDnsCache.isValidUrl(TURNSERVER_SHARD - index) && index < MAX_TURN_SERVERS)
    {
        int shard = TURNSERVER_SHARD - index;
        std::string host = mKarereClient.mDnsCache.getUrl(shard).host;
        unsigned int dnsRequestId = mDnsRequestId;  // capture the value for the lambda
        auto wptr = weakHandle();
        mDnsResolver.wsResolveDNS(mKarereClient.websocketIO, host.c_str(),
                                                [wptr, this, shard, dnsRequestId]
                                                (int statusDNS, const std::vector<std::string> &ipsv4, const std::vector<std::string> &ipsv6)
        {
            if (wptr.deleted())
                return;

            if (mKarereClient.isTerminated())
            {
                RTCM_LOG_DEBUG("DNS resolution completed but karere client was terminated.");
                return;
            }

            if (dnsRequestId != mDnsRequestId)
            {
                RTCM_LOG_DEBUG("DNS resolution completed but ignored: a newer attempt is already started (old: %d, new: %d)",
                               dnsRequestId, mDnsRequestId);
                return;
            }

            if (statusDNS < 0)
            {
                RTCM_LOG_ERROR("Async DNS error in rtcModule. Error code: %d", statusDNS);
            }

            assert(mKarereClient.mDnsCache.hasRecord(shard));

            if (statusDNS >= 0 && (ipsv4.size() || ipsv6.size()))
            {
                if (mKarereClient.mDnsCache.setIp(shard, ipsv4, ipsv6))
                {
                    RTCM_LOG_WARNING("New IP for TURN servers: ipv4 - %s     ipv6 - %s",
                                 ipsv4.size() ? ipsv4[0].c_str() : "(empty)",
                                 ipsv6.size() ? ipsv6[0].c_str() : "(empty)");
                }

            }
            else
            {
                mKarereClient.mDnsCache.invalidateIps(shard);
            }

            updateTurnServers();

        });

        index++;
    }
}

void RtcModule::updateTurnServers()
{
    StaticProvider iceServersProvider;
    string jsonCachedTurnServers = getCachedTurnServers();
    iceServersProvider.setServers(jsonCachedTurnServers.c_str());
    setIceServers(iceServersProvider);
}

void RtcModule::onKickedFromChatRoom(Id chatid)
{
    auto callIt = mCalls.find(chatid);
    if (callIt != mCalls.end())
    {
        RTCM_LOG_WARNING("We have been removed from chatroom: %s, and we are in a call. Finishing the call", chatid.toString().c_str());
        auto wptr = weakHandle();
        callIt->second->destroy(TermCode::kErrKickedFromChat, true, "Destroy the call due to we have been removed from chatroom")
        .then([this, chatid, wptr]()
        {
            if (wptr.deleted())
                return;

            auto callHandlerIt = mCallHandlers.find(chatid);
            if (callHandlerIt != mCallHandlers.end())
            {
                RTCM_LOG_WARNING("We have been removed from chatroom %s -> finishing existing call %s",
                                 chatid.toString().c_str(), callHandlerIt->second->getCallId().toString().c_str());
                callHandlerIt->second->removeAllParticipants();
            }

            removeCallWithoutParticipants(chatid);
        });
    }
    else
    {
        auto callHandlerIt = mCallHandlers.find(chatid);
        if (callHandlerIt != mCallHandlers.end())
        {
            RTCM_LOG_WARNING("We have been removed from chatroom %s -> finishing existing call %s",
                             chatid.toString().c_str(), callHandlerIt->second->getCallId().toString().c_str());
            callHandlerIt->second->removeAllParticipants();
        }

        removeCallRetry(chatid, false);
        removeCallWithoutParticipants(chatid);
    }
}

uint32_t RtcModule::clientidFromPeer(karere::Id chatid, Id userid)
{
    auto callIt = mCalls.find(chatid);
    if (callIt != mCalls.end())
    {
        return callIt->second->clientidFromSession(userid);
    }

    return 0;
}

void RtcModule::retryCalls(int shardNo)
{
    for (auto it = mRetryCall.begin(); it != mRetryCall.end();)
    {
        Chat &chat = mKarereClient.mChatdClient->chats(it->first);
        if (chat.connection().shardNo() == shardNo)
        {
            karere::Id chatid = chat.chatId();
            auto itHandler = mCallHandlers.find(chatid);
            karere::AvFlags flags = it->second.first;
            joinCall(chatid, flags, *itHandler->second, itHandler->second->getCallId());
            // It isn't neccesary call onReconnectingState(false) because internal call manage the states
            it++;
            removeCallRetry(chatid);
        }
        else
        {
            it++;
        }
    }
}

void RtcModule::handleCallDataRequest(Chat &chat, Id userid, uint32_t clientid, Id callid, AvFlags avFlagsRemote)
{
    karere::Id chatid = chat.chatId();
    karere::Id myHandle = mKarereClient.myHandle();
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
            if (existingCall->caller() == userid && existingCall->callerClient() == clientid)
            {
                // Skip second CALLDATA when we join to chatroom
                // First callData when we connect
                return;
            }

            RTCM_LOG_DEBUG("handleCallDataRequest: Receive a CALLDATA with other call in progress");
            bool isCallToSameUser = (userid == existingCall->caller());
            existingCall->sendBusy(isCallToSameUser);
            return;
        }
        else if (Id::greaterThanForJs(myHandle, userid))
        {
            RTCM_LOG_DEBUG("handleCallDataRequest: Waiting for the other peer hangup its incoming call and answer our call");
            return;
        }

        // hang up existing call and answer automatically incoming call
        avFlags = existingCall->sentFlags();
        answerAutomatic = true;
        auto itHandler = mCallHandlers.find(chatid);
        if (itHandler != mCallHandlers.end())
        {
            itHandler->second->removeParticipant(userid, clientid);
        }

        existingCall->destroy(TermCode::kErrAlready, false, "We have a call collision and we remove the call and answer the other one");
    }
    else if (chat.isGroup() && itCallHandler != mCallHandlers.end() && itCallHandler->second->isParticipating(myHandle))
    {
        // Other client of our user is participanting in the call
        RTCM_LOG_DEBUG("handleCallDataRequest: Ignoring call request: We are already in the group call from another client");
        return ;
    }

    auto ret = mCalls.emplace(chatid, std::make_shared<Call>(*this, chat, callid, chat.isGroup(),
                                                             true, nullptr, userid, clientid, answerAutomatic));
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
            static_cast<Call*>(wcall.weakPtr())->destroy(TermCode::kAnswerTimeout, false, "Destroy call due it has not been answer after timeout");
        }, kCallAnswerTimeout+4000, mKarereClient.appCtx); // local timeout a bit longer that the caller
    }
    else
    {
        call->answer(avFlags);
    }
}

Call::Call(RtcModule& rtcModule, chatd::Chat& chat, karere::Id callid, bool isGroup,
           bool isJoiner, ICallHandler* handler, Id callerUser, uint32_t callerClient, bool callRecovered)
    : ICall(rtcModule, chat, callid, isGroup, isJoiner, handler, callerUser, callerClient)
    , mName("call["+mId.toString()+"]")
    , mRecovered(callRecovered) // the joiner is actually the answerer in case of new call
{
    if (isJoiner && mCallerUser && mCallerClient)
    {
        mState = kStateRingIn;
    }
    else
    {
        mState = kStateInitial;
        assert(isJoiner || (!callerUser && !callerClient));
    }

    mSessionsInfo.clear();

    auto wptr = weakHandle();
    mStatsTimer = setInterval([this, wptr]()
    {
        if (wptr.deleted())
            return;

        for (auto it = mSessions.begin(); it != mSessions.end(); it++)
        {
            if (it->second->getState() == Session::kStateInProgress)
            {
                it->second->pollStats();
            }
        }
    }, kStatsPeriod * 1000, mManager.mKarereClient.appCtx);
}

void Call::handleMessage(RtMessage& packet)
{
    switch (packet.type)
    {
        case RTCMD_CALL_TERMINATE:
            // This message can be received from old clients. It can be ignored
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
        case RTCMD_SDP_OFFER:
            msgSdpOffer(packet);
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

            if (reason == TermCode::kErrPeerOffline)
            {
                // kErrPeerOffline is generated only internally, prevent a malicious peer
                // from messing up our logic
                reason = TermCode::kAppTerminating;
            }

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

void Call::getLocalStream(AvFlags av)
{
    mLocalStream = std::make_shared<artc::LocalStreamHandle>();

    IVideoRenderer* renderer = NULL;
    FIRE_EVENT(CALL, onLocalStreamObtained, renderer);
    mLocalPlayer.reset(new artc::StreamPlayer(renderer, mManager.mKarereClient.appCtx));
    if (av.video())
    {
        enableVideo(true);
    }

    mLocalPlayer->enableVideo(av.video() || !av.onHold());

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack =
            artc::gWebrtcContext->CreateAudioTrack("a"+std::to_string(artc::generateId()), artc::gWebrtcContext->CreateAudioSource(cricket::AudioOptions()));

    if (!av.audio() || av.onHold())
    {
        audioTrack->set_enabled(false);
    }

    mLocalStream->addAudioTrack(audioTrack);

    setState(Call::kStateHasLocalStream);

    if (av.onHold())
    {
        FIRE_EVENT(CALL, onOnHold, av.onHold());
    }
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
    else if (code == TermCode::kErrNotSupported)
    {
        if (packet.userid != mChat.client().myHandle() && !mChat.isGroup())
        {
            mNotSupportedAnswer = true;
        }
        else
        {
            SUB_LOG_INFO("Ingoring CALL_REQ_DECLINE (kErrNotSupported) group Chat or same user");
        }
    }
    else
    {
        SUB_LOG_INFO("Ingoring CALL_REQ_DECLINE with unexpected termnation code %s", termCodeToStr(code));
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
        SUB_LOG_INFO("Ignoring CALL_REQ_CANCEL for an unknown request id");
        return;
    }

    assert(mCallerUser);
    assert(mCallerClient);
    // CALL_REQ_CANCEL callid.8 reason.1
    if (mCallerUser != packet.userid || mCallerClient != packet.clientid)
    {
        SUB_LOG_INFO("Ignoring CALL_REQ_CANCEL from a client that did not send the call request");
        return;
    }

    auto term = packet.payload.read<uint8_t>(8);
    if (term == TermCode::kUserHangup)
    {
        destroy(static_cast<TermCode>(kCallReqCancel | TermCode::kPeer), false, "Call request has been cancelled by the caller");
    }
    else
    {
        destroy(static_cast<TermCode>(term | TermCode::kPeer), false, "Call request has been cancelled by other reason");
    }
}

void Call::msgSdpOffer(RtMessage& packet)
{
    // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
    if (mState != kStateJoining && mState != kStateInProgress)
    {
        SUB_LOG_WARNING("Ignoring unexpected SDP offer while in state %s", stateStr());
        return;
    }

    EndpointId endPoint(packet.userid, packet.clientid);
    auto sessionsInfoIt = mSessionsInfo.find(endPoint);
    if (sessionsInfoIt == mSessionsInfo.end())
    {
        SUB_LOG_ERROR("Received SDP_OFFER without having previously sent SESSION, ignoring");
        return;
    }

    std::shared_ptr<Session> sess(new Session(*this, packet, &sessionsInfoIt->second));
    mSessions[sessionsInfoIt->second.mSessionId] = sess;
    notifyCallStarting(*sess);
    auto wptr = weakHandle();
    sess->getPeerKeey().then([wptr, sess]()
    {
        if (wptr.deleted())
        {
            return;
        }

        sess->createRtcConn();
        sess->processSdpOfferSendAnswer();
        sess->processPackets();
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
        {
            return;
        }

        SUB_LOG_WARNING("Session destroyed: %s", err.msg().c_str());
    });

    mSessionsInfo.erase(endPoint);

}

void Call::handleReject(RtMessage& packet)
{
    if (packet.callid != mId)
    {
        SUB_LOG_WARNING("Ingoring unexpected call id");
        return;
    }

    if (packet.userid != mChat.client().myHandle())
    {
        if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
        {
            SUB_LOG_WARNING("Ingoring unexpected CALL_REQ_DECLINE while in state %s", stateToStr(mState));
            return;
        }
        if (mIsGroup)
        {
            // in groupcalls, a peer declining a call should not finish the call
            SUB_LOG_INFO("Ignoring CALL_REQ_DECLINE. A peer of the group call has declined the request");
            return;
        }
        if (!mSessions.empty())
        {
            // in both 1on1 calls and groupcalls, receiving a request-decline should not destroy the call
            // if there's already a session (in 1on1, it may happen when the answerer declined from a 3rd client, but
            // already answered)
            SUB_LOG_INFO("Ignoring CALL_REQ_DECLINE. There are active sessions already, so the call is in progress.");
            return;
        }
        destroy(static_cast<TermCode>(TermCode::kCallRejected | TermCode::kPeer), false, "Call request has been rejected");
    }
    else // Call has been rejected by other client from same user
    {
        assert(packet.clientid != mChat.connection().clientId());

        if (mState != Call::kStateRingIn)
        {
            SUB_LOG_WARNING("Ingoring unexpected CALL_REQ_DECLINE while in state %s", stateToStr(mState));
            return;
        }

        destroy(static_cast<TermCode>(TermCode::kRejElsewhere), false, "Call request has been rejected by other client from same user");
    }
}

void Call::msgRinging(RtMessage& packet)
{
    if (mState != Call::kStateReqSent && mState != Call::kStateInProgress)
    {
        SUB_LOG_INFO("Ignoring unexpected RINGING");
        return;
    }

    if (mHadRingAck)
    {
        return;
    }

    mHadRingAck = true;
    FIRE_EVENT(CALL, onRingOut, packet.userid);
}

void Call::clearCallOutTimer()
{
    if (!mCallOutTimer) {
        return;
    }
    cancelTimeout(mCallOutTimer, mManager.mKarereClient.appCtx);
    mCallOutTimer = 0;
    mIsRingingOut = false;
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
        destroy(static_cast<TermCode>(TermCode::kBusy | TermCode::kPeer), false, "The peer has rejected the call because it's busy");
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

    if (mState == Call::kStateJoining)
    {
        if (!mInCallPingTimer)
        {
            startIncallPingTimer();
        }

        setState(Call::kStateInProgress);
        monitorCallSetupTimeout();
    }

    EndpointId peerEndPointId(packet.userid, packet.clientid);
    if (mSessionsInfo.find(peerEndPointId) != mSessionsInfo.end())
    {
        SUB_LOG_INFO("Detected simultaneous join with Peer %s (0x%x)", peerEndPointId.userid.toString().c_str(), peerEndPointId.clientid);
        EndpointId ourEndPointId(mManager.mKarereClient.myHandle(), mChat.connection().clientId());
        if (EndpointId::greaterThanForJs(ourEndPointId, peerEndPointId))
        {
            SUB_LOG_INFO("Detected simultaneous join - received RTCMD_SESSION after having already sent one. "
                            "Our endpoint is greater (userid %s, clientid 0x%x), ignoring received SESSION"
                            , ourEndPointId.userid.toString().c_str(), ourEndPointId.clientid );

            return;
        }
    }

    std::shared_ptr<Session> sess(new Session(*this, packet));
    mSessions[sess->sessionId()] = sess;
    cancelSessionRetryTimer(sess->mPeer, sess->mPeerClient);
    notifyCallStarting(*sess);
    SdpKey encKey;
    packet.payload.read(24, encKey);
    auto wptr = weakHandle();
    sess->getPeerKeey().then([wptr, this, sess, encKey]()
    {
        if (wptr.deleted())
        {
            return;
        }

        mManager.crypto().decryptKeyFrom(sess->mPeer, encKey, sess->mPeerHashKey);
        sess->createRtcConnSendOffer();
        sess->processPackets();
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
        {
            return;
        }

        SUB_LOG_WARNING("Session destroyed: %s", err.msg().c_str());
    });
}

void Call::notifyCallStarting(Session &/*sess*/)
{
    if (!mCallStartingSignalled)
    {
        mCallStartingSignalled = true;
        FIRE_EVENT(CALL, onCallStarting);
    }
}

void Call::msgJoin(RtMessage& packet)
{
    if (mState == kStateRingIn && packet.userid == mManager.mKarereClient.myHandle())
    {
        // Another client of our own user has already answered the call
        // --> add the participant to the call, so when the upcoming CALLDATA from caller
        // (indicating that a session has been established with our user, but other users should
        // keep ringing), this client does not start ringing again
        mHandler->addParticipant(packet.userid, packet.clientid, karere::AvFlags());
        destroy(TermCode::kAnsElsewhere, false, "In 1on1, the call has been answer in other peer");
    }
    else if (!packet.chat.isGroup() && hasSessionWithUser(packet.userid))
    {
        mManager.cmdEndpoint(RTCMD_CALL_REQ_CANCEL, packet, mId, TermCode::kAnsElsewhere);
        SUB_LOG_WARNING("Ignore a JOIN from our in 1to1 chatroom, we have a session or have sent a session request");
        return;
    }
    else if (packet.userid == mManager.mKarereClient.myHandle() && !packet.chat.isGroup())
    {
        SUB_LOG_INFO("Ignore a JOIN from our own user in 1to1 chatroom");
        return;
    }
    else if (mState == Call::kStateJoining || mState == Call::kStateInProgress || mState == Call::kStateReqSent)
    {
        packet.callid = packet.payload.read<uint64_t>(0);
        assert(packet.callid);
        for (auto itSession = mSessions.begin(); itSession != mSessions.end(); itSession++)
        {
            if (itSession->second->peer() == packet.userid && itSession->second->peerClient() == packet.clientid)
            {
                if (itSession->second->getState() < Session::kStateTerminating)
                {
                    SUB_LOG_WARNING("Ignoring JOIN from User: %s (client: 0x%x) to whom we already have a session",
                                    itSession->second->peer().toString().c_str(), itSession->second->peerClient());
                    return;
                }

                if (!itSession->second->mTerminatePromise.done())
                {
                    SUB_LOG_INFO("Force to finish session with User: %s (client: 0x%x)",
                                    itSession->second->peer().toString().c_str(), itSession->second->peerClient());

                    assert(itSession->second->getState() == Session::kStateTerminating);
                    auto pms = itSession->second->mTerminatePromise;
                    pms.resolve();
                }
            }
        }

        if (mState == Call::kStateReqSent || mState == Call::kStateJoining)
        {
            if (!mInCallPingTimer)
            {
                startIncallPingTimer();
            }

            setState(Call::kStateInProgress);
            monitorCallSetupTimeout();

            // Send OP_CALLDATA with call inProgress
            if (mState == Call::kStateReqSent && !chat().isGroup())
            {
                mIsRingingOut = false;
                if (!sendCallData(CallDataState::kCallDataNotRinging))
                {
                    SUB_LOG_INFO("Ignoring JOIN from User: %s (client: 0x%x) because cannot send CALLDATA (offline)", packet.userid.toString().c_str(), packet.clientid);
                    return;
                }
            }
        }

        auto it = mManager.mRetryCallTimers.find(chat().chatId());
        if (it != mManager.mRetryCallTimers.end())
        {
            cancelTimeout(it->second, mManager.mKarereClient.appCtx);
            mManager.mRetryCallTimers.erase(it);
            mManager.mRetryCall.erase(chat().chatId());
            mHandler->onReconnectingState(false);
        }

        EndpointId endPointId(packet.userid, packet.clientid);
        mSessionsInfo.erase(endPointId);
        auto wptr = weakHandle();
        loadCryptoPeerKey(packet.userid).then([wptr, this, packet, endPointId](Buffer *)
        {
            if (wptr.deleted())
            {
                return;
            }

            Id newSid = mManager.random<uint64_t>();
            SdpKey encKey;
            SdpKey ownHashKey;
            mManager.random(ownHashKey);
            mManager.crypto().encryptKeyTo(packet.userid, ownHashKey, encKey);
            uint8_t flags = kSupportsStreamReneg;   // no need to send the A/V flags again, already sent in CALLDATA
            // SESSION callid.8 sid.8 anonId.8 encHashKey.32 mId.8 flags.1
            mManager.cmdEndpoint(RTCMD_SESSION, packet,
                packet.callid,
                newSid,
                mManager.mOwnAnonId,
                encKey,
                mId,
                flags);

            // read received flags in JOIN:
            bool supportRenegotiation = ((packet.payload.buf()[kOffsetFlagsJoin] & kSupportsStreamReneg) != 0);

            // A/V flags are also included, but not used, since flags in CALLDATA prevails here and later on
            // the SDP_OFFER & SDP_ANSWER will include update value of A/V flags anyway
            // bool audio = ((packet.payload.buf()[kRenegotationPositionJoin] & AvFlags::kAudio) != 0);
            // bool video = ((packet.payload.buf()[kRenegotationPositionJoin] & AvFlags::kVideo) != 0);
            mSessionsInfo[endPointId] = Session::SessionInfo(newSid, ownHashKey, supportRenegotiation);
        });

        cancelSessionRetryTimer(endPointId.userid, endPointId.clientid);
    }
    else
    {
        SUB_LOG_WARNING("Ignoring unexpected JOIN");
        return;
    }
}

::promise::Promise<void> Call::gracefullyTerminateAllSessions(TermCode code)
{
    SUB_LOG_DEBUG("gracefully term all sessions");
    std::vector<::promise::Promise<void>> promises;
    for (auto it = mSessions.begin(); it != mSessions.end();)
    {
        std::shared_ptr<Session> session = it++->second;
        promises.push_back(session->terminateAndDestroy(code));
    }
    return ::promise::when(promises)
    .fail([](const ::promise::Error& err)
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
        return ::promise::_Void();
    }

    for (auto& item: mSessions)
    {
        if (item.second->mState < Session::kStateTerminating)
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
        if (++ctx->count > 7 || mChat.connection().state() != Connection::State::kStateConnected)
        {
            cancelInterval(mDestroySessionTimer, mManager.mKarereClient.appCtx);
            mDestroySessionTimer = 0;
            SUB_LOG_INFO("Timed out waiting for all sessions to terminate, force closing them");
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
        cancelInterval(mDestroySessionTimer, mManager.mKarereClient.appCtx);
        mDestroySessionTimer = 0;
        ctx->pms.resolve();
    }, 200, mManager.mKarereClient.appCtx);
    return ctx->pms;
}

promise::Promise<void> Call::terminateAllSessionInmediately(TermCode code)
{
    for (auto it = mSessions.begin(); it != mSessions.end();)
    {
        std::shared_ptr<Session> session = it++->second;
        session->terminateAndDestroy(code);
        session->forceDestroy();
    }

    return promise::_Void();
}

Promise<void> Call::destroy(TermCode code, bool weTerminate, const string& msg)
{
    if (mState == Call::kStateDestroyed)
    {
        return ::promise::_Void();
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

    mTermCode = (mState == kStateReqSent && code == TermCode::kUserHangup) ? kCallReqCancel : code;  // adjust for onStateChange()
    mPredestroyState = mState;
    setState(Call::kStateTerminating);
    clearCallOutTimer();
    if (mVideoDevice)
    {
        mVideoDevice->releaseDevice();
    }

    mLocalPlayer.reset();
    mLocalStream.reset();

    Promise<void> pms((::promise::Empty())); //non-initialized promise
    if (weTerminate)
    {
        switch (mPredestroyState)
        {
        case kStateReqSent:
            cmdBroadcast(RTCMD_CALL_REQ_CANCEL, mId, (code == TermCode::kDestroyByCallCollision) ? TermCode::kUserHangup : code);
            code = kCallReqCancel;  // overwrite code for onDestroy() callback
            pms = ::promise::_Void();
            break;
        case kStateRingIn:
            cmdBroadcast(RTCMD_CALL_REQ_DECLINE, mId, code);
            pms = ::promise::_Void();
            break;
        default:
            if (code == TermCode::kAppTerminating)
            {
                pms = terminateAllSessionInmediately(code);
            }
            else
            {
                // if we initiate the call termination, we must initiate the
                // session termination handshake
                pms = gracefullyTerminateAllSessions(code);
            }
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

        TermCode codeWithOutPeer = static_cast<TermCode>(code & ~TermCode::kPeer);
        if (codeWithOutPeer == TermCode::kAnsElsewhere || codeWithOutPeer == TermCode::kErrAlready || codeWithOutPeer == TermCode::kAnswerTimeout)
        {
            SUB_LOG_DEBUG("Not posting termination CALLDATA because term code is kAnsElsewhere, kErrAlready or kAnswerTimeout");
        }
        else if (mPredestroyState == kStateRingIn)
        {
            SUB_LOG_DEBUG("Not sending CALLDATA because we were passively ringing");
        }
        else if (mInCallPingTimer)
        {
            sendCallData(CallDataState::kCallDataEnd);
        }

        assert(mSessions.empty());
        stopIncallPingTimer(mInCallPingTimer);
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
        destroy(TermCode::kErrNetSignalling, false, "Failure to send BroadCast command");
    }, mManager.mKarereClient.appCtx);
    return false;
}

bool Call::broadcastCallReq()
{
    if (mState >= Call::kStateTerminating)
    {
        SUB_LOG_INFO("broadcastCallReq: Call terminating/destroyed");
        return false;
    }
    assert(mState == Call::kStateHasLocalStream);

    mIsRingingOut = true;
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

        mIsRingingOut = false;

        if (mState == Call::kStateReqSent)
        {
            if (mNotSupportedAnswer && !mChat.isGroup())
            {
                destroy(static_cast<TermCode>(TermCode::kErrNotSupported | TermCode::kPeer), true, "No peer has webrtc capabilities to answer the call");
            }
            else
            {
                hangup(TermCode::kAnswerTimeout);
            }
        }
        else if (chat().isGroup() && mState == Call::kStateInProgress)
        {
            // In group calls we don't stop ringing even when call is answered, but stop it
            // after some time (we use the same kAnswerTimeout duration in order to share the timer)
            sendCallData(CallDataState::kCallDataNotRinging);
        }
    }, RtcModule::kCallAnswerTimeout, mManager.mKarereClient.appCtx);
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
    }, RtcModule::kIncallPingInterval, mManager.mKarereClient.appCtx);
}

void Call::asyncDestroy(TermCode code, bool weTerminate)
{
    auto wptr = weakHandle();
    marshallCall([wptr, this, code, weTerminate]()
    {
        if (wptr.deleted())
            return;
        destroy(code, weTerminate, "Failure to send a command");
    }, mManager.mKarereClient.appCtx);
}

void Call::stopIncallPingTimer(bool endCall)
{
    if (mInCallPingTimer)
    {
        cancelInterval(mInCallPingTimer, mManager.mKarereClient.appCtx);
        mInCallPingTimer = 0;
    }

    if (endCall)
    {
        mChat.sendCommand(Command(OP_ENDCALL) + mChat.chatId() + uint64_t(0) + uint32_t(0));
    }
}

void Call::removeSession(Session& sess, TermCode reason)
{
    karere::Id sessionId = sess.mSid;
    karere::Id sessionPeer = sess.mPeer;
    uint32_t sessionPeerClient = sess.mPeerClient;
    bool caller = sess.isCaller();
    mSessions.erase(sessionId);

    if (mState == kStateTerminating)
    {
        return;
    }

    EndpointId endpointId(sessionPeer, sessionPeerClient);
    if (!Session::isTermRetriable(reason))
    {
        mSessionsReconnectionInfo.erase(endpointId);
        destroyIfNoSessionsOrRetries(reason);
        return;
    }

    auto sessionReconnectionIt = mSessionsReconnectionInfo.find(endpointId);
    if (sessionReconnectionIt == mSessionsReconnectionInfo.end())
    {
        SessionReconnectInfo reconnectInfo;
        mSessionsReconnectionInfo[endpointId] = reconnectInfo;
        sessionReconnectionIt = mSessionsReconnectionInfo.find(endpointId);
    }

    SessionReconnectInfo& info = sessionReconnectionIt->second;
    info.setReconnections(info.getReconnections() + 1);
    info.setOldSid(sessionId);

    // If we want to terminate the call (no matter if initiated by us or peer), we first
    // set the call's state to kTerminating. If that is not set, then it's only the session
    // that terminates for a reason that is not fatal to the call,
    // and can try re-establishing the session
    if (cancelSessionRetryTimer(sessionPeer, sessionPeerClient))
    {
        SUB_LOG_DEBUG("removeSession: Trying to remove a session for which there is a scheduled retry, the retry should not be there");
        assert(false);
    }

    TermCode terminationCode = (TermCode)(reason & ~TermCode::kPeer);
    if (terminationCode == TermCode::kErrIceFail || terminationCode == TermCode::kErrIceTimeout || terminationCode == kErrSessSetupTimeout)
    {
        if (mIceFails.find(endpointId) == mIceFails.end())
        {
            mIceFails[endpointId] = 1;
        }
        else
        {
            mIceFails[endpointId]++;
        }
    }

    if (!caller)
    {
        SUB_LOG_DEBUG("Session to %s failed, re-establishing it...", sessionId.toString().c_str());
        auto wptr = weakHandle();
        marshallCall([wptr, this, sessionPeer, sessionPeerClient]()
        {
            if (wptr.deleted())
                return;

            rejoin(sessionPeer, sessionPeerClient);

        }, mManager.mKarereClient.appCtx);
    }
    else
    {
        // Else wait for peer to re-join...
        SUB_LOG_DEBUG("Session to %s failed, expecting peer to re-establish it...", sessionId.toString().c_str());
    }

    // set a timeout for the session recovery
    auto wptr = weakHandle();
    mSessRetries[endpointId] = setTimeout([this, wptr, endpointId]()
    {
        if (wptr.deleted())
            return;

        mSessRetries.erase(endpointId);
        mSessionsReconnectionInfo.erase(endpointId);
        if (mState >= kStateTerminating) // call already terminating
        {
           return; //timer is not relevant anymore
        }

        if (hasNoSessionsOrPendingRetries() && mManager.mRetryCall.find(mChat.chatId()) == mManager.mRetryCall.end())
        {
            SUB_LOG_DEBUG("Timed out waiting for peer to rejoin, terminating call");
            hangup(kErrSessRetryTimeout);
        }
    }, RtcModule::kSessSetupTimeout, mManager.mKarereClient.appCtx);
}
bool Call::startOrJoin(AvFlags av)
{
    mLocalFlags = av;
    manager().updatePeerAvState(mChat.chatId(), mId, mChat.client().mKarereClient->myHandle(), mChat.connection().clientId(), av);

    if (!mLocalPlayer)
    {
        getLocalStream(av);
    }

    if (mIsJoiner)
    {
        return join();
    }
    else
    {
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
    mSessionsInfo.clear();
    // JOIN:
    // chatid.8 userid.8 clientid.4 dataLen.2 type.1 callid.8 anonId.8
    // if userid is not specified, join all clients in the chat, otherwise
    // join a specific user (used when a session gets broken)
    setState(Call::kStateJoining);
    uint8_t flags = sentFlags() | kSupportsStreamReneg;
    bool sent = userid
            ? cmd(RTCMD_JOIN, userid, 0, mId, mManager.mOwnAnonId, flags)
            : cmdBroadcast(RTCMD_JOIN, mId, mManager.mOwnAnonId, flags);

    if (!sent)
    {
        asyncDestroy(TermCode::kErrNetSignalling, true);
        return false;
    }

    if (!mRecovered)
    {
        startIncallPingTimer();
    }

    // we have session setup timeout timer, but in case we don't even reach a session creation,
    // we need another timer as well
    auto wptr = weakHandle();
    setTimeout([wptr, this]()
    {
        if (wptr.deleted())
            return;
        if (mState <= Call::kStateJoining)
        {
            destroy(TermCode::kErrSessSetupTimeout, true, "In state joining, call hasn't progressed to correct state");
        }
    }, RtcModule::kSessSetupTimeout, mManager.mKarereClient.appCtx);
    return true;
}

bool Call::rejoin(karere::Id userid, uint32_t clientid)
{
    assert(mState == Call::kStateInProgress);
    EndpointId endPoint(userid, clientid);
    mSessionsInfo.erase(endPoint);
    // JOIN:
    // chatid.8 userid.8 clientid.4 dataLen.2 type.1 callid.8 anonId.8
    // if userid is not specified, join all clients in the chat, otherwise
    // join a specific user (used when a session gets broken)
    uint8_t flags = sentFlags() | kSupportsStreamReneg;
    bool sent = cmd(RTCMD_JOIN, userid, clientid, mId, mManager.mOwnAnonId, flags);
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
    if (state == CallDataState::kCallDataInvalid)
    {
        state = CallDataState::kCallDataMute;
    }


    uint16_t payLoadLen = sizeof(mId) + sizeof(uint8_t) + sizeof(uint8_t);
    if (state == CallDataState::kCallDataEnd)
    {
         payLoadLen += sizeof(uint8_t);
    }

    mLastCallData = state;

    Command command = Command(chatd::OP_CALLDATA) + mChat.chatId();
    command.write<uint64_t>(9, 0);
    command.write<uint32_t>(17, 0);
    command.write<uint16_t>(21, payLoadLen);
    command.write<uint64_t>(23, mId);
    command.write<uint8_t>(31, state);
    uint8_t flags = sentFlags().value();
    if (mIsRingingOut)
    {
        flags = flags | kFlagRinging;
    }

    command.write<uint8_t>(32, flags);
    if (state == CallDataState::kCallDataEnd)
    {
        command.write<uint8_t>(33, convertTermCodeToCallDataCode());
    }

    SUB_LOG_DEBUG("CALLDATA Send: State: %d", state);
    if (!mChat.sendCommand(std::move(command)))
    {
        auto wptr = weakHandle();
        marshallCall([wptr, this]()
        {
            if (wptr.deleted())
                return;
            destroy(TermCode::kErrNetSignalling, true, "Failure to send Calldata");
        }, mManager.mKarereClient.appCtx);

        return false;
    }

    return true;
}

void Call::destroyIfNoSessionsOrRetries(TermCode reason)
{
    if (!hasNoSessionsOrPendingRetries())
    {
        return;
    }

    karere::Id chatid = mChat.chatId();
    if (mManager.mRetryCall.find(chatid) == mManager.mRetryCall.end())
    {
        SUB_LOG_DEBUG("Everybody left, terminating call");
        destroy(reason, false, "Everybody left");
        return;
    }

    RTCM_LOG_DEBUG("Launch reconnection call at pasive mode");

    auto itRetryTimerHandle = mManager.mRetryCallTimers.find(chatid);
    if (itRetryTimerHandle != mManager.mRetryCallTimers.end())
    {
        // There is a retry and it isn't neccesary launch another one
        return;
    }

    auto wptr = weakHandle();
    RtcModule* manager = &mManager;
    auto wptrManager = manager->weakHandle();
    mManager.mRetryCallTimers[chatid] = setTimeout([this, wptr, wptrManager, manager, chatid, reason]()
    {
        if (wptrManager.deleted())
        {
            return;
        }

        manager->mRetryCall.erase(chatid);
        manager->mRetryCallTimers.erase(chatid);

        if (wptr.deleted())
            return;

        SUB_LOG_DEBUG("Everybody left, terminating call- After reconnection");
        mHandler->setReconnectionFailed();
        destroy(reason, false, "Everybody left - After reconnection");

    }, RtcModule::kRetryCallTimeout, mManager.mKarereClient.appCtx);
}

bool Call::hasNoSessionsOrPendingRetries() const
{
    return mSessions.empty() && mSessRetries.empty();
}

uint8_t Call::convertTermCodeToCallDataCode()
{
    uint8_t codeToChatd = chatd::CallDataReason::kEnded;
    switch (mTermCode & ~TermCode::kPeer)
    {
        case kUserHangup:
        {
            switch (mPredestroyState)
            {
                case kStateRingIn:
                    assert(false);  // it should be kCallRejected
                case kStateReqSent:
                    assert(false);  // it should be kCallReqCancel
                    codeToChatd = chatd::CallDataReason::kCancelled;
                    break;

                case kStateInProgress:
                case kStateJoining:
                    codeToChatd = chatd::CallDataReason::kEnded;
                    break;

                default:
                    assert(false);  // kStateTerminating, kStateDestroyed, kStateHasLocalStream shouldn't arrive here
                    codeToChatd = chatd::CallDataReason::kFailed;
                    break;
            }
            break;
        }

        case kCallerGone:
            assert(false);
        case kCallReqCancel:
            assert(mPredestroyState == kStateReqSent || mPredestroyState == kStateJoining);
            codeToChatd = chatd::CallDataReason::kCancelled;
            break;

        case kCallRejected:
            codeToChatd = chatd::CallDataReason::kRejected;
            break;

        case kAnsElsewhere:
            SUB_LOG_ERROR("Can't generate a history call ended message for local kAnsElsewhere code");
            assert(false);
            break;
        case kRejElsewhere:
            SUB_LOG_ERROR("Can't generate a history call ended message for local kRejElsewhere code");
            codeToChatd = chatd::CallDataReason::kRejected;
            assert(false);
            break;

        case kDestroyByCallCollision:
            codeToChatd = chatd::CallDataReason::kRejected;
            break;
        case kAnswerTimeout:
        case kRingOutTimeout:
            codeToChatd = chatd::CallDataReason::kNoAnswer;
            break;

        case kAppTerminating:
            codeToChatd = (mPredestroyState == kStateInProgress) ? chatd::CallDataReason::kEnded : chatd::CallDataReason::kFailed;
            break;

        case kBusy:
            assert(!isJoiner());
            codeToChatd = chatd::CallDataReason::kRejected;
            break;

        default:
            if (!isTermError(mTermCode))
            {
                assert(false);
                SUB_LOG_ERROR("convertTermCodeToCallDataCode: Don't know how to translate term code %s, returning FAILED",
                              termCodeToStr(mTermCode));
            }
            codeToChatd = chatd::CallDataReason::kFailed;
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
        cancelTimeout(timerHandle, mManager.mKarereClient.appCtx);
        mSessRetries.erase(itSessionRetry);
        return true;
    }

    return false;
}

void Call::monitorCallSetupTimeout()
{
    if (mCallSetupTimer)
    {
        cancelInterval(mCallSetupTimer, mManager.mKarereClient.appCtx);
        mCallSetupTimer = 0;
    }

    auto wptr = weakHandle();
    mCallSetupTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted())
            return;
        mCallSetupTimer = 0;

        if (mState != kStateInProgress)
        {
            RTCM_LOG_INFO("Timeout expired to setup a call, but state is %s", stateStr());
            return;
        }

        RTCM_LOG_ERROR("Timeout expired to setup a call");
        hangup(TermCode::kErrCallSetupTimeout);

    }, RtcModule::kCallSetupTimeout, mManager.mKarereClient.appCtx);
}

void Call::enableAudio(bool enable)
{
    if (mState >= Call::kStateTerminating)
    {
        return;
    }

    mLocalStream->audio()->set_enabled(enable);
    for(std::pair<karere::Id, shared_ptr<Session>> session : mSessions)
    {
        if (session.second->mAudioSender)
        {
            session.second->mAudioSender->track()->set_enabled(enable && !session.second->mPeerAv.onHold());
        }
    }
}

void Call::enableVideo(bool enable)
{
    if (mState >= Call::kStateTerminating)
    {
        return;
    }

    if (enable)
    {
        rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
        if (!mVideoDevice)
        {
            webrtc::VideoCaptureCapability capabilities;
            if (mChat.isGroup())
            {
                capabilities.width = 320;
                capabilities.height = 240;
                capabilities.maxFPS = 25;
            }
            else
            {
                capabilities.width = 640;
                capabilities.height = 480;
                capabilities.maxFPS = 30;
            }

            mVideoDevice = artc::VideoManager::Create(capabilities, mManager.mVideoDeviceSelected, artc::gAsyncWaiter->guiThread());
            assert(mVideoDevice);

            videoTrack = artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mVideoDevice->getVideoTrackSource());
            mLocalStream->addVideoTrack(videoTrack);
        }
        else
        {
            videoTrack = mLocalStream->video();
            assert(videoTrack);
        }

        if (mManager.mVideoDeviceSelected.empty())
        {
            SUB_LOG_ERROR("Unable to open device, no device selected");
            return;
        }

        if (mLocalFlags.onHold())
        {
            return;
        }

        mVideoDevice->openDevice(mManager.mVideoDeviceSelected);
        mLocalPlayer->attachVideo(videoTrack);
        mLocalStream->video()->set_enabled(true);
        for(std::pair<karere::Id, shared_ptr<Session>> session : mSessions)
        {
            if (session.second->mPeerSupportRenegotiation)
            {
                if (session.second->mVideoSender)
                {
                    rtc::scoped_refptr<webrtc::VideoTrackInterface> interface =
                            artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()),  videoTrack->GetSource());

                    session.second->mVideoSender->SetTrack(interface);
                }
                else
                {
                    std::vector<std::string> vector;
                    vector.push_back(session.second->mName);
                    rtc::scoped_refptr<webrtc::VideoTrackInterface> interface =
                            artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()),  videoTrack->GetSource());
                    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> error = session.second->mRtcConn->AddTrack(interface, vector);
                    if (!error.ok())
                    {
                        SUB_LOG_WARNING("Error: %s", error.MoveError().message());
                        session.second->destroy(TermCode::kErrInternal);
                        return;
                    }

                    session.second->mVideoSender = error.MoveValue();
                }

                if (session.second->mRemotePlayer)
                {
                    session.second->mRemotePlayer->enableVideo(session.second->mPeerAv.video() && !session.second->mPeerAv.onHold());
                }

                if (session.second->mPeerAv.onHold())
                {
                    session.second->mVideoSender->track()->set_enabled(false);
                }
            }
            else
            {
                session.second->terminateAndDestroy(TermCode::kStreamChange);
            }
        }
    }
    else
    {
        mLocalPlayer->detachVideo();
        if (mLocalStream->video())
        {
            mLocalStream->video()->set_enabled(false);
        }

        for(std::pair<karere::Id, shared_ptr<Session>> session : mSessions)
        {
            if (session.second->mPeerSupportRenegotiation)
            {
                assert(session.second->mVideoSender);
                session.second->mVideoSender->SetTrack(nullptr);
            }
            else
            {
                session.second->terminateAndDestroy(TermCode::kStreamChange);
            }
        }

        if (mVideoDevice)
        {
            mVideoDevice->releaseDevice();
        }
    }
}

bool Call::hasSessionWithUser(Id userId)
{
    for (auto itSession = mSessions.begin(); itSession != mSessions.end(); itSession++)
    {
        if (itSession->second->peer() == userId)
        {
            return true;
        }
    }

    for (auto itSentSession = mSessionsInfo.begin(); itSentSession != mSessionsInfo.end(); itSentSession++)
    {
        if (itSentSession->first.userid == userId)
        {
            return true;
        }
    }

    return false;
}
void Call::sendAVFlags()
{
    sendCallData();
    AvFlags av = mLocalStream ? mLocalStream->effectiveAv() : AvFlags(0);
    av.setOnHold(mLocalFlags.onHold());
    for (auto& item: mSessions)
    {
        item.second->sendAv(av);
    }
}

promise::Promise<Buffer*> Call::loadCryptoPeerKey(Id peerid)
{
    return mManager.mKarereClient.userAttrCache().getAttr(peerid, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY);
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
        SUB_LOG_INFO("answer: It's not possible join to the call, there are too many participants");
        return false;
    }

    return startOrJoin(av);
}

void Call::hangup(TermCode reason)
{
    mManager.removeCallRetry(mChat.chatId(), false);

    switch (mState)
    {
    case kStateReqSent:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kUserHangup;
        }
        else
        {
            assert(reason == TermCode::kUserHangup || reason == TermCode::kAnswerTimeout ||
                   reason == TermCode::kRingOutTimeout || reason == TermCode::kDestroyByCallCollision
                   || reason == TermCode::kAppTerminating);
        }

        destroy(reason, true, "Cancel call request in caller");
        return;

    case kStateRingIn:
        if (reason == TermCode::kInvalid)
        {
            reason = TermCode::kCallRejected;
        }
        else
        {
            assert(reason == TermCode::kAppTerminating
                   || (false && "Hangup reason can only be undefined, kBusy or kAppTerminating when hanging up call in state kRingIn"));
            reason = TermCode::kInvalid;
        }
        assert(mSessions.empty());
        destroy(reason, true, "Reject call request in callee");
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
    destroy(reason, true, "Hangup the cal");
}

Call::~Call()
{
    if (mState != Call::kStateDestroyed)
    {
        stopIncallPingTimer();
        if (mDestroySessionTimer)
        {
            cancelInterval(mDestroySessionTimer, mManager.mKarereClient.appCtx);
            mDestroySessionTimer = 0;
        }

        if (mVideoDevice)
        {
            mVideoDevice->releaseDevice();
        }

        clearCallOutTimer();
        mLocalPlayer.reset();
        mLocalStream.reset();
        setState(Call::kStateDestroyed);
        FIRE_EVENT(CALL, onDestroy, mTermCode == TermCode::kNotFinished ? TermCode::kErrInternal : mTermCode, false, "Callback from Call::dtor");

        SUB_LOG_DEBUG("Forced call to onDestroy from call dtor");
    }

    if (mStatsTimer)
    {
        cancelInterval(mStatsTimer, mManager.mKarereClient.appCtx);
    }

    SUB_LOG_DEBUG("Destroyed");
}
void Call::onClientLeftCall(Id userid, uint32_t clientid)
{
    if (userid == mManager.mKarereClient.myHandle() && clientid == mChat.connection().clientId())
    {
        if (mRecovered && mState == kStateJoining)
        {
            SUB_LOG_DEBUG("Ignoring ENDCALL received for a reconnect call while in kJoining state");
            return;
        }

        SUB_LOG_DEBUG("ENDCALL received for ourselves, finishing the call");
        auto wptr = weakHandle();
        destroy(TermCode::kErrNetSignalling, false, "ENDCALL received for ourselves, finishing the call")
        .then([wptr, this]
        {
            if (wptr.deleted())
            {
                return;
            }

            karere::Id chatid = chat().chatId();
            auto it = mManager.mRetryCall.find(chatid);
            if (it == mManager.mRetryCall.end())
            {
                karere::AvFlags flags = sentFlags();
                mManager.mRetryCall[chatid] = std::pair<karere::AvFlags, bool>(flags, true);
                auto itHandler = mManager.mCallHandlers.find(chatid);
                mManager.joinCall(chatid, flags, *itHandler->second, mId);
                mManager.mRetryCall.erase(chatid);
            }
        });
        return;
    }

    if (mState == kStateRingIn && isCaller(userid, clientid))   // caller went offline
    {
        destroy(TermCode::kCallerGone, false, "Caller has lost its connection, destroy the call");
        return;
    }

    for (auto& item: mSessions)
    {
        auto sess = item.second;
        if (sess->mPeer == userid && sess->mPeerClient == clientid)
        {
            if (mSessions.size() == 1)
            {
                mManager.launchCallRetry(mChat.chatId(), sentFlags(), false);
            }

            sess->destroy(static_cast<TermCode>(TermCode::kErrPeerOffline | TermCode::kPeer));
            return;
        }
    }

    if (mSessRetries.find(EndpointId(userid, clientid)) != mSessRetries.end())
    {
        if (mSessRetries.size() == 1 && mSessions.size() == 0)
        {
            mManager.launchCallRetry(mChat.chatId(), sentFlags(), false);
        }

        cancelSessionRetryTimer(userid, clientid);
        destroyIfNoSessionsOrRetries(TermCode::kErrPeerOffline);
    }

    // We discard the previous JOIN becasue we have rececived an ENDCALL from that peer
    mSessionsInfo.erase(EndpointId(userid, clientid));
}

bool Call::changeLocalRenderer(IVideoRenderer* renderer)
{
    if (!mLocalPlayer)
        return false;
    mLocalPlayer->changeRenderer(renderer);
    return true;
}

void Call::notifySessionConnected(Session& /*sess*/)
{
    if (mCallStartedSignalled)
        return;

    sendCallData(CallDataState::kCallDataSession);

    if (mCallSetupTimer)
    {
        cancelInterval(mCallSetupTimer, mManager.mKarereClient.appCtx);
        mCallSetupTimer = 0;
    }

    mCallStartedSignalled = true;
    FIRE_EVENT(CALL, onCallStarted);
}

AvFlags Call::muteUnmute(AvFlags av)
{
    mLocalFlags = av;
    if (!mLocalStream || av.onHold())
    {
        return mLocalFlags;
    }

    AvFlags oldAv = mLocalStream->effectiveAv();

    if (oldAv.video() != av.video() && !av.onHold())
    {
        enableVideo(av.video());
    }

    if (oldAv.audio() != av.audio()  && !av.onHold())
    {
        enableAudio(av.audio());
    }

    mLocalStream->setAv(av);
    av = mLocalStream->effectiveAv();
    if (av == oldAv)
    {
        return oldAv;
    }

    mLocalPlayer->enableVideo(av.video());
    manager().updatePeerAvState(mChat.chatId(), mId, mChat.client().mKarereClient->myHandle(), mChat.connection().clientId(), av);

    if (!mLocalFlags.onHold())
    {
        sendAVFlags();
    }

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

void Call::setOnHold(bool onHold)
{
    assert(onHold != mLocalFlags.onHold());
    mLocalFlags.setOnHold(onHold);

    if (!mLocalStream)
    {
        return;
    }

    if (mLocalFlags.audio())
    {
        enableAudio(!onHold);
    }

    if (mLocalFlags.video())
    {
        enableVideo(!onHold);
    }

    for(std::pair<karere::Id, shared_ptr<Session>> session : mSessions)
    {
        if (session.second->mRemotePlayer)
        {
            if (session.second->mRemotePlayer->getAudioTrack())
            {
                session.second->mRemotePlayer->getAudioTrack()->set_enabled(!onHold && !session.second->mPeerAv.onHold());
            }

            if (session.second->mRemotePlayer->getVideoTrack())
            {
                session.second->mRemotePlayer->getVideoTrack()->set_enabled(!onHold && !session.second->mPeerAv.onHold());
            }

            if (session.second->mPeerAv.video())
            {
                session.second->mRemotePlayer->enableVideo(!onHold && !session.second->mPeerAv.onHold());
            }
        }
    }

    sendAVFlags();
    FIRE_EVENT(CALL, onOnHold, onHold);
}

void Call::sendBusy(bool isCallToSameUser)
{
    // Broadcast instead of send only to requestor, so that all other our clients know we rejected the call
    cmdBroadcast(RTCMD_CALL_REQ_DECLINE, mId, isCallToSameUser ? TermCode::kErrAlready : TermCode::kBusy);
}

uint32_t Call::clientidFromSession(Id userid)
{
    for (auto& item: mSessions)
    {
        if (item.second->peer() == userid)
        {
            return item.second->peerClient();
        }
    }

    return 0;
}

void Call::updateAvFlags(Id userid, uint32_t clientid, AvFlags flags)
{
    for (auto it = mSessions.begin(); it != mSessions.end(); it++)
    {
        if (it->second->peer() == userid &&
                it->second->peerClient() == clientid &&
                it->second->receivedAv() != flags)
        {
            it->second->updateAvFlags(flags);
        }
    }
}

bool Call::isCaller(Id userid, uint32_t clientid)
{
    return (userid == mCallerUser && clientid == mCallerClient);
}

void Call::changeVideoInDevice()
{
    enableVideo(false);
    mVideoDevice = nullptr;
    if (!mLocalFlags.onHold())
    {
        enableVideo(true);
    }
}

bool Call::isAudioLevelMonitorEnabled() const
{
    return mAudioLevelMonitorEnabled;
}

void Call::enableAudioLevelMonitor(bool enable)
{
    if (mAudioLevelMonitorEnabled == enable)
    {
        return;
    }

    mAudioLevelMonitorEnabled = enable;

    for (auto& sessionIt : mSessions)
    {
        if (sessionIt.second->mRemotePlayer && sessionIt.second->mRemotePlayer->isAudioAttached())
        {
            if (mAudioLevelMonitorEnabled)
            {
                sessionIt.second->mRemotePlayer->getAudioTrack()->AddSink(sessionIt.second->mAudioLevelMonitor.get());
            }
            else
            {
                sessionIt.second->mRemotePlayer->getAudioTrack()->RemoveSink(sessionIt.second->mAudioLevelMonitor.get());
            }
        }
    }
}

AvFlags Call::sentFlags() const
{
    if (mLocalFlags.onHold())
    {
        return mLocalFlags;
    }

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

    C: Creates a new session with random sid, and state SessState.kWaitSdpOffer
        => send SESSION callid.8 sid.8 anonId.8 encHashKey.32 actualCallId.8
        => call state: CallState.kCallInProgress (in case of group calls call state may already be kCallInProgress)

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
Session::Session(Call& call, RtMessage& packet, const SessionInfo *sessionParameters)
:ISession(call, packet.userid, packet.clientid), mManager(call.mManager)
{
    // Packet can be RTCMD_SESSION or RTCMD_SDP_OFFER
    mHandler = call.callHandler()->onNewSession(*this);
    mAudioLevelMonitor.reset(new AudioLevelMonitor(*this, *mHandler));
    assert(!sessionParameters || packet.type == RTCMD_SDP_OFFER);
    if (packet.type == RTCMD_SDP_OFFER) // peer's offer
    {
        // SDP_OFFER sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        mIsJoiner = false;
        mSid = packet.payload.read<uint64_t>(0);
        setState(kStateWaitLocalSdpAnswer);
        mPeerAnonId = packet.payload.read<uint64_t>(8);
        mOwnHashKey = sessionParameters->mOwnHashKey;
        // The peer is likely to send ICE candidates immediately after the offer,
        // but we can't process them until setRemoteDescription is ready, so
        // we have to store them in a queue
        packet.payload.read(48, mPeerHash);
        SdpKey encKey;
        packet.payload.read(16, encKey);
        mCall.mManager.crypto().decryptKeyFrom(mPeer, encKey, mPeerHashKey);
        mPeerAv = packet.payload.read<uint8_t>(80);
        uint16_t sdpLen = packet.payload.read<uint16_t>(81);
        assert((int) packet.payload.dataSize() >= 83 + sdpLen);
        packet.payload.read(83, sdpLen, mPeerSdpOffer);
        mPeerSupportRenegotiation = sessionParameters->mPeerSupportRenegotiation;   // as received in JOIN

        FIRE_EVENT(SESSION, onPeerMute, mPeerAv, AvFlags(0));
        if (mPeerAv.onHold())
        {
            FIRE_EVENT(SESSION, onOnHold, true);
        }
    }
    else if (packet.type == RTCMD_SESSION)
    {
        // SESSION callid.8 sid.8 anonId.8 encHashKey.32
        mIsJoiner = true;
        mSid = packet.payload.read<uint64_t>(8);
        setState(kStateWaitSdpAnswer);
        assert(packet.payload.dataSize() >= 56);
        call.mManager.random(mOwnHashKey);
        mPeerAnonId = packet.payload.read<uint64_t>(16);
        mPeerSupportRenegotiation = ((packet.payload.buf()[Call::kOffsetFlagsSession] & Call::kSupportsStreamReneg) != 0);
    }
    else
    {
        assert(false);
        SUB_LOG_ERROR("Attempted to create a Session object with packet of type: %s", rtcmdTypeToStr(packet.type));
    }

    SUB_LOG_INFO("============== own sdp key: %s\n", StaticBuffer(mOwnHashKey.data, sizeof(mOwnHashKey.data)).toString().c_str());

    mName = "sess[" + mSid.toString() + "]";
    auto wptr = weakHandle();
    mSetupTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted())
            return;

        SUB_LOG_INFO("Timeout expired to setup a session");

        mSetupTimer = 0;

        TermCode terminationCode = TermCode::kErrSessSetupTimeout;
        if ((time(nullptr) - mTsSdpHandshakeCompleted) > RtcModule::kIceTimeout)
        {
            terminationCode = TermCode::kErrIceTimeout;
            SUB_LOG_INFO("ICE connect timed out. Terminating session with kErrIceTimeout");
        }

        terminateAndDestroy(terminationCode);

    }, RtcModule::kSessSetupTimeout, call.mManager.mKarereClient.appCtx);
}

void Session::setState(uint8_t newState)
{
    if (mState == newState)
        return;

    auto oldState = mState;
    sStateDesc.assertStateChange(oldState, newState);
    mState = newState;
    SUB_LOG_DEBUG("State changed: %s -> %s", stateToStr(oldState), stateToStr(mState));

    if (mSetupTimer && mState >= kStateInProgress)
    {
        cancelTimeout(mSetupTimer, mCall.mManager.mKarereClient.appCtx);
        mSetupTimer = 0;
    }

    FIRE_EVENT(SESSION, onSessStateChange, mState);
}

void Session::handleMessage(RtMessage& packet)
{
    if (mFechingPeerKeys)
    {
        SUB_LOG_DEBUG("Waiting for peer key queue packet");
        mPacketQueue.push_back(packet);
        return;
    }

    switch (packet.type)
    {
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
        case RTCMD_SDP_OFFER_RENEGOTIATE:
            msgSdpOfferRenegotiate(packet);
            return;
        case RTCMD_SDP_ANSWER_RENEGOTIATE:
            msgSdpAnswerRenegotiate(packet);
            return;
        case RTCMD_END_ICE_CANDIDATES:
            return;
        default:
            SUB_LOG_WARNING("Don't know how to handle", packet.typeStr());
            return;
    }
}

void Session::createRtcConn()
{
    EndpointId endPoint(mPeer, mPeerClient);
    if (mCall.mIceFails.find(endPoint) != mCall.mIceFails.end())
    {
        RTCM_LOG_ERROR("Using ALL ICE servers, because ICE to this peer has failed %d times", mCall.mIceFails[endPoint]);
        StaticProvider iceServerStatic(mCall.mManager.mStaticIceServers);
        mCall.mManager.setIceServers(iceServerStatic);
    }

    mRtcConn = artc::myPeerConnection<Session>(mCall.mManager.mIceServers, *this);
    RTCM_LOG_INFO("Create RTC connection ICE server: %s", mCall.mManager.mIceServers[0].uri.c_str());
    if (mCall.mLocalStream)
    {
        std::vector<std::string> vector;
        vector.push_back(mName);

        if (mCall.sentFlags().video())
        {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> videoInterface =
                    artc::gWebrtcContext->CreateVideoTrack("v"+std::to_string(artc::generateId()), mCall.mLocalStream->video()->GetSource());
            webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> error = mRtcConn->AddTrack(videoInterface, vector);

            if (!error.ok())
            {
                SUB_LOG_WARNING("Error: %s", error.MoveError().message());
                destroy(TermCode::kErrInternal);
                return;
            }

            mVideoSender = error.MoveValue();
        }

        rtc::scoped_refptr<webrtc::AudioTrackInterface> audioInterface =
                artc::gWebrtcContext->CreateAudioTrack("a"+std::to_string(artc::generateId()), mCall.mLocalStream->audio()->GetSource());
        webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> error = mRtcConn->AddTrack(audioInterface, vector);
        if (!error.ok())
        {
            SUB_LOG_WARNING("Error: %s", error.MoveError().message());
        }

        mAudioSender = error.MoveValue();

        if (!mCall.sentFlags().audio())
        {
            mAudioSender->track()->set_enabled(false);
        }

        if (mCall.mLocalFlags.onHold() || mPeerAv.onHold())
        {
            if (mVideoSender && mVideoSender->track())
            {
                mVideoSender->track()->set_enabled(false);
            }

            if (mAudioSender && mAudioSender->track())
            {
                mAudioSender->track()->set_enabled(false);
            }
        }
    }

    mStatRecorder.reset(new stats::Recorder(*this, kStatsPeriod, kMaxStatsPeriod));
    mStatRecorder->start();
}

promise::Promise<void> Session:: processSdpOfferSendAnswer()
{
    if (!verifySdpFingerprints(mPeerSdpOffer))
    {
        SUB_LOG_WARNING("Fingerprint verification error, immediately terminating session");
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forge attempt");
        return ::promise::_Void();
    }

    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* sdp = webrtc::CreateSessionDescription("offer", mPeerSdpOffer, &error);
    if (!sdp)
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP offer: line="+error.line+"\nError: "+error.description);
        return ::promise::_Void();
    }
    auto wptr = weakHandle();
    return mRtcConn.setRemoteDescription(sdp)
    .fail([](const ::promise::Error& err)
    {
        return ::promise::Error(err.msg(), 1, kErrSetSdp); //we signal 'remote' (i.e. protocol) error with errCode == 1
    })
    .then([this, wptr]() -> Promise<webrtc::SessionDescriptionInterface*>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return ::promise::Error("Session killed");

        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
        //Probably only required for sdpOffer but follow same approach that webClient
        options.offer_to_receive_audio = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
        options.offer_to_receive_video = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
        return mRtcConn.createAnswer(options);
    })
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> Promise<void>
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return ::promise::Error("Session killed");

        sdp->ToString(&mOwnSdpAnswer);
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, this]()
    {
        if (wptr.deleted() || (mState > Session::kStateInProgress))
            return;

        uint8_t opcode;
        if (mState < kStateInProgress)
        {
            mTsSdpHandshakeCompleted = time(nullptr);
            setState(kStateConnecting);
            opcode = RTCMD_SDP_ANSWER;
        }
        else
        {
            opcode = RTCMD_SDP_ANSWER_RENEGOTIATE;
        }

        SdpKey ownFprHash;
        // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
        mCall.mManager.crypto().mac(mOwnSdpAnswer, mPeerHashKey, ownFprHash);
        cmd(opcode,
            ownFprHash,
            mCall.sentFlags().value(),
            static_cast<uint16_t>(mOwnSdpAnswer.size()),
            mOwnSdpAnswer);
    })
    .fail([wptr, this](const ::promise::Error& err)
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

void Session::forceDestroy()
{
    if (!mTerminatePromise.done())
    {
        mTerminatePromise.resolve();
    }
}

//PeerConnection events
void Session::onAddStream(artc::tspMediaStream stream)
{
    mRemoteStream = stream;
    if (mRemotePlayer)
    {
        SUB_LOG_ERROR("onRemoteStreamAdded: Session already has a remote player, ignoring event");
        return;
    }

    IVideoRenderer* renderer = NULL;
    FIRE_EVENT(SESSION, onRemoteStreamAdded, renderer);
    assert(renderer);
    mRemotePlayer.reset(new artc::StreamPlayer(renderer, mManager.mKarereClient.appCtx));
    mRemotePlayer->setOnMediaStart([this]()
    {
        FIRE_EVENT(SESSION, onDataRecv);
    });
    mRemotePlayer->attachToStream(stream);
    mRemotePlayer->enableVideo(mPeerAv.video() && !mPeerAv.onHold());
    if (mRemotePlayer->getVideoTrack())
    {
        mRemotePlayer->getVideoTrack()->set_enabled(!mCall.mLocalFlags.onHold());
    }

    if (mRemotePlayer->isAudioAttached() && mCall.isAudioLevelMonitorEnabled())
    {
        mRemotePlayer->getAudioTrack()->set_enabled(!mCall.mLocalFlags.onHold());
        mRemotePlayer->getAudioTrack()->AddSink(mAudioLevelMonitor.get());
    }
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
        if (mRemotePlayer->isAudioAttached() && mCall.isAudioLevelMonitorEnabled())
        {
            mRemotePlayer->getAudioTrack()->RemoveSink(mAudioLevelMonitor.get());
        }
        mRemotePlayer->detachFromStream();
        mRemotePlayer.reset();
    }
    mRemoteStream.release();
    FIRE_EVENT(SESSION, onRemoteStreamRemoved);
}

void Session::onIceCandidate(std::shared_ptr<artc::IceCandText> cand)
{
    if (mState >= kStateTerminating)
    {
        return;
    }

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
        cancelIceDisconnectionTimer();

        if (mRenegotiationInProgress)
        {
            SUB_LOG_DEBUG("Skip Ice connection closed, renegotiation in progress");
            return;
        }

        terminateAndDestroy(TermCode::kErrIceDisconn);
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
    {
        cancelIceDisconnectionTimer();
        TermCode termCode = (mState == kStateInProgress) ? TermCode::kErrIceDisconn : TermCode::kErrIceFail;
        terminateAndDestroy(termCode);
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
    {
        handleIceDisconnected();
    }
    else if (state == webrtc::PeerConnectionInterface::kIceConnectionConnected)
    {
        if (mState == kStateInProgress)
        {
            handleIceConnectionRecovered();
            return;
        }

        mManager.updatePeerAvState(mCall.chat().chatId(), mCall.id(), mPeer, mPeerClient, mPeerAv);
        setState(kStateInProgress);
        mTsIceConn = time(NULL);
        mAudioPacketLostAverage = 0;
        mCall.notifySessionConnected(*this);

        // Compatibility with old clients notify avFlags
        if (mCall.mLocalFlags.onHold())
        {
            AvFlags av = mCall.mLocalStream ? mCall.mLocalStream->effectiveAv() : AvFlags(0);
            av.setOnHold(mCall.mLocalFlags.onHold());
            sendAv(av);
        }

        EndpointId endpointId(mPeer, mPeerClient);
        mCall.mIceFails.erase(endpointId);
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

void Session::onTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    SUB_LOG_DEBUG("onTrack:");
    if (mState != kStateInProgress)
    {
        return;
    }

    if (!mRemotePlayer)
    {
        IVideoRenderer* renderer = NULL;
        FIRE_EVENT(SESSION, onRemoteStreamAdded, renderer);
        mRemotePlayer.reset(new artc::StreamPlayer(renderer, mManager.mKarereClient.appCtx));
    }

    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO)
    {
        mRemotePlayer->attachVideo(transceiver->receiver()->streams()[0]->GetVideoTracks()[0]);
        mRemotePlayer->enableVideo(mPeerAv.video());
    }
    else if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO && mCall.isAudioLevelMonitorEnabled())
    {
        mRemotePlayer->getAudioTrack()->AddSink(mAudioLevelMonitor.get());
    }
}

void Session::onRenegotiationNeeded()
{
    if (mState != kStateInProgress)
    {
        return;
    }

    if (mRenegotiationInProgress)
    {
        SUB_LOG_INFO("Ignoring multiple calls of onRenegotiationNeeded");
        return;
    }

    setStreamRenegotiationTimeout();
    SUB_LOG_DEBUG("Renegotiation while in progress, sending sdp offer");
    sendOffer();
}

void Session::updateAvFlags(AvFlags flags)
{
    auto oldAv = mPeerAv;
    mPeerAv = flags;
    if (mRemotePlayer && !mCall.mLocalFlags.onHold())
    {
        mRemotePlayer->enableVideo(mPeerAv.video());
    }

    if (oldAv.onHold() != mPeerAv.onHold())
    {
        setOnHold(mPeerAv.onHold());
    }

    FIRE_EVENT(SESSION, onPeerMute, mPeerAv, oldAv);
}

//end of event handlers

// stats interface
//====
void Session::sendAv(AvFlags av)
{
    cmd(RTCMD_MUTE, av.value());
}

promise::Promise<void> Session::createRtcConnSendOffer()
{
    assert(mIsJoiner); // the joiner sends the SDP offer
    assert(mPeerAnonId);
    createRtcConn();
    return sendOffer();
}

Promise<void> Session::sendOffer()
{
    auto wptr = weakHandle();
    bool isRenegotiation = mState == kStateInProgress;
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    options.offer_to_receive_video = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;
    return mRtcConn.createOffer(options)
    .then([wptr, this](webrtc::SessionDescriptionInterface* sdp) -> Promise<void>
    {
        if (wptr.deleted())
            return ::promise::_Void();

        KR_THROW_IF_FALSE(sdp->ToString(&mOwnSdpOffer));
        return mRtcConn.setLocalDescription(sdp);
    })
    .then([wptr, isRenegotiation, this]()
    {
        if (wptr.deleted())
            return;

        if (mCall.state() != Call::kStateInProgress)
        {
             terminateAndDestroy(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + "Unexpected state");
             return;
        }

        SdpKey hash;
        mCall.mManager.crypto().mac(mOwnSdpOffer, mPeerHashKey, hash);

        // SDP_OFFER/RTCMD_SDP_OFFER_RENEGOTIATE sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
        if (isRenegotiation)
        {
            SdpKey encKey;
            memset(encKey.data, 0, sizeof(encKey.data));
            cmd(RTCMD_SDP_OFFER_RENEGOTIATE,
                mCall.mManager.mOwnAnonId,
                encKey,
                hash,
                mCall.mLocalStream->effectiveAv().value(),
                static_cast<uint16_t>(mOwnSdpOffer.size()),
                mOwnSdpOffer
            );
        }
        else
        {
            mManager.mKarereClient.userAttrCache().getAttr(mPeer, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY)
            .then([wptr, this, hash](Buffer*)
            {
                SdpKey encKey;
                mCall.mManager.crypto().encryptKeyTo(mPeer, mOwnHashKey, encKey);
                cmd(RTCMD_SDP_OFFER,
                    mCall.mManager.mOwnAnonId,
                    encKey,
                    hash,
                    mCall.mLocalStream->effectiveAv().value(),
                    static_cast<uint16_t>(mOwnSdpOffer.size()),
                    mOwnSdpOffer
                );
            });

        }
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
            return;
        terminateAndDestroy(TermCode::kErrSdp, std::string("Error creating SDP offer: ") + err.msg());
    });
}

void Session::msgSdpAnswer(RtMessage& packet)
{
    if (mState != Session::kStateWaitSdpAnswer)
    {
        SUB_LOG_INFO("Ingoring unexpected SDP_ANSWER");
        return;
    }

    setState(kStateConnecting);
    auto wptr = weakHandle();
    setRemoteAnswerSdp(packet)
    .then([wptr, this]
    {
        if (wptr.deleted())
            return;

        mTsSdpHandshakeCompleted = time(nullptr);
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
            mCall.destroy(TermCode::kErrNetSignalling, false, "Failure to send command");
        }
        return false;
    }
    return true;
}

Promise<void> Session::terminateAndDestroy(TermCode code, const std::string& msg)
{
    if (mState == Session::kStateTerminating)
    {
        assert(mTermCode != TermCode::kInvalid);
        if (!isTermRetriable(code) && isTermRetriable(mTermCode))
        {
            mTermCode = code;
        }

        return mTerminatePromise;
    }

    if (mState == kStateDestroyed)
        return ::promise::_Void();

    mTermCode = code;

    if (!msg.empty())
    {
        RTCM_LOG_ERROR("Terminating due to: %s", msg.c_str());
    }
    assert(!mTerminatePromise.done());
    setState(kStateTerminating);

    if (!cmd(RTCMD_SESS_TERMINATE, mTermCode))
    {
        if (!mTerminatePromise.done())
        {
            auto pms = mTerminatePromise;
            pms.resolve();
        }
    }

    auto wptr = weakHandle();
    setTimeout([wptr, this]()
    {
        if (wptr.deleted() || mState != Session::kStateTerminating)
            return;

        if (!mTerminatePromise.done())
        {
            SUB_LOG_INFO("Terminate ack didn't arrive withing timeout, destroying session anyway");
            auto pms = mTerminatePromise;
            pms.resolve();
        }
    }, RtcModule::kSessFinishTimeout, mManager.mKarereClient.appCtx);

    auto pms = mTerminatePromise;
    return pms
    .then([wptr, this, msg]()
    {
        if (wptr.deleted())
            return;
        destroy(mTermCode, msg);
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

    TermCode code = static_cast<TermCode>(packet.payload.read<uint8_t>(8));

    if (mState == kStateTerminating)
    {
        assert(mTermCode != TermCode::kInvalid);
        if (!isTermRetriable(code) && isTermRetriable(mTermCode))
        {
            mTermCode = code;
        }

        // handle terminate as if it were an ack - in both cases the peer is terminating
        msgSessTerminateAck(packet);
        // mTerminate promise is resolved and session object is destroyed when promise is resolved (terminateAndDestroy)
        return;
    }
    else if (mState == kStateDestroyed)
    {
        SUB_LOG_INFO("Ignoring SESS_TERMINATE for a dead session");
        return;
    }
    else
    {
        mTermCode = code;
    }

    if (code == TermCode::kErrIceDisconn && mTsIceConn)
    {
        mIceDisconnectionTs = time(nullptr);
    }

    setState(kStateTerminating);
    destroy(static_cast<TermCode>(mTermCode | TermCode::kPeer));
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

    mRtcConn->RemoveTrackNew(mVideoSender);
    mRtcConn->RemoveTrackNew(mAudioSender);

    removeRtcConnection();

    mRemotePlayer.reset();
    FIRE_EVENT(SESSION, onRemoteStreamRemoved);
    setState(kStateDestroyed);
    FIRE_EVENT(SESSION, onSessDestroy, static_cast<TermCode>(code & (~TermCode::kPeer)),
        !!(code & TermCode::kPeer), msg);
    mCall.removeSession(*this, code);
}

void Session::submitStats(TermCode termCode, const std::string& errInfo)
{
    std::string deviceInformation = mManager.getDeviceInfo();

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

    info.iceDisconnections = mIceDisconnections;
    info.maxIceDisconnectionTime = mMaxIceDisconnectedTime;
    auto sessionReconnectionIt = mCall.mSessionsReconnectionInfo.find(EndpointId(mPeer, mPeerClient));
    if (sessionReconnectionIt != mCall.mSessionsReconnectionInfo.end())
    {
        info.previousSessionId = sessionReconnectionIt->second.getOldSid();
        info.reconnections = sessionReconnectionIt->second.getReconnections();
    }

    if (mStatRecorder)
    {
        std::string stats = mStatRecorder->terminate(info);
        mCall.mManager.mKarereClient.api.sdk.sendChatStats(stats.c_str(), CHATSTATS_PORT);
    }

    return;
}

// we actually verify the whole SDP, not just the fingerprints
bool Session::verifySdpFingerprints(const std::string& sdp)
{
    SdpKey hash;
    mCall.mManager.crypto().mac(sdp, mOwnHashKey, hash);
    bool match = true; // constant time compare
    for (unsigned int i = 0; i < sizeof(SdpKey); i++)
    {
        match &= (hash.data[i] == mPeerHash.data[i]);
    }
    return match;
}

void Session::msgIceCandidate(RtMessage& packet)
{
    if (mState >= kStateTerminating)
    {
        return;
    }

    assert(!mPeerSdpAnswer.empty() || !mPeerSdpOffer.empty());
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
    AvFlags flags(packet.payload.read<uint8_t>(8));
    updateAvFlags(flags);
}

void Session::msgSdpOfferRenegotiate(RtMessage &packet)
{
    if (mState != kStateInProgress)
    {
        SUB_LOG_ERROR("Ignoring SDP_OFFER_RENEGOTIATE received for a session not in kSessInProgress state");
        return;
    }

    // SDP_OFFER_RENEGOTIATE sid.8 anonId.8 encHashKey.32 fprHash.32 av.1 sdpLen.2 sdpOffer.sdpLen
    uint16_t sdpLen = packet.payload.read<uint16_t>(81);
    assert(packet.payload.size() >= 83 + sdpLen);
    packet.payload.read(83, sdpLen, mPeerSdpOffer);
    packet.payload.read(48, mPeerHash);

    setStreamRenegotiationTimeout();
    auto wptr = weakHandle();
    processSdpOfferSendAnswer()
    .then([this, wptr]()
    {
        if (wptr.deleted())
            return;

        renegotiationComplete();
    });
}

void Session::msgSdpAnswerRenegotiate(RtMessage &packet)
{
    if (!mStreamRenegotiationTimer)
    {
        SUB_LOG_INFO("Ingoring SDP_ANSWER_RENEGOTIATE - not in renegotiation state");
        return;
    }

    if (mState != kStateInProgress)
    {
         SUB_LOG_INFO("Ignoring SDP_ANSWER_RENEGOTIATE received for a session not in kSessInProgress state");
         return;
    }

    auto wptr = weakHandle();
    setRemoteAnswerSdp(packet)
    .then([this, wptr]()
    {
        if (wptr.deleted())
            return;

        renegotiationComplete();
    });
}

Session::~Session()
{
    removeRtcConnection();
    cancelIceDisconnectionTimer();
    SUB_LOG_DEBUG("Destroyed");
}

void Session::pollStats()
{
    mRtcConn->GetStats(static_cast<webrtc::StatsObserver*>(mStatRecorder.get()), nullptr, mStatRecorder->getStatsLevel());
    unsigned int statsSize = mStatRecorder->mStats->mSamples.size();
    if (statsSize != mPreviousStatsSize)
    {
        manageNetworkQuality(mStatRecorder->mStats->mSamples.back());
        mPreviousStatsSize = statsSize;
    }
}

void Session::manageNetworkQuality(stats::Sample *sample)
{
    int previousNetworkquality = mNetworkQuality;
    mNetworkQuality = sample->lq;
    if (previousNetworkquality != mNetworkQuality)
    {
        FIRE_EVENT(SESSION, onSessionNetworkQualityChange, mNetworkQuality);
    }
}

bool Session::isTermRetriable(TermCode reason)
{
    TermCode termCode = static_cast<TermCode>(reason & ~TermCode::kPeer);
    return (termCode != TermCode::kErrPeerOffline) && (termCode != TermCode::kUserHangup) && (termCode != TermCode::kAppTerminating);
}

karere::Id SessionReconnectInfo::getOldSid() const
{
    return mOldSid;
}

unsigned int SessionReconnectInfo::getReconnections() const
{
    return mReconnections;
}


void SessionReconnectInfo::setOldSid(const Id &oldSid)
{
    mOldSid = oldSid;
}

void SessionReconnectInfo::setReconnections(unsigned int reconnections)
{
    mReconnections = reconnections;
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

bool ICall::isInProgress() const
{
    return (mState > Call::kStateInitial && mState < Call::kStateTerminating);
}

const char* ISession::stateToStr(uint8_t state)
{
    switch(state)
    {
        RET_ENUM_NAME(kStateInitial);
        RET_ENUM_NAME(kStateWaitSdpOffer);
        RET_ENUM_NAME(kStateWaitSdpAnswer);
        RET_ENUM_NAME(kStateWaitLocalSdpAnswer);
        RET_ENUM_NAME(kStateConnecting);
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
    {
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
    Call::stateToStr
};

const StateDesc Session::sStateDesc = {
    {
        { kStateWaitSdpOffer, kStateWaitSdpAnswer, kStateWaitLocalSdpAnswer},
        { kStateWaitLocalSdpAnswer, kStateTerminating, kStateDestroyed }, //for kStateWaitSdpOffer
        { kStateConnecting, kStateTerminating, kStateDestroyed },         //for kStateWaitLocalSdpAnswer
        { kStateConnecting, kStateTerminating, kStateDestroyed },         //for kStateWaitSdpAnswer
        { kStateInProgress, kStateTerminating, kStateDestroyed},          //for kStateConnecting
        { kStateTerminating, kStateDestroyed },                           //for kStateInProgress
        { kStateDestroyed },                             //for kStateTerminating
        {}                                               //for kStateDestroyed
    },
    Session::stateToStr
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
        RET_ENUM_NAME(RTCMD_SDP_OFFER_RENEGOTIATE);
        RET_ENUM_NAME(RTCMD_SDP_ANSWER_RENEGOTIATE);
        RET_ENUM_NAME(RTCMD_END_ICE_CANDIDATES);
        default: return "(invalid RTCMD)";
    }
}
const char* termCodeToStr(uint8_t code)
{
    switch(code)
    {
        RET_ENUM_NAME(kUserHangup);
        RET_ENUM_NAME(kCallReqCancel);
        RET_ENUM_NAME(kCallRejected);
        RET_ENUM_NAME(kAnsElsewhere);
        RET_ENUM_NAME(kRejElsewhere);
        RET_ENUM_NAME(kAnswerTimeout);
        RET_ENUM_NAME(kRingOutTimeout);
        RET_ENUM_NAME(kAppTerminating);
        RET_ENUM_NAME(kCallerGone);
        RET_ENUM_NAME(kBusy);
        RET_ENUM_NAME(kStreamChange);
        RET_ENUM_NAME(kNotFinished);
        RET_ENUM_NAME(kDestroyByCallCollision);
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
        RET_ENUM_NAME(kErrSessSetupTimeout);
        RET_ENUM_NAME(kErrSessRetryTimeout);
        RET_ENUM_NAME(kErrAlready);
        RET_ENUM_NAME(kErrNotSupported);
        RET_ENUM_NAME(kErrCallSetupTimeout);
        RET_ENUM_NAME(kErrKickedFromChat);
        RET_ENUM_NAME(kErrIceTimeout);
        RET_ENUM_NAME(kErrStreamRenegotation);
        RET_ENUM_NAME(kErrStreamRenegotationTimeout);
        RET_ENUM_NAME(kInvalid);
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
    std::stringstream stream;
    stream << std::hex << clientid;
    auto dataLen = buf.read<uint16_t>(RtMessage::kHdrLen - 2) - 1;
    auto type = buf.read<uint8_t>(23);
    std::string result = Command::opcodeToStr(opcode);
    result.append(": ").append(rtcmdTypeToStr(type));
    result.append(" chatid: ").append(chatid.toString())
          .append(" userid: ").append(userid.toString())
          .append(" clientid: ").append(stream.str());
    StaticBuffer data(nullptr, 0);
    data.assign(buf.readPtr(RtMessage::kPayloadOfs, dataLen), dataLen);

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
        case RTCMD_SESS_TERMINATE:
            result.append(" reason: ").append(termCodeToStr(data.read<uint8_t>(8)));
        case RTCMD_SDP_OFFER:
        case RTCMD_SDP_ANSWER:
        case RTCMD_ICE_CANDIDATE:
        case RTCMD_SESS_TERMINATE_ACK:
        case RTCMD_MUTE:
            result.append(" sid: ").append(Id(data.read<uint64_t>(0)).toString());
            break;
    }
    return result;
}

int Session::calculateNetworkQuality(const stats::Sample *sample)
{
    assert(sample);

    // check audio quality based on packets lost
    long audioPacketLostAverage;
    long audioPacketLost = sample->astats.plDifference;
    if (audioPacketLost > mAudioPacketLostAverage)
    {
        audioPacketLostAverage = mAudioPacketLostAverage;
    }
    else
    {
        audioPacketLostAverage = (mAudioPacketLostAverage * 4 + audioPacketLost) / 5;
    }

    if (audioPacketLostAverage > 2)
    {
        // We have lost audio packets since the last sample, that's the worst network quality
        SUB_LOG_INFO("Audio packets lost, returning 0 link quality");
        return 0;
    }

    // check bandwidth available for video frames wider than 480px
    long bwav = sample->vstats.s.bwav;
    long width = sample->vstats.s.width;
    if (bwav && width)
    {
        if (width >= 480)
        {
            if (bwav < 30)
            {
                return 0;
            }
            else if (bwav < 64)
            {
                return 1;
            }
            else if (bwav < 160)
            {
                return 2;
            }
            else if (bwav < 300)
            {
                return 3;
            }
            else if (bwav < 400)
            {
                return 4;
            }
            else
            {
                return 5;
            }
        }

        // check video frames per second
        long fps = sample->vstats.s.fps;
        if (fps < 15)
        {
            if (fps < 3)
            {
                return 0;
            }
            else if (fps < 5)
            {
                return 1;
            }
            else if (fps < 10)
            {
                return 2;
            }
            else
            {
                return 3;
            }
        }
    }

    // check connection's round-trip time
    if (sample->cstats.rtt)
    {
        long crtt = sample->cstats.rtt;
        if (crtt < 150)
        {
            return 5;
        }
        else if (crtt < 200)
        {
            return 4;
        }
        else if (crtt < 300)
        {
            return 3;
        }
        else if (crtt < 500)
        {
            return 2;
        }
        else if (crtt < 1000)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    SUB_LOG_INFO("Don't have any key stat param to estimate network quality from");
    return kNetworkQualityDefault;
}

void Session::removeRtcConnection()
{
    if (mRtcConn)
    {
        if (mRtcConn->signaling_state() != webrtc::PeerConnectionInterface::kClosed)
        {
            mRtcConn->Close();
        }

        mRtcConn.release();
    }

}

void Session::setStreamRenegotiationTimeout()
{
    if (mStreamRenegotiationTimer)
    {
        SUB_LOG_INFO("New renegotation started, while another in-progress");
        cancelTimeout(mStreamRenegotiationTimer, mManager.mKarereClient.appCtx);
    }

    auto wptr = weakHandle();

    mRenegotiationInProgress = true;
    mStreamRenegotiationTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        mRenegotiationInProgress = false;
        if (!mStreamRenegotiationTimer || mState >= kStateTerminating)
        {
            mStreamRenegotiationTimer = 0;
            return;
        }

        mStreamRenegotiationTimer = 0;
        terminateAndDestroy(TermCode::kErrStreamRenegotationTimeout);
    }, RtcModule::kStreamRenegotiationTimeout, mManager.mKarereClient.appCtx);
}

void Session::renegotiationComplete()
{
    assert(mStreamRenegotiationTimer);
    cancelTimeout(mStreamRenegotiationTimer, mManager.mKarereClient.appCtx);
    mStreamRenegotiationTimer = 0;
    mRenegotiationInProgress = false;
}

promise::Promise<void> Session::setRemoteAnswerSdp(RtMessage &packet)
{
    // SDP_ANSWER sid.8 fprHash.32 av.1 sdpLen.2 sdpAnswer.sdpLen
    mPeerAv.set(packet.payload.read<uint8_t>(40));
    auto sdpLen = packet.payload.read<uint16_t>(41);
    assert((int)packet.payload.dataSize() >= sdpLen + 43);
    packet.payload.read(43, sdpLen, mPeerSdpAnswer);
    packet.payload.read(8, mPeerHash);
    if (!verifySdpFingerprints(mPeerSdpAnswer))
    {
        terminateAndDestroy(TermCode::kErrFprVerifFailed, "Fingerprint verification failed, possible forgery");
        return promise::_Void();
    }

    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface *sdp = webrtc::CreateSessionDescription("answer", mPeerSdpAnswer, &error);
    if (!sdp)
    {
        terminateAndDestroy(TermCode::kErrSdp, "Error parsing peer SDP answer: line="+error.line+"\nError: "+error.description);
        return promise::_Void();
    }

    FIRE_EVENT(SESSION, onPeerMute, mPeerAv, AvFlags(0));
    if (mPeerAv.onHold())
    {
        setOnHold(mPeerAv.onHold());
    }

    auto wptr = weakHandle();
    return mRtcConn.setRemoteDescription(sdp)
    .fail([wptr, this](const ::promise::Error& err)
    {
        if (wptr.deleted())
            return;

        std::string msg = "Error setting SDP answer: " + err.msg();
        terminateAndDestroy(TermCode::kErrSdp, msg);
    });
}

void Session::setOnHold(bool onHold)
{
    FIRE_EVENT(SESSION, onOnHold, onHold);

    if (onHold)
    {
        if (mCall.mLocalFlags.video())
        {
            if (mVideoSender->track())
            {
                mVideoSender->track()->set_enabled(false);
            }
        }

        if (mPeerAv.video() && mRemotePlayer)
        {
            mRemotePlayer->enableVideo(false);
        }

        if (mCall.mLocalFlags.audio() && mAudioSender && mAudioSender->track())
        {
            mAudioSender->track()->set_enabled(false);
        }
    }
    else if (!mCall.mLocalFlags.onHold())
    {
        if (mPeerAv.video())
        {
            if (mRemotePlayer)
            {
                mRemotePlayer->enableVideo(true);
            }
        }

        if (mCall.mLocalFlags.video() && mVideoSender && mVideoSender->track())
        {
            mVideoSender->track()->set_enabled(true);
        }

        if (mCall.mLocalFlags.audio() && mAudioSender && mAudioSender->track())
        {
            mAudioSender->track()->set_enabled(true);
        }
    }
}

void Session::handleIceConnectionRecovered()
{
    if (!mIceDisconnectionTs)
    {
        return;
    }

    cancelTimeout(mMediaRecoveryTimer, mManager.mKarereClient.appCtx);
    mMediaRecoveryTimer = 0;

    time_t iceReconnectionDuration = time(nullptr) - mIceDisconnectionTs;
    if (iceReconnectionDuration > mMaxIceDisconnectedTime)
    {
        mMaxIceDisconnectedTime = iceReconnectionDuration;
    }

    mIceDisconnections++;
}

void Session::handleIceDisconnected()
{
    mIceDisconnectionTs = time(nullptr);
    cancelIceDisconnectionTimer();

    auto wptr = this->weakHandle();
    mMediaRecoveryTimer = setTimeout([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        SUB_LOG_INFO("Timed out waiting for media connection to recover, terminating session");
        terminateAndDestroy(TermCode::kErrIceDisconn);
    }, RtcModule::kMediaConnRecoveryTimeout, mManager.mKarereClient.appCtx);
}

void Session::cancelIceDisconnectionTimer()
{
    if (mMediaRecoveryTimer)
    {
        cancelTimeout(mMediaRecoveryTimer, mManager.mKarereClient.appCtx);
        mMediaRecoveryTimer = 0;
    }
}

promise::Promise<void> Session::getPeerKeey()
{
    mFechingPeerKeys = true;
    auto wptr = weakHandle();
    return mCall.loadCryptoPeerKey(mPeer).then([wptr, this](Buffer *) -> promise::Promise<void>
    {
        if (wptr.deleted())
        {
            promise::Promise<void> promise;
            promise.reject("Destroyed while waiting for peer key");
            return promise;
        }

        mFechingPeerKeys = false;
        return promise::_Void();
    });
}

void Session::processPackets()
{
    for (RtMessage packet : mPacketQueue)
    {
        if (mFechingPeerKeys)
        {
            return;
        }

        handleMessage(packet);
    }
}

AudioLevelMonitor::AudioLevelMonitor(const Session &session, ISessionHandler &sessionHandler)
    : mSessionHandler(sessionHandler), mSession(session)
{
}

void AudioLevelMonitor::OnData(const void *audio_data, int bits_per_sample, int /*sample_rate*/, size_t number_of_channels, size_t number_of_frames)
{
    if (!mSession.receivedAv().audio())
    {
        if (mAudioDetected)
        {
            mAudioDetected = false;
            mSessionHandler.onSessionAudioDetected(mAudioDetected);
        }

        return;
    }

    assert(bits_per_sample == 16);
    time_t nowTime = time(NULL);
    if (nowTime - mPreviousTime > 2) // Two seconds between samples
    {
        mPreviousTime = nowTime;
        size_t valueCount = number_of_channels * number_of_frames;
        int16_t *data = (int16_t*)audio_data;
        int16_t audioMaxValue = data[0];
        int16_t audioMinValue = data[0];
        for (size_t i = 1; i < valueCount; i++)
        {
            if (data[i] > audioMaxValue)
            {
                audioMaxValue = data[i];
            }

            if (data[i] < audioMinValue)
            {
                audioMinValue = data[i];
            }
        }

        bool audioDetected = (abs(audioMaxValue) + abs(audioMinValue) > kAudioThreshold);
        if (audioDetected != mAudioDetected)
        {
            mAudioDetected = audioDetected;
            mSessionHandler.onSessionAudioDetected(mAudioDetected);
        }
    }
}

void globalCleanup()
{
    if (!artc::isInitialized())
        return;
    artc::cleanup();
}
}
