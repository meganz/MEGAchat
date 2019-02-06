#ifndef __CHATD_MSG_H__
#define __CHATD_MSG_H__

#include <stdint.h>
#include <string>
#include <buffer.h>
#include "karereId.h"

enum
{
    CHATD_KEYID_INVALID = 0,                // used when no keyid is set
    CHATD_KEYID_UNCONFIRMED = 0xfffffffe,   // used when a new keyid has been requested. Should be kept as constant as possible and in the range of 0xffff0001 to 0xffffffff
    CHATD_KEYID_MAX = 0xffffffff,           // higher keyid allowed for unconfirmed new keys
    CHATD_KEYID_MIN = 0xffff0000            // lower keyid allowed for unconfirmed new keys
};

namespace chatd
{

typedef uint32_t KeyId;
typedef uint64_t BackRefId;

static bool isLocalKeyId(KeyId localKeyid)
{
    return (localKeyid >= CHATD_KEYID_MIN);
}

enum { kMaxBackRefs = 32 };

// command opcodes
enum Opcode
{
    /**
      * @brief
      * C->S: Must respond to a received KEEPALIVE within 30 seconds (if local user is away,
      * send KEEPALIVEAWAY).
      *
      * S->C: Initiates keepalives to the client every minute.
      */
    OP_KEEPALIVE = 0,

    /**
      * @brief
      * C->S: Subscribe to events for this chat. Client has no existing message buffer.
      * Send: <chatid> <userid> <user_priv>
      *
      * S->C: Indicates users in this chat and their privilege level as well as privilege
      * Receive: <chatid> <userid> <priv>
      */
    OP_JOIN = 1,

    /**
      * @brief
      * S->C: Received as result of HIST command. It reports an old message.
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_OLDMSG = 2,

    /**
      * @brief
      * C->S: Add new message to this chat. msgid is a temporary random 64-bit
      *    integer ("msgxid"). No two such msgxids must be generated in the same chat
      *    in the same server-time second, or only the first NEWMSG will be written.
      *
      * S->C: A different connection has added a message to the chat.
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_NEWMSG = 3,

    /**
      * @brief
      * C->S: Update existing message. Can only be updated within one hour of the physical
      *    arrival time. updatedelta must be larger than the previous updatedelta, or
      *    the MSGUPD will fail. The keyid must not change.
      *
      * S->C: A message was updated (always sent as MSGUPD). The `ts_send` is
      * zero for all updates, except when the type of message is a truncate. In that
      * case, the `ts_send` overwrites the former ts and the `ts_update` is zero.
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_MSGUPD = 4,

    /**
      * @brief
      * C->S: Indicate the last seen message so that the new message count can be
      *   managed across devices.
      * Send: <chatid> <msgid>
      *
      * S->C: The last seen message has been updated (from another device or current
      * device). Also received as part of the login process.
      * Receive: <chatid> <msgid>
      */
    OP_SEEN = 5,

    /**
      * @brief
      * C->S: Indicate the last successfully written msgid so that the "delivered"
      *   indication can be managed in 1-on-1 chats.
      * Send: <chatid> <msgid>
      *
      * S->C: The last delivered msgid was updated for this 1-on-1 chat.
      * Receive: <chatid> <msgid>
      */
    OP_RECEIVED = 6,

    /**
      * @brief Currently not implemented.
      * <chatid><timestamp>
      */
    OP_RETENTION = 7,

    /**
      * @brief
      * C->S: Requests the next -count older messages, which will be sent as a sequence
      *    of OLDMSGs, followed by a HISTDONE. Note that count is always negative.
      * Send: <chatid> <count>
      *
      * This command is sent at the following situations:
      *  1. After a JOIN command to load last messages, when no local history.
      *  2. When the app requests to load more old messages and there's no more history
      *     in the local cache (both RAM and DB).
      *  3. When the last text message is requested and not found yet.
      *
      * This command results in receiving:
      *  1. The last SEEN message, if any.
      *  2. The last RECEIVED message, if any.
      *  3. The NEWKEY to encrypt/decrypt history, if any.
      *  4. As many OLDMSGs as messages in history, up to `count`.
      *  5. A notification when HISTDONE.
      */
    OP_HIST = 8,

