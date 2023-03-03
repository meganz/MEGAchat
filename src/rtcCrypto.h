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

// Asymmetric cryptography using ECDH X25519.
static constexpr int X25519_PRIV_KEY_LEN = crypto_box_SECRETKEYBYTES;
static constexpr int X25519_PUB_KEY_LEN  = crypto_box_PUBLICKEYBYTES;
struct X25519KeyPair
{
    unsigned char privKey[X25519_PRIV_KEY_LEN];
    unsigned char pubKey[X25519_PUB_KEY_LEN];

    X25519KeyPair() = default;
    X25519KeyPair(const X25519KeyPair &aux)
    {
        memcpy(privKey, aux.privKey, X25519_PRIV_KEY_LEN);
        memcpy(pubKey, aux.pubKey, X25519_PUB_KEY_LEN);
    }

    X25519KeyPair(const strongvelope::EcKey& priv, const strongvelope::EcKey& pub)
    {
        memcpy(privKey, priv.ubuf(), priv.bufSize());
        memcpy(pubKey, pub.ubuf(), pub.bufSize());
    }

    X25519KeyPair* copy() const
    {
        return new X25519KeyPair(*this);
    }
};

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
    std::shared_ptr<strongvelope::SendKey> generateSendKey() override;
    promise::Promise<Buffer*> getCU25519PublicKey(const karere::Id &peer) override;
    static std::string keyToStr(const strongvelope::SendKey& key);
    static strongvelope::SendKey strToKey(const std::string& keystr);
    static void strToKey(const std::string& keystr, strongvelope::SendKey &res);

    /** Asymmetric cryptography using ECDH X25519. */
    /**
     * @brief Generates an X25519 EC key pair:
     *  - The private key size in raw format is 32 bit
     *  - The public  key size in raw format is 32 bit
     *
     * @return a pointer to X25519KeyPair
     */
    X25519KeyPair* genX25519KeyPair();

    /**
     * @brief Derive user public ephemeral key with own user private ephemeral key (SHA256 - based HKDF transform)
     */
    bool deriveEphemeralKey(std::string& peerEphemeralPubkey, const unsigned char* privEphemeral, strongvelope::EcKey &output, const std::vector<std::string>& peerIvs, const std::vector<std::string> &myIvs);

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
