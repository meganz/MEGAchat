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
class MegaChatListItem;

/**
 * @brief Provide information about a call
 *
 * A call can be obtained with the callback MegaChatCallListener::onChatCallUpdate where MegaChatApi has
 * the ownership of the object. Or by a getter where a copy is provided, MegaChatApi::getChatCall
 * and MegaChatApi::getChatCallByCallId
 *
 * The states that a call has during its life time are:
 * Outgoing call:
 *  - CALL_STATUS_INITIAL
 *  - CALL_STATUS_HAS_LOCAL_STREAM
 *  - CALL_STATUS_REQUEST_SENT
 *  - CALL_STATUS_IN_PROGRESS
 *  - CALL_STATUS_TERMINATING
 *  - CALL_STATUS_DESTROYED
 *
 * Incoming call:
 *  - CALL_STATUS_RING_IN
 *  - CALL_STATUS_JOINING
 *  - CALL_STATUS_IN_PROGRESS
 *  - CALL_STATUS_TERMINATING
 *  - CALL_STATUS_DESTROYED
 */
class MegaChatCall
{
public:
    enum
    {
        CALL_STATUS_INITIAL = 0,        /// Initial state
        CALL_STATUS_HAS_LOCAL_STREAM,   /// Call has obtained a local video-audio stream
        CALL_STATUS_REQUEST_SENT,       /// Call request has been sent to receiver
        CALL_STATUS_RING_IN,            /// Call is at incoming state, it has not been answered or rejected yet
        CALL_STATUS_JOINING,            /// Intermediate state, while connection is stablished
        CALL_STATUS_IN_PROGRESS,        /// Call is stablished and there is a full communication
        CALL_STATUS_TERMINATING,        ///
        CALL_STATUS_DESTROYED,          /// Call is finished and resources can be released
    };

    enum
    {
        CHANGE_TYPE_STATUS = 0x01,          /// Call status has changed
        CHANGE_TYPE_LOCAL_AVFLAGS = 0x02,   /// Local audio/video flags has changed
        CHANGE_TYPE_REMOTE_AVFLAGS = 0x04,  /// Remote audio/video flags has changed
        CHANGE_TYPE_TEMPORARY_ERROR = 0x08, /// New temporary error is notified
        CHANGE_TYPE_RINGING_STATUS = 0x10,  /// Peer has change its ringing state
    };

    enum
    {
        TERM_CODE_USER_HANGUP       = 0,    /// Normal user hangup
        TERM_CODE_CALL_REQ_CANCEL   = 1,    /// Call request was canceled before call was answered
        TERM_CODE_CALL_REJECT       = 2,    /// Outgoing call has been rejected by the peer OR incoming call has been rejected by
                                            /// another client of our user
        TERM_CODE_ANSWER_ELSE_WHERE = 3,    /// Call was answered on another device of ours
        TERM_CODE_ANSWER_TIMEOUT    = 5,    /// Call was not answered in a timely manner
        TERM_CODE_RING_OUT_TIMEOUT  = 6,    /// We have sent a call request but no RINGING received within this timeout - no other
                                            /// users are online
        TERM_CODE_APP_TERMINATING   = 7,    /// The application is terminating
        TERM_CODE_BUSY              = 9,    /// Peer is in another call
        TERM_CODE_NOT_FINISHED      = 10,   /// The call is in progress, no termination code yet
        TERM_CODE_ERROR             = 21    /// Notify any error type
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
     * @brief Returns the status of the call
     *
     * @return the call status
     * Valid values are:
     *  - CALL_STATUS_INITIAL = 0
     *  - CALL_STATUS_HAS_LOCAL_STREAM = 1
     *  - CALL_STATUS_REQUEST_SENT = 2
     *  - CALL_STATUS_RING_IN = 3
     *  - CALL_STATUS_JOINING = 4
     *  - CALL_STATUS_IN_PROGRESS = 5
     *  - CALL_STATUS_TERMINATING = 6
     *  - CALL_STATUS_DESTROYED = 7
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
    virtual MegaChatHandle getId() const;