    /**
      * @brief <chatid> <msgid0> <msgid1>
      *
      * C->S: Request existing message range (<msgid0> is the oldest, <msgid1> the newest).
      * Responds with all messages newer than <msgid1> (if any), followed by a HISTDONE.
      *
      * @obsolete This command is obsolete, replaced by JOINRANGEHIST.
      */
    OP_RANGE = 9,

    /**
      * @brief
      * S->C: The NEWMSG with msgxid was successfully written and now has the permanent
      *    msgid.
      * Receive: <msgxid> <msgid>
      */
    OP_NEWMSGID = 10,

    /**
      * @brief
      * S->C: The previously sent command with op_code has failed for a command-specific reason
      * Receive: <chatid> <generic_id> <op_code> <reason>
      *
      * Server can reject:
      *  - JOIN: the user doesn't participate in the chatroom
      *  - NEWMSG: no write-access or participants have changed
      *  - MSGUPD | MSGUPDX: participants have changed or the message is too old
      */
    OP_REJECT = 11,

    /**
      * @brief BROADCAST <chatid> <userid> <broadcasttype>
      *
      * It's a command that it is sent by user in a chat and it's received by rest of users.
      *
      * C->S: Used to inform all users in `chatid` that about certain type of event.
      * S->C: Informs the client that `userid` in `chatid` about certain type of event.
      *
      * Valid broadcast types:
      *     1 --> means user-is-typing
      */
    OP_BROADCAST = 12,

    /**
      * @brief
      * S->C: Receive as result of HIST command, it notifies the end of history fetch.
      * Receive: <chatid>
      *
      * @note There may be more history in server, but the HIST <count> is already satisfied.
      */
    OP_HISTDONE = 13,

    /**
      * @brief
      * C->S: Add new key to repository. Payload format is (userid.8 keylen.2 key)*
      * Send: <chatid> <keyxid> <payload>
      *
      * S->C: Key notification. Payload format is (userid.8 keyid.4 keylen.2 key)*
      * Receive: <chatid> <keyid> <payload>
      *
      * Note that <keyxid> should be constant. Valid range: [0xFFFF0001 - 0xFFFFFFFF]
      * Note that ( chatid, userid, keyid ) is unique. Neither ( chatid, keyid ) nor
      * ( userid, keyid ) are unique!
      * Note that <keyxid> is per connection. It does not survive a reconnection.
      */
    OP_NEWKEY = 17,

    /**
      * @brief
      * S->C: Signal the final <keyid> for a newly allocated key.
      * Receive: <chatid> <keyxid> <keyid>
      */
    OP_NEWKEYID = 18,

    /**
      * @brief
      * C->S: Subscribe to events for this chat, indicating the existing message range
      *    (msgid0 is the oldest, msgid1 the newest). Responds with all messages newer
      *    than msgid1 (if any) as NEWMSG, followed by a HISTDONE.
      * Send: <chatid> <msgid0> <msgid1>
      */
    OP_JOINRANGEHIST = 19,

    /**
      * @brief
      * C->S: Update existing message. Can only be updated within one hour of the physical
      *    arrival time. updatedelta must be larger than the previous updatedelta, or
      *    the MSGUPD will fail. The keyid must not change.
      *
      * Send: <chatid> <userid> <msgxid> <timestamp> <updatedelta> <key(x)id> <payload>
      */
    OP_MSGUPDX = 20,

    /**
      * @brief
      * S->C: The NEWMSG with msgxid had been written previously, informs of the permanent
      *     msgid.
      * Receive: <msgxid> <msgid>
      */
    OP_MSGID = 21,

    /**
      * @brief The unique identifier per-client, per-shard. The id is different for each shard.
      *
      * C->S: It sends a seed to generate the unique client id. The seed is 64 bits length.
      * The seed can be the same for all shards.
      * Send: <seed>
      *
      * S->C: It receives the client id generated by server using the seed. It is different for each
      * shard, despite the provided seed for each shard were the same. Client id is 32 bits length.
      * Receive: <clientid>
      *
      * */
    OP_CLIENTID = 24,

