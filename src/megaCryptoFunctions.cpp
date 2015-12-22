#include "karereCommon.h" //for std::to_string on android, needed by promise.h
#include "megaCryptoFunctions.h"
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "sdkApi.h"
#include "buffer.h"
#include "base64.h"

using namespace mega;
using namespace std;

namespace rtcModule
{

//the fprmac key is 32 bytes, base64 encoded without padding is 43
enum {kFprMacKeyLen = 43};


MegaCryptoFuncs::MegaCryptoFuncs(MyMegaApi& megaApi)
:mMega(megaApi)
{
    const char* privk = mMega.userData->getPrivateKey();
    size_t privkLen;
    if (!privk || !(privkLen = strlen(privk)))
        throw std::runtime_error("MegaCryptoFunctions ctor: No private key available");
    Buffer binprivk(privkLen);
    auto binlen = base64urldecode(privk, privkLen, binprivk.buf(), binprivk.bufSize());
    binprivk.setDataSize(binlen);
    int ret = mPrivKey.setkey(AsymmCipher::PRIVKEY, (const byte*)binprivk.buf(), binprivk.dataSize());
    if (!ret)
        throw std::runtime_error("MegaCryptoFunctions ctor: Error setting private key");
}

std::string MegaCryptoFuncs::generateMac(const std::string& data, const std::string& key)
{
    Buffer binkey(key.size()+4);
    int binKeyLen = base64urldecode(key.c_str(), key.size(), binkey.buf(), binkey.bufSize());
    binkey.setDataSize(binKeyLen);
    // You may use other hash engines. e.g EVP_md5(), EVP_sha224, EVP_sha512, etc
    unsigned char binmac[EVP_MAX_MD_SIZE];
    unsigned int bmlen = 0;
    HMAC(EVP_sha256(), binkey.buf(), binkey.dataSize(), (unsigned char*)data.c_str(),
         data.size(), binmac, &bmlen);
    assert(bmlen = 32);
    return base64urlencode(binmac, bmlen);
//    assert(maclen == kFprMacLen); //32 bytes to base64 without padding
//   printf("\n===================== generateMac: data='%s', key='%s', mac='%s'\n",
//          data.c_str(), key.c_str(), mac->c_str());
//    return mac;
}

std::string MegaCryptoFuncs::decryptMessage(const std::string& b64msg)
{
    Buffer binmsg(b64msg.size());
    int binlen = base64urldecode(b64msg.c_str(), b64msg.size(), binmsg.buf(), binmsg.bufSize());
    binmsg.setDataSize(binlen);
    char msg[1024]; //accommodate 8192 bit RSA keylen
    mPrivKey.decrypt((const byte*)binmsg.buf(), binmsg.dataSize(), (byte*)msg, sizeof(msg));
    return string(msg, kFprMacKeyLen);
}

std::string MegaCryptoFuncs::encryptMessageForJid(const std::string& msg, const std::string& bareJid)
{
    if (bareJid.empty())
        throw std::runtime_error("encryptMessageForJid: Empty JID provided");
    if (msg.size() != kFprMacKeyLen)
        throw std::runtime_error("encryptMessageForJid: Message must be exactly 43 bytes long");
    auto it = mKeysLoaded.find(bareJid);
    if (it == mKeysLoaded.end())
        throw std::runtime_error("encryptMessageForJid: No key loaded for jid "+bareJid);
    byte buf[1024]; //8196 max RSA key len
    int binlen = it->second.encrypt((byte*)msg.c_str(), msg.size(), buf, 1024);
    if (!binlen)
        throw std::runtime_error("encryptMessageForJid: encrypt() returned 0");
    return base64urlencode(buf, binlen);
}

promise::Promise<void> MegaCryptoFuncs::preloadCryptoForJid(const std::string& bareJid)
{
    auto pos = bareJid.find('@');
    if (pos == std::string::npos)
        return promise::Error("preloadCryptoForJid: Jid does not contain an '@'");
    if (mKeysLoaded.find(bareJid) != mKeysLoaded.end())
        return promise::_Void();
    SdkString handle(MegaApi::base32ToBase64(bareJid.substr(0, pos).c_str()));

    return mMega.call(&MegaApi::getUserData, handle.c_str())
    .then([this, bareJid](ReqResult result) -> promise::Promise<void>
    {
        auto& key = mKeysLoaded[bareJid];
        size_t keylen = strlen(result->getPassword());
        if (keylen < 1)
            return promise::Error("Public key returned by API is empty", -1, ERRTYPE_MEGASDK);

        char binkey[1024];
        int binlen = base64urldecode(result->getPassword(), keylen, binkey, 1024);
        int ret = key.setkey(AsymmCipher::PUBKEY, (byte*)binkey, binlen);
        if (!ret)
            return promise::Error("Error parsing public key", -1, ERRTYPE_MEGASDK);
        return promise::_Void();
    });
}

std::string MegaCryptoFuncs::scrambleJid(const std::string& jid)
{
    return string(jid.c_str(), jid.size()); //TODO: Implement
}

std::string MegaCryptoFuncs::generateFprMacKey()
{
    unsigned char buf[32];
    PrnGen::genblock(buf, 32);
    auto key = base64urlencode(buf, 32);
    assert(key.size() == kFprMacKeyLen);
    return key;
}

std::string MegaCryptoFuncs::generateRandomString(size_t size)
{
    int binsize = size*3/4+4;
    Buffer buf(binsize);
    PrnGen::genblock((byte*)buf.buf(), buf.bufSize());
    buf.setDataSize(binsize);
    auto str = base64urlencode(buf.buf(), buf.dataSize());
    assert(str.size() >= size);
    str.resize(size);
    return str;
}
}
