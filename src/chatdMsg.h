#ifndef __CHATD_MSG_H__
#define __CHATD_MSG_H__

#include <stdint.h>
#include <string>
#include <buffer.h>
#include "karereId.h"

enum { CHATD_KEYID_INVALID = 0, CHATD_KEYID_UNCONFIRMED = 0xffffffff };

namespace chatd
{

typedef uint32_t KeyId;
typedef uint64_t BackRefId;
typedef uint32_t ClientId;

enum { kMaxBackRefs = 32 };

// command opcodes
enum Opcode
{
    OP_KEEPALIVE = 0,

    /**
      * @brief It must be the very first command to send, so the user actually join the chatroom.
      *
      * Send: <chatid> <userid> <user_priv>
      * @note user_priv is usually Priv::PRIV_NOCHANGE, so chatd replies with the actual privilege.
      *
      * Receive: <chatid> <userid> <priv>
      * @note a JOIN is received per user, with its corresponding privilege
      *
      */
    OP_JOIN = 1,

    /**
      * @brief Received as result of HIST command. It reports an old message.
      *
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_OLDMSG = 2,

    /**
      * @brief Received as result of HIST command. It reports a new message.
      *
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_NEWMSG = 3,

    /**
      * @brief Received as result of HIST command. It reports the edition of an existing message.
      *
      * Receive: <chatid> <userid> <msgid> <ts_send> <ts_update> <keyid> <msglen> <msg>
      */
    OP_MSGUPD = 4,

    /**
      * @brief Received as result of HIST comand. It reports the last seen message id
      *
      * Receive: <chatid> <msgid>
      *
      */
    OP_SEEN = 5,

    /**
      * @brief It notifies about reception of a message
      *
      * Send: <chatid> <msgid>
      *
      * Receive: <chatid> <msgid>
      *
      */
    OP_RECEIVED = 6,

    OP_RETENTION = 7,

    /**
      * @brief Usually sent immediately after JOIN, it retrieves history from the chatroom.
      *
      * Send: <chatid> <count>
      * @note count is usually a negative number to fetch old history, possitive for new history.
      *
      * This command results in receiving:
      *  - the last SEEN message
      *  - the last RECEIVED message
      *  - the NEWKEY to encrypt/decrypt history
      *  - zero or more OLDMSG
      *  - a notification when HISTDONE
      *
      */
    OP_HIST = 8,

    OP_RANGE = 9,
    OP_NEWMSGID = 10,

    /**
      * @brief Received when the server rejects a command
      *
      * Received: <chatid> <generic_id> <op_code> <reason>
      *
      * Server can reject:
      *  - JOIN: the user doesn't participate in the chatroom
      *  - NEWMSG | MSGUPD | MSGUPDX: participants have changed or the message is too old
      */
    OP_REJECT = 11,

    OP_BROADCAST = 12,

    /**
      * @brief Received as result of HIST command, it notifies the end of history fetch.
      *
      * Receive: <chatid>
      *
      * @note There may be more history in server, but the HIST <count> is already satisfied.
      *
      */
    OP_HISTDONE = 13,