    /**
      * @brief  <chatid> <userid> <clientid> <payload-len> <payload>
      *
      * C->S: send to specified recipient(s)
      * S->C: delivery from specified sender
      */
    OP_RTMSG_BROADCAST = 25,

    /**
      * @brief  <chatid> <userid> <clientid> <payload-len> <payload>
      *
      * C->S: send to specified recipient(s)
      * S->C: delivery from specified sender
      */
    OP_RTMSG_USER = 26,

    /**
      * @brief  <chatid> <userid> <clientid> <payload-len> <payload>
      *
      * C->S: send to specified recipient(s)
      * S->C: delivery from specified sender
      */
    OP_RTMSG_ENDPOINT = 27,

    /**
      * @brief  <chatid> <userid> <clientid>
      *
      * C->S: register / keepalive participation in channel's call (max interval: 5 s)
      * S->C: notify all clients of someone having joined the call (sent immediately after a chatd connection is established)
      *
      * If there are less than two parties pinging INCALL in a 1:1 chat with CALLDATA having its connected flag set,
      * the call is considered terminated, and an `ENDCALL` is broadcast.
      * @note that chatd does not parse CALLDATA yet, so the above is not enforced (yet).
      */
    OP_INCALL = 28,

    /**
      * @brief  <chatid> <userid> <clientid>
      *
      * C->S: device has left call
      * S->C: notify all clients of someone having left the call
      */
    OP_ENDCALL = 29,

    /**
      * @brief
      *
      * C->S: Must respond to a received KEEPALIVE within 30 seconds. If local user is away,
      * send KEEPALIVEAWAY
      */
    OP_KEEPALIVEAWAY = 30,

    /**
      * @brief <chatid> <userid> <clientid> <payload-Len> <payload>
      *
      * C->S: set/update call data (gets broadcast to all people in the chat)
      * S->C: notify call data changes (sent immediately after a chatd connection is established
      *     and additionally after JOIN, for those unknown chatrooms at the moment the chatd connection is established
      *
      */
    OP_CALLDATA = 31,

    /**
      * @brief <randomToken>
      *
      * C->S: send an echo packet with 1byte payload. Server will respond with the
      * same byte in return.
      *
      * This command helps to detect a broken socket when it silently dies.
      */
    OP_ECHO = 32,

    /**
      * @brief <chatid> <userid> <msgid> <unicodechar32le>
      *
      * User add a reaction to message
      */
    OP_ADDREACTION = 33,

    /**
      * @brief <chatid> <userid> <msgid> <unicodechar32le>
      *
      * User delete a reaction to message
      */
    OP_DELREACTION = 34,

    /**
      * @brief <chatid>
      *
      * C->S: ping server to ensure client is up to date for the specified chatid
      * S->C: response to the ping initiated by client
      */
    OP_SYNC = 38,

    /**
      ** @brief <chatid> <callDuration.4>
      *
      * S->C: inform about call duration in seconds for a call that exists before we get online.
      * It is sent before any INCALL or CALLDATA.
      */
    OP_CALLTIME = 42,

    /**
      ** @brief
      *
      * C->S: Add new message to this chat. msgid is a temporary random 64-bit
      *    integer ("msgxid"). No two such msgxids must be generated in the same chat
      *    in the same server-time second, or only the first NEWNODEMSG will be written.
      *
      *    This opcode must be used, instead of OP_NEWMSG, to send messages that include
      *    node/s attachment/s (type kMsgNodeAttachment). Messages sent with this opcode
      *    will behave exactly as the ones sent with OP_NEWMSG, but chatd will mark them
      *    as attachments. In result, client can use OP_NODEHIST to retrieve messages of
      *    this type.
      */
    OP_NEWNODEMSG = 44,

    /**
      * @brief
      * C->S: Requests the count older attach messages from msgid, which will be sent as a sequence
      *    of OLDMSGs, followed by a HISTDONE.
      * Send: <chatid> <msgid> <count>
      */
    OP_NODEHIST = 45,

