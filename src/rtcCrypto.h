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
class RtcCryptoMeetings: public rtcModule::IRtcCryptoMeetings
{
protected:
    karere::Client& mClient;
    void computeSymmetricKey(const karere::Id &peer, strongvelope::SendKey& output);
public:
    RtcCryptoMeetings(karere::Client& client);
    void decryptKeyFrom(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) override;
    void encryptKeyTo(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) override;
    void xorWithCallKey(const strongvelope::SendKey &callKey, strongvelope::SendKey &sendKey) override;
    void xorWithCallKey(const mega::byte* callKey, mega::byte* key);
    std::shared_ptr<strongvelope::SendKey> generateSendKey() override;
    promise::Promise<Buffer*> getCU25519PublicKey(const karere::Id &peer) override;
    static std::string keyToStr(const strongvelope::SendKey& key);
    static strongvelope::SendKey strToKey(const std::string& keystr);
    static void strToKey(const std::string& keystr, strongvelope::SendKey &res);

    /**
     * @brief Verify call participant ephemeral public key signature
     * This method verify call participant ephemeral public key signature
     */
    promise::Promise<bool>
    verifyKeySignature(const std::string& msg, const std::string& recvsignature, const karere::Id &chatid, const karere::Id& peer);

    /**
     * @brief sign ephemeral key with Ed25519 key
     * This method signs string: sesskey|<callId>|<clientId>|<pubkey> with Ed25519 key and encode in B64
     *
     * @param str plain ephemeral key string with the format: sesskey|<callId>|<clientId>|<pubkey>
     * @return a Base64 encoded string which represents the signed ephemeral key with Ed25519 key
     */
    std::string signEphemeralKey(const std::string& str) const;

    /**
     * @brief Get my user Ed25519 keypair (for EdDSA signature)
     * @return My user Ed25519 keypair
     */
    std::pair<strongvelope::EcKey, strongvelope::EcKey> getEd25519Keypair() const;
};
}
#endif // MEGACRYPTOFUNCTIONS_H
