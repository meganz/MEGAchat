#ifndef IRTC_MODULE_H
#define IRTC_MODULE_H

#include "IJingleSession.h"
#include "IVideoRenderer.h"
#include "mstrophepp.h" //only needed for IPlugin
#include "ITypes.h"

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
struct IAnswerCall: public IDestroy
{
/** Answers or rejects an incoming call
 * @param accept Whether to accept or reject the call
 * @param answerAv What media to send when accepting the call.
 * @param reason Reason for call reject.
 * @param text Full text message for call reject reason.
 *  Could be an error message. \c NULL for none
 * */
    virtual int answer(bool accept, const AvFlags& answerAv,
                       const char* reason, const char* text) = 0;
    /**
     * @returns A null-terminated char* array of file paths names that are to received
     * in a data call. This being NULL means that the call is a media call, and
     * being non-NULL means it's a data call.
     */
    virtual const char* const* files() const = 0;
    /** The types of media (audio and/or video) that the peer sends */
    virtual const AvFlags& peerAv() const = 0;
    /**
     * Returns whether the incoming call request is still valid at this
     * specific moment. Will be false if the call was canceled by the peer
     * or timed out
    */
    virtual bool reqStillValid() const = 0;
};

struct StatOptions
{
    bool enableStats = true;
    int scanPeriod = -1;
    int maxSamplePeriod = -1;
};
struct IRtcStats {};

/**
 * Public event handler callback interface.
 * All events from the Rtc module are sent to the application
 * via this interface
*/
struct IEventHandler: public IDestroy
{
/**
* Fired when there was an error obtaining local audio and/or video streams
* @param errMsg The error message
* @param cont In some situations it may be possible to continue and establish
* a call even if there was an error obtaining the local media stream. For example
* if we still want to receive the peer's audio and video (and not send ours). In
* such cases the \c cont parameter is not NULL and points to a boolean,
* set to false by default. If you want to continue with establishing the call,
* set it to \c true to continue.
*/
    virtual void onLocalMediaFail(const char* errMsg, int* cont=nullptr) {}
/**
* @brief Fired only when we are the caller, when the call session has just been
* created.
* @param sess The session object
* @param isDataCall Denotes whether the call is a data or media call
*/
    virtual void onCallInit(IJingleSession* sess, int isDataCall) {}
/**
* @brief A call requested by us was declined by the remote
* @param fullPeerJid The full jid of the peer endpoint that declined the call
* @param sid The session id of the call
* @param reason - The one-word reason for the decline, as specified by the remote
* @param text - Verbose message why the call was declined. May be NULL
* @param isDataCall - denotes if the call is media or data call
*/
    virtual void oncallDeclined(const char* fullPeerJid, const char* sid,
      const char* reason, const char* text, int isDataCall){}
/**
* @brief Fired when the remote did not answer the call within an acceptable
* timeout
* @param peer - The JID of the peer whom we were calling. It can be
* a full or bare JID, depending what was given to startMediaCall().
*/
    virtual void onCallAnswerTimeout(const char* peer) {}
/**
* @brief Fired when the local audio/video stream has just been obtained.
* This method is _not_ called for subsequent calls if the local stream is already
* being used
* @param rendererRet The API user must return an IVideoRenderer instance to be
* fed the video frames of the local video stream. The implementation of this
* interface usually renders the frames in the application's GUI.
*/
    virtual void onLocalStreamObtained(IVideoRenderer** rendererRet) {}
/**
* @brief Fired when an incoming call request is received.
* @param ansCtrl This is an 'call answer controller' object.
* Call methods of this object to answer/decline the call, check if the request
* is still valid, or examine details about the call request.
*/
    virtual void onCallIncomingRequest(IAnswerCall* ansCtrl) {}
/**
* @brief Fired when *we* answer the call, but before the actual
*  processing of ICE candidates and RTP connection establishment.
* @param sess The newly created session
*/
    virtual void onCallAnswered(IJingleSession* sess) {}
/**
* @brief Fired just before a remote stream goes away, to remove
* the video player from the GUI.
* @param sess The session on which the remote stream goes away
* @param videoRenderer The existing video renderer instance, as provided
* by the app in onRemoteSdpRecv. The application can safely destroy that
* instance.
*/
    virtual void remotePlayerRemove(IJingleSession* sess, IVideoRenderer* videoRenderer) {}
    /**
     * @brief Fired when remote media/data packets start actually being received.
     * This is the event that denotes a successful establishment of the incoming
     * stream flow. All successful handshake events before that mean nothing
     * if this one does not occur!
     * @param sess The session on which stream started being received.
     * @param statOptions Options that control how statistics will be collected
     * for this call session.
     */
    virtual void onMediaRecv(IJingleSession* sess, StatOptions* statOptions) {}
    /**
     * @brief A call that has been accepted has ended - either normally by user
     * hangup, or because of an error. This event is fired also in case of
     * RTP fingerprint verification failure.
     * @param sess The session of the call. This session will be terminated.
     * @param reason The reason for which the call is terminated. If the
     * fingerprint verification failed, the reason will be 'security'
     * If the peer terminated the call, the reason will be prefixed by 'peer-'.
     * For example, if we hung up the call, the reason would be 'hangup',
     * but if the peer hung up (with reason 'hangup'), the reason would be 'peer-hangup'
     * @param text A more detailed description of the reason. Can be an error
     * message. For fingerprint verification failure it is "fingerprint verification failed"
     * @param stats A stats object thet contains full or basic statistics for the call.
     */
    virtual void onCallEnded(IJingleSession* sess, const char* reason, const char* text,
                             IRtcStats* stats) {}
    /**
     * @brief Fired when the media and codec description of the remote side
     * has been received and parsed by webRTC. At this point it is known exactly
     * what type of media, including codecs etc. the peer will send, and the stack
     * is prepared to handle it. If the remote is about to send video, then rendererRet
     * will be non-null, see below.
     * @param sess The call session on which the remote SDP was received.
     * @param rendererRet If the remote is about to send video, this pointer is
     * non-null and points to a pointer that should receive an IVideoRenderer
     * instance. This instance will receive decoded video frames from the remote peer.
     */
    virtual void onRemoteSdpRecv(IJingleSession* sess, IVideoRenderer** rendererRet) {}
    /**
     * @brief Fired when a Jingle error XML stanza is received.
     * @param sess The session on which the error occurred
     * @param origin A short string identifying the operation during which the error occurred.
     * @param stanza The error message or XML stanza, as string
     * @param origXml The message to which the error reply was received,
     * i.e. XML of the stanza to which the error response was received. Can be NULL.
     * @param type The type of the error. Currently always 's', meaning that the
     * error was received as an error stanza, and stanza and origXml are stanzas
     */
    virtual void onJingleError(IJingleSession* sess, const char* origin, const char* stanza,
                   const char* origXml, char type) {}
    /**
     * @brief Fired when all current calls have muted sending local video, so the local video
     * display should be stopped. Ideally in this case the camera should be stopped,
     * but currently this is not possible as Firefox does not support SDP renegotiation.
     */
    virtual void onLocalVideoDisabled() {}
    /**
     * @brief Fired when at least one call enabled sending local video, or such
     * a call was started. The GUI should enable the local video display.
     */
    virtual void onLocalVideoEnabled() {}
protected:
    /**
     * @brief Non-public destructor to avoid mixing memory managers. Destreuction
     * is done via calling IDestroy's destroy() method that this interface implements
     */
    virtual ~IEventHandler() {}
};

