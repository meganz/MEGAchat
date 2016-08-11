#ifndef IRTC_MODULE_H
#define IRTC_MODULE_H
/**
 * @file IRtcModule.h
 * @brief Public interface of the webrtc module
 *
 * (c) 2013-2015 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */
#include "mstrophepp.h" //only needed for IPlugin
#include "karereCommon.h" //for AvFlags
#ifdef _WIN32
    #ifdef RTCM_BUILDING
        #define RTCM_API __declspec(dllexport)
    #else
        #define RTCM_API __declspec(dllimport)
    #endif
#else
    #define RTCM_API __attribute__((visibility("default")))
#endif

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

//forward declarations
//class Call;
typedef unsigned char TermCode;
typedef enum
{
    kCallStateOutReq = 0,
    kCallStateInReq,
    kCallStateInReqWaitJingle,
    kCallStateSession,
    kCallStateEnded
} CallState;

typedef karere::AvFlags AvFlags;
typedef std::function<bool(bool, AvFlags)> CallAnswerFunc;
namespace stats
{
class IRtcStats;
struct Options;
}
class IVideoRenderer;
class IEventHandler;
class ICryptoFunctions;
class IRtcModule;
class RtcModule;

/** Length of Jingle session id strings */
enum {
    RTCM_SESSIONID_LEN = 16
};

/** @brief A class represending a media call. The protected members are internal
 stuff, and are here only for performance reasons, to not have virtual methods
 for every property
*/
class ICall
{
///@cond PRIVATE
protected:
    RtcModule& mRtc;
    bool mIsCaller;
    CallState mState;
    IEventHandler* mHandler;
    std::string mSid;
    std::string mOwnJid; //may be chatroom-specific
    std::string mPeerJid;
    std::string mPeerAnonId;
    bool mIsFileTransfer;
    bool mHasReceivedMedia = false;
    ICall(RtcModule& aRtc, bool isCaller, CallState aState, IEventHandler* aHandler,
         const std::string& aSid, const std::string& aPeerJid, bool aIsFt,
         const std::string& aOwnJid)
    : mRtc(aRtc), mIsCaller(isCaller), mState(aState), mHandler(aHandler), mSid(aSid),
      mOwnJid(aOwnJid), mPeerJid(aPeerJid), mIsFileTransfer(aIsFt) {}
///@endcond
public:
    //TermCode (call termination reason) codes
enum
{
    kNormalHangupFirst = 0, ///< First enum specifying a normal call termination
    kUserHangup = 0, ///< Normal user hangup
    kCallReqCancel = 1, ///< Call request was canceled before call was answered
    kAnsweredElsewhere = 2, ///< Call was answered on another device of ours
    kRejectedElsewhere = 3, ///< Call was rejected on another device of ours
    kAnswerTimeout = 4, ///< Call was not answered in a timely manner
    kNormalHangupLast = 4, ///< Last enum specifying a normal call termination
    kReserved1 = 5,
    kReserved2 = 6,
    kErrorFirst = 7, ///< First enum specifying call termination due to error
    kInitiateTimeout = 7, ///< The calling side did not initiate the Jingle session in a timely manner after the call received accepted the call
    kApiTimeout = 8, ///< Mega API timed out on some request (usually for RSA keys)
    kFprVerifFail = 9, ///< Peer DTLS-SRTP fingerprint verification failed, posible MiTM attack
    kProtoTimeout = 10, ///< Protocol timeout - one if the peers did not send something that was expected, in a timely manner
    kProtoError = 11, ///< General protocol error
    kInternalError = 12, ///< Internal error in the client
    kNoMediaError = 13, ///< There is no media to be exchanged - both sides don't have audio/video to send
    kXmppDisconnError = 14, ///< XMPP client was disconnected
    kErrorLast = 14, ///< Last enum indicating call termination due to error
    kTermLast = 14, ///< Last call terminate enum value
    kPeer = 128 ///< If this flag is set, the condition specified by the code happened at the peer, not at our side
};
    const std::string& sid() const { return mSid; } ///< The jingle session id of the call

