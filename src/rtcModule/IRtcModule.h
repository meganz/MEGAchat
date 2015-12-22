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


class ICall
{
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
    ICall(RtcModule& aRtc, bool isCaller, CallState aState, IEventHandler* aHandler,
         const std::string& aSid, const std::string& aPeerJid, bool aIsFt,
         const std::string& aOwnJid)
    : mRtc(aRtc), mIsCaller(isCaller), mState(aState), mHandler(aHandler), mSid(aSid),
      mOwnJid(aOwnJid), mPeerJid(aPeerJid), mIsFileTransfer(aIsFt) {}
public: //TermCode (call termination reason) codes
enum
{
    kNormalHangupFirst = 0,
    kUserHangup = 0,
    kCallReqCancel = 1,
    kAnsweredElsewhere = 2,
    kRejectedElsewhere = 3,
    kAnswerTimeout = 4,
    kNormalHangupLast = 4,
    kReserved1 = 5,
    kReserved2 = 6,
    kErrorFirst = 7,
    kInitiateTimeout = 7,
    kApiTimeout = 8,
    kFprVerifFail = 9,
    kProtoTimeout = 10,
    kProtoError = 11,
    kInternalError = 12,
    kNoMediaError = 13,
    kXmppDisconnError = 14,
    kErrorLast = 14,
    kTermLast = 14,
    kPeer = 128
};
    const std::string& sid() const { return mSid; }

    RtcModule& rtc() const { return mRtc; }
    CallState state() const { return mState; }
    bool isCaller() const { return mIsCaller; }
    static const char* termcodeToMsg(TermCode event);
    static std::string termcodeToReason(TermCode event);
    static TermCode strToTermcode(std::string event);
    IEventHandler& handler() const { return *mHandler; }
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
* set to false by default. If you want to continue with establishing the call,
* set it to \c true to continue.
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
    virtual void onSession();

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
* @param sess The session on which the remote stream goes away
* @param videoRenderer The existing video renderer instance, as provided
* by the app in onRemoteSdpRecv. The application can safely destroy that
* instance.
*/
    virtual void removeRemotePlayer(IVideoRenderer* videoRenderer) {}
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
* @brief A call that has been accepted has ended - either normally by user
* hangup, or because of an error. The JIngle session of the call will be destroyed,
* so the applicaton must not access it after this callback.
* This event is fired also in case of
* RTP fingerprint verification failure.
* @param reason The reason for which the call is terminated. If the
* fingerprint verification failed, the reason will be 'security'
* If the peer terminated the call, the reason will be prefixed by 'peer-'.
* For example, if we hung up the call, the reason would be 'hangup',
* but if the peer hung up (with reason 'hangup'), the reason would be 'peer-hangup'
* @param text A more detailed description of the reason. Can be an error
* message. For fingerprint verification failure it is "fingerprint verification failed"
* @param stats A stats object thet contains full or basic statistics for the call.
*/
    virtual void onCallEnded(TermCode termcode, const char* text,
                             const std::shared_ptr<stats::IRtcStats>& stats) {}
/**
* @brief Fired when the media and codec description of the remote side
* has been received and parsed by webRTC. At this point it is known exactly
* what type of media, including codecs etc. the peer will send, and the stack
* is prepared to handle it. If the remote is about to send video, then rendererRet
* will be non-null, see below.
* @param rendererRet If the remote is about to send video, this pointer is
* non-null and points to a pointer that should receive an IVideoRenderer
* instance. This instance will receive decoded video frames from the remote peer.
*/
    virtual void onRemoteSdpRecv(IVideoRenderer** rendererRet) {}
/**
* @brief Fired when a Jingle error XML stanza is received.
* @param origin A short string identifying the operation during which the error occurred.
* @param stanza The error message or XML stanza, as string
* @param origXml The message to which the error reply was received,
* i.e. XML of the stanza to which the error response was received. Can be NULL.
* @param type The type of the error. Currently always 's', meaning that the
* error was received as an error stanza, and stanza and origXml are stanzas
*/
    virtual void onError(const std::string& msg) {}
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

    virtual void onMute(AvFlags what);
    virtual void onUnmute(AvFlags what);
protected:
/**
* @brief Non-public destructor to avoid mixing memory managers. Destruction
* is done via calling IDestroy's destroy() method that this interface implements
*/
    virtual ~IEventHandler() {}
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
    virtual IEventHandler* onIncomingCallRequest(const std::shared_ptr<ICall>& call,
        std::shared_ptr<CallAnswerFunc>& ans,
        std::shared_ptr<std::function<bool()> >& reqStillValid,
        const AvFlags& peerMedia, std::shared_ptr<std::set<std::string> >& files) = 0;
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
    virtual bool selectVideoInDevice(const std::string& devname) = 0;
    virtual bool selectAudioInDevice(const std::string& devname) = 0;
    virtual int muteUnmute(AvFlags state, const std::string& jid) = 0;
    virtual void startMediaCall(IEventHandler* userHandler, const std::string& targetJid,
        AvFlags av, const char* files[]=nullptr, const std::string& myJid="") = 0;
    virtual void hangupAll(TermCode code = ICall::kUserHangup, const char* text=nullptr) = 0;
    virtual std::shared_ptr<ICall> getCallBySid(const std::string& sid) = 0;
    virtual ~IRtcModule(){}
};

IRtcModule* create(xmpp_conn_t* conn, IGlobalEventHandler* handler,
                  ICryptoFunctions* crypto, const char* iceServers);
void glovalCleanup();

}

#endif

