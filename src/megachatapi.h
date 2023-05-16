/**
 * @file megachatapi.h
 * @brief Public header file of the intermediate layer for the MEGA Chat C++ SDK.
 *
 * (c) 2013-2016 by Mega Limited, Auckland, New Zealand
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

#ifndef MEGACHATAPI_H
#define MEGACHATAPI_H


#include <megaapi.h>

namespace mega { class MegaApi; }

namespace megachat
{

typedef uint64_t MegaChatHandle;
typedef int64_t MegaChatTimeStamp; // unix timestamp
typedef int MegaChatIndex;  // int32_t

/**
 * @brief MEGACHAT_INVALID_HANDLE Invalid value for a handle
 *
 * This value is used to represent an invalid handle. Several MEGA objects can have
 * a handle but it will never be megachat::MEGACHAT_INVALID_HANDLE
 *
 */
const MegaChatHandle MEGACHAT_INVALID_HANDLE = ~(MegaChatHandle)0;
const MegaChatIndex MEGACHAT_INVALID_INDEX = 0x7fffffff;
const MegaChatTimeStamp MEGACHAT_INVALID_TIMESTAMP = 0;
const int MAX_MESSAGES_PER_BLOCK = 256;
const int MIN_MESSAGES_PER_BLOCK = 1;

class MegaChatApi;
class MegaChatApiImpl;
class MegaChatRequest;
class MegaChatRequestListener;
class MegaChatError;
class MegaChatMessage;
class MegaChatRoom;
class MegaChatRoomListener;
class MegaChatCall;
class MegaChatCallListener;
class MegaChatVideoListener;
class MegaChatListener;
class MegaChatNotificationListener;
class MegaChatListItem;
class MegaChatNodeHistoryListener;
class MegaChatScheduledRules;
class MegaChatScheduledFlags;
class MegaChatScheduledMeeting;
class MegaChatScheduledMeetingList;
class MegaChatScheduledMeetingOccurrList;

/**
 * @brief Provide information about a session
 *
 * A session is an object that represents a client that is active in a call which I am participating.
 * A call contains none or several sessions and it can be obtained with MegaChatCall::getMegaChatSession.
 * MegaChatCall has the ownership of the object. Session updates are notified by
 * MegaChatCallListener::onChatSessionUpdate
 *
 * The states that a session has during its life time are:
 * Outgoing call:
 *  - SESSION_STATUS_INVALID = 0xFF
 *  - SESSION_STATUS_IN_PROGRESS = 0
 *  - SESSION_STATUS_DESTROYED = 1
 */
class MegaChatSession
{
public:
    enum
    {
        SESSION_STATUS_INVALID = 0xFF,
        SESSION_STATUS_IN_PROGRESS = 0,         /// Session is operative
        SESSION_STATUS_DESTROYED            /// Session is finished and resources can be released
    };

    enum
    {
        CHANGE_TYPE_NO_CHANGES = 0x00,              /// Session doesn't have any change
        CHANGE_TYPE_STATUS = 0x01,                  /// Session status has changed
        CHANGE_TYPE_REMOTE_AVFLAGS = 0x02,          /// Remote audio/video flags has changed
        CHANGE_TYPE_SESSION_SPEAK_REQUESTED = 0x04, /// Session speak requested
        CHANGE_TYPE_SESSION_ON_LOWRES = 0x08,       /// Low-Res video received
        CHANGE_TYPE_SESSION_ON_HIRES = 0x10,        /// Hi-Res video received
        CHANGE_TYPE_SESSION_ON_HOLD = 0x20,         /// Session is on hold
        CHANGE_TYPE_AUDIO_LEVEL = 0x40,             /// Indicates if peer is speaking
        CHANGE_TYPE_PERMISSIONS = 0x80,             /// Indicates that peer moderator role status has changed
    };

    enum {
        SESS_TERM_CODE_INVALID          = -1,   // Session has been finished by an invalid reason
        SESS_TERM_CODE_RECOVERABLE      = 0,    // Session has been finished by a recoverable reason
        SESS_TERM_CODE_NON_RECOVERABLE  = 1,    // Session has been finished by a non recoverable reason
    };

    virtual ~MegaChatSession();

    /**
     * @brief Creates a copy of this MegaChatSession object
     *
     * The resulting object is fully independent of the source MegaChatSession,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatSession object
     */
    virtual MegaChatSession *copy();

    /**
     * @brief Returns the status of the session
     *
     * @return the session status
     * Valid values are:
     *  - SESSION_STATUS_IN_PROGRESS = 0
     *  - SESSION_STATUS_DESTROYED = 1
     */
    virtual int getStatus() const;

    /**
     * @brief Returns the MegaChatHandle with the peer id.
     *
     * @return MegaChatHandle of the peer.
     */
    virtual MegaChatHandle getPeerid() const;

    /**
     * @brief Returns the MegaChatHandle with the id of the client.
     *
     * @return MegaChatHandle of the client.
     */
    virtual MegaChatHandle getClientid() const;

    /**
     * @brief Returns audio state for the session
     *
     * @return true if audio is enable, false if audio is disable
     */
    virtual bool hasAudio() const;

    /**
     * @brief Returns video state for the session
     *
     * This method returns if peer associated to this session is sending video (it doesn't means that we have requested)
     * This method doesn't provide information about video quality (high resolution or low resolution) nor video source (camera or screen share).
     *
     * The following methods returns more specific information about video state for this session
     *  - MegaChatSession::isHiResVideo:
     *          peer associated to this session is sending hi-res video (camera or screen share)
     *
     *  - MegaChatSession::isLowResVideo:
     *          peer associated to this session is sending low-res video (camera or screen share)
     *
     *  - MegaChatSession::hasCamera():
     *          peer associated to this session is sending video from camera (low resolution or high resolution)
     *
     *  - MegaChatSession::isLowResCamera():
     *          peer associated to this session is sending video from camera in low resolution
     *
     *  - MegaChatSession::isHiResCamera():
     *          peer associated to this session is sending video from camera in high resolution
     *
     *  - MegaChatSession::hasScreenShare():
     *          peer associated to this session is sending video from screen share (low resolution or high resolution)
     *
     *  - MegaChatSession::isHiResScreenShare():
     *          peer associated to this session is sending video from screen share in high resolution
     *
     *  - MegaChatSession::isLowResScreenShare():
     *          peer associated to this session is sending video from screen share in low resolution
     *
     *
     * @return true if video is enable, false if video is disabled
     */
    virtual bool hasVideo() const;

    /**
     * @brief Returns true if peer associated to this session is sending video (camera or screen share) in high resolution
     *
     * @note Indicate if peer is sending high resolution video (camera or screen share) at this moment.
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video (camera or screen share) in high resolution
     */
    virtual bool isHiResVideo() const;

    /**
     * @brief Returns true if peer associated to this session is sending video (camera or screen share) in low resolution
     *
     * @note Indicate if peer is sending low resolution video (camera or screen share) at this moment.
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video (camera or screen share) in low resolution
     */
    virtual bool isLowResVideo() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from camera (low or high resolution)
     *
     * @note Indicate if peer is sending video from camera (low or high resolution)
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video from camera (low or high resolution)
     */
    virtual bool hasCamera() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from camera in low resolution
     *
     * @note Indicate if peer is sending video from camera in low resolution
     * We can configure the session for receive video but peer is not sending yet
     *
     * @note in case peer associated to this session is sending camera and screen share simultaneously,
     * we will receive camera on low resolution track, and screen share in high resolution track
     *
     * @return true if peer associated to this session is sending video from camera in low resolution
     */
    virtual bool isLowResCamera() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from camera in high resolution
     *
     * @note Indicate if peer is sending video from camera in high resolution
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video from camera in high resolution
     */
    virtual bool isHiResCamera() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from screen share (low or high resolution)
     *
     * @note Indicate if peer is sending video from screen share (low or high resolution)
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video from screen share (low or high resolution)
     */
    virtual bool hasScreenShare() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from screen share in high resolution
     *
     * @note Indicate if peer is sending video from screen share in high resolution
     * We can configure the session for receive video but peer is not sending yet
     *
     * @note in case peer associated to this session is sending camera and screen share simultaneously,
     * we will receive camera on low resolution track, and screen share in high resolution track
     *
     * @return true if peer associated to this session is sending video from screen share in high resolution
     */
    virtual bool isHiResScreenShare() const;

    /**
     * @brief Returns true if peer associated to this session is sending video from screen share in low resolution
     *
     * @note Indicate if peer is sending video from screen share in low resolution
     * We can configure the session for receive video but peer is not sending yet
     *
     * @return true if peer associated to this session is sending video from screen share in low resolution
     */
    virtual bool isLowResScreenShare() const;

    /**
     * @brief Returns if session is on hold
     *
     * @return true if session is on hold
     */
    virtual bool isOnHold() const;

    /**
     * @brief Returns a bit field with the changes of the session
     *
     * This value is only useful for session notified by MegaChatCallListener::onChatSessionUpdate
     * that can notify about session modifications. The value only will be valid inside
     * MegaChatCallListener::onChatSessionUpdate. A copy of MegaChatSession will be necessary to use
     * outside this callback.
     *
     * @return The returned value is an OR combination of these flags:
     *
     *  - MegaChatSession::CHANGE_TYPE_STATUS = 0x01
     * Check if the status of the session changed
     *
     *  - MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS = 0x02
     * Check MegaChatSession::hasAudio() and MegaChatSession::hasVideo() value
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_SPEAK_REQUESTED = 0x04
     * Check MegaChatSession::hasRequestSpeak
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_ON_LOWRES = 0x08
     * Check MegaChatSession::canRecvVideoLowRes
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_ON_HIRES = 0x10
     * Check MegaChatSession::canRecvVideoHiRes
     *
     * - MegaChatSession::CHANGE_TYPE_SESSION_ON_HOLD = 0x20
     * Check MegaChatSession::isOnHold
     *
     * - MegaChatSession::CHANGE_TYPE_AUDIO_LEVEL = 0x40
     * Check MegaChatSession::isAudioDetected
     *
     * - MegaChatSession::CHANGE_TYPE_PERMISSIONS = 0x80
     * Check MegaChatSession::isModerator
     */
    virtual int getChanges() const;

    /**
     * @brief Returns session termCode
     *
     * The value returned by this method will be only valid when MegaChatSession::hasChanged(MegaChatSession::CHANGE_TYPE_STATUS)
     * is true, and MegaChatSession::getStatus is MegaChatSession::SESSION_STATUS_DESTROYED.
     *
     * Posible returned values by this method:
     *  - MegaChatSession::SESS_TERM_CODE_INVALID           = -1
     *  - MegaChatSession::SESS_TERM_CODE_RECOVERABLE       = 0
     *  - MegaChatSession::SESS_TERM_CODE_NON_RECOVERABLE   = 1
     *
     * If returned value is SESS_TERM_CODE_RECOVERABLE it means that session ended by a recoverable reason, and the peer
     * represented by that session is probably trying to reconnect to the Meeting. In case that value is
     * SESS_TERM_CODE_NON_RECOVERABLE, we can asume that session has ended, and peer won't try to reconnect automatically.
     *
     * @return session termCode
     */
    virtual int getTermCode() const;

    /**
     * @brief Returns true if this session has an specific change
     *
     * This value is only useful for session notified by MegaChatCallListener::onChatSessionUpdate
     * that can notify about session modifications. The value only will be valid inside
     * MegaChatCallListener::onChatSessionUpdate. A copy of MegaChatSession will be necessary to use
     * outside this callback.
     *
     * In other cases, the return value of this function will be always false.
     *
     * @param changeType The type of change to check. It can be one of the following values:
     *
     *  - MegaChatSession::CHANGE_TYPE_STATUS = 0x01
     * Check if the status of the session changed
     *
     *  - MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS = 0x02
     * Check MegaChatSession::hasAudio() and MegaChatSession::hasVideo() value
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_SPEAK_REQUESTED = 0x04
     * Check MegaChatSession::hasRequestSpeak
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_ON_LOWRES = 0x08
     * Check MegaChatSession::canRecvVideoLowRes
     *
     *  - MegaChatSession::CHANGE_TYPE_SESSION_ON_HIRES = 0x10
     * Check MegaChatSession::canRecvVideoHiRes
     *
     * - MegaChatSession::CHANGE_TYPE_SESSION_ON_HOLD = 0x20
     * Check MegaChatSession::isOnHold
     *
     * - MegaChatSession::CHANGE_TYPE_AUDIO_LEVEL = 0x40
     * Check MegaChatSession::isAudioDetected
     *
     * - MegaChatSession::CHANGE_TYPE_PERMISSIONS = 0x80
     * Check MegaChatSession::isModerator
     *
     * @return true if this session has an specific change
     */
    virtual bool hasChanged(int changeType) const;

    /**
     * @brief Returns if peer has request speak
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @return true if has request speak
     */
    virtual bool hasRequestSpeak() const;

    /**
     * @brief Returns if audio is detected for this session
     *
     * @return true if audio is detected for this session, false in other case
     */
    virtual bool isAudioDetected() const;

    /**
     * @brief Returns if our client is ready to receive high resolution video from the participant of this session
     *
     * @note If this method returns true doesn't mean that we receive video in high resolution, maybe
     * client has their video disable but we are ready to receive it
     *
     * @return true if we are ready to receive video in high resolution
     */
    virtual bool canRecvVideoHiRes() const;

    /**
     * @brief Returns if our client is ready to receive low resolution video from the participant of this session
     *
     * @note If this method returns true doesn't mean that we receive video in low resolution, maybe
     * client has their video disable but we are ready to receive it
     *
     * @return true if we are ready to receive video in low resolution
     */
    virtual bool canRecvVideoLowRes() const;

    /**
     * @brief Returns if peer associated to the session, has moderator role in the call
     *
     * Participants with moderator role can:
     *  - End groupal calls for all participants
     *  - Approve/reject speaker requests
     *
     * @return True if peer associated to the session, has moderator role in the call
     */

    virtual bool isModerator() const;

    /**
     * @brief Returns session av flags in a readable format
     *
     * You take the ownership of the returned value.
     *
     * @return session av flags in a readable format
     */
    virtual char* avFlagsToString() const;
};

/**
 * @brief Provide information about a call
 *
 * A call can be obtained with the callback MegaChatCallListener::onChatCallUpdate where MegaChatApi has
 * the ownership of the object. Or by a getter where a copy is provided, MegaChatApi::getChatCall
 * and MegaChatApi::getChatCallByCallId
 *
 * The states that a call has during its life time are:
 *  - CALL_STATUS_INITIAL
 *  - CALL_STATUS_USER_NO_PRESENT
 *  - CALL_STATUS_CONNECTING
 *  - CALL_STATUS_JOINING
 *  - CALL_STATUS_IN_PROGRESS
 *  - CALL_STATUS_TERMINATING_USER_PARTICIPATION
 *  - CALL_STATUS_DESTROYED
 */
class MegaChatCall
{
public:
    enum
    {
        CALL_STATUS_INITIAL = 0,                        /// Initial state
        CALL_STATUS_USER_NO_PRESENT,                    /// User is no present in the call or you haven't answered the call yet
        CALL_STATUS_CONNECTING,                         /// Intermediate state, while connection sfu is established
        CALL_STATUS_JOINING,                            /// In this state configure connection with SFU
        CALL_STATUS_IN_PROGRESS,                        /// Call is established and there is a full communication with SFU
        CALL_STATUS_TERMINATING_USER_PARTICIPATION,     /// User go out from call, but the call is active in other users
        CALL_STATUS_DESTROYED,                          /// Call is finished and resources can be released
    };

    enum
    {
        CHANGE_TYPE_NO_CHANGES = 0x00,              /// Call doesn't have any change
        CHANGE_TYPE_STATUS = 0x01,                  /// Call status has changed
        CHANGE_TYPE_LOCAL_AVFLAGS = 0x02,           /// Local audio/video flags has changed
        CHANGE_TYPE_RINGING_STATUS = 0x04,          /// Peer has changed its ringing state
        CHANGE_TYPE_CALL_COMPOSITION = 0x08,        /// Call composition has changed (User added or removed from call)
        CHANGE_TYPE_CALL_ON_HOLD = 0x10,            /// Call is set onHold
        CHANGE_TYPE_CALL_SPEAK = 0x20,              /// Speak has been enabled
        CHANGE_TYPE_AUDIO_LEVEL = 0x40,             /// Indicates if we are speaking
        CHANGE_TYPE_NETWORK_QUALITY = 0x80,         /// Network quality has changed
        CHANGE_TYPE_OUTGOING_RINGING_STOP = 0x100,  /// Call (1on1) outgoing ringing has stopped (only valid if our own client has started the call)
        CHANGE_TYPE_OWN_PERMISSIONS = 0x200,        /// Indicates that own peer moderator role status has changed
        CHANGE_TYPE_GENERIC_NOTIFICATION = 0x400,   /// Generic notification
    };

    enum
    {
        NOTIFICATION_TYPE_INVALID   = 0,            /// Invalid notification type
        NOTIFICATION_TYPE_SFU_ERROR = 1,            /// Error received from SFU
        NOTIFICATION_TYPE_SFU_DENY  = 2,            /// Command denied by SFU
    };

    enum
    {
        CALL_QUALITY_HIGH_DEF = 0,
        CALL_QUALITY_HIGH_MEDIUM = 1,
        CALL_QUALITY_HIGH_LOW = 2,
    };

    enum
    {
        NETWORK_QUALITY_BAD  = 0,            // Bad network quality detected
        NETWORK_QUALITY_GOOD = 1,            // Good network quality detected
    };

    enum {
        AUDIO = 0,
        VIDEO = 1,
        ANY_FLAG = 2
    };

    enum {
        PEER_REMOVED = -1,
        NO_COMPOSITION_CHANGE = 0,
        PEER_ADDED = 1,
    };

    enum {
        TERM_CODE_INVALID                   = -1,   // This value is returned while call is in states < CALL_STATUS_IN_PROGRESS
        TERM_CODE_HANGUP                    = 0,    // Call has been finished by user
        TERM_CODE_TOO_MANY_PARTICIPANTS     = 1,    // No possible to join the call, too many participants
        TERM_CODE_REJECT                    = 2,    // Caller has hang up the call before nobody answered the call
        TERM_CODE_ERROR                     = 3,    // Call error has been received
        TERM_CODE_NO_PARTICIPATE            = 4,    // User has been removed from chatroom
        TERM_CODE_TOO_MANY_CLIENTS          = 5,    // Too many clients of same user connected
    };

    enum
    {
        END_CALL_REASON_INVALID         = -1,    /// Invalid endcall reason (it can be ignored)
        END_CALL_REASON_ENDED           = 1,     /// Call finished normally
        END_CALL_REASON_REJECTED        = 2,     /// Call was rejected by callee
        END_CALL_REASON_NO_ANSWER       = 3,     /// Call wasn't answered
        END_CALL_REASON_FAILED          = 4,     /// Call finished by an error
        END_CALL_REASON_CANCELLED       = 5,     /// Call was canceled by caller.
        END_CALL_REASON_BY_MODERATOR    = 6      /// group or meeting call has been ended by moderator
    };

    enum
    {
        SFU_DENY_INVALID                  = -1,   // Invalid command
        SFU_DENY_AUDIO                    = 0,    // Av command denied by SFU (enable/disable audio video)
        SFU_DENY_JOIN                     = 1,    // JOIN command denied by SFU
    };

    virtual ~MegaChatCall();

    /**
     * @brief Creates a copy of this MegaChatCall object
     *
     * The resulting object is fully independent of the source MegaChatCall,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatCall object
     */
    virtual MegaChatCall *copy();

    /**
     * @brief Returns a readable string that represents a termcode
     *
     * @return A readable string that represents a termcode
     */
    static const char* termcodeToString(int termcode);

    /**
     * @brief Returns the status of the call
     *
     * @return the call status
     * Valid values are:
     *  - CALL_STATUS_INITIAL = 0
     *  - CALL_STATUS_USER_NO_PRESENT = 1
     *  - CALL_STATUS_CONNECTING = 2
     *  - CALL_STATUS_JOINING = 3
     *  - CALL_STATUS_IN_PROGRESS = 4
     *  - CALL_STATUS_TERMINATING_USER_PARTICIPATION = 5
     *  - CALL_STATUS_DESTROYED = 6
     */
    virtual int getStatus() const;

    /**
     * @brief Returns the MegaChatHandle of the chat. Every call is asociated to a chatroom
     *
     * @return MegaChatHandle of the chat.
     */
    virtual MegaChatHandle getChatid() const;

    /**
     * @brief Returns the call identifier
     *
     * @return MegaChatHandle of the call.
     */
    virtual MegaChatHandle getCallId() const;

    /**
     * @brief Return audio state for local
     *
     * @return true if audio is enable, false if audio is disable
     */
    virtual bool hasLocalAudio() const;

    /**
     * @brief Return video state for local
     *
     * @return true if video is enable, false if video is disable
     */
    virtual bool hasLocalVideo() const;

    /**
     * @brief Returns a bit field with the changes of the call
     *
     * This value is only useful for calls notified by MegaChatCallListener::onChatCallUpdate
     * that can notify about call modifications. The value only will be valid inside
     * MegaChatCallListener::onChatCallUpdate. A copy of MegaChatCall will be necessary to use
     * outside this callback.
     *
     * @return The returned value is an OR combination of these flags:
     *
     * - MegaChatCall::CHANGE_TYPE_STATUS   = 0x01
     * Check if the status of the call changed
     *
     * - MegaChatCall::CHANGE_TYPE_AVFLAGS  = 0x02
     * Check MegaChatCall::hasAudio() and MegaChatCall::hasVideo() value
     *
     * - MegaChatCall::CHANGE_TYPE_RINGING_STATUS = 0x04
     * Check MegaChatCall::isRinging() value
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION = 0x08
     * @see MegaChatCall::getClientidCallCompositionChange value
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_ON_HOLD = 0x10
     * Check MegaChatCall::isOnHold()
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_SPEAK = 0x20
     * Check MegaChatCall::isAllowSpeak()
     *
     * - MegaChatCall::CHANGE_TYPE_NETWORK_QUALITY = 0x80
     * Check MegaChatCall::getNetworkQuality()
     *
     * CHANGE_TYPE_OUTGOING_RINGING_STOP = 0x100
     * Call outgoing ringing has stopped (only valid if our own client has started the call)
     *
     * - MegaChatCall::CHANGE_TYPE_OWN_PERMISSIONS = 0x200
     * Own peer moderator role status has changed (Check MegaChatCall::isOwnModerator)
     */
    virtual int getChanges() const;

    /**
     * @brief Returns true if this call has an specific change
     *
     * This value is only useful for call notified by MegaChatCallListener::onChatCallUpdate
     * that can notify about call modifications. The value only will be valid inside
     * MegaChatCallListener::onChatCallUpdate. A copy of MegaChatCall will be necessary to use
     * outside this callback
     *
     * In other cases, the return value of this function will be always false.
     *
     * @param changeType The type of change to check. It can be one of the following values:
     *
     * - MegaChatCall::CHANGE_TYPE_STATUS   = 0x01
     * Check if the status of the call changed
     *
     * - MegaChatCall::CHANGE_TYPE_AVFLAGS  = 0x02
     * Check MegaChatCall::hasAudio() and MegaChatCall::hasVideo() value
     *
     * - MegaChatCall::CHANGE_TYPE_RINGING_STATUS = 0x04
     * Check MegaChatCall::isRinging() value
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION = 0x08
     * @see MegaChatCall::getClientidCallCompositionChange value
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_ON_HOLD = 0x10
     * Check MegaChatCall::isOnHold()
     *
     * - MegaChatCall::CHANGE_TYPE_CALL_SPEAK = 0x20
     * Check MegaChatCall::isAllowSpeak()
     *
     * - MegaChatCall::CHANGE_TYPE_NETWORK_QUALITY = 0x80
     * Check MegaChatCall::getNetworkQuality()
     *
     * CHANGE_TYPE_OUTGOING_RINGING_STOP = 0x100
     * Call outgoing ringing has stopped (only valid if our own client has started the call)
     *
     * - MegaChatCall::CHANGE_TYPE_OWN_PERMISSIONS = 0x200
     * Own peer moderator role status has changed (Check MegaChatCall::isOwnModerator)
     *
     * @return true if this call has an specific change
     */
    virtual bool hasChanged(int changeType) const;

    /**
     * @brief Returns if local audio is detected
     *
     * @deprecated
     *
     * @return true if audio is detected
     */
    virtual bool isAudioDetected() const;

    /**
     * @brief Return call duration
     *
     * @note If the call is not finished yet, the returned value represents the elapsed time
     * since the beginning of the call until now.
     *
     * @return Call duration
     */
    virtual int64_t getDuration() const;

    /**
     * @brief Return the timestamp when call has started
     *
     * @return Initial timestamp
     */
    virtual int64_t getInitialTimeStamp() const;

    /**
     * @brief Return the timestamp when call has finished
     *
     * @note If call is not finished yet, it will return 0.
     *
     * @return Final timestamp or 0 if call is in progress
     */
    virtual int64_t getFinalTimeStamp() const;

    /**
     * @brief Returns an error or warning code for this call
     *
     * This method can be used for different purposes.
     *
     * If MegaChatCall::hasChanged(MegaChatCall::CHANGE_TYPE_GENERIC_NOTIFICATION) is  true and 
     * MegaChatCall::getNotificationType is equal to MegaChatCall::NOTIFICATION_TYPE_SFU_DENY,
     * this method returns the command that has been previously denied by SFU.
     * Valid values are:
     *      - SFU_DENY_AUDIO
     *      - SFU_DENY_JOIN
     *
     * If MegaChatCall::hasChanged(MegaChatCall::CHANGE_TYPE_GENERIC_NOTIFICATION) is  true and 
     * MegaChatCall::getNotificationType is equal to MegaChatCall::NOTIFICATION_TYPE_SFU_ERROR,
     * this method returns the termination code for this call due to an error notification received from SFU
     * Valid values are:
     *      - TERM_CODE_INVALID
     *      - TERM_CODE_HANGUP
     *      - TERM_CODE_TOO_MANY_PARTICIPANTS
     *      - TERM_CODE_ERROR
     *      - TERM_CODE_REJECT
     *      - TERM_CODE_NO_PARTICIPATE
     *
     * If MegaChatCall::hasChanged(MegaChatCall::CHANGE_TYPE_STATUS) is true and MegaChatCall::getStatus() ==
     * MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION, this method returns the termination code for this call
     * Valid values are:
     *      - TERM_CODE_INVALID
     *      - TERM_CODE_HANGUP
     *      - TERM_CODE_TOO_MANY_PARTICIPANTS
     *      - TERM_CODE_ERROR
     *      - TERM_CODE_REJECT
     *      - TERM_CODE_NO_PARTICIPATE
     *
     * @return error or warning code for this call
     */
    virtual int getTermCode() const;

    /**
     * @brief Returns the remote endcall reason for this call
     *
     * @note this value only will be valid in state CALL_STATUS_DESTROYED
     *
     * Valid values are:
     *  - END_CALL_REASON_INVALID       = -1,  (Invalid endcall reason, it can be ignored)
     *  - END_CALL_REASON_ENDED         = 1,   (Call finished normally)
     *  - END_CALL_REASON_REJECTED      = 2,   (Call was rejected by callee)
     *  - END_CALL_REASON_NO_ANSWER     = 3,   (Call wasn't answered)
     *  - END_CALL_REASON_FAILED        = 4,   (Call finished by an error)
     *  - END_CALL_REASON_CANCELLED     = 5    (Call was canceled by caller)
     *  - END_CALL_REASON_BY_MODERATOR  = 6    (Call was ended by moderator)
     *
     * @return endCall reason for the call
     */
    virtual int getEndCallReason() const;

    /**
     * @brief Return the notification type, when a call notification is forwarded to the apps
     *
     * @note this value only will be valid in the following scenarios:
     *      - MegaChatCall::CHANGE_TYPE_GENERIC_NOTIFICATION is notified via MegaChatCallListener::onChatCallUpdate
     *
     * Valid values returned by this method are:
     *      - MegaChatCall::NOTIFICATION_TYPE_INVALID   = 0
     *      - MegaChatCall::NOTIFICATION_TYPE_SFU_ERROR = 1     /// Error received from SFU
     *      - MegaChatCall::NOTIFICATION_TYPE_SFU_DENY  = 2     /// Notification about command denied by SFU
     *
     * @return the notification type, when a call notification is forwarded to the apps
     */
    virtual int getNotificationType() const;

    /**
     * @brief Returns the status of the remote call
     *
     * Only valid for outgoing calls. It becomes true when the receiver of the call
     * has received the call request but have not answered yet. Once the user answers or
     * rejects the call, this function returns false.
     *
     * For incoming calls, this function always returns false.
     *
     * @return True if the receiver of the call is aware of the call and is ringing, false otherwise.
     */
    virtual bool isRinging() const;

    /**
     * @brief Returns if our own user has moderator role in the call
     *
     * Participants with moderator role can:
     *  - End groupal calls for all participants
     *  - Approve/reject speaker requests
     *
     * @return True if our own user has moderator role in the call
     */
    virtual bool isOwnModerator() const;

    /**
     * @brief Get a list with the ids of client that have a session with me
     *
     * If there aren't any sessions at the call, an empty MegaHandleList will be returned.
     *
     * You take the ownership of the returned value.
     *
     * @return A list of handles with the ids of clients
     */
    virtual mega::MegaHandleList *getSessionsClientid() const;

    /**
     * @brief Returns the session for a client
     *
     * If \c clientId has not any session in the call NULL will be returned
     *
     * The MegaChatCall retains the ownership of the returned MegaChatSession. It will be only
     * valid until the MegaChatCall is deleted. If you want to save the MegaChatSession,
     * use MegaChatSession::copy
     *
     * @param clientId MegaChatHandle that identifies the clientId
     * @return Session for \c clientId
     */
    virtual MegaChatSession *getMegaChatSession(MegaChatHandle clientId);

    /**
     * @brief Returns the handle of the peer that has been added/removed to call
     *
     * This function only returns a valid value when MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION is notified
     * via MegaChatCallListener::onChatCallUpdate
     *
     * @return Handle of the peer which has been added/removed to call
     */
    virtual MegaChatHandle getPeeridCallCompositionChange() const;

    /**
     * @brief Returns if peer has been added or removed from the call
     *
     * This function only returns a valid value when MegaChatCall::CHANGE_TYPE_CALL_COMPOSITION is notified
     * via MegaChatCallListener::onChatCallUpdate
     *
     * Valid values:
     *   PEER_REMOVED = -1,
     *   NO_COMPOSITION_CHANGE = 0,
     *   PEER_ADDED = 1,
     *
     * @note During reconnection this callback has to be ignored to avoid notify that all users
     * in the call have left the call and have joined again. When status change to In-progres again,
     * the GUI can be adapted to all participants in the call
     *
     * @return if peer with peerid has been added/removed from call
     */
    virtual int  getCallCompositionChange() const;