    OP_LAST = OP_NODEHIST,
    OP_INVALIDCODE = 0xFF
};

// privilege levels
enum Priv: signed char
{
    PRIV_INVALID = -10,
    PRIV_NOCHANGE = -2,
    PRIV_NOTPRESENT = -1,
    PRIV_RDONLY = 0,
    PRIV_FULL = 2,
    PRIV_OPER = 3
};

class Message: public Buffer
{
public:
    enum Type: uint8_t
    {
        kMsgInvalid           = 0x00,
        kMsgNormal            = 0x01,
        kMsgManagementLowest  = 0x02,
        kMsgAlterParticipants = 0x02,
        kMsgTruncate          = 0x03,
        kMsgPrivChange        = 0x04,
        kMsgChatTitle         = 0x05,
        kMsgCallEnd           = 0x06,
        kMsgCallStarted       = 0x07,
        kMsgManagementHighest = 0x07,
        kMsgOffset            = 0x55,   // Offset between old message types and new message types
        kMsgUserFirst         = 0x65,
        kMsgAttachment        = 0x65,   // kMsgNormal's subtype = 0x10
        kMsgRevokeAttachment  = 0x66,   // kMsgNormal's subtype = 0x11
        kMsgContact           = 0x67,   // kMsgNormal's subtype = 0x12
        kMsgContainsMeta      = 0x68,   // kMsgNormal's subtype = 0x13
        kMsgVoiceClip         = 0x69    // kMsgNormal's subtype = 0x14
    };

    enum ContainsMetaSubType: uint8_t
    {
        kInvalid              = 0xff,
        kRichLink             = 0x00,
        kGeoLocation          = 0x01
    };

    enum Status
    {
        kSending, ///< Message has not been sent or is not yet confirmed by the server
        kSendingManual, ///< Message is too old to auto-retry sending, or group composition has changed. User must explicitly confirm re-sending. All further messages queued for sending also need confirmation
        kServerReceived, ///< Message confirmed by server, but not yet delivered to recepient(s)
        kServerRejected, ///< Message is rejected by server for some reason (editing too old message for example)
        kDelivered, ///< Peer confirmed message receipt. Used only for 1on1 chats
        kLastOwnMessageStatus = kDelivered, //if a status is <= this, we created the msg, oherwise not
        kNotSeen, ///< User hasn't read this message yet
        kSeen ///< User has read this message
    };
    enum { kFlagForceNonText = 0x01 };

    enum EncryptionStatus
    {
        kNotEncrypted        = 0,    /// Message already decrypted
        kEncryptedPending    = 1,    /// Message pending to be decrypted (transient)
        kEncryptedNoKey      = 2,    /// Key not found for the message (permanent failure)
        kEncryptedSignature  = 3,    /// Signature verification failure (permanent failure)
        kEncryptedMalformed  = 4,    /// Malformed/corrupted data in the message (permanent failure)
        kEncryptedNoType     = 5     /// Management message of unknown type (transient, not supported by the app yet)
        // if type of management message is unknown, it would be stored encrypted and will not be decrypted
        // even if the library adds support to the new type (unless the message is reloaded from server)
    };

    /** @brief Info recorder in a management message.
     * When a message is a management message, _and_ it needs to carry additional
     * info besides the standard fields (such as sender), the additional data
     * is put inside the message body, laid out as this structure. This is done
     * locally, and has nothing to do with the format of the management message
     * on the wire (where it us usually encoded via TLV). Note that not all management
     * messages need this additional data - for example, a history truncate message
     * has info only about the user that truncated the history, whose handle is contained
     * in the userid property of the Message object, so truncate messages are empty and
     * don't contain a ManagementInfo structure.
     */
    struct ManagementInfo
    {
        /** @brief The affected user.
         * In case of:
         * \c kMsgPrivChange - the user whose privilege changed
         * \c kMsgAlterParticipants - the user who was added/or excluded from the chatroom
         */
        karere::Id target;

