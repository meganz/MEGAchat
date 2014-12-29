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
    virtual IString* generateMac(const CString& data, const CString& key);
    virtual IString* decryptMessage(const CString& msg);
    virtual IString* encryptMessageForJid(const CString& msg, const CString& bareJid);
    virtual void preloadCryptoForJid(const CString& jid, void* userp,
        void(*cb)(void* userp, const CString& errMsg));
    virtual IString* scrambleJid(const CString& jid);
    virtual IString* generateFprMacKey();
    virtual IString* generateRandomString(size_t size);
    virtual ~MegaCryptoFuncs() {}
};

}
#endif // MEGACRYPTOFUNCTIONS_H
