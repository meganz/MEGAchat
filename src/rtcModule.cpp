#include "rtcModule.h"
#include "StringUtils.h"
#include "base/services.h"
#include "strophe.jingle.session.h"

#define RTCM_EVENT(name,...) \
    printf("Event: %s\n", #name);
//    mEventHandler->name(#__VA_ARGS__)

using namespace std;
using namespace promise;
using namespace strophe;
using namespace mega;
using namespace placeholders;

namespace rtcModule
{
//===
string avFlagsToString(const AvFlags& av);
AvFlags avFlagsOfStream(artc::tspMediaStream& stream, const AvFlags& flags);

RtcModule::RtcModule(xmpp_conn_t* conn, IEventHandler* handler,
               ICryptoFunctions* crypto, const char* iceServers)
:Jingle(conn, crypto, iceServers), mEventHandler(handler)
{}

void RtcModule::logInputDevices()
{
    auto& devices = mDeviceManager.inputDevices();
    KR_LOG_DEBUG("Input devices on this system:");
    for (const auto& dev: devices.audio)
        KR_LOG_DEBUG("\tAudio: %s\n", dev.name.c_str());
    for (const auto& dev: devices.video)
        KR_LOG_DEBUG("\tVideo: %s\n", dev.name.c_str());
}

string RtcModule::getLocalAudioAndVideo()
{
    string errors;
    if (hasLocalStream())
        throw runtime_error("getLocalAudioAndVideo: Already has tracks");
    const auto& devices = deviceManager.inputDevices();
    if (devices.video.size() > 0)
      try
        {
             mLocalTracks.video = deviceManager.getUserVideo(
                          artc::MediaGetOptions(devices.video[0]));
        }
        catch(exception& e)
        {
            errors.append("Error getting video device: ")
                  .append(e.what()?e.what():"Unknown error")+='\n';
        }

    if (devices.audio.size() > 0)
        try
        {
            mLocalTracks.audio = deviceManager.getUserAudio(
                          artc::MediaGetOptions(devices.audio[0]));
        }
        catch(exception& e)
        {
            errors.append("Error getting audio device: ")
                  .append(e.what()?e.what():"Unknown error")+='\n';
        }
    return errors;
}
template <class OkCb, class ErrCb>
void RtcModule::myGetUserMedia(const AvFlags& av, OkCb okCb, ErrCb errCb, bool allowEmpty)
{
    bool lmfCalled = false;
    try
    {
        string errors;
        bool alreadyHadStream = hasLocalStream();
        if (!alreadyHadStream)
        {
            errors = getLocalAudioAndVideo();
            mLocalStreamRefCount = 0;
        }
        auto stream = artc::gWebrtcContext->CreateLocalMediaStream("localStream");
        if(!stream.get())
        {
            string msg = "Could not create local stream: CreateLocalMediaStream() failed";
            lmfCalled = true;
            RTCM_EVENT(onLocalMediaFail, msg.c_str());
            return errCb(msg);
        }

        if (!hasLocalStream() || !errors.empty())
        {
            string msg = "Error getting local stream track(s)";
            if (!errors.empty())
                msg.append(": ").append(errors);

            if (!allowEmpty)
            {
                lmfCalled = true;
                RTCM_EVENT(onLocalMediaFail, msg.c_str());
                return errCb(msg);
            }
            else
            {
                lmfCalled = true;
                bool cont = false;
                RTCM_EVENT(onLocalMediaFail, msg.c_str(), &cont);
                if (!cont)
                    return errCb(msg);
            }
        }

        if (mLocalTracks.audio)
            KR_THROW_IF_FALSE(stream->AddTrack(
                              deviceManager.cloneAudioTrack(mLocalTracks.audio.get())));
        if (mLocalTracks.video)
            KR_THROW_IF_FALSE(stream->AddTrack(
                              deviceManager.cloneVideoTrack(mLocalTracks.video.get())));

        if (!alreadyHadStream)
            createLocalPlayer(); //creates local player but does not link it to stream
        refLocalStream(av.video); //links player to stream
        okCb(stream);
    }
    catch(exception& e)
    {
        if (!lmfCalled)
            RTCM_EVENT(onLocalMediaFail, e.what());
        return errCb(e.what()?e.what():"Error getting local media stream");
    }
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
            terminateAll("disconnected", nullptr, true); //TODO: Maybe move to Jingle?
            //freeLocalStreamIfUnused();
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

int RtcModule::startMediaCall(char* sidOut, const char* targetJid, const AvFlags& av,
                              const char* files[], const char* myJid)
{
  if (!sidOut || !targetJid)
      return RTCM_EINVAL;
  enum State
  {
      kNotYetUserMedia = 0, //not yet got usermedia
      kGotUserMediaWaitingPeer = 1, //got user media and waiting for peer
      kPeerAnsweredOrTimedout = 2, //peer answered or timed out,
      kCallCanceledByUs = 3 //call was canceled by us via the cancel() method of the call request object
  };
  struct StateContext
  {
      string sid;
      string targetJid;
      string ownFprMacKey;
      string myJid;
      bool isBroadcast;
      xmpp_handler ansHandler = nullptr;
      xmpp_handler declineHandler = nullptr;
      State state = kNotYetUserMedia;
      artc::tspMediaStream sessStream;
      AvFlags av;
      unique_ptr<vector<string> > files;
  };
  shared_ptr<StateContext> state(new StateContext);
  state->av = av; //we need to remember av in cancel handler as well, for freeing the local stream
  state->isBroadcast = strophe::getResourceFromJid(targetJid).empty();
  state->ownFprMacKey = VString(crypto().generateFprMacKey()).c_str();
  state->sid = VString(crypto().generateRandomString(RTCM_SESSIONID_LEN)).c_str();
  state->myJid = myJid?myJid:"";
  state->targetJid = targetJid;
//  state->files = describeFiles(files);  TODO: Implement
  auto initiateCallback = [this, state, files](artc::tspMediaStream sessStream)
  {
      auto actualAv = getStreamAv(sessStream);
      if ((state->av.audio && !actualAv.audio) || (state->av.video && !actualAv.video))
      {
          KR_LOG_WARNING("startMediaCall: Could not obtain audio or video stream requested by the user");
          state->av.audio = actualAv.audio;
          state->av.video = actualAv.video;
      }
      if (state->state == kCallCanceledByUs)
      {//call was canceled before we got user media
          //freeLocalStreamIfUnused();
          return;
      }
      state->state = kGotUserMediaWaitingPeer;
      state->sessStream = sessStream;
// Call accepted handler
      state->ansHandler = mConn.addHandler([this, state](Stanza stanza, void*, bool& keep)
      {
          try
          {
              if (stanza.attr("sid") != state->sid)
                  return; //message not for us, keep handler(by default keep==true)

              keep = false;
              mCallRequests.erase(state->sid);

              if (state->state != kGotUserMediaWaitingPeer)
                  return;
              state->state = kPeerAnsweredOrTimedout;
              mConn.removeHandler(state->declineHandler);
              state->declineHandler = nullptr;
              state->ansHandler = nullptr;
// The crypto exceptions thrown here will simply discard the call request and remove the handler
              string peerFprMacKey = stanza.attr("fprmackey");
              try
              {
                  peerFprMacKey = VString(crypto().decryptMessage(peerFprMacKey.c_str())).c_str();
                  if (peerFprMacKey.empty())
                      peerFprMacKey = VString(crypto().generateFprMacKey()).c_str();
              }
              catch(exception& e)
              {
                  peerFprMacKey = VString(crypto().generateFprMacKey()).c_str();
              }
              const char* peerAnonId = stanza.attr("anonid");
              if (peerAnonId[0] == 0) //empty
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

              initiate(state->sid.c_str(), fullPeerJid,
                  state->myJid.empty()?mConn.jid():state->myJid.c_str(), state->sessStream,
                  state->av, {
                      {"ownFprMacKey", state->ownFprMacKey.c_str()},
                      {"peerFprMacKey", peerFprMacKey.c_str()},
                      {"peerAnonId", peerAnonId}
                  })
                 // TODO: files
                 // files?ftManager.createUploadHandler(sid, fullPeerJid, fileArr):NULL);
              .then([this](const std::shared_ptr<JingleSession>& sess)
              {
                  RTCM_EVENT(onCallInit, sess, !!state->files);
                  return 0;
              });
        }
        catch(runtime_error& e)
        {
          unrefLocalStream(state->av.video);
          KR_LOG_ERROR("Exception in call answer handler:\n%s\nIgnoring call", e.what());
        }
      }, NULL, "message", "megaCallAnswer", state->targetJid.c_str(), nullptr, STROPHE_MATCH_BAREJID);

//Call declined handler
      state->declineHandler = mConn.addHandler([this, state](Stanza stanza, void*, bool& keep)
      {
          if (stanza.attr("sid") != state->sid) //this message was not for us
              return;

          keep = false;
          mCallRequests.erase(state->sid);

          if (state->state != kGotUserMediaWaitingPeer)
              return;

          state->state = kPeerAnsweredOrTimedout;
          mConn.removeHandler(state->ansHandler);
          state->ansHandler = nullptr;
          state->declineHandler = nullptr;
          state->sessStream = nullptr;
          unrefLocalStream(state->av.video);

          string text;
          Stanza body = stanza.child("body", true);
          if (body)
          {
              const char* txt = body.textOrNull();
              if (txt)
                  text = xmlUnescape(txt);
          }
          const char* fullPeerJid = stanza.attr("from");

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
          RTCM_EVENT(oncallDeclined,
              fullPeerJid, //peer
              state->sid.c_str(),
              stanza.attrOrNull("reason"),
              text.empty()?nullptr:text.c_str(),
              !!files, //isDataCall
          );
      },
      nullptr, "message", "megaCallDecline", state->targetJid.c_str(), nullptr, STROPHE_MATCH_BAREJID);

      auto sendCall = new function<void(const char*)>([this, state](const char* errMsg)
      {
          if (errMsg)
          {
              onInternalError(errMsg, "preloadCryptoForJid");
              return;
          }
          Stanza msg(mConn);
          msg.setName("message")
             .setAttr("to", state->targetJid.c_str())
             .setAttr("type", "megaCall")
             .setAttr("sid", state->sid.c_str())
             .setAttr("fprmackey",
                 VString(crypto().encryptMessageForJid(state->ownFprMacKey.c_str(), state->targetJid.c_str())).c_str())
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
          msg.setAttr("media", avFlagsToString(state->av).c_str());
          mConn.send(msg);

          if (!state->files)
              setTimeout([this, state]()
              {
                  mCallRequests.erase(state->sid);
                  if (state->state != 1)
                      return;

                  state->state = kPeerAnsweredOrTimedout;
                  mConn.removeHandler(state->ansHandler);
                  state->ansHandler = nullptr;
                  mConn.removeHandler(state->declineHandler);
                  state->declineHandler = nullptr;
                  state->sessStream = nullptr;
                  unrefLocalStream(state->av.video);
                  Stanza cancelMsg(mConn);
                  cancelMsg.setName("message")
                      .setAttr("type", "megaCallCancel")
                      .setAttr("to", getBareJidFromJid(state->targetJid).c_str());
                  xmpp_send(mConn, cancelMsg);
                  RTCM_EVENT(onCallAnswerTimeout, state->targetJid.c_str());
              },
              callAnswerTimeout);
      });
      crypto().preloadCryptoForJid(strophe::getBareJidFromJid(state->targetJid).c_str(),
          static_cast<void*>(sendCall), [](void* userp, const char* errMsg)
          {
              unique_ptr<function<void(const char*)> >
                      sendCallFunc(static_cast<function<void(const char*)>* >(userp));
              (*sendCallFunc)(errMsg);
          });

  }; //end initiateCallback()
  if (state->av.audio || state->av.video) //same as av, but we use state->av to make sure it's set up correctly
      myGetUserMedia(state->av, initiateCallback, [](const string& errMsg){}, true);
  else
      initiateCallback(nullptr);

  //return an object with a cancel() method
  if (mCallRequests.find(state->sid) != mCallRequests.end())
      throw runtime_error("Assert failed: There is already a call request with this sid");
  mCallRequests.emplace(std::piecewise_construct,
    std::forward_as_tuple(state->sid),
    std::forward_as_tuple(new CallRequest(state->targetJid, !!state->files,
    [this, state]() //call request cancel function
    {
      mCallRequests.erase(state->sid);
      if (state->state == kPeerAnsweredOrTimedout)
          return false;
      if (state->state == kGotUserMediaWaitingPeer)
      { //same as if (ansHandler)
            state->state = kCallCanceledByUs;
            mConn.removeHandler(state->ansHandler);
            state->ansHandler = nullptr;
            mConn.removeHandler(state->declineHandler);
            state->declineHandler = nullptr;

            unrefLocalStream(state->av.video);
            Stanza cancelMsg(mConn);
            cancelMsg.setName("message")
                    .setAttr("to", getBareJidFromJid(state->targetJid).c_str())
                    .setAttr("sid", state->sid.c_str())
                    .setAttr("type", "megaCallCancel");
            mConn.send(cancelMsg);
            return true;
      }
      else if (state->state == kNotYetUserMedia)
      {
          state->state = kCallCanceledByUs;
          return true;
      }
      KR_LOG_WARNING("RtcSession: BUG: cancel() called when state has an unexpected value of %d", state->state);
      return false;
  })));
  strncpy(sidOut, state->sid.c_str(), RTCM_SESSIONID_LEN);
  return 0;
}

template <class CB>
void RtcModule::enumCallsForHangup(CB cb, const char* reason, const char* text)
{
    for (auto callReq=mCallRequests.begin(); callReq!=mCallRequests.end();)
    {
        if (cb(callReq->first, callReq->second->targetJid,
               1|(callReq->second->isFileTransfer?0x0100:0)))
        {
            auto erased = callReq;
            callReq++;
            erased->second->cancel(); //erases 'erased'
        }
        else
        {
            callReq++;
        }
    }
    for (auto ans=mAutoAcceptCalls.begin(); ans!=mAutoAcceptCalls.end();)
    {
        if (cb(ans->first, ans->second->at("from"),
               2|(ans->second->ftHandler?0x0100:0)))
        {
            auto erased = ans;
            ans++;
            cancelAutoAcceptEntry(erased, reason, text);
        }
        else
        {
            ans++;
        }
    }
    for (auto sess=mSessions.begin(); sess!=mSessions.end();)
    {
        if (cb(sess->first, sess->second->peerJid(),
               3|(sess->second->ftHandler()?0x0100:0)))
        {
            auto erased = sess;
            sess++;
            terminateBySid(erased->first.c_str(), reason, text);
        }
        else
        {
            sess++;
        }
    }
}
inline bool callTypeMatches(int type, char userType)
{
    bool isFtCall = (type & 0x0100);
    return (((userType == 'm') && !isFtCall) ||
            ((userType == 'f') && isFtCall));
}

int RtcModule::hangupBySid(const char* sid, char callType, const char* reason, const char* text)
{
    int term = 0;
    enumCallsForHangup([sid, &term, callType](const string& aSid, const string& peer, int type)
    {
        if (!callTypeMatches(type, callType))
            return false;

        if (aSid == sid)
        {
            if (term)
                KR_LOG_WARNING("hangupBySid: BUG: Already terminated one call with that sid");
            term++;
            return true;
        }
        else
        {
            return false;
        }
    },
    reason, text);
    return term;
}
int RtcModule::hangupByPeer(const char* peerJid, char callType, const char* reason, const char* text)
{
    int term = 0;
    bool isBareJid = !!strchr(peerJid, '/');
    if (isBareJid)
        enumCallsForHangup([peerJid, callType, &term](const string& aSid, const string& peer, int type)
        {
            if (!callTypeMatches(type, callType))
                return false;
            if (getBareJidFromJid(peer) == peerJid)
            {
                term++;
                return true;
            }
            else
            {
                return false;
            }
        }, reason, text);
    else
        enumCallsForHangup([peerJid, &term, callType](const string& aSid, const string& peer, int type)
        {
            if (!callTypeMatches(type, callType))
                return false;
            if (peer == peerJid)
            {
                term++;
                return true;
            }
            else
            {
                return false;
            }
        }, reason, text);
    return term;
}

int RtcModule::hangupAll(const char* reason, const char* text)
{
    bool term = 0;
    enumCallsForHangup([&term](const string&, const string&, int)
    {
        term++;
        return true;
    },
    reason, text);
    return term;
}

int RtcModule::muteUnmute(bool state, const AvFlags& what, const char* jid)
{
    int affected = 0;
    if (jid)
    {
        bool isBareJid = !!strchr(jid, '/');
        for (auto& sess: mSessions)
        {
            const auto& peer = sess.second->peerJid();
            bool match = isBareJid?(jid==getBareJidFromJid(peer)):(jid==peer);
            if (!match)
                continue;
            affected++;
            sess.second->muteUnmute(state, what);
        }
    }
// If we are muting all calls, disable also local video playback as well
// In Firefox, all local streams are only references to gLocalStream, so muting any of them
// mutes all and the local video playback.
// In Chrome all local streams are independent, so the local video stream has to be
// muted explicitly as well
    if (what.video && (!jid || (affected >= mSessions.size())))
    {
        if (state)
            disableLocalVideo();
        else
            enableLocalVideo();
    }
    return affected;
}

void RtcModule::onPresenceUnavailable(Stanza pres)
{
    const char* from = pres.attr("from");
    for (auto sess = mSessions.begin(); sess!=mSessions.end();)
    {
        if (sess->second->peerJid() == from)
        {
            auto erased = sess++;
            terminate(erased->second.get(), "peer-disconnected", nullptr);
        }
        else
        {
            sess++;
        }
    }
}

void RtcModule::createLocalPlayer()
{
// This is called by myGetUserMedia when the the local stream is obtained (was not open before)
    if (mLocalVideo)
        throw new Error("Local stream just obtained, but localVid was not NULL");
    IVideoRenderer* renderer = NULL;
    RTCM_EVENT(onLocalStreamObtained, &renderer);
    if (!renderer)
    {
        onInternalError("User event handler did not return a video renderer interface", "onLocalStreamObtained");
        return;
    }
    mLocalVideo.reset(new artc::StreamPlayer(renderer, nullptr, nullptr));
//    RTCM_EVENT(onLocalStreamObtained); //TODO: Maybe provide some interface to the player, but must be virtual because it crosses the module boundary
//    maybeCreateVolMon();
}

struct AnswerCallController: public IAnswerCall
{
    RtcModule& self;
    shared_ptr<CallAnswerFunc> mAnsFunc;
    shared_ptr<function<bool(void)> > mReqStillValid;
    AvFlags mPeerAv;
    shared_ptr<set<string> > mFiles;
    vector<const char*> mFileCStrings;

    AnswerCallController(RtcModule& aSelf, shared_ptr<CallAnswerFunc>& ansFunc,
      shared_ptr<function<bool()> >& aReqStillValid, const AvFlags& aPeerAv,
      shared_ptr<set<string> >& aFiles)
    :self(aSelf), mAnsFunc(ansFunc),
      mReqStillValid(aReqStillValid), mPeerAv(aPeerAv), mFiles(aFiles)
    {
        if(mFiles)
        {
            for(auto& f: *mFiles)
                mFileCStrings.push_back(f.c_str());
            mFileCStrings.push_back(nullptr);
        }
    }
    virtual const char* const* files() const
    {
        if (!mFiles)
            return nullptr;
        else
            return &(mFileCStrings[0]);
    }
    virtual const AvFlags& peerAv() const {return mPeerAv;}
    virtual bool reqStillValid() const {return (*mReqStillValid)();}
    virtual int answer(bool accept, const AvFlags &answerAv,
        const char *reason, const char *text)
    {
        if (!accept)
            return (*mAnsFunc)(false, nullptr, reason?reason:"busy", text)?0:ECANCELED;
        if (!(*mReqStillValid)())
            return ECANCELED;
        if (!mFiles)
        {
            //WARNING: The lifetime of the object may be shorter than of the callbacks,
            //so we don't capture 'this' and use the members, but capture all members
            //we need
            auto& ansFunc = mAnsFunc;
            self.myGetUserMedia(answerAv,
               [ansFunc, answerAv](artc::tspMediaStream sessStream)
            {
                shared_ptr<AnswerOptions> opts(new AnswerOptions);
                opts->localStream = sessStream;
                opts->av = avFlagsOfStream(sessStream, answerAv);
                (*ansFunc)(true, opts, nullptr, nullptr);
            },
            [ansFunc](const string& err)
            {
                (*ansFunc)(false, shared_ptr<AnswerOptions>(), "error", ("There was a problem accessing user's camera or microphone. Error: "+err).c_str());
            },
            //allow to answer with no media only if peer is sending something
            (mPeerAv.audio || mPeerAv.video));
        }
        else
        {//file transfer
            (*mAnsFunc)(true, nullptr, nullptr, nullptr);
        }
      return 0;
    }
};

void RtcModule::onIncomingCallRequest(const char* from, shared_ptr<CallAnswerFunc>& ansFunc,
    shared_ptr<function<bool()> >& reqStillValid, const AvFlags& peerMedia,
    shared_ptr<set<string> >& files)
{
    //this is the C cross-module answer function that the application calls to answer or reject the call
    AnswerCallController* ansCtrl = new AnswerCallController(
                *this, ansFunc, reqStillValid, peerMedia, files);
    RTCM_EVENT(onCallIncomingRequest, ansCtrl);
}
void RtcModule::onCallAnswered(JingleSession& sess)
{
    RTCM_EVENT(onCallAnswered, sess);
}

void RtcModule::removeRemotePlayer(JingleSession& sess)
{
    /**
        The media session with peer JID has been destroyed, and the video element
            has to be removed from the DOM.
        @event "remote-player-remove"
        @type object
        @property {string} id The id of the html video element to be removed
        @property {string} peer The full jid of the peer
        @property {SessWrapper} sess
    */
    if (!sess.remotePlayer)
    {
        KR_LOG_ERROR("removeVideo: remote player is already NULL");
        return;
    }
    sess.remotePlayer->stop();
    RTCM_EVENT(remotePlayerRemove, sess, remotePlayer->preDestroy());
    sess.remotePlayer.reset();
}
//Called by the remote media player when the first frame is about to be rendered, analogous to
//onMediaRecv in the js version
void RtcModule::onMediaStart(const string& sid)
{
    auto it = mSessions.find(sid);
    if (it == mSessions.end())
    {
        KR_LOG_DEBUG("Received onMediaStart for a non-existent sessions");
        return;
    }
    JingleSession& sess = *(it->second);
    if (!sess.remotePlayer)
    {
        KR_LOG_DEBUG("Received onMediaStart for a session witn NULL remote player");
        return;
    }
    StatOptions statOptions;
    RTCM_EVENT(onMediaRecv, &sess, &statOptions);
    if (statOptions.enableStats)
    {
        if (statOptions.scanPeriod < 0)
            statOptions.scanPeriod = 1;
        if (statOptions.maxSamplePeriod < 0)
            statOptions.maxSamplePeriod = 5;

        sess.mStatsRecorder.reset(new StatsRecorder(sess, statOptions));
        sess.mStatsRecorder->start();
    }
    sess.tsMediaStart = timestampMs();
}

void RtcModule::onCallTerminated(JingleSession* sess, const char* reason, const char* text,
                      FakeSessionInfo* noSess)
{
 //WARNING: sess may be a dummy object, only with peerjid property, in case something went
 //wrong before the actual session was created, e.g. if SRTP fingerprint verification failed
   if (sess)
   {
       removeRemoteVideo(*sess);
       unrefLocalStream(sess->mLocalAvState.video);

       assert(sess->mStatsRecorder);
       auto stats = sess->mStatsRecorder->terminate(makeCallId(sess));
       stats->termRsn = reason?reason:"(unknown)";
//           jQuery.ajax(this.statsUrl, {
//                type: 'POST',
//                data: JSON.stringify(obj.stats||obj.basicStats)
//        });
       RTCM_EVENT(onCallEnded, sess, stats.get());
   }
   else //no sess
   {
       BasicStats bstats(*noSess, reason);
       RTCM_EVENT(onCallEnded, usess, &bstats);
   }
 }

//onRemoteStreamAdded -> onMediaStart() event from player -> onMediaRecv() -> addVideo()
void RtcModule::onRemoteStreamAdded(JingleSession& sess, artc::tspMediaStream stream)
{
    if (sess.remotePlayer)
    {
        KR_LOG_WARNING("onRemoteStreamAdded: Session '%s' already has a remote player, ignoring event", sess.sid().c_str());
        return;
    }

    IVideoRenderer* renderer = NULL;
    IVideoRenderer** rendererRet = stream->GetVideoTracks().empty()?nullptr:&renderer;
    RTCM_EVENT(onRemoteSdpRecv, &sess, rendererRet);
    if (rendererRet && !*rendererRet)
        KR_LOG_ERROR("onRemoteSdpRecv: No video renderer provided by application");
    sess.remotePlayer.reset(new artc::StreamPlayer(renderer));
    sess.remotePlayer->setOnMediaStart(std::bind(&RtcModule::onMediaStart, this, sess.sid()));
    sess.remotePlayer->attachToStream(stream);
}

//void onRemoteStreamRemoved() - not interested to handle here

void RtcModule::onJingleError(JingleSession* sess, const std::string& origin,
                           const string& stanza, strophe::Stanza orig, char type)
{
    const char* errType = NULL;
    if (type == 't')
        errType = "Timeout";
    else if (type == 's')
        errType = "Error";
    else
        errType = "(Bug)";
    auto origXml = orig.dump();
    KR_LOG_ERROR("%s getting response to '%s' packet, session: '%s'\nerror-packet:\n%s\norig-packet:\n%s\n",
            errType, origin.c_str(), sess?sess->sid().c_str():"(none)", stanza.c_str(),
            origXml.c_str());
    RTCM_EVENT(onJingleError, sess, origin.c_str(), stanza.c_str(), origXml.c_str(), type);
}

int RtcModule::getSentAvBySid(const char* sid, AvFlags& av)
{
    if (!sid)
        return RTCM_EINVAL;
    auto it = mSessions.find(sid);
    if (it == mSessions.end())
        return RTCM_ENOTFOUND;
    auto& localStream = it->second->getLocalStream();
    if (!localStream)
        return RTCM_ENONE;
//we don't use sess.mutedState because in Forefox we don't have a separate
//local streams for each session, so (un)muting one session's local stream applies to all
//other sessions, making mutedState out of sync
    auto audTracks = localStream->GetAudioTracks();
    auto vidTracks = localStream->GetVideoTracks();
    av.audio = (!audTracks.empty() && audTracks[0]->enabled());
    av.video = (!vidTracks.empty() && vidTracks[0]->enabled());
    return 0;
}
template <class F>
int RtcModule::getAvByJid(const char* jid, AvFlags& av, F&& func)
{
    bool isBare = (strchr(jid, '/') == nullptr);
    for (auto& sess: mSessions)
    {
        string sessJid = isBare?getBareJidFromJid(sess.second->peerJid()):sess.second->peerJid();
        if (sessJid == jid)
            return func(sess.first.c_str(), av);
    }
    return RTCM_ENOTFOUND;
}
int RtcModule::getSentAvByJid(const char* jid, AvFlags& av)
{
    return getAvByJid(jid, av, bind(&RtcModule::getSentAvBySid, this, _1, _2));
}

int RtcModule::getReceivedAvBySid(const char* sid, AvFlags& av)
{
    if (!sid)
        return RTCM_EINVAL;
    auto it = mSessions.find(sid);
    if (it == mSessions.end())
        return RTCM_ENOTFOUND;
    auto& remoteStream = it->second->getRemoteStream();
    if (!remoteStream)
        return RTCM_ENONE;
    auto& m = it->second->mRemoteAvState;
    av.audio = !remoteStream->GetAudioTracks().empty() && m.audio;
    av.video = !remoteStream->GetVideoTracks().empty() && m.video;
    return 0;
}
int RtcModule::getReceivedAvByJid(const char* jid, AvFlags& av)
{
    return getAvByJid(jid, av, bind(&RtcModule::getReceivedAvByJid, this, _1, _2));
}

IJingleSession* RtcModule::getSessionByJid(const char* fullJid, char type)
{
    if(!fullJid)
        return nullptr;
 //TODO: We get only the first media session to fullJid, but there may be more
    for (auto& it: mSessions)
    {
        JingleSession& sess = *(it.second);
        if (((type == 'm') && sess.ftHandler()) ||
            ((type == 'f') && !sess.ftHandler()))
            continue;
        if (sess.peerJid() == fullJid)
            return static_cast<IJingleSession*>(it.second.get());
    }
    return nullptr;
}

IJingleSession* RtcModule::getSessionBySid(const char* sid)
{
    if (!sid)
        return nullptr;
    auto it = mSessions.find(sid);
    if (it == mSessions.end())
        return nullptr;
    else
        return it->second.get();
}

/**
  Updates the ICE servers that will be used in the next call.
  @param iceServers An array of ice server objects - same as the iceServers parameter in
          the RtcSession constructor
*/
/** url:xxx, user:xxx, pass:xxx; url:xxx, user:xxx... */
int RtcModule::updateIceServers(const char* servers)
{
    bool has = false;
    try
    {
        has = setIceServers(servers);
    }
    catch(exception& e)
    {
        KR_LOG_WARNING("Error setting ICE servers: %s", e.what());
        return RTCM_EINVAL;
    }
    return has?1:0;
}

void RtcModule::refLocalStream(bool sendsVideo)
{
    mLocalStreamRefCount++;
    if (sendsVideo)
    {
        mLocalVideoRefCount++;
        enableLocalVideo();
    }
}
void RtcModule::unrefLocalStream(bool sendsVideo)
{
    mLocalStreamRefCount--;
    if (sendsVideo)
        mLocalVideoRefCount--;

    if ((mLocalStreamRefCount <= 0) && (mLocalVideoRefCount > 0))
    {
        onInternalError("BUG: local stream refcount dropped to zero, but local video refcount is > 0", "unrefLocalStream");
        mLocalVideoRefCount = 0; //quick fix, should never happen
    }
    if (mLocalVideoRefCount <= 0)
        disableLocalVideo();

    if (mLocalStreamRefCount <= 0)
        freeLocalStream();
}

void RtcModule::freeLocalStream()
{
    if (!hasLocalStream())
    {
        KR_LOG_WARNING("freeLocalStream: local stream is null");
        return;
    }
    if (mLocalStreamRefCount > 0)
    {
        onInternalError(("BUG: localStream refcount is > 0 ("+to_string(mLocalStreamRefCount)+")").c_str(), "freeLocalStream");
        return;
    }
    if (mLocalStreamRefCount < 0)
    {
        KR_LOG_WARNING("freeLocalStream: local stream refcount is negative: %d", mLocalStreamRefCount);
        mLocalStreamRefCount = 0; //in case it was negative for some reason
    }
    if (mLocalVideoRefCount != 0)
    {
        onInternalError("About to free local stream, but local video refcount is not 0", "freeLocalStream");
        disableLocalVideo(); //detaches local stream from local video player
    }
    mLocalTracks.audio = nullptr;
    mLocalTracks.video = nullptr;
}
 /**
    Releases any global resources referenced by this instance, such as the reference
    to the local stream and video. This should be called especially if multiple instances
    of RtcSession are used in a single JS context
 */
 RtcModule::~RtcModule()
{
    hangupAll("app-terminate", nullptr);
    if (mLocalTracks.audio || mLocalTracks.video || mLocalVideo)
    {
        onInternalError("BUG: Local stream or local player was not freed", "RtcModule::~RtcModule");
    }
}

int RtcModule::isRelay(const char* sid)
{
    if (!sid)
        return RTCM_EINVAL;
    auto it = mSessions.find(sid);
    if (it == mSessions.end())
        return RTCM_ENOTFOUND;
    if (!it->second->mStatsRecorder)
        return RTCM_EUNKNOWN;
     return it->second->mStatsRecorder->isRelay();
}


AvFlags avFlagsOfStream(artc::tspMediaStream& stream, const AvFlags& flags)
{
    AvFlags ret;
    if (!stream)
    {
        ret.audio = false;
        ret.video = false;
    }
    else
    {
        ret.audio = (flags.audio && (!stream->GetAudioTracks().empty()));
        ret.video = (flags.video && (!stream->GetVideoTracks().empty()));
    }
    return ret;
}


void RtcModule::disableLocalVideo()
{
    if (!mLocalVideoEnabled)
        return;
     if (!mLocalVideo)
     {
         onInternalError("mLocalVideoEnabled is true, but there is no local player", "disableLocalVideo");
         return;
     }
// All references to local video are muted, disable local video display
    mLocalVideo->detachVideo();
    mLocalVideoEnabled = false;
    RTCM_EVENT(onLocalVideoDisabled);
}

void RtcModule::enableLocalVideo()
{
    if(mLocalVideoEnabled)
        return;
    if (!mLocalVideo)
    {
        onInternalError("Can't enable video, there is no local player", "enableLocalVideo");
        return;
    }
    if (mLocalTracks.video)
        mLocalVideo->attachVideo(mLocalTracks.video);
    RTCM_EVENT(onLocalVideoEnabled);
    mLocalVideoEnabled = true;
    mLocalVideo->start();
}

void RtcModule::removeRemoteVideo(JingleSession& sess)
{

}

/**
 Creates a unique string identifying the call,
 that is independent of whether the
 caller or callee generates it. Used only for sending stats
*/
string RtcModule::makeCallId(IJingleSession* sess)
{
    assert(sess);
    if (sess->isCaller())
        return mOwnAnonId+":"+sess->getPeerAnonId()+":"+sess->getSid();
      else
        return string(sess->getPeerAnonId())+":"+mOwnAnonId+":"+sess->getSid();
}

AvFlags RtcModule::getStreamAv(artc::tspMediaStream& stream)
{
    AvFlags result;
    if (!stream)
    {
        result.audio = result.video = false;
    }
    else
    {
        result.audio = !stream->GetAudioTracks().empty();
        result.video = !stream->GetVideoTracks().empty();
    }
    return result;
}

string avFlagsToString(const AvFlags &av)
{
    string result;
    if (av.audio)
        result+='a';
    if (av.video)
        result+='v';
    if (result.empty())
        result+='_';
    return result;

} //end namespace
}