        /** In case of:
         * \c kMsgPrivChange - the new privilege of the user whose handle is in \c target
         * \c kMsgAlterParticipants - this is used as a flag to specify whether the user
         * was:
         * - removed - the value is \c PRIV_NOTPRESENT
         * - added - the value of \c PRIV_NOCHANGE
         */
        Priv privilege = PRIV_INVALID;
    };

    class CallEndedInfo
    {
        public:
        karere::Id callid;
        uint8_t termCode = 0;
        uint32_t duration = 0;
        std::vector<karere::Id> participants;

        static CallEndedInfo *fromBuffer(const char *buffer, size_t len)
        {
            CallEndedInfo *info = new CallEndedInfo;
            size_t numParticipants;
            unsigned int lenCallid = sizeof (info->callid);
            unsigned int lenDuration = sizeof (info->duration);
            unsigned int lenTermCode = sizeof (info->termCode);
            unsigned int lenNumParticipants = sizeof (numParticipants);
            unsigned int lenId = sizeof (karere::Id);

            if (!buffer || len < (lenCallid + lenDuration + lenTermCode + lenNumParticipants))
            {
                delete info;
                return NULL;
            }

            unsigned int position = 0;
            memcpy(&info->callid, &buffer[position], lenCallid);
            position += lenCallid;
            memcpy(&info->duration, &buffer[position], lenDuration);
            position += lenDuration;
            memcpy(&info->termCode, &buffer[position], lenTermCode);
            position += lenTermCode;
            memcpy(&numParticipants, &buffer[position], lenNumParticipants);
            position += lenNumParticipants;

            if (len < (position + (lenId * numParticipants)))
            {
                delete info;
                return NULL;
            }

            for (size_t i = 0; i < numParticipants; i++)
            {
                karere::Id id;
                memcpy(&id, &buffer[position], lenId);
                position += lenId;
                info->participants.push_back(id);
            }

            return info;
        }
    };

private:
    //avoid setting the id and flag pairs one by one by making them accessible only by setId(Id,bool)
    karere::Id mId;
    bool mIdIsXid = false;

protected:
    uint8_t mIsEncrypted = kNotEncrypted;

public:
    karere::Id userid;
    uint32_t ts;
    uint16_t updated;
    KeyId keyid;
    unsigned char type;
    BackRefId backRefId = 0;
    std::vector<BackRefId> backRefs;
    mutable void* userp;
    mutable uint8_t userFlags = 0;
    bool richLinkRemoved = 0;

    karere::Id id() const { return mId; }
    void setId(karere::Id aId, bool isXid) { mId = aId; mIdIsXid = isXid; }
    bool isSending() const { return mIdIsXid; }

    bool isLocalKeyid() const { return isLocalKeyId(keyid); }