    /**
     * @brief Get a list with the ids of peers that are participating in the call
     *
     * This function returns a value regardless your own user participates or not.
     *
     * @note If client is participating with several clients, it return only one instance
     *
     * You take the ownership of the returned value.
     *
     * @return A list of handles with the ids of peers
     */
    virtual mega::MegaHandleList *getPeeridParticipants() const;

    /**
     * @brief Get a MegaHandleList with the ids of peers that have moderator role in the call
     *
     * Participants with moderator role can:
     *  - End groupal calls for all participants
     *  - Approve/reject speaker requests
     *
     * You take the ownership of the returned value.
     *
     * @return A MegaHandleList of handles of peers that have moderator role in the call
     */
    virtual mega::MegaHandleList* getModerators() const;

    /**
     * @brief Get the number of peers participating in the call
     *
     * This function returns a value regardless your own user participates or not.
     *
     * @note If client is participating with several clients, it's taken as one peer
     *
     * @return Number of active participants in the call
     */
    virtual int getNumParticipants() const;

    /**
     * @brief Returns if call has been ignored
     *
     * @return True if the call has been ignored, false otherwise.
     */
    virtual bool isIgnored() const;

    /**
     * @brief Returns if call is incoming
     *
     * @return True if incoming call, false if outgoing
     */
    virtual bool isIncoming() const;

    /**
     * @brief Returns if call is outgoing
     *
     * @note in case another client logged in with the same account, has started the call,
     * this method will also return true.
     *
     * @return True if outgoing call, false if incoming
     */
    virtual bool isOutgoing() const;

    /**
     * @brief Returns true if our client has started the call
     *
     * @note in case another client logged in with the same account, has started the call,
     * this method will return false, but MegaChatCall::isOutgoing will return true. In this
     * case call is considerated an outgoing call, but our client wouldn't have started it.
     *
     * @return True if our client has started the call
     */
    virtual bool isOwnClientCaller() const;

    /**
     * @brief Returns the handle from user that has started the call
     *
     * This function only returns a valid value when call is or has gone through CALL_STATUS_RING_IN state.
     * In any other case, it will be MEGACHAT_INVALID_HANDLE
     *
     * @return user handle of caller
     */
    virtual MegaChatHandle getCaller() const;

    /**
     * @brief Returns if call is on hold
     *
     * @return true if call is on hold
     */
    virtual bool isOnHold() const;

    /**
     * @brief getGenericMessage Returns a generic message string
     *
     * This function only returns a non empty string in the following scenarios:
     *      - MegaChatCall::CHANGE_TYPE_GENERIC_NOTIFICATION is notified via MegaChatCallListener::onChatCallUpdate
     *
     * The MegaChatCall retains the ownership of the returned string
     *
     * @return a generic message string
     */
    virtual const char* getGenericMessage() const;

    /**
     * @brief Returns if user can speak in a call
     *
     * @note If there isn't a call in that chatroom, this method returns false
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @return True if user is allowed to speak in the call
     */
    virtual bool isSpeakAllow() const;

    /**
     * @brief Returns network quality
     *
     * The valid network quality values are:
     *  - MegaChatCall::NETWORK_QUALITY_BAD          = 0,    // Bad network quality detected
     *  - MegaChatCall::NETWORK_QUALITY_GOOD         = 1,    // Good network quality detected
     *
     * The value returned by this method, only can be considered as valid, when is notified by MegaChatCallListener::onChatCallUpdate
     * and MegaChatCall::hasChanged(MegaChatCall::CHANGE_TYPE_NETWORK_QUALITY) is true.
     *
     * @note The app may want to show a "slow network" warning when the quality is MegaChatCall::NETWORK_QUALITY_BAD, and remove it
     * when the quality is MegaChatCall::NETWORK_QUALITY_GOOD.
     *
     * @return network quality
     */
    virtual int getNetworkQuality() const;

    /**
     * @brief Returns if we have request speak
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @return true if we have request speak
     */
    virtual bool hasRequestSpeak() const;
};

/**
 * @brief Interface to get video frames from calls
 *
 * The same interface is used to receive local or remote video, but it has to be un/registered
 * by different functions:
 *
 *  - MegaChatApi::addChatLocalVideoListener / MegaChatApi::removeChatLocalVideoListener
 *  - MegaChatApi::addChatRemoteVideoListener / MegaChatApi::removeChatRemoteVideoListener
 */
class MegaChatVideoListener
{
public:
    virtual ~MegaChatVideoListener() {}

    /**
     * @brief This function is called when a new image from a local or remote device is available
     *
     * @param api MegaChatApi connected to the account
     * @param chatid MegaChatHandle that provides the video
     * @param width Size in pixels
     * @param height Size in pixels
     * @param buffer Data buffer in format ARGB: 4 bytes per pixel (total size: width * height * 4)
     * @param size Buffer size in bytes
     *
     *  The MegaChatVideoListener retains the ownership of the buffer.
     */
    virtual void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);
};

/**
 * @brief Interface to get notifications about calls
 *
 * You can un/subscribe to changes related to a MegaChatCall by using:
 *
 *  - MegaChatApi::addChatCallListener / MegaChatApi::removeChatCallListener
 *
 */
class MegaChatCallListener
{
public:
    virtual ~MegaChatCallListener() {}

    /**
     * @brief This function is called when there are changes in the call
     *
     * The changes can be accessed by MegaChatCall::getChanges or MegaChatCall::hasChanged
     *
     * The SDK retains the ownership of the MegaChatCall.
     * The call object that it contains will be valid until this function returns.
     * If you want to save the object, use MegaChatCall::copy.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi connected to the account
     * @param call MegaChatCall that contains the call with its changes
     */
    virtual void onChatCallUpdate(MegaChatApi* api, MegaChatCall *call);

    /**
     * @brief This function is called when there are changes in a session
     *
     * The changes can be accessed by MegaChatSession::getChanges or MegaChatSession::hasChanged
     *
     * The SDK retains the ownership of the MegaChatSession.
     * The call object that it contains will be valid until this function returns.
     * If you want to save the object, use MegaChatSession::copy.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi connected to the account
     * @param chatid MegaChatHandle that identifies the chat room
     * @param callid MegaChatHandle that identifies the call
     * @param session MegaChatSession that contains the session with its changes
     */
    virtual void onChatSessionUpdate(MegaChatApi* api, MegaChatHandle chatid, MegaChatHandle callid, MegaChatSession *session);
};

/**
 * @brief Interface to get notifications about scheduled meetings and scheduled meetings occurrences
 *
 * You can un/subscribe to changes by using:
 *
 *  - MegaChatApi::addSchedMeetingListener / MegaChatApi::removeSchedMeetingListener
 *
 */
class MegaChatScheduledMeetingListener
{
public:
    virtual ~MegaChatScheduledMeetingListener() {}

    /**
     * @brief This function is called when there are changes in the MegaChatScheduledMeeting
     *
     * Check documentation of MegaChatScheduledMeeting::hasChanged to get a complete list
     * of posible changes
     *
     * @param api MegaChatApi connected to the account
     * @param sm MegaChatScheduledMeeting that contains the updates relatives to the scheduled meeting
     */
    virtual void onChatSchedMeetingUpdate(MegaChatApi* /*api*/, MegaChatScheduledMeeting *sm);

    /**
     * @brief This function is called when the scheduled meeting occurrences for a chatroom have changed
     *
     * If append is false app must discard all occurrences for this chat and fetch again by calling
     * MegaChatApi->fetchScheduledMeetingOccurrencesByChat with param since equal to MEGACHAT_INVALID_TIMESTAMP.
     *
     * If append is true, new occurrences has been received from API (no need to discard current ones)
     *
     * @param api MegaChatApi connected to the account
     * @param chatid MegaChatHandle that identifies the chat room
     */
    virtual void onSchedMeetingOccurrencesUpdate(MegaChatApi* /*api*/, MegaChatHandle chatid, bool append);
};

class MegaChatPeerList
{
public:
    enum {
        PRIV_UNKNOWN = -2,
        PRIV_RM = -1,
        PRIV_RO = 0,
        PRIV_STANDARD = 2,
        PRIV_MODERATOR = 3
    };

    /**
     * @brief Creates a new instance of MegaChatPeerList
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatPeerList * createInstance();

    virtual ~MegaChatPeerList();

    /**
     * @brief Creates a copy of this MegaChatPeerList object
     *
     * The resulting object is fully independent of the source MegaChatPeerList,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatPeerList object
     */
    virtual MegaChatPeerList *copy() const;

    /**
     * @brief addPeer Adds a new chat peer to the list
     *
     * @param h MegaChatHandle of the user to be added
     * @param priv Privilege level of the user to be added
     * Valid values are:
     * - MegaChatPeerList::PRIV_RM = -1
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     */
    virtual void addPeer(MegaChatHandle h, int priv);

    /**
     * @brief Returns the MegaChatHandle of the chat peer at the position i in the list
     *
     * If the index is >= the size of the list, this function returns MEGACHAT_INVALID_HANDLE.
     *
     * @param i Position of the chat peer that we want to get from the list
     * @return MegaChatHandle of the chat peer at the position i in the list
     */
    virtual MegaChatHandle getPeerHandle(int i) const;

    /**
     * @brief Returns the privilege of the chat peer at the position i in the list
     *
     * If the index is >= the size of the list, this function returns PRIV_UNKNOWN.
     *
     * @param i Position of the chat peer that we want to get from the list
     * @return Privilege level of the chat peer at the position i in the list.
     * Valid values are:
     * - MegaChatPeerList::PRIV_UNKNOWN = -2
     * - MegaChatPeerList::PRIV_RM = -1
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     */
    virtual int getPeerPrivilege(int i) const;

    /**
     * @brief Returns the number of chat peer in the list
     * @return Number of chat peers in the list
     */
    virtual int size() const;

protected:
    MegaChatPeerList();

};

/**
 * @brief List of MegaChatRoom objects
 *
 * A MegaChatRoomList has the ownership of the MegaChatRoom objects that it contains, so they will be
 * only valid until the MegaChatRoomList is deleted. If you want to retain a MegaChatRoom returned by
 * a MegaChatRoomList, use MegaChatRoom::copy.
 *
 * Objects of this class are immutable.
 */
class MegaChatRoomList
{
public:
    virtual ~MegaChatRoomList() {}

    virtual MegaChatRoomList *copy() const;

    /**
     * @brief Returns the MegaChatRoom at the position i in the MegaChatRoomList
     *
     * The MegaChatRoomList retains the ownership of the returned MegaChatRoom. It will be only valid until
     * the MegaChatRoomList is deleted.
     *
     * If the index is >= the size of the list, this function returns NULL.
     *
     * @param i Position of the MegaChatRoom that we want to get for the list
     * @return MegaChatRoom at the position i in the list
     */
    virtual const MegaChatRoom *get(unsigned int i)  const;

    /**
     * @brief Returns the number of MegaChatRooms in the list
     * @return Number of MegaChatRooms in the list
     */
    virtual unsigned int size() const;

};

/**
 * @brief List of MegaChatListItem objects
 *
 * A MegaChatListItemList has the ownership of the MegaChatListItem objects that it contains, so they will be
 * only valid until the MegaChatListItemList is deleted. If you want to retain a MegaChatListItem returned by
 * a MegaChatListItemList, use MegaChatListItem::copy.
 *
 * Objects of this class are immutable.
 */
class MegaChatListItemList
{
public:
    virtual ~MegaChatListItemList() {}

    virtual MegaChatListItemList *copy() const;

    /**
     * @brief Returns the MegaChatRoom at the position i in the MegaChatListItemList
     *
     * The MegaChatListItemList retains the ownership of the returned MegaChatListItem. It will be only valid until
     * the MegaChatListItemList is deleted.
     *
     * If the index is >= the size of the list, this function returns NULL.
     *
     * @param i Position of the MegaChatListItem that we want to get for the list
     * @return MegaChatListItem at the position i in the list
     */
    virtual const MegaChatListItem *get(unsigned int i)  const;

    /**
     * @brief Returns the number of MegaChatListItems in the list
     * @return Number of MegaChatListItem in the list
     */
    virtual unsigned int size() const;

};

/**
 * @brief This class store rich preview data
 *
 * This class contains the data for rich links.
 */
class MegaChatRichPreview
{
public:
    virtual ~MegaChatRichPreview() {}
    virtual MegaChatRichPreview *copy() const;

    /**
      * @brief Returns rich preview text
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Text from rich preview
      *
      * @deprecated use MegaChatContainsMeta::getTextMessage instead, it contains the same
      * value. This function will eventually be removed in future versions of MEGAchat.
      */
    virtual const char *getText() const;

    /**
      * @brief Returns rich preview title
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Title from rich preview
      */
    virtual const char *getTitle() const;

    /**
      * @brief Returns rich preview description
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Description from rich preview
      */
    virtual const char *getDescription() const;

    /**
      * @brief Returns rich preview image
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Image from rich preview as a byte array encoded in Base64URL, or NULL if not available.
      */
    virtual const char *getImage() const;

    /**
      * @brief Returns rich preview image format
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Image format from rich preview
      */
    virtual const char *getImageFormat() const;

    /**
      * @brief Returns rich preview icon
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Icon from rich preview as a byte array encoded in Base64URL, or NULL if not available.
      */
    virtual const char *getIcon() const;

    /**
      * @brief Returns rich preview icon format
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Icon format from rich preview
      */
    virtual const char *getIconFormat() const;

    /**
      * @brief Returns rich preview url
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Url from rich preview
      */
    virtual const char *getUrl() const;

    /**
      * @brief Returns domain name from rich preview url
      *
      * The MegaChatRichPreview retains the ownership of the returned string. It will
      * be only valid until the MegaChatRichPreview is deleted.
      *
      * @return Domain name from rich preview url
      */
    virtual const char *getDomainName() const;
};

/**
 * @brief This class store geolocation data
 *
 * This class contains the data for geolocation.
 */
class MegaChatGeolocation
{
public:
    virtual ~MegaChatGeolocation() {}
    virtual MegaChatGeolocation *copy() const;

    /**
      * @brief Returns geolocation longitude
      *
      * @return Geolocation logitude value
      */
    virtual float getLongitude() const;

    /**
      * @brief Returns geolocation latitude
      *
      * @return Geolocation latitude value
      */
    virtual float getLatitude() const;

    /**
      * @brief Returns preview from shared geolocation
      *
      * The MegaChatGeolocation retains the ownership of the returned string. It will
      * be only valid until the MegaChatGeolocation is deleted.
      * It can be NULL
      *
      * @return Preview from geolocation as a byte array encoded in Base64URL, or NULL if not available.
      */
    virtual const char *getImage() const;
};

/**
 * @brief This class stores giphy data
 *
 * This class contains the data for giphy.
 */
class MegaChatGiphy
{
public:
    virtual ~MegaChatGiphy() {}
    virtual MegaChatGiphy* copy() const;

    /**
      * @brief Returns source of the mp4
      *
      * The MegaChatGiphy retains the ownership of the returned string. It will
      * be only valid until the MegaChatGiphy is deleted.
      * It can be nullptr
      *
      * @return mp4 source of the giphy or nullptr if not available.
      */
    virtual const char* getMp4Src() const;

    /**
      * @brief Returns source of the webp
      *
      * The MegaChatGiphy retains the ownership of the returned string. It will
      * be only valid until the MegaChatGiphy is deleted.
      * It can be nullptr
      *
      * @return webp source of the giphy or nullptr if not available.
      */
    virtual const char* getWebpSrc() const;

    /**
      * @brief Returns title of the giphy
      *
      * The MegaChatGiphy retains the ownership of the returned string. It will
      * be only valid until the MegaChatGiphy is deleted.
      * It can be nullptr
      *
      * @return title of the giphy or nullptr if not available.
      */
    virtual const char* getTitle() const;

    /**
      * @brief Returns size of the mp4
      *
      * @return mp4 size value
      */
    virtual long getMp4Size() const;

    /**
      * @brief Returns size of the webp
      *
      * @return webp size value
      */
    virtual long getWebpSize() const;

    /**
      * @brief Returns width of the giphy
      *
      * @return giphy width value
      */
    virtual int getWidth() const;

    /**
      * @brief Returns height of the giphy
      *
      * @return giphy height value
      */
    virtual int getHeight() const;
};

/**
 * @brief This class represents meta contained
 *
 * This class includes pointer to differents kind of meta contained, like MegaChatRichPreview.
 *
 * @see MegaChatMessage::containsMetaType()
 */
class MegaChatContainsMeta
{
public:
    enum
    {
      CONTAINS_META_INVALID         = -1,   /// Unknown type of meta contained
      CONTAINS_META_RICH_PREVIEW    = 0,    /// Rich-preview type for meta contained
      CONTAINS_META_GEOLOCATION     = 1,    /// Geolocation type for meta contained
      CONTAINS_META_GIPHY           = 3,    /// Giphy type for meta contained
    };

    virtual ~MegaChatContainsMeta() {}

    virtual MegaChatContainsMeta *copy() const;

    /**
     * @brief Returns the type of meta contained
     *
     *  - MegaChatContainsMeta::CONTAINS_META_INVALID        = -1
     * Unknown meta contained data in the message
     *
     *  - MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW   = 0
     * Meta contained is from rich preview type
     *
     *  - MegaChatContainsMeta::CONTAINS_META_GEOLOCATION  = 1
     * Meta contained is from geolocation type
     *
     *  - MegaChatContainsMeta::CONTAINS_META_GIPHY  = 2
     * Meta contained is from giphy type  
     * @return Type from meta contained of the message
     */
    virtual int getType() const;

    /**
     * @brief Returns a generic message to be shown when app does not support the type of the contained meta
     *
     * This string is always available for all messages of MegaChatMessage::TYPE_CONTAINS_META.
     * When the app does not support yet the sub-type of contains-meta, this string
     * can be shown as alternative.
     *
     * The MegaChatContainsMeta retains the ownership of the returned string. It will
     * be only valid until the MegaChatContainsMeta is deleted.
     *
     * @return String to be shown when app can't parse the meta contained
     */
    virtual const char *getTextMessage() const;

    /**
     * @brief Returns data about rich-links
     *
     * @note This function only returns a valid object in case the function
     * \c MegaChatContainsMeta::getType returns MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW.
     * Otherwise, it returns NULL.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatContainsMeta object is deleted.
     *
     * @return MegaChatRichPreview with details about rich-link.
     */
    virtual const MegaChatRichPreview *getRichPreview() const;

    /**
     * @brief Returns data about geolocation
     *
     * @note This function only returns a valid object in case the function
     * \c MegaChatContainsMeta::getType returns MegaChatContainsMeta::CONTAINS_META_GEOLOCATION.
     * Otherwise, it returns NULL.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatContainsMeta object is deleted.
     *
     * @return MegaChatGeolocation with details about geolocation.
     */
    virtual const MegaChatGeolocation *getGeolocation() const;
    
    /**
     * @brief Returns data about giphy
     *
     * @note This function only returns a valid object in case the function
     * \c MegaChatContainsMeta::getType returns MegaChatContainsMeta::CONTAINS_META_GIPHY.
     * Otherwise, it returns nullptr.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatContainsMeta object is deleted.
     *
     * @return MegaChatGiphy with details about giphy.
     */
    virtual const MegaChatGiphy* getGiphy() const;
};

class MegaChatMessage
{
public:
    // Online status of message
    enum {
        STATUS_UNKNOWN              = -1,   /// Invalid status
        // for outgoing messages
        STATUS_SENDING              = 0,    /// Message has not been sent or is not yet confirmed by the server
        STATUS_SENDING_MANUAL       = 1,    /// Message is too old to auto-retry sending, or group composition has changed, or user has read-only privilege, or user doesn't belong to chatroom. User must explicitly confirm re-sending. All further messages queued for sending also need confirmation
        STATUS_SERVER_RECEIVED      = 2,    /// Message confirmed by server, but not yet delivered to recepient(s)
        STATUS_SERVER_REJECTED      = 3,    /// Message is rejected by server for some reason (the message was confirmed but we didn't receive the confirmation because went offline or closed the app before)
        STATUS_DELIVERED            = 4,    /// Peer confirmed message receipt. Available only for 1on1 chats, but currently not in use.
        // for incoming messages
        STATUS_NOT_SEEN             = 5,    /// User hasn't read this message yet
        STATUS_SEEN                 = 6     /// User has read this message
    };

    // Types of message
    enum {
        TYPE_UNKNOWN                = -1,   /// Unknown type of message (apps should hide them)
        TYPE_INVALID                = 0,    /// Invalid type
        TYPE_NORMAL                 = 1,    /// Regular text message
        TYPE_LOWEST_MANAGEMENT      = 2,
        TYPE_ALTER_PARTICIPANTS     = 2,    /// Management message indicating the participants in the chat have changed
        TYPE_TRUNCATE               = 3,    /// Management message indicating the history of the chat has been truncated
        TYPE_PRIV_CHANGE            = 4,    /// Management message indicating the privilege level of a user has changed
        TYPE_CHAT_TITLE             = 5,    /// Management message indicating the title of the chat has changed
        TYPE_CALL_ENDED             = 6,    /// Management message indicating a call has finished
        TYPE_CALL_STARTED           = 7,    /// Management message indicating a call has started
        TYPE_PUBLIC_HANDLE_CREATE   = 8,    /// Management message indicating a public handle has been created
        TYPE_PUBLIC_HANDLE_DELETE   = 9,    /// Management message indicating a public handle has been removed
        TYPE_SET_PRIVATE_MODE       = 10,   /// Management message indicating the chat mode has been set to private
        TYPE_SET_RETENTION_TIME     = 11,   /// Management message indicating the retention time has changed
        TYPE_SCHED_MEETING          = 12,   /// Management message indicating that a scheduled meeting is created/updated
        TYPE_HIGHEST_MANAGEMENT     = 12,
        TYPE_NODE_ATTACHMENT        = 101,   /// User message including info about shared nodes
        TYPE_REVOKE_NODE_ATTACHMENT = 102,   /// User message including info about a node that has stopped being shared (obsolete)
        TYPE_CONTACT_ATTACHMENT     = 103,   /// User message including info about shared contacts
        TYPE_CONTAINS_META          = 104,   /// User message including additional metadata (ie. rich-preview for links)
        TYPE_VOICE_CLIP             = 105,   /// User message including info about shared voice clip
    };

    enum
    {
        CHANGE_TYPE_STATUS          = 0x01,
        CHANGE_TYPE_CONTENT         = 0x02,
        CHANGE_TYPE_ACCESS          = 0x04,  /// When the access to attached nodes has changed (obsolete)
        CHANGE_TYPE_TIMESTAMP       = 0x08,  /// When ts has been updated by chatd in confirmation
    };

    enum
    {
        REASON_PEERS_CHANGED        = 1,    /// Group chat participants have changed
        REASON_TOO_OLD              = 2,    /// Message is too old to auto-retry sending
        REASON_GENERAL_REJECT       = 3,    /// chatd rejected the message, for unknown reason
        REASON_NO_WRITE_ACCESS      = 4,    /// Read-only privilege or not belong to the chatroom
        REASON_NO_CHANGES           = 6     /// Edited message has the same content than original message
    };

    enum
    {
        END_CALL_REASON_ENDED           = 1,    /// Call finished normally
        END_CALL_REASON_REJECTED        = 2,    /// Call was rejected by callee
        END_CALL_REASON_NO_ANSWER       = 3,    /// Call wasn't answered
        END_CALL_REASON_FAILED          = 4,    /// Call finished by an error
        END_CALL_REASON_CANCELLED       = 5,    /// Call was canceled by caller.
        END_CALL_REASON_BY_MODERATOR    = 6     /// group or meeting call has been ended by moderator
    };

    enum
    {
        DECRYPTING          = 1,        /// Message pending to be decrypted
        INVALID_KEY         = 2,        /// Key not found for the message (permanent failure)
        INVALID_SIGNATURE   = 3,        /// Signature verification failure (permanent failure)
        INVALID_FORMAT      = 4,        /// Malformed/corrupted data in the message (permanent failure)
        INVALID_TYPE        = 5         /// Management message of unknown type (transient, not supported by the app yet)
    };

    virtual ~MegaChatMessage() {}
    virtual MegaChatMessage *copy() const;

    /**
     * @brief Returns the status of the message.
     *
     * Valid values are:
     *  - STATUS_UNKNOWN            = -1
     *  - STATUS_SENDING            = 0
     *  - STATUS_SENDING_MANUAL     = 1
     *  - STATUS_SERVER_RECEIVED    = 2
     *  - STATUS_SERVER_REJECTED    = 3
     *  - STATUS_SERVER_DELIVERED   = 4
     *  - STATUS_NOT_SEEN           = 5
     *  - STATUS_SEEN               = 6
     *
     * If status is STATUS_SENDING_MANUAL, the user can whether manually retry to send the
     * message (get content and send new message as usual through MegaChatApi::sendMessage),
     * or discard the message. In both cases, the message should be removed from the manual-send
     * queue by calling MegaChatApi::removeUnsentMessage once the user has sent or discarded it.
     *
     * @return Returns the status of the message.
     */
    virtual int getStatus() const;

    /**
     * @brief Returns the identifier of the message.
     *
     * @return MegaChatHandle that identifies the message in this chatroom
     */
    virtual MegaChatHandle getMsgId() const;

    /**
     * @brief Returns the temporal identifier of the message
     *
     * The temporal identifier has different usages depending on the status of the message:
     *  - MegaChatMessage::STATUS_SENDING: valid until it's confirmed by the server.
     *  - MegaChatMessage::STATUS_SENDING_MANUAL: valid until it's removed from manual-send queue.
     *
     * @note If status is STATUS_SENDING_MANUAL, this value can be used to identify the
     * message moved into the manual-send queue. The message itself will be identified by its
     * MegaChatMessage::getRowId from now on. The row id can be passed to
     * MegaChatApi::removeUnsentMessage to definitely remove the message.
     *
     * For messages in a different status than above, this identifier should not be used.
     *
     * @return MegaChatHandle that temporary identifies the message
     */
    virtual MegaChatHandle getTempId() const;

    /**
     * @brief Returns the index of the message in the loaded history
     *
     * The higher is the value of the index, the newer is the chat message.
     * The lower is the value of the index, the older is the chat message.
     *
     * @note This index is can grow on both direction: increments are due to new
     * messages in the history, and decrements are due to old messages being loaded
     * in the history buffer.
     *
     * @return Index of the message in the loaded history.
     */
    virtual int getMsgIndex() const;

    /**
     * @brief Returns a MegaChatHandle
     *
     * If MegaChatMessage::getType returns MegaChatMessage::TYPE_SCHED_MEETING, this method
     * returns the scheduled meeting id, of the updated scheduled meeting
     *
     * If MegaChatMessage::getType doesn't returns MegaChatMessage::TYPE_SCHED_MEETING, this method returns:
     *  - For outgoing messages, the handle of the target user.
     *  - For incoming messages, the handle of the sender.
     *
     * @return a MegaChatHandle
     */
    virtual MegaChatHandle getUserHandle() const;

    /**
     * @brief Returns the type of message.
     *
     * Valid values are:
     *  - TYPE_INVALID: Invalid type. In those cases, the MegaChatMessage::getCode can take the following values:
     *      * INVALID_FORMAT
     *      * INVALID_SIGNATURE
     *  - TYPE_NORMAL: Regular text message
     *  - TYPE_ALTER_PARTICIPANTS: Management message indicating the participants in the chat have changed
     *  - TYPE_TRUNCATE: Management message indicating the history of the chat has been truncated
     *  - TYPE_PRIV_CHANGE: Management message indicating the privilege level of a user has changed
     *  - TYPE_CHAT_TITLE: Management message indicating the title of the chat has changed
     *  - TYPE_ATTACHMENT: User message including info about a shared node
     *  - TYPE_REVOKE_ATTACHMENT: User message including info about a node that has stopped being shared
     *  - TYPE_CONTACT: User message including info about a contact
     *  - TYPE_VOICE_CLIP: User messages incluiding info about a node that represents a voice-clip
     *  - TYPE_UNKNOWN: Unknown message, should be ignored/hidden. The MegaChatMessage::getCode can take the following values:
     *      * INVALID_TYPE
     *      * INVALID_KEYID
     *      * DECRYPTING
     *
     * @return Returns the Type of message.
     */
    virtual int getType() const;

    /**
     * @brief Returns if the message has any confirmed reaction.
     *
     * @return Returns true if the message has any confirmed reaction, otherwise returns false.
     */
    virtual bool hasConfirmedReactions() const;

    /**
     * @brief Returns the timestamp of the message.
     * @return Returns the timestamp of the message.
     */
    virtual int64_t getTimestamp() const;

    /**
     * @brief Returns the content of the message
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatMessage object is deleted.
     *
     * @note If message is of type MegaChatMessage::TYPE_CONTAINS_META, for convenience this function
     * will return the same content than MegaChatContainsMeta::getTextMessage
     *
     * @return Content of the message. If message was deleted, it returns NULL.
     */
    virtual const char *getContent() const;

    /**
     * @brief Returns whether the message is an edit of the original message
     * @return True if the message has been edited. Otherwise, false.
     */
    virtual bool isEdited() const;

    /**
     * @brief Returns whether the message has been deleted
     * @return True if the message has been deleted. Otherwise, false.
     */
    virtual bool isDeleted() const;

    /**
     * @brief Returns whether the message can be edited
     *
     * Currently, messages are editable only during a timeframe (1 hour). Later on, the
     * edit will be rejected. The same applies to deletions.
     *
     * @return True if the message can be edited. Otherwise, false.
     */
    virtual bool isEditable() const;

    /**
     * @brief Returns whether the message can be deleted
     *
     * Currently, messages can be deleted only during a timeframe (1 hour). Later on, the
     * deletion will be rejected.
     *
     * @return True if the message can be deleted. Otherwise, false.
     */
    virtual bool isDeletable() const;

    /**
     * @brief Returns whether the message is a management message
     *
     * Management messages are intented to record in the history any change related
     * to the management of the chatroom, such as a title change or an addition of a peer.
     *
     * @return True if the message is a management message.
     */
    virtual bool isManagementMessage() const;

    /**
     * @brief Return the handle of the user relative to the action
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_ALTER_PARTICIPANTS: handle of the user who is added/removed
     *  - MegaChatMessage::TYPE_PRIV_CHANGE: handle of the user whose privilege is changed
     *  - MegaChatMessage::TYPE_REVOKE_ATTACHMENT: handle of the node which access has been revoked
     *  - MegaChatMessage::TYPE_SCHED_MEETING: scheduled meeting handle of the updated scheduled meeting
     * @return Handle of the user/node, depending on the type of message
     */
    virtual MegaChatHandle getHandleOfAction() const;

    /**
     * @brief Return the privilege of the user relative to the action
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
     *      - When a peer is removed: MegaChatRoom::PRIV_RM
     *      - When a peer is added: MegaChatRoom::PRIV_UNKNOWN
     *  - MegaChatMessage::TYPE_PRIV_CHANGE: the new privilege of the user
     *  - MegaChatMessage::TYPE_SCHED_MEETING: bitmask with the scheduled meeting changed fields (check hasSchedMeetingChanged)
     *
     * @return Privilege level as above
     */
    virtual int getPrivilege() const;

