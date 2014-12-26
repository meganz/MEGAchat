#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include "IString.h"

namespace rtcModule
{
class ICryptoFunctions
{
public:
    virtual IString* generateMac(const InputString& data, const InputString& key) = 0;
    virtual IString* decryptMessage(const InputString& msg) = 0;
    virtual IString* encryptMessageForJid(const InputString& msg, const InputString& jid) = 0;
    virtual void preloadCryptoForJid(const InputString& jid, void* userp,
        void(*cb)(void* userp, const InputString& errMsg)) = 0;
    virtual IString* scrambleJid(const InputString& jid) = 0;
    virtual IString* generateRandomString(size_t size) = 0;
    virtual IString* generateFprMacKey() = 0;
    virtual ~ICryptoFunctions() {}
};
}

#endif // ICRYPTOFUNCTIONS_H
