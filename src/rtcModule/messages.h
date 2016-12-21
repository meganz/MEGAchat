#ifndef MESSAGES_H
#define MESSAGES_H
namespace rtcModule
{
typedef karere::Id Sid;
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
    static inline const char* typeToString(uint8_t type);
};

const char* msgTypeToString()
template <int minSize>
struct SessMessage: public chatd::RtMessageWithEndpoint
{
    verifyMinSize() const
    {
        if (payloadSize() < minSize)
            throw std::runtime_error(std::string(msgTypeToString())+" message has too small payload");
    }
    Sid sid() { return readPayload<uint64_t>(0); }
    using chatd::RtMessageWithEndpoint::RtMessageWithEndpoint;
};

//sid.8 nonce.16 anonId.12 av.1 sdpLen.2 sdpOffer.sdpLen
struct SdpOffer: public SessMessage
{
    const char* nonce() const { return payloadReadPtr(8, 16); }
    const char* anonId() const { return payloadReadPtr()
};
}
#endif // MESSAGES_H

