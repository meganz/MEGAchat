#ifndef DUMMYCRYPTO_H
#define DUMMYCRYPTO_H
#include "ICryptoFunctions.h"
#include <string>
#include <set>
#include "ITypes.h"

namespace rtcModule {

class DummyCrypto: public rtcModule::ICryptoFunctions
{
protected:
    std::string mOwnJid;
    std::set<std::string> mKeysLoaded;
public:
    DummyCrypto(const std::string& ownJid):mOwnJid(ownJid){}
    //ICryptoFunctions interface implementation
    virtual IString* generateMac(const CString& data, const CString& key);
    virtual IString* decryptMessage(const CString& msg);
    virtual IString* encryptMessageForJid(const CString& msg, const CString& bareJid);
    virtual void preloadCryptoForJid(const CString& jid, void* userp,
        void(*cb)(void* userp, const CString& errMsg));
    virtual IString* scrambleJid(const CString& jid);
    virtual IString* generateFprMacKey();
    virtual IString* generateRandomString(size_t size);
    virtual ~DummyCrypto() {}
};

std::string makeRandomString(int len);

}


#endif