    /**
     * @brief Return a generic code used for different purposes
     *
     * The code returned by this method is valid only in the following cases:
     *
     *  - Messages with status MegaChatMessage::STATUS_SENDING_MANUAL: the code specifies
     * the reason because the server rejects the message. The possible values are:
     *      - MegaChatMessage::REASON_PEERS_CHANGED     = 1
     *      - MegaChatMessage::REASON_TOO_OLD           = 2
     *      - MegaChatMessage::REASON_GENERAL_REJECT    = 3
     *      - MegaChatMessage::REASON_NO_WRITE_ACCESS   = 4
     *      - MegaChatMessage::REASON_NO_CHANGES        = 6
     *
     * @return A generic code for additional information about the message.
     */
    virtual int getCode() const;

    /**
     * @brief Return number of user that have been attached to the message
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_CONTACT_ATTACHMENT: the number of users in the message
     *
     * @return Number of users that have been attached to the message
     */
    virtual unsigned int getUsersCount() const;

    /**
     * @brief Return the handle of the user that has been attached in \c index position
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_CONTACT_ATTACHMENT: the handle of the user
     *
     * If the index is >= the number of users attached to the message, this function
     * will return MEGACHAT_INVALID_HANDLE.
     *
     * @param index of the users inside user vector
     * @return The handle of the user
     */
    virtual MegaChatHandle getUserHandle(unsigned int index) const;

    /**
     * @brief Return the name of the user that has been attached in \c index position
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_CONTACT_ATTACHMENT: the name of the user
     *
     * If the index is >= the number of users attached to the message, this function
     * will return NULL.
     *
     * @param index of the users inside user vector
     * @return The name of the user
     */
    virtual const char *getUserName(unsigned int index) const;

    /**
     * @brief Return the email of the user that has been attached in \c index position
     *
     * Only valid for management messages:
     *  - MegaChatMessage::TYPE_CONTACT_ATTACHMENT: the handle of the user
     *
     * If the index is >= the number of users attached to the message, this function
     * will return NULL.
     *
     * @param index of the users inside user vector
     * @return The email of the user
     */
    virtual const char *getUserEmail(unsigned int index) const;

    /**
     * @brief Return a list with all MegaNode attached to the message
     *
     * @return list with MegaNode
     */
    virtual mega::MegaNodeList *getMegaNodeList() const;

    /**
     * @brief Return a list with handles
     *
     * The SDK retains the ownership of the returned value.It will be valid until
     * the MegaChatMessage object is deleted.
     *
     * It can be used for different purposes.
     * Valid for:
     *  - MegaChatMessage::TYPE_CALL_ENDED
     *   It will be empty if MegaChatMessage::getTermCode is not END_CALL_REASON_ENDED either END_CALL_REASON_FAILED
     *
     * @return list with MegaHandle
     */
    virtual mega::MegaHandleList *getMegaHandleList() const;

    /**
     * @brief Return call duration in seconds
     *
     * This funcion returns a valid value for:
     *  - MegaChatMessage::TYPE_CALL_ENDED
     *
     * @return Call duration
     */
    virtual int getDuration() const;

    /**
     * @brief Return retention time in seconds
     *
     * This function only returns a valid value for messages of type:
     *  - MegaChatMessage::TYPE_SET_RETENTION_TIME
     *
     * @return Retention time (in seconds)
     */
    virtual unsigned int getRetentionTime() const;

    /**
     * @brief Return the termination code of the call
     *
     * This funcion returns a valid value for:
     *  - MegaChatMessage::TYPE_CALL_ENDED
     *
     * The possible values for termination codes are the following:
     *  - END_CALL_REASON_ENDED         = 1
     *  - END_CALL_REASON_REJECTED      = 2
     *  - END_CALL_REASON_NO_ANSWER     = 3
     *  - END_CALL_REASON_FAILED        = 4
     *  - END_CALL_REASON_CANCELLED     = 5
     *  - END_CALL_REASON_BY_MODERATOR  = 6
     *
     * @return Call termination code
     */
    virtual int getTermCode() const;

    /**
     * @brief Returns true if this scheduled meeting associated to this message has an specific change
     *
     * This funcion returns a valid value for: MegaChatMessage::TYPE_SCHED_MEETING
     * In other cases, the return value of this function will be always false.
     *
     * @param changeType The type of change to check. It can be one of the following values:
     * - MegaChatScheduledMeeting::SC_PARENT    [1]  - Parent scheduled meeting id has changed
     * - MegaChatScheduledMeeting::SC_TZONE     [2]  - Timezone has changed
     * - MegaChatScheduledMeeting::SC_START     [3]  - Start date time has changed
     * - MegaChatScheduledMeeting::SC_END       [4]  - End date time has changed
     * - MegaChatScheduledMeeting::SC_TITLE     [5]  - Title has changed
     * - MegaChatScheduledMeeting::SC_DESC      [6]  - Description has changed
     * - MegaChatScheduledMeeting::SC_ATTR      [7]  - Attributes have changed
     * - MegaChatScheduledMeeting::SC_OVERR     [8]  - Override date time has changed
     * - MegaChatScheduledMeeting::SC_CANC      [9]  - Cancelled flag has changed
     * - MegaChatScheduledMeeting::SC_FLAGS     [10] - Scheduled meetings flags have changed
     * - MegaChatScheduledMeeting::SC_RULES     [11] - Repetition rules have changed
     *
     * @return true if this scheduled meeting associated to this message has an specific change
     */
    virtual bool hasSchedMeetingChanged(unsigned int change) const;

    /**
     * @brief Returns a MegaStringList list relative to the action
     *
     * The MegaChatMessage retains the ownership of the MegaStringList.
     *
     * This funcion returns a valid value for:
     * - MegaChatMessage::TYPE_SCHED_MEETING: the first element of the list, represents the old title,
     *   and the second element of the list, represents the new title
     *
     * @return a MegaStringList list relative to the action
     * for scheduled meetings params changed
     */
    virtual const mega::MegaStringList* getStringList() const;

     /** @brief Return the id for messages in manual sending status / queue
     *
     * This value can be used to identify the message moved into the manual-send
     * queue. The row id can be passed to MegaChatApi::removeUnsentMessage to
     * definitely remove the message.
     *
     * @note this id is only valid for messages in STATUS_SENDING_MANUAL. For any
     * other message, the function returns MEGACHAT_INVALID_HANDLE.
     *
     * @return The id of the message in the manual sending queue.
     */
    virtual MegaChatHandle getRowId() const;

    /**
     * @brief Returns a bit field with the changes of the message
     *
     * This value is only useful for messages notified by MegaChatRoomListener::onMessageUpdate
     * that can notify about message modifications.
     *
     * @return The returned value is an OR combination of these flags:
     *
     * - MegaChatMessage::CHANGE_TYPE_STATUS   = 0x01
     * Check if the status of the message changed
     *
     * - MegaChatMessage::CHANGE_TYPE_CONTENT  = 0x02
     * Check if the content of the message changed
     *
     * - MegaChatMessage::CHANGE_TYPE_ACCESS   = 0x04
     * Check if the access to attached nodes has changed
     *
     * - MegaChatMessage::CHANGE_TYPE_TIMESTAMP   = 0x08
     * Check if the ts has been updated by chatd
     */
    virtual int getChanges() const;

    /**
     * @brief Returns true if this message has an specific change
     *
     * This value is only useful for nodes notified by MegaChatRoomListener::onMessageUpdate
     * that can notify about the message modifications.
     *
     * In other cases, the return value of this function will be always false.
     *
     * @param changeType The type of change to check. It can be one of the following values:
     *
     * - MegaChatMessage::CHANGE_TYPE_STATUS   = 0x01
     * Check if the status of the message changed
     *
     * - MegaChatMessage::CHANGE_TYPE_CONTENT  = 0x02
     * Check if the content of the message changed
     *
     * - MegaChatMessage::CHANGE_TYPE_ACCESS   = 0x04
     * Check if the access to attached nodes has changed
     *
     * - MegaChatMessage::CHANGE_TYPE_TIMESTAMP   = 0x08
     * Check if the ts has been updated by chatd
     *
     * @return true if this message has an specific change
     */
    virtual bool hasChanged(int changeType) const;

    /**
     * @brief Returns the meta contained
     *
     * This function a valid value only if the type of the message is MegaChatMessage::TYPE_CONTAINS_META.
     * Otherwise, it returns NULL.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatMessage object is deleted.
     *
     * @return MegaChatContainsMeta with the details of meta contained
     */
    virtual const MegaChatContainsMeta *getContainsMeta() const;
};

/**
 * @brief Provides information about an asynchronous request
 *
 * Most functions in this API are asynchonous, except the ones that never require to
 * contact MEGA servers. Developers can use listeners (MegaListener, MegaChatRequestListener)
 * to track the progress of each request. MegaChatRequest objects are provided in callbacks sent
 * to these listeners and allow developers to know the state of the request, their parameters
 * and their results.
 *
 * Objects of this class aren't live, they are snapshots of the state of the request
 * when the object is created, they are immutable.
 *
 * These objects have a high number of 'getters', but only some of them return valid values
 * for each type of request. Documentation of each request specify which fields are valid.
 *
 */
class MegaChatRequest
{
public:
    enum {
        TYPE_INITIALIZE,// (obsolete)
        TYPE_CONNECT,   // (obsolete) connect to chatd (call it after login+fetchnodes with MegaApi)
        TYPE_DELETE,    // delete MegaChatApi instance
        TYPE_LOGOUT,    // delete existing Client and creates a new one
        TYPE_SET_ONLINE_STATUS,
        TYPE_START_CHAT_CALL, TYPE_ANSWER_CHAT_CALL,
        TYPE_DISABLE_AUDIO_VIDEO_CALL, TYPE_HANG_CHAT_CALL,
        TYPE_CREATE_CHATROOM, TYPE_REMOVE_FROM_CHATROOM,
        TYPE_INVITE_TO_CHATROOM, TYPE_UPDATE_PEER_PERMISSIONS,
        TYPE_EDIT_CHATROOM_NAME, TYPE_EDIT_CHATROOM_PIC,
        TYPE_TRUNCATE_HISTORY,
        TYPE_SHARE_CONTACT,
        TYPE_GET_FIRSTNAME, TYPE_GET_LASTNAME,
        TYPE_DISCONNECT, TYPE_GET_EMAIL,
        TYPE_ATTACH_NODE_MESSAGE, TYPE_REVOKE_NODE_MESSAGE,
        TYPE_SET_BACKGROUND_STATUS, TYPE_RETRY_PENDING_CONNECTIONS,
        TYPE_SEND_TYPING_NOTIF, TYPE_SIGNAL_ACTIVITY,
        TYPE_SET_PRESENCE_PERSIST, TYPE_SET_PRESENCE_AUTOAWAY,
        TYPE_LOAD_AUDIO_VIDEO_DEVICES, // Deprecated
        TYPE_ARCHIVE_CHATROOM,
        TYPE_PUSH_RECEIVED, TYPE_SET_LAST_GREEN_VISIBLE, TYPE_LAST_GREEN,
        TYPE_LOAD_PREVIEW, TYPE_CHAT_LINK_HANDLE,
        TYPE_SET_PRIVATE_MODE, TYPE_AUTOJOIN_PUBLIC_CHAT, TYPE_CHANGE_VIDEO_STREAM,
        TYPE_IMPORT_MESSAGES,  TYPE_SET_RETENTION_TIME, TYPE_SET_CALL_ON_HOLD,
        TYPE_ENABLE_AUDIO_LEVEL_MONITOR, TYPE_MANAGE_REACTION,
        TYPE_GET_PEER_ATTRIBUTES, TYPE_REQUEST_SPEAK, TYPE_APPROVE_SPEAK,
        TYPE_REQUEST_HIGH_RES_VIDEO, TYPE_REQUEST_LOW_RES_VIDEO,
        TYPE_OPEN_VIDEO_DEVICE, TYPE_REQUEST_HIRES_QUALITY,
        TYPE_DEL_SPEAKER, TYPE_REQUEST_SVC_LAYERS,
        TYPE_SET_CHATROOM_OPTIONS,
        TYPE_CREATE_SCHEDULED_MEETING, // Deprecated
        TYPE_DELETE_SCHEDULED_MEETING, TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES,
        TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE,
        TYPE_UPDATE_SCHEDULED_MEETING,
        TOTAL_OF_REQUEST_TYPES
    };

    enum {
        AUDIO = 0,
        VIDEO = 1
    };

    virtual ~MegaChatRequest();

    /**
     * @brief Creates a copy of this MegaChatRequest object
     *
     * The resulting object is fully independent of the source MegaChatRequest,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatRequest object
     */
    virtual MegaChatRequest *copy();

    /**
     * @brief Returns the type of request associated with the object
     * @return Type of request associated with the object
     */
    virtual int getType() const;

    /**
     * @brief Returns a readable string that shows the type of request
     *
     * This function returns a pointer to a statically allocated buffer.
     * You don't have to free the returned pointer
     *
     * @return Readable string showing the type of request
     */
    virtual const char *getRequestString() const;

    /**
     * @brief Returns a readable string that shows the type of request
     *
     * This function provides exactly the same result as MegaChatRequest::getRequestString.
     * It's provided for a better Java compatibility
     *
     * @return Readable string showing the type of request
     */
    virtual const char* toString() const;

    /**
     * @brief Returns the tag that identifies this request
     *
     * The tag is unique for the MegaChatApi object that has generated it only
     *
     * @return Unique tag that identifies this request
     */
    virtual int getTag() const;

    /**
     * @brief Returns a number related to this request
     * @return Number related to this request
     */
    virtual long long getNumber() const;

    /**
     * @brief Return the number of times that a request has temporarily failed
     * @return Number of times that a request has temporarily failed
     */
    virtual int getNumRetry() const;

    /**
     * @brief Returns a flag related to the request
     *
     * This value is valid for these requests:
     * - MegaChatApi::createChat - Creates a chat for one or more participants
     * - MegaChatApi::openChatPreview - Returns true if it's a meeting room
     * - MegaChatApi::checkChatPreview -Returns true if it's a meeting room
     *
     * @return Flag related to the request
     */
    virtual bool getFlag() const;

    /**
     * @brief Returns the list of peers in a chat.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * This value is valid for these requests:
     * - MegaChatApi::createChat - Returns the list of peers and their privilege level
     *
     * @return List of peers of a chat
     */
    virtual MegaChatPeerList *getMegaChatPeerList();

    /**
     * @brief Returns the handle that identifies the chat
     * @return The handle of the chat
     */
    virtual MegaChatHandle getChatHandle();

    /**
     * @brief Returns the handle that identifies the user
     * @return The handle of the user
     */
    virtual MegaChatHandle getUserHandle();

    /**
     * @brief Returns the privilege level
     * @return The access level of the user in the chat
     */
    virtual int getPrivilege();

    /**
     * @brief Returns a text relative to this request
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * @return Text relative to this request
     */
    virtual const char *getText() const;

    /**
     * @brief Returns a link relative to this request
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * @return Link relative to this request
     */
    virtual const char *getLink() const;

    /**
     * @brief Returns a message contained on request
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * @return Message relative to this request
     */
    virtual MegaChatMessage *getMegaChatMessage();

    /**
     * @brief Returns the list of nodes on this request.
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * This value is valid for these requests:
     * - MegaChatApi::attachNodes - Returns the list of nodes attached to the message
     *
     * @return List of nodes in this request
     */
    virtual mega::MegaNodeList *getMegaNodeList();

    /**
     * @brief Returns the list of handles related to this request
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * This value is valid for these requests:
     * - MegaChatApi::pushReceived - Returns the list of ids for unread messages in the chatid
     *   (you can get the list of chatids from \c getMegaHandleList)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return mega::MegaHandleList of handles for a given chatid
     */
    virtual mega::MegaHandleList *getMegaHandleListByChat(MegaChatHandle chatid);

    /**
     * @brief Returns the scheduled meeting list
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * @return scheduled meeting list
     */
    virtual MegaChatScheduledMeetingList* getMegaChatScheduledMeetingList() const;

    /**
     * @brief Returns the scheduled meeting occurrences list
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * @return scheduled meeting occurrences list
     */
    virtual MegaChatScheduledMeetingOccurrList* getMegaChatScheduledMeetingOccurrList() const;

    /**
     * @brief Returns the list of handles related to this request
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatRequest object is deleted.
     *
     * This value is valid for these requests:
     * - MegaChatApi::pushReceived - Returns the list of chatids with unread messages
     * - MegaChatApi::openChatPreview - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     * - MegaChatApi::checkChatPreview - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     *
     * @return mega::MegaHandleList of handles for a given chatid
     */
    virtual mega::MegaHandleList *getMegaHandleList();

    /**
     * @brief Returns the type of parameter related to the request
     *
     * This value is valid for these requests:
     * - MegaChatApi::enableAudio - Returns MegaChatRequest::AUDIO
     * - MegaChatApi::disableAudio - Returns MegaChatRequest::AUDIO
     * - MegaChatApi::enableVideo - Returns MegaChatRequest::VIDEO
     * - MegaChatApi::disableVideo - Returns MegaChatRequest::VIDEO
     * - MegaChatApi::attachVoiceMessage - Returns one
     * - MegaChatApi::attachNode - Returns zero
     * - MegaChatApi::retryPendingConnections - Returns one for refreshUrl
     * - MegaChatApi::pushReceived - Returns zero Android, one IOS
     *
     * @return Type of parameter related to the request
     */
    virtual int getParamType();
};

/**
 * @brief Interface to receive information about requests
 *
 * All requests allows to pass a pointer to an implementation of this interface in the last parameter.
 * You can also get information about all requests using MegaChatApi::addChatRequestListener
 *
 * MegaListener objects can also receive information about requests
 *
 * This interface uses MegaChatRequest objects to provide information of requests. Take into account that not all
 * fields of MegaChatRequest objects are valid for all requests. See the documentation about each request to know
 * which fields contain useful information for each one.
 *
 */
class MegaChatRequestListener
{
public:
    /**
     * @brief This function is called when a request is about to start being processed
     *
     * The SDK retains the ownership of the request parameter.
     * Don't use it after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     */
    virtual void onRequestStart(MegaChatApi* api, MegaChatRequest *request);

    /**
     * @brief This function is called when a request has finished
     *
     * There won't be more callbacks about this request.
     * The last parameter provides the result of the request. If the request finished without problems,
     * the error code will be API_OK
     *
     * The SDK retains the ownership of the request and error parameters.
     * Don't use them after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     * @param e Error information
     */
    virtual void onRequestFinish(MegaChatApi* api, MegaChatRequest *request, MegaChatError* e);

    /**
     * @brief This function is called to inform about the progres of a request
     *
     * The SDK retains the ownership of the request parameter.
     * Don't use it after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     * @see MegaChatRequest::getTotalBytes MegaChatRequest::getTransferredBytes
     */
    virtual void onRequestUpdate(MegaChatApi*api, MegaChatRequest *request);

    /**
     * @brief This function is called when there is a temporary error processing a request
     *
     * The request continues after this callback, so expect more MegaChatRequestListener::onRequestTemporaryError or
     * a MegaChatRequestListener::onRequestFinish callback
     *
     * The SDK retains the ownership of the request and error parameters.
     * Don't use them after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     * @param error Error information
     */
    virtual void onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError* error);
    virtual ~MegaChatRequestListener();
};

/**
 * @brief Represents the configuration of the online presence for the account
 *
 * The online presence configuration includes the following:
 *
 * - Online status - it can be one of the following values:
 *
 *      - MegaChatApi::STATUS_OFFLINE = 1
 *          The user appears as being offline
 *
 *      - MegaChatApi::STATUS_AWAY = 2
 *          The user is away and might not answer.
 *
 *      - MegaChatApi::STATUS_ONLINE = 3
 *          The user is connected and online.
 *
 *      - MegaChatApi::STATUS_BUSY = 4
 *          The user is busy and don't want to be disturbed.
 *
 * - Autoway: if enabled, the online status will change from MegaChatApi::STATUS_ONLINE to
 *  MegaChatApi::STATUS_AWAY automatically after a timeout.
 *
 * @note The autoaway settings are preserved even when the auto-away mechanism is inactive (i.e. when
 * the status is other than online or the user has enabled the persistence of the status.
 * When the autoaway mechanish is enabled, it requires the app calls \c MegaChatApi::signalPresenceActivity
 * in order to prevent becoming MegaChatApi::STATUS_AWAY automatically after the timeout.
 * You can check if the autoaway mechanism is active by calling \c MegaChatApi::isSignalActivityRequired.
 * While the is in background status, without user's activity, there is no need tosignal it.
 *
 * - Persist: if enabled, the online status will be preserved, even if user goes offline or closes the app
 *
 * - Last-green visibility: if enabled, the last-time the user was seen as MegaChatApi::STATUS_ONLINE will
 * be retrievable by other users. If disabled, it's kept secret.
 *
 * @note The last-green visibility can be changed by MegaChatApi::setLastGreenVisible and can be checked by
 * MegaChatPresenceConfig::isLastGreenVisible. The last-green time for other users can be retrieved
 * by MegaChatApi::requestLastGreen.
 * @note While the last-green visibility is disabled, the last-green time will not be recorded by the server.
 *
 * - Pending: if true, it means the configuration is being saved in the server, but not confirmed yet
 *
 * @note When the online status is pending, apps may notice showing a blinking status or similar.
 */
class MegaChatPresenceConfig
{
public:
    virtual ~MegaChatPresenceConfig() {}

    /**
     * @brief Creates a copy of this MegaChatPresenceConfig object
     *
     * The resulting object is fully independent of the source MegaChatPresenceConfig,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatRequest object
     */
    virtual MegaChatPresenceConfig *copy() const;

    /**
     * @brief Get the online status specified in the settings
     *
     * It can be one of the following values:
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_AWAY = 2
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 3
     * The user is connected and online.
     *
     * - MegaChatApi::STATUS_BUSY = 4
     * The user is busy and don't want to be disturbed.
     */
    virtual int getOnlineStatus() const;

    /**
     * Whether the autoaway setting is enabled or disabled. Note
     * that the option can be enabled, but the auto-away mechanism
     * can be inactive. I.e. when the status is not online or the user
     * has enabled the persistence of the status.
     *
     * @see \c MegaChatPresenceConfig::isPersist
     *
     * @return True if the user will be away after a timeout.
     */
    virtual bool isAutoawayEnabled() const;

    /**
     * @return Number of seconds to change the online status to away
     */
    virtual int64_t getAutoawayTimeout() const;

    /**
     * @return True if the online status will persist after going offline and/or closing the app
     */
    virtual bool isPersist() const;

    /**
     * @return True if the presence configuration is pending to be confirmed by server
     */
    virtual bool isPending() const;

    /**
     * @return True if our last green is visible to other users
     */
    virtual bool isLastGreenVisible() const;
};

/**
 * @brief Interface to receive SDK logs
 *
 * You can implement this class and pass an object of your subclass to MegaChatApi::setLoggerObject
 * to receive SDK logs. You will have to use also MegaChatApi::setLogLevel to select the level of
 * the logs that you want to receive.
 *
 */
class MegaChatLogger
{
public:
    /**
     * @brief This function will be called with all logs with level <= your selected
     * level of logging (by default it is MegaChatApi::LOG_LEVEL_INFO)
     *
     * The SDK retains the ownership of this string, it won't be valid after this funtion returns.
     *
     * @param loglevel Log level of this message
     *
     * Valid values are:
     * - MegaChatApi::LOG_LEVEL_ERROR   = 1
     * - MegaChatApi::LOG_LEVEL_WARNING = 2
     * - MegaChatApi::LOG_LEVEL_INFO    = 3
     * - MegaChatApi::LOG_LEVEL_VERBOSE = 4
     * - MegaChatApi::LOG_LEVEL_DEBUG   = 5
     * - MegaChatApi::LOG_LEVEL_MAX     = 6
     *
     * @param message Log message
     *
     * The SDK retains the ownership of this string, it won't be valid after this funtion returns.
     *
     */
    virtual void log(int loglevel, const char *message);
    virtual ~MegaChatLogger(){}
};

/**
 * @brief Provides information about an error
 */
class MegaChatError
{
public:
    enum {
        ERROR_OK        =   0,
        ERROR_UNKNOWN   =  -1,		// internal error
        ERROR_ARGS      =  -2,		// bad arguments
        ERROR_TOOMANY   =  -6,		// too many uses for this resource
        ERROR_NOENT     =  -9,		// resource does not exist
        ERROR_ACCESS    = -11,		// access denied
        ERROR_EXIST     = -12		// resource already exists
    };

    MegaChatError() {}
    virtual ~MegaChatError() {}

    virtual MegaChatError *copy() = 0;

    /**
     * @brief Returns the error code associated with this MegaChatError
     * @return Error code associated with this MegaChatError
     */
    virtual int getErrorCode() const = 0;

    /**
     * @brief Returns the type of the error associated with this MegaChatError
     * @return Type of the error associated with this MegaChatError
     */
    virtual int getErrorType() const = 0;

    /**
     * @brief Returns a readable description of the error
     *
     * @return Readable description of the error
     */
    virtual const char* getErrorString() const = 0;

    /**
     * @brief Returns a readable description of the error
     *
     * This function provides exactly the same result as MegaChatError::getErrorString.
     * It's provided for a better Java compatibility
     *
     * @return Readable description of the error
     */
    virtual const char* toString() const = 0;
};

/**
 * @brief Allows to manage the chat-related features of a MEGA account
 *
 * You must provide an appKey to use this SDK. You can generate an appKey for your app for free here:
 * - https://mega.nz/#sdk
 *
 * To properly initialize the chat engine and start using the chat features, you should follow this sequence:
 *     1. Create an object of MegaApi class (see https://github.com/meganz/sdk/tree/master#usage)
 *     2. Create an object of MegaChatApi class: passing the MegaApi instance to the constructor,
 * so the chat SDK can create its client and register listeners to receive the own handle, list of users and chats
 *     3. Call MegaChatApi::init() to initialize the chat engine.
 *         [at this stage, if the app provided a session id, it can retrieve chatrooms and can operate in offline mode (INIT_HAS_OFFLINE_SESSION)]
 *         [If the app provided a email+pwd, it will enter into INIT_WAITING_NEW_SESSION and needs to wait for INIT_ONLINE_SESSION]
 *     4. Call MegaApi::login() and wait for completion
 *     5. Call MegaApi::fetchnodes() and wait for completion
 *         [at this stage, cloud storage apps are ready, but chat-engine is offline]
 *     6. The app is ready to operate when the callback onChatInitStateUpdate() notifies a valid
 *          session: INIT_ONLINE_SESSION
 *
 * Important considerations:
 *  - In order to logout from the account, the app should call MegaApi::logout before MegaChatApi::logout.
 *  - The instance of MegaChatApi must be deleted before the instance of MegaApi passed to the constructor.
 *  - In case we have init session in anonymous mode the app should call MegaChatApi::logout manually.
 *
 * In order to initialize in anonymous mode, the app will skip the steps 1, 4 and 5, but needs to perform steps 2, 3 and
 * 6 accordingly (but replacing the call to MegaChatApi::init by MegaChatApi::initAnonymous).
 *
 * Some functions in this class return a pointer and give you the ownership. In all of them, memory allocations
 * are made using new (for single objects) and new[] (for arrays) so you should use delete and delete[] to free them.
 */
class MegaChatApi
{

public:
    enum {
        STATUS_OFFLINE    = 1,      /// Can be used for invisible mode
        STATUS_AWAY       = 2,      /// User is not available
        STATUS_ONLINE     = 3,      /// User is available
        STATUS_BUSY       = 4,      /// User don't expect notifications nor call requests
        STATUS_INVALID    = 15      /// Invalid value. Presence not received yet
    };

    enum
    {
        //0 is reserved to overwrite completely disabled logging. Used only by logger itself
        LOG_LEVEL_ERROR     = 1,    /// Error information but will continue application to keep running.
        LOG_LEVEL_WARNING   = 2,    /// Information representing errors in application but application will keep running
        LOG_LEVEL_INFO      = 3,    /// Mainly useful to represent current progress of application.
        LOG_LEVEL_VERBOSE   = 4,    /// More information than the usual logging mode
        LOG_LEVEL_DEBUG     = 5,    /// Informational logs, that are useful for developers. Only applicable if DEBUG is defined.
        LOG_LEVEL_MAX       = 6     /// Maximum level of informational logs
    };

    enum
    {
        SOURCE_ERROR    = -1,
        SOURCE_NONE     = 0,
        SOURCE_LOCAL,
        SOURCE_REMOTE
    };

    enum
    {
        INIT_ERROR                  = -1,   /// Initialization failed --> disable chat
        INIT_NOT_DONE               = 0,    /// Initialization not done yet
        INIT_WAITING_NEW_SESSION    = 1,    /// No \c sid provided at init() --> force a login+fetchnodes
        INIT_OFFLINE_SESSION        = 2,    /// Initialization successful for offline operation
        INIT_ONLINE_SESSION         = 3,    /// Initialization successful for online operation --> login+fetchnodes completed
        INIT_ANONYMOUS              = 4,    /// Initialization successful for anonymous operation
        INIT_TERMINATED             = 5,    /// Client is disconnected and terminated
        INIT_NO_CACHE               = 7,    /// Cache not available for \c sid provided --> it requires login+fetchnodes
    };

    enum
    {
        DISCONNECTED    = 0,    /// No connection established
        CONNECTING      = 1,    /// A call to connect() is in progress
        CONNECTED       = 2     /// A call to connect() succeed
    };

    enum
    {
        CHAT_CONNECTION_OFFLINE     = 0,    /// No connection to chatd, offline mode
        CHAT_CONNECTION_IN_PROGRESS = 1,    /// Establishing connection to chatd
        CHAT_CONNECTION_LOGGING     = 2,    /// Connected to chatd, logging in (not ready to send/receive messages, etc)
        CHAT_CONNECTION_ONLINE      = 3     /// Connection with chatd is ready and logged in
    };

    enum
    {
        DB_ERROR_UNEXPECTED         = -1,   /// Unexpected database error (not received by apps, just for internal use)
        DB_ERROR_IO                 = 1,    /// I/O error in Data base    (non recoverable)
        DB_ERROR_FULL               = 2,    /// Database or disk is full  (non recoverable)
    };

    enum
    {
        CHAT_TYPE_ALL             = 0,  /// All chats types
        CHAT_TYPE_INDIVIDUAL      = 1,  /// 1on1 chats
        CHAT_TYPE_GROUP           = 2,  /// Group chats, public and private ones (non meeting rooms)
        CHAT_TYPE_GROUP_PRIVATE   = 3,  /// Private group chats (non meeting rooms)
        CHAT_TYPE_GROUP_PUBLIC    = 4,  /// Public group chats  (non meeting rooms)
        CHAT_TYPE_MEETING_ROOM    = 5,  /// Meeting rooms
        CHAT_TYPE_NON_MEETING     = 6,  /// Non meeting rooms (1on1 and groupchats public and private ones)
    };

