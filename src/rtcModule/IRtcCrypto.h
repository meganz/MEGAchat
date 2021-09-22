#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include <string>
#include <promise.h>
#include <karereId.h>
#include "strongvelope.h"

namespace rtcModule
{

class IRtcCryptoMeetings
{
public:
    virtual void decryptKeyFrom(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) = 0;
    virtual void encryptKeyTo(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) = 0;
    virtual void xorWithCallKey(const strongvelope::SendKey &callKey, strongvelope::SendKey &sendKey) = 0;
    virtual std::shared_ptr<strongvelope::SendKey> generateSendKey() = 0;
    virtual promise::Promise<Buffer*> getCU25519PublicKey(const karere::Id &peer) = 0;
    virtual ~IRtcCryptoMeetings(){}
};
}

#endif // ICRYPTOFUNCTIONS_H
