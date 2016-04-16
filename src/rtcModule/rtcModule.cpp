#include "rtcModule.h"
#include "stringUtils.h"
#include "../base/services.h"
#include "strophe.jingle.session.h"
#include "rtcStats.h"
#include <base/services-http.hpp>
#include <retryHandler.h>
#include <serverListProvider.h>
#include "rtcmPrivate.h"
#include "IRtcModule.h"
#include "streamPlayer.h"
#include "IVideoRenderer.h"
#include "ICryptoFunctions.h"

using namespace std;
using namespace promise;
using namespace strophe;
using namespace mega;
using namespace karere;
using namespace placeholders;

namespace rtcModule
{

RtcModule::RtcModule(xmpp_conn_t* conn, IGlobalEventHandler* handler,
               ICryptoFunctions* crypto, const char* iceServers)
:Jingle(conn, handler, crypto, iceServers)
{
    mOwnAnonId = crypto->scrambleJid(mConn.fullJid());
    initInputDevices();
}

void RtcModule::discoAddFeature(const char* feature)
{
    mGlobalHandler->discoAddFeature(feature);
}

void RtcModule::initInputDevices()
{
    auto& devices = mDeviceManager.inputDevices();
    if (!devices.audio.empty())
        selectAudioInDevice(devices.audio[0].name);
    if (!devices.video.empty())
        selectVideoInDevice(devices.video[0].name);
    KR_LOG_INFO("Input devices on this system:");
    for (const auto& dev: devices.audio)
        KR_LOG_INFO("\tAudio: %s [id=%s]", dev.name.c_str(), dev.id.c_str());
    for (const auto& dev: devices.video)
        KR_LOG_INFO("\tVideo: %s [id=%s]", dev.name.c_str(), dev.id.c_str());
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
bool RtcModule::selectVideoInDevice(const string &devname)
{
    return selectDevice(devname, mDeviceManager.inputDevices().video, mVideoInDeviceName);
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
bool RtcModule::hasCaptureActive()
{
    return (mAudioInput || mVideoInput);
}

std::shared_ptr<artc::LocalStreamHandle> RtcModule::getLocalStream(std::string& errors)
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
        auto opts = std::make_shared<artc::MediaGetOptions>(*device);
        //opts.constraints.SetMandatoryMinWidth(1280);
        //opts.constraints.SetMandatoryMinHeight(720);
        mVideoInput = deviceManager.getUserVideo(opts);
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
        mAudioInput = deviceManager.getUserAudio(
                std::make_shared<artc::MediaGetOptions>(*device));
    }
    catch(exception& e)
    {
        mAudioInput.reset();
        errors.append("Error getting audio device: ")
              .append(e.what()?e.what():"Unknown error")+='\n';
    }
    return std::make_shared<artc::LocalStreamHandle>(
                mAudioInput?mAudioInput.getTrack():nullptr,
                mVideoInput?mVideoInput.getTrack():nullptr);
}

bool Call::startLocalStream(bool allowEmpty)
{
    string errors;
    if (mLocalStream)
        return true;

    try
    {
        mLocalStream = mRtc.getLocalStream(errors);
        assert(mLocalStream);
        createLocalPlayer();
        return true;
    }
    catch(std::exception& e)
    {
        errors.append("Exception while getting local media:\n");
        auto msg = e.what();
        if (!msg)
            msg = "(No exception message)";
        errors.append(msg)+='\n';
    }

    //we can be here only if there was an exception
    mLocalStream.reset();
    bool cont = allowEmpty;
    if (allowEmpty)
        RTCM_EVENT(this, onLocalMediaFail, errors, &cont);
    if (cont)
        return true;

    RTCM_EVENT(this, onLocalMediaFail, errors, nullptr);
    hangup(kNoMediaError, errors.c_str());
    return false;
}
 
void RtcModule::onConnState(const xmpp_conn_event_t status,
            const int error, xmpp_stream_error_t * const stream_error)
{
    Base::onConnState(status, error, stream_error);
    switch (status)
    {
        case XMPP_CONN_FAIL:
        case XMPP_CONN_DISCONNECT:
        {
            hangupAll(Call::kXmppDisconnError); //TODO: Maybe move to Jingle?
            break;
        }
        case XMPP_CONN_CONNECT:
        {
            mConn.addHandler(std::bind(&RtcModule::onPresenceUnavailable, this, _1),
               NULL, "presence", "unavailable");
            break;
        }
    }
}

void RtcModule::startMediaCall(IEventHandler* handler, const std::string& targetJid,
        AvFlags av, const char* files[], const std::string& myJid)
{
  if (!handler || targetJid.empty())
      throw std::runtime_error("No handler or target JID specified");
  enum State
  {
      kNotYetUserMedia = 0, //not yet got usermedia
      kGotUserMediaWaitingPeer = 1, //got user media and waiting for peer
      kPeerAnsweredOrTimedout = 2, //peer answered or timed out,
      kCallCanceledByUs = 3 //call was canceled by us via the cancel() method of the call request object
  };
  struct StateContext
  {
      RtcModule& mSelf;
      string sid;
      string targetJid;
      string ownFprMacKey;
      string myJid;
      bool isBroadcast;
      xmpp_uid ansHandler = 0;
      xmpp_uid declineHandler = 0;
      State state = kNotYetUserMedia;
      artc::tspMediaStream sessStream;
      AvFlags av = AvFlags(false, false);
      karere::ServerList<karere::TurnServerInfo> turnServers;
      unique_ptr<vector<string> > files;
      StateContext(RtcModule& module): mSelf(module){}
      void freeHandlersExcept(xmpp_uid* handler = nullptr)
      {
          if (handler)
              *handler = 0;
          if (ansHandler)
          {
              mSelf.mConn.removeHandler(ansHandler);
              ansHandler = 0;
          }
          if (declineHandler)
          {
              mSelf.mConn.removeHandler(declineHandler);
              declineHandler = 0;
          }
      }
  };
  shared_ptr<StateContext> state = make_shared<StateContext>(*this);
  state->av = av; //we need to remember av in cancel handler as well, for freeing the local stream
  state->isBroadcast = strophe::getResourceFromJid(targetJid).empty();
  state->ownFprMacKey = crypto().generateFprMacKey();
  state->sid = crypto().generateRandomString(RTCM_SESSIONID_LEN);
  state->myJid = myJid;
  state->targetJid = targetJid;
//  state->files = describeFiles(files);  TODO: Implement
  auto initiateCallback = make_shared<std::function<void(Call&)>>(
  [this, state](Call& aCall)
  {
      auto actualAv = aCall.mLocalStream ? aCall.mLocalStream->av() : AvFlags(false,false);
      if (state->av != actualAv)
      {
          KR_LOG_WARNING("startMediaCall: Could not obtain audio or video stream requested by the user");
          state->av = actualAv;
      }
      if (state->state == kCallCanceledByUs)
      {//call was canceled before we got user media
          return;
      }
      state->state = kGotUserMediaWaitingPeer;
// Call accepted handler
      state->ansHandler = mConn.addHandler([this, state](Stanza stanza, void*, bool& keep)
      {
          try
          {
              if (stanza.attr("sid") != state->sid)
                  return; //message not for us, keep handler(by default keep==true)

              keep = false;
              state->freeHandlersExcept(&state->ansHandler);

              if (state->state != kGotUserMediaWaitingPeer)
                  return;

              GET_CALL(state->sid, kCallStateOutReq, return);

              state->state = kPeerAnsweredOrTimedout;
              //TODO: enable when implemented in js
              auto av = AvFlags(true,true); //parseAvString(stanza.attr("media"));
              if (!av.any() && !call->mLocalStream->av().any())
              {
                  call->hangup(Call::kNoMediaError);
                  return;
              }
// The crypto exceptions thrown here will simply discard the call request and remove the handler
              string peerFprMacKey = stanza.attr("fprmackey");
              try
              {
                  call->mPeerFprMacKey = crypto().decryptMessage(peerFprMacKey);
                  if (call->mPeerFprMacKey.empty())
                      call->mPeerFprMacKey = crypto().generateFprMacKey();
              }
              catch(exception& e)
              {
                  call->mPeerFprMacKey = crypto().generateFprMacKey();
              }
              call->mPeerAnonId = stanza.attr("anonid");
              if (call->mPeerAnonId.empty())
                  throw runtime_error("Empty anonId in peer's call answer stanza");
              const char* fullPeerJid = stanza.attr("from");
              if (state->isBroadcast)
              {
                  Stanza msg(mConn);
                  msg.setName("message")
                     .setAttr("to", strophe::getBareJidFromJid(state->targetJid).c_str())
                     .setAttr("type", "megaNotifyCallHandled")
                     .setAttr("sid", state->sid.c_str())
                     .setAttr("by", fullPeerJid)
                     .setAttr("accepted", "1");
                  xmpp_send(mConn, msg);
              }

              call->createSession(fullPeerJid, state->myJid);
              // TODO: files
              // files?ftManager.createUploadHandler(sid, fullPeerJid, fileArr):NULL);
              call->initiate();
        }
        catch(runtime_error& e)
        {
              erase(state->sid);
              KR_LOG_ERROR("Exception in call answer handler:\n%s\nIgnoring call", e.what());
        }
      }, NULL, "message", "megaCallAnswer", state->targetJid.c_str(), nullptr, STROPHE_MATCH_BAREJID);

//Call declined handler
      state->declineHandler = mConn.addHandler([this, state](Stanza stanza, void*, bool& keep)
      {
          if (stanza.attr("sid") != state->sid) //this message was not for us
              return;
          keep = false;
          state->freeHandlersExcept(&state->declineHandler);
          if (state->state != kGotUserMediaWaitingPeer)
              return;
          GET_CALL(state->sid, kCallStateOutReq, return);

          ScopedCallDestroy remover(*call, Call::kUserHangup|Call::kPeer);
          state->state = kPeerAnsweredOrTimedout;
          Stanza body = stanza.child("body", true);
          if (body)
          {
              const char* txt = body.textOrNull();
              if (txt)
                  remover.text = karere::xmlUnescape(txt);
          }
          const char* fullPeerJid = stanza.attr("from");
          call->mPeerJid = fullPeerJid;

          if (state->isBroadcast)
          {
              Stanza msg(mConn);
              msg.setName("message")
                 .setAttr("to", getBareJidFromJid(state->targetJid.c_str()).c_str())
                 .setAttr("type", "megaNotifyCallHandled")
                 .setAttr("sid", state->sid.c_str())
                 .setAttr("by", fullPeerJid)
                 .setAttr("accepted", "0");
              mConn.send(msg);
          }
      },
      nullptr, "message", "megaCallDecline", state->targetJid.c_str(), nullptr, STROPHE_MATCH_BAREJID);

      Stanza msg(mConn);
      msg.setName("message")
         .setAttr("to", state->targetJid.c_str())
         .setAttr("type", "megaCall")
         .setAttr("sid", state->sid.c_str())
         .setAttr("fprmackey",
              crypto().encryptMessageForJid(state->ownFprMacKey, state->targetJid).c_str())
         .setAttr("anonid", mOwnAnonId.c_str());
/*          if (files)
          {
              var infos = {};
              fileArr = [];
              var uidPrefix = sid+Date.now();
              var rndArr = new Uint32Array(4);
              crypto.getRandomValues(rndArr);
              for (var i=0; i<4; i++)
                  uidPrefix+=rndArr[i].toString(32);
              for (var i=0; i<options.files.length; i++) {
                  var file = options.files[i];
                  file.uniqueId = uidPrefix+file.name+file.size;
                  fileArr.push(file);
                  var info = {size: file.size, mime: file.type, uniqueId: file.uniqueId};
                  infos[file.name] = info;
              }
              msgattrs.files = JSON.stringify(infos);
          } else {
 */
      msg.setAttr("media", state->av.toString().c_str());
      mConn.send(msg);

      if (!state->files)
      {
          setTimeout([this, state]()
          {
              if (state->state != kGotUserMediaWaitingPeer)
                  return;
              state->state = kPeerAnsweredOrTimedout;
              hangupBySid(state->sid, Call::kAnswerTimeout);
          },
          callAnswerTimeout);
      };
  }); //end initiateCallback()
  //return an object with a cancel() method
  Call::HangupFunc cancelFunc = [this, state](TermCode reason, const string& text) -> bool //call request cancel function
  {
      state->freeHandlersExcept();
      state->state = kCallCanceledByUs;
      Stanza cancelMsg(mConn);
      cancelMsg.setName("message")
               .setAttr("to", getBareJidFromJid(state->targetJid).c_str())
               .setAttr("sid", state->sid.c_str())
               .setAttr("type", "megaCallCancel")
               .setAttr("reason", reason==Call::kUserHangup
                        ? "user" : Call::termcodeToReason(reason).c_str());
      if(!text.empty())
          cancelMsg.setAttr("text", text.c_str());
      mConn.send(cancelMsg);
      return true;
  };

  auto& call = addCall(kCallStateOutReq, true, handler, state->sid,
        std::move(cancelFunc), targetJid, state->av, !!files, state->myJid);
  call->mOwnFprMacKey = state->ownFprMacKey;
  RTCM_EVENT(call, onOutgoingCallCreated, static_pointer_cast<ICall>(call));

  if (!call->startLocalStream(true))
  { //user explicitly chose not to contunue
      return;
  }
  auto cryptoPms =
    crypto().preloadCryptoForJid(getBareJidFromJid(state->targetJid));

  auto gelbPms = mTurnServerProvider->getServers(mIceFastGetTimeout)
  .then([state](ServerList<TurnServerInfo>* servers)
  {
      state->turnServers = *servers;
  });

  //TODO: Maybe call initiateCallback in parallel with fetching crypto and querying gelb
  when(cryptoPms, gelbPms)
  .then([this, state, call, initiateCallback]()
  {
      setIceServers(state->turnServers);
      (*initiateCallback)(*call);
  })
  .fail([](const Error& err)
  {
      KR_LOG_ERROR("Call setup failed: %s", err.what());
  });
}

inline bool callTypeMatches(int type, char userType)
{
    if (userType == 'a')
        return true;
    bool isFtCall = (type & 0x0100);
    return (((userType == 'm') && !isFtCall) ||
            ((userType == 'f') && isFtCall));
}

int RtcModule::muteUnmute(AvFlags what, bool state, const std::string& bareJid)
{
    size_t affected = 0;
    if (bareJid.empty())
    {
        for (auto& item: *this)
        {
            item.second->muteUnmute(what, state);
            affected++;
        }
    }
    else
    {
        for (auto& call: *this)
        {
            //TODO: call can have its peerJid be a bare jid (in case no session yet),
            //match against that as well
            const auto& peer = call.second->mPeerJid;
            if (bareJid != getBareJidFromJid(peer))
                continue;
            affected++;
            call.second->muteUnmute(what, state);
        }
    }
// If we are muting all calls, local video playback will be disabled by the refcounting
// mechanism as well
// In Firefox, all local streams are only references to gLocalStream, so muting any of them
// mutes all and the local video playback.
// In Chrome all local streams are independent, so the local video stream has to be
// muted explicitly as well
    return affected;
}

void RtcModule::onPresenceUnavailable(Stanza pres)
{
    const char* from = pres.attr("from");
    for (auto it = begin(); it != end();)
    {
        if (it->second->mPeerJid == from)
        {
            auto erased = it++;
            erased->second->hangup(Call::kXmppDisconnError, nullptr);
        }
        else
        {
            it++;
        }
    }
}

void Call::createLocalPlayer()
{
    assert(mLocalStream);
// This is called by myGetUserMedia when the the local stream is obtained (was not open before)
    if (mLocalPlayer) //should never happend
    {
        KR_LOG_ERROR("Local stream just obtained, but mLocalPlayer was not NULL");
        mLocalPlayer->attachVideo(mLocalStream->video());
        return;
    }
    IVideoRenderer* renderer = NULL;
    RTCM_EVENT(this, onLocalStreamObtained, renderer);
    if (!renderer)
    {
        hangup(Call::kInternalError, "onLocalStreamObtained did not return a video renderer interface");
        return;
    }
    mLocalPlayer.reset(new artc::StreamPlayer(renderer, nullptr, mLocalStream->video()));
    mLocalPlayer->start();
// TODO: Maybe provide some interface to the player, but must be virtual because it crosses the module boundary
//  maybeCreateVolMon();
}


void Call::removeRemotePlayer()
{
    if (!mRemotePlayer)
    {
        KR_LOG_ERROR("removeVideo: remote player is already NULL");
        return;
    }
    mRemotePlayer->stop();
    mRemotePlayer.reset();
    RTCM_EVENT(this, removeRemotePlayer);
}
//Called by the remote media player when the first frame is about to be rendered, analogous to
//onMediaRecv in the js version
void Call::onMediaStart()
{
    if (!mRemotePlayer || !mSess)
    {
        KR_LOG_DEBUG("Received onMediaStart but remote player or session is NULL");
        return;
    }
    stats::Options statOptions;
    mHasReceivedMedia = true;
    RTCM_EVENT(this, onMediaRecv, statOptions);
    if (statOptions.enableStats)
    {
        if (statOptions.scanPeriod < 0)
            statOptions.scanPeriod = 1000;
        if (statOptions.maxSamplePeriod < 0)
            statOptions.maxSamplePeriod = 5000;

        mSess->mStatsRecorder.reset(new stats::Recorder(*mSess, statOptions));
        mSess->mStatsRecorder->start();
    }
    mSess->tsMediaStart = karere::timestampMs();
}

void Call::createSession(const std::string& peerJid, const std::string& ownJid,
    FileTransferHandler *ftHandler)
{
    assert(!mSess);
//  assert(!ownJid.empty());  FIXME
    assert(!peerJid.empty());
    mOwnJid = ownJid;
    mPeerJid = peerJid;
    mSess.reset(new JingleSession(*this, ftHandler));
}

bool Call::hangup(TermCode termcode, const string& text, bool rejectIncoming)
{
    if (mState == kCallStateEnded)
        return false;
    bool ret = (mHangupFunc && !mSess
            && ((mState != kCallStateInReq) || rejectIncoming))
        ? mHangupFunc(termcode, text)
        : true;
    destroy(termcode, text);
    return ret;
}

std::shared_ptr<stats::IRtcStats>
Call::hangupSession(TermCode termcode, const string& text, bool nosend)
{
    assert(mSess);
    auto reason = Call::termcodeToReason(termcode);
    mSess->terminate(reason, text, nosend);
    std::shared_ptr<stats::IRtcStats> ret(mSess->mStatsRecorder //stats are created only if onRemoteSdp occurs
        ? static_cast<stats::IRtcStats*>(mSess->mStatsRecorder->mStats.release())
        : static_cast<stats::IRtcStats*>(new stats::BasicStats(*this, reason.c_str())));

    mSess.reset();
    mState = kCallStateEnded;
    return ret;
}

void Call::destroy(TermCode termcode, const std::string& text, bool noSessTermSend)
{
    if (mState == kCallStateEnded)
        throw std::runtime_error("Call::destroy: call already destoyed");
    std::shared_ptr<stats::IRtcStats> stats(mSess
        ? hangupSession(termcode, text, noSessTermSend)
        : std::shared_ptr<stats::IRtcStats>(new stats::BasicStats(*this, Call::termcodeToReason(termcode))));

    setState(kCallStateEnded);
    mLocalPlayer.reset();
    mRemotePlayer.reset();
    mLocalStream.reset(); //guarantees camera release, if object destroy is delayed because of shared_ptr references kept somewhere

    auto it = mRtc.find(mSid);
    if (it == mRtc.end())
        throw std::runtime_error("Call::destroy: BUG: Call wasn't in the calls map");
    std::shared_ptr<Call> call(it->second);
    mRtc.erase(it);
//post to the end of app message queue as there may be queued events related to the
//call that don't have a shared_ptr to the call object, such as video frames
    ::mega::marshallCall([call, termcode, text, stats]()
    {
        KR_LOG_RTC_EVENT("%s -> onCallEnded: %s%s, msg: '%s'", call->mSid.c_str(),
            termcodeToMsg(termcode), ((termcode&kPeer)?" by peer":""), text.c_str());
        try
        {
            call->mHandler->onCallEnded(termcode, text, stats);
        }
        catch(...){}
    });

    //post stats, references to the call object end here
    auto json = std::make_shared<std::string>();
    stats->toJson(*json);
    ::mega::retry([json](int no)
    {
        return ::mega::http::postString("https://stats.karere.mega.nz/stats", json, "application/json");
    })
    .then([](const std::shared_ptr<std::string>& response)
    {
        KR_LOG_DEBUG("Callstats successfully posted");
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error posting stats: %s", err.what());
    });
}

//onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
void Call::onRemoteStreamAdded(artc::tspMediaStream stream)
{
    if (!mSess)
    {
        KR_LOG_ERROR("onRemoteStreamAdded for a call with no session");
        return;
    }
    if (mRemotePlayer)
    {
        KR_LOG_WARNING("onRemoteStreamAdded: Session '%s' already has a remote player, ignoring event", mSid.c_str());
        return;
    }

    IVideoRenderer* renderer = NULL;
    RTCM_EVENT(this, onRemoteSdpRecv, renderer);
    if (!renderer)
    {
        hangup(kInternalError, "onRemoteSdpRecv: No video renderer provided by application");
        return;
    }
    mRemotePlayer.reset(new artc::StreamPlayer(renderer));
    mRemotePlayer->setOnMediaStart(std::bind(&Call::onMediaStart, this));
    mRemotePlayer->attachToStream(stream);
    mRemotePlayer->start();
}

void Call::onRemoteStreamRemoved(artc::tspMediaStream)
{
    if(!mRemotePlayer)
        return;
    removeRemotePlayer();
}
void Call::changeLocalRenderer(IVideoRenderer* renderer)
{
    if (!mLocalPlayer)
        throw std::runtime_error("No local stream yet, please wait for onLocalStreamObtained");
    mLocalPlayer->changeRenderer(renderer);
}

//void onRemoteStreamRemoved() - not interested to handle here

AvFlags Call::sentAv() const
{
    if (!mLocalStream)
        return AvFlags(false, false);
    else
        return mLocalStream->effectiveAv();
}

AvFlags Call::receivedAv() const
{
    if (!mSess)
        return AvFlags(false, false);
    auto& remoteStream = mSess->getRemoteStream();
    if (!remoteStream)
        return AvFlags(false, false);
    auto ret = mSess->mRemoteAvState;
    ret.audio &= !remoteStream->GetAudioTracks().empty();
    ret.video &= !remoteStream->GetVideoTracks().empty();
    return ret;
}

std::shared_ptr<Call> RtcModule::getCallByJid(const char* fullJid, char type)
{
    if(!fullJid)
        return nullptr;
 //TODO: We get only the first media session to fullJid, but there may be more
    for (auto& item: *this)
    {
        auto& call = item.second;
        if (call->mPeerJid != fullJid)
            continue;
        if (((type == 'm') && call->mIsFileTransfer) ||
            ((type == 'f') && !call->mIsFileTransfer))
            continue;
        return call;
    }
    return nullptr;
}

/**
    Releases any global resources referenced by this instance, such as the reference
    to the local stream and video. This should be called especially if multiple instances
    of RtcSession are used in a single JS context
 */
RtcModule::~RtcModule()
{
    destroyAll(Call::kUserHangup);
    if (mAudioInput || mVideoInput)
    {
        KR_LOG_ERROR("RtcModule::~RtcModule: BUG: After destroying all calls, media input devices are still in use");
    }
}

int Call::isRelayed() const
{
    if (!mSess || !mSess->mStatsRecorder)
        return RTCM_EUNKNOWN;
    return mSess->mStatsRecorder->isRelay();
}

void Call::onPeerMute(AvFlags affected)
{
    RTCM_EVENT(this, onPeerMute, affected);
}
void Call::onPeerUnmute(AvFlags affected)
{
    RTCM_EVENT(this, onPeerUnmute, affected);
}

void Call::muteUnmute(AvFlags what, bool state)
{
//First do the actual muting, and only then send the signalling
    auto prevAv = mLocalAv;
    auto newState = mLocalAv;
    if (what.audio)
        newState.audio = state;
    if (what.video)
        newState.video = state;
    if (mLocalStream)
    {
        mLocalStream->setAvState(newState);
        newState = mLocalAv = mLocalStream->effectiveAv();
    }
    if (mLocalPlayer && (newState.video != prevAv.video))
    {
        if (newState.video)
            mLocalPlayer->start();
        else
            mLocalPlayer->stop();
    }
    if (mSess)
        mSess->sendMuteDelta(prevAv, mLocalAv);
}

std::string Call::id() const
{
    return mSid;
}

bool gInitialized = false;

RTCM_API IRtcModule* create(xmpp_conn_t* conn, IGlobalEventHandler* handler,
                  ICryptoFunctions* crypto, const char* iceServers)
{
    if (!gInitialized)
    {
        artc::init(nullptr);
        gInitialized = true;
    }
    return new RtcModule(conn, handler, crypto, iceServers);
}

RTCM_API void globalCleanup()
{
    if (!gInitialized)
        return;
    artc::cleanup();
    gInitialized = false;
}

} //end namespace

