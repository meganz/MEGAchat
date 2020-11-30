#include "sfu.h"
#include <base/promise.h>
#include <megaapi.h>

namespace sfu
{

std::string Command::COMMAND_IDENTIFIER = "cmd";
std::string AVCommand::COMMAND_NAME = "AV";
std::string AnswerCommand::COMMAND_NAME = "ANSWER";

SfuConnection::SfuConnection(const std::string &sfuUrl, karere::Client& karereClient, karere::Id cid)
    : mSfuUrl(sfuUrl)
    , mKarereClient(karereClient)
    , mCid(cid)
{
    mCommands[AVCommand::COMMAND_NAME] = mega::make_unique<AVCommand>(std::bind(&SfuConnection::handleAvCommand, this,  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    mCommands[AnswerCommand::COMMAND_NAME] = mega::make_unique<AnswerCommand>(std::bind(&SfuConnection::handleAnswerCommand, this,  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
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

bool SfuConnection::handleAvCommand(karere::Id cid, karere::Id peer, int av)
{
    return true;
}

bool SfuConnection::handleAnswerCommand(karere::Id, int, std::vector<karere::Id>, const std::string &, std::vector<karere::Id>)
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

    rapidjson::Value::ConstMemberIterator peerIterator = command.FindMember("peer");
    if (peerIterator == command.MemberEnd() || !peerIterator->value.IsString())
    {
        SFU_LOG_ERROR("Received data doesn't have 'peer' field");
        return false;
    }

    std::string peerString = peerIterator->value.GetString();
    ::mega::MegaHandle peer = ::mega::MegaApi::base64ToUserHandle(peerString.c_str());

    rapidjson::Value::ConstMemberIterator avIterator = command.FindMember("av");
    if (avIterator == command.MemberEnd() || !avIterator->value.IsInt())
    {
        SFU_LOG_ERROR("Received data doesn't have 'av' field");
        return false;
    }

    int av = avIterator->value.GetInt();
    return mComplete(cid, peer, av);;
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

    std::vector<karere::Id> peers;
    for (unsigned int j = 0; j < peersIterator->value.Capacity(); ++j)
    {
        if (peersIterator->value[j].IsString())
        {
            std::string peerString = peersIterator->value[j].GetString();
            ::mega::MegaHandle peer = ::mega::MegaApi::base64ToUserHandle(peerString.c_str());
            peers.push_back(peer);
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::processCommand: invalid value at array 'peers'");
            return false;
        }
    }

    rapidjson::Value::ConstMemberIterator speakersIterator = command.FindMember("speakers");
    if (speakersIterator == command.MemberEnd() || !speakersIterator->value.IsArray())
    {
        SFU_LOG_ERROR("AnswerCommand::processCommand: Received data doesn't have 'speakers' field");
        return false;
    }

    std::vector<karere::Id> speakers;
    for (unsigned int j = 0; j < speakersIterator->value.Capacity(); ++j)
    {
        if (speakersIterator->value[j].IsString())
        {
            std::string speakerString = speakersIterator->value[j].GetString();
            ::mega::MegaHandle speaker = ::mega::MegaApi::base64ToUserHandle(speakerString.c_str());
            peers.push_back(speaker);
        }
        else
        {
            SFU_LOG_ERROR("AnswerCommand::processCommand: invalid value at array 'speakers'");
            return false;
        }
    }

    //TODO: vthumbs

    return mComplete(cid, isModerator, peers, sdpString, speakers);
}
}
