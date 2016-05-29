#ifndef __CHATD_MSG_H__
#define __CHATD_MSG_H__

#include <stdint.h>
#include <string>
#include <buffer.h>
#include "karereId.h"

enum { CHATD_KEYID_INVALID = 0, CHATD_KEYID_UNCONFIRMED = 0xffffffff };

namespace chatd
{

//typedef karere::Id Id;
typedef uint32_t KeyId;
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
    OP_LAST = OP_MSGID
};

// privilege levels
enum Priv: signed char
{
    PRIV_NOCHANGE = -2,
    PRIV_NOTPRESENT = -1,
    PRIV_RDONLY = 0,
    PRIV_RDWR = 1,
    PRIV_FULL = 2,
    PRIV_OPER = 3
};

class Message: public Buffer
{
public:
    enum Type: unsigned char
    {
        kNormalMsg = 0,
        kUserMsg = 16,
        kUserMsgAlterParticipants = 16
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
    Type type;
    mutable void* userp;
    mutable uint16_t userFlags = 0;
    karere::Id id() const { return mId; }
    bool isSending() const { return mIdIsXid; }
    bool isEncrypted() const { return mIsEncrypted; }
    void setId(karere::Id aId, bool isXid) { mId = aId; mIdIsXid = isXid; }
    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
          Buffer&& buf, bool aIsSending=false, KeyId aKeyid=CHATD_KEYID_INVALID,
          Type aType=kNormalMsg, void* aUserp=nullptr)
      :Buffer(std::forward<Buffer>(buf)), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid),
          ts(aTs), updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp){}
    explicit Message(karere::Id aMsgid, karere::Id aUserid, uint32_t aTs, uint16_t aUpdated,
            const char* msg, size_t msglen, bool aIsSending=false,
            KeyId aKeyid=CHATD_KEYID_INVALID, Type aType=kNormalMsg, void* aUserp=nullptr)
        :Buffer(msg, msglen), mId(aMsgid), mIdIsXid(aIsSending), userid(aUserid), ts(aTs),
            updated(aUpdated), keyid(aKeyid), type(aType), userp(aUserp) {}
    static const char* statusToStr(unsigned status)
    {
        return (status > kSeen) ? "(invalid status)" : statusNames[status];
    }
protected:
    static const char* statusNames[];
    friend class Chat;
};

class Command: public Buffer
{
private:
    Command(const Command&) = delete;
protected:
    static const char* opcodeNames[];
public:
    Command(): Buffer(){}
    Command(Command&& other): Buffer(std::forward<Buffer>(other)) {assert(!other.buf() && !other.bufSize() && !other.dataSize());}
    Command(uint8_t opcode, uint8_t reserve=64): Buffer(reserve) { write(0, opcode); }
    template<class T>
    Command&& operator+(const T& val)
    {
        append(val);
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
    static const char* opcodeToStr(uint8_t code)
    {
        return (code > OP_LAST) ? "(invalid opcode)" : opcodeNames[code];
    }
    virtual ~Command(){}
};
class KeyCommand: public Command
{
public:
    explicit KeyCommand(karere::Id chatid=karere::Id::null(), uint32_t keyid=CHATD_KEYID_UNCONFIRMED,
        size_t reserve=64)
    : Command(OP_NEWKEY, reserve)
    {
        append(chatid).append(keyid).append<uint32_t>(0); //last is length of keys payload, initially empty
    }
    KeyId keyId() const { return read<uint32_t>(9); }
    void setChatId(karere::Id aChatId) { write<uint64_t>(1, aChatId.val); }
    void setKeyId(uint32_t keyid) { write(9, keyid); }
    void addKey(karere::Id userid, void* keydata, uint16_t keylen)
    {
        uint32_t& payloadSize = mapRef<uint32_t>(13);
        payloadSize+=(10+keylen); //userid.8+len.2+keydata.keylen
        append(keydata, keylen);
    }
    bool hasKeys() const { return dataSize() > 17; }
    void clearKeys() { setDataSize(17); } //opcode.1+chatid.8+keyid.4+length.4
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
        write(1, chatid);write(9, userid);write(17, msgid);write(25, ts);
        write(29, updated);write(31, keyid);write(35, 0); //msglen
    }
    MsgCommand(): Command() {} //for loading the buffer
    karere::Id msgid() const { return read<uint64_t>(17); }
    void setId(karere::Id aMsgid) { write(17, aMsgid); }
    KeyId keyId() const { return read<KeyId>(31); }
    void setKeyId(KeyId aKeyid) { write(31, aKeyid); }
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

}

#endif
