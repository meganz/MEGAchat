#include "karereCommon.h" //for std::to_string on android, needed by promise.h
#include "megaCryptoFunctions.h"
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "sdkApi.h"
#include "buffer.h"
#include "base64.h"
#include <chatClient.h>
#include <db.h>
#include <userAttrCache.h>

using namespace mega;
using namespace std;
using namespace karere;

namespace rtcModule
{

//the fprmac key is 32 bytes, base64 encoded without padding is 43
enum {kFprMacKeyLen = 43};


MegaCryptoFuncs::MegaCryptoFuncs(Client& client)
:mClient(client)
{
    if (!mClient.mMyPrivRsaLen)
        throw std::runtime_error("MegaCryptoFunctions ctor: No private key available");
    int ret = mPrivKey.setkey(AsymmCipher::PRIVKEY, (const byte*)mClient.mMyPrivRsa, mClient.mMyPrivRsaLen);
    if (!ret)
        throw std::runtime_error("MegaCryptoFunctions ctor: Error setting private key");
    loadCache();
}

void MegaCryptoFuncs::loadCache()
{
    SqliteStmt stmt(mClient.db, "select userid, data from userattrs where type = ?");
    stmt << karere::USER_ATTR_RSA_PUBKEY;
    while(stmt.step())
    {
        uint64_t userid = stmt.uint64Col(0);
        Buffer buf;
        stmt.blobCol(1, buf);
        bool ok = loadKey(userid, buf.buf(), buf.dataSize());
        if (!ok)
        {
            KR_LOG_WARNING("Error loading pubkic RSA key from cache for user %d, deleting from db", userid);
            sqliteQuery(mClient.db, "delete from usetattrs where userid=? and type=?",
                        userid, USER_ATTR_RSA_PUBKEY);
        }
    }
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
uint64_t useridFromJid(const std::string& jid)
{
    auto end = jid.find('@');
    if (end != 13)
    {
        KR_LOG_WARNING("useridFromJid: Invalid Mega JID '%s'", jid.c_str());
        return ::mega::UNDEF;
    }
    uint64_t userid;
#ifndef NDEBUG
    auto len =
#endif
    ::mega::Base32::atob(jid.c_str(), (byte*)&userid, end);
    assert(len == 8);
    return userid;
}
std::string MegaCryptoFuncs::encryptMessageForJid(const std::string& msg, const std::string& bareJid)
{
    if (bareJid.empty())
        throw std::runtime_error("encryptMessageForJid: Empty JID provided");
    if (msg.size() != kFprMacKeyLen)
        throw std::runtime_error("encryptMessageForJid: Message must be exactly 43 bytes long");
    auto userid = useridFromJid(bareJid);
    auto pms = mClient.userAttrCache().getAttr(userid, USER_ATTR_RSA_PUBKEY);
    if (pms.done() != promise::kSucceeded)
        throw std::runtime_error("encryptMessageForJid: No key loaded for jid "+bareJid);
    ::mega::AsymmCipher key;
    int ret = key.setkey(::mega::AsymmCipher::PUBKEY, pms.value()->ubuf(), pms.value()->dataSize());
    if (!ret)
        throw std::runtime_error("encryptMessageForJid: Can't parse key for jid "+bareJid);

    byte buf[1024]; //8196 max RSA key len
    int binlen = key.encrypt((byte*)msg.c_str(), msg.size(), buf, 1024);
    if (!binlen)
        throw std::runtime_error("encryptMessageForJid: encrypt() returned 0");
    return base64urlencode(buf, binlen);
}

bool MegaCryptoFuncs::loadKey(uint64_t userid, const char* keydata, size_t keylen)
{
    auto& key = mKeysLoaded[userid];
    int ret = key.setkey(AsymmCipher::PUBKEY, (byte*)keydata, keylen);
    if (!ret)
    {
        mKeysLoaded.erase(userid);
        return false;
    }
    return true;
}

promise::Promise<void> MegaCryptoFuncs::preloadCryptoForJid(const std::string& bareJid)
{
    auto userid = useridFromJid(bareJid);
    if (userid == ::mega::UNDEF)
        return promise::Error("preloadCryptoForJid: Invalid Mega jid "+bareJid);
    return mClient.userAttrCache().getAttr(userid, USER_ATTR_RSA_PUBKEY)
    .then([this, userid](Buffer* binkey) -> promise::Promise<void>
    {
        if (!binkey || binkey->empty())
            return promise::Error("Public key returned by API is empty", -1, ERRTYPE_MEGASDK);

        return promise::_Void();
    })
    .fail([this, userid](const promise::Error& err)
    {
        KR_LOG_ERROR("Error fetching RSA pubkey for user %s: %s", base64urlencode(&userid, sizeof(userid)).c_str(), err.what());
        return err;
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