    /**
     * @brief Return audio state for local
     *
     * @return true if audio is enable, false if audio is disable
     */
    virtual bool hasLocalAudio();

    /**
     * @brief Return audio state for remote
     *
     * @return true if audio is enable, false if audio is disable
     */
    virtual bool hasRemoteAudio();

    /**
     * @brief Return video state for local
     *
     * @return true if video is enable, false if video is disable
     */
    virtual bool hasLocalVideo();

    /**
     * @brief Return video state for remote
     *
     * @return true if video is enable, false if video is disable
     */
    virtual bool hasRemoteVideo();

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
     * - MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS  = 0x04
     * Check MegaChatCall::hasAudio() and MegaChatCall::hasVideo() value
     *
     * - MegaChatCall::CHANGE_TYPE_TEMPORARY_ERROR  = 0x08
     * Check MegaChatCall::getTemporaryError() value
     *
     * - MegaChatCall::CHANGE_TYPE_RINGING_STATUS = 0x10
     * Check MegaChatCall::isRinging() value
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
     * - MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS  = 0x04
     * Check MegaChatCall::hasAudio() and MegaChatCall::hasVideo() value
     *
     * - MegaChatCall::CHANGE_TYPE_TEMPORARY_ERROR  = 0x08
     * Check MegaChatCall::getTemporaryError() value
     *
     * - MegaChatCall::CHANGE_TYPE_RINGING_STATUS = 0x10
     * Check MegaChatCall::isRinging() value
     *
     * @return true if this call has an specific change
     */
    virtual bool hasChanged(int changeType) const;

    /**
     * @brief Return call duration
     *
     * @note If the call is not finished yet, the returned value representes the elapsed time
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
     * @brief Returns the content of the temporary error
     *
     * This temporary error is cleared after notification through MegaChatCallListener::onChatCallUpdate
     *
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatCall object is deleted.
     *
     * @return Content of the error. If there isn't a error, it returns a empty string.
     */
    virtual const char *getTemporaryError() const;

    /**
     * @brief Returns the termination code for this call
     *
     * @note If the call is not finished yet, it returns MegaChatCall::TERM_CODE_NOT_FINISHED.
     *
     * To check if the call was terminated locally or remotely, see MegaChatCall::isLocalTermCode().
     *
     * @return termination code for the call
     */
    virtual int getTermCode() const;

    /**
     * @brief Returns if the call finished locally or remotely
     *
     * @return True if the call finished locally. False if the call finished remotely
     */
    virtual bool isLocalTermCode() const;

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
};

/**
 * @brief Interface to get video frames from calls
 *
 * The same interface is used to receive local or remote video, but it has to be un/registered
 * by differents functions:
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
        TYPE_INVALID                = 0,    /// Invalid type
        TYPE_NORMAL                 = 1,    /// Regular text message
        TYPE_ALTER_PARTICIPANTS     = 2,    /// Management message indicating the participants in the chat have changed
        TYPE_TRUNCATE               = 3,    /// Management message indicating the history of the chat has been truncated
        TYPE_PRIV_CHANGE            = 4,    /// Management message indicating the privilege level of a user has changed
        TYPE_CHAT_TITLE             = 5,    /// Management message indicating the title of the chat has changed
        TYPE_NODE_ATTACHMENT        = 16,   /// User message including info about shared nodes
        TYPE_REVOKE_NODE_ATTACHMENT = 17,   /// User message including info about a node that has stopped being shared (obsolete)
        TYPE_CONTACT_ATTACHMENT     = 18,   /// User message including info about shared contacts
    };

    enum
    {
        CHANGE_TYPE_STATUS          = 0x01,
        CHANGE_TYPE_CONTENT         = 0x02,
        CHANGE_TYPE_ACCESS          = 0x04  /// When the access to attached nodes has changed (obsolete)
    };

    enum
    {
        REASON_PEERS_CHANGED        = 1,    /// Group chat participants have changed
        REASON_TOO_OLD              = 2,    /// Message is too old to auto-retry sending
        REASON_GENERAL_REJECT       = 3,    /// chatd rejected the message, for unknown reason
        REASON_NO_WRITE_ACCESS      = 4,    /// Read-only privilege or not belong to the chatroom
        REASON_NO_CHANGES           = 6     /// Edited message has the same content than original message
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
     * @brief Returns the handle of the user.
     *
     * @return For outgoing messages, it returns the handle of the target user.
     * For incoming messages, it returns the handle of the sender.
     */
    virtual MegaChatHandle getUserHandle() const;

