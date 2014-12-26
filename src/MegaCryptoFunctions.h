#ifndef MEGACRYPTOFUNCTIONS_H
#define MEGACRYPTOFUNCTIONS_H
#include <map>
#include "ICryptoFunctions.h"
#include <mega/crypto/cryptopp.h>
#include "sdkApi.h"

namespace rtcModule
{
class MegaCryptoFuncs: public rtcModule::ICryptoFunctions
{
protected:
    struct MegaUserInfo
    {
        std::string jid;
        mega::AsymmCipher pubk;
    };

    std::string mOwnJid;
    //cache that maps user emails to jid&public key
    std::map<std::string, MegaUserInfo> mKeysLoaded;
    mega::AsymmCipher mPrivKey;
public:
    MegaCryptoFuncs(const std::string& ownJid, const char* privk, size_t privkLen);
    //ICryptoFunctions interface implementation
    virtual IString* generateMac(const char* data, const char* key);
    virtual IString* decryptMessage(const char* msg);
    virtual IString* encryptMessageForJid(const char* msg, const char* bareJid);
    virtual void preloadCryptoForJid(const char* jid, void* userp,
        void(*cb)(void* userp, const char* errMsg));
    virtual IString* scrambleJid(const char* jid);
    virtual IString* generateFprMacKey();
    virtual IString* generateRandomString(size_t size);
    virtual ~MegaCryptoFuncs() {}
};

}
#endif // MEGACRYPTOFUNCTIONS_H