    uint8_t isEncrypted() const { return mIsEncrypted; }
    bool isPendingToDecrypt() const { return (mIsEncrypted == kEncryptedPending); }
    // true if message is valid, but permanently undecryptable (not transient like unknown types or keyid not found)
    bool isUndecryptable() const { return (mIsEncrypted == kEncryptedMalformed || mIsEncrypted == kEncryptedSignature); }
    void setEncrypted(uint8_t encrypted) { mIsEncrypted = encrypted; }

    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
          Buffer&& buf, bool aIsSending=false, KeyId aKeyid=CHATD_KEYID_INVALID,
          unsigned char aType=kMsgNormal, void* aUserp=nullptr)
      :Buffer(std::forward<Buffer>(buf)), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid),
          ts(aTs), updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}

    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
            const char* msg, size_t msglen, bool aIsSending=false,
            KeyId aKeyid=CHATD_KEYID_INVALID, unsigned char aType=kMsgInvalid, void* aUserp=nullptr)
        :Buffer(msg, msglen), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid), ts(aTs),
            updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}

    Message(const Message& msg)
        : Buffer(msg.buf(), msg.dataSize()), mId(msg.id()), mIdIsXid(msg.mIdIsXid), mIsEncrypted(msg.mIsEncrypted),
          userid(msg.userid), ts(msg.ts), updated(msg.updated), keyid(msg.keyid), type(msg.type), backRefId(msg.backRefId),
          backRefs(msg.backRefs), userp(msg.userp), userFlags(msg.userFlags), richLinkRemoved(msg.richLinkRemoved)
    {}

    /** @brief Returns the ManagementInfo structure contained within the message
     * content. Throws if the message is not a management message, or if the
     * size of the message contents is smaller than the size of ManagementInfo,
     * but otherwise does not guarentee that the data inside the message
     * is actually a ManagementInfo structure
     */
    ManagementInfo mgmtInfo() { throwIfNotManagementMsg(); return read<ManagementInfo>(0); }

    /** @brief A \c const version of mgmtInfo() */
    const ManagementInfo mgmtInfo() const { throwIfNotManagementMsg(); return read<ManagementInfo>(0); }

    /** @brief Allocated a ManagementInfo structure in the message's buffer,
     * and writes the contents of the provided structure. The message contents
     * *must* be empty when the method is called.
     */
    void createMgmtInfo(const ManagementInfo& src)
    {
        assert(empty());
        append(&src, sizeof(src));
    }

    void createCallEndedInfo(const CallEndedInfo& src)
    {
        assert(empty());
        append(&src.callid, sizeof(src.callid));
        append(&src.duration, sizeof(src.duration));
        append(&src.termCode, sizeof(src.termCode));
        size_t numParticipants = src.participants.size();
        append(&numParticipants, sizeof(numParticipants));
        for (size_t i = 0; i < numParticipants; i++)
        {
            append(&src.participants[i], sizeof(src.participants[i]));
        }
    }

    static const char* statusToStr(unsigned status)
    {
        return (status > kSeen) ? "(invalid status)" : statusNames[status];
    }
    StaticBuffer backrefBuf() const
    {
        return backRefs.empty()
            ?StaticBuffer(nullptr, 0)
            :StaticBuffer((const char*)&backRefs[0], backRefs.size()*8);
    }

    /** @brief Creates a human readable string that describes the management
     * message. Used for debugging
     */
    std::string managementInfoToString() const; //implementation is in strongelope.cpp, as the management info is created there

    /** @brief Returns whether this message is a management message. */
    bool isManagementMessage() const
    {
        return (keyid == 0) && !isSending();    // msgs in sending status use keyid=CHATD_KEYID_INVALID (0)
    }
    bool isManagementMessageKnownType()
    {
        return (isManagementMessage()
                && type >= Message::kMsgManagementLowest
                && type <= Message::kMsgManagementHighest);
    }
    bool isOwnMessage(karere::Id myHandle) const { return (userid == myHandle); }
    bool isDeleted() const { return (updated && !size()); } // returns false for truncate (update = 0)
    bool isValidLastMessage() const
    {
        return (!isDeleted()                        // skip deleted messages (keep truncates)
                && type != kMsgRevokeAttachment     // skip revokes
                && type != kMsgInvalid              // skip (still) encrypted messages
                && (!mIsEncrypted                   // include decrypted messages
                    || isUndecryptable()));         // or undecryptable messages due to permantent error
    }
    // conditions to consider unread messages should match the
    // ones in ChatdSqliteDb::getUnreadMsgCountAfterIdx()
    bool isValidUnread(karere::Id myHandle) const
    {
        return (!isOwnMessage(myHandle)             // exclude own messages
                && !isDeleted()                     // exclude deleted messages
                && (!mIsEncrypted                   // include decrypted messages
                    || isUndecryptable())           // or undecryptable messages due to permantent error
                && (type == kMsgNormal              // exclude any unknown type (not shown in the apps)
                    || type == kMsgAttachment
                    || type == kMsgContact
                    || type == kMsgContainsMeta
                    || type == kMsgVoiceClip)
                );
    }
    ContainsMetaSubType containMetaSubtype() const
    {
        return (type == kMsgContainsMeta && dataSize() > 2) ? ((ContainsMetaSubType)*(buf()+2)) : ContainsMetaSubType::kInvalid;
    }
    std::string containsMetaJson() const
    {
        return (type == kMsgContainsMeta && dataSize() > 3) ? std::string(buf()+3, dataSize() - 3) : "";
    }

    /** @brief Convert attachment etc. special messages to text */
    std::string toText() const
    {
        if (empty())
            return std::string();

        if (type == kMsgNormal)
            return std::string(buf(), dataSize());

        //special messages have a 2-byte binary prefix
        assert(dataSize() > 2);
        return std::string(buf()+2, dataSize()-2);
    }

    /** @brief Throws an exception if this is not a management message. */
    void throwIfNotManagementMsg() const { if (!isManagementMessage()) throw std::runtime_error("Not a management message"); }

    static bool hasUrl(const std::string &text, std::string &url);
    static bool parseUrl(const std::string &url);
    static void removeUnnecessaryLastCharacters(std::string& buf);
    static void removeUnnecessaryFirstCharacters(std::string& buf);
    static bool isValidEmail(const std::string &buf);

