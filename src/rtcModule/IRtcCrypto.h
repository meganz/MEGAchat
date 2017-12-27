#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include <string>
#include <promise.h>
#include <karereId.h>

namespace rtcModule
{
struct SdpKey;
/** @brief Interface for implementing the SRTP fingerprint verification
 *
 * The fingerprint verification resembles the ZRTP verification, but uses
 * the Mega key infrastructure instead of manually verifying the security code.
 * The scheme is as follows:\n
 * Each peer generates a 32-byte random key called an fprMacKey.
 * Then encrypts it with the public RSA key of the remote peer and sends it to him.
 * The peer decrypts int with their private RSA key. Then each side generates
 * a SHA256 HMAC of their own media encryption fingerprint, keyed with the peer's
 * fprMacKey. Then sends it to the remote. The remote does the same
 * calculation for its own media crypto fingerprint, and compares what it received.
 * They must match.
 */

class IRtcCrypto
{
public:
    /** @brief Generates a HMAC of the fingerprint hash+a fixed string, keyed with
     * the peer's fprMacKey
     */
    virtual void mac(const std::string& data, const SdpKey& key, SdpKey& output) = 0;

    /** @brief Decrypt the SdpKey with the peer's public key and our own private key. */
    virtual void decryptKeyFrom(karere::Id peer, const SdpKey& data, SdpKey& output) = 0;

    /**
     * @brief
     * Encrypt an SdpKey with the peer's public key and our own private key.
     * The key of that peer must have been pre-loaded.
     **/
    virtual void encryptKeyTo(karere::Id peer, const SdpKey& data, SdpKey& output) = 0;

    /** @brief Fetches the specified peer's public CU25519 key */
    virtual promise::Promise<void> waitForPeerKeys(karere::Id peer) = 0;

    /** @brief
     * Used to anonymize the user in submitting call statistics.
     * @obsolete We stop using anonymized ids since chatd already knows the actual user is.
    */
    virtual karere::Id anonymizeId(karere::Id userid) = 0;

    /** @brief Generic random string function */
    virtual void random(char* buf, size_t len) = 0;

    virtual ~IRtcCrypto(){}
};
}

#endif // ICRYPTOFUNCTIONS_H
