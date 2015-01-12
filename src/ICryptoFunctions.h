#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include "ITypes.h"

/** Fingerprint verification scheme description
 * The fingerprint verification resembles the ZRTP verification, but uses
 * the Mega key infrastructure instead of manually verifying the security code.
 * The scheme is as follows:
 * Each peer generates a 32-byte random key called an fprMacKey.
 * Then encrypts it with the public RSA key of the remote peer and sends it to him.
 * The peer decrypts int with their private RSA key. Then each side generates
 * a SHA256 HMAC of their own media encryption fingerprint, keyed with the peer's
 * fprMacKey. Then sends it to the remote. The remote does the same
 * calculation for its own media crypto fingerprint, and compares what it received.
 * They must match.
 */

namespace rtcModule
{
class ICryptoFunctions: public IDestroy
{
public:
    /** @brief Generates a HMAC of the fingerprint hash+a fixed string, keyed with
     * the peer's fprMacKey
     */
    virtual IString* generateMac(const CString& data, const CString& key) = 0;
    /** @brief
     * Decrypt a message encrypted with our own public key, using our private key
    */
    virtual IString* decryptMessage(const CString& msg) = 0;
    /**
     * @brief
     * Encrypt a message with the peer's public key. The key of that JID myst have
     * been pre-loaded using preloadCryptoForJid()
     **/
    virtual IString* encryptMessageForJid(const CString& msg, const CString& jid) = 0;
    /** @brief
     * Fetches the specified JID's public key for use with encryptMessageForJid()
    */
    virtual void preloadCryptoForJid(const CString& jid, void* userp,
        void(*cb)(void* userp, const CString& errMsg)) = 0;
    /** @brief
     * Used to anonymize the user in submitting call statistics
    */
    virtual IString* scrambleJid(const CString& jid) = 0;
    /** @brief Generic random string function */
    virtual IString* generateRandomString(size_t size) = 0;
    /** @brief Uses generateRandomString() to generate the 32-byte fprMacKey */
    virtual IString* generateFprMacKey() = 0;
protected:
    /** @brief
     * Non-pubnlic dtor. Use destroy() to delete the object, which giarantees
     * that the memory will be freed by the correct memory manager
     */
    virtual ~ICryptoFunctions(){}
};
}

#endif // ICRYPTOFUNCTIONS_H