    RtcModule& rtc() const { return mRtc; }
    CallState state() const { return mState; } ///< The call state
    bool isCaller() const { return mIsCaller; } ///< Whether we initiated the call
    const std::string& peerJid() const { return mPeerJid; }
    const std::string& peerAnonId() const { return mPeerAnonId; }
    virtual const std::string& ownAnonId() const;
    /// @cond PRIVATE
    RTCM_API static const char* termcodeToMsg(TermCode event);
    RTCM_API static std::string termcodeToReason(TermCode event);
    RTCM_API static TermCode strToTermcode(std::string event);
    ///@endcond
    /** The current call event handler */
    IEventHandler& handler() const { return *mHandler; }
    /** Hangs up the call.
     * @param text An optional textual reason for hanging up the call. This reason
     * is sent to the peer. Normally not used.
     */
    virtual bool hangup(const std::string& text="") = 0;
    /** Mutes/unmutes the spefified channel (audio and/or video) */
    virtual void muteUnmute(AvFlags what, bool state) = 0;
    /** Changes the event handler that receives call events.
     * This may be necessary when the app initially displays a call answer gui,
     * and after the call is established, displays a call GUI that should
     * receive further events about the call
     */
    RTCM_API IEventHandler* changeEventHandler(IEventHandler* handler);
    /** Changes the instance that received local video stream. This may be
     * convenient for example if the app displays local video in a different GUI
     * before the call is answered and after that
     */
    virtual void changeLocalRenderer(IVideoRenderer* renderer) = 0;
    /** Returns \c true if we have already received media packets from the peer,
     * \c false otherwise.
     */
    bool hasReceivedMedia() const { return mHasReceivedMedia; }
    /** Specifies what streams we currently send - audio and/or video */
    virtual AvFlags sentAv() const = 0;
    /** Specifies what streams we currently (should) receive - audio and/or video.
      * Note that this does not mean that we are actually receiving them, but only that
      * the peer has declared that it will send them
      */
    virtual AvFlags receivedAv() const = 0;///<whether we receive audio and/or video
};

/**
 * Call event handler callback interface. When the call is created, the user
 * provides this interface to receive further events from the call.
*/
class IEventHandler
{
public:
/**
* Fired when there was an error obtaining local audio and/or video streams
* @param errMsg The error message
* @param cont In some situations it may be possible to continue and establish
* a call even if there was an error obtaining the local media stream. For example
* if we still want to receive the peer's audio and video (and not send ours). In
* such cases the \c cont parameter is not NULL and points to a boolean,
* set to true by default. If you want to not continue with establishing the call,
* set it to \c false to terminate the call.
*/
virtual void onLocalMediaFail(const std::string& errMsg, bool* cont) {}
/**
* @brief Fired when an _outgoing_ call has just been created, via
* \c startMediaCall(). This is a chance for the app to associate the call object
* with the \c rtcm::IEventHandler interface that it provided.
* @param call The Call object for which events will be received on this handler
*/
virtual void onOutgoingCallCreated(const std::shared_ptr<ICall>& call) {}
/**
* @brief A call requested by us was declined by the remote
* @param fullPeerJid The full jid of the peer endpoint that declined the call
* @param reason - The one-word reason for the decline, as specified by the remote
* @param text - Verbose message why the call was declined. May be NULL
* @param isDataCall - denotes if the call is media or data call
*/

/** Called when a session for the call has just been created */
    virtual void onSession() {}

/** Called when out outgoing call was answered
 * @param peerJid - the full jid of the answerer
 */
virtual void onCallAnswered(const std::string& peerFullJid) {}
/**
* @brief Fired when the local audio/video stream has just been obtained.
* @param [out] localVidRenderer The API user must return an IVideoRenderer
* instance to be fed the video frames of the local video stream. The implementation
* of this interface usually renders the frames in the application's GUI.
*/
    virtual void onLocalStreamObtained(IVideoRenderer*& localVidRenderer) {}
/**
* @brief Fired just before a remote stream goes away, to remove
* the video player from the GUI.
*/
    virtual void removeRemotePlayer() {}
/**
* @brief Fired when remote media/data packets start actually being received.
* This is the event that denotes a successful establishment of the incoming
* stream flow. All successful handshake events before that mean nothing
* if this one does not occur!
* @param sess The session on which stream started being received.
* @param statOptions Options that control how statistics will be collected
* for this call session.
*/
    virtual void onMediaRecv(stats::Options& statOptions) {}
/**
* @brief A call is about to be deleted. The call has ended either normally by user
* hangup, or because of an error, or it was never established.
* This event is fired also in case of RTP fingerprint verification failure.
* The user must release any shared_ptr references it may have to the ICall object,
* to allow the call object to be actually deleted and memory re-claimed.
* It is guaranteed that this event handler will not be called anymore, so it's safe
* to delete it as well, even from within this method.
* @param reason The reason for which the call is terminated.
* If the peer terminated the call, the reason will have the ICall::kPeer bit set.
* @param text A more detailed description of the reason, or \c nullptr. Can be an error
* message. For fingerprint verification failure it is "fingerprint verification failed"
* @param stats A stats object thet contains full or basic statistics for the call.
* Reference to the stats object can be kept, its lifetime is not associated in any
* way with the lifetime of the call object.
*/
    virtual void onCallEnded(TermCode termcode, const std::string& text,
                             const std::shared_ptr<stats::IRtcStats>& stats) {}
/**
* @brief Fired when the media and codec description of the remote side
* has been received and parsed by webRTC. At this point it is known exactly
* what type of media, including codecs etc. the peer will send, and the stack
* is prepared to handle it.
* @param rendererRet The application must return an IVideoRenderer via this parameter.
* This instance will receive decoded video frames from the remote peer.
* The renderer is required even if the remote is does not send video initially,
* as it may enable video sending during the call.
*/
    virtual void onRemoteSdpRecv(IVideoRenderer*& rendererRet) {}
    /**
     * @brief The remote has muted audio and/or video sending. If video was muted,
     * it is guaranteed that the video renderer will not receive any frames until
     * \c onPeerUnmute() for video is received. This allows the app to draw an avatar
     * of the peer on the viewport.
     * @param what Specifies what was muted
     */
    virtual void onPeerMute(AvFlags what) {}
    /**
     * @brief The remote has unmuted audio and/or video
     * @param what Specifies what was unmuted
     */
    virtual void onPeerUnmute(AvFlags what) {}
};