//===
/** Interface of the RtcModule
* As this interface has to work across a DLL boundary,
* operator delete can't be called directly becaue it would use the allocator on the side
* of the DLL boundary on which it is called. Instead, the interface has a virtual delete()
* method that calls the destructor at the implementation side of the boundary
*/
class IRtcModule: public virtual strophe::IPlugin
{
public:
    /**
    @brief Initiates a media call to the specified peer
    @param targetJid
    The JID of the callee. Can be a full jid (including resource),
    or a bare JID (without resource), in which case the call request will be broadcast
    using a special \<message\> packet. For more details on the call broadcast mechanism,
    see the Wiki
    @param av Whether to send local video/audio. Ignored if \c files is not NULL
    @param myJid
    Necessary only if doing MUC, because the user's JID in the
    room is different than her normal JID. If NULL,
    the user's 'normal' JID will be used
    @param sidOut - a pointer to a string buffer that will receive the session id of the
    call that is initiated. The size of the buffer must be at least RTCM_SESSIONID_SIZE+1 bytes
    @param files Array of null-terminated strings specifying the files that are to be sent,
    in case this is a data call. NULL if a media call is to be initiated.
    @returns 0 on success, a negative RTCM error code in failure
    */
    virtual int startMediaCall(char* sidOut, const char* targetJid, const AvFlags& av,
        const char* files[]=nullptr, const char* myJid=nullptr) = 0;
    /**
     * @brief Hangs up a call, cancels a outgoing call request or cancels
     * an already accepted but not yet established incoming call. The call is
     * identified by the session id. Note that there may not yet be a real session
     * with that sid, in case of not yet established calls.
     * @param sid The session id to identify the call
     * @param callType Filter the affected calls by type - 'f' for file/data calls,
     * 'm' for media calls, 0 for any call.
     * @param reason Specify reason for the hangup/cancel
     * @param text More detailed description of the reason. can be NULL
     * @return The number of calls that matched and were terminated
     */
    virtual int hangupBySid(const char* sid, char callType, const char* reason, const char* text) = 0;
    /**
     * @brief Similar to \c hangupBySid(), but the call is identified by the
     * peer's JID.
     * @param peerJid The JID to indentify the call(s)- can be bare or full.
     * If a bare JID is specified, all calls to trsource of that
     * bare JID will (and matching the call type) will be matched.
     * @param callType 'm' for media, 'f' for file transfer, 0 for any
     * @param reason Reason for termination
     * @param text Detailed description of reason
     * @return
     */
    virtual int hangupByPeer(const char* peerJid, char callType, const char* reason, const char* text) = 0;
    /**
     * @brief Hangs up all current calls, oputgoing call requests, accepted
     * but not yet established calls.
     * @param reason The reason for the hangup
     * @param text The detailed description/error message of the hangup reason.
    */
    virtual int hangupAll(const char* reason, const char* text) = 0;
    /**
    @brief Mutes/unmutes audio/video
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
    /**
     * @brief Obtains info what type of media (audio, video) we are sending on
     * a session specified by a sid
     * @param sid the session id that specifies the session
     * @param av The audio/video flags structure that receives the info
     * @return An error code: RTCM_ENOTFOUND if a session with the given \c sid
     * was not found, RTCM_NONE if the session is not linked to an input stream
     * (yet). Otherwise returns 0 and the AVFlags structure gets filled with the flags.
     */
    virtual int getSentAvBySid(const char* sid, AvFlags& av) = 0;
    /**
     * @brief Similar to getSentAvBySid, but the session is identified by a full Jid.
     * @param jid The full or bare Jid of the peer.
     *  \warning There could be multiple sessions that match the specified jid,
     * either if it was a bare jid, or if there are multiple sessions to the same
     * full jid. In case of multiple matches, the first will be considered.
     */
    virtual int getSentAvByJid(const char* jid, AvFlags& av) = 0;
    /**
    Get info whether remote audio and video are being received at the moment
    in a call to the specified full JID
    @param jid The full or bare peer JID to identify the call.
    \warning Beware that if more than one session match the specified Jid (either
    because it is bare, or there are multiple sessions to the same full Jid), then
    the first will be returned. If you need more un-ambiguous interface, use the
    \c  bySid version of the function.
    @param av Returns the audio/video flags
    @returns 0 on success or an RTCM error code on fail. See getSentAvBySid() for
    error codes returned
    */
    virtual int getReceivedAvByJid(const char* jid, AvFlags& av) = 0;
    /**
     * @brief Similar to \c getSentAvByJid, but
     *  the call is unambiguously identified by the session id
    */
    virtual int getReceivedAvBySid(const char* sid, AvFlags& av) = 0;
    /**
     * @brief Obtains an ISession object of a session identified by a fullJid and
     * a call type. Note that if there are several sessions that match the criteria,
     * only the first will be returned.
     * @param fullJid The full jid of the peer
     * @param type Type of the session - 'm' for media, 'f' for file transfer, 0 for any
     * @return The ISession object or NULL if not session was found that matches
     * the criteria
     */
    virtual IJingleSession* getSessionByJid(const char* fullJid, char type='m') = 0;
    /**
     * @brief Similar to getSessionByJid, but the session is identified by a
     * session id.
     */
    virtual IJingleSession* getSessionBySid(const char* sid) = 0;
    /**
    @brief Updates the ICE servers that will be used in the next call.
    @param iceServers An array of ice server objects - same as the iceServers parameter in
          the RtcSession constructor
    */
    /** url:xxx, user:xxx, pass:xxx; url:xxx, user:xxx... */
    virtual int updateIceServers(const char* iceServers) = 0;
    /**
     * @brief Returns whether the call or file transfer with the given
      sessionId is being relaid via a TURN server or not.
      @param sid The session id of the call
      @returns 1 if the call/transfer is being relayed, 0 if not, negative RTCM error code if
     there was an error or the status is unknown (not established yet or no statistics available)
    */
    virtual int isRelay(const char* sid) = 0;
    //@{
    /** @brief Get a list of all input audio or video devices detected
     * @return A list of all detected input devices in the system.
     *  You take responsibility for freeing the IDeviceList object
     */
    virtual IDeviceList* getAudioInDevices() = 0;
    virtual IDeviceList* getVideoInDevices() = 0;
    //@}
    //@{
    /**
     * @brief selectAudioInDevice
     * @param devname The name of the device, as returned by IDeviceList.name()
     * @return The index of the device in the current enumeration, or -1 if the device
     * was not found. In the latter case, the device is not changed
     */
    virtual int selectAudioInDevice(const char* devname) = 0;
    /**
     * @brief similar to selectAudioInDevice
     */
    virtual int selectVideoInDevice(const char* devname) = 0;
    //@}
};

}

#endif

