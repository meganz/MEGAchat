#ifndef ICRYPTOFUNCTIONS_H
#define ICRYPTOFUNCTIONS_H
#include <stddef.h> //size_t

namespace karere
{
namespace rtcModule
{
class ICryptoFunctions
{
public:
    class IString
    {
    public:
        virtual ~IString(){}
        virtual const char* c_str() const = 0;
        virtual bool empty() const = 0;
    };

    virtual IString* generateMac(const char* data, const char* key) = 0;
    virtual IString* decryptMessage(const char* msg) = 0;
    virtual IString* encryptMessageForJid(const char* msg, const char* jid) = 0;
    virtual void preloadCryptoForJid(const char* jid, void* userp,
        void(*cb)(void* userp, const char* errMsg)) = 0;
    virtual IString* scrambleJid(const char* jid) = 0;
    virtual IString* generateRandomString(size_t size) = 0;
    virtual IString* generateFprMacKey() = 0;
    virtual ~ICryptoFunctions() {}
};
}
}
#endif // ICRYPTOFUNCTIONS_H
