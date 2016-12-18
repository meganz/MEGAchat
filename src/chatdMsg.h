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
    OP_JOIN = 1,
    OP_OLDMSG = 2,
    OP_NEWMSG = 3,
    OP_MSGUPD = 4,
    OP_SEEN = 5,
    OP_RECEIVED = 6,
    OP_RETENTION = 7,
    OP_HIST = 8,
    OP_RANGE = 9,
    OP_NEWMSGID = 10,
    OP_REJECT = 11,
    OP_BROADCAST = 12,
    OP_HISTDONE = 13,
    OP_NEWKEY = 17,
    OP_KEYID = 18,
    OP_JOINRANGEHIST = 19,
    OP_MSGUPDX = 20,
    OP_MSGID = 21,
    OP_CLIENTID = 24,
    OP_RTMSG_BROADCAST = 25,
    OP_RTMSG_USER = 26,
    OP_RTMSG_ENDPOINT = 27
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
    enum: unsigned char
    {
        kMsgInvalid = 0,
        kMsgNormal = 1,
        kMsgManagementLowest = 2,
        kMsgAlterParticipants = 2,
        kMsgTruncate = 3,
        kMsgPrivChange = 4,
        kMsgChatTitle = 5,
        kMsgManagementHighest = 5,
        kMsgUserFirst = 16
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
    bool mIsEncrypted = false;
public:
    karere::Id userid;
    uint32_t ts;
    uint16_t updated;
    uint32_t keyid;
    unsigned char type;
    BackRefId backRefId;
    std::vector<BackRefId> backRefs;
    mutable void* userp;
    mutable uint16_t userFlags = 0;
    karere::Id id() const { return mId; }
    bool isSending() const { return mIdIsXid; }
    bool isEncrypted() const { return mIsEncrypted; }
    void setId(karere::Id aId, bool isXid) { mId = aId; mIdIsXid = isXid; }
    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
          Buffer&& buf, bool aIsSending=false, KeyId aKeyid=CHATD_KEYID_INVALID,
          unsigned char aType=kMsgNormal, void* aUserp=nullptr)
      :Buffer(std::forward<Buffer>(buf)), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid),
          ts(aTs), updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}
    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
            const char* msg, size_t msglen, bool aIsSending=false,
            KeyId aKeyid=CHATD_KEYID_INVALID, unsigned char aType=kMsgNormal, void* aUserp=nullptr)
        :Buffer(msg, msglen), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid), ts(aTs),
            updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}

    /** @brief Returns the ManagementInfo structure contained within the message
     * content. Throws if the message is not a management message, or if the
     * size of the message contents is smaller than the size of ManagementInfo,
     * but otherwise does not guarentee that the data inside the message
     * is actually a ManagementInfo structure
     */
    ManagementInfo& mgmtInfo() { throwIfNotManagementMsg(); return read<ManagementInfo>(0); }

    /** @brief A \c const version of mgmtInfo() */
    const ManagementInfo& mgmtInfo() const { throwIfNotManagementMsg(); return read<ManagementInfo>(0); }

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
    Command(uint8_t opcode, uint8_t reserve=64): Command(opcode, reserve, 0){}
    Command(Command&& other): Buffer(std::forward<Buffer>(other)) {assert(!other.buf() && !other.bufSize() && !other.dataSize());}
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
    const char* opcodeName() const { return opcodeToStr(opcode()); }
    static const char* opcodeToStr(uint8_t code);
    virtual void toString(char* buf, size_t bufsize) const;
    virtual ~Command(){}
};

class KeyCommand: public Command
{
public:
    explicit KeyCommand(karere::Id chatid=karere::Id::null(), uint32_t keyid=CHATD_KEYID_UNCONFIRMED,
        uint8_t reserve=128)
    : Command(OP_NEWKEY, reserve)
    {
        append(chatid.val).append<uint32_t>(keyid).append<uint32_t>(0); //last is length of keys payload, initially empty
    }
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
    virtual void toString(char* buf, size_t bufsize) const;
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
    MsgCommand(): Command() {} //for loading the buffer
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
    virtual void toString(char* buf, size_t bufsize) const;
};