    /**
      * @brief Received when there's a new key for the chatroom.
      *
      * Send: <chatid> <keyid> <keylen> <key>
      * @note keyid is currently not used for any purpose.
      *
      */
    OP_NEWKEY = 17,
    OP_KEYID = 18,
    OP_JOINRANGEHIST = 19,  /// <chatid> <oldest_known_msgid> <newest_known_msgid>
    OP_MSGUPDX = 20,
    OP_MSGID = 21,
    OP_CLIENTID = 24,
    OP_RTMSG_BROADCAST = 25,
    OP_RTMSG_USER = 26,
    OP_RTMSG_ENDPOINT = 27,
    OP_KEEPALIVEAWAY = 30,
    OP_LAST = OP_KEEPALIVEAWAY
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
    enum: uint8_t
    {
        kMsgInvalid           = 0x00,
        kMsgNormal            = 0x01,
        kMsgManagementLowest  = 0x02,
        kMsgAlterParticipants = 0x02,
        kMsgTruncate          = 0x03,
        kMsgPrivChange        = 0x04,
        kMsgChatTitle         = 0x05,
        kMsgManagementHighest = 0x05,
        kMsgUserFirst         = 0x10,
        kMsgAttachment        = 0x10,
        kMsgRevokeAttachment  = 0x11,
        kMsgContact           = 0x12

    };
    enum Status
    {
        kSending, //< Message has not been sent or is not yet confirmed by the server
        kSendingManual, //< Message is too old to auto-retry sending, or group composition has changed. User must explicitly confirm re-sending. All further messages queued for sending also need confirmation
        kServerReceived, //< Message confirmed by server, but not yet delivered to recepient(s)
        kServerRejected, //< Message is rejected by server for some reason (editing too old message for example)
        kDelivered, //< Peer confirmed message receipt. Used only for 1on1 chats
        kLastOwnMessageStatus = kDelivered, //if a status is <= this, we created the msg, oherwise not
        kNotSeen, //< User hasn't read this message yet
        kSeen //< User has read this message
    };
    enum { kFlagForceNonText = 0x01 };
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

private:
//avoid setting the id and flag pairs one by one by making them accessible only by setXXX(Id,bool)
    karere::Id mId;
    bool mIdIsXid = false;
protected:
    uint8_t mIsEncrypted = 0; //0 = not encrypted, 1 = encrypted, 2 = encrypted, there was a decrypt error
    uint8_t mFlags = 0;
public:
    karere::Id userid;
    uint32_t ts;
    uint16_t updated;
    uint32_t keyid;
    unsigned char type;
    BackRefId backRefId;
    std::vector<BackRefId> backRefs;
    mutable void* userp;
    mutable uint8_t userFlags = 0;
    karere::Id id() const { return mId; }
    bool isSending() const { return mIdIsXid; }
    uint8_t isEncrypted() const { return mIsEncrypted; }
    void setEncrypted(uint8_t encrypted) { mIsEncrypted = encrypted; }
    void setId(karere::Id aId, bool isXid) { mId = aId; mIdIsXid = isXid; }
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
     * @returns A reference to the newly created and filled ManagementInfo structure */
    ManagementInfo& createMgmtInfo(const ManagementInfo& src)
    {
        assert(empty());
        append(&src, sizeof(src));
        return *reinterpret_cast<ManagementInfo*>(buf());
    }

    static const char* statusToStr(unsigned status)
    {
        return (status > kSeen) ? "(invalid status)" : statusNames[status];
    }
    void generateRefId();
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
    bool isManagementMessage() const { return type >= kMsgManagementLowest && type <= kMsgManagementHighest; }
    bool isText() const
    {
        return (!empty()                                // skip deleted messages
                && ((mFlags & kFlagForceNonText) == 0)  // only want text messages
                && (type == kMsgNormal                  // include normal messages
                    || type == kMsgAttachment           // include node-attachment messages
                    || type == kMsgContact));           // include contact-attachment messages
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
protected:
    static const char* statusNames[];
    friend class Chat;
};

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
protected:
    Command(uint8_t opcode, uint8_t reserve, uint8_t dataSize)
    : Buffer(reserve, dataSize) { write(0, opcode); }
    Command(const char* data, size_t size): Buffer(data, size){}
public:
    enum { kBroadcastUserTyping = 1 };
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
        return ((op == OP_NEWMSG) || (op == OP_MSGUPD) || (op == OP_MSGUPDX));
    }
    uint8_t opcode() const { return read<uint8_t>(0); }
    static const char* opcodeToStr(uint8_t opcode);
    const char* opcodeName() const { return opcodeToStr(opcode()); }
    virtual std::string toString() const;
    static const char* opcodeToStr(uint8_t code);
    virtual std::string toString() const;
    virtual ~Command(){}
};

