#ifndef MEGACRYPTOFUNCTIONS_H
#define MEGACRYPTOFUNCTIONS_H
#include <map>
#include "IRtcCrypto.h"

#ifndef ENABLE_CHAT
    #define ENABLE_CHAT 1
#endif
namespace karere { class Client; }
namespace strongvelope { template<size_t L> class Key; typedef Key<16> SendKey; }
namespace rtcModule
{
class RtcCrypto: public rtcModule::IRtcCrypto
{
protected:
    karere::Client& mClient;
    void computeSymmetricKey(karere::Id peer, strongvelope::SendKey& output);
public:
    RtcCrypto(karere::Client& client);
    virtual void mac(const std::string& data, const SdpKey& key, SdpKey& output);
    virtual void decryptKeyFrom(karere::Id peer, const SdpKey& data, SdpKey& output);
    virtual void encryptKeyTo(karere::Id peer, const SdpKey& data, SdpKey& output);
    virtual promise::Promise<void> waitForPeerKeys(karere::Id peer);
    virtual karere::Id anonymizeId(karere::Id userid);
    virtual void random(char* buf, size_t len);
};

}
#endif // MEGACRYPTOFUNCTIONS_H
