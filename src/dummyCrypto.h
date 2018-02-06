#ifndef DUMMYCRYPTO_H
#define DUMMYCRYPTO_H
#include "IRtcCrypto.h"
#include <string>
#include <set>

namespace rtcModule {

class DummyCrypto: public rtcModule::IRtcCrypto
{
protected:
    std::string mOwnJid;
    std::set<std::string> mKeysLoaded;
public:
    DummyCrypto(const std::string& ownJid):mOwnJid(ownJid){}
    //ICryptoFunctions interface implementation
    virtual std::string generateMac(const std::string& data, const std::string& key);
    virtual std::string decryptMessage(const std::string& msg);
    virtual std::string encryptMessageForJid(const std::string& msg, const std::string& bareJid);
    promise::Promise<void> preloadCryptoForJid(const std::string& jid);
    virtual std::string scrambleJid(const std::string& jid);
    virtual std::string generateFprMacKey();
    virtual std::string generateRandomString(size_t size);
    virtual ~DummyCrypto() {}
};

std::string makeRandomString(int len);

}


#endif