    enum
    {
        CHAT_OPTION_EMPTY            = 0x00,   /// Empty chat options
        CHAT_OPTION_SPEAK_REQUEST    = 0x01,   /// Speak request
        CHAT_OPTION_WAITING_ROOM     = 0x02,   /// Waiting room
        CHAT_OPTION_OPEN_INVITE      = 0x04,   /// Open invite
    };

    enum
    {
        CHAT_FILTER_BY_NO_FILTER                = 0,
        CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP      = 1,
        CHAT_FILTER_BY_PUBLIC_OR_PRIVATE        = 2,
        CHAT_FILTER_BY_MEETING_OR_NON_MEETING   = 4,
        CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED = 8,
        CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE     = 16,
        CHAT_FILTER_BY_READ_OR_UNREAD           = 32,
    };

    enum
    {
        CHAT_GET_INDIVIDUAL = 1,  CHAT_GET_GROUP        = 0,
        CHAT_GET_PUBLIC     = 2,  CHAT_GET_PRIVATE      = 0,
        CHAT_GET_MEETING    = 4,  CHAT_GET_NON_MEETING  = 0,
        CHAT_GET_ARCHIVED   = 8,  CHAT_GET_NON_ARCHIVED = 0,
        CHAT_GET_ACTIVE     = 16, CHAT_GET_NON_ACTIVE   = 0,
        CHAT_GET_READ       = 32, CHAT_GET_UNREAD       = 0,
    };

    // SFUID default value. API will start calls in SFU server it consider
    static constexpr int SFU_ID_DEFAULT = -1;

    // chat will reuse an existent megaApi instance (ie. the one for cloud storage)
    /**
     * @brief Creates an instance of MegaChatApi to access to the chat-engine.
     *
     * @param megaApi Instance of MegaApi to be used by the chat-engine.
     */
    MegaChatApi(mega::MegaApi *megaApi);

    virtual ~MegaChatApi();

    static const char *getAppDir();

    /**
     * @brief Set a MegaChatLogger implementation to receive SDK logs
     *
     * Logs received by this objects depends on the active log level.
     * By default, it is MegaChatApi::LOG_LEVEL_INFO. You can change it
     * using MegaChatApi::setLogLevel.
     *
     * The logger object can be removed by passing NULL as \c megaLogger.
     *
     * @param megaLogger MegaChatLogger implementation. NULL to remove the existing object.
     */
    static void setLoggerObject(MegaChatLogger *megaLogger);

    /**
     * @brief Set the active log level
     *
     * This function sets the log level of the logging system. If you set a log listener using
     * MegaApi::setLoggerObject, you will receive logs with the same or a lower level than
     * the one passed to this function.
     *
     * @param logLevel Active log level
     *
     * Valid values are:
     * - MegaChatApi::LOG_LEVEL_ERROR   = 1
     * - MegaChatApi::LOG_LEVEL_WARNING = 2
     * - MegaChatApi::LOG_LEVEL_INFO    = 3
     * - MegaChatApi::LOG_LEVEL_VERBOSE = 4
     * - MegaChatApi::LOG_LEVEL_DEBUG   = 5
     * - MegaChatApi::LOG_LEVEL_MAX     = 6
     */
    static void setLogLevel(int logLevel);

    /**
     * @brief Enable the usage of colouring for logging in the console
     *
     * Karere library uses ANSI escape codes to color messages in the log when they
     * are printed in a terminal. However, sometimes the terminal doesn't support those
     * codes, resulting on weird characters at the beggining of each line.
     *
     * By default, colors are disabled.
     *
     * @param useColors True to enable them, false to disable.
     */
    static void setLogWithColors(bool useColors);

    /**
     * @brief Enable the logging in the console
     *
     * By default, logging to console is enabled.
     *
     * @param enable True to enable it, false to disable.
     */
    static void setLogToConsole(bool enable);

    /**
     * @brief Initializes karere
     *
     * If no session is provided, karere will listen to the fetchnodes event in order to register
     * a new session and create its cache. It will return MegaChatApi::INIT_WAITING_NEW_SESSION.
     *
     * If a session id is provided, karere will try to resume the session from its cache and will
     * return MegaChatApi::INIT_OFFLINE_SESSION.
     *
     * If a session id is provided but the correspoding cache is not available, it will return
     * MegaChatApi::INIT_NO_CACHE and the app should go through a login + fetchnodes in order to
     * re-create a new cache from scratch. No need to invalidate the SDK's cache, MEGAchat's cache
     * will be regenerated based on data from SDK's cache upong fetchnodes completion.
     *
     * The initialization status is notified via `MegaChatListener::onChatInitStateUpdate`. See
     * the documentation of the callback for possible values.
     *
     * This function should be called before MegaApi::login and MegaApi::fetchnodes.
     *
     * In case of background services, like CameraUploads or NSE, the init() should be followed by
     * a call to MegaChatApi::setBackgroundStatus(true), so the client connects to chatd/presenced
     * signalling the background state appropriately.
     *
     * @param sid Session id that wants to be resumed, or NULL if a new session will be created.
     * @return The initialization state
     */
    int init(const char *sid);

    /**
     * @brief Initializes karere in Lean Mode
     *
     * In Lean Mode, the app may skip the fetchnodes step after login.
     * MEGAchat will not wait for the completion of fetchnodes. It will resume the cached
     * state from persistent storage.
     *
     * @note This mode is required by iOS Notification Service Extension (NSE). The extension restricts
     * the amount of memory used by the app. In order to avoid OOM errors, the iOS app may use this mode
     * to skip the fetchnodes and, consequently, save some bytes by not loading all the nodes of the
     * account in memory.
     *
     * If a session id is provided, karere will try to resume the session from its cache and will
     * return MegaChatApi::INIT_OFFLINE_SESSION. Since a fetchnodes is not requires for this mode,
     * the app should not expect a transition to MegaChatApi::INIT_ONLINE_SESION.
     *
     * If no session is provided, or if it is provided but the correspoding cache is not available,
     * it will return MegaChatApi::INIT_ERROR. No Lean Mode will be available in that case.
     *
     * The initialization status is notified via `MegaChatListener::onChatInitStateUpdate`. See
     * the documentation of the callback for possible values.
     *
     * This function should be called before MegaApi::login.
     *
     * @param sid Session id that wants to be resumed.
     * @return The initialization state
     */
    int initLeanMode(const char *sid);

    /**
     * @brief Import messages from an external DB
     *
     * This method allows to import messages from an external cache. The cache should be a copy
     * of the app's cache, but may include new messages that wants to be imported into the app's
     * cache in one shot. In case the history has been truncated, this method applies truncation.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_IMPORT_MESSAGES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getText - Returns the cache path
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getNumber - Total number of messages added/updated
     *
     * @note This mode is required by iOS Notification Service Extension (NSE). The extension runs
     * separately from iOS app, with its independent cache.
     *
     * The request will fail with MegaChatError::ERROR_ACCESS when this function is
     * called without a previous call to \c MegaChatApi::init or when the initialization
     * state is other than MegaChatApi::INIT_OFFLINE_SESSION or MegaChatApi::INIT_ONLINE_SESSION.
     *
     * @param externalDbPath path of the external BD
     * @param listener MegaChatRequestListener to track this request
     */
    void importMessages(const char *externalDbPath, MegaChatRequestListener *listener = nullptr);

    /**
     * @brief Reset the Client Id for chatd
     *
     * When the app is running and another instance is launched i.e (share-extension in iOS),
     * chatd closes the connection if a new connection is established with the same Client Id.
     *
     * The purpose of this function is reset the Client Id in order to avoid that chatd closes
     * the other connections.
     *
     * This function should be called after MegaChatApi::init.
     */
    void resetClientid();

    /**
     * @brief Initializes karere in anonymous mode for preview of chat-links
     *
     * The initialization state will be MegaChatApi::INIT_ANONYMOUS if successful. In
     * case of initialization error, it will return MegaChatApi::INIT_ERROR.
     *
     * This function should be called to preview chat-links without a valid session (anonymous mode).
     *
     * @note The app will not call MegaApi::login nor MegaApi::fetchnodes.
     *
     * The anonymous mode is going to initialize the chat engine but is not going to login in MEGA,
     * so the way to logout in anoymous mode is call MegaChatApi::logout manually.
     *
     * @return The initialization state
     */
    int initAnonymous();

    /**
     * @brief Returns the current initialization state
     *
     * The possible values are:
     *  - MegaChatApi::INIT_ERROR = -1
     *  - MegaChatApi::INIT_NOT_DONE = 0
     *  - MegaChatApi::INIT_WAITING_NEW_SESSION = 1
     *  - MegaChatApi::INIT_OFFLINE_SESSION = 2
     *  - MegaChatApi::INIT_ONLINE_SESSION = 3
     *  - MegaChatApi::INIT_ANONYMOUS = 4
     *  - MegaChatApi::INIT_TERMINATED = 5
     *  - MegaChatApi::INIT_NO_CACHE = 7
     *
     * If \c MegaChatApi::init() has not been called yet, this function returns INIT_NOT_DONE
     *
     * If the chat-engine is being logged out, it returns INIT_TERMINATED.
     *
     * @return The current initialization state
     */
    int getInitState();

    // ============= Requests ================

    /**
     * @brief Returns the current state of the client
     *
     * It can be one of the following values:
     *  - MegaChatApi::DISCONNECTED = 0
     *  - MegaChatApi::CONNECTING   = 1
     *  - MegaChatApi::CONNECTED    = 2
     *
     * @note Even if this function returns CONNECTED, it does not mean the client
     * is fully connected to chatd and presenced.
     * @see MegaChatApi::getChatConnectionState and MegaChatApi::areAllChatsLoggedIn.
     *
     * @return The connection's state of the client
     */
    int getConnectionState();

    /**
     * @brief Returns the current state of the connection to chatd for a given chatroom
     *
     * The possible values are:
     *  - MegaChatApi::CHAT_CONNECTION_OFFLINE      = 0
     *  - MegaChatApi::CHAT_CONNECTION_IN_PROGRESS  = 1
     *  - MegaChatApi::CHAT_CONNECTION_LOGGING      = 2
     *  - MegaChatApi::CHAT_CONNECTION_ONLINE       = 3
     *
     * You can check if all chats are online with MegaChatApi::areAllChatsLoggedIn.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return The state of connection
     */
    int getChatConnectionState(MegaChatHandle chatid);
    
    /**
     * @brief Check whether client is logged in into all chats
     *
     * @return True if connection to chatd is MegaChatApi::CHAT_CONNECTION_ONLINE, false otherwise.
     */
    bool areAllChatsLoggedIn();

