#include "MegaCryptoFunctions.h"
#include "AutoHandle.h"
#include <mega/base64.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "sdkApi.h"
#include "ITypesImpl.h"

using namespace mega;
namespace rtcModule
{

//the fprmac key is 32 bytes, padded is 33, base64 encoded is x 4/3 = 44
enum {kFprMacKeyLen = 44};

template <class T, class D=T>
static void delarray(T* str)
{  delete[] (D*)str;  }


typedef MyAutoHandle<byte*, void(*)(byte*), delarray<byte>, nullptr> Buffer;
typedef MyAutoHandle<const char*, void(*)(const char*), delarray<const char, char>, nullptr> MegaCStr;

MegaCryptoFuncs::MegaCryptoFuncs(MyMegaApi& megaApi)
:mMega(megaApi)
{
    const char* privk = mMega.userData->getPrivateKey();
    size_t privkLen;
    if (!privk || !(privkLen = strlen(privk)))
        throw std::runtime_error("MegaCryptoFunctions ctor: No private key available");
    printf("Private key: %s\n", privk);
    Buffer binprivk(new byte[privkLen+4]);
    int binlen = mega::Base64::atob(privk, binprivk, privkLen);
    printf("binlen = %d, first 2 bytes: %x %x\n", binlen, binprivk.handle()[0], binprivk.handle()[1]);
    int ret = mPrivKey.setkey(mega::AsymmCipher::PRIVKEY, binprivk, binlen);
    if (!ret)
        throw std::runtime_error("MEgaCryptoFunctions ctor: Error setting private key");
}

IString* MegaCryptoFuncs::generateMac(const CString& data, const CString& key)
{
    Buffer binkey(new byte[key.size()+4]);
    int binKeyLen = mega::Base64::atob(key.c_str(), binkey, key.size());
    // You may use other hash engines. e.g EVP_md5(), EVP_sha224, EVP_sha512, etc
    unsigned char binmac[EVP_MAX_MD_SIZE];
    unsigned int bmlen = 0;
    HMAC(EVP_sha256(), binkey, binKeyLen, (unsigned char*)data.c_str(), data.size(),
         binmac, &bmlen);
    auto mac = new IString_buffer<>(new char[45]);
    size_t maclen = Base64::btoa(binmac, bmlen, mac->bufWritePtr());
    assert(maclen == 43);
    mac->bufWritePtr()[43] = '=';
    mac->setStrSize(44);
    return mac;
}

IString* MegaCryptoFuncs::decryptMessage(const CString& b64msg)
{
    Buffer binmsg(new byte[b64msg.size()]);
    int binlen = Base64::atob(b64msg.c_str(), binmsg, b64msg.size());
    auto ret = new IString_buffer<>(new char[1024]); //accommondate 8192 bit RSA keylen
    mPrivKey.decrypt(binmsg, binlen, (byte*)ret->bufWritePtr(), 1024);
    ret->setStrSize(kFprMacKeyLen);
    printf("=============== Peer fprMacKey: %s\n", ret->c_str());
    return ret;
}

IString* MegaCryptoFuncs::encryptMessageForJid(const CString& msg, const CString& bareJid)
{
    if (!bareJid)
        return nullptr;
    if (msg.size() != kFprMacKeyLen)
    {
        fprintf(stderr, "encryptMessageForJid: Message must be exactly 44 bytes long, but is %lu bytes", (unsigned long)msg.size());
        return nullptr;
    }
    printf("=============== FprMacKey: %s\n", msg.c_str());
    auto it = mKeysLoaded.find(string(bareJid.c_str(), bareJid.size()));
    if (it == mKeysLoaded.end())
        return nullptr;
    byte buf[1024]; //8196 max RSA key len
    int binlen = it->second.encrypt((byte*)msg.c_str(), msg.size(), buf, 1024);
    if (!binlen)
    {
        fprintf(stderr, "encryptMessageForJid: encrypt() returned 0\n");
        return nullptr;
    }
    auto ret = new IString_buffer<>(new char[binlen*4/3+4]);
    int b64size = mega::Base64::btoa(buf, binlen, ret->bufWritePtr());
    ret->setStrSize(b64size);
    return ret;
}

void MegaCryptoFuncs::preloadCryptoForJid(const CString& bareJid, void* userp,
    void(*cb)(void*, const CString&))
{
    const char* pos = strchr(bareJid.c_str(), '@');
    if (!pos)
        return cb(userp, "Jid does not contain an '@'");
    shared_ptr<string> spBareJid(new string(bareJid.c_str(), bareJid.size()));
    if (mKeysLoaded.find(*spBareJid) != mKeysLoaded.end())
        return cb(userp, nullptr);
    string jiduser(bareJid.c_str(), pos-bareJid.c_str());
    MegaCStr handle((char*)(MegaApi::base32ToBase64(jiduser.c_str())));

    mMega.call(&MegaApi::getUserData, handle.handle())
    .then([this, spBareJid, userp, cb](ReqResult result)
    {
        auto& key = mKeysLoaded[*spBareJid];
        size_t keylen = strlen(result->getPassword());
        if (keylen < 1)
            return promise::reject<int>(promise::Error("Public key returned by API is empty", -1, ERRTYPE_MEGASDK));

        Buffer binkey(new byte[keylen+4]);
        int binlen = mega::Base64::atob(result->getPassword(), binkey, keylen);
        int ret = key.setkey(AsymmCipher::PUBKEY, binkey, binlen);
        if (!ret)
            return promise::reject<int>(promise::Error("Error parsing public key", -1, ERRTYPE_MEGASDK));
        cb(userp, nullptr);
        return promise::Promise<int>(0);
    })
    .fail([userp, cb](const promise::Error& err)
    {
        cb(userp, err.msg());
        return 0;
    });
}

IString* MegaCryptoFuncs::scrambleJid(const CString& jid)
{
    return new IString_string(string(jid.c_str(), jid.size())); //TODO: Implement
}

IString* MegaCryptoFuncs::generateFprMacKey()
{
    unsigned char buf[32];
    PrnGen::genblock(buf, 32);
    std::string b64(44, 0);
    int b64len = mega::Base64::btoa(buf, 32, (char*)b64.data());
    assert(b64len == 43);
    b64[43]='=';
    return new IString_string(b64);
}

IString* MegaCryptoFuncs::generateRandomString(size_t size)
{
    int binsize = size*3/4+4;
    Buffer buf(new byte[binsize]);
    PrnGen::genblock(buf, binsize);
    std::string b64(size+16, 0); //16 is just a buffer overflow precaution
    int b64len = Base64::btoa(buf, binsize, (char*)b64.data());
    assert((size_t)b64len >= size);
    assert((size_t)b64len < size+16);

    b64.resize(size);
    return new IString_string(b64);
}
}
