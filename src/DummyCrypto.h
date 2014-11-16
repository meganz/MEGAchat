#ifndef DUMMYCRYPTO_H
#define DUMMYCRYPTO_H
#include "ICryptoFunctions.h"
#include <string>
#include <set>

class DummyCrypto: public karere::rtcModule::ICryptoFunctions
{
protected:
    std::string mOwnJid;
    std::set<std::string> mKeysLoaded;
public:
    class StringImpl: public IString
    {
    public:
        std::string mString;
        StringImpl(std::string&& str): mString(str){}
        virtual ~StringImpl(){}
        virtual const char* c_str() const {return mString.c_str();}
        virtual size_t size() const {return mString.size();}
        virtual bool empty() const {return mString.empty();}
    };
    DummyCrypto(const std::string& ownJid):mOwnJid(ownJid){}
    //ICryptoFunctions interface implementation
    virtual IString* generateMac(const char* data, const char* key);
    virtual IString* decryptMessage(const char* msg);
    virtual IString* encryptMessageForJid(const char* msg, const char* bareJid);
    virtual void preloadCryptoForJid(const char* jid, void* userp,
        void(*cb)(void* userp, const char* errMsg));
    virtual IString* scrambleJid(const char* jid);
    virtual IString* generateFprMacKey();
    virtual IString* generateRandomString(size_t size);
    virtual ~DummyCrypto() {}
};


#endif