class RtMessage: public Command
{
public:
    typedef uint8_t Type;
    enum: Type { kQueryBit = 0x80 };
    typedef uint32_t QueryId;
protected:
    uint8_t mHdrLen;
    /** Crates an RtMessage as a base for derived classes (with userid/clientid)
     * @param opcode The chatd command opcode. Can be OP_RTMSG_BROADCAST,
     * OP_RTMSG_USER, OP_RTMSG_CLIENT
     * @param type The payload-specific type. This is the first byte of the payload
     * @param reserve How much bytes to reserve in the buffer for the payload data.
     * This does not include the payload type byte.
     * @param hdrlen The length of the header. This is used to calculate the data
     * length field from the total buffer length. Does not include the payload type byte,
     * which is part of the payload data.
     */
    RtMessage(uint8_t opcode, Type type, uint16_t reserve, unsigned char hdrlen)
        : Command(opcode, hdrlen+1+reserve, hdrlen), mHdrLen(hdrlen)
    {
        //(opcode.1 chatid.8 [userid.8 [clientid.4]] len.2) (type.1 data.(len-1))
        //              ^                                          ^
        //          header.hdrlen                             payload.len
        assert(mHdrLen >= 11); //the minimum one is for RTMSG_BROADCAST and is 11
        Buffer::write<uint8_t, false>(0, opcode);
        Buffer::write<uint8_t, false>(11, type);
        //chatid and payload length are unknown at this point, we only reserve the space for them
    }
    RtMessage(const char* data, size_t dataSize, uint8_t hdrlen)
        : Command(data, dataSize), mHdrLen(hdrlen){} //sanity check is done in the RtMessageWithEndpoint, as it is the only one that can be instantiated from raw data
    void updateLenField()
    {
        Buffer::write<uint16_t, false>(mHdrLen-2, dataSize()-mHdrLen);
    }
    void updateChatidField(karere::Id chatid)
    {
        Buffer::write<uint64_t, false>(1, chatid.val);
    }
    using Command::read; //hide all read methods, as they will read the whole command, not the payload
    using Command::write;
public:
    RtMessage(Type type, uint16_t reserve): RtMessage(OP_RTMSG_BROADCAST, type, reserve, 11){}
    Type type() const { return read<uint8_t, false>(mHdrLen); }
    uint8_t hdrLen() const { return mHdrLen; }
    void finalizeFields(karere::Id chatid)
    {
        updateLenField(); updateChatidField(chatid);
    }
    template <class T, bool check=true, typename=typename std::enable_if<std::is_pod<T>::value>::type>
    const T& readPayload(size_t offset) const
    {
        return Buffer::read<T, check>(mHdrLen+1+offset);
    }
    template<class T, bool check=true, typename=typename std::enable_if<std::is_pod<T>::value>::type>
    void writePayload(size_t offset, T val)
    {
        write<T,check>(offset+mHdrLen, val);
    }
    const char* payloadReadPtr(size_t offset, size_t len)
    {
        return Buffer::readPtr(mHdrLen+offset, len);
    }
    const char* payloadPtr() const { return buf()+mHdrLen; }
    size_t payloadSize() const { return dataSize() - mHdrLen; }
    bool hasPayload() const { return payloadSize() > 1; }
};

class RtMessageWithUser: public RtMessage
{
public:
    struct Key
    {
        Type type;
        karere::Id userid;
        bool operator<(Key other) const
        {
            if (type != other.type)
                return type < other.type;
            else
                return userid < other.userid;
        }
        Key(Type aType, karere::Id aId)
        : type(aType), userid(aId){}
    };
protected:
    RtMessageWithUser(uint8_t opcode, Type type, karere::Id userid,
        uint16_t reserve, uint8_t hdrlen)
        :RtMessage(opcode, type, reserve, hdrlen)
    {
        //(opcode.1 chatid.8 userid.8 len.2) (type.1 data.len)
        assert(mHdrLen >= 19);
        Buffer::write<uint64_t>(9, userid.val);
    }
    RtMessageWithUser(const char* data, size_t dataSize, uint8_t hdrlen)
        :RtMessage(data, dataSize, hdrlen){}
public:
    karere::Id userid() const { return read<uint64_t, false>(9); }
    RtMessageWithUser(Type type, karere::Id userid, uint16_t reserve)
        :RtMessageWithUser(OP_RTMSG_USER, type, userid, reserve, 19){}
};

class RtMessageWithEndpoint: public RtMessageWithUser
{
public:
    struct Key: public RtMessageWithUser::Key
    {
        ClientId clientid;
        bool operator<(Key other) const
        {
            if (type != other.type)
                return type < other.type;
            if (userid != other.userid)
                return userid < other.userid;
            return clientid < other.clientid;
        }
        Key(Type aType, karere::Id aId, ClientId aClientId)
            :RtMessageWithUser::Key(aType, aId), clientid(aClientId){}
    };
    ClientId clientid() const { return read<uint32_t, false>(17); }
    QueryId queryId() const
    {
        if ((type() & kQueryBit) == 0)
            throw std::runtime_error("Message is not a query");
        return read<QueryId>(1);
    }
    RtMessageWithEndpoint(Type aType, karere::Id aUserid, ClientId aClientid, uint16_t reserve)
        :RtMessageWithUser(OP_RTMSG_ENDPOINT, aType, aUserid, reserve, 23)
    {
        //(opcode.1 chatid.8 userid.8 clientid.4 len.2) (type.1 payload.len)
        Buffer::write<uint32_t>(17, aClientid);
    }

    //parses received message buffers
    RtMessageWithEndpoint(const char* data, size_t dataSize)
        :RtMessageWithUser(data, dataSize, 23)
    {
        if (dataSize < 24) //header.23 + type.1
            throw std::runtime_error("RtMessageWithEndpoint: Data is smaller than header size");
#ifndef NDEBUG
        assert(opcode() == OP_RTMSG_ENDPOINT); //guaranteed by code (the receive function's switch() statement)
        auto payloadLen = read<uint16_t>(21);
        assert(payloadLen == dataSize-23); //this should be guaranteed by the code - the buffer size is calculated from the payload length field
#endif
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
