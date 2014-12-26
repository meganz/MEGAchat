#include "MegaCryptoFunctions.h"
#include "AutoHandle.h"
#include <mega/base64.h>
#include <openssl/hmac.h>

namespace rtcModule
{
enum {kMacKeyBinLen = 16};
typedef MyAutoHandle<char*, void(void*), free, nullptr> Buffer;

MegaCryptoFuncs::MegaCryptoFuncs(const std::string& ownJid, const char* privk, size_t privkLen)
:mOwnJid(ownJid)
{
    Buffer binprivk(malloc(privkLen+4));
    int binlen = mega::Base64::atob(privk, binprivk, privkLen);
    mPrivKey.setkey(symmCipher::PRIVKEY, binprivk, binlen);
}

virtual IString* MegaCryptoFuncs::generateMac(const char* data, const char* key)
{
    size_t datalen = strlen(data);
    size_t keylen = strlen(key);
    MyAutoHandle<char*, void(void*), free, nullptr> binkey(malloc(keylen+4));
    int binKeyLen = Base64::atob(key, binkey, keylen);
    // You may use other hash engines. e.g EVP_md5(), EVP_sha224, EVP_sha512, etc
    char binmac[EVP_MAX_MD_SIZE];
    int bmlen = 0;
    HMAC(EVP_sha256(), binkey, binKeyLen, (unsigned char*)data, datalen, binmac, &bmlen);
    auto mac = new IString_buffer(malloc(bmlen*2));
    size_t maclen = Base64::btoa(binmac, bmlen, mac->bufWritePtr());
    mac->setStrSize(maclen);
    return mac;
}

virtual IString* decryptMessage(const char* msg, size_t len=string::npos)
{
    if (len == (size_t)-1)
        len = strlen(msg);
    auto ret = new IString_buffer(malloc(len+4));
    mPrivKey.decrypt(msg, len, ret->bufWritePtr(), len+4);
    ret->setStrSize(/*TODO: How do we know decrypted msg size?*/);
    return ret;
}

virtual IString* encryptMessageForJid(const char* msg, const char* bareJid)
{
    if (!bareJid)
        return nullptr;
    auto it = mKeysLoaded.find(bareJid);
    if (it == mKeysLoaded.end())
        return nullptr;

    it->pubk.encrypt(msg, msglen, )
}

virtual void preloadCryptoForJid(const char* jid, void* userp,
    void(*cb)(void* userp, const char* errMsg));
virtual IString* scrambleJid(const char* jid);
virtual IString* generateFprMacKey();
virtual IString* generateRandomString(size_t size);
