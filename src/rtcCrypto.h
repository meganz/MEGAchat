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
// Asymmetric cryptography using ECDH X25519.
static constexpr int X25519_PRIV_KEY_LEN = crypto_box_SECRETKEYBYTES;
static constexpr int X25519_PUB_KEY_LEN  = crypto_box_PUBLICKEYBYTES;
struct X25519KeyPair
{
    unsigned char privKey[X25519_PRIV_KEY_LEN];
    unsigned char pubKey[X25519_PUB_KEY_LEN];
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
};
}
#endif // MEGACRYPTOFUNCTIONS_H
