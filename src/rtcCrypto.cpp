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
#include "cryptofunctions.h"
#include <cryptopp/hkdf.h> // required for key derivation

using namespace mega;
using namespace karere;
using namespace CryptoPP;
namespace rtcModule
{
RtcCryptoMeetings::RtcCryptoMeetings(karere::Client& client)
:mClient(client)
{

}

void RtcCryptoMeetings::computeSymmetricKey(Id peer, strongvelope::SendKey &output)
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

void RtcCryptoMeetings::xorWithCallKey(const byte* callKey, byte* key)
{
    SymmCipher::xorblock(callKey, key);
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

std::pair<strongvelope::EcKey, strongvelope::EcKey>
RtcCryptoMeetings::getEd25519Keypair()
{
    std::pair<strongvelope::EcKey, strongvelope::EcKey> keypair;
    keypair.first.assign(mClient.mMyPrivEd25519, 32);
    getPubKeyFromPrivKey(keypair.first, strongvelope::kKeyTypeEd25519, keypair.second);
    return keypair;
}

promise::Promise<bool>
RtcCryptoMeetings::verifyKeySignature(const std::string& msg, const std::string& recvsignature, const karere::Id& chatid, const karere::Id& peer)
{
    ChatRoomList::iterator it = mClient.chats->find(chatid);
    if (it != mClient.chats->end())
    {
       const ChatRoom* chatroom = it->second;
       return mClient.userAttrCache().getAttr(peer, ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY, chatroom->chat().getPublicHandle())
       .then([ recvsignature, msg](Buffer* key) -> bool
       {
           std::string signatureBin =  mega::Base64::atob(recvsignature);
           std::string pubUserED25519(key->buf(), key->dataSize());
           int res = crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(signatureBin.data()),
                                               reinterpret_cast<const unsigned char*>(msg.data()),
                                               msg.size(),
                                               reinterpret_cast<const unsigned char*>(pubUserED25519.data()));

           return (res == 0); // if crypto_sign_verify_detached returns 0 signature has been verified
       })
       .fail([](const ::promise::Error& err)
       {
           return ::promise::Error(err);
       });
    }

    return promise::Promise<bool>(false);
}

bool RtcCryptoMeetings::deriveEphemeralKey(std::string& peerEphemeralPubkey, const unsigned char* privEphemeral, strongvelope::EcKey& output,
                                           const std::vector<std::string>& peerIvs, const std::vector<std::string>& myIvs)
{
    if (peerIvs.size() < 2 || myIvs.size() < 2)
    {
        return false;
    }

    strongvelope::Key<crypto_scalarmult_BYTES> sharedSecret;
    std::string pubkeyBin =  mega::Base64::atob(peerEphemeralPubkey);
    if (crypto_scalarmult(sharedSecret.ubuf(), privEphemeral, reinterpret_cast<const unsigned char*>(pubkeyBin.data())))
    {
        return false;
    }

    // generate salt with two of 8-Byte stream encryption iv of the peer and two of our 8-Byte stream encryption iv sorted alphabetically
    std::string salt;
    vector<string> v { peerIvs[1], peerIvs[2], myIvs[1], myIvs[2] };
    sort(v.begin(), v.end());
    std::for_each(v.begin(), v.end(), [&salt](std::string &s){ salt += s; });
    std::vector<byte> saltBin = sfu::Command::hexToByteArray(salt);

    HKDF<CryptoPP::SHA256> hkdf;
    hkdf.DeriveKey(output.ubuf(), output.bufSize(), sharedSecret.ubuf(), sharedSecret.bufSize(), saltBin.data(), saltBin.size(), nullptr, 0);
    return true;
}

X25519KeyPair* RtcCryptoMeetings::genX25519KeyPair()
{
    X25519KeyPair* keyPair = new X25519KeyPair();
    crypto_box_keypair(keyPair->pubKey, keyPair->privKey);
    return keyPair;
}
}
