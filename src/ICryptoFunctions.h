#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t
#include "ITypes.h"

namespace rtcModule
{
class ICryptoFunctions: public IDestroy
{
public:
    virtual IString* generateMac(const CString& data, const CString& key) = 0;
    virtual IString* decryptMessage(const CString& msg) = 0;
    virtual IString* encryptMessageForJid(const CString& msg, const CString& jid) = 0;
    virtual void preloadCryptoForJid(const CString& jid, void* userp,
        void(*cb)(void* userp, const CString& errMsg)) = 0;
    virtual IString* scrambleJid(const CString& jid) = 0;
    virtual IString* generateRandomString(size_t size) = 0;
    virtual IString* generateFprMacKey() = 0;
protected:
    virtual ~ICryptoFunctions(){}
};
}

#endif // ICRYPTOFUNCTIONS_H
