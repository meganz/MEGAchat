#ifndef MESSAGES_H
#define MESSAGES_H
namespace rtcModule
{
typedef karere::Id Sid;
template <int minSize>
struct Message: public chatd::RtMessageWithEndpoint
{
    enum: uint8_t
    {
        kTypeCallRequest = 2,
        kTypeCallReqHandledElsewhere = 3,
        kTypeCallReqCanceled = 4,
        kTypeJoin = 5,
        kTypeSdpOffer = 6,
        kTypeSdpAnswer = 7,
        kTypeIceCanidate = 8,
        kTypeHangup = 9
    };
#ifndef NDEBUG
    enum: bool { kDebugReads = true };
#else
    enum: bool { kDebugReads = false };
#endif
    static inline const char* typeToString(uint8_t type);
    using chatd::RtMessageWithEndpoint::RtMessageWithEndpoint;
    template <class T, bool clearSrc=false>
    static T* castTo(RtMessageWithEndpoint*& msg)
    {
        auto ret = static_cast<T*>(msg);
        ret->verifyMinSize();
        if (clearSrc)
            msg = nullptr;
        return ret;
    }
    verifyMinSize() const
    {
        if (payloadSize() < minSize)
            throw std::runtime_error(std::string(msgTypeToString())+" message has too small payload");
    }
    template <class T>
    T read(uint16_t offset)
    {
        return payloadRead<T, kDebugReads>(offset);
    }
    const char* ptr(uint16_t offset) { return payloadPtr<kDebugReads>(offset); }
    template<class T=uint16_t>
    std::string readString(uint16_t offset)
    {
        auto len = read<T>(offset);
        return std::string(payloadReadPtr(offset+sizeof(T), len), len);
    }
};

//rid.4
struct CallRequest: public Message<4>
{
    uint32_t rid() const { return read<uint32_t>(0); }
    virtual std::string toString() const
    { return std::string("CALL_REQUEST: rid=")+std::to_string(rid()); }
};

//nonce.16 anonId.12
struct JoinRequest: public Message<28>
{
    const char* nonce() const { return ptr(0); }
    const char* anonId() const { return ptr(16); }
    std::string toString() const
    {
        std::string result;
        result.reserve(64);
        result.append("JOIN: nonce=")
              .append(base64urlencode(nonce, 16))
              .append(", anonId=")
              .append(base64urlencode(anonId(), 12));
        return result;
    }
};

//rid.4 caller.8 callerClientId.4 answered.1
struct NotifyCallHandled: public Message<17>
{
    uint32_t rid() const { return read<uint32_t>(0); }
    karere::Id caller() const { return read<uint64_t>(4); }
    chatd::ClientId callerClient() const { return read<uint32_t>(12); }
    bool accepted() const { return read<uint8_t>(16) != 0; }
    virtual std::string toString() const
    {
        char buf[10];
        snprintf(buf, 10, callerClient());
        std::string result;
        result.reserve(64);
        result.append("CALL_HANDLED: rid = ")
              .append(std::to_string(rid()))
              .append(", caller= ").append(caller.toString())
              .append("[").append(buf).append("], accepted=")
              .append(std::to_string(accepted()));
        return result;
    }
};

template <int minSize>
struct SessMessage: public Message<minSize>
{
    Sid sid() { return read<uint64_t>(0); }
    virtual std::string toString() const
    {
        return std::string("cmd ").append(std::to_string(type()))
            .append(": sid=").append(sid().toString());
    }
};

//sid.8 nonce.16 anonId.12 hash.32 av.1 sdpLen.2 sdpOffer.sdpLen
struct SdpOffer: public SessMessage<71>
{
    const char* nonce() const { return ptr(8); }
    const char* anonId() const { return ptr(24); }
    const char* encryptedFprHash() const { return ptr(36); }
    AvFlags av() const { return AvFlags(read<uint8_t>(68)); }
    std::string sdpOffer() const { return readString(69); }
    std::string toString()
    {
        std::string result;
        result.reserve(64);
        result.append("SDP_OFFER: sid=").append(sid.toString())
              .append(", nonce=).append(base64urlencode(nonce(), 16)
              .append(", anonId=").append(base64urlencode(anonId, 12)
              .append(", av=").append(av().tostring());
       return result;
    }
};

//sid.8 hash.32 av.1 sdpLen.2 sdpOffer.sdpLen
struct SdpAnswer: public SessMessage<43>
{
    const char* encryptedFprHash() const { return ptr(8); }
    AvFlags av() const { return AvFlags(read<uint8_t>(9)); }
    std::string sdpOffer() const { return readString(10); }
    std::string toString()
    {
        std::string result;
        result.reserve(64);
        result.append("SDP_ANSWER: sid=").append(sid().toStrong())
              .append(", av=").append(av().toString());
    }
};

//sid.8 mlineIdx.1 midLen.1 mid.midLen candLen.2 cand.candLen
struct IceCandidate: public SessMessage<14>
{
    uint8_t mLineIdx() const { return read<uint8_t>(8); }
    std::string mid() const { return readString(10); }
    std::string cand() const { readString(10+read<uint8_t>(10)); }
    std::string toString() const
    {
        std::string result;
        auto text = cand();
        result.reserve(32+text.size());
        result.append("ICE_CAND: mid= ").append(mid()).append(", cand=\n'")
        .append(text)+='\'';
        return result;
    }
};
}
#endif // MESSAGES_H