    /**
     * @brief Returns the type of message.
     *
     * Valid values are:
     *  - TYPE_INVALID: Invalid type
     *  - TYPE_NORMAL: Regular text message
     *  - TYPE_ALTER_PARTICIPANTS: Management message indicating the participants in the chat have changed
     *  - TYPE_TRUNCATE: Management message indicating the history of the chat has been truncated
     *  - TYPE_PRIV_CHANGE: Management message indicating the privilege level of a user has changed
     *  - TYPE_CHAT_TITLE: Management message indicating the title of the chat has changed
     *  - TYPE_ATTACHMENT: User message including info about a shared node
     *  - TYPE_REVOKE_ATTACHMENT: User message including info about a node that has stopped being shared
     *  - TYPE_CONTACT: User message including info about a contact
     *
     * @return Returns the Type of message.
     */
    virtual int getType() const;

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
     *
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
     * @return list with MegaNode
     */
    virtual mega::MegaNodeList *getMegaNodeList() const;

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
     * @return true if this message has an specific change
     */
    virtual bool hasChanged(int changeType) const;
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
        TYPE_CONNECT,   // connect to chatd (call it after login+fetchnodes with MegaApi)
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
        TYPE_LOAD_AUDIO_VIDEO_DEVICES,
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
     * @brief Returns the type of parameter related to the request
     *
     * This value is valid for these requests:
     * - MegaChatApi::enableAudio - Returns MegaChatRequest::AUDIO
     * - MegaChatApi::disableAudio - Returns MegaChatRequest::AUDIO
     * - MegaChatApi::enableVideo - Returns MegaChatRequest::VIDEO
     * - MegaChatApi::disableVideo - Returns MegaChatRequest::VIDEO
     * - MegaChatApi::answerChatCall - Returns one
     * - MegaChatApi::rejectChatCall - Returns zero
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
 * in order to prevent becoming MegaChatApi::STATUS_AWAY automatically after the timeout. *
 * You can check if the autoaway mechanism is active by calling \c MegaChatApi::isSignalActivityRequired
 * or also by checking \c MegaChatPresenceConfig::isSignalActivityRequired.
 *
 * - Persist: if enabled, the online status will be preserved, even if user goes offline or closes the app
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
     * @return True if the app is required to call MegaChatApi::signalPresenceActivity
     */
    virtual bool isSignalActivityRequired() const;
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
     * @param source Location where this log was generated
     *
     * For logs generated inside the SDK, this will contain the source file and the line of code.
     * The SDK retains the ownership of this string, it won't be valid after this funtion returns.
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
 *         [at this stage, the app can retrieve chatrooms and can operate in offline mode]
 *     4. Call MegaApi::login() and wait for completion
 *     5. Call MegaApi::fetchnodes() and wait for completion
 *         [at this stage, cloud storage apps are ready, but chat-engine is offline]
 *     6. Call MegaChatApi::connect() and wait for completion
 *     7. The app is ready to operate
 *
 * Important considerations:
 *  - In order to logout from the account, the app should call MegaApi::logout before MegaChatApi::logout.
 *  - The instance of MegaChatApi must be deleted before the instance of MegaApi passed to the constructor.
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
        STATUS_INVALID    = 0xFF    /// Invalid value. Presence not received yet
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
        INIT_WAITING_NEW_SESSION    = 1,    /// No \c sid provided at init() --> force a login+fetchnodes
        INIT_OFFLINE_SESSION        = 2,    /// Initialization successful for offline operation
        INIT_ONLINE_SESSION         = 3,    /// Initialization successful for online operation --> login+fetchnodes completed
        INIT_NO_CACHE               = 7     /// Cache not available for \c sid provided --> remove SDK cache and force a login+fetchnodes
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