protected:
    static const char* statusNames[];
    friend class Chat;
};

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
protected:
    Command(uint8_t opcode, uint8_t reserve, uint8_t payloadSize=0)
    : Buffer(reserve, payloadSize+1) { write(0, opcode); }
    Command(const char* data, size_t size): Buffer(data, size){}
public:
    enum { kBroadcastUserTyping = 1,  kBroadcastUserStopTyping = 2};
    Command(): Buffer(){}
    Command(Command&& other)
    : Buffer(std::forward<Buffer>(other))
    { assert(!other.buf() && !other.bufSize() && !other.dataSize()); }

    explicit Command(uint8_t opcode, size_t reserve=64)
    : Buffer(reserve) { write(0, opcode); }

    template<class T>
    Command&& operator+(const T& val)
    {
        append(val);
        return std::move(*this);
    }
    Command&& operator+(karere::Id id)
    {
        append(id.val);
        return std::move(*this);
    }
    Command&& operator+(const Buffer& msg)
    {
        append<uint32_t>(msg.dataSize());
        append(msg.buf(), msg.dataSize());
        return std::move(*this);
    }
    bool isMessage() const
    {
        auto op = opcode();
        return ((op == OP_NEWMSG) || (op == OP_NEWNODEMSG)|| (op == OP_MSGUPD) || (op == OP_MSGUPDX));
    }
    uint8_t opcode() const { return read<uint8_t>(0); }
    static const char* opcodeToStr(uint8_t opcode);
    const char* opcodeName() const { return opcodeToStr(opcode()); }
    static std::string toString(const StaticBuffer& data);
    virtual std::string toString() const;
    virtual ~Command(){}
};

/**
 * @brief The KeyCommand class represents a `NEWKEY` command for chatd.
 *
 * It inherits from Buffer and the structure of the byte-sequence is:
 *      opcode.1 + chatid.8 + keyid.4 + keyblobslen.4 + keyblobs.keylen
 *
 * The keyblobs follow the structure:
 *      userid.8 + keylen.2 + key.keylen
 *
 * Additionally, the KeyCommand stores the given local keyid, which is used
 * internally. The keyid encoded in the command is always hardwire to the
 * constant value CHATD_KEYID_UNCONFIRMED.
 */
class KeyCommand: public Command
{
private:
    KeyId mLocalKeyid;

public:
    explicit KeyCommand(karere::Id chatid, KeyId aLocalkeyid, size_t reserve=128)
    : Command(OP_NEWKEY, reserve), mLocalKeyid(aLocalkeyid)
    {
        assert(isLocalKeyId(mLocalKeyid));

        KeyId keyid = CHATD_KEYID_UNCONFIRMED;
        append(chatid.val).append<KeyId>(keyid).append<uint32_t>(0); //last is length of keys payload, initially empty
    }