/** @brief The interface to answer or reject a call */
class ICallAnswer
{
public:
    /** The call object */
    virtual std::shared_ptr<ICall> call() const = 0;
 /**This method shows whether the call can still be answered or rejected, i.e.
    The call may have already been canceled */
    virtual bool reqStillValid() const = 0;
 /** The list of files, in case of a data call */
    virtual std::set<std::string>* files() const = 0;
    /** What media the caller will send to us initially. This determined whether
     * this is an audio or video call. Note that for example an audio call can
     * subsequently add video */
    virtual AvFlags peerMedia() const = 0;
    /** Answer or reject the call.
     * @param accept - if true, answers the call, if false - rejects it
     * @param ownMedia - when answering the call, specified whether we send
     * audio and/or video */
    virtual bool answer(bool accept, AvFlags ownMedia) = 0;
};

class IGlobalEventHandler
{
public:
/**
* @brief Fired when an incoming call request is received.
* @param call The call object
* @param ans The function that the user should call to answer or reject the call
* @param reqStillValid Can be called at any time to determine if the call request
* is still valid
* @param peerMedia Flags showing whether the caller has audio and video enabled for
* the call
* @param files Will be non-empty if this is a file transfer call.
* Call methods of this object to answer/decline the call, check if the request
* is still valid, or examine details about the call request.
* @returns The user should return an event handler that will handle any further events
* on that call, even if the call is not answered. This can be initially set to an
* 'incoming call' GUI and if the call is actually answered, reset to an actual call
* event handler.
*/
    virtual IEventHandler* onIncomingCallRequest(const std::shared_ptr<ICallAnswer>& ans) = 0;
/**
* @brief discoAddFeature Called when rtcModule wants to add a disco feature
* to the xmpp client. The implementation should normally have instantiated
* the disco plugin on the connection, and this method should forward the call
* to the addFeature() method of the plugin. This is to abstract the disco
* interface, so that the rtcModule does not care how disco is implemented.
* @param feature The disco feature string to add
*/
    virtual void discoAddFeature(const char* feature) {}
};

class IRtcModule: public strophe::IPlugin
{
public:
    virtual void getAudioInDevices(std::vector<std::string>& devices) const = 0;
    virtual void getVideoInDevices(std::vector<std::string>& devices) const = 0;

    virtual bool selectVideoInDevice(const std::string& devname) = 0;
    virtual bool selectAudioInDevice(const std::string& devname) = 0;
    virtual int muteUnmute(AvFlags what, bool state, const std::string& jid) = 0;
    virtual void startMediaCall(IEventHandler* userHandler, const std::string& targetJid,
        AvFlags av, const char* files[]=nullptr, const std::string& myJid="") = 0;
    virtual void hangupAll(TermCode code = ICall::kUserHangup, const std::string& text="") = 0;
    virtual std::shared_ptr<ICall> getCallBySid(const std::string& sid) = 0;
    virtual ~IRtcModule(){}
};

RTCM_API IRtcModule* create(xmpp_conn_t* conn, IGlobalEventHandler* handler,
                  ICryptoFunctions* crypto, const char* iceServers);
RTCM_API void globalCleanup();

}

#endif