    // chat will reuse an existent megaApi instance (ie. the one for cloud storage)
    /**
     * @brief Creates an instance of MegaChatApi to access to the chat-engine.
     *
     * @param megaApi Instance of MegaApi to be used by the chat-engine.
     */
    MegaChatApi(mega::MegaApi *megaApi);

//    // chat will use its own megaApi, a new instance
//    MegaChatApi(const char *appKey, const char* appDir);

    virtual ~MegaChatApi();

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
     * MegaChatApi::INIT_NO_CACHE and the app should go through a new fetchnodes in order to
     * re-create a new cache from scratch.
     *
     * The initialization status is notified via `MegaChatListener::onChatInitStateUpdate`. See
     * the documentation of the callback for possible values.
     *
     * This function should be called before MegaApi::login and MegaApi::fetchnodes.
     *
     * @param sid Session id that wants to be resumed, or NULL if a new session will be created.
     * @return The initialization state
     */
    int init(const char *sid);

    /**
     * @brief Returns the current initialization state
     *
     * The possible values are:
     *  - MegaChatApi::INIT_ERROR = -1
     *  - MegaChatApi::INIT_WAITING_NEW_SESSION = 1
     *  - MegaChatApi::INIT_OFFLINE_SESSION = 2
     *  - MegaChatApi::INIT_ONLINE_SESSION = 3
     *  - MegaChatApi::INIT_NO_CACHE = 7
     *
     * The returned value will be undefined if \c init(sid) has not been called yet.
     *
     * @return The current initialization state
     */
    int getInitState();

    // ============= Requests ================

    /**
     * @brief Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     *
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void connect(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function is intended to be used instead of MegaChatApi::connect when the connection
     * is done by a service in background, which is launched without user-interaction. It avoids
     * to notify to the server that this client is active, but actually the user is away.
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void connectInBackground(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Disconnect from chat-related servers (chatd, presenced and Gelb).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISCONNECT
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void disconnect(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current state of the connection
     *
     * It can be one of the following values:
     *  - MegaChatApi::DISCONNECTED = 0
     *  - MegaChatApi::CONNECTING   = 1
     *  - MegaChatApi::CONNECTED    = 2
     *
     * @return The state of connection
     */
    int getConnectionState();

    /**
     * @brief Returns the current state of the connection to chatd
     *
     * The possible values are:
     *  - MegaChatApi::CHAT_CONNECTION_OFFLINE      = 0
     *  - MegaChatApi::CHAT_CONNECTION_IN_PROGRESS  = 1
     *  - MegaChatApi::CHAT_CONNECTION_LOGGING      = 2
     *  - MegaChatApi::CHAT_CONNECTION_ONLINE       = 3
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return The state of connection
     */
    int getChatConnectionState(MegaChatHandle chatid);
    
