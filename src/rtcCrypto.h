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

const static uint8_t KEY_ENCRYPT_IV_LENGTH = 12;

class RtcCryptoMeetings: public rtcModule::IRtcCryptoMeetings
{
protected:
    karere::Client& mClient;
    void computeSymmetricKey(karere::Id peer, strongvelope::SendKey& output);
public:
    RtcCryptoMeetings(karere::Client& client);
    void decryptKeyFrom(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) override;
    void encryptKeyTo(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output) override;
    void xorWithCallKey(const strongvelope::SendKey &callKey, strongvelope::SendKey &sendKey) override;
    void xorWithCallKey(const byte* callKey, byte* key);
    std::shared_ptr<strongvelope::SendKey> generateSendKey() override;
    promise::Promise<Buffer*> getCU25519PublicKey(const karere::Id &peer) override;
    static std::string keyToStr(const strongvelope::SendKey& key);
    static strongvelope::SendKey strToKey(const std::string& keystr);
    static void strToKey(const std::string& keystr, strongvelope::SendKey &res);

    /**
     * @brief Derive user public ephemeral key with own user private ephemeral key (SHA256 - based HKDF transform)
     */
    bool deriveEphemeralKey(std::string& peerEphemeralPubkey, const byte* privEphemeral, std::string& derivedKeyPair, const std::vector<std::string>& peerIvs, const std::vector<std::string>& myIvs);

    /**
     * @brief Verify call participant ephemeral public key signature
     * This method verify call participant ephemeral public key signature
     */
    promise::Promise<bool>
    verifyKeySignature(const std::string& msg, const std::string& recvsignature, const karere::Id &chatid, const karere::Id& peer);

    /**
     * @brief Get my user Ed25519 keypair (for EdDSA signature)
     * @return My user Ed25519 keypair
     */
    std::pair<strongvelope::EcKey, strongvelope::EcKey> getEd25519Keypair();
};
}
#endif // MEGACRYPTOFUNCTIONS_H