    KeyId localKeyid() const { return mLocalKeyid; }
    KeyId keyId() const { return read<KeyId>(9); }
    void setChatId(karere::Id aChatId) { write<uint64_t>(1, aChatId.val); }
    void setKeyId(KeyId keyid) { write(9, keyid); }
    void addKey(karere::Id userid, void* keydata, uint16_t keylen)
    {
        assert(keydata && (keylen != 0));
        uint32_t& payloadSize = mapRef<uint32_t>(13);
        payloadSize+=(10+keylen); //userid.8+len.2+keydata.keylen
        append<uint64_t>(userid.val).append<uint16_t>(keylen);
        append(keydata, keylen);
    }
    bool hasKeys() const { return dataSize() > 17; }
    uint32_t keybloblen() const { return read<uint32_t>(13); }
    StaticBuffer keyblob() const
    {
        auto len = keybloblen();
        return StaticBuffer(readPtr(17, len), len);
    }
    void setKeyBlobs(const char* keyblob, uint32_t len)
    {
        write(13, len);
        memcpy(writePtr(17, len), keyblob, len);
        setDataSize(17 + len);
    }

    void clearKeys() { setDataSize(17); }
    virtual std::string toString() const;
};

//we need that special class because we may update key ids after keys get confirmed,
//so in case of NEWMSG with keyxid, if the client reconnects between key confirm and
//NEWMSG send, the NEWMSG would not use the no longer valid keyxid, but a real key id
class MsgCommand: public Command
{
public:
    explicit MsgCommand(uint8_t opcode, karere::Id chatid, karere::Id userid,
        karere::Id msgid, uint32_t ts, uint16_t updated, KeyId keyid=CHATD_KEYID_INVALID)
    :Command(opcode)
    {
        write(1, chatid.val);write(9, userid.val);write(17, msgid.val);write(25, ts);
        write(29, updated);write(31, keyid);write(35, 0); //msglen
    }
    MsgCommand(size_t reserve): Command(OP_INVALIDCODE, reserve) {} //for loading the buffer
    karere::Id msgid() const { return read<uint64_t>(17); }
    karere::Id userId() const { return read<uint64_t>(9); }
    void setId(karere::Id aMsgid) { write(17, aMsgid.val); }
    KeyId keyId() const { return read<KeyId>(31); }
    void setKeyId(KeyId aKeyid) { write(31, aKeyid); }
    StaticBuffer msg() const
    {
        auto len = msglen();
        return StaticBuffer(readPtr(39, len), len);
    }
    uint32_t msglen() const { return read<uint32_t>(35); }
    uint16_t updated() const { return read<uint16_t>(29); }
    uint32_t ts() const { return read<uint32_t>(25); }
    void clearMsg()
    {
        if (msglen() > 0)
            memset(buf()+39, 0, msglen()); //clear old message memory
        write(35, (uint32_t)0);
    }
    void setMsg(const char* msg, uint32_t msglen)
    {
        write(35, msglen);
        memcpy(writePtr(39, msglen), msg, msglen);
    }
    void updateMsgSize()
    {
        write<uint32_t>(35, dataSize()-39);
    }
};

//for exception message purposes
static inline std::string operator+(const char* str, karere::Id id)
{
    std::string result(str);
    result.append(id.toString());
    return result;
}
static inline std::string& operator+(std::string&& str, karere::Id id)
{
    str.append(id.toString());
    return str;
}

enum ChatState
{
    kChatStateOffline = 0,
    kChatStateConnecting,       // connecting to chatd (resolve DNS, open socket...)
    kChatStateJoining,          // connection to chatd is established, logging in
    kChatStateOnline            // login completed (HISTDONE received for JOIN/JOINRANGEHIST)
};

static inline const char* chatStateToStr(unsigned state)
{
    static const char* chatStates[] =
    { "Offline", "Connecting", "Joining", "Online"};

    if (state > chatd::kChatStateOnline)
        return "(unkown state)";
    else
        return chatStates[state];
}

static inline const char* privToString(Priv priv)
{
    switch (priv)
    {
    case PRIV_NOCHANGE:
        return "No change";
    case PRIV_NOTPRESENT:
        return "Not present";
    case PRIV_RDONLY:
        return "READONLY";
    case PRIV_FULL:
        return "READ_WRITE";
    case PRIV_OPER:
        return "OPERATOR";
    default:
        return "(unknown privilege)";
    }
}
}
#endif