    /**
     * @brief Refresh DNS servers and retry pending connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void retryPendingConnections(MegaChatRequestListener *listener = NULL);

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
     * @brief Set your online status.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHAT_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getNumber - Returns the new status of the user in chat.
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
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true if autoaway is enabled.
     * - MegaChatRequest::getNumber - Returns the specified timeout.
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
     * @brief Get your online status.
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
     * @brief Get the current presence configuration
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
     * The associated request type with this request is MegaChatRequest::TYPE_SET_BACKGROUND_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getfLAG - Returns the background status
     *
     * @param status True if the the app is in background, false if in foreground.
     */
    void setBackgroundStatus(bool background, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current firstname of the user
     *
     * This function is useful to get the firstname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_FIRSTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the firstname of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    void getUserFirstname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Returns the current lastname of the user
     *
     * This function is useful to get the lastname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_LASTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the lastname of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    void getUserLastname(MegaChatHandle userhandle, MegaChatRequestListener *listener = NULL);

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
     * You take the ownership of the returned value
     *
     * @return List of MegaChatListItemList objects with all chatrooms of this account.
     */
    MegaChatListItemList *getChatListItems();

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
     * Inactive chatrooms with unread messages are not considered.
     *
     * @return The number of chatrooms with unread messages
     */
    int getUnreadChats();

    /**
     * @brief Return the chatrooms that are currently active
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the active chatrooms
     */
    MegaChatListItemList *getActiveChatListItems();

    /**
     * @brief Return the chatrooms that are currently inactive
     *
     * Chatrooms became inactive when you left a groupchat or, for 1on1 chats,
     * when the contact-relationship is broken (you remove the contact or you are
     * removed by the other contact).
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the active chatrooms
     */
    MegaChatListItemList *getInactiveChatListItems();

    /**
     * @brief Return the chatrooms that have unread messages
     *
     * You take the onwership of the returned value.
     *
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
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
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
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers.
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
     * - MegaChatError::ERROR_ARGS - If the chatid or user handle are invalid
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
     * but this specific message will be overwritten by a management message. You can
     * expect a call to \c MegaChatRoomListener::onMessageUpdate where the message
     * will have no content and it will be of type \c MegaChatMessage::TYPE_TRUNCATE.
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
     * @brief Allows a logged in operator/moderator to clear the entire history of a chat
     *
     * If the history is not already empty, the latest message will be overwritten by
     * a management message. You can expect a call to \c MegaChatRoomListener::onMessageUpdate
     * where the message will have no content and it will be of type
     * \c MegaChatMessage::TYPE_TRUNCATE.
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
     * When there are no more history available from the reported source of messages
     * (local / remote), or when the requested \c count has been already loaded,
     * the callback MegaChatRoomListener::onMessageLoaded will be called with a NULL message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param count The number of requested messages to load.
     *
     * @return Return the source of the messages that is going to be fetched. The possible values are:
     *   - MegaChatApi::SOURCE_ERROR = -1: history has to be fetched from server, but we are offline
     *   - MegaChatApi::SOURCE_NONE = 0: there's no more history available (not even int the server)
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
     * This function allows to retrieve only those messages that are already loaded
     * and notified by MegaChatRoomListener::onMessageLoaded and/or messages that are
     * in sending-status (not yet confirmed). For any other message, this function
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
     * @param rowId Manual sending queue id of the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    MegaChatMessage *getManualSendingMessage(MegaChatHandle chatid, MegaChatHandle rowid);

    /**
     * @brief Sends a new message to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId()
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
     * @note Any tailing carriage return and/or line feed ('\r' and '\n') will be removed.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msg Content of the message
     *
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    MegaChatMessage *sendMessage(MegaChatHandle chatid, const char* msg);

    /**
     * @brief Sends a contact or a group of contacts to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId()
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
     void attachNode(MegaChatHandle chatid, MegaChatHandle nodehandle, MegaChatRequestListener *listener = NULL);

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
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @param msg New content of the message
     * @param msglen New length of the message
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
     * @return The last-seen-by-us MegaChatMessage, or NULL if error.
     */
    MegaChatMessage *getLastMessageSeen(MegaChatHandle chatid);

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

#ifndef KARERE_DISABLE_WEBRTC
    // Audio/Video device management
    /**
     * @brief Returns a list with the names of available audio devices
     *
     * If no device is found, it returns an empty list.
     * You take the ownership of the returned value
     *
     * @return Names of the available audio devices
     */
    mega::MegaStringList *getChatAudioInDevices();

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
     * @brief Select the audio device to be used in calls
     *
     * Audio device identifiers are obtained with function MegaChatApi::getChatAudioInDevices
     *
     * @note Audio device must be configured before starting a call. It cannot be changed
     * once the call has started.
     *
     * @param device Identifier of device to be selected
     * @return True if device has been selected. False in other case
     */
    bool setChatAudioInDevice(const char *device);

    /**
     * @brief Select the video device to be used in calls
     *
     * Video device identifiers are obtained with function MegaChatApi::getChatVideoInDevices
     *
     * @note Video device must be configured before starting a call. It cannot be changed
     * once the call has started.
     *
     * @param device Identifier of device to be selected
     * @return True if device has been selected. False in other case
     */
    bool setChatVideoInDevice(const char *device);

    // Call management
    /**
     * @brief Start a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true if it is a video-audio call or false for audio call
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param listener MegaChatRequestListener to track this request
     */
    void startChatCall(MegaChatHandle chatid, bool enableVideo = true, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Answer a call received in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ANSWER_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true if it is a video-audio call or false for audio call
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param listener MegaChatRequestListener to track this request
     */
    void answerChatCall(MegaChatHandle chatid, bool enableVideo = true, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Hang a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    void hangChatCall(MegaChatHandle chatid, MegaChatRequestListener *listener = NULL);

    /**
     * @brief Hang all active calls
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void hangAllChatCalls(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Enable audio for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getParamType - Returns MegaChatRequest::AUDIO
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
     * @brief Search all audio and video devices available at that moment.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES
     *
     * After call this function, available devices can be obtained calling MegaChatApi::getChatAudioInDevices
     * or MegaChatApi::getChatVideoInDevices.
     *
     * Call this function to update the list of available devices, ie. after plug-in a webcam to your PC.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void loadAudioVideoDeviceList(MegaChatRequestListener *listener = NULL);

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
     * @brief Returns number of calls that there are at the system
     * @return number of calls in the system
     */
    int getNumCalls();

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
    void removeChatRoomListener(MegaChatRoomListener *listener);

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
     * @brief Register a listener to receive video from local device
     *
     * You can use MegaChatApi::removeChatLocalVideoListener to stop receiving events.
     *
     * @param listener MegaChatVideoListener that will receive local video
     */
    void addChatLocalVideoListener(MegaChatVideoListener *listener);

    /**
     * @brief Unregister a MegaChatVideoListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);

    /**
     * @brief Register a listener to receive video from remote device
     *
     * You can use MegaChatApi::removeChatRemoteVideoListener to stop receiving events.
     *
     * @param listener MegaChatVideoListener that will receive remote video
     */
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);

    /**
     * @brief Unregister a MegaChatVideoListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);
#endif

    static void setCatchException(bool enable);

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
        CHANGE_TYPE_STATUS          = 0x01, /// obsolete
        CHANGE_TYPE_OWN_PRIV        = 0x02, /// Our privilege level has changed
        CHANGE_TYPE_UNREAD_COUNT    = 0x04,
        CHANGE_TYPE_PARTICIPANTS    = 0x08, /// A participant joined/left the chatroom or its privilege changed
        CHANGE_TYPE_TITLE           = 0x10,
        CHANGE_TYPE_CLOSED          = 0x20, /// The chatroom has been left by own user
        CHANGE_TYPE_LAST_MSG        = 0x40, /// Last message recorded in the history, or chatroom creation data if no history at all (not even clear-history message)
        CHANGE_TYPE_LAST_TS         = 0x80  /// Timestamp of the last activity
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
     * If the message is of type MegaChatMessage::TYPE_ATTACHMENT, this function
     * returns the filenames of the attached nodes. The filenames of nodes are separated
     * by ASCII character '0x01'
     * If the message is of type MegaChatMessage::TYPE_CONTACT, this function
     * returns the usernames. The usernames are separated
     * by ASCII character '0x01'
     * 
     * The SDK retains the ownership of the returned value. It will be valid until
     * the MegaChatListItem object is deleted. If you want to save the MegaChatMessage,
     * use MegaChatMessage::copy
     *
     * @return The last message received.
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
     *  - 0xFF when it's still fetching from server (for the public API)
     *
     * @return The type of the last message
     */
    virtual int getLastMessageType() const;

    /**
     * @brief Returns the sender of last message
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
     * @brief Returns whether the user is member of the chatroom (for groupchats),
     * or the user is contact with the peer (for 1on1 chats).
     *
     * @return True if the chat is active, false otherwise.
     */
    virtual bool isActive() const;

    /**
     * @brief Returns the userhandle of the Contact in 1on1 chatrooms
     *
     * The returned value is only valid for 1on1 chatrooms. For groupchats, it will
     * return MEGACHAT_INVALID_HANDLE.
     *
     * @return The userhandle of the Contact
     */
    virtual MegaChatHandle getPeerHandle() const;
};

class MegaChatRoom
{
public:

    enum
    {
        CHANGE_TYPE_STATUS          = 0x01, /// obsolete
        CHANGE_TYPE_UNREAD_COUNT    = 0x02,
        CHANGE_TYPE_PARTICIPANTS    = 0x04, /// joins/leaves/privileges/names
        CHANGE_TYPE_TITLE           = 0x08,
        CHANGE_TYPE_USER_TYPING     = 0x10, /// User is typing. \see MegaChatRoom::getUserTyping()
        CHANGE_TYPE_CLOSED          = 0x20, /// The chatroom has been left by own user
        CHANGE_TYPE_OWN_PRIV        = 0x40  /// Our privilege level has changed
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
     * @brief Returns the privilege level of the user in this chat.
     *
     * If the user doesn't participate in this MegaChatRoom, this function returns PRIV_UNKNOWN.
     *
     * @param Handle of the peer whose privilege is requested.
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
     * If the user doesn't participate in this MegaChatRoom, this function returns NULL.
     *
     * @param Handle of the peer whose name is requested.
     * @return Firstname of the chat peer with the handle specified.
     */
    virtual const char *getPeerFirstnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the current lastname of the peer
     *
     * If the user doesn't participate in this MegaChatRoom, this function returns NULL.
     *
     * @param Handle of the peer whose name is requested.
     * @return Lastname of the chat peer with the handle specified.
     */
    virtual const char *getPeerLastnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the current fullname of the peer
     *
     * If the user doesn't participate in this MegaChatRoom, this function returns NULL.
     *
     * You take the ownership of the returned value. Use delete [] value
     *
     * @param Handle of the peer whose name is requested.
     * @return Fullname of the chat peer with the handle specified.
     */
    virtual const char *getPeerFullnameByHandle(MegaChatHandle userhandle) const;

    /**
     * @brief Returns the email address of the peer
     *
     * If the user doesn't participate in this MegaChatRoom, this function returns NULL.
     *
     * @param Handle of the peer whose email is requested.
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
     * You take the ownership of the returned value. Use delete [] value
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
     * @brief getTitle Returns the title of the chat, if any.
     *
     * @return The title of the chat as a null-terminated char array.
     */
    virtual const char *getTitle() const;

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
     * @brief Returns the handle of the user who is typing a message in the chatroom
     *
     * Normally the app should have a timer that is reset each time a typing
     * notification is received. When the timer expires, it should hide the notification GUI.
     *
     * @return The user that is typing
     */
    virtual MegaChatHandle getUserTyping() const;

    /**
     * @brief Returns whether the user is member of the chatroom (for groupchats),
     * or the user is contact with the peer (for 1on1 chats).
     *
     * @return True if the chat is active, false otherwise.
     */
    virtual bool isActive() const;

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
     *
     * The SDK retains the ownership of the MegaChatListItem in the second parameter.
     * The MegaChatListItem object will be valid until this function returns. If you
     * want to save the MegaChatListItem, use MegaChatListItem::copy
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
     *  - MegaChatApi::INIT_WAITING_NEW_SESSION = 0
     *  - MegaChatApi::INIT_OFFLINE_SESSION = 1
     *  - MegaChatApi::INIT_ONLINE_SESSION = 2
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
     * chat server has changed.
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
};

}

#endif // MEGACHATAPI_H
