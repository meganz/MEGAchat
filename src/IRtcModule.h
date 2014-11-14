#include "strophe.jingle.h"
#include "streamPlayer.h"

namespace karere
{
namespace rtcModule
{
/** Interface of object passed to onIncomingCallRequest. This interface contains methods
 *  to get info about the call request, has a method to answer or reject the call.
 */
struct IAnswerCall
{
    virtual int answer(bool accept, const AvFlags& answerAv,
                       const char* reason, const char* text) = 0;
    virtual const char** files() const = 0;
    virtual const AvFlags& peerAv() const = 0;
    virtual bool reqStillValid() const = 0;
    virtual void destroy() { delete this; }
private:
    virtual ~IAnswerCall() {} //deleted via destoy() only to use our memory manager
};

/** Public event handler callback interface.
 *  All events from the Rtc module are sent to the application via this interface */
struct IEventHandler
{
//TODO: Define
};
//===
/** Interface of the RtcModule */
class IRtcModule
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

virtual int startMediaCall(char* sidOut, const char* targetJid, const AvFlags& av, const char* files[]=NULL,
                      const char* myJid=NULL);
virtual bool hangupBySid(const char* sid, char callType, const char* reason, const char* text);
virtual bool hangupByPeer(const char* peerJid, char callType, const char* reason, const char* text);
virtual int hangupAll(const char* reason, const char* text);
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
virtual int muteUnmute(bool state, const AvFlags& what, const char* jid);
virtual int getSentAvBySid(const char* sid, AvFlags& av);
virtual int getSentAvByJid(const char* jid, AvFlags& av);
/**
    Get info whether remote audio and video are being received at the moment in a call to the specified JID
    @param fullJid The full peer JID to identify the call
    @param av Returns the audio/video flags
    @returns 0 on success or an RTCM error code on fail
 */
virtual int getReceivedAvBySid(const char* sid, AvFlags& av);
/** Similar to \cgetSentAvByJid, but the call is identified by the full jis of the peer */
virtual int getReceivedAvByJid(const char* jid, AvFlags& av);
virtual IJingleSession* getSessionByJid(const char* fullJid, char type='m');
virtual IJingleSession* getSessionBySid(const char* sid);
/**
  Updates the ICE servers that will be used in the next call.
  @param iceServers An array of ice server objects - same as the iceServers parameter in
          the RtcSession constructor
*/
/** url:xxx, user:xxx, pass:xxx; url:xxx, user:xxx... */
virtual int updateIceServers(const char* iceServers);
/** Returns whether the call or file transfer with the given
    sessionId is being relaid via a TURN server or not.
      @param sid The session id of the call
      @returns 1 if the call/transfer is being relayed, 0 if not, negative RTCM error code if
 there was an error or the status is unknown (not established yet or no statistics available)
*/
virtual int isRelay(const char* sid);
};

