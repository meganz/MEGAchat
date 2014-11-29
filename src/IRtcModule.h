#ifndef IRTC_MODULE_H
#define IRTC_MODULE_H

#include "IJingleSession.h"
#include "IVideoRenderer.h"
#include "mstrophepp.h" //only needed for IPlugin

namespace rtcModule
{
/** RtcModule API Error codes */
enum {
    RTCM_EOK = 0,
    RTCM_EINVAL = -1,
    RTCM_EUNKNOWN = -2,
    RTCM_ENOTFOUND = -3,
    RTCM_ENONE = -4
};
/** Length of Jingle session id strings */
enum {
    RTCM_SESSIONID_LEN = 16
};

/** Interface of object passed to onIncomingCallRequest. This interface contains methods
 *  to get info about the call request, has a method to answer or reject the call.
 */
struct IAnswerCall
{
    virtual int answer(bool accept, const AvFlags& answerAv,
                       const char* reason, const char* text) = 0;
    virtual const char* const* files() const = 0;
    virtual const AvFlags& peerAv() const = 0;
    virtual bool reqStillValid() const = 0;
    virtual void destroy() { delete this; }
protected:
    virtual ~IAnswerCall() {} //deleted via destoy() only to use our memory manager
};

struct StatOptions
{
    bool enableStats = true;
    int scanPeriod = -1;
    int maxSamplePeriod = -1;
};
struct IRtcStats {};
/** Public event handler callback interface.
 *  All events from the Rtc module are sent to the application via this interface */
struct IEventHandler
{
    virtual void addDiscoFeature(const char* feature) {}
    virtual void onLocalMediaFail(const char* errMsg, int* cont=nullptr) {}
    virtual void onCallInit(IJingleSession* sess, int isDataCall) {}
    virtual void oncallDeclined(const char* peer, const char* sid, const char* reason,
        const char* text, int isDataCall){}
    virtual void onCallAnswerTimeout(const char* peer) {}
    virtual void onLocalStreamObtained(IVideoRenderer** rendererRet) {}
    virtual void onCallIncomingRequest(IAnswerCall* ansCtrl) {}
    virtual void onCallAnswered(IJingleSession* sess) {}
    virtual void remotePlayerRemove(IJingleSession* sess, IVideoRenderer* videoRenderer) {}
    virtual void onMediaRecv(IJingleSession* sess, StatOptions* statOptions) {}
    virtual void onCallEnded(IJingleSession* sess, IRtcStats* stats) {}
    virtual void onRemoteSdpRecv(IJingleSession* sess, IVideoRenderer** rendererRet) {}
    virtual void onJingleError(IJingleSession* sess, const char* origin, const char* stanza,
                   const char* origXml, char type) {}
    virtual void onLocalVideoDisabled() {}
    virtual void onLocalVideoEnabled() {}
};

//===
/** Interface of the RtcModule */
class IRtcModule: public virtual strophe::IPlugin
{
public:
    /**
    Initiates a media call to the specified peer
    @param targetJid
    The JID of the callee. Can be a full jid (including resource),
    or a bare JID (without resource), in which case the call request will be broadcast
    using a special <message> packet. For more details on the call broadcast mechanism,
    see the Wiki
    @param av Whether to send local video/audio. Ignored if \c files is not NULL
    @param myJid
    Necessary only if doing MUC, because the user's JID in the
    room is different than her normal JID. If NULL,
    the user's 'normal' JID will be used
    @param sidOut - a pointer to a string buffer that will receive the session id of the
    call that is initiated. The size if the buffer must be at least RTCM_SESSIONID_SIZE+1 bytes
    @param files Array of null-terminated strings specifying the files that are to be sent,
    in case this is a data call. NULL if a media call is to be initiated.
    @returns 0 on success, a negative RTCM error code in failure
    */
    virtual int startMediaCall(char* sidOut, const char* targetJid, const AvFlags& av,
        const char* files[]=nullptr, const char* myJid=nullptr) = 0;
    virtual int hangupBySid(const char* sid, char callType, const char* reason, const char* text) = 0;
    virtual int hangupByPeer(const char* peerJid, char callType, const char* reason, const char* text) = 0;
    virtual int hangupAll(const char* reason, const char* text) = 0;
    /**
    Mutes/unmutes audio/video
    @param state
        Specifies whether to mute or unmute:
        <i>true</i> mutes,  <i>false</i> unmutes.
    @param what
        Determine whether the (un)mute operation applies to audio and/or video channels
    @param jid
        If not NULL, specifies that the mute operation will apply only
        to the call to the given JID. If not specified,
        the (un)mute will be applied to all ongoing calls.
    */
    virtual int muteUnmute(bool state, const AvFlags& what, const char* jid) = 0;
    virtual int getSentAvBySid(const char* sid, AvFlags& av) = 0;
    virtual int getSentAvByJid(const char* jid, AvFlags& av) = 0;
    /**
    Get info whether remote audio and video are being received at the moment in a call to the specified JID
    @param fullJid The full peer JID to identify the call
    @param av Returns the audio/video flags
    @returns 0 on success or an RTCM error code on fail
    */
    virtual int getReceivedAvBySid(const char* sid, AvFlags& av) = 0;
    /** Similar to \cgetSentAvByJid, but the call is identified by the full jis of the peer */
    virtual int getReceivedAvByJid(const char* jid, AvFlags& av) = 0;
    virtual IJingleSession* getSessionByJid(const char* fullJid, char type='m') = 0;
    virtual IJingleSession* getSessionBySid(const char* sid) = 0;
    /**
    Updates the ICE servers that will be used in the next call.
    @param iceServers An array of ice server objects - same as the iceServers parameter in
          the RtcSession constructor
    */
    /** url:xxx, user:xxx, pass:xxx; url:xxx, user:xxx... */
    virtual int updateIceServers(const char* iceServers) = 0;
    /** Returns whether the call or file transfer with the given
    sessionId is being relaid via a TURN server or not.
      @param sid The session id of the call
      @returns 1 if the call/transfer is being relayed, 0 if not, negative RTCM error code if
     there was an error or the status is unknown (not established yet or no statistics available)
    */
    virtual int isRelay(const char* sid) = 0;
    virtual void destroy() = 0;
protected:
//    virtual ~IRtcModule(){}
};

}

#endif

