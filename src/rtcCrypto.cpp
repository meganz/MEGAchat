#include "karereCommon.h" //for std::to_string on android, needed by promise.h
#include "rtcCrypto.h"
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <chatClient.h>
#include <userAttrCache.h>
#include <strongvelope/strongvelope.h>
#include <rtcModule/webrtc.h>
#include <sodium.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <mega.h>
using namespace mega;
using namespace karere;
using namespace CryptoPP;
namespace rtcModule
{
RtcCryptoMeetings::RtcCryptoMeetings(karere::Client& client)
:mClient(client)
{

}

void RtcCryptoMeetings::computeSymmetricKey(const Id &peer, strongvelope::SendKey &output)
{
    auto pms = mClient.userAttrCache().getAttr(peer, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY);
    if (!pms.done())
        throw std::runtime_error("RtcCrypto::computeSymmetricKey: Key not readily available in cache");
    if (pms.failed())
        throw std::runtime_error("RtcCrypto:computeSymmetricKey: Error getting key for user "+ peer.toString()+" :"+pms.error().msg());
    Buffer* pubKey = pms.value();
    if (pubKey->empty())
        throw std::runtime_error("RtcCrypto:computeSymmetricKey: Empty Cu25519 chat key for user "+peer.toString());
    strongvelope::Key<crypto_scalarmult_BYTES> sharedSecret;
    auto ignore = crypto_scalarmult(sharedSecret.ubuf(), (const unsigned char*)mClient.mMyPrivCu25519, pubKey->ubuf());
    (void)ignore;
    strongvelope::deriveSharedKey(sharedSecret, output, "webrtc pairwise key\x01");
}

void RtcCryptoMeetings::decryptKeyFrom(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output)
{
    strongvelope::SendKey aesKey;
    computeSymmetricKey(peer, aesKey);
    assert(aesKey.dataSize() == 16);
    ECB_Mode<AES>::Decryption aesdecryption(aesKey.ubuf(), aesKey.dataSize());
    aesdecryption.ProcessData(output.ubuf(), data.ubuf(), data.dataSize());
}

void RtcCryptoMeetings::encryptKeyTo(const karere::Id &peer, const strongvelope::SendKey &data, strongvelope::SendKey &output)
{
    strongvelope::SendKey aesKey;
    computeSymmetricKey(peer, aesKey);
    assert(aesKey.dataSize() == 16);
    ECB_Mode<AES>::Encryption aesencryption(aesKey.ubuf(), aesKey.dataSize());
    aesencryption.ProcessData(output.ubuf(), data.ubuf(), data.dataSize());

}

std::shared_ptr<strongvelope::SendKey> RtcCryptoMeetings::generateSendKey()
{
    std::shared_ptr<strongvelope::SendKey> key;
    key.reset(new strongvelope::SendKey);
    key->setDataSize(AES::BLOCKSIZE);
    randombytes_buf(key->ubuf(), AES::BLOCKSIZE);
    return key;
}

::promise::Promise<Buffer *> RtcCryptoMeetings::getCU25519PublicKey(const Id &peer)
{
    return mClient.userAttrCache().getAttr(peer, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY);
}

void RtcCryptoMeetings::xorWithCallKey(const strongvelope::SendKey &callKey, strongvelope::SendKey &sendKey)
{
    SymmCipher::xorblock(static_cast<const ::byte *> (callKey.ubuf()), static_cast<::byte *>(sendKey.ubuf()));
}

std::string RtcCryptoMeetings::keyToStr(const strongvelope::SendKey& key)
{
    return std::string(key.buf(), key.dataSize());
}

void RtcCryptoMeetings::strToKey(const std::string& keystr, strongvelope::SendKey &res)
{
    res.setDataSize(keystr.size());
    memcpy(res.ubuf(), keystr.data(), keystr.size());
}
}