class KeyCommand: public Command
{
public:
    explicit KeyCommand(karere::Id chatid, uint32_t keyid=CHATD_KEYID_UNCONFIRMED,
        size_t reserve=128)
    : Command(OP_NEWKEY, reserve)
    {
        append(chatid.val).append<uint32_t>(keyid).append<uint32_t>(0); //last is length of keys payload, initially empty
    }
    KeyCommand(): Command(){} //for db loading
    KeyId keyId() const { return read<uint32_t>(9); }
    void setChatId(karere::Id aChatId) { write<uint64_t>(1, aChatId.val); }
    void setKeyId(uint32_t keyid) { write(9, keyid); }
    void addKey(karere::Id userid, void* keydata, uint16_t keylen)
    {
        assert(keydata && (keylen != 0));
        uint32_t& payloadSize = mapRef<uint32_t>(13);
        payloadSize+=(10+keylen); //userid.8+len.2+keydata.keylen
        append<uint64_t>(userid.val).append<uint16_t>(keylen);
        append(keydata, keylen);
    }
    bool hasKeys() const { return dataSize() > 17; }
    void clearKeys() { setDataSize(17); } //opcode.1+chatid.8+keyid.4+length.4
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
    MsgCommand(size_t reserve): Command(reserve) {} //for loading the buffer
    karere::Id msgid() const { return read<uint64_t>(17); }
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
    virtual std::string toString() const;
};

class RtMessageComposer: public Command
{
protected:
    using Command::read; // hide all read/write methods, as they will include the
    using Command::write; // whole command, not the payload
public:
    enum { kHdrLen = 23 };
    /** Creates an RtMessage as a base for derived classes (with userid/clientid)
     * @param opcode The chatd command opcode. Can be OP_RTMSG_BROADCAST,
     * OP_RTMSG_USER, OP_RTMSG_CLIENT
     * @param type The payload-specific type. This is the first byte of the payload
     * @param reserve How much bytes to reserve in the buffer for the payload data.
     * This does not include the payload type byte.
     * @param hdrlen The length of the header. This is used to calculate the data
     * length field from the total buffer length. Does not include the payload type byte,
     * which is part of the payload data.
     */
    RtMessageComposer(uint8_t opcode, Type type, uint16_t reserve=32)
    : Command(opcode, kHdrLen+1+reserve)
    {
        //(opcode.1 chatid.8 userid.8 clientid.4 len.2) (type.1 data.(len-1))
        //              ^                                          ^
        //          header.23                             payload.len
        setDataSize(kHdrLen+1);
        append<uint8_t>(kHdrLen, type);
        updateLenField();
    }
    void updateLenField()
    {
        assert(dataSize()-kHdrLen >= 1);
        Buffer::write<uint16_t>(kHdrLen-2, dataSize()-kHdrLen);
    }
    void setChatid(karere::Id chatid) { Buffer::write<uint64_t>(1, chatid.val); }
    void setUserid(karere::Id userid) { Buffer::write<uint64_t>(9, userid.val); }
    void setClientid(uint32_t clientid) { Buffer::write<uint32_t>(17, clientid); }
public:
    template <class T, bool check=true, typename=typename std::enable_if<std::is_pod<T>::value>::type>
    const T& payloadRead(size_t offset) const
    {
        return Buffer::read<T>(kHdrLen+1+offset);
    }
    template<class T, bool check=true, typename=typename std::enable_if<std::is_pod<T>::value>::type>
    void payloadWrite(size_t offset, T val)
    {
        write<T>(offset+kHdrLen+1, val);
    }
    const char* payloadReadPtr(size_t offset, size_t len)
    {
        return Buffer::readPtr(mHdrLen+1+offset, len);
    }
    const char* payloadPtr() const { return buf()+kHdrLen+1; }
    const char* payloadPtr(uint16_t offset) const
    {
        auto dataOfs = kHdrLen+1+offset;
        if (dataOfs >= dataSize())
            throw std::runtime_error("RtMessage::payloadPtr: offset "+std::to_string(offset)+" points past end of data (dataSize="+std::to_string(dataSize())+")");
        return buf()+offset;
    }
    size_t payloadSize() const { return dataSize()-kHdrLen-1; }
    bool hasPayload() const { return payloadSize() > 0; }
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
{kChatStateOffline = 0, kChatStateConnecting, kChatStateJoining, kChatStateOnline};
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
