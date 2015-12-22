#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include <string>
#include <promise.h>

namespace rtcModule
{

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

class ICryptoFunctions
{
public:
    /** @brief Generates a HMAC of the fingerprint hash+a fixed string, keyed with
     * the peer's fprMacKey
     */
    virtual std::string generateMac(const std::string& data, const std::string& key) = 0;
    /** @brief
     * Decrypt a message encrypted with our own public key, using our private key
    */
    virtual std::string decryptMessage(const std::string& msg) = 0;
    /**
     * @brief
     * Encrypt a message with the peer's public key. The key of that JID myst have
     * been pre-loaded using preloadCryptoForJid()
     **/
    virtual std::string encryptMessageForJid(const std::string& msg, const std::string& jid) = 0;
    /** @brief
     * Fetches the specified JID's public key for use with encryptMessageForJid()
    */
    virtual promise::Promise<void> preloadCryptoForJid(const std::string& jid) = 0;
    /** @brief
     * Used to anonymize the user in submitting call statistics
    */
    virtual std::string scrambleJid(const std::string& jid) = 0;
    /** @brief Generic random string function */
    virtual std::string generateRandomString(size_t size) = 0;
    /** @brief Uses generateRandomString() to generate the 32-byte fprMacKey */
    virtual std::string generateFprMacKey() = 0;
    virtual ~ICryptoFunctions(){}
};
}

#endif // ICRYPTOFUNCTIONS_H