    /**
     * @brief Refresh DNS servers and retry pending connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true if force disconnect
     * - MegaChatRequest::getParamType - Returns one for refresh url
     *
     * @param disconnect False to simply abort any backoff, true to disconnect and reconnect from scratch.
     * @param listener MegaChatRequestListener to track this request
     */
    void retryPendingConnections(bool disconnect = false, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Refresh URLs and establish fresh connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     *
     * A disconnect will be forced automatically, followed by a reconnection to the fresh URLs
     * retrieved from API. This parameter is useful when the URL for the API is changed
     * via MegaApi::changeApiUrl.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void refreshUrl(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Logout of chat servers invalidating the session
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOGOUT.
     *
     * The request will fail with MegaChatError::ERROR_ACCESS when this function is
     * called without a previous call to \c MegaChatApi::init or when MEGAchat is already
     * logged out.
     *
     * @note MEGAchat automatically logs out when it detects the MegaApi instance has an
     * invalid session id. No need to call it explicitely, except to disable the chat.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void logout(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Logout of chat servers without invalidating the session
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOGOUT
     *
     * After calling \c localLogout, the subsequent call to MegaChatApi::init expects to
     * have an already existing session created by MegaApi::fastLogin(session)
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void localLogout(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Set your configuration for online status.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHAT_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getNumber - Returns the new status of the user in chat.
     *
     * The request will fail with MegaChatError::ERROR_ARGS when this function is
     * called with the same value \c status than the currently cofigured status.
     * @see MegaChatPresenceConfig::getOnlineStatus to check the current status.
     *
     * The request will fail with MegaChatError::ERROR_ACCESS when this function is
     * called and the connection to presenced is down.
     *
     * @param status Online status in the chat.
     *
     * It can be one of the following values:
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_AWAY = 2
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 3
     * The user is connected and online.
     *
     * - MegaChatApi::STATUS_BUSY = 4
     * The user is busy and don't want to be disturbed.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void setOnlineStatus(int status, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable/disable the autoaway option, with one specific timeout
     *
     * When autoaway is enabled and persist is false, the app should call to
     * \c signalPresenceActivity regularly in order to keep the current online status.
     * Otherwise, after \c timeout seconds, the online status will be changed to away.
     *
     * The maximum timeout for the autoaway feature is 87420 seconds, roughly a day.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true if autoaway is enabled.
     * - MegaChatRequest::getNumber - Returns the specified timeout.
     *
     * The request will fail with MegaChatError::ERROR_ARGS when this function is
     * called with a larger timeout than the maximum allowed, 87420 seconds.
     *
     * @param enable True to enable the autoaway feature
     * @param timeout Seconds to wait before turning away (if no activity has been signalled)
     * @param listener MegaChatRequestListener to track this request
     */
    void setPresenceAutoaway(bool enable, int64_t timeout, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable/disable the persist option
     *
     * When this option is enable, the online status shown to other users will be the
     * one specified by the user, even when you are disconnected.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRESENCE_PERSIST
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true if presence status is persistent.
     *
     * @param enable True to enable the persist feature
     * @param listener MegaChatRequestListener to track this request
     */
    void setPresencePersist(bool enable, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable/disable the visibility of when the logged-in user was online (green)
     *
     * If this option is disabled, the last-green won't be available for other users when it is
     * requested through MegaChatApi::requestLastGreen. The visibility is enabled by default.
     *
     * While this option is disabled and the user sets the green status temporary, the number of
     * minutes since last-green won't be updated. Once enabled back, the last-green will be the
     * last-green while the visibility was enabled (or updated if the user sets the green status).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_LAST_GREEN_VISIBLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true when attempt to enable visibility of last-green.
     *
     * @param enable True to enable the visibility of our last green
     * @param listener MegaChatRequestListener to track this request
     */
    void setLastGreenVisible(bool enable, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request the number of minutes since the user was seen as green by last time.
     *
     * Apps may call this function to retrieve the minutes elapsed since the user was seen
     * as green (MegaChatApi::STATUS_ONLINE) by last time.
     * Apps must NOT call this function if the current status of the user is already green.
     *
     * The number of minutes since the user was seen as green by last time, if any, will
     * be notified in the MegaChatListener::onChatPresenceLastGreen callback. Note that,
     * if the user was never seen green by presenced or the user has disabled the visibility
     * of the last-green with MegaChatApi::setLastGreenVisible, there will be no notification
     * at all.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LAST_GREEN
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle() - Returns the handle of the user
     *
     * @param userid MegaChatHandle from user that last green has been requested
     * @param listener MegaChatRequestListener to track this request
     */
    void requestLastGreen(MegaChatHandle userid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Signal there is some user activity
     *
     * When the presence configuration is set to autoaway (and persist is false), this
     * function should be called regularly to not turn into away status automatically.
     *
     * A good approach is to call this function with every mouse move or keypress on desktop
     * platforms; or at any finger tap or gesture and any keypress on mobile platforms.
     *
     * Failing to call this function, you risk a user going "Away" while typing a lengthy message,
     * which would be awkward.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SIGNAL_ACTIVITY.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void signalPresenceActivity(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Get your currently online status.
     *
     * @note This function may return a different online status than the online status from
     * MegaChatPresenceConfig::getOnlineStatus. In example, when the user has configured the
     * autoaway option, after the timeout has expired, the status will be Away instead of Online.
     *
     * It can be one of the following values:
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_AWAY = 2
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 3
     * The user is connected and online.
     *
     * - MegaChatApi::STATUS_BUSY = 4
     * The user is busy and don't want to be disturbed.
     */
    int getOnlineStatus();

    /**
     * @brief Check if the online status is already confirmed by the server
     *
     * When a new online status is requested by MegaChatApi::setOnlineStatus, it's not
     * immediately set, but sent to server for confirmation. If the status is not confirmed
     * the requested online status will not be seen by other users yet.
     *
     * The apps may use this function to indicate the status is not confirmed somehow, like
     * with a slightly different icon, blinking or similar.
     *
     * @return True if the online status is confirmed by server
     */
    bool isOnlineStatusPending();

    /**
     * @brief Get the current presence configuration
     *
     * You take the ownership of the returned value
     *
     * @see \c MegaChatPresenceConfig for further details.
     *
     * @return The current presence configuration, or NULL if not received yet from server
     */
    MegaChatPresenceConfig *getPresenceConfig();

    /**
     * @brief Returns whether the autoaway mechanism is active.
     *
     * @note This function may return false even when the Presence settings
     * establish that autoaway option is active. It happens when the persist
     * option is enabled and when the status is offline or away.
     *
     * @return True if the app should call \c MegaChatApi::signalPresenceActivity
     */
    bool isSignalActivityRequired();

    /**
     * @brief Get the online status of a user.
     *
     * It can be one of the following values:
     *
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_AWAY = 2
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 3
     * The user is connected and online.
     *
     * - MegaChatApi::STATUS_BUSY = 4
     * The user is busy and don't want to be disturbed.
     *
     * @param userhandle Handle of the peer whose name is requested.
     * @return Online status of the user
     */
    int getUserOnlineStatus(MegaChatHandle userhandle);

    /**
     * @brief Set the status of the app
     *
     * Apps in mobile devices can be in different status. Typically, foreground and
     * background. The app should define its status in order to receive notifications
     * from server when the app is in background.
     *
     * This function doesn't have any effect until MEGAchat is fully initialized (meaning that
     * MegaChatApi::getInitState returns the value MegaChatApi::INIT_OFFLINE_SESSION or
     * MegaChatApi::INIT_ONLINE_SESSION).
     *
     * If MEGAchat is currently not connected to chatd, the request will fail with a
     * MegaChatError::ERROR_ACCESS. If that case, when transitioning from foreground to
     * background, the app should wait for being reconnected (@see MegaChatListener::onChatConnectionStateUpdate)
     * in order to ensure the server is aware of the new status of the app, specially in iOS where
     * the OS may kill the connection.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_BACKGROUND_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns the value of 1st parameter
     *
     * @param background True if the the app is in background, false if in foreground.
     */
    void setBackgroundStatus(bool background, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the background status established in MEGAchat
     *
     * This function will return -1 when MEGAchat is not fully initialized. It requires that
     * MegaChatApi::getInitState returns the value MegaChatApi::INIT_OFFLINE_SESSION or
     * MegaChatApi::INIT_ONLINE_SESSION.
     *
     * @return 0 for foreground, 1 for background, -1 if not fully initialized
     */
    int getBackgroundStatus();

    /**
     * @brief Returns the current firstname of the user
     *
     * This function is useful to get the firstname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_FIRSTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     * - MegaChatRequest::getLink - Returns the authorization token. Previewers of chatlinks are not allowed
     * to retrieve user attributes like firstname or lastname, unless they provide a valid authorization token.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the firstname of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param authorizationToken This value can be obtained with MegaChatRoom::getAuthorizationToken
     * @param listener MegaChatRequestListener to track this request
     */
    void getUserFirstname(MegaChatHandle userhandle, const char *authorizationToken, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current firstname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose first name is requested.
     * @return The first name from user
     */
    const char* getUserFirstnameFromCache(MegaChatHandle userhandle);

    /**
     * @brief Returns the current lastname of the user
     *
     * This function is useful to get the lastname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_LASTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     * - MegaChatRequest::getLink - Returns the authorization token. Previewers of chatlinks are not allowed
     * to retrieve user attributes like firstname or lastname, unless they provide a valid authorization token.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the lastname of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param authorizationToken This value can be obtained with MegaChatRoom::getAuthorizationToken
     * @param listener MegaChatRequestListener to track this request
     */
    void getUserLastname(MegaChatHandle userhandle, const char *authorizationToken, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current lastname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose last name is requested.
     * @return The last name from user
     */
    const char* getUserLastnameFromCache(MegaChatHandle userhandle);

    /**
     * @brief Returns the current fullname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose fullname is requested.
     * @return The full name from user
     */
    const char* getUserFullnameFromCache(MegaChatHandle userhandle);

    /**
     * @brief Returns the current email address of the contact
     *
     * This function is useful to get the email address of users you are NOT contact with.
     * Note that for any other user without contact relationship, this function will return NULL.
     *
     * You take the ownership of the returned value
     *
     * This function is useful to get the email address of users who participate in a groupchat with
     * you but are not your contacts.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_EMAIL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the email address of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    void getUserEmail(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current email address of the user
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose email is requested.
     * @return The email from user
     */
    const char* getUserEmailFromCache(MegaChatHandle userhandle);

    /**
     * @brief Returns the known alias given to the user
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose alias is requested.
     * @return The alias from user
     */
    const char *getUserAliasFromCache(MegaChatHandle userhandle);

    /**
     * @brief Returns all the known aliases
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @return The list of aliases
     */
    ::mega::MegaStringMap *getUserAliasesFromCache();

    /**
     * @brief request to server user attributes
     *
     * This function is useful to get the email address, first name, last name and full name
     * from chat link participants that they are not loaded
     *
     * After request is finished, you can call to MegaChatApi::getUserFirstnameFromCache,
     * MegaChatApi::getUserLastnameFromCache, MegaChatApi::getUserFullnameFromCache,
     * MegaChatApi::getUserEmailFromCache (email will not available in anonymous mode)
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_PEER_ATTRIBUTES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of chat
     * - MegaChatRequest::getMegaHandleList - Returns the handles of user that attributes have been requested
     *
     * @param chatid Handle of the chat whose member attributes requested
     * @param userList List of user whose attributes has been requested
     * @param listener MegaChatRequestListener to track this request
     */
    void loadUserAttributes(MegaChatHandle chatid, mega::MegaHandleList *userList, MegaChatRequestListener *listener = nullptr);

    /**
     * @brief Maximum number of participants in a chat whose attributes are automatically fetched
     *
     * For very large chatrooms, the user attributes that are usually pre-loaded automatically by MEGAchat
     * are not loaded. Instead, the app needs to call MegaChatApi::loadUserAttributes in order to request them.
     * Once the request finishes, attributes like the firstname or the email will be available through the getters,
     * like MegaChatApi::getUserFirstnameFromCache and alike.
     * @return Maximun number of member in public chat which attributes are requested automatically
     */
    unsigned int getMaxParticipantsWithAttributes();

    /**
     * @brief Returns the current email address of the contact
     *
     * This function is useful to get the email address of users you are contact with and users
     * you were contact with in the past and later on the contact relationship was broken.
     * Note that for any other user without contact relationship, this function will return NULL.
     *
     * You take the ownership of the returned value
     *
     * @param userhandle Handle of the user whose name is requested.
     * @return The email address of the contact, or NULL if not found.
     */
    char *getContactEmail(MegaChatHandle userhandle);

    /**
     * @brief Returns the userhandle of the contact
     *
     * This function is useful to get the handle of users you are contact with and users
     * you were contact with in the past and later on the contact relationship was broken.
     * Note that for any other user without contact relationship, this function will return
     * MEGACHAT_INVALID_HANDLE.
     *
     * @param email Email address of the user whose handle is requested.
     * @return The userhandle of the contact, or MEGACHAT_INVALID_HANDLE if not found.
     */
    MegaChatHandle getUserHandleByEmail(const char *email);

    /**
     * @brief Returns the handle of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * @return Own user handle
     */
    MegaChatHandle getMyUserHandle();

    /**
     * @brief Returns the client id handle of the logged in user for a chatroom
     *
     * The clientId is not the same for all chatrooms. If \c chatid is invalid, this function
     * returns 0
     *
     * In offline mode (MegaChatApi::INIT_OFFLINE_SESSION), this function returns 0
     *
     * @return Own client id handle
     */
    MegaChatHandle getMyClientidHandle(MegaChatHandle chatid);

    /**
     * @brief Returns the firstname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user firstname
     */
    char *getMyFirstname();

    /**
     * @brief Returns the lastname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user lastname
     */
    char *getMyLastname();

    /**
     * @brief Returns the fullname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user fullname
     */
    char *getMyFullname();

    /**
     * @brief Returns the email of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user email
     */
    char *getMyEmail();


    /**
     * @brief Get all chatrooms (1on1 and groupal) of this MEGA account
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatRoom objects with all chatrooms of this account.
     */
    MegaChatRoomList *getChatRooms();

    /**
     * @brief Returns a list of chatrooms of this MEGA account filtered by type
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * @param type Type of the chatrooms returned by this method.
     * Valid values for param type are:
     * - MegaChatApi::CHAT_TYPE_ALL             = 0,  /// All chats types
     * - MegaChatApi::CHAT_TYPE_INDIVIDUAL      = 1,  /// 1on1 chats
     * - MegaChatApi::CHAT_TYPE_GROUP           = 2,  /// Group chats, public and private ones (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PRIVATE   = 3,  /// Private group chats (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PUBLIC    = 4,  /// Public group chats  (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_MEETING_ROOM    = 5,  /// Meeting rooms
     * - MegaChatApi::CHAT_TYPE_NON_MEETING     = 6,  /// Non meeting rooms (1on1 and groupchats public and private ones)
     *
     * In case you provide an invalid value for type param, this method will returns an empty list
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatRoom objects filtered by type of this account.
     */
    MegaChatRoomList* getChatRoomsByType(int type = CHAT_TYPE_ALL);

    /**
     * @brief Get the MegaChatRoom that has a specific handle
     *
     * You can get the handle of a MegaChatRoom using MegaChatRoom::getChatId or
     * MegaChatListItem::getChatId.
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatRoom object for the specified \c chatid
     */
    MegaChatRoom *getChatRoom(MegaChatHandle chatid);

    /**
     * @brief Get the MegaChatRoom for the 1on1 chat with the specified user
     *
     * If the 1on1 chat with the user specified doesn't exist, this function will
     * return NULL.
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @param userhandle MegaChatHandle that identifies the user
     * @return MegaChatRoom object for the specified \c userhandle
     */
    MegaChatRoom *getChatRoomByUser(MegaChatHandle userhandle);

    /**
     * @brief Get all chatrooms (individual and groupal) with limited information
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * You take the ownership of the returned value
     *
     * @param mask represents what filters to apply to the list of chats
     * - To apply individual/group filter use       CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP
     * - To apply public/private filter use         CHAT_FILTER_BY_PUBLIC_OR_PRIVATE
     * - To apply meeting/non-meeting filter use    CHAT_FILTER_BY_MEETING_OR_NON_MEETING
     * - To apply archived/non-archived filter use  CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
     * - To apply active/non-active filter use      CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE
     * - To apply read/unread (messages) filter use CHAT_FILTER_BY_READ_OR_UNREAD
     * To combine different masks in any order use the + operator
     * CHAT_FILTER_BY_NO_FILTER mask should be used alone and it will ignore any value in the param filter
     *
     * @param filter represents the values to apply in the filter
     * To select individual chats use   CHAT_GET_INDIVIDUAL
     * To select group chats use        CHAT_GET_GROUP
     * To select public chats use       CHAT_GET_PUBLIC
     * To select private chats use      CHAT_GET_PRIVATE
     * To select meeting chats use      CHAT_GET_MEETING
     * To select non-meeting chats use  CHAT_GET_NON_MEETING
     * To select archived chats use     CHAT_GET_ARCHIVED
     * To select non-archived chats use CHAT_GET_NON_ARCHIVED
     * To select active chats use       CHAT_GET_ACTIVE
     * To select non-active chats use   CHAT_GET_NON_ACTIVE
     * To select read chats use         CHAT_GET_READ
     * To select unread chats use       CHAT_GET_UNREAD
     * Multiple conditions in any order may be applied using the + operator to combine them
     *
     * In case you provide an invalid filter (i.e. combination of mask and filter params), this function
     * returns an empty list
     *
     * @return List of MegaChatListItemList objects with all chatrooms of this account.
     */
    MegaChatListItemList* getChatListItems(const int mask, const int filter) const;

    /**
     * @brief Get all chatrooms (1on1 and groupal) with limited information
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * This function filters out archived chatrooms. You can retrieve them by using
     * the function \c getArchivedChatListItems.
     *
     * You take the ownership of the returned value
     *
     * @deprecated use getChatListItems instead,
     * @return List of MegaChatListItemList objects with all chatrooms of this account.
     */
    MegaChatListItemList *getChatListItems();

    /**
     * @brief Get all chatrooms (1on1 and groupal) with limited information filtered by type
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * This function filters out archived chatrooms. You can retrieve them by using
     * the function \c getArchivedChatListItems.
     *
     * You take the ownership of the returned value
     *
     * @param type Type of the chatListItems returned by this method.
     * Valid values for param type are:
     * - MegaChatApi::CHAT_TYPE_ALL             = 0,  /// All chats types
     * - MegaChatApi::CHAT_TYPE_INDIVIDUAL      = 1,  /// 1on1 chats
     * - MegaChatApi::CHAT_TYPE_GROUP           = 2,  /// Group chats, public and private ones (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PRIVATE   = 3,  /// Private group chats (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PUBLIC    = 4,  /// Public group chats  (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_MEETING_ROOM    = 5,  /// Meeting rooms
     * - MegaChatApi::CHAT_TYPE_NON_MEETING     = 6,  /// Non meeting rooms (1on1 and groupchats public and private ones)
     *
     * In case you provide an invalid value for type param, this method returns an empty list
     *
     * @deprecated use getChatListItems instead,
     * @return List of MegaChatListItemList objects with all chatrooms of this account filtered by type.
     */
    MegaChatListItemList* getChatListItemsByType(int type = CHAT_TYPE_ALL);

    /**
     * @brief Get all chatrooms (1on1 and groupal) that contains a certain set of participants
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * This function returns even archived chatrooms.
     *
     * You take the ownership of the returned value
     *
     * @param peers MegaChatPeerList that contains the user handles of the chat participants,
     * except our own handle because MEGAchat doesn't include them in the map of members for each chatroom.
     *
     * @return List of MegaChatListItemList objects with the chatrooms that contains a certain set of participants.
     */
    MegaChatListItemList *getChatListItemsByPeers(MegaChatPeerList *peers);

    /**
     * @brief Get the MegaChatListItem that has a specific handle
     *
     * You can get the handle of the chatroom using MegaChatRoom::getChatId or
     * MegaChatListItem::getChatId.
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatListItem object for the specified \c chatid
     */
    MegaChatListItem *getChatListItem(MegaChatHandle chatid);

    /**
     * @brief Return the number of chatrooms with unread messages
     *
     * Archived chatrooms or chatrooms in preview mode with unread messages
     * are not considered.
     *
     * @return The number of chatrooms with unread messages
     */
    int getUnreadChats();

    /**
     * @brief Return the chatrooms that are currently active
     *
     * This function filters out archived chatrooms.
     *
     * You take the onwership of the returned value.
     *
     * @deprecated use getChatListItems instead,
     * @return MegaChatListItemList including all the active chatrooms
     */
    MegaChatListItemList *getActiveChatListItems();

    /**
     * @brief Return the chatrooms that are currently inactive
     *
     * Chatrooms became inactive when you left a groupchat or you are removed by
     * a moderator. 1on1 chats do not become inactive, just read-only.
     *
     * This function filters out archived chatrooms.
     *
     * You take the onwership of the returned value.
     *
     * @deprecated use getChatListItems instead,
     * @return MegaChatListItemList including all the active chatrooms
     */
    MegaChatListItemList *getInactiveChatListItems();

    /**
     * @brief Return the archived chatrooms
     *
     * You take the onwership of the returned value.
     *
     * @deprecated use getChatListItems instead,
     * @return MegaChatListItemList including all the archived chatrooms
     */
    MegaChatListItemList *getArchivedChatListItems();

    /**
     * @brief Return the chatrooms that have unread messages
     *
     * Archived chatrooms with unread messages are not considered.
     *
     * This function filters out archived chatrooms.
     *
     * You take the onwership of the returned value.
     *
     * @deprecated use getChatListItems instead,
     * @return MegaChatListItemList including all the chatrooms with unread messages
     */
    MegaChatListItemList *getUnreadChatListItems();

    /**
     * @brief Get the chat id for the 1on1 chat with the specified user
     *
     * If the 1on1 chat with the user specified doesn't exist, this function will
     * return MEGACHAT_INVALID_HANDLE.
     *
     * @param userhandle MegaChatHandle that identifies the user
     * @return MegaChatHandle that identifies the 1on1 chatroom
     */
    MegaChatHandle getChatHandleByUser(MegaChatHandle userhandle);

    /**
     * @brief Allows to enable/disable the open invite option for a chat room
     * 
     * The open invite option allows users with MegaChatRoom::PRIV_STANDARD privilege, to invite other users into the chat
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHATROOM_OPTIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getPrivilege - Returns MegaChatApi::CHAT_OPTION_OPEN_INVITE
     * - MegaChatRequest::getFlag - Returns true if enabled was set true, otherwise it will return false
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ARGS - If the chatroom is a 1on1 chat
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_EXIST - If the value of enabled is the same as open invite option
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enabled True if we want to enable open invite option, otherwise false.
     * @param listener MegaChatRequestListener to track this request
     */
    void setOpenInvite(MegaChatHandle chatid, bool enabled, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Allows to enable/disable the speak request option for a chat room
     * 
     * If speak request option is enabled, during calls non moderator users, must request permission to speak
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHATROOM_OPTIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getPrivilege - Returns MegaChatApi::CHAT_OPTION_SPEAK_REQUEST
     * - MegaChatRequest::getFlag - Returns true if enabled was set true, otherwise it will return false
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ARGS - If the chatroom is a 1on1 chat
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_EXIST - If the value of enabled is the same as speak request option
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enabled True if we want to enable speak request option, otherwise false.
     * @param listener MegaChatRequestListener to track this request
     */
    void setSpeakRequest(MegaChatHandle chatid, bool enabled, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Allows to enable/disable the waiting room option for a chat room
     * 
     * If waiting room option is enabled, during calls non moderator members, will be placed into a waiting room.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHATROOM_OPTIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getPrivilege - Returns MegaChatApi:::CHAT_OPTION_WAITING_ROOM
     * - MegaChatRequest::getFlag - Returns true if enabled was set true, otherwise it will return false
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ARGS - If the chatroom is a 1on1 chat
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_EXIST - If the value of enabled is the same as waiting room option
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enabled True if we want to enable waiting room, otherwise false.
     * @param listener MegaChatRequestListener to track this request
     */
    void setWaitingRoom(MegaChatHandle chatid, bool enabled, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Creates a chat for one or more participants, allowing you to specify their
     * permissions and if the chat should be a group chat or not (when it is just for 2 participants).
     *
     * There are two types of chat: permanent an group. A permanent chat is between two people, and
     * participants can not leave it.
     *
     * The creator of the chat will have moderator level privilege and should not be included in the
     * list of peers.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns if the new chat is a group chat or permanent chat
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     * - MegaChatError::ERROR_ACCESS - If no peers are provided for a 1on1 chatroom.
     *
     * @note If you are trying to create a chat with more than 1 other person, then it will be forced
     * to be a group chat.
     *
     * @note If peers list contains only one person, group chat is not set and a permament chat already
     * exists with that person, then this call will return the information for the existing chat, rather
     * than a new chat.
     *
     * @param group Flag to indicate if the chat is a group chat or not
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param listener MegaChatRequestListener to track this request
     */
    void createChat(bool group, MegaChatPeerList *peers, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Creates a chat for one or more participants, allowing you to specify their
     * permissions and if the chat should be a group chat or not (when it is just for 2 participants).
     *
     * There are two types of chat: permanent an group. A permanent chat is between two people, and
     * participants can not leave it.
     *
     * The creator of the chat will have moderator level privilege and should not be included in the
     * list of peers.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns if the new chat is a group chat or permanent chat
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     * - MegaChatError::ERROR_ACCESS - If no peers are provided for a 1on1 chatroom.
     *
     * @note If you are trying to create a chat with more than 1 other person, then it will be forced
     * to be a group chat.
     *
     * @note If peers list contains only one person, group chat is not set and a permament chat already
     * exists with that person, then this call will return the information for the existing chat, rather
     * than a new chat.
     *
     * @param group Flag to indicate if the chat is a group chat or not
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param listener MegaChatRequestListener to track this request
     */
    void createChat(bool group, MegaChatPeerList *peers, const char *title, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Creates a groupal chat for one or more participants, allowing you to specify their permissions and creation chat options
     *
     * The creator of the chat will have moderator level privilege and should not be included in the
     * list of peers.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns if the new chat is a group chat or permanent chat
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @note If you are trying to create a chat with more than 1 other person, then it will be forced
     * to be a group chat.
     *
     * @note If peers list contains only one person, group chat is not set and a permament chat already
     * exists with that person, then this call will return the information for the existing chat, rather
     * than a new chat.
     *
     * @param group Flag to indicate if the chat is a group chat or not
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    void createGroupChat(MegaChatPeerList* peers, const char* title,  bool speakRequest, bool waitingRoom, bool openInvite, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Creates an public chatroom for multiple participants (groupchat)
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    void createPublicChat(MegaChatPeerList *peers, const char *title = NULL, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Creates an public chatroom for multiple participants (groupchat) allowing you to specify creation chat options
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    void createPublicChat(MegaChatPeerList* peers, const char* title, bool speakRequest, bool waitingRoom, bool openInvite,  MegaChatRequestListener* listener = NULL);

    /**
     * @brief Creates a meeting
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getNumber - Returns always 1, since the chatroom is a meeting
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    void createMeeting(const char *title = NULL, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Creates a chatroom and a scheduled meeting for that chatroom
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always true as we are going to create a new chatroom
     * - MegaChatRequest::request->getNumber - Returns true if new chat is going to be a Meeting room
     * - MegaChatRequest::request->getPrivilege - Returns true is new chat is going to be a public chat room
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting
     *   (containing the params provided by user), or NULL in case request finished with an error.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting
     * (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if no peerlist is provided
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     * - MegaChatError::ERROR_ARGS  - if isMeeting is set true but publicChat is set to false
     * - MegaChatError::ERROR_ARGS  - if title (Max: 30 characters) or description (Max: 4000 characters) length exceed limits
     * - MegaChatError::ERROR_ACCESS  - if no user privilege is provided or no peers are provided for a group chatroom
     *
     * @param isMeeting True to create a meeting room, otherwise false
     * @param publicChat True to create a public chat, otherwise false
     * @param title Null-terminated character string with the scheduled meeting title. Maximum allowed length is MegaChatScheduledMeeting::MAX_TITLE_LENGTH characters
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param timezone Timezone where we want to schedule the meeting
     * @param startDate start date time of the meeting with the format (unix timestamp UTC)
     * @param endDate end date time of the meeting with the format (unix timestamp UTC)
     * @param description Null-terminated character string with the scheduled meeting description. Maximum allowed length is MegaChatScheduledMeeting::MAX_DESC_LENGTH characters
     * Note that description is a mandatory field, so in case you want to set an empty description, please provide an empty string with Null-terminated character at the end
     * @param flags Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     * @param attributes - not supported yet
     * @param listener MegaChatRequestListener to track this request
     */
    void createChatroomAndSchedMeeting(MegaChatPeerList* peerList, bool isMeeting, bool publicChat, const char* title, bool speakRequest, bool waitingRoom, bool openInvite,
                                                          const char* timezone, MegaChatTimeStamp startDate, MegaChatTimeStamp endDate, const char* description,
                                                          const MegaChatScheduledFlags* flags, const MegaChatScheduledRules* rules,
                                                          const char* attributes = NULL, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Modify an existing scheduled meeting
     *
     * Note: this action won't create a child scheduled meeting
     *
     * The associated request type with this request is MegaChatRequest::TYPE_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getNumber - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getPrivilege - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting
     * (containing the params provided by user), or NULL in case request finished with an error.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting
     * (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid or schedId are invalid
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, description, flags and rules are invalid
     * - MegaChatError::ERROR_ARGS  - if title (Max: 30 characters) or description (Max: 4000 characters) length exceed limits
     * - MegaChatError::ERROR_NOENT - if chatroom or scheduled meeting don't exist
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies the scheduled meeting
     * @param timezone Timezone where we want to schedule the meeting
     * @param startDate start date time of the meeting with the format (unix timestamp UTC)
     * @param endDate end date time of the meeting with the format (unix timestamp UTC)
     * @param title Null-terminated character string with the scheduled meeting title. Maximum allowed length is MegaChatScheduledMeeting::MAX_TITLE_LENGTH characters
     * @param description Null-terminated character string with the scheduled meeting description. Maximum allowed length is MegaChatScheduledMeeting::MAX_DESC_LENGTH characters
     * @param cancelled True if scheduled meeting is going to be cancelled
     * @param flags Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     * @param listener MegaChatRequestListener to track this request
     */
    void updateScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId, const char* timezone, MegaChatTimeStamp startDate, MegaChatTimeStamp endDate,
                                                                         const char* title, const char* description, bool cancelled, const MegaChatScheduledFlags* flags, const MegaChatScheduledRules* rules,
                                                                         MegaChatRequestListener* listener = NULL);

    /**
     * @brief Modify an existing scheduled meeting occurrence
     *
     * Note: A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
     * A scheduled meeting can produce one or multiple scheduled meeting occurrences
     *
     * Important considerations:
     *  - If the scheduled meeting associated to the occurrence we want to modify, doesn't have repetition rules, OR already has a
     *    parent scheduled meeting, this method won't to create a new child scheduled meeting (API requirement).
     *
     *  - If the scheduled meeting associated to the occurrence we want to modify, has repetition rules AND doesn't have a parent
     *    scheduled meeting, this method will create a new child scheduled meeting (with it's own schedId), that contains
     *    the modified ocurrence (API requirement)
     *
     * The associated request type with this request is MegaChatRequest::TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getNumber - Returns the original startDateTime of the occurrence that we want to modify
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting
     * (containing the params provided by user), or NULL in case request finished with an error.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting associated to the modified occurrence
     *  (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies the scheduled meeting
     * @param schedStartDate start date time that along with schedId identifies the occurrence with the format (unix timestamp UTC)
     * @param overrides new start date time of the occurrence with the format (unix timestamp UTC)
     * @param newEndDate new end date time of the occurrence with the format (unix timestamp UTC)
     * @param cancelled True if scheduled meeting occurrence is going to be cancelled
     * @param listener MegaChatRequestListener to track this request
     */
    void updateScheduledMeetingOccurrence(MegaChatHandle chatid, MegaChatHandle schedId, MegaChatTimeStamp overrides,  MegaChatTimeStamp newStartDate,
                                          MegaChatTimeStamp newEndDate, bool cancelled, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Removes a scheduled meeting by scheduled meeting id and chatid
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DELETE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getUserHandle - Returns the scheduled meeting id
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid or schedId are invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom or scheduled meeting does not exists
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies a scheduled meeting
     * @param listener MegaChatRequestListener to track this request
     */
    void removeScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Get a list of all scheduled meeting for a chatroom
     *
     * Important consideration:
     * A Chatroom only should have one root scheduled meeting associated, it means that for all scheduled meeting
     * returned by this method, just one should have an invalid parent sched Id (MegaChatScheduledMeeting::parentSchedId)
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @return List of MegaChatScheduledMeeting objects for a chatroom.
     */
    MegaChatScheduledMeetingList* getScheduledMeetingsByChat(MegaChatHandle chatid);

    /**
     * @brief Get a scheduled meeting given a chatid and a scheduled meeting id
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies a scheduled meeting
     * @return A MegaChatScheduledMeeting given a chatid and a scheduled meeting id
     */
    MegaChatScheduledMeeting* getScheduledMeeting(MegaChatHandle chatid, MegaChatHandle schedId);

    /**
     * @brief Get a list of all scheduled meeting for all chatrooms
     *
     * Important consideration:
     * For every chatroom there should only exist one root scheduled meeting associated, it means that for all scheduled meeting
     * returned by this method, there should be just one scheduled meeting, with an invalid parent sched Id (MegaChatScheduledMeeting::parentSchedId),
     * for every different chatid.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatScheduledMeeting objects for all chatrooms.
     */
    MegaChatScheduledMeetingList* getAllScheduledMeetings();

    /**
     * @brief Get a list of all scheduled meeting occurrences for a chatroom
     *
     * A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
     * A scheduled meeting can produce one or multiple scheduled meeting occurrences
     *
     * The associated request type with this request is MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getMegaChatScheduledMeetingOccurrList - Returns a list of scheduled meeting occurrences
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid is invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void fetchScheduledMeetingOccurrencesByChat(MegaChatHandle chatid, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Get a list of all scheduled meeting occurrences for a chatroom
     *
     * A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
     * A scheduled meeting can produce one or multiple scheduled meeting occurrences
     *
     * The associated request type with this request is MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getMegaChatScheduledMeetingOccurrList - Returns a list of scheduled meeting occurrences
     *   ordered by startDateTime, or NULL
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid is invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param since Timestamp from which API will generate more occurrences
     * @param listener MegaChatRequestListener to track this request
     */
    void fetchScheduledMeetingOccurrencesByChat(MegaChatHandle chatid, MegaChatTimeStamp since, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Creates a meeting
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getNumber - Returns always 1, since the chatroom is a meeting
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    void createMeeting(const char* title, bool speakRequest, bool waitingRoom, bool openInvite, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Check if there is an existing chat-link for an public chat
     *
     * This function allows any chat participant to check whether a public handle for public
     * chats exist and, if any, it returns a chat-link that any user can use to preview or join the chatroom.
     *
     * @see \c MegaChatApi::createPublicChat for more details.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getNumRetry - Returns 0
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the chat-link for the chatroom, if it already exist
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void queryChatLink(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Create a chat-link for a public chat
     *
     * This function allows moderators to create a public handle for public chats and returns
     * a chat-link that any user can use to preview or join the chatroom.
     *
     * @see \c MegaChatApi::createPublicChat for more details.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getNumRetry - Returns 1
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the chat-link for the chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void createChatLink(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Adds a user to an existing chat. To do this you must have the
     * moderator privilege in the chat, and the chat must be a group chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_INVITE_TO_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user to be invited
     * - MegaChatRequest::getPrivilege - Returns the privilege level wanted for the user
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers
     * or the target is not actually contact of the user.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot invite peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param uh MegaChatHandle that identifies the user
     * @param privilege Privilege level for the new peers. Valid values are:
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     * @param listener MegaChatRequestListener to track this request
     */
    void inviteToChat(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allow a user to add himself to an existing public chat. To do this the public chat must be in preview mode,
     * the result of a previous call to openChatPreview(), and the public handle contained in the chat-link must be still valid.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns invalid handle to identify that is an autojoin
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - If the chatroom is not groupal, public or is not in preview mode.
     * - MegaChatError::ERROR_NOENT - If the chat room does not exists, the chatid is not valid or the
     * public handle is not valid.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void autojoinPublicChat(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allow a user to rejoin to an existing public chat. To do this the public chat
     * must have a valid public handle.
     *
     * This function must be called only after calling:
     * - MegaChatApi::openChatPreview and receive MegaChatError::ERROR_EXIST for a chatroom where
     * your own privilege is MegaChatRoom::PRIV_RM (You are trying to preview a public chat which
     * you were part of, so you have to rejoin it)
     *
     * The associated request type with this request is MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the public handle of the chat to identify that
     * is a rejoin
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If the chatroom is not groupal, the chatroom is not public
     * or the chatroom is in preview mode.
     * - MegaChatError::ERROR_NOENT - If the chatid is not valid, there isn't any chat with the specified
     * chatid or the chat doesn't have a valid public handle.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param ph MegaChatHandle that corresponds with the public handle of chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void autorejoinPublicChat(MegaChatHandle chatid, MegaChatHandle ph, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Remove another user from a chat. To remove a user you need to have the
     * operator/moderator privilege. Only groupchats can be left.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user to be removed
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to remove peers.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot remove peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param uh MegaChatHandle that identifies the user.
     * @param listener MegaChatRequestListener to track this request
     */
    void removeFromChat(MegaChatHandle chatid, MegaChatHandle uh, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Leave a chatroom. Only groupchats can be left.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to remove peers.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot remove peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void leaveChat(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows a logged in operator/moderator to adjust the permissions on any other user
     * in their group chat. This does not work for a 1:1 chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user whose permission
     * is to be upgraded
     * - MegaChatRequest::getPrivilege - Returns the privilege level wanted for the user
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to update the privilege level.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param uh MegaChatHandle that identifies the user
     * @param privilege Privilege level for the existing peer. Valid values are:
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     * @param listener MegaChatRequestListener to track this request
     */
    void updateChatPermissions(MegaChatHandle chatid, MegaChatHandle uh, int privilege, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows a logged in operator/moderator to truncate their chat, i.e. to clear
     * the entire chat history up to a certain message. All earlier messages are wiped,
     * but this specific message will be overwritten by a management message. In addition
     * all reactions associated to the message are wiped and must be cleared by applications.
     *
     * You can expect a call to \c MegaChatRoomListener::onMessageUpdate where the message
     * will have no content and it will be of type \c MegaChatMessage::TYPE_TRUNCATE. Any
     * reactions associated to the original message will be cleared.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_TRUNCATE_HISTORY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the message identifier to truncate from.
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to truncate the chat history
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chatid or messageid are invalid
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param messageid MegaChatHandle that identifies the message to truncate from
     * @param listener MegaChatRequestListener to track this request
     */
    void truncateChat(MegaChatHandle chatid, MegaChatHandle messageid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows a logged in operator/moderator to clear the entire chat history
     *
     * If the history is not already empty, the latest message will be overwritten by
     * You can expect a call to \c MegaChatRoomListener::onMessageUpdate
     * where the message will have no content and it will be of type
     * \c MegaChatMessage::TYPE_TRUNCATE. Any reactions associated to the original
     * message will be cleared.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_TRUNCATE_HISTORY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to truncate the chat history
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chatid is invalid
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void clearChatHistory(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows to set the title of a group chat
     *
     * Only participants with privilege level MegaChatPeerList::PRIV_MODERATOR are allowed to
     * set the title of a chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_EDIT_CHATROOM_NAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getText - Returns the title of the chat.
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers.
     * - MegaChatError::ERROR_ARGS - If there's a title and it's not Base64url encoded.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param title Null-terminated character string with the title that wants to be set. If the
     * title is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    void setChatTitle(MegaChatHandle chatid, const char *title, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows any user to preview a public chat without being a participant
     *
     * This function loads the required data to preview a public chat referenced by a
     * chat-link. It returns the actual \c chatid, the public handle, the number of peers
     * and also the title.
     *
     * If this request success, the caller can proceed as usual with
     * \c MegaChatApi::openChatRoom to preview the chatroom in read-only mode, followed by
     * a MegaChatApi::closeChatRoom as usual.
     *
     * The previewer may choose to join the public chat permanently, becoming a participant
     * with read-write privilege, by calling MegaChatApi::autojoinPublicChat.
     *
     * Instead, if the previewer is not interested in the chat anymore, it can remove it from
     * the list of chats by calling MegaChatApi::closeChatPreview.
     * @note If the previewer doesn't explicitely close the preview, it will be lost if the
     * app is closed. A preview of a chat is not persisted in cache.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_PREVIEW
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getLink - Returns the chat link.
     * - MegaChatRequest::getFlag - Returns true (openChatPreview)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If chatlink has not an appropiate format
     * - MegaChatError::ERROR_EXIST - If the chatroom already exists:
     *      + If the chatroom is in preview mode the user is trying to preview a public chat twice
     *      + If the chatroom is not in preview mode but is active, the user is trying to preview a
     *      chat which he is part of.
     *      + If the chatroom is not in preview mode but is inactive, the user is trying to preview a
     *      chat which he was part of. In this case the user will have to call MegaChatApi::autorejoinPublicChat to join
     *      to autojoin the chat again. Note that you won't be able to preview a public chat any more, once
     *      you have been part of the chat.
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the public handle is not valid.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK or MegaError::ERROR_EXIST:
     * - MegaChatRequest::getChatHandle - Returns the chatid of the chat.
     * - MegaChatRequest::getNumber - Returns the number of peers in the chat.
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     * - MegaChatRequest::getUserHandle - Returns the public handle of chat.
     * - MegaChatRequest::getMegaHandleList - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     * - MegaChatRequest::getParamType - Returns 1 if it's a meeting room
     *
     * On the onRequestFinish, when the error code is MegaError::ERROR_OK, you need to call
     * MegaChatApi::openChatRoom to receive notifications related to this chat
     *
     * @param link Null-terminated character string with the public chat link
     * @param listener MegaChatRequestListener to track this request
     */
    void openChatPreview(const char *link, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows any user to obtain basic information abouts a public chat if
     * a valid public handle exists.
     *
     * This function returns the actual \c chatid, the number of peers and also the title.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_PREVIEW
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getLink - Returns the chat link.
     * - MegaChatRequest::getFlag - Returns false (checkChatLink)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If chatlink has not an appropiate format
     * - MegaChatError::ERROR_NOENT - If the chatroom not exists or the public handle is not valid.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the chatid of the chat.
     * - MegaChatRequest::getNumber - Returns the number of peers in the chat.
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     * - MegaChatRequest::getMegaHandleList - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     * - MegaChatRequest::getParamType - Returns 1 if it's a meeting room
     *
     * @param link Null-terminated character string with the public chat link
     * @param listener MegaChatRequestListener to track this request
     */
    void checkChatLink(const char *link, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Set the chat mode to private
     *
     * This function set the chat mode to private and invalidates the public handle if exists
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRIVATE_MODE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chat room does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_TOOMANY - If the chat is public and there are too many participants.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void setPublicChatToPrivate(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Invalidates the current public handle
     *
     * This function invalidates the current public handle.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getNumRetry - Returns 0
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void removeChatLink(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Allows to un/archive chats
     *
     * This is a per-chat and per-user option, and it's intended to be used when the user does
     * not care anymore about an specific chatroom. Archived chatrooms should be displayed in a
     * different section or alike, so it can be clearly identified as archived.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ARCHIVE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns if chat is to be archived or unarchived
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ENOENT - If the chatroom doesn't exists.
     * - MegaChatError::ERROR_ACCESS - If caller is not operator.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param archive True to set the chat as archived, false to unarchive it.
     * @param listener MegaChatRequestListener to track this request
     */
    void archiveChat(MegaChatHandle chatid, bool archive, MegaChatRequestListener *listener = NULL);

    /**
     * @brief This function allows a logged in operator/moderator to specify a message retention
     * timeframe in seconds, after which older messages in the chat are automatically deleted.
     * In order to disable the feature, the period of time can be set to zero (infinite).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_RETENTION_TIME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getNumber - Returns the retention timeframe in seconds
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If the chatid is invalid
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have operator privileges
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param period retention timeframe in seconds, after which older messages in the chat are automatically deleted
     * @param listener MegaChatRequestListener to track this request
     */
    void setChatRetentionTime(MegaChatHandle chatid, unsigned int period, MegaChatRequestListener *listener = NULL);

    /**
     * @brief This method should be called when a chat is opened
     *
     * The second parameter is the listener that will receive notifications about
     * events related to the specified chatroom. The same listener should be provided at
     * MegaChatApi::closeChatRoom to unregister it.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRoomListener to track events on this chatroom. NULL is not allowed.
     *
     * @return True if success, false if listener is NULL or the chatroom is not found.
     */
    bool openChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener);

    /**
     * @brief This method should be called when a chat is closed.
     *
     * It automatically unregisters the listener passed as the second paramenter, in
     * order to stop receiving the related events. Note that this listener should be
     * the one registered by MegaChatApi::openChatRoom.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRoomListener to be unregistered.
     */
    void closeChatRoom(MegaChatHandle chatid, MegaChatRoomListener *listener);

    /**
     * @brief This method should be called when we want to close a public chat preview
     *
     * It automatically disconnect to this chat, remove all internal data related, and make
     * a cache cleanup in order to clean all the related records.
     *
     * Additionally, MegaChatListener::onChatListItemUpdate will be called with an item
     * returning true for the type of change CHANGE_TYPE_PREVIEW_CLOSED
     *
     * @param chatid MegaChatHandle that identifies the chat room
     */
    void closeChatPreview(MegaChatHandle chatid);

    /**
     * @brief Initiates fetching more history of the specified chatroom.
     *
     * The loaded messages will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener).
     *
     * The corresponding callback is MegaChatRoomListener::onMessageLoaded.
     * 
     * Messages are always loaded and notified in strict order, from newest to oldest.
     *
     * @note The actual number of messages loaded can be less than \c count. One reason is
     * the history being shorter than requested, the other is due to internal protocol
     * messages that are not intended to be displayed to the user. Additionally, if the fetch
     * is local and there's no more history locally available, the number of messages could be
     * lower too (and the next call to MegaChatApi::loadMessages will fetch messages from server).
     *
     * @note \c count has a maximun value of 256. If user requests more than 256 messages,
     *  only 256 messages will returned if exits
     *
     * When there are no more history available from the reported source of messages
     * (local / remote), or when the requested \c count has been already loaded,
     * the callback MegaChatRoomListener::onMessageLoaded will be called with a NULL message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param count The number of requested messages to load (Range 1 - 256)
     *
     * @return Return the source of the messages that is going to be fetched. The possible values are:
     *   - MegaChatApi::SOURCE_ERROR = -1: history has to be fetched from server, but we are not logged in yet
     *   - MegaChatApi::SOURCE_NONE = 0: there's no more history available (not even in the server)
     *   - MegaChatApi::SOURCE_LOCAL: messages will be fetched locally (RAM or DB)
     *   - MegaChatApi::SOURCE_REMOTE: messages will be requested to the server. Expect some delay
     *
     * The value MegaChatApi::SOURCE_REMOTE can be used to show a progress bar accordingly when network operation occurs.
     */
    int loadMessages(MegaChatHandle chatid, int count);

    /**
     * @brief Checks whether the app has already loaded the full history of the chatroom
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return True the whole history is already loaded (including old messages from server).
     */
    bool isFullHistoryLoaded(MegaChatHandle chatid);

    /**
     * @brief Returns the MegaChatMessage specified from the chat room.
     *
     * This function allows to retrieve only those messages that are been loaded, received and/or
     * sent (confirmed and not yet confirmed). For any other message, this function
     * will return NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message (msg id or a temporal id)
     * @return The MegaChatMessage object, or NULL if not found.
     */
    MegaChatMessage *getMessage(MegaChatHandle chatid, MegaChatHandle msgid);

    /**
     * @brief Returns the MegaChatMessage specified from the chat room stored in node history
     *
     * This function allows to retrieve only those messages that are in the node history
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    MegaChatMessage *getMessageFromNodeHistory(MegaChatHandle chatid, MegaChatHandle msgid);

    /**
     * @brief Returns the MegaChatMessage specified from manual sending queue.
     *
     * The identifier of messages in manual sending status is notified when the
     * message is moved into that queue or while loading history. In both cases,
     * the callback MegaChatRoomListener::onMessageLoaded will be received with a
     * message object including the row id.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param rowid Manual sending queue id of the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    MegaChatMessage *getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid);

    /**
     * @brief Sends a new message to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * After this function, MegaChatApi::sendStopTypingNotification has to be called. To notify other clients
     * that it isn't typing
     *
     * You take the ownership of the returned value.
     *
     * @note Any tailing carriage return and/or line feed ('\r' and '\n') will be removed.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msg Content of the message
     *
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    MegaChatMessage *sendMessage(MegaChatHandle chatid, const char* msg);

    /**
     * @brief Sends a new giphy to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *     
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param srcMp4 Source location of the mp4
     * @param srcWebp Source location of the webp
     * @param sizeMp4 Size in bytes of the mp4
     * @param sizeWebp Size in bytes of the webp
     * @param width Width of the giphy
     * @param height Height of the giphy
     * @param title Title of the giphy
     *
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
    */
    MegaChatMessage *sendGiphy(MegaChatHandle chatid, const char* srcMp4, const char* srcWebp, long long sizeMp4, long long sizeWebp, int width, int height, const char* title);

    /**
     * @brief Sends a contact or a group of contacts to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param handles mega::MegaHandleList with contacts to be attached
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    MegaChatMessage *attachContacts(MegaChatHandle chatid, mega::MegaHandleList* handles);

    /**
     * @brief Forward a message with attach contact
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *
     * @param sourceChatid MegaChatHandle that identifies the chat room where the source message is
     * @param msgid MegaChatHandle that identifies the message that is going to be forwarded
     * @param targetChatId MegaChatHandle that identifies the chat room where the message is going to be forwarded
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    MegaChatMessage *forwardContact(MegaChatHandle sourceChatid, MegaChatHandle msgid, MegaChatHandle targetChatId);

    /**
     * @brief Sends a node or a group of nodes to the specified chatroom
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getNodeList - Returns the list of nodes
     * - MegaChatRequest::getParamType - Returns 0 (to identify the attachment as regular attachment message)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodes Array of nodes that the user want to attach
     * @param listener MegaChatRequestListener to track this request
     */
     void attachNodes(MegaChatHandle chatid, mega::MegaNodeList *nodes, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Share a geolocation in the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param longitude from shared geolocation
     * @param latitude from shared geolocation
     * @param img Preview as a byte array encoded in Base64URL. It can be NULL
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
     MegaChatMessage *sendGeolocation(MegaChatHandle chatid, float longitude, float latitude, const char *img = NULL);

     /**
      * @brief Edit a geolocation message
      *
      * Message's edits are only allowed during a short timeframe, usually 1 hour.
      * Message's deletions are equivalent to message's edits, but with empty content.
      *
      * There is only one pending edit for not-yet confirmed edits. Therefore, this function will
      * discard previous edits that haven't been notified via MegaChatRoomListener::onMessageUpdate
      * where the message has MegaChatMessage::hasChanged(MegaChatMessage::CHANGE_TYPE_CONTENT).
      *
      * If the edit is rejected because the original message is too old, this function return NULL.
      *
      * When an already delivered message (MegaChatMessage::STATUS_DELIVERED) is edited, the status
      * of the message will change from STATUS_SENDING directly to STATUS_DELIVERED again, without
      * the transition through STATUS_SERVER_RECEIVED. In other words, the protocol doesn't allow
      * to know when an edit has been delivered to the target user, but only when the edit has been
      * received by the server, so for convenience the status of the original message is kept.
      * @note if MegaChatApi::isMessageReceptionConfirmationActive returns false, messages may never
      * reach the status delivered, since the target user will not send the required acknowledge to the
      * server upon reception.
      *
      * After this function, MegaChatApi::sendStopTypingNotification has to be called. To notify other clients
      * that it isn't typing
      *
      * You take the ownership of the returned value.
      *
      * @param chatid MegaChatHandle that identifies the chat room
      * @param msgid MegaChatHandle that identifies the message
      * @param longitude from shared geolocation
      * @param latitude from shared geolocation
      * @param img Preview as a byte array encoded in Base64URL. It can be NULL
      * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
      */
      MegaChatMessage *editGeolocation(MegaChatHandle chatid, MegaChatHandle msgid, float longitude, float latitude, const char *img = NULL);

    /**
     * @brief Revoke the access to a node in the specified chatroom
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::geUserHandle - Returns the handle of the node
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodeHandle MegaChatHandle that identifies the node to revoke access to
     * @param listener MegaChatRequestListener to track this request
     */
    void revokeAttachment(MegaChatHandle chatid, MegaChatHandle nodeHandle, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Sends a node to the specified chatroom
     *
     * The attachment message includes information about the node, so the receiver can download
     * or import the node.
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the handle of the node
     * - MegaChatRequest::getParamType - Returns 0 (to identify the attachment as regular attachment message)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom, the node or the target user don't exists
     * - MegaChatError::ERROR_ACCESS - If the target user is the same as caller
     * - MegaChatError::ERROR_ACCESS - If the target user is anonymous but the chat room is in private mode
     * - MegaChatError::ERROR_ACCESS - If caller is not an operator or the target user is not a chat member
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodehandle Handle of the node that the user wants to attach
     * @param listener MegaChatRequestListener to track this request
     */
     void attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Sends a node that contains a voice message to the specified chatroom
     *
     * The voice clip message includes information about the node, so the receiver can reproduce it online.
     *
     * In contrast to other functions to send messages, such as MegaChatApi::sendMessage or
     * MegaChatApi::attachContacts, this function is asynchronous and does not return a MegaChatMessage
     * directly. Instead, the MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the handle of the node
     * - MegaChatRequest::getParamType - Returns 1 (to identify the attachment as a voice message)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodehandle Handle of the node that the user wants to attach
     * @param listener MegaChatRequestListener to track this request
    */
    void attachVoiceMessage(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Revoke the access to a node granted by an attachment message
     *
     * The attachment message will be deleted as any other message. Therefore,
     *
     * The revoke is actually a deletion of the former message. Hence, the behavior is the
     * same than a regular deletion.
     * @see MegaChatApi::editMessage or MegaChatApi::deleteMessage for more information.
     *
     * If the revoke is rejected because the attachment message is too old, or if the message is
     * not an attachment message, this function returns NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return MegaChatMessage that will be modified. NULL if the message cannot be edited (too old)
     */
     MegaChatMessage *revokeAttachmentMessage(MegaChatHandle chatid, MegaChatHandle msgid);

    /** Returns whether the logged in user has been granted access to the node
     *
     * Access to attached nodes received in chatrooms is granted when the message
     * is sent, but it can be revoked afterwards.
     *
     * This convenience method allows to check if you still have access to a node
     * or it was revoked. Usually, apps will show the attachment differently when
     * access has been revoked.
     *
     * @note The returned value will be valid only for nodes attached to messages
     * already loaded in an opened chatroom. The list of revoked nodes is updated
     * accordingly while the chatroom is open, based on new messages received.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodeHandle MegaChatHandle that identifies the node to check its access
     *
     * @return True if the user has access to the node in this chat.
     */
    bool isRevoked(MegaChatHandle chatid, MegaChatHandle nodeHandle) const;

    /**
     * @brief Edits an existing message
     *
     * Message's edits are only allowed during a short timeframe, usually 1 hour.
     * Message's deletions are equivalent to message's edits, but with empty content.
     *
     * There is only one pending edit for not-yet confirmed edits. Therefore, this function will
     * discard previous edits that haven't been notified via MegaChatRoomListener::onMessageUpdate
     * where the message has MegaChatMessage::hasChanged(MegaChatMessage::CHANGE_TYPE_CONTENT).
     *
     * If the edit is rejected because the original message is too old, this function return NULL.
     *
     * When an already delivered message (MegaChatMessage::STATUS_DELIVERED) is edited, the status 
     * of the message will change from STATUS_SENDING directly to STATUS_DELIVERED again, without
     * the transition through STATUS_SERVER_RECEIVED. In other words, the protocol doesn't allow
     * to know when an edit has been delivered to the target user, but only when the edit has been
     * received by the server, so for convenience the status of the original message is kept.
     * @note if MegaChatApi::isMessageReceptionConfirmationActive returns false, messages may never
     * reach the status delivered, since the target user will not send the required acknowledge to the
     * server upon reception.
     *
     * After this function, MegaChatApi::sendStopTypingNotification has to be called. To notify other clients
     * that it isn't typing
     * 
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @param msg New content of the message
     *
     * @return MegaChatMessage that will be modified. NULL if the message cannot be edited (too old)
     */
    MegaChatMessage *editMessage(MegaChatHandle chatid, MegaChatHandle msgid, const char* msg);

    /**
     * @brief Deletes an existing message
     *
     * @note Message's deletions are equivalent to message's edits, but with empty content.
     * @see \c MegaChatapi::editMessage for more information.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return MegaChatMessage that will be deleted. NULL if the message cannot be deleted (too old)
     */
    MegaChatMessage *deleteMessage(MegaChatHandle chatid, MegaChatHandle msgid);

    /**
     * @brief Remove an existing rich-link metadata
     *
     * This function will remove the metadata associated to the URL in the content of the message.
     * The message will be edited and will be converted back to the MegaChatMessage::TYPE_NORMAL.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return MegaChatMessage that will be modified. NULL if the message cannot be edited (too old, not rich-link...)
     */
    MegaChatMessage *removeRichLink(MegaChatHandle chatid, MegaChatHandle msgid);

    /**
     * @brief Sets the last-seen-by-us pointer to the specified message
     *
     * The last-seen-by-us pointer is persisted in the account, so every client will
     * be aware of the last-seen message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return False if the \c chatid is invalid or the message is older
     * than last-seen-by-us message. True if success.
     */
    bool setMessageSeen(MegaChatHandle chatid, MegaChatHandle msgid);

    /**
     * @brief Returns the last-seen-by-us message
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return The last-seen-by-us MegaChatMessage, or NULL if \c chatid is invalid or
     * last message seen is not loaded in memory.
     */
    MegaChatMessage *getLastMessageSeen(MegaChatHandle chatid);

    /**
     * @brief Returns message id of the last-seen-by-us message
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return Message id for the last-seen-by-us, or invalid handle if \c chatid is invalid or
     * the user has not seen any message in that chat
     */
    MegaChatHandle getLastMessageSeenId(MegaChatHandle chatid);

    /**
     * @brief Removes the unsent message from the queue
     *
     * Messages with status MegaChatMessage::STATUS_SENDING_MANUAL should be
     * removed from the manual send queue after user discards them or resends them.
     *
     * The identifier of messages in manual sending status is notified when the
     * message is moved into that queue or while loading history. In both cases,
     * the callback MegaChatRoomListener::onMessageLoaded will be received with a
     * message object including the row id.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param rowId Manual sending queue id of the message
     */
    void removeUnsentMessage(MegaChatHandle chatid, MegaChatHandle rowId);

    /**
     * @brief Send a notification to the chatroom that the user is typing
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SEND_TYPING_NOTIF
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void sendTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Send a notification to the chatroom that the user has stopped typing
     *
     * This method has to be called when the text edit label is cleared
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_STOP_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SEND_TYPING_NOTIF
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void sendStopTypingNotification(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns whether reception of messages is acknowledged
     *
     * In case this function returns true, an acknowledgement will be sent for each
     * received message, so the sender will eventually know the message is received.
     *
     * In case this function returns false, the acknowledgement is not sent and, in
     * consequence, messages at the sender-side will not reach the status MegaChatMessage::STATUS_DELIVERED.
     *
     * @note This feature is only available for 1on1 chatrooms.
     *
     * @return True if received messages are acknowledged. False if they are not.
     */
    bool isMessageReceptionConfirmationActive() const;

    /**
     * @brief Saves the current state
     *
     * The DB cache works with transactions. In order to prevent losing recent changes when the app
     * dies abruptly (usual case in mobile apps), it is recommended to call this method, so the
     * transaction is committed.
     *
     * This method should be called ONLY when the app is prone to be killed, whether by the user or the
     * operative system. Otherwise, transactions are committed regularly.
     *
     * In case disk I/O error, this function could result in the init state changing to
     * MegaChatApi::INIT_ERROR.
     */
    void saveCurrentState();

    /**
     * @brief Notify MEGAchat a push has been received (in Android)
     *
     * This method should be called when the Android app receives a push notification.
     * As result, MEGAchat will retrieve from server the latest changes in the history
     * for every chatroom and will provide to the app the list of unread messages that
     * are suitable to create OS notifications.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_PUSH_RECEIVED
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Return if the push should beep (loud) or not (silent)
     * - MegaChatRequest::getChatHandle - Return MEGACHAT_INVALID_HANDLE
     * - MegaChatRequest::getParamType - Return 0
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaHandleList- Returns the list of chatids of chats with messages to notify
     * - MegaChatRequest::getMegaHandleListByChat- Returns the list of msgids to notify for a given chat
     *
     * You can get the MegaChatMessage object by using the function \c MegaChatApi::getMessage
     *
     * @note A maximum of 6 messages per chat is returned by this function, regardless there might be
     * more unread messages. This function only searchs among local messages known by client (already loaded
     * from server and loaded in RAM). At least 32 messages are loaded in RAM for each chat.
     *
     * @param beep True if push should generate a beep, false if it shouldn't.
     * @param listener MegaChatRequestListener to track this request
     */
    void pushReceived(bool beep, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Notify MEGAchat a push has been received (in iOS)
     *
     * This method should be called when the iOS app receives a push notification.
     * As result, MEGAchat will retrieve from server the latest changes in the history
     * for one specific chatroom or for every chatroom.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_PUSH_RECEIVED
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Return if the push should beep (loud) or not (silent)
     * - MegaChatRequest::getChatHandle - Return the chatid to check for updates
     * - MegaChatRequest::getParamType - Return 1
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom does not does not exist.
     * - MegaChatError::ERROR_ACCESS - If the chatroom is archived (no notification should be generated).
     *
     * @param beep True if push should generate a beep, false if it shouldn't.
     * @param chatid MegaChatHandle that identifies the chat room, or MEGACHAT_INVALID_HANDLE for all chats
     * @param listener MegaChatRequestListener to track this request
     */
    void pushReceived(bool beep, MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);


#ifndef KARERE_DISABLE_WEBRTC
    // Video device management

    /**
     * @brief Returns a list with the names of available video devices available
     *
     * If no device is found, it returns an empty list.
     * You take the ownership of the returned value
     *
     * @return Names of the available video devices
     */
    mega::MegaStringList *getChatVideoInDevices();

    /**
     * @brief Select the video device to be used in calls
     *
     * Video device identifiers are obtained with function MegaChatApi::getChatVideoInDevices
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHANGE_VIDEO_STREAM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getText - Returns the device
     *
     * @param device Identifier of device to be selected
     * @param listener MegaChatRequestListener to track this request
     */
    void setChatVideoInDevice(const char *device, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the video selected device name
     *
     * You take the ownership of the returned value
     *
     * @return Device selected name
     */
    char *getVideoDeviceSelected();

    // Call management
    /**
     * @brief Start a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS
     *  - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD or MegaChatPeerList::PRIV_MODERATOR.
     *  - if peer of a 1on1 chatroom it's a non visible contact
     *  - if this function is called without being already connected to chatd.
     *  - if the chatroom is in preview mode.
     *  - if the chatroom has waiting room option enabled.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * The request will fail with MegaChatError::ERROR_EXISTS
     * - if there is a previous attempt still in progress (the call doesn't exist yet)
     * - if there is already another attempt to start a call for this chat, and call already exists but we don't participate
     * - if the call already exists and we already participate
     * In case that call already exists MegaChatRequest::getUserHandle will return its callid.
     *
     * The request will fail with MegaChatError::ERROR_NOENT
     * - if the chatroom doesn't exists.
     *
     * @note If the call has reached the maximum number of videos supported, the video-flag automatically be disabled.
     * @see MegaChatApi::getMaxVideoCallParticipants
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for starting a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    void startChatCall(MegaChatHandle chatid, bool enableVideo = true, bool enableAudio = true, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Start a call in a chatroom without ringing the participants (just for scheduled meeting context)
     *
     * When a scheduled meeting exists for a chatroom, and a call is started in that scheduled meeting context, it won't
     * ring the participants.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     * - MegaChatRequest::getUserHandle() - Returns the scheduled meeting id;
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS
     *  - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD or MegaChatPeerList::PRIV_MODERATOR.
     *  - if peer of a 1on1 chatroom it's a non visible contact
     *  - if this function is called without being already connected to chatd.
     *  - if the chatroom is in preview mode.
     *  - if the chatroom has waiting room option enabled.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * The request will fail with MegaChatError::ERROR_EXISTS
     * - if there is a previous attempt still in progress (the call doesn't exist yet)
     * - if there is already another attempt to start a call for this chat, and call already exists but we don't participate
     * - if the call already exists and we already participate
     * In case that call already exists MegaChatRequest::getUserHandle will return its callid.
     *
     * The request will fail with MegaChatError::ERROR_NOENT
     * - if the chatroom doesn't exists.
     * - if the scheduled meeting doesn't exists
     *
     * @note If the call has reached the maximum number of videos supported, the video-flag automatically be disabled.
     * @see MegaChatApi::getMaxVideoCallParticipants
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param schedId MegaChatHandle scheduled meeting id that identifies the scheduled meeting context in which we will start the call
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for starting a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    void startChatCallNoRinging(MegaChatHandle chatid, MegaChatHandle schedId, bool enableVideo = true, bool enableAudio = true, MegaChatRequestListener* listener = NULL);

    /**
     * @brief Answer a call received in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ANSWER_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS when this function is
     * called without being already connected to chatd.
     *
     * The request will fail with MegaChatError::ERROR_ACCESS if the chatroom has waiting room option enabled.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * The request will fail with MegaChatError::ERROR_EXISTS if there is already another attempt to answer a call
     * for this chat in progress.
     *
     * @note If the call has reached the maximum number of videos supported, the video-flag automatically be disabled.
     * @see MegaChatApi::getMaxVideoCallParticipants
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for answering a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    void answerChatCall(MegaChatHandle chatid, bool enableVideo = true, bool enableAudio = true, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Hang up a call
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the call identifier
     * - MegaChatRequest::getFlag - Returns false
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS   - if webrtc is not initialized
     * - MegaChatError::ERROR_ARGS    - if invalid callid provided
     * - MegaChatError::ERROR_NOENT   - if there is not any call with that callid or chatroom has not been found
     *
     * @param callid MegaChatHandle that identifies the call
     * @param listener MegaChatRequestListener to track this request
     */
    void hangChatCall(MegaChatHandle callid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief End a call in a chat room (user must be moderator)
     *
     * The scenario where this method is used, it's when moderator wants intentionally
     * to end a groupchat or meeting call for all participants
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the call identifier
     * - MegaChatRequest::getFlag - Returns true
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS   - if webrtc is not initialized
     * - MegaChatError::ERROR_ARGS    - if invalid callid provided
     * - MegaChatError::ERROR_NOENT   - if there is not any call with that callid or chatroom has not been found
     * - MegaChatError::ERROR_ACCESS  - if we try to end a call withouth enough privileges
     *
     * @param callid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void endChatCall(MegaChatHandle callid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable audio for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getParamType - Returns MegaChatRequest::AUDIO
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call sending audio already (no more audio slots are available).
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void enableAudio(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Disable audio for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getParamType - Returns MegaChatRequest::AUDIO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void disableAudio(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable video for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getParamType - MegaChatRequest::VIDEO
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call sending video already (no more video slots are available).
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void enableVideo(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Disable video for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getParamType - Returns MegachatRequest::VIDEO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void disableVideo(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request a high resolution quality level from a session
     *
     * Valid values for quality param are:
     *  + MegaChatCall::CALL_QUALITY_HIGH_DEF = 0,     // Default hi-res quality
     *  + MegaChatCall::CALL_QUALITY_HIGH_MEDIUM = 1,  // 2x lower resolution
     *  + MegaChatCall::CALL_QUALITY_HIGH_LOW = 2,     // 4x lower resolution
     *
     * Currently, the default high resolution is 960 x 540 pixels.
     *
     * This method can be used by the apps to lower the resolution according to their
     * needs. Ie. showing 2 videos in high res, each of them with 480x270, should
     * result on the app requesting to lower quality to CALL_QUALITY_HIGH_MEDIUM.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIRES_QUALITY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     * - MegaChatRequest::getParamType  - Returns the quality level requested
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param quality The quality level requested
     * @param listener MegaChatRequestListener to track this request
     */
    void requestHiResQuality(MegaChatHandle chatid, MegaChatHandle clientId, int quality, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Remove an active speaker from the call
     *
     * This method can be called by the speaker itself (voluntary action) or by any moderator of the groupchat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DEL_SPEAKER
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - if specified chatid is invalid
     * - MegaChatError::ERROR_NOENT  - if there's no a call in the specified chatroom
     * - MegaChatError::ERROR_ACCESS - if clientId is not MEGACHAT_INVALID_HANDLE (own user),
     * and our own privilege is different than MegaChatPeerList::PRIV_MODERATOR
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client, or MEGACHAT_INVALID_HANDLE for own user
     * @param listener MegaChatRequestListener to track this request
     */
    void removeSpeaker(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Set/unset a call on hold
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CALL_ON_HOLD
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true (set on hold) false (unset on hold)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param setOnHold indicates if call is set or unset on hold
     * @param listener MegaChatRequestListener to track this request
     */
    void setCallOnHold(MegaChatHandle chatid, bool setOnHold, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Open video device
     *
     * The associated request type with this request is MegaChatRequest::TYPE_OPEN_VIDEO_DEVICE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true open device
     *
     * @note App is responsible to release device and remove MegaChatVideoListener
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void openVideoDevice(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Release video device
     *
     * The associated request type with this request is MegaChatRequest::TYPE_OPEN_VIDEO_DEVICE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns false close device
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void releaseVideoDevice(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Get the MegaChatCall associated with a chatroom
     *
     * If \c chatid is invalid or there isn't any MegaChatCall associated with the chatroom,
     * this function returns NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatCall object associated with chatid or NULL if it doesn't exist
     */
    MegaChatCall *getChatCall(MegaChatHandle chatid);

    /**
     * @brief Mark as ignored the call associated with a chatroom
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return true if call can be marked as ignored, otherwise return false.
     */
    bool setIgnoredCall(MegaChatHandle chatid);

    /**
     * @brief Get the MegaChatCall that has a specific id
     *
     * You can get the id of a MegaChatCall using MegaChatCall::getId().
     *
     * You take the ownership of the returned value.
     *
     * @param callId MegaChatHandle that identifies the call
     * @return MegaChatCall object for the specified \c callId. NULL if call doesn't exist
     */
    MegaChatCall *getChatCallByCallId(MegaChatHandle callId);

    /**
     * @brief Returns number of calls that are currently active
     * @note You may not participate in all those calls.
     * @return number of calls in the system
     */
    int getNumCalls();

    /**
     * @brief Get a list with the ids of chatrooms where there are active calls
     *
     * The list of ids can be retrieved for calls in one specific state by setting
     * the parameter \c callState. If state is -1, it returns all calls regardless their state.
     *
     * You take the ownership of the returned value.
     *
     * @param state of calls that you want receive, -1 to consider all states
     * @return A list of handles with the ids of chatrooms where there are active calls
     */
    mega::MegaHandleList *getChatCalls(int callState = -1);

    /**
     * @brief Get a list with the ids of active calls
     *
     * You take the ownership of the returned value.
     *
     * @return A list of ids of active calls
     */
    mega::MegaHandleList *getChatCallsIds();

    /**
     * @brief Returns true if there is a call at chatroom with id \c chatid
     *
     * @note It's not necessary that we participate in the call, but other participants do.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return True if there is a call in a chatroom. False in other case
     */
    bool hasCallInChatRoom(MegaChatHandle chatid);

    /**
     * @brief Returns the maximum call participants
     *
     * @return Maximum call participants
     */
    int getMaxCallParticipants();

    /**
     * @brief Returns the maximum video call participants
     *
     * @return Maximum video call participants
     */
    int getMaxVideoCallParticipants();

    /**
     * @brief Returns if audio level monitor is enabled
     *
     * It's false by default
     *
     * @note If there isn't a call in that chatroom in which user is participating,
     * audio Level monitor will be always false
     *
     * @param chatid MegaChatHandle that identifies the chat room from we want know if audio level monitor is disabled
     * @return true if audio level monitor is enabled
     */
    bool isAudioLevelMonitorEnabled(MegaChatHandle chatid);

    /**
     * @brief Enable or disable audio level monitor
     *
     * It's false by default and it's app responsibility to enable it
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ENABLE_AUDIO_LEVEL_MONITOR
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns if enable or disable the audio level monitor
     *
     * @note If there isn't a call in that chatroom in which user is participating,
     * audio Level monitor won't be able established
     *
     * @param enable True for enable audio level monitor, False to disable
     * @param chatid MegaChatHandle that identifies the chat room where we can enable audio level monitor
     * @param listener MegaChatRequestListener to track this request
     */
    void enableAudioLevelMonitor(bool enable, MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request become a speaker
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that it is a enable request operation
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void requestSpeak(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Remove a request to become a speaker
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that it is a remove request operation
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void removeRequestSpeak(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Approve speak request
     *
     * This method has to be called only by a user with moderator role
     *
     * The associated request type with this request is MegaChatRequest::TYPE_APPROVE_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that approve the request
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    void approveSpeakRequest(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Reject speak request
     *
     * This method has to be called only by a user with moderator role
     *
     * The associated request type with this request is MegaChatRequest::TYPE_APPROVE_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that reject the request
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    void rejectSpeakRequest(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request high resolution video from a client
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that request high resolution video
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     * - MegaChatRequest::getPrivilege - Returns MegaChatCall::CALL_QUALITY_HIGH_DEF
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    void requestHiResVideo(MegaChatHandle chatid, MegaChatHandle clientId, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request high resolution video from a client with a specified resolution quality level
     *
     * Valid values for quality param are:
     *  + MegaChatCall::CALL_QUALITY_HIGH_DEF = 0,     // Default hi-res quality
     *  + MegaChatCall::CALL_QUALITY_HIGH_MEDIUM = 1,  // 2x lower resolution
     *  + MegaChatCall::CALL_QUALITY_HIGH_LOW = 2,     // 4x lower resolution
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that request high resolution video
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     * - MegaChatRequest::getPrivilege - Returns the resolution quality level for received video
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param quality resolution quality level for received video
     * @param listener MegaChatRequestListener to track this request
     */
    void requestHiResVideoWithQuality(MegaChatHandle chatid, MegaChatHandle clientId, int quality, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Stop high resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that stop high resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of clients Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    void stopHiResVideo(MegaChatHandle chatid, mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Request low resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_LOW_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that request low resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of client Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    void requestLowResVideo(MegaChatHandle chatid, ::mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Stop low resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_LOW_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that stop low resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of clients Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    void stopLowResVideo(MegaChatHandle chatid, ::mega::MegaHandleList *clientIds, MegaChatRequestListener *listener = NULL);

#endif

    // Listeners
    /**
     * @brief Register a listener to receive global events
     *
     * You can use MegaChatApi::removeChatListener to stop receiving events.
     *
     * @param listener Listener that will receive global events
     */
    void addChatListener(MegaChatListener *listener);

    /**
     * @brief Unregister a MegaChatListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatListener(MegaChatListener *listener);

    /**
     * @brief Register a listener to receive all events about an specific chat
     *
     * You can use MegaChatApi::removeChatRoomListener to stop receiving events.
     *
     * Note this listener is feeded with data from a chatroom that is opened. It
     * is required to call \c MegaChatApi::openChatRoom. Otherwise, the listener
     * will NOT receive any callback.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener Listener that will receive all events about an specific chat
     */
    void addChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);

    /**
     * @brief Unregister a MegaChatRoomListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatRoomListener(MegaChatHandle chatid, MegaChatRoomListener *listener);

    /**
     * @brief Register a listener to receive all events about requests
     *
     * You can use MegaChatApi::removeChatRequestListener to stop receiving events.
     *
     * @param listener Listener that will receive all events about requests
     */
    void addChatRequestListener(MegaChatRequestListener* listener);

    /**
     * @brief Unregister a MegaChatRequestListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatRequestListener(MegaChatRequestListener* listener);

    /**
     * @brief Register a listener to receive notifications
     *
     * You can use MegaChatApi::removeChatRequestListener to stop receiving events.
     *
     * @param listener Listener that will receive all events about requests
     */
    void addChatNotificationListener(MegaChatNotificationListener *listener);

    /**
     * @brief Unregister a MegaChatNotificationListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatNotificationListener(MegaChatNotificationListener* listener);

    /**
     * @brief Adds a reaction for a message in a chatroom
     *
     * The reactions updates will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener). The corresponding callback
     * is MegaChatRoomListener::onReactionUpdate.
     *
     * Note that receiving an onRequestFinish with the error code MegaChatError::ERROR_OK, does not ensure
     * that add reaction has been applied in chatd. As we've mentioned above, reactions updates will
     * be notified through callback MegaChatRoomListener::onReactionUpdate.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_MANAGE_REACTION
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatid that identifies the chatroom
     * - MegaChatRequest::getUserHandle - Returns the msgid that identifies the message
     * - MegaChatRequest::getText - Returns a UTF-8 NULL-terminated string that represents the reaction
     * - MegaChatRequest::getFlag - Returns true indicating that requested action is add reaction
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - if reaction is NULL or the msgid references a management message.
     * - MegaChatError::ERROR_NOENT - if the chatroom/message doesn't exists
     * - MegaChatError::ERROR_TOOMANY - if the message has reached the maximum limit of reactions,
     * and reaction has not been added yet. MegaChatRequest::getNumber() will return -1
     * - MegaChatError::ERROR_TOOMANY - if our own user has reached the maximum limit of reactions
     * MegaChatRequest::getNumber() will return 1
     * - MegaChatError::ERROR_ACCESS - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD
     * or MegaChatPeerList::PRIV_MODERATOR.
     * - MegaChatError::ERROR_EXIST - if our own user has reacted previously with this reaction for this message
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL-terminated string that represents the reaction
     * @param listener MegaChatRequestListener to track this request
     */
    void addReaction(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Removes a reaction for a message in a chatroom
     *
     * The reactions updates will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener). The corresponding callback
     * is MegaChatRoomListener::onReactionUpdate.
     *
     * Note that receiving an onRequestFinish with the error code MegaChatError::ERROR_OK, does not ensure
     * that remove reaction has been applied in chatd. As we've mentioned above, reactions updates will
     * be notified through callback MegaChatRoomListener::onReactionUpdate.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_MANAGE_REACTION
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatid that identifies the chatroom
     * - MegaChatRequest::getUserHandle - Returns the msgid that identifies the message
     * - MegaChatRequest::getText - Returns a UTF-8 NULL-terminated string that represents the reaction
     * - MegaChatRequest::getFlag - Returns false indicating that requested action is remove reaction
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS: if reaction is NULL or the msgid references a management message.
     * - MegaChatError::ERROR_NOENT: if the chatroom/message doesn't exists
     * - MegaChatError::ERROR_ACCESS: if our own privilege is different than MegaChatPeerList::PRIV_STANDARD
     * or MegaChatPeerList::PRIV_MODERATOR
     * - MegaChatError::ERROR_EXIST - if your own user has not reacted to the message with the specified reaction.
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL-terminated string that represents the reaction
     * @param listener MegaChatRequestListener to track this request
     */
    void delReaction(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the number of users that reacted to a message with a specific reaction
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL terminated string that represents the reaction
     *
     * @return return the number of users that reacted to a message with a specific reaction,
     * or -1 if the chatroom or message is not found.
     */
    int getMessageReactionCount(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction) const;

     /**
      * @brief Gets a list of reactions associated to a message
      *
      * You take the ownership of the returned value.
      *
      * @param chatid MegaChatHandle that identifies the chatroom
      * @param msgid MegaChatHandle that identifies the message
      * @return return a list with the reactions associated to a message.
      */
    ::mega::MegaStringList* getMessageReactions(MegaChatHandle chatid, MegaChatHandle msgid);

     /**
      * @brief Gets a list of users that reacted to a message with a specific reaction
      *
      * You take the ownership of the returned value.
      *
      * @param chatid MegaChatHandle that identifies the chatroom
      * @param msgid MegaChatHandle that identifies the message
      * @param reaction UTF-8 NULL terminated string that represents the reaction
      *
      * @return return a list with the users that reacted to a message with a specific reaction.
      */
    ::mega::MegaHandleList* getReactionUsers(MegaChatHandle chatid, MegaChatHandle msgid, const char *reaction);

    /**
     * @brief Enable / disable the public key pinning
     *
     * Public key pinning is enabled by default for all sensible communications.
     * It is strongly discouraged to disable this feature.
     *
     * @param enable true to keep public key pinning enabled, false to disable it
     */
    void setPublicKeyPinning(bool enable);

#ifndef KARERE_DISABLE_WEBRTC
    /**
     * @brief Register a listener to receive all events about calls
     *
     * You can use MegaChatApi::removeChatCallListener to stop receiving events.
     *
     * @param listener MegaChatCallListener that will receive all call events
     */
    void addChatCallListener(MegaChatCallListener *listener);

    /**
     * @brief Unregister a MegaChatCallListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatCallListener(MegaChatCallListener *listener);

    /**
     * @brief Register a listener to receive all events about scheduled meetings
     *
     * You can use MegaChatApi::removeSchedMeetingListener to stop receiving events.
     *
     * @param listener MegaChatScheduledMeetingListener that will receive all scheduled meetings events
     */
    void addSchedMeetingListener(MegaChatScheduledMeetingListener* listener);

    /**
     * @brief Unregister a MegaChatScheduledMeetingListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeSchedMeetingListener(MegaChatScheduledMeetingListener* listener);

    /**
     * @brief Register a listener to receive video from local device for an specific chat room
     *
     * You can use MegaChatApi::removeChatLocalVideoListener to stop receiving events.
     *
     * @note if we want to receive video before start a call (openVideoDevice), we have to
     * register a MegaChatVideoListener with chatid = MEGACHAT_INVALID_HANDLE
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatVideoListener that will receive local video
     */
    void addChatLocalVideoListener(MegaChatHandle chatid, MegaChatVideoListener *listener);

    /**
     * @brief Unregister a MegaChatVideoListener
     *
     * This listener won't receive more events.
     * @note if we want to remove the listener added to receive video frames before start a call
     * we have to use chatid = MEGACHAT_INVALID_HANDLE
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener Object that is unregistered
     */
    void removeChatLocalVideoListener(MegaChatHandle chatid, MegaChatVideoListener *listener);

    /**
     * @brief Register a listener to receive video from remote device for an specific chat room and peer
     *
     * You can use MegaChatApi::removeChatRemoteVideoListener to stop receiving events.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param hiRes boolean that identify if video is high resolution or low resolution
     * @param listener MegaChatVideoListener that will receive remote video
     */
    void addChatRemoteVideoListener(MegaChatHandle chatid, MegaChatHandle clientId, bool hiRes, MegaChatVideoListener *listener);

    /**
     * @brief Unregister a MegaChatVideoListener
     *
     * This listener won't receive more events.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param hiRes boolean that identify if video is high resolution or low resolution
     * @param listener Object that is unregistered
     */
    void removeChatRemoteVideoListener(MegaChatHandle chatid, MegaChatHandle clientId, bool hiRes, MegaChatVideoListener *listener);

    /**
     * @brief Change the SFU id
     *
     * This function allows to set the SFU server where all chat calls will be started
     * It's only useful for testing or debugging purposes.
     *
     * Note: To restore default behavior (SFU assigned by API), sfuid param must be set to SFU_ID_DEFAULT
     *
     * @param sfuid New SFU id
     */
    void setSFUid(int sfuid);
#endif

    static void setCatchException(bool enable);

    /**
     * @brief Checks whether \c text contains a URL
     *
     * @param text String to search for a URL
     * @return True if \c text contains a URL
     */
    static bool hasUrl(const char* text);

    /**
     * @brief Checks if a chat option is enabled in a bitmask
     *
     * Valid values for option are:
     * - MegaChatApi::CHAT_OPTION_SPEAK_REQUEST
     * - MegaChatApi::CHAT_OPTION_WAITING_ROOM
     * - MegaChatApi::CHAT_OPTION_OPEN_INVITE
     *
     * @param option Option to check if it's enabled in a bitmask
     * @param chatOptionsBitMask Bitmask that represents a set of chat options
     * @return True if specified option is enabled in the bitmask
     */
    static bool hasChatOptionEnabled(int option, int chatOptionsBitMask);

    /**
     * @brief This method should be called when a node history is opened
     *
     * One node history only can be opened once before it will be closed
     * The same listener should be provided at MegaChatApi::closeChatRoom to unregister it
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatNodeHistoryListener to receive node history events. NULL is not allowed.
     *
     * @return True if success, false if listener is NULL or the chatroom is not found
     */
    bool openNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);

    /**
     * @brief This method should be called when a node history is closed
     *
     * Note that this listener should be the one registered by MegaChatApi::openNodeHistory
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatNodeHistoryListener to receive node history events. NULL is not allowed.
     *
     * @return True if success, false if listener is NULL or the chatroom is not found
     */
    bool closeNodeHistory(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);

    /**
     * @brief Register a listener to receive all events about a specific node history
     *
     * You can use MegaChatApi::removeNodeHistoryListener to stop receiving events.
     *
     * Note this listener is feeded with data from a node history that is opened. It
     * is required to call \c MegaChatApi::openNodeHistory. Otherwise, the listener
     * will NOT receive any callback.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener Listener that will receive node history events
     */
    void addNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);

    /**
     * @brief Unregister a MegaChatNodeHistoryListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeNodeHistoryListener(MegaChatHandle chatid, MegaChatNodeHistoryListener *listener);

    /**
     * @brief Initiates fetching more node history of the specified chatroom.
     *
     * The loaded messages will be notified one by one through the MegaChatNodeHistoryListener
     * specified at MegaChatApi::openNodeHistory (and through any other listener you may have
     * registered by calling MegaChatApi::addNodeHistoryListener).
     *
     * The corresponding callback is MegaChatNodeHistoryListener::onAttachmentLoaded.
     *
     * Messages are always loaded and notified in strict order, from newest to oldest.
     *
     * @note The actual number of messages loaded can be less than \c count. Because
     * the history being shorter than requested. Additionally, if the fetch is local
     * and there's no more history locally available, the number of messages could be
     * lower too (and the next call to MegaChatApi::loadMessages will fetch messages from server).
     *
     * When there are no more history available from the reported source of messages
     * (local / remote), or when the requested \c count has been already loaded,
     * the callback  MegaChatNodeHistoryListener::onAttachmentLoaded will be called with a NULL message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param count The number of requested messages to load.
     *
     * @return Return the source of the messages that is going to be fetched. The possible values are:
     *   - MegaChatApi::SOURCE_ERROR = -1: we are not logged in yet
     *   - MegaChatApi::SOURCE_NONE = 0: there's no more history available (not even in the server)
     *   - MegaChatApi::SOURCE_LOCAL: messages will be fetched locally (RAM or DB)
     *   - MegaChatApi::SOURCE_REMOTE: messages will be requested to the server. Expect some delay
     *
     * The value MegaChatApi::SOURCE_REMOTE can be used to show a progress bar accordingly when network operation occurs.
     */
    int loadAttachments(MegaChatHandle chatid, int count);

private:
    MegaChatApiImpl *pImpl;
};

/**
 * @brief Represents every single chatroom where the user participates
 *
 * Unlike MegaChatRoom, which contains full information about the chatroom,
 * objects of this class include strictly the minimal information required
 * to populate a list of chats:
 *  - Chat ID
 *  - Title
 *  - Online status
 *  - Unread messages count
 *  - Visibility of the contact for 1on1 chats
 *
 * Changes on any of this fields will be reported by a callback: MegaChatListener::onChatListItemUpdate
 * It also notifies about a groupchat that has been closed (the user has left the room).
 */
class MegaChatListItem
{
public:

    enum
    {
        CHANGE_TYPE_STATUS              = 0x01,  /// obsolete
        CHANGE_TYPE_OWN_PRIV            = 0x02,  /// Our privilege level has changed
        CHANGE_TYPE_UNREAD_COUNT        = 0x04,  /// Unread count updated
        CHANGE_TYPE_PARTICIPANTS        = 0x08,  /// A participant joined/left the chatroom or its privilege changed
        CHANGE_TYPE_TITLE               = 0x10,  /// Title updated
        CHANGE_TYPE_CLOSED              = 0x20,  /// The chatroom has been left by own user
        CHANGE_TYPE_LAST_MSG            = 0x40,  /// Last message recorded in the history, or chatroom creation data if no history at all (not even clear-history message)
        CHANGE_TYPE_LAST_TS             = 0x80,  /// Timestamp of the last activity
        CHANGE_TYPE_ARCHIVE             = 0x100, /// Archived or unarchived
        CHANGE_TYPE_CALL                = 0x200, /// There's a new call or a call has finished
        CHANGE_TYPE_CHAT_MODE           = 0x400, /// User has set chat mode to private
        CHANGE_TYPE_UPDATE_PREVIEWERS   = 0x800, /// The number of previewers has changed
        CHANGE_TYPE_PREVIEW_CLOSED      = 0x1000,/// The chat preview has been closed
        CHANGE_TYPE_DELETED             = 0x2000 /// The chat has been taken down and should be hiden from the list of chats
    };

    virtual ~MegaChatListItem() {}
    virtual MegaChatListItem *copy() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;

    /**
     * @brief Returns the MegaChatHandle of the chat.
     * @return MegaChatHandle of the chat.
     */
    virtual MegaChatHandle getChatId() const;

    /**
     * @brief getTitle Returns the title of the chat, if any.
     *
     * @return The title of the chat as a null-terminated char array.
     */
    virtual const char *getTitle() const;

    /**
     * @brief Returns the own privilege level in this chatroom.
     *
     * This privilege is the same from MegaChatRoom::getOwnPrivilege.
     *
     * It could be used to show/hide options at the chatlist level that
     * are only allowed to peers with the highest privilege level.
     *
     * The returned value will be one of these:
     * - MegaChatRoom::PRIV_UNKNOWN = -2
     * - MegaChatRoom::PRIV_RM = -1
     * - MegaChatRoom::PRIV_RO = 0
     * - MegaChatRoom::PRIV_STANDARD = 2
     * - MegaChatRoom::PRIV_MODERATOR = 3
     *
     * @return The current privilege of the logged in user in this chatroom.
     */
    virtual int getOwnPrivilege() const;

    /**
     * @brief Returns the number of unread messages for the chatroom
     *
     * It can be used to display an unread message counter next to the chatroom name
     *
     * @return The count of unread messages as follows:
     *  - If the returned value is 0, then the indicator should be removed.
     *  - If the returned value is > 0, the indicator should show the exact count.
     *  - If the returned value is < 0, then there are at least that count unread messages,
     * and possibly more. In that case the indicator should show e.g. '2+'
     */
    virtual int getUnreadCount() const;

    /**
     * @brief Returns the content of the last message for the chatroom
     *
     * If there are no messages in the history or the last message is still
     * pending to be retrieved from the server, it returns an empty string.
     *
     * The returned value of this function depends on the type of message:
     *
     *  - MegaChatMessage::TYPE_NORMAL: content of the message
     *  - MegaChatMessage::TYPE_ATTACHMENT: filenames of the attached nodes (separated by ASCII character '0x01')
     *  - MegaChatMessage::TYPE_CONTACT: usernames of the attached contacts (separated by ASCII character '0x01')
     *  - MegaChatMessage::TYPE_CONTAINS_META: original content of the messsage
     *  - MegaChatMessage::TYPE_VOICE_CLIP: filename of the attached node
     *  - MegaChatMessage::TYPE_CHAT_TITLE: new title
     *  - MegaChatMessage::TYPE_TRUNCATE: empty string
     *  - MegaChatMessage::TYPE_ALTER_PARTICIPANTS: empty string
     *  - MegaChatMessage::TYPE_PRIV_CHANGE: empty string
     *  - MegaChatMessage::TYPE_CALL_ENDED: string set separated by ASCII character '0x01'
     *      Structure: duration(seconds)'0x01'termCode'0x01'participants1'0x01'participants2'0x01'...
     *      duration and termCode are numbers coded in ASCII, participants are handles in base64 format.
     *      Valid TermCode are:
     *          + END_CALL_REASON_ENDED
     *          + END_CALL_REASON_REJECTED
     *          + END_CALL_REASON_NO_ANSWER
     *          + END_CALL_REASON_FAILED
     *          + END_CALL_REASON_CANCELLED
     *          + END_CALL_REASON_BY_MODERATOR
     *      If termCode is END_CALL_REASON_REJECTED, END_CALL_REASON_NO_ANSWER, END_CALL_REASON_CANCELLED
     *      any participant won't be added
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatListItem object is deleted. If you want to save the MegaChatMessage,
     * use MegaChatMessage::copy.
     *
     * @return The content of the last message received.
     */
    virtual const char *getLastMessage() const;

    /**
     * @brief Returns message id of the last message in this chatroom.
     *
     * If the message is still not confirmed by server, the id could be a temporal
     * id. @see \c MegaChatApi::sendMessage for more information about the msg id.
     *
     * @return MegaChatHandle representing the id of last message.
     */
    virtual MegaChatHandle getLastMessageId() const;

    /**
     * @brief Returns the type of last message
     *
     * The possible values are as follows:
     *  - MegaChatMessage::TYPE_INVALID:  when no history (or truncate message)
     *  - MegaChatMessage::TYPE_NORMAL: for regular text messages
     *  - MegaChatMessage::TYPE_ATTACHMENT: for messages sharing a node
     *  - MegaChatMessage::TYPE_CONTACT: for messages sharing a contact
     *  - MegaChatMessage::TYPE_CONTAINS_META: for messages with meta-data
     *  - MegaChatMessage::TYPE_VOICE_CLIP: for voice-clips
     *  - 0xFF when it's still fetching from server (for the public API)
     *
     * @return The type of the last message
     */
    virtual int getLastMessageType() const;

    /**
     * @brief Returns the sender of last message
     *
     * This function only returns a valid user handle when the last message type is
     * not MegaChatMessage::TYPE_INVALID or 0xFF. Otherwise, it returns INVALID_HANDLE.
     *
     * @return MegaChatHandle representing the user who sent the last message
     */
    virtual MegaChatHandle getLastMessageSender() const;

    /**
     * @brief Returns the timestamp of the latest activity in the chatroom
     *
     * This function returns the timestamp of the latest message, including management messages.
     * If there's no history at all, or is still being fetched from server, it will return
     * the creation timestamp of the chatroom.
     *
     * @return The timestamp relative to the latest activity in the chatroom.
     */
    virtual int64_t getLastTimestamp() const;

    /**
     * @brief Returns whether this chat is a group chat or not
     * @return True if this chat is a group chat. Only chats with more than 2 peers are groupal chats.
     */
    virtual bool isGroup() const;

    /**
     * @brief Returns whether this chat is a public chat or not
     * @return True if this chat is a public chat.
     */
    virtual bool isPublic() const;

    /**
     * @brief Returns whether a public chat is in preview mode or not
     * @return True if this public chat is in preview mode.
     */
    virtual bool isPreview() const;

    /**
     * @brief Returns whether the user is member of the chatroom (for groupchats),
     * or the user is contact with the peer (for 1on1 chats).
     *
     * @return True if the chat is active, false otherwise.
     */
    virtual bool isActive() const;

    /**
     * @brief Returns whether the chat is currently archived or not.
     * @return True if the chat is archived, false otherwise.
     */
    virtual bool isArchived() const;

    /**
     * @brief Returns whether the chat has been deleted
     * @return True if the chat is deleted, false otherwise.
     */
    virtual bool isDeleted() const;

    /**
     * @brief Returns whether the chat has a call in progress or not.
     * @return True if a call is in progress in this chat, false otherwise.
     */
    virtual bool isCallInProgress() const;

    /**
     * @brief Returns the userhandle of the Contact in 1on1 chatrooms
     *
     * The returned value is only valid for 1on1 chatrooms. For groupchats, it will
     * return MEGACHAT_INVALID_HANDLE.
     *
     * @return The userhandle of the Contact
     */
    virtual MegaChatHandle getPeerHandle() const;

    /**
     * @brief Returns privilege established at last message
     *
     * The returned value is only valid if last message is from type MegaChatMessage::TYPE_ALTER_PARTICIPANTS
     * and MegaChatMessage::TYPE_PRIV_CHANGE.
     *
     * @return prilvilege stablished at last message
     */
    virtual int getLastMessagePriv() const;

    /**
     * @brief Returns the handle of the target user
     *
     * The returned value is only valid if last message is from type MegaChatMessage::TYPE_ALTER_PARTICIPANTS
     * and MegaChatMessage::TYPE_PRIV_CHANGE.
     *
     * @return Handle of the target user
     */
    virtual MegaChatHandle getLastMessageHandle() const;

    /**
     * @brief Returns the number of previewers in this chat
     * @return
     */
    virtual unsigned int getNumPreviewers() const;

    /**
     * @brief Returns if chatroom is a meeting
     *
     * @return True if chatroom is a meeting
     */
    virtual bool isMeeting() const;

};

class MegaChatRoom
{
public:

    enum
    {
        CHANGE_TYPE_STATUS              = 0x01, /// obsolete
        CHANGE_TYPE_UNREAD_COUNT        = 0x02,
        CHANGE_TYPE_PARTICIPANTS        = 0x04, /// joins/leaves/privileges/names
        CHANGE_TYPE_TITLE               = 0x08,
        CHANGE_TYPE_USER_TYPING         = 0x10, /// User is typing. \see MegaChatRoom::getUserTyping()
        CHANGE_TYPE_CLOSED              = 0x20, /// The chatroom has been left by own user
        CHANGE_TYPE_OWN_PRIV            = 0x40, /// Our privilege level has changed
        CHANGE_TYPE_USER_STOP_TYPING    = 0x80, /// User has stopped to typing. \see MegaChatRoom::getUserTyping()
        CHANGE_TYPE_ARCHIVE             = 0X100, /// Archived or unarchived
        CHANGE_TYPE_CHAT_MODE           = 0x400, /// User has set chat mode to private
        CHANGE_TYPE_UPDATE_PREVIEWERS   = 0x800,  /// The number of previewers has changed
        CHANGE_TYPE_RETENTION_TIME      = 0x1000, /// The retention time has changed
        CHANGE_TYPE_OPEN_INVITE         = 0x2000, /// The open invite mode option has changed
        CHANGE_TYPE_SPEAK_REQUEST       = 0x4000, /// The speak request option has changed
        CHANGE_TYPE_WAITING_ROOM        = 0x8000, /// The waiting room option has changed
    };

    enum {
        PRIV_UNKNOWN    = -2,
        PRIV_RM         = -1,
        PRIV_RO         = 0,
        PRIV_STANDARD   = 2,
        PRIV_MODERATOR  = 3
    };

    virtual ~MegaChatRoom() {}
    virtual MegaChatRoom *copy() const;

    static const char *privToString(int);
    static const char *statusToString(int status);

    /**
     * @brief Returns the MegaChatHandle of the chat.
     * @return MegaChatHandle of the chat.
     */
    virtual MegaChatHandle getChatId() const;

    /**
     * @brief Returns your privilege level in this chat
     * @return
     */
    virtual int getOwnPrivilege() const;

    /**
     * @brief Returns the number of previewers in this chat
     * @return
     */
    virtual unsigned int getNumPreviewers() const;

    /**
     * @brief Returns the privilege level of the user in this chat.
     *
     * If the user doesn't participate in this MegaChatRoom, this function returns PRIV_UNKNOWN.
     *
     * @param userhandle Handle of the peer whose privilege is requested.
     * @return Privilege level of the chat peer with the handle specified.
     * Valid values are:
     * - MegaChatPeerList::PRIV_UNKNOWN = -2
     * - MegaChatPeerList::PRIV_RM = -1
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     */
    virtual int getPeerPrivilegeByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the current firstname of the peer
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the firstname with
     * MegaChatApi::getUserFirstnameFromCache.
     *
     * @deprecated Use MegaChatApi::getUserFirstnameFromCache
     *
     * @param userhandle Handle of the peer whose name is requested.
     * @return Firstname of the chat peer with the handle specified.
     */
    virtual const char *getPeerFirstnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the current lastname of the peer
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the lastname with
     * MegaChatApi::getUserLastnameFromCache.
     *
     * @deprecated Use MegaChatApi::getUserLastnameFromCache
     *
     * @param userhandle Handle of the peer whose name is requested.
     * @return Lastname of the chat peer with the handle specified.
     */
    virtual const char *getPeerLastnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the current fullname of the peer
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the full name  with
     * MegaChatApi::getUserFullnameFromCache
     *
     * You take the ownership of the returned value. Use delete [] value
     *
     * @deprecated Use MegaChatApi::getUserFullnameFromCache
     *
     * @param userhandle Handle of the peer whose name is requested.
     * @return Fullname of the chat peer with the handle specified.
     */
    virtual const char *getPeerFullnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the email address of the peer
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the email with
     * MegaChatApi::getUserEmailFromCache.
     *
     * @deprecated Use MegaChatApi::getUserEmailFromCache
     *
     * @param userhandle Handle of the peer whose email is requested.
     * @return Email address of the chat peer with the handle specified.
     */
    virtual const char *getPeerEmailByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the number of participants in the chat
     * @return Number of participants in the chat
     */
    virtual unsigned int getPeerCount() const;

    /**
     * @brief Returns the handle of the user
     *
     * If the index is >= the number of participants in this chat, this function
     * will return MEGACHAT_INVALID_HANDLE.
     *
     * @param i Position of the peer whose handle is requested
     * @return Handle of the peer in the position \c i.
     */
    virtual MegaChatHandle getPeerHandle(unsigned int i) const;

    /**
     * @brief Returns the privilege level of the user in this chat.
     *
     * If the index is >= the number of participants in this chat, this function
     * will return PRIV_UNKNOWN.
     *
     * @param i Position of the peer whose handle is requested
     * @return Privilege level of the peer in the position \c i.
     * Valid values are:
     * - MegaChatPeerList::PRIV_UNKNOWN = -2
     * - MegaChatPeerList::PRIV_RM = -1
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     */
    virtual int getPeerPrivilege(unsigned int i) const;

    /**
     * @brief Returns the current firstname of the peer
     *
     * If the index is >= the number of participants in this chat, this function
     * will return NULL.
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the firstname with
     * MegaChatApi::getUserFirstnameFromCache.
     *
     * @deprecated Use MegaChatApi::getUserFirstnameFromCache
     *
     * @param i Position of the peer whose name is requested
     * @return Firstname of the peer in the position \c i.
     */
    virtual const char *getPeerFirstname(unsigned int i) const;

    /**
     * @brief Returns the current lastname of the peer
     *
     * If the index is >= the number of participants in this chat, this function
     * will return NULL.
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the lastname with
     * MegaChatApi::getUserLastnameFromCache.
     *
     * @deprecated Use MegaChatApi::getUserLastnameFromCache
     *
     * @param i Position of the peer whose name is requested
     * @return Lastname of the peer in the position \c i.
     */
    virtual const char *getPeerLastname(unsigned int i) const;

    /**
     * @brief Returns the current fullname of the peer
     *
     * If the index is >= the number of participants in this chat, this function
     * will return NULL.
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the fullname with
     * MegaChatApi::getUserFullnameFromCache.
     *
     * You take the ownership of the returned value. Use delete [] value
     *
     * @deprecated Use MegaChatApi::getUserFullnameFromCache
     *
     * @param i Position of the peer whose name is requested
     * @return Fullname of the peer in the position \c i.
     */
    virtual const char *getPeerFullname(unsigned int i) const;

    /**
     * @brief Returns the email address of the peer
     *
     * If the index is >= the number of participants in this chat, this function
     * will return NULL.
     *
     * NULL can be returned in public link if number of particpants is greater
     * than MegaChatApi::getMaxParticipantsWithAttributes. In this case, you have to
     * request the user attributes with MegaChatApi::loadUserAttributes. To improve
     * the performance, if several users has to be request, call MegaChatApi::loadUserAttributes
     * with a package of users. When request is finished you can get the email with
     * MegaChatApi::getUserEmailFromCache.
     *
     * @deprecated Use MegaChatApi::getUserEmailFromCache
     *
     * @param i Position of the peer whose email is requested
     * @return Email address of the peer in the position \c i.
     */
    virtual const char *getPeerEmail(unsigned int i) const;

    /**
     * @brief Returns whether this chat is a group chat or not
     * @return True if this chat is a group chat. Only chats with more than 2 peers are groupal chats.
     */
    virtual bool isGroup() const;

    /**
     * @brief Returns whether this chat is a public chat or not
     * @return True if this chat is a public chat.
     */
    virtual bool isPublic() const;

    /**
     * @brief Returns whether this chat is in preview mode or not
     * @return True if this chat is in preview mode.
     */
    virtual bool isPreview() const;

    /**
     * @brief Get the authorization token in preview mode
     *
     * This method returns an authorization token that can be used to authorize
     * nodes received as attachments while in preview mode, so the node can be
     * downloaded/imported into the account via MegaApi::authorizeChatNode.
     *
     * If the chat is not in preview mode, this function will return NULL.
     *
     * You take the ownership of the returned value. Use delete [] value
     *
     * @return Auth token or NULL if not in preview mode.
     */
    virtual const char *getAuthorizationToken() const;

    /**
     * @brief Returns the title of the chat, if any.
     *
     * In case the chatroom has not a customized title, it will be created using the
     * names of participants.
     *
     * @return The title of the chat as a null-terminated char array.
     */
    virtual const char *getTitle() const;

    /**
     * @brief Returns true if the chatroom has a customized title
     * @return True if custom title was set
     */
    virtual bool hasCustomTitle() const;

    /**
     * @brief Returns the number of unread messages for the chatroom
     *
     * It can be used to display an unread message counter next to the chatroom name
     *
     * @return The count of unread messages as follows:
     *  - If the returned value is 0, then the indicator should be removed.
     *  - If the returned value is > 0, the indicator should show the exact count.
     *  - If the returned value is < 0, then there are at least that count unread messages,
     * and possibly more. In that case the indicator should show e.g. '2+'
     */
    virtual int getUnreadCount() const;

    /**
     * @brief Returns the handle of the user who is typing or has stopped typing a message in the chatroom
     *
     * The app should have a timer that is reset each time a typing
     * notification is received. When the timer expires, it should hide the notification
     *
     * @return The user that is typing
     */
    virtual MegaChatHandle getUserTyping() const;

    /**
     * @brief Returns the handle of the user who has been Joined/Removed/change its name
     *
     * This method return a valid value when hasChanged(CHANGE_TYPE_PARTICIPANTS) true
     *
     * @return The user that has changed
     */
    virtual MegaChatHandle getUserHandle() const;

    /**
     * @brief Returns whether the user is member of the chatroom (for groupchats),
     * or the user is contact with the peer (for 1on1 chats).
     *
     * @return True if the chat is active, false otherwise.
     */
    virtual bool isActive() const;

    /**
     * @brief Returns whether the chat is currently archived or not.
     * @return True if the chat is archived, false otherwise.
     */
    virtual bool isArchived() const;

    /**
     * @brief Returns the retention time for this chat
     * @return The retention time for this chat
     */
    virtual unsigned int getRetentionTime() const;

    /**
     * @brief Returns the creation timestamp of the chat.
     * @return The creation timestamp of the chat.
     */
    virtual int64_t getCreationTs() const;

    /**
     * @brief Returns whether the chat is a meeting room
     * @return True if chat is a meeting room
     */
    virtual bool isMeeting() const;

    /**
     * @brief Returns if waiting room is enabled for a chat
     * During calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @return True if waiting room is enabled
     */
    virtual bool isWaitingRoom() const;

    /**
     * @brief Returns if users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @return True if users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     */
    virtual bool isOpenInvite() const;

    /**
     * @brief Returns if during calls, non moderator users, must request permission to speak
     * @return True if during calls, non moderator users, must request permission to speak.
     */
    virtual bool isSpeakRequest() const;

    virtual int getChanges() const;
    virtual bool hasChanged(int changeType) const;
};

/**
 * @brief Interface to get all information related to chats of a MEGA account
 *
 * Implementations of this interface can receive all events (request, global, call, video).
 *
 * Multiple inheritance isn't used for compatibility with other programming languages
 *
 * The implementation will receive callbacks from an internal worker thread.
 *
 */
class MegaChatListener
{
public:
    virtual ~MegaChatListener() {}

    /**
     * @brief This function is called when there are new chats or relevant changes on existing chats.
     *
     * The possible changes that are notified are the following:
     *  - Title
     *  - Unread messages count
     *  - Online status
     *  - Visibility: the contact of 1on1 chat has changed. i.e. added or removed
     *  - Participants: new peer added or existing peer removed
     *  - Last message: the last relevant message in the chatroom
     *  - Last timestamp: the last date of any activity in the chatroom
     *  - Archived: when the chat becomes archived/unarchived
     *  - Calls: when there is a new call or a call has finished
     *  - Chat mode: when an user has set chat mode to private
     *  - Previewers: when the number of previewers has changed
     *  - Preview closed: when the chat preview has been closed
     *
     * The SDK retains the ownership of the MegaChatListItem in the second parameter.
     * The MegaChatListItem object will be valid until this function returns. If you
     * want to save the MegaChatListItem, use MegaChatListItem::copy
     *
     * @note changes about participants in chat link won't be notified until chat
     *  is logged in
     *
     * @param api MegaChatApi connected to the account
     * @param item MegaChatListItem representing a 1on1 or groupchat in the list.
     */
    virtual void onChatListItemUpdate(MegaChatApi* api, MegaChatListItem *item);

    /**
     * @brief This function is called when the status of the initialization has changed
     *
     * The possible values are:
     *  - MegaChatApi::INIT_ERROR = -1
     *  - MegaChatApi::INIT_WAITING_NEW_SESSION = 1
     *  - MegaChatApi::INIT_OFFLINE_SESSION = 2
     *  - MegaChatApi::INIT_ONLINE_SESSION = 3
     *
     * @param api MegaChatApi connected to the account
     * @param newState New state of initialization
     */
    virtual void onChatInitStateUpdate(MegaChatApi* api, int newState);

    /**
     * @brief This function is called when the online status of a user has changed
     *
     * @param api MegaChatApi connected to the account
     * @param userhandle MegaChatHandle of the user whose online status has changed
     * @param status New online status
     * @param inProgress Whether the reported status is being set or it is definitive (only for your own changes)
     *
     * @note When the online status is in progress, apps may notice showing a blinking status or similar.
     */
    virtual void onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress);

    /**
     * @brief This function is called when the presence configuration has changed
     *
     * @param api MegaChatApi connected to the account
     * @param config New presence configuration
     */
    virtual void onChatPresenceConfigUpdate(MegaChatApi* api, MegaChatPresenceConfig *config);

    /**
     * @brief This function is called when the connection state to a chatroom has changed
     *
     * The possible values are:
     *  - MegaChatApi::CHAT_CONNECTION_OFFLINE      = 0
     *  - MegaChatApi::CHAT_CONNECTION_IN_PROGRESS  = 1
     *  - MegaChatApi::CHAT_CONNECTION_LOGGING      = 2
     *  - MegaChatApi::CHAT_CONNECTION_ONLINE       = 3
     *
     * @note If \c chatid is MEGACHAT_INVALID_HANDLE, it means that you are connected to all
     * active chatrooms. It will only happens when \c newState is MegaChatApi::CHAT_CONNECTION_ONLINE.
     * The other connection states are not notified for all chats together, but only individually.
     *
     * @param api MegaChatApi connected to the account
     * @param chatid MegaChatHandle that identifies the chat room
     * @param newState New state of the connection
     */
    virtual void onChatConnectionStateUpdate(MegaChatApi* api, MegaChatHandle chatid, int newState);

    /**
     * @brief This function is called when server notifies last-green's time of the a user
     *
     * In order to receive this notification, MegaChatApi::requestLastGreen has to be called previously.
     *
     * @note If the requested user has disabled the visibility of last-green or has never been green,
     * this callback will NOT be triggered at all.
     *
     * If the value of \c lastGreen is 65535 minutes (the maximum), apps should show "long time ago"
     * or similar, rather than the specific time period.
     *
     * @param api MegaChatApi connected to the account
     * @param userhandle MegaChatHandle of the user whose last time green is notified
     * @param lastGreen Time elapsed (minutes) since the last time user was green
     */
    virtual void onChatPresenceLastGreen(MegaChatApi* api, MegaChatHandle userhandle, int lastGreen);

    /** @brief This function is called when an error occurred in an operation with karere Db
     * Possible returned values:
     *   - MegaChatApi::DB_ERROR_IO               = 1,    /// I/O error in Data base
     *   - MegaChatApi::DB_ERROR_FULL             = 2,    /// Database or disk is full
     *
     * @param error Numeric error code
     * @param errStr Error message
     */
    virtual void onDbError(MegaChatApi *api, int error, const char* msg);
};

/**
 * @brief Interface to receive information about one chatroom.
 *
 * A pointer to an implementation of this interface is required when calling MegaChatApi::openChatRoom.
 * When a chatroom is closed (MegaChatApi::closeChatRoom), the listener is automatically removed.
 * You can also register additional listeners by calling MegaChatApi::addChatRoomListener and remove them
 * by using MegaChatApi::removeChatRoomListener
 *
 * This interface uses MegaChatRoom and MegaChatMessage objects to provide information of the chatroom
 * and its messages respectively.
 *
 * The implementation will receive callbacks from an internal worker thread. *
 */
class MegaChatRoomListener
{
public:
    virtual ~MegaChatRoomListener() {}

    /**
     * @brief This function is called when there are changes in the chatroom
     *
     * The changes can include: a user join/leaves the chatroom, a user changes its name,
     * the unread messages count has changed, the online state of the connection to the
     * chat server has changed, the chat becomes archived/unarchived, there is a new call
     * or a call has finished, the chat has been changed into private mode, the number of
     * previewers has changed, the user has started/stopped typing.
     *
     * @note changes about participants in chat link won't be notified until chat
     * is logged in
     *
     * @param api MegaChatApi connected to the account
     * @param chat MegaChatRoom that contains the updates relatives to the chat
     */
    virtual void onChatRoomUpdate(MegaChatApi* api, MegaChatRoom *chat);

    /**
     * @brief This function is called when new messages are loaded
     *
     * You can use MegaChatApi::loadMessages to request loading messages.
     *
     * When there are no more message to load from the source reported by MegaChatApi::loadMessages or
     * there are no more history at all, this function is also called, but the second parameter will be NULL.
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg The MegaChatMessage object, or NULL if no more history available.
     */
    virtual void onMessageLoaded(MegaChatApi* api, MegaChatMessage *msg);   // loaded by loadMessages()

    /**
     * @brief This function is called when a new message is received
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg MegaChatMessage representing the received message
     */
    virtual void onMessageReceived(MegaChatApi* api, MegaChatMessage *msg);

    /**
     * @brief This function is called when an existing message is updated
     *
     * i.e. When a submitted message is confirmed by the server, the status chages
     * to MegaChatMessage::STATUS_SERVER_RECEIVED and its message id is considered definitive.
     *
     * An important case is when the edition of a message is rejected. In those cases, the message
     * status of \c msg will be MegaChatMessage::STATUS_SENDING_MANUAL and the app reason of rejection
     * is recorded in MegaChatMessage::getCode().
     *
     * Another edge case is when a new message was confirmed but the app didn't receive the confirmation
     * from the server. In that case, you will end up with a message in MegaChatMessage::STATUS_SENDING
     * due to the sending retry, another one in MegaChatMessage::STATUS_SERVER_RECEIVED or
     * MegaChatMessage::STATUS_DELIVERED due to the message already confirmed/delivered. Finally, you
     * will receive this callback updating the status to MegaChatMessage::STATUS_SERVER_REJECTED with
     * MegaChatMessage::getCode() equal to 0 and the corresponding MegaChatMessage::getTempId().
     * The app should discard the message in sending status, in pro of the confirmed message to avoid
     * duplicated message in the history.
     * @note if MegaChatApi::isMessageReceptionConfirmationActive returns false, messages may never
     * reach the status delivered, since the target user will not send the required acknowledge to the
     * server upon reception.
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg MegaChatMessage representing the updated message
     */
    virtual void onMessageUpdate(MegaChatApi* api, MegaChatMessage *msg);

    /**
     * @brief This function is called when the local history of a chatroom is about to be discarded and
     * reloaded from server.
     *
     * Server can reject to provide all new messages if there are too many since last connection. In that case,
     * all the locally-known history will be discarded (both from memory and cache) and the server will provide
     * the most recent messages in this chatroom.
     *
     * @note When this callback is received, any reference to messages should be discarded. New messages will be
     * loaded from server and notified as in the case where there's no cached messages at all.
     *
     * @param api MegaChatApi connected to the account
     * @param chat MegaChatRoom whose local history is about to be discarded
     */
    virtual void onHistoryReloaded(MegaChatApi* api, MegaChatRoom *chat);


    /**
     * @brief This function is called when a message has been reacted (or an existing reaction has been removed)
     *
     * @param api MegaChatApi connected to the account
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL-terminated string that represents the reaction
     * @param count Number of users who have reacted to this message with the same reaction
     */
    virtual void onReactionUpdate(MegaChatApi* api, MegaChatHandle msgid, const char* reaction, int count);

    /**
     * @brief This function is called when we need to clear messages previous to retention time,
     * all messages previous to received msg as parameter must be cleared.
     *
     * @param api MegaChatApi connected to the account
     * @param msg Most recent message whose timestamp has exceeded retention time
     */
    virtual void onHistoryTruncatedByRetentionTime(MegaChatApi* /*api*/, MegaChatMessage* /*msg*/);
};

/**
 * @brief Interface to get notifications to show to the user on mobile devices
 *
 * Mobile platforms usually provide a framework to push-notifications to mobile devices.
 * The app needs to register a push-notification token (@see MegaApi::registerPushNotifications in the SDK)
 * in order to get those notifications (triggered by MEGA servers on certain events).
 *
 * This listener provides the required data to prepare platform-specific notifications for
 * several events, such as new messages received, deletions, truncation of history...
 *
 * Multiple inheritance isn't used for compatibility with other programming languages
 *
 * The implementation will receive callbacks from an internal worker thread.
 *
 */
class MegaChatNotificationListener
{
public:
    virtual ~MegaChatNotificationListener() {}

    /**
     * @brief This function is called when there are interesting events for notifications
     *
     * The possible events that are notified are the following:
     *  - Reception of a new message from other user if still unseen.
     *  - Edition/deletion of received unseen messages.
     *  - Trucate of history (for both, when truncate is ours or theirs).
     *  - Changes on the lastest message seen by us (don't notify previous unseen messages).
     *
     * Depending on the status of the message (seen or unseen), if it has been edited/deleted,
     * or even on the type of the message (truncate), the app should add/update/clear the corresponding
     * notifications on the mobile device.
     *
     * The SDK retains the ownership of the MegaChatMessage in the third parameter.
     * The MegaChatMessage object will be valid until this function returns. If you
     * want to save the MegaChatMessage, use MegaChatMessage::copy
     *
     * @param api MegaChatApi connected to the account
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msg MegaChatMessage representing a 1on1 or groupchat in the list.
     */
    virtual void onChatNotification(MegaChatApi* api, MegaChatHandle chatid, MegaChatMessage *msg);
};

/**
 * @brief Interface to receive information about node history of a chatroom.
 *
 * A pointer to an implementation of this interface is required when calling MegaChatApi::openNodeHistory.
 * When node history of a chatroom is closed (MegaChatApi::closeNodeHistory), the listener is automatically removed.
 * You can also register additional listeners by calling MegaChatApi::addNodeHistoryListener and remove them
 * by using MegaChatApi::removeNodeHistoryListener
 *
 * The implementation will receive callbacks from an internal worker thread.
 */
class MegaChatNodeHistoryListener
{
public:
    virtual ~MegaChatNodeHistoryListener() {}

    /**
     * @brief This function is called when new attachment messages are loaded
     *
     * You can use MegaChatApi::loadAttachments to request loading messages.
     *
     * When there are no more message to load from the source reported by MegaChatApi::loadAttachments or
     * there are no more history at all, this function is also called, but the second parameter will be NULL.
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg The MegaChatMessage object, or NULL if no more history available.
     */
    virtual void onAttachmentLoaded(MegaChatApi *api, MegaChatMessage *msg);

    /**
     * @brief This function is called when a new attachment message is received
     *
     * The SDK retains the ownership of the MegaChatMessage in the second parameter. The MegaChatMessage
     * object will be valid until this function returns. If you want to save the MegaChatMessage object,
     * use MegaChatMessage::copy for the message.
     *
     * @param api MegaChatApi connected to the account
     * @param msg MegaChatMessage representing the received message
     */
    virtual void onAttachmentReceived(MegaChatApi *api, MegaChatMessage *msg);

    /**
     * @brief This function is called when an attachment message is deleted
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message that has been deleted
     */
    virtual void onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid);

    /**
     * @brief This function is called when history is trucated
     *
     * If no messages are left in the node-history, the msgid will be MEGACHAT_INVALID_HANDLE.
     *
     * @param api MegaChatApi connected to the account
     * @param msgid id of the message from which history has been trucated
     */
    virtual void onTruncate(MegaChatApi *api, MegaChatHandle msgid);
};

/**
 * @brief This class represents a set of meetings flags in a bit mask format, where every flag is represented by 1 bit
 */
class MegaChatScheduledFlags
{
public:
    enum
    {
        FLAGS_SEND_EMAILS      = 0, // API will send out calendar emails for this meeting if it's enabled
        FLAGS_SIZE             = 1, // size in bits of flags bitmask
    };

    virtual ~MegaChatScheduledFlags();

    /**
     * @brief Creates a new instance of MegaChatScheduledFlags
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatScheduledFlags* createInstance();

    /**
     * @brief Creates a copy of this virtual MegaChatScheduledFlags object
     *
     * The resulting object is fully independent of the source MegaChatScheduledFlags,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledFlags object
     */
    virtual MegaChatScheduledFlags* copy() const;

    /**
     * @brief Reset the value of all options (to disabled)
     */
    virtual void reset();

    /**
     * @brief Enables or disables the value of sending emails flag.
     * If this flag is enabled, API will send out calendar emails for this meeting
     */
    virtual void setSendEmails(bool /*enabled*/);

    /**
     * @brief Returns true if sending emails flag is enabled
     * If this flag is enabled, API will send out calendar emails for this meeting
     *
     * @return True if sending emails flag is enabled, otherwise returns false.
     */
    virtual bool sendEmails() const;

    /**
     * @brief Returns true if all flags are disabled
     *
     * @return True if all flags are disabled, otherwise returns false.
     */
    virtual bool isEmpty() const;
};

/**
 * @brief This class represents a set of set of rules that can be defined for a Scheduled meeting.
 */
class MegaChatScheduledRules
{
public:
    enum {
        FREQ_INVALID    = -1,
        FREQ_DAILY      = 0,
        FREQ_WEEKLY     = 1,
        FREQ_MONTHLY    = 2,
    };

    static constexpr int INTERVAL_INVALID = 0;
    virtual ~MegaChatScheduledRules();

    /**
     * @brief Creates a new instance of MegaChatScheduledRules
     *
     * @param freq: scheduled meeting frequency, this is used in conjunction with interval
     * valid values for this param:
     *  + MegaChatScheduledRules::FREQ_DAILY
     *  + MegaChatScheduledRules::FREQ_WEEKLY
     *  + MegaChatScheduledRules::FREQ_MONTHLY
     *
     * @param interval: repetition interval in relation to the frequency
     * @param until: specifies when the repetitions should end
     * @param byWeekDay: allows us to specify that an event will only occur on given week day/s.
     * to use this param, freq param must be set to MegaChatScheduledRules::FREQ_WEEKLY
     *
     * @param byMonthDay: allows us to specify that an event will only occur on a given day/s of the month
     * to use this param, freq param must be set to MegaChatScheduledRules::FREQ_MONTHLY
     *
     * @param byMonthWeekDay: allows us to specify that an event will only occurs on a specific weekday offset of the month. (i.e every 2nd Sunday of each month)
     * to use this param, freq param must be set to MegaChatScheduledRules::FREQ_MONTHLY
     *
     * Important: byWeekDay, byMonthDay and byMonthWeekDay are not compatible between them, so only one of these values, can be set at the same time.
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatScheduledRules* createInstance(int freq,
                                                  int interval = INTERVAL_INVALID,
                                                  MegaChatTimeStamp until = MEGACHAT_INVALID_TIMESTAMP,
                                                  const ::mega::MegaIntegerList* byWeekDay = NULL,
                                                  const ::mega::MegaIntegerList* byMonthDay = NULL,
                                                  const ::mega::MegaIntegerMap* byMonthWeekDay = NULL);

    /**
     * @brief Creates a copy of this MegaChatScheduledRules object
     *
     * The resulting object is fully independent of the source MegaChatScheduledRules,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledRules object
     */
    virtual MegaChatScheduledRules* copy() const;

    /**
     * @brief Sets the frequency of the scheduled meeting. This is used in conjunction with interval,
     * to allow for a repeatable skips in the event timeline.
     *
     * Valid values for frequency are:
     *  - MegaChatScheduledRules::FREQ_DAILY   = 0
     *  - MegaChatScheduledRules::FREQ_WEEKLY  = 1
     *  - MegaChatScheduledRules::FREQ_MONTHLY = 2
     *
     * @param freq The frequency of the scheduled meeting
     */
    virtual void setFreq(int freq);

    /**
     * @brief Sets the repetition interval in relation to the frequency
     *
     * @param interval The repetition interval in relation to the frequency
     */
    virtual void setInterval(int interval);

    /**
     * @brief Sets the until value that indicates when the repetitions should end
     *
     * @param until Value that indicates when the repetitions should end
     */
    virtual void setUntil(MegaChatTimeStamp until);

    /**
     * @brief Sets the week days when the event will occur
     *
     * @param byWeekDay A MegaIntegerList with the week days when the event will occur
     */
    virtual void setByWeekDay(const ::mega::MegaIntegerList* byWeekDay);

    /**
     * @brief Sets the days of the month when the event will occur
     *
     * @param byMonthDay A MegaIntegerList with the days of the month when the event will occur
     */
    virtual void setByMonthDay(const ::mega::MegaIntegerList* byMonthDay);

    /**
     * @brief Sets one or multiple weekday offset (ie: [5,4] event will occur every 5th Thursday of each month)
     *
     * @return A MegaIntegerMap <offset, weekday> that allows to specify one or multiple weekday offset
     */
    virtual void setByMonthWeekDay(const ::mega::MegaIntegerMap* byMonthWeekDay);

    /**
     * @brief Returns the frequency of the scheduled. This value is used in conjunction with interval,
     * to allow for a repeatable skips in the event timeline.
     *
     * Valid values for frequency are:
     *  - MegaChatScheduledRules::FREQ_DAILY   = 0
     *  - MegaChatScheduledRules::FREQ_WEEKLY  = 1
     *  - MegaChatScheduledRules::FREQ_MONTHLY = 2
     *
     * @return The frequency of the scheduled meeting
     */
    virtual int freq() const;

    /**
     * @brief Returns repetition interval in relation to the frequency
     *
     * @return The inverval in relation to the frequency of the scheduled meeting
     */
    virtual int interval() const;

    /**
     * @brief Returns when the repetitions should end.
     *
     * @note: If this method returns MEGACHAT_INVALID_TIMESTAMP it means that
     * the repetitions will never end.
     *
     * @return When the repetitions should end
     */
    virtual MegaChatTimeStamp until() const;

    /**
     * @brief Returns a MegaIntegerList with the week days when the event will occur
     *
     * @return A MegaIntegerList with the week days when the event will occur
     */
    virtual const mega::MegaIntegerList* byWeekDay() const;

    /**
     * @brief Returns a MegaIntegerList with the days of the month when the event will occur
     *
     * @return A MegaIntegerList with the days of the month when the event will occur
     */
    virtual const mega::MegaIntegerList* byMonthDay() const;

    /**
     * @brief Returns a MegaIntegerMap <offset, weekday> that allows to specify one or multiple weekday offset (ie: [5,4] event will occur every 5th Thursday of each month)
     *
     * @return A MegaIntegerMap <offset, weekday> that allows to specify one or multiple weekday offset
     */
    virtual const mega::MegaIntegerMap* byMonthWeekDay() const;

    /**
     * @brief Returns if a given frequency is valid or not
     *
     * @return True if freq is valid, otherwise false
     */
    static bool isValidFreq(int freq);

    /**
     * @brief Returns if a given interval is valid or not
     *
     * @return True if interval is valid, otherwise false
     */
    static bool isValidInterval(int interval);
};

/**
 * @brief This class represents a scheduled meeting. Scheduled Meetings allows the user to specify an event that will occur in the future.
 * The user can also specify a set of rules for repetition, these rules enable an event to reoccur periodically.
 *
 * Important consideration:
 * A Chatroom only should have one root scheduled meeting associated, it means that just one scheduled meeting for a chatroom,
 * should have an invalid parent sched Id (MegaChatScheduledMeeting::parentSchedId)
 *
 */
class MegaChatScheduledMeeting
{
public:
    enum
    {
       SC_NEW_SCHED        = 0,
       SC_PARENT           = 1,
       SC_TZONE            = 2,
       SC_START            = 3,
       SC_END              = 4,
       SC_TITLE            = 5,
       SC_DESC             = 6,
       SC_ATTR             = 7,
       SC_OVERR            = 8,
       SC_CANC             = 9,
       SC_FLAGS            = 10,
       SC_RULES            = 11,
       SC_FLAGS_SIZE       = 12,
    };

    static constexpr unsigned int MAX_TITLE_LENGTH = 30;
    static constexpr unsigned int MAX_DESC_LENGTH = 3000;
    static constexpr unsigned int MIN_OCURRENCES = 10;
    static constexpr unsigned int NUM_OCURRENCES_REQ = 20;

    virtual ~MegaChatScheduledMeeting();

    /**
     * @brief Creates a new instance of MegaChatScheduledMeeting
     *
     * @param chatid        : chat handle
     * @param schedId       : scheduled meeting handle
     * @param parentSchedId : parent scheduled meeting handle
     * @param cancelled     : cancelled flag
     * @param timezone      : timeZone
     * @param startDateTime : start dateTime (unix timestamp UTC)
     * @param endDateTime   : end dateTime (unix timestamp UTC)
     * @param title         : meeting title
     * @param description   : meeting description
     * @param attributes    : attributes to store any additional data
     * @param overrides     : start dateTime of the original meeting series event to be replaced (unix timestamp UTC)
     * @param flags         : flags bitmask (used to store additional boolean settings as a bitmask)
     * @param rules         : scheduled meetings rules
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatScheduledMeeting* createInstance (MegaChatHandle chatid, MegaChatHandle schedId, MegaChatHandle parentSchedId, MegaChatHandle organizerUserId,
                                                     int cancelled, const char* timezone, MegaChatTimeStamp startDateTime,
                                                     MegaChatTimeStamp endDateTime, const char* title, const char* description, const char* attributes,
                                                     MegaChatTimeStamp overrides, const MegaChatScheduledFlags *flags, const MegaChatScheduledRules *rules);

    /**
     * @brief Creates a copy of this MegaChatScheduledMeeting object
     *
     * The resulting object is fully independent of the source MegaChatScheduledMeeting,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledMeeting object
     */
    virtual MegaChatScheduledMeeting* copy() const;

    /**
     * @brief Returns if scheduled meeting is cancelled or not
     *
     * @return True if scheduled meeting is cancelled, otherwise returns false
     */
    virtual int cancelled() const;

    /**
     * @brief Returns true if this scheduled meeting has an specific change
     *
     * This value is only useful for call notified by MegaChatScheduledMeetingListener::onChatSchedMeetingUpdate
     * that can notify about scheduled meetings modifications. The value only will be valid inside
     * MegaChatScheduledMeetingListener::onChatSchedMeetingUpdate. A copy of MegaChatScheduledMeeting will be
     * necessary to use outside this callback
     *
     * In other cases, the return value of this function will be always false.
     *
     * @param changeType The type of change to check. It can be one of the following values:
     *
     * - MegaChatScheduledMeeting::SC_PARENT    [1]  - Parent scheduled meeting id has changed
     * - MegaChatScheduledMeeting::SC_TZONE     [2]  - Timezone has changed
     * - MegaChatScheduledMeeting::SC_START     [3]  - Start date time has changed
     * - MegaChatScheduledMeeting::SC_END       [4]  - End date time has changed
     * - MegaChatScheduledMeeting::SC_TITLE     [5]  - Title has changed
     * - MegaChatScheduledMeeting::SC_DESC      [6]  - Description has changed
     * - MegaChatScheduledMeeting::SC_ATTR      [7]  - Attributes have changed
     * - MegaChatScheduledMeeting::SC_OVERR     [8]  - Override date time has changed
     * - MegaChatScheduledMeeting::SC_CANC      [9]  - Cancelled flag has changed
     * - MegaChatScheduledMeeting::SC_FLAGS     [10] - Scheduled meetings flags have changed
     * - MegaChatScheduledMeeting::SC_RULES     [11] - Repetition rules have changed
     *
     * @return true if this scheduled meeting has an specific change
     */
    virtual bool hasChanged(size_t change) const;

    /**
     * @brief Returns if the MegaChatScheduledMeeting is new
     *
     * @return True if the MegaChatScheduledMeeting is new
     */
    virtual bool isNew() const;

    /**
     * @brief Returns if the MegaChatScheduledMeeting has been removed
     *
     * @return True if the MegaChatScheduledMeeting has been removed
     */
    virtual bool isDeleted() const;

    /**
     * @brief Returns the MegaChatHandle of the chat
     *
     * @return MegaChatHandle of the chat
     */
    virtual MegaChatHandle chatId() const;

    /**
     * @brief Returns the MegaChatHandle that identifies the scheduled meeting
     *
     * @return MegaChatHandle that identifies the scheduled meeting
     */
    virtual MegaChatHandle schedId() const;

    /**
     * @brief Returns the MegaChatHandle that identifies the parent scheduled meeting
     *
     * @return MegaChatHandle that identifies the parent scheduled meeting
     */
    virtual MegaChatHandle parentSchedId() const;

    /**
     * @brief Returns the MegaChatHandle of the organizer user of the scheduled meeting
     *
     * @return MegaChatHandle of the organizer user of the scheduled meeting
     */
    virtual MegaChatHandle organizerUserId() const;

    /**
     * @brief Returns the time zone
     *
     * @return time zone
     */
    virtual const char* timezone() const;

    /**
     * @brief Returns the start dateTime (for the first occurrence) of the scheduled Meeting (unix timestamp UTC)
     *
     * To check if a recurrent scheduled meeting is a past meeting, you can check MegaChatScheduledRules::until,
     * which returns the dateDate when the repetitions ends, because this method returns the start dateTime of the first occurrence
     * without taking into account if that occurrence is in the future or in the past.
     *
     * @note The value returned by this method should only be used for purposes related to this scheduled meeting, not for
     * it's occurrences. Any information related to the occurrences of this scheduled meeting, must be retrieved by calling
     * MegaChatApi::fetchScheduledMeetingOccurrencesByChat
     *
     * @return the start dateTime of the scheduled Meeting
     */
    virtual MegaChatTimeStamp startDateTime() const;

    /**
     * @brief Returns the end dateTime of the scheduled Meeting (unix timestamp UTC)
     *
     * @return the end dateTime of the scheduled Meeting
     */
    virtual MegaChatTimeStamp endDateTime() const;

    /**
     * @brief Returns the scheduled meeting title
     *
     * @return The title of the scheduled meeting
     */
    virtual const char* title() const;

    /**
     * @brief Returns the scheduled meeting description
     *
     * @return The description of the scheduled meeting
     */
    virtual const char* description() const;

    /**
     * @brief Returns additional scheduled meetings attributes
     *
     * @return Additional scheduled meetings attributes
     */
    virtual const char* attributes() const;

    /**
     * @brief Returns the start dateTime of the original meeting series event to be replaced (unix timestamp UTC)
     *
     * @return the start dateTime of the original meeting series event to be replaced
     */
    virtual MegaChatTimeStamp overrides() const;

    /**
     * @brief Returns a pointer to MegaChatScheduledFlags that contains the scheduled meetings flags
     *
     * The SDK retains the ownership of the MegaChatScheduledFlags
     *
     * @return A pointer to MegaChatScheduledFlags that contains the scheduled meetings flags
     */
    virtual MegaChatScheduledFlags* flags() const;

    /**
     * @brief Returns a pointer to MegaChatScheduledRules that contains the scheduled meetings rules
     *
     * The SDK retains the ownership of the MegaChatScheduledRules
     *
     * @return A pointer to MegaChatScheduledRules that contains the scheduled meetings rules
     */
    virtual MegaChatScheduledRules* rules() const;


    /**
     * @brief Returns if scheduled meeting title length is valid or not
     *
     * @return True if scheduled meeting title length is valid, otherwise returns false
     */
    static int isValidTitleLength(const char* title);

    /**
     * @brief Returns if scheduled meeting description length is valid or not
     *
     * @return True if scheduled meeting description length is valid, otherwise returns false
     */
    static int isValidDescriptionLength(const char* desc);
};

/**
 * @brief This class represents a scheduled meeting occurrence.
 * A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
 * A scheduled meeting can produce one or multiple scheduled meeting occurrences
 */
class MegaChatScheduledMeetingOccurr
{
public:
    virtual ~MegaChatScheduledMeetingOccurr();

    /**
     * @brief Creates a copy of this MegaChatScheduledMeetingOccurr object
     *
     * The resulting object is fully independent of the source MegaChatScheduledMeetingOccurr,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledMeetingOccurr object
     */
    virtual MegaChatScheduledMeetingOccurr* copy() const;

    /**
     * @brief Returns if scheduled meeting occurrence is cancelled or not
     *
     * @return True if scheduled meeting occurrence is cancelled, otherwise returns false
     */
    virtual int cancelled() const;

    /**
     * @brief Returns the MegaChatHandle that identifies the scheduled meeting
     *
     * @return MegaChatHandle that identifies the scheduled meeting
     */
    virtual MegaChatHandle schedId() const;

    /**
     * @brief Returns the MegaChatHandle that identifies the parent scheduled meeting
     *
     * @return MegaChatHandle that identifies the parent scheduled meeting
     */
    virtual MegaChatHandle parentSchedId() const;

    /**
     * @brief Returns the start dateTime of the original meeting series event to be replaced (unix timestamp)
     *
     * @return the start dateTime of the original meeting series event to be replaced
     */
    virtual MegaChatTimeStamp overrides() const;

    /**
     * @brief Returns the time zone
     *
     * @return time zone
     */
    virtual const char* timezone() const;

    /**
     * @brief Returns the start dateTime of the scheduled Meeting occurrence (unix timestamp UTC)
     *
     * @return the start dateTime of the scheduled Meeting occurrence
     */
    virtual MegaChatTimeStamp startDateTime() const;

    /**
     * @brief Returns the end dateTime of the scheduled Meeting occurrence (unix timestamp UTC)
     *
     * @return the end dateTime of the scheduled Meeting occurrence
     */
    virtual MegaChatTimeStamp endDateTime() const;
};

/**
 * @brief List of MegaChatScheduledMeeting objects
 */
class MegaChatScheduledMeetingList
{
public:
    virtual ~MegaChatScheduledMeetingList();

    /**
     * @brief Creates a new instance of MegaChatScheduledMeetingList
     *
     * You take the ownership of the returned object
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatScheduledMeetingList* createInstance();

    /**
     * @brief Creates a copy of this MegaChatScheduledMeetingList object
     *
     * The resulting object is fully independent of the source MegaChatScheduledMeetingList,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledMeetingList object
     */
    virtual MegaChatScheduledMeetingList *copy() const;

    /**
     * @brief Returns the number of elements in the list
     * @return Number of elements in the list
     */
    virtual unsigned long size() const;

    /**
     * @brief Returns the MegaChatScheduledMeeting at the position i in the MegaChatScheduledMeetingList
     *
     * If the index is >= the size of the list, this function returns NULL.
     *
     * @param i Position of the element that we want to get for the list
     * @return MegaChatScheduledMeeting at the position i in the MegaChatScheduledMeetingList
     */
    virtual const MegaChatScheduledMeeting* at(unsigned long i) const;

    /**
     * @brief Add element to the MegaChatScheduledMeetingList
     *
     * The SDK adquires the ownership of provided MegaChatScheduledMeeting
     *
     * @param sm MegaChatScheduledMeeting to add to list
     */
    virtual void insert(MegaChatScheduledMeeting* sm);

    /**
     * @brief Clears the MegaChatScheduledMeetingList
     */
    virtual void clear();
};

/**
 * @brief List of MegaChatScheduledMeetingOccurr objects
 */
class MegaChatScheduledMeetingOccurrList
{
public:
    virtual ~MegaChatScheduledMeetingOccurrList();

    /**
     * @brief Creates a new instance of MegaChatScheduledMeetingList
     *
     * You take the ownership of the returned object
     *
     * @return A pointer to the superclass of the private object
     */
    static MegaChatScheduledMeetingOccurrList* createInstance();

    /**
     * @brief Creates a copy of this MegaChatScheduledMeetingOccurrList object
     *
     * The resulting object is fully independent of the source MegaChatScheduledMeetingOccurrList,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You take the ownership of the returned object
     *
     * @return Copy of the MegaChatScheduledMeetingOccurrList object
     */
    virtual MegaChatScheduledMeetingOccurrList *copy() const;

    /**
     * @brief Returns the number of elements in the list
     * @return Number of elements in the list
     */
    virtual unsigned long size() const;

    /**
     * @brief Returns the MegaChatScheduledMeetingOccurr at the position i in the MegaChatScheduledMeetingOccurrList
     *
     * If the index is >= the size of the list, this function returns NULL.
     *
     * @param i Position of the element that we want to get for the list
     * @return MegaChatScheduledMeetingOccurr at the position i in the MegaChatScheduledMeetingOccurrList
     */
    virtual const MegaChatScheduledMeetingOccurr* at(unsigned long i) const;

    /**
     * @brief Add element to the MegaChatScheduledMeetingOccurrList
     *
     * The SDK adquires the ownership of provided MegaChatScheduledMeetingOccurr
     *
     * @param sm MegaChatScheduledMeetingOccurr to add to list
     */
    virtual void insert(MegaChatScheduledMeetingOccurr* sm);

    /**
     * @brief Clears the MegaChatScheduledMeetingOccurrList
     */
    virtual void clear();
};
}

#endif // MEGACHATAPI_H
