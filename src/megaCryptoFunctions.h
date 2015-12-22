#ifndef MEGACRYPTOFUNCTIONS_H
#define MEGACRYPTOFUNCTIONS_H
#include <map>
#include "ICryptoFunctions.h"
//#define USE_CRYPTOPP 1
#include <mega.h>
//#include <mega/crypto/cryptopp.h>

class MyMegaApi;
namespace rtcModule
{
class MegaCryptoFuncs: public rtcModule::ICryptoFunctions
{
protected:
    //cache that maps user emails to jid&public key
    std::map<std::string, mega::AsymmCipher> mKeysLoaded;
    mega::AsymmCipher mPrivKey;
    MyMegaApi& mMega;
public:
    MegaCryptoFuncs(MyMegaApi& megaApi);
    //ICryptoFunctions interface implementation
    virtual std::string generateMac(const std::string& data, const std::string& key);
    virtual std::string decryptMessage(const std::string& msg);
    virtual std::string encryptMessageForJid(const std::string& msg, const std::string& bareJid);
    virtual promise::Promise<void> preloadCryptoForJid(const std::string& jid);
    virtual std::string scrambleJid(const std::string& jid);
    virtual std::string generateFprMacKey();
    virtual std::string generateRandomString(size_t size);
};

}
#endif // MEGACRYPTOFUNCTIONS_H
