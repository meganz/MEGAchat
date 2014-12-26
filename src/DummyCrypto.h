#ifndef DUMMYCRYPTO_H
#define DUMMYCRYPTO_H
#include "ICryptoFunctions.h"
#include <string>
#include <set>
#include "ICryptoString.h"

namespace rtcModule {

class DummyCrypto: public rtcModule::ICryptoFunctions
{
protected:
    std::string mOwnJid;
    std::set<std::string> mKeysLoaded;
public:
    DummyCrypto(const std::string& ownJid):mOwnJid(ownJid){}
    //ICryptoFunctions interface implementation
    virtual IString* generateMac(const InputString& data, const InputString& key);
    virtual IString* decryptMessage(const InputString& msg);
    virtual IString* encryptMessageForJid(const InputString& msg, const InputString& bareJid);
    virtual void preloadCryptoForJid(const InputString& jid, void* userp,
        void(*cb)(void* userp, const InputString& errMsg));
    virtual IString* scrambleJid(const InputString& jid);
    virtual IString* generateFprMacKey();
    virtual IString* generateRandomString(size_t size);
    virtual ~DummyCrypto() {}
};

}


#endif
