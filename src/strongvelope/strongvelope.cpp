/*
 * strongvelope.cpp
 *
 *  Created on: 17/11/2015
 *      Author: admin2
 */

#include <stdint.h>
#define _DEFAULT_SOURCE 1
#ifdef __APPLE__
    #include <libkern/OSByteOrder.h>
    static inline uint64_t be64toh(uint64_t x) { return OSSwapBigToHostInt64(x); }
#elif defined(WIN32)
    #include <stdlib.h>
    static inline uint64_t be64toh(uint64_t x) { return _byteswap_uint64(x); }
#else
    #include <endian.h>
    #ifndef __STDC_FORMAT_MACROS
        #define __STDC_FORMAT_MACROS 1
    #endif
    #include <inttypes.h>
#endif

#include "strongvelope.h"
#include "cryptofunctions.h"
#include <ctime>
#include "sodium.h"
#include "tlvstore.h"
#include <userAttrCache.h>
#include <mega.h>
#include <megaapi.h>
#include <db.h>
#ifndef _MSC_VER
#include <codecvt>   // deprecated
#endif
#include <locale>
#include <karereCommon.h>

namespace strongvelope
{
using namespace karere;
using namespace promise;
using namespace CryptoPP;
using namespace chatd;

const std::string SVCRYPTO_PAIRWISE_KEY = "strongvelope pairwise key\x01";
const std::string SVCRYPTO_SIG = "strongvelopesig";
void deriveNonceSecret(const StaticBuffer& masterNonce, const StaticBuffer &result,
                       Id recipient=Id::null());


const char* tlvTypeToString(uint8_t type)
{
    switch(type)
    {
        case TLV_TYPE_SIGNATURE: return "TLV_SIGNATURE";
        case TLV_TYPE_MESSAGE_TYPE: return "TLV_MESSAGE_TYPE";
        case TLV_TYPE_NONCE: return "TLV_NONCE";
        case TLV_TYPE_RECIPIENT: return "TLV_RECIPIENT";
        case TLV_TYPE_KEYS: return "TLV_KEYS";
        case TLV_TYPE_KEY_IDS: return "TLV_KEY_IDS";
        case TLV_TYPE_PAYLOAD: return "TLV_PAYLOAD";
        case TLV_TYPE_INC_PARTICIPANT: return "TLV_INC_PARTICIPANT";
        case TLV_TYPE_EXC_PARTICIPANT: return "TLV_EXC_PARTICIPANT";
        case TLV_TYPE_INVITOR: return "TLV_INVITOR";
        case TLV_TYPE_OWN_KEY: return "TLV_OWN_KEY";
        case TLV_TYPE_KEYBLOB: return "TLV_TYPE_KEYBLOB";
        default: return "(unknown)";
    }
}

uint32_t getKeyIdLength(uint32_t protocolVersion)
{
    return (protocolVersion == 1) ? 8 : 4;
}

EncryptedMessage::EncryptedMessage(const Message& msg, const StaticBuffer& aKey)
: key(aKey), backRefId(msg.backRefId)
{
    assert(!key.empty());
    randombytes_buf(nonce.buf(), nonce.bufSize());
    Key<32> derivedNonce(32); //deriveNonceSecret uses dataSize() to confirm there is buffer space
    deriveNonceSecret(nonce, derivedNonce);
    derivedNonce.setDataSize(SVCRYPTO_NONCE_SIZE+4); //truncate to nonce size+32bit counter

    *reinterpret_cast<uint32_t*>(derivedNonce.buf()+SVCRYPTO_NONCE_SIZE) = 0; //zero the 32-bit counter
    assert(derivedNonce.dataSize() == AES::BLOCKSIZE);

    size_t brsize = msg.backRefs.size()*8;
    size_t binsize = 10+brsize;
    Buffer buf(binsize+msg.dataSize());
    buf.append<uint64_t>(msg.backRefId)
       .append<uint16_t>(brsize);
    if (brsize)
    {
        buf.append((const char*)(&msg.backRefs[0]), brsize);
    }
    if (!msg.empty())
    {
        buf.append(msg);
    }
    ciphertext = aesCTREncrypt(std::string(buf.buf(), buf.dataSize()),
        key, derivedNonce);
}

/**
 * Decrypts a message symmetrically using AES-128-CTR.
 *
 * @param key Symmetric encryption key.
 * @param outMsg The message object to write the decrypted data to.
 */
void ParsedMessage::symmetricDecrypt(const StaticBuffer& key, Message& outMsg)
{
    if (payload.empty())
    {
        outMsg.clear();
        return;
    }
    Id chatid = mProtoHandler.chatid;
    STRONGVELOPE_LOG_DEBUG("Decrypting msg %s", outMsg.id().toString().c_str());
    Key<32> derivedNonce;
    // deriveNonceSecret() needs at least 32 bytes output buffer
    deriveNonceSecret(nonce, derivedNonce);
    derivedNonce.setDataSize(AES::BLOCKSIZE);
    // For AES CRT mode, we take the first 12 bytes as the nonce,
    // and the remaining 4 bytes as the counter, which is initialized to zero
    *reinterpret_cast<uint32_t*>(derivedNonce.buf()+SVCRYPTO_NONCE_SIZE) = 0;
    std::string cleartext = aesCTRDecrypt(std::string(payload.buf(), payload.dataSize()),
        key, derivedNonce);
    parsePayload(StaticBuffer(cleartext, false), outMsg);
    outMsg.setEncrypted(Message::kNotEncrypted);
}

/**
 * Derive the nonce to use for an encryption for a particular recipient
 * or message payload encryption.
 *
 * The returned nonce is a 32-byte buffer, of which a suitable slice is
 * later used for encryption. It is derived from the message's master nonce.
 *
 * @param masterNonce
 *     Master nonce as transmitted in the message, used as a base to derive
 *     a nonce secret from.
 * @param recipient
 *     Recipient's user handle. If not set, using the string "payload" for
 *     message payload encryption.
 * @param [out] result
 *     A 32-byte buffer where the derived nonce will be written.
 */
void deriveNonceSecret(const StaticBuffer& masterNonce, const StaticBuffer& result,
                       Id recipient)
{
    result.checkDataSize(32);
    Buffer recipientStr;
    if (recipient == Id::null())
        recipientStr.append(std::string("payload"));
    else
        recipientStr.append(recipient.val);

    // Equivalent to first block of HKDF, see RFC 5869.
    hmac_sha256_bytes(recipientStr, masterNonce, result);
}

void ProtocolHandler::signMessage(const StaticBuffer& signedData,
        uint8_t protoVersion, uint8_t msgType, const SendKey& msgKey,
        StaticBuffer& signature)
{
    assert(signature.dataSize() == crypto_sign_BYTES);
// To save space, myPrivEd25519 holds only the 32-bit seed of the priv key,
// without the pubkey part, so we add it here
    Buffer key(myPrivEd25519.dataSize()+myPubEd25519.dataSize());
    key.append(myPrivEd25519).append(myPubEd25519);

    Buffer toSign(msgKey.dataSize()+signedData.dataSize()+SVCRYPTO_SIG.size()+10);
    toSign.append(SVCRYPTO_SIG)
          .append<uint8_t>(protoVersion)
          .append<uint8_t>(msgType)
          .append(msgKey)
          .append(signedData);

    crypto_sign_detached(signature.ubuf(), NULL, toSign.ubuf(),
        toSign.dataSize(), key.ubuf());
}

bool ParsedMessage::verifySignature(const StaticBuffer& pubKey, const SendKey& sendKey)
{
    assert(pubKey.dataSize() == 32);
    if (protocolVersion < 2)
    {
        //legacy
        Buffer messageStr(SVCRYPTO_SIG.size()+signedContent.dataSize());
        messageStr.append(SVCRYPTO_SIG.c_str(), SVCRYPTO_SIG.size())
        .append(signedContent);
        return (crypto_sign_verify_detached(signature.ubuf(), messageStr.ubuf(),
                messageStr.dataSize(), pubKey.ubuf()) == 0);
    }

    assert(sendKey.dataSize() == 16);
    Buffer messageStr(SVCRYPTO_SIG.size()+sendKey.dataSize()+signedContent.dataSize()+2);

    messageStr.append(SVCRYPTO_SIG.c_str(), SVCRYPTO_SIG.size())
    .append<uint8_t>(protocolVersion)
    .append<uint8_t>(type)
    .append(sendKey)
    .append(signedContent);

//    STRONGVELOPE_LOG_DEBUG("signature:\n%s", signature.toString().c_str());
//    STRONGVELOPE_LOG_DEBUG("message:\n%s", messageStr.toString().c_str());
//    STRONGVELOPE_LOG_DEBUG("pubKey:\n%s", pubKey.toString().c_str());
    // if crypto_sign_verify_detached does not return 0, it means Incorrect signature!
    return (crypto_sign_verify_detached(signature.ubuf(), messageStr.ubuf(),
            messageStr.dataSize(), pubKey.ubuf()) == 0);
}

/**
 * Derive the shared confidentiality key.
 *
 * The key is a 32-byte string, half of which is later used for
 * AES-128. It is derived from the Diffie-Hellman shared secret, a x25519
 * public value, using HKDF-SHA256.
 * The info string for the HKDF is \c infoStr. In strongvelope, this is set to
 * the constant "strongvelope pairwise key".
 * @param sharedSecret
 *     Input IKM for the HKDF. In mpENC, this is the x25519 public key
 *     result of the group key agreement.
 * @param output Output buffer for result.
 */
void deriveSharedKey(const StaticBuffer& sharedSecret, SendKey& output, const std::string& padString)
{
    assert(output.dataSize() == AES::BLOCKSIZE);
    // Equivalent to first block of HKDF, see RFC 5869.
    Key<32> sharedSecretKey;
    hmac_sha256_bytes(sharedSecret, StaticBuffer(nullptr, 0), sharedSecretKey); //step 1 - extract
    Key<32> step2;
    hmac_sha256_bytes(StaticBuffer(padString, false), sharedSecretKey, step2); //step 2 - expand
    memcpy(output.buf(), step2.buf(), AES::BLOCKSIZE);
}

ParsedMessage::ParsedMessage(const Message& binaryMessage, ProtocolHandler& protoHandler)
: mProtoHandler(protoHandler)
{
    if(binaryMessage.empty())
    {
        throw std::runtime_error("parsedMessage::parse: Empty binary message");
    }
    protocolVersion = binaryMessage.read<uint8_t>(0);
    if (protocolVersion > SVCRYPTO_PROTOCOL_VERSION)
        throw std::runtime_error("Message protocol version "+std::to_string(protocolVersion)+" is newer than the latest supported by this client. Message dump: "+binaryMessage.toString());
    sender = binaryMessage.userid;

    size_t offset;
    bool isLegacy = (protocolVersion < 2);
    if (isLegacy)
    {
        offset = 1;
        managementInfo = std::unique_ptr<chatd::Message::ManagementInfo>(new chatd::Message::ManagementInfo());
    }
    else
    {
        offset = 2;
        type = binaryMessage.read<uint8_t>(1);
        if (type == chatd::Message::kMsgAlterParticipants || type == chatd::Message::kMsgPrivChange)
        {
            managementInfo.reset(new chatd::Message::ManagementInfo());
        }
        else if (type == chatd::Message::kMsgCallEnd)
        {
            callEndedInfo.reset(new chatd::Message::CallEndedInfo());
        }
    }
    TlvParser tlv(binaryMessage, offset, isLegacy);
    TlvRecord record(binaryMessage);
    std::string recordNames;
    while (tlv.getRecord(record))
    {
        recordNames.append(tlvTypeToString(record.type))+=", ";
        switch (record.type)
        {
            case TLV_TYPE_SIGNATURE:
            {
                signature.assign(record.buf(), record.dataLen);
                auto nextOffset = record.dataOffset+record.dataLen;
                signedContent.assign(binaryMessage.buf()+nextOffset, binaryMessage.dataSize()-nextOffset);
                break;
            }
            case TLV_TYPE_NONCE:
            {
                nonce.assign(record.buf(), record.dataLen);
                break;
            }
            case TLV_TYPE_MESSAGE_TYPE:
            {
                // if MESSAGE_TYPE, get the first byte from the record value.
                type = record.read<uint8_t>();
                break;
            }
            case TLV_TYPE_INC_PARTICIPANT:
            {
                if (type == chatd::Message::kMsgCallEnd)
                {
                    assert(callEndedInfo);
                    callEndedInfo->participants.push_back(record.read<uint64_t>());
                }
                else if (type == chatd::Message::kMsgAlterParticipants || type == chatd::Message::kMsgPrivChange)
                {
                    assert(managementInfo);
                    if (managementInfo->target || managementInfo->privilege != PRIV_INVALID)
                        throw std::runtime_error("TLV_TYPE_INC_PARTICIPANT: Already parsed an incompatible TLV record");
                    managementInfo->privilege = chatd::PRIV_NOCHANGE;
                    managementInfo->target = record.read<uint64_t>();
                }
                else
                {
                    KARERE_LOG_ERROR(krLogChannel_strongvelope, "TLV_TYPE_INC_PARTICIPANT: This message type is not ready to receive this record");
                }
                break;
            }
            case TLV_TYPE_EXC_PARTICIPANT:
            {
                if (type == chatd::Message::kMsgAlterParticipants || type == chatd::Message::kMsgPrivChange)
                {
                    assert(managementInfo);
                    if (managementInfo->target || managementInfo->privilege != PRIV_INVALID)
                        throw std::runtime_error("TLV_TYPE_EXC_PARTICIPANT: Already parsed an incompatible TLV record");
                    managementInfo->privilege = chatd::PRIV_NOTPRESENT;
                    managementInfo->target = record.read<uint64_t>();
                }
                else
                {
                    KARERE_LOG_ERROR(krLogChannel_strongvelope, "TLV_TYPE_EXC_PARTICIPANT: This message type is not ready to receive this record");
                }
                break;
            }
            case TLV_TYPE_INVITOR:
            {
                sender = record.read<uint64_t>();
                break;
            }
            case TLV_TYPE_PRIVILEGE:
            {
                if (type == chatd::Message::kMsgAlterParticipants || type == chatd::Message::kMsgPrivChange)
                {
                    assert(managementInfo);
                    managementInfo->privilege = (chatd::Priv)record.read<uint8_t>();
                }
                else
                {
                    KARERE_LOG_ERROR(krLogChannel_strongvelope, "TLV_TYPE_PRIVILEGE: This message type is not ready to receive this record");
                }
                break;
            }
            case TLV_TYPE_KEYBLOB:
            {
                encryptedKey.assign(record.buf(), record.dataLen);
                break;
            }
            //legacy key stuff
            case TLV_TYPE_RECIPIENT:
            {
                if (!managementInfo)
                {
                    KARERE_LOG_ERROR(krLogChannel_strongvelope, "TLV_TYPE_RECIPIENT: This message type is not ready to receive this record");
                    assert(false);
                    break;
                }

                if (managementInfo->target)
                    throw std::runtime_error("Already had one RECIPIENT tlv record");
                record.validateDataLen(8);
                managementInfo->target = binaryMessage.read<uint64_t>(record.dataOffset);
                break;
            }
            case TLV_TYPE_KEYS:
            {
//KEYS, not KEY, because these can be pairs of current+previous key, concatenated and encrypted together
                encryptedKey.assign(record.buf(), record.dataLen);
                break;
            }
            case TLV_TYPE_KEY_IDS:
            {
//KEY_IDS, not KEY_ID, because the record may contain the previous keyid appended as well
                // The key length can change depending on the version
                uint32_t keyIdLength = getKeyIdLength(protocolVersion);
                if (record.dataLen != keyIdLength && record.dataLen != keyIdLength*2)
                    throw std::runtime_error("Key id length is not appropriate for this protocol version "+
                        std::to_string(protocolVersion)+
                        ": expected "+std::to_string(keyIdLength)+" actual: "+std::to_string(record.dataLen));
//we don't do minimal record size checks, as read() does them
//if we attempt to read past end of buffer, read() will throw
                if (keyIdLength == 4)
                {
                    keyId = ntohl(binaryMessage.read<uint32_t>(record.dataOffset));
                    prevKeyId = (record.dataLen > 4)
                        ? ntohl(binaryMessage.read<uint32_t>(record.dataOffset+4))
                        : 0;
                }
                else if (keyIdLength == 8)
                {
                    keyId = be64toh(binaryMessage.read<uint64_t>(record.dataOffset));
                    prevKeyId = (record.dataLen > 8)
                        ? be64toh(binaryMessage.read<uint64_t>(record.dataOffset+8))
                        : 0;
                }
                break;
            }
            //===
            case TLV_TYPE_PAYLOAD:
            {
//                if (type != SVCRYPTO_MSGTYPE_KEYED && type != SVCRYPTO_MSGTYPE_FOLLOWUP)
//                    throw std::runtime_error("Payload record found in a non-regular message");
                payload.assign(binaryMessage.buf()+record.dataOffset, record.dataLen);
                break;
            }
            default:
                throw std::runtime_error("Unknown TLV record type "+std::to_string(record.type)+" in message "+binaryMessage.id().toString());
        }
    }
    if (!recordNames.empty())
    {
        recordNames.resize(recordNames.size()-2);
        Id chatid = protoHandler.chatid;
        STRONGVELOPE_LOG_DEBUG("msg %s: read %s",
            binaryMessage.id().toString().c_str(), recordNames.c_str());
    }
}

void ParsedMessage::parsePayloadWithUtfBackrefs(const StaticBuffer &data, Message &msg)
{
    Id chatid = mProtoHandler.chatid;
    if (data.empty())
    {
        STRONGVELOPE_LOG_DEBUG("Empty message payload");
        return;
    }
#ifndef _MSC_VER
    // codecvt is deprecated
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert("parsePayload: Error doing utf8/16 conversion");
    std::u16string u16 = convert.from_bytes(data.buf(), data.buf()+data.dataSize());
    size_t len = u16.size();
#else
    std::string inpututf8(data.buf(), data.buf() + data.dataSize());
    std::string outpututf8;
    ::mega::MegaApi::utf8ToUtf16(inpututf8.c_str(), &outpututf8);
    std::u16string u16((char16_t*)outpututf8.data(), outpututf8.size() / 2);
    size_t len = u16.size();
#endif

    if(len < 10)
        throw std::runtime_error("parsePayload: payload is less than backrefs minimum size");

    Buffer data8(len);
    data8.setDataSize(10);
    for (size_t i=0; i<10; i++)
    {
//            if (u16[i] > 255)
//                  printf("char > 255: 0x%x, at offset %zu\n", u16[i], i);
        *(data8.buf()+i) = u16[i] & 0xff;
    }
    msg.backRefId = data8.read<uint64_t>(0);
    uint16_t refsSize = data8.read<uint16_t>(8);

    //convert back to utf8 the binary part, only to determine its utf8 len
#ifndef _MSC_VER
    size_t binlen8 = convert.to_bytes(&u16[0], &u16[refsSize+10]).size();
#else
    std::string result8;
    ::mega::MegaApi::utf16ToUtf8((wchar_t*)u16.data(), u16.size(), &result8);
    size_t binlen8 = result8.size();
#endif

    if (data.dataSize() > binlen8)
        msg.assign(data.buf()+binlen8, data.dataSize()-binlen8);
    else
    {
        msg.clear();
    }
}

void ParsedMessage::parsePayload(const StaticBuffer &data, Message &msg)
{
    if (this->protocolVersion < 3)
    {
        parsePayloadWithUtfBackrefs(data, msg);
        return;
    }

    if(data.dataSize() < 10)
        throw std::runtime_error("parsePayload: payload is less than backrefs minimum size");

    msg.backRefId = data.read<uint64_t>(0);
    uint16_t refsSize = data.read<uint16_t>(8);
    assert(msg.backRefs.empty());
    msg.backRefs.reserve(refsSize/8);
    size_t binsize = 10+refsSize;
    if (data.dataSize() < binsize)
        throw std::runtime_error("parsePayload: Payload size "+std::to_string(data.dataSize())+" is less than size of backrefs "+std::to_string(binsize)+"\nMessage:"+data.toString());
    char* end = data.buf() + binsize;
    for (char* prefid = data.buf() + 10; prefid < end; prefid += sizeof(uint64_t))
    {
        msg.backRefs.push_back(Buffer::alignSafeRead<uint64_t>(prefid));
    }
    if (data.dataSize() > binsize)
    {
        msg.assign(data.buf()+binsize, data.dataSize()-binsize);
    }
    else
    {
        msg.clear();
    }
}

ProtocolHandler::ProtocolHandler(karere::Id ownHandle,
    const StaticBuffer& privCu25519,
    const StaticBuffer& privEd25519,
    const StaticBuffer& privRsa,
    karere::UserAttrCache& userAttrCache, SqliteDb &db, Id aChatId, void *ctx)
: chatd::ICrypto(ctx), mOwnHandle(ownHandle), myPrivCu25519(privCu25519),
 myPrivEd25519(privEd25519), myPrivRsaKey(privRsa),
 mUserAttrCache(userAttrCache), mDb(db), chatid(aChatId)
{
    getPubKeyFromPrivKey(myPrivEd25519, kKeyTypeEd25519, myPubEd25519);
    loadKeysFromDb();
    loadUnconfirmedKeysFromDb();
    auto var = getenv("KRCHAT_FORCE_RSA");
    if (var)
    {
        mForceRsa = true;
        STRONGVELOPE_LOG_WARNING("KRCHAT_FORCE_RSA env var detected, will force RSA for key encryption");
    }
}

unsigned int ProtocolHandler::getCacheVersion() const
{
    return mCacheVersion;
}

void ProtocolHandler::loadKeysFromDb()
{
    SqliteStmt stmt(mDb, "select userid, keyid, key from sendkeys where chatid=?");
    stmt << chatid;
    while(stmt.step())
    {
        auto key = std::make_shared<SendKey>();
        stmt.blobCol(2, *key);
        Id userid(stmt.uint64Col(0));
        uint64_t keyid = stmt.uint64Col(1);

#ifndef NDEBUG
        auto ret =
#endif
        mKeys.emplace(std::piecewise_construct,
            std::forward_as_tuple(userid, keyid),
            std::forward_as_tuple(key));
        assert(ret.second);
    }
    STRONGVELOPE_LOG_DEBUG("(%" PRId64 "): Loaded %zu send keys from database", chatid, mKeys.size());
}

void ProtocolHandler::loadUnconfirmedKeysFromDb()
{
    SqliteStmt stmt(mDb, "select recipients, key_cmd, keyid from sending "
                    "where chatid=? and key_cmd not null order by rowid asc");
    stmt << chatid;
    while(stmt.step())
    {
        assert(stmt.hasBlobCol(1));

        // read recipients
        Buffer recpts;
        stmt.blobCol(0, recpts);
        karere::SetOfIds recipients;
        recipients.load(recpts);

        // read new key blobs
        Buffer keyBlobs;
        stmt.blobCol(1, keyBlobs);

        // read keyid
        KeyId keyid = (KeyId)stmt.intCol(2);
        assert(isLocalKeyId(keyid));

        //pick the version that is encrypted for us
        const char *pos = keyBlobs.buf();
        const char *end = keyBlobs.buf() + keyBlobs.dataSize();
        std::shared_ptr<Buffer> encryptedKey = nullptr;
        while (pos < end)
        {
            karere::Id receiver = Buffer::alignSafeRead<uint64_t>(pos);
            pos += 8;

            uint16_t keylen = Buffer::alignSafeRead<uint16_t>(pos);
            pos += 2;

            if (receiver == mOwnHandle)
            {
                encryptedKey = std::make_shared<Buffer>(16);
                encryptedKey->assign(pos, 16);
                break;
            }

            pos += keylen;
        }

        if (!encryptedKey) // not found
        {
            STRONGVELOPE_LOG_ERROR("(%" PRId64 "): own key not found in the keyblob from KeyCommand", chatid);
            assert(false);
            continue;
        }

        auto wptr = weakHandle();
        auto pms = decryptKey(encryptedKey, mOwnHandle, mOwnHandle);
        assert(pms.succeeded()); // our keys must be available in cache
        pms.then([this, wptr, recipients, keyid](const std::shared_ptr<SendKey>& key)
        {
            wptr.throwIfDeleted();

            mCurrentKey = key;
            mCurrentKeyId = keyid;
            mCurrentKeyParticipants = recipients;
            mCurrentLocalKeyId = keyid;

            NewKeyEntry entry(key, recipients, keyid);
            mUnconfirmedKeys.push_back(entry);
        });
    }

    STRONGVELOPE_LOG_DEBUG("(%" PRId64 "): Loaded %zu unconfirmed keys from database", chatid, mUnconfirmedKeys.size());
}

void ProtocolHandler::msgEncryptWithKey(const Message& src, MsgCommand& dest,
    const StaticBuffer& key)
{
    // create 'nonce' and encrypt plaintext --> `ciphertext`
    EncryptedMessage encryptedMessage(src, key);
    assert(!encryptedMessage.ciphertext.empty());

    // prepare TLV for content: <nonce><ciphertext>
    TlvWriter tlv(encryptedMessage.ciphertext.size()+128); //only signed content goes here
    tlv.addRecord(TLV_TYPE_NONCE, encryptedMessage.nonce);
    tlv.addRecord(TLV_TYPE_PAYLOAD, StaticBuffer(encryptedMessage.ciphertext, false));

    // prepare TLV for signature: <signature>
    Key<64> signature;
    signMessage(tlv, SVCRYPTO_PROTOCOL_VERSION, SVCRYPTO_MSGTYPE_FOLLOWUP,
                encryptedMessage.key, signature);
    TlvWriter sigTlv;
    sigTlv.addRecord(TLV_TYPE_SIGNATURE, signature);

    // finally, prepare the MsgCommand: <protVer><msgType><sigTLV><contentTLV>
    dest.reserve(tlv.dataSize()+sigTlv.dataSize()+2);
    dest.append<uint8_t>(SVCRYPTO_PROTOCOL_VERSION)
        .append<uint8_t>(SVCRYPTO_MSGTYPE_FOLLOWUP)
        .append(sigTlv)
        .append(tlv.buf(), tlv.dataSize()); //tlv must always be last, and the payload must always be last within the tlv, because the payload may span till end of message, (len code = 0xffff)
    dest.updateMsgSize();
}

promise::Promise<std::shared_ptr<SendKey>>
ProtocolHandler::computeSymmetricKey(karere::Id userid, const std::string& padString)
{
    auto it = mSymmKeyCache.find(userid);
    if (it != mSymmKeyCache.end())
    {
        return it->second;
    }
    auto wptr = weakHandle();
    return mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY)
    .then([wptr, this, userid, padString](const StaticBuffer* pubKey) -> promise::Promise<std::shared_ptr<SendKey>>
    {
        wptr.throwIfDeleted();
        // We may have had 2 almost parallel requests, and the second may
        // have put the key into the cache already
        auto it = mSymmKeyCache.find(userid);
        if (it != mSymmKeyCache.end())
            return it->second;

        if (pubKey->empty())
            return promise::Error("Empty Cu25519 chat key for user "+userid.toString());
        Key<crypto_scalarmult_BYTES> sharedSecret;
        sharedSecret.setDataSize(crypto_scalarmult_BYTES);
        auto ignore = crypto_scalarmult(sharedSecret.ubuf(), myPrivCu25519.ubuf(), pubKey->ubuf());
        (void)ignore;
        auto result = std::make_shared<SendKey>();
        deriveSharedKey(sharedSecret, *result, padString);
        mSymmKeyCache.emplace(userid, result);
        return result;
    });
}

Promise<std::shared_ptr<Buffer>>
ProtocolHandler::encryptKeyTo(const std::shared_ptr<SendKey>& sendKey, karere::Id toUser)
{
    auto wptr = weakHandle();
    return computeSymmetricKey(toUser)
    .then([wptr, this, sendKey](const std::shared_ptr<SendKey>& symkey) -> Promise<std::shared_ptr<Buffer>>
    {
        wptr.throwIfDeleted();
        if (mForceRsa)
            return promise::Error("Test: Forcing RSA");

        assert(symkey->dataSize() == SVCRYPTO_KEY_SIZE);
        auto result = std::make_shared<Buffer>((size_t)AES::BLOCKSIZE);
        result->setDataSize(AES::BLOCKSIZE); //dataSize() is used to check available buffer space of StaticBuffers
        aesECBEncrypt(*sendKey, *symkey, *result);
        return result;
    })
    .fail([wptr, this, toUser, sendKey](const promise::Error& err)
    {
        wptr.throwIfDeleted();
        STRONGVELOPE_LOG_DEBUG("Can't use EC encryption for user %s (error '%s'), falling back to RSA", toUser.toString().c_str(), err.what());
        return rsaEncryptTo(std::static_pointer_cast<StaticBuffer>(sendKey), toUser);
    })
    .fail([toUser, wptr, this](const promise::Error& err)
    {
        wptr.throwIfDeleted();
        STRONGVELOPE_LOG_ERROR("No public encryption key (RSA or x25519) available for %s", toUser.toString().c_str());
        return err;
    });
}

promise::Promise<std::shared_ptr<Buffer>>
ProtocolHandler::rsaEncryptTo(const std::shared_ptr<StaticBuffer>& data, Id toUser)
{
    assert(data->dataSize() <= 512);
    return mUserAttrCache.getAttr(toUser, USER_ATTR_RSA_PUBKEY)
    .then([data, toUser](Buffer* rsapub) -> promise::Promise<std::shared_ptr<Buffer>>
    {
        assert(rsapub && !rsapub->empty());
        ::mega::AsymmCipher key;
        auto ret = key.setkey(::mega::AsymmCipher::PUBKEY, rsapub->ubuf(), rsapub->dataSize());
        if (!ret)
            return promise::Error("Error parsing fetched public RSA key of user "+toUser.toString(), EINVAL, SVCRYPTO_ERRTYPE);

        auto output = std::make_shared<Buffer>(512);
        Buffer input;
        //prepend 16-bit byte length prefix in network byte order
        input.write<uint16_t>(0, htons(data->dataSize()));
        input.append(*data);
        assert(input.dataSize() == data->dataSize()+2);
        auto enclen = key.encrypt(input.ubuf(), input.dataSize(), (unsigned char*)output->writePtr(0, 512), 512);
        assert(enclen <= 512);
        output->setDataSize(enclen);
        return output;
    });
}

promise::Promise<std::shared_ptr<Buffer>>
ProtocolHandler::legacyDecryptKeys(const std::shared_ptr<ParsedMessage>& parsedMsg)
{
    // Check if sender key is encrypted using RSA.
    if (parsedMsg->encryptedKey.dataSize() < SVCRYPTO_RSA_ENCRYPTION_THRESHOLD)
    {
        if (parsedMsg->encryptedKey.dataSize() % AES::BLOCKSIZE)
            throw std::runtime_error("legacyDecryptKeys: invalid aes-encrypted key size");
        assert(parsedMsg->managementInfo);
        assert(parsedMsg->managementInfo->target);
        assert(parsedMsg->sender);
        Id otherParty = (parsedMsg->sender == mOwnHandle)
            ? parsedMsg->managementInfo->target
            : parsedMsg->sender;
        auto wptr = weakHandle();
        return computeSymmetricKey(otherParty)
        .then([this, wptr, parsedMsg](const std::shared_ptr<SendKey>& symKey)
        {
            wptr.throwIfDeleted();
            Key<32> iv;
            deriveNonceSecret(parsedMsg->nonce, iv, parsedMsg->managementInfo->target);
            iv.setDataSize(AES::BLOCKSIZE);

            // decrypt key
            auto result = std::make_shared<Buffer>();
            aesCBCDecrypt(parsedMsg->encryptedKey, *symKey, iv, *result);
            return result;
        });
    }
    else
    {
        // decrypt key using RSA
        auto result = std::make_shared<Buffer>();
        rsaDecrypt(parsedMsg->encryptedKey, *result);
        return result;
    }
}

promise::Promise<std::shared_ptr<SendKey>>
ProtocolHandler::decryptKey(std::shared_ptr<Buffer>& key, Id sender, Id receiver)
{
    // Check if sender key is encrypted using Cu25519 keys
    if (key->dataSize() < SVCRYPTO_RSA_ENCRYPTION_THRESHOLD)
    {
        if (key->dataSize() % AES::BLOCKSIZE)
            throw std::runtime_error("decryptKey: invalid aes-encrypted key size");

        Id otherParty = (sender == mOwnHandle) ? receiver : sender;
        auto wptr = weakHandle();
        return computeSymmetricKey(otherParty)
        .then([this, wptr, key, receiver](const std::shared_ptr<SendKey>& symmKey)
        {
            wptr.throwIfDeleted();
            // decrypt key
            auto result = std::make_shared<SendKey>();
            result->setDataSize(AES::BLOCKSIZE);
            aesECBDecrypt(*key, *symmKey, *result);
            return result;
        });
    }
    else    // legacy RSA encryption
    {
        STRONGVELOPE_LOG_DEBUG("Decrypting key from user %s using RSA", sender.toString().c_str());
        Buffer buf; //TODO: Maybe refine this
        rsaDecrypt(*key, buf);
        if (buf.dataSize() != AES::BLOCKSIZE)
            throw std::runtime_error("decryptKey: Unexpected rsa-decrypted send key length");
        auto result = std::make_shared<SendKey>();
        memcpy(result->buf(), buf.buf(), AES::BLOCKSIZE);
        return result;
    }
}

void ProtocolHandler::rsaDecrypt(const StaticBuffer& data, Buffer& output)
{
    assert(!myPrivRsaKey.empty());
    ::mega::AsymmCipher key;
    auto ret = key.setkey(::mega::AsymmCipher::PRIVKEY, myPrivRsaKey.ubuf(), myPrivRsaKey.dataSize());
    if (!ret)
        throw std::runtime_error("Error setting own RSA private key");
    auto len = data.dataSize();
    output.reserve(len);
    output.setDataSize(len);
    key.decrypt(data.ubuf(), len, output.ubuf(), len);
    uint16_t actualLen = ntohs(output.read<uint16_t>(0));
    assert(actualLen <= myPrivRsaKey.dataSize());
    memmove(output.buf(), output.buf()+2, actualLen);
    output.setDataSize(actualLen);
}

promise::Promise<std::pair<MsgCommand*, KeyCommand*>>
ProtocolHandler::msgEncrypt(Message* msg, const SetOfIds &recipients, MsgCommand* msgCmd)
{
    // if keyid has not been assigned yet...
    if (msg->keyid == CHATD_KEYID_INVALID)
    {
        assert(msgCmd->opcode() == OP_NEWMSG || msgCmd->opcode() == OP_NEWNODEMSG);
        // MSGUPDXs are created with keyid=INVALID, but as soon as the precedent NEWMSG
        // is encrypted, their corresponding MSGUPDX in the sending queue get their keyid
        // updated to the keyid used for the original NEWMSG

        // if already have a suitable key...
        if (mCurrentKey && mCurrentKeyParticipants == recipients)
        {
            msg->keyid = mCurrentKeyId;
            msgCmd->setKeyId(isLocalKeyId(mCurrentKeyId) ? CHATD_KEYID_UNCONFIRMED : mCurrentKeyId);
            msgEncryptWithKey(*msg, *msgCmd, *mCurrentKey);
            return std::make_pair(msgCmd, (KeyCommand*)nullptr);
        }
        else
        {
            auto wptr = weakHandle();
            return createNewKey(recipients)
            .then([this, wptr, msg, msgCmd](std::pair<KeyCommand*, std::shared_ptr<SendKey>> result) mutable
            {
                wptr.throwIfDeleted();
                msg->keyid = result.first->localKeyid();
                msgCmd->setKeyId(CHATD_KEYID_UNCONFIRMED);
                msgEncryptWithKey(*msg, *msgCmd, *result.second);
                return std::make_pair(msgCmd, result.first);
            });
        }
    }
    else if (msg->isLocalKeyid())   // edit of message whose key is in-flight
    {
        assert(msgCmd->opcode() == OP_MSGUPDX);

        for (auto& it: mUnconfirmedKeys)
        {
            // check this unconfirmed key was used to encrypt this message
            // It may happen that several messages for the same set of participants
            // are encrypted to different keys: K1+M1+K2+M2+K3+M3+M3upd
            // (K1 and K3 are for the same set of participants, K2 for a different one)
            NewKeyEntry entry = it;
            if (entry.recipients == recipients && entry.localKeyid == msg->keyid)
            {
                msgCmd->setKeyId(CHATD_KEYID_UNCONFIRMED);
                msgEncryptWithKey(*msg, *msgCmd, *entry.key);
                return std::make_pair(msgCmd, (KeyCommand*)nullptr);
            }
        }
        return promise::Error("Unconfirmed key for edit of msg "+msg->id().toString()+" not found.", EINVAL, SVCRYPTO_ENOKEY);
    }
    else    // confirmed keyid
    {
        assert(msgCmd->opcode() != OP_NEWMSG && msgCmd->opcode() != OP_NEWNODEMSG);  // new messages, at this stage, have an invalid keyid

        auto wptr = weakHandle();
        return getKey(UserKeyId(mOwnHandle, msg->keyid))
        .then([this, wptr, msg, msgCmd](const std::shared_ptr<SendKey>& key)
        {
            wptr.throwIfDeleted();
            msgCmd->setKeyId(msg->keyid);
            msgEncryptWithKey(*msg, *msgCmd, *key);
            return std::make_pair(msgCmd, (KeyCommand*)nullptr);
        });
    }
}

Message* ProtocolHandler::legacyMsgDecrypt(const std::shared_ptr<ParsedMessage>& parsedMsg,
    Message* msg, const SendKey& key)
{
    parsedMsg->symmetricDecrypt(key, *msg);
    msg->setEncrypted(Message::kNotEncrypted);
    return msg;
}

promise::Promise<std::string>
ProtocolHandler::decryptChatTitle(const Buffer& data)
{
    try
    {
        Buffer copy(data.dataSize());
        copy.copyFrom(data);
        auto msg = std::make_shared<chatd::Message>(
            karere::Id::null(), karere::Id::null(), 0, 0, std::move(copy));

        auto parsedMsg = std::make_shared<ParsedMessage>(*msg, *this);
        return parsedMsg->decryptChatTitle(msg.get(), false)
        // warning: parsedMsg must be kept alive when .then() is executed, so we
        // capture the shared pointer to it. Msg also must be kept alive, as
        // the promise returns it
        .then([msg, parsedMsg](Message* retMsg)
        {
            return std::string(retMsg->buf(), retMsg->dataSize());
        });
    }
    catch(std::exception& e)
    {
        return promise::Error(e.what(), EPROTO, SVCRYPTO_ERRTYPE);
    }
}

void ProtocolHandler::onHistoryReload()
{
    mCacheVersion++;
}

promise::Promise<Message*> ProtocolHandler::handleManagementMessage(
        const std::shared_ptr<ParsedMessage>& parsedMsg, Message* msg)
{
    if (msg->isManagementMessageKnownType())
    {
        msg->userid = parsedMsg->sender;
        msg->clear();
    }
    // else --> it's important to not clear the message, so future executions can
    // retry to decode it in case the new type is supported

    switch(parsedMsg->type)
    {
        case Message::kMsgAlterParticipants:
        case Message::kMsgPrivChange:
        {
            assert(parsedMsg->managementInfo);
            msg->createMgmtInfo(*(parsedMsg->managementInfo));
            msg->setEncrypted(Message::kNotEncrypted);

            return msg;
        }
        case Message::kMsgCallStarted:
        case Message::kMsgTruncate:
        {
            msg->setEncrypted(Message::kNotEncrypted);
            return msg;
        }
        case Message::kMsgChatTitle:
        {
            return parsedMsg->decryptChatTitle(msg, true);
        }
        case Message::kMsgCallEnd:
        {
            assert(parsedMsg->callEndedInfo);
            parsedMsg->callEndedInfo->callid = parsedMsg->payload.read<uint64_t>(0);
            parsedMsg->callEndedInfo->termCode= parsedMsg->payload.read<uint8_t>(8);
            parsedMsg->callEndedInfo->duration = parsedMsg->payload.read<uint32_t>(9);

            msg->createCallEndedInfo(*(parsedMsg->callEndedInfo));
            msg->setEncrypted(Message::kNotEncrypted);
            return msg;
        }
        default:
            return promise::Error("Unknown management message type "+
                std::to_string(parsedMsg->type), EINVAL, SVCRYPTO_ENOTYPE);
    }
}


//We should have already received and decrypted the key in advance
//(which is also async). This will have fetched the public Cu25519 key of
//the peer (unless the key was encrypted using RSA), but we still need the
//Ed25519 key for signature verification, which would not be fetched when the key
//is decrypted.
Promise<Message*> ProtocolHandler::msgDecrypt(Message* message)
{
    unsigned int cacheVersion = mCacheVersion;
    try
    {
        // deleted message
        if (message->empty())
        {
            message->setEncrypted(Message::kNotEncrypted);
            return Promise<Message*>(message);
        }

        // Get type
        auto parsedMsg = std::make_shared<ParsedMessage>(*message, *this);
        message->type = parsedMsg->type;

        // if message comes from API and uses keyid=0, it's a management message
        if (message->userid == karere::Id::COMMANDER() && message->keyid == 0)
        {
            return handleManagementMessage(parsedMsg, message);
        }

        // check tampering of management messages
        if (message->keyid == 0 || message->userid == karere::Id::COMMANDER())
        {
            return promise::Error("Invalid message. type: "+std::to_string(message->type)+
                                  " userid: "+message->userid.toString()+
                                  " keyid: "+std::to_string(message->keyid), EINVAL, SVCRYPTO_EMALFORMED);
        }

        // Get keyid
        uint64_t keyid;
        bool isLegacy = (parsedMsg->protocolVersion <= 1);
        if (isLegacy)
        {
            keyid = parsedMsg->keyId;
        }
        else
        {
            keyid = message->keyid;
        }

        // Get sender key
        struct Context
        {
            std::shared_ptr<SendKey> sendKey;
            EcKey edKey;
        };
        auto ctx = std::make_shared<Context>();

        auto symPms = getKey(UserKeyId(message->userid, keyid), isLegacy)
        .then([ctx](const std::shared_ptr<SendKey>& key)
        {
            ctx->sendKey = key;
        });

        // Get signing key
        auto edPms = mUserAttrCache.getAttr(parsedMsg->sender,
            ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY)
        .then([ctx](Buffer* key)
        {
            ctx->edKey.assign(key->buf(), key->dataSize());
        });

        // Verify signature and decrypt
        auto wptr = weakHandle();
        return promise::when(symPms, edPms)
        .then([this, wptr, message, parsedMsg, ctx, isLegacy, keyid, cacheVersion]() ->promise::Promise<Message*>
        {
            if (wptr.deleted())
            {
                return promise::Error("msgDecrypt: strongvelop deleted, ignore message", EINVAL, SVCRYPTO_EEXPIRED);
            }

            if (cacheVersion != mCacheVersion)
            {
                return promise::Error("msgDecrypt: history was reloaded, ignore message", EINVAL, SVCRYPTO_ENOMSG);
            }

            if (!parsedMsg->verifySignature(ctx->edKey, *ctx->sendKey))
            {
                return promise::Error("Signature invalid for message "+
                                      message->id().toString(), EINVAL, SVCRYPTO_ESIGNATURE);
            }

            if (isLegacy)
            {
                return legacyMsgDecrypt(parsedMsg, message, *ctx->sendKey);
            }

            // Decrypt message payload.
            parsedMsg->symmetricDecrypt(*ctx->sendKey, *message);
            return message;
        });
    }
    catch(std::runtime_error& e)
    {
        // ParsedMessage ctor throws if unexpected format, unknown/missing TLVs, etc.
        return promise::Error(e.what(), EINVAL, SVCRYPTO_EMALFORMED);
    }
}

Promise<void>
ProtocolHandler::legacyExtractKeys(const std::shared_ptr<ParsedMessage>& parsedMsg)
{
    if (parsedMsg->encryptedKey.empty())
        return promise::Error("legacyExtractKeys: No encrypted keys found in parsed message", EPROTO, SVCRYPTO_ERRTYPE);

    auto& key1 = mKeys[UserKeyId(parsedMsg->sender, parsedMsg->keyId)];
    if (!key1.key)
    {
        if (!key1.pms)
            key1.pms.reset(new Promise<std::shared_ptr<SendKey>>);
    }
    if (parsedMsg->prevKeyId)
    {
        auto& key2 = mKeys[UserKeyId(parsedMsg->sender, parsedMsg->prevKeyId)];
        if (!key2.key)
        {
            if (!key2.pms)
                key2.pms.reset(new Promise<std::shared_ptr<SendKey>>);
        }
    }
    auto wptr = weakHandle();
    return legacyDecryptKeys(parsedMsg)
    .then([this, wptr, parsedMsg](const std::shared_ptr<Buffer>& keys)
    {
        wptr.throwIfDeleted();
        // Add keys
        addDecryptedKey(UserKeyId(parsedMsg->sender, parsedMsg->keyId),
            std::make_shared<SendKey>(keys->buf(), (size_t)AES::BLOCKSIZE));
        if (parsedMsg->prevKeyId)
        {
            assert(keys->dataSize() == AES::BLOCKSIZE*2);
            addDecryptedKey(UserKeyId(parsedMsg->sender, parsedMsg->prevKeyId),
            std::make_shared<SendKey>(keys->buf()+AES::BLOCKSIZE, (size_t)AES::BLOCKSIZE));
        }
    });
}

void ProtocolHandler::onKeyReceived(KeyId keyid, Id sender, Id receiver,
                                    const char* data, uint16_t dataLen)
{
    auto encKey = std::make_shared<Buffer>(data, dataLen);
    UserKeyId ukid(sender, keyid);

    // decrypt key (could require to fetch public key from API asynchronously)
    auto pms = decryptKey(encKey, sender, receiver);
    if (pms.succeeded())    // decryption was synchronous
    {
        addDecryptedKey(ukid, pms.value());
        return;
    }

    // check if key is already being decrypted (received twice)
    auto& entry = mKeys[ukid];
    if (entry.pms)
    {
        STRONGVELOPE_LOG_WARNING("Key %d from user %s is already being decrypted", keyid, sender.toString().c_str());
        return;
    }

    // if it was not being decrypted yet, associate a promise
    STRONGVELOPE_LOG_DEBUG("onKeyReceived: Created a key entry with promise for key %d of user %s", keyid, sender.toString().c_str());
    auto wptr = weakHandle();
    entry.pms.reset(new Promise<std::shared_ptr<SendKey>>);
    pms.then([this, wptr, ukid](const std::shared_ptr<SendKey>& key)
    {
        wptr.throwIfDeleted();
        //addDecryptedKey will remove entry.pms, but anyone that has already
        //attached to it will be notified first
        addDecryptedKey(ukid, key);
    });
    pms.fail([this, wptr, ukid](const promise::Error& err)
    {
        wptr.throwIfDeleted();
        STRONGVELOPE_LOG_ERROR("Removing key entry for key %u - decryptKey() failed with error '%s'", ukid.keyid, err.what());

        auto it = mKeys.find(ukid);
        assert(it != mKeys.end());
        assert(it->second.pms);
        it->second.pms->reject(err);
        mKeys.erase(it);
        return err;
    });
}

void ProtocolHandler::addDecryptedKey(UserKeyId ukid, const std::shared_ptr<SendKey>& key)
{
    assert(key->dataSize() == SVCRYPTO_KEY_SIZE);
    STRONGVELOPE_LOG_DEBUG("Adding key %lld of user %s", ukid.keyid, ukid.user.toString().c_str());

    auto& entry = mKeys[ukid];
    if (entry.key)  // if KeyEntry already had a decrypted key assigned to it...
    {
        if (memcmp(entry.key->buf(), key->buf(), SVCRYPTO_KEY_SIZE))
            throw std::runtime_error("addDecryptedKey: Key with id "+std::to_string(ukid.keyid)+" from user '"+ukid.user.toString()+"' already known but different");

        STRONGVELOPE_LOG_DEBUG("addDecryptedKey: Key %lld from user %s already known and is same", ukid.keyid, ukid.user.toString().c_str());
    }
    else    // new key was confirmed or received key wast not decrypted yet...
    {
        entry.key = key;
        try
        {
            mDb.query("insert or ignore into sendkeys(chatid, userid, keyid, key, ts) values(?,?,?,?,?)",
                chatid, ukid.user, ukid.keyid, *key, (int)time(NULL));
        }
        catch(std::exception& e)
        {
            STRONGVELOPE_LOG_ERROR("Exception while saving sendkey to db: %s", e.what());
            throw std::runtime_error("addDecryptedKey: Exception while saving sendkey to db: "+std::string(e.what()));
        }
    }

    // finally, notify anyone waiting for decryption of the received key (if decryption was asynchronous)
    if (entry.pms)
    {
        entry.pms->resolve(entry.key);
        entry.pms.reset();
    }
}
promise::Promise<std::shared_ptr<SendKey>>
ProtocolHandler::getKey(UserKeyId ukid, bool legacy)
{
    auto kit = mKeys.find(ukid);
    if (kit == mKeys.end())
    {
        if (legacy)
        {
            auto& key = mKeys[ukid];
            key.pms.reset(new Promise<std::shared_ptr<SendKey>>);
            return *key.pms;
        }
        else
        {
            return promise::Error("Key with id "+std::to_string(ukid.keyid)+
            " from user "+ukid.user.toString()+" not found", EINVAL, SVCRYPTO_ENOKEY);
        }
    }

    auto& entry = kit->second;
    if (entry.key)  // key is available
    {
        return entry.key;
    }
    else if (entry.pms) // key is being decrypted
    {
        return (*entry.pms);
    }
    else
    {
        assert(false && "Key found but no promise not key member are set");
        return std::shared_ptr<SendKey>(); //return something, just to mute the gcc warning
    }
}

void ProtocolHandler::onKeyConfirmed(KeyId localkeyid, KeyId keyid)
{
    // new keys are always confirmed in the same order than received by chatd
    auto it = mUnconfirmedKeys.begin();
    if (it == mUnconfirmedKeys.end())
    {
        STRONGVELOPE_LOG_WARNING("onKeyConfirmed: unexpected confirmation of key");
        return;
    }

    NewKeyEntry &entry = *it;
    UserKeyId userKeyId(mOwnHandle, keyid);
    std::shared_ptr<SendKey> confirmedKey = entry.key;
    assert(entry.localKeyid == localkeyid);
    assert(mKeys.find(userKeyId) == mKeys.end());

    // add confirmed key to mKeys
    addDecryptedKey(userKeyId, confirmedKey);

    // check if confirmed key is the currentKey
    if (mCurrentKey && mUnconfirmedKeys.size() == 1)
    {
        // if there are multiple new keys in-flight, the currentKey
        // is the last one being confirmed
        assert(memcmp(mCurrentKey->buf(), confirmedKey->buf(), mCurrentKey->dataSize()) == 0);
        assert(mCurrentKeyId == localkeyid);
        assert(mCurrentKeyParticipants == entry.recipients);

        mCurrentKeyId = keyid;  // localkeyid --> keyid
    }

    // remove it from queue of unconfirmed keys
    mUnconfirmedKeys.erase(it);
}

void ProtocolHandler::onKeyRejected()
{
    // new keys are always rejected in the same order than received by chatd
    auto it = mUnconfirmedKeys.begin();
    if (it == mUnconfirmedKeys.end())
    {
        STRONGVELOPE_LOG_WARNING("onKeyRejected: unexpected rejection of key");
        return;
    }

    // check if rejected key is the current key
    NewKeyEntry &entry = *it;
    std::shared_ptr<SendKey> rejectedKey = entry.key;
    if (mCurrentKey && mUnconfirmedKeys.size() == 1)
    {
        assert(memcmp(mCurrentKey->buf(), rejectedKey->buf(), mCurrentKey->dataSize()) == 0);
        assert(mCurrentKeyId == entry.localKeyid);
        assert(mCurrentKeyParticipants == entry.recipients);

        resetSendKey();
    }

    // remove it from queue of unconfirmed keys
    mUnconfirmedKeys.erase(it);
}

promise::Promise<std::pair<KeyCommand*, std::shared_ptr<SendKey>>>
ProtocolHandler::createNewKey(const SetOfIds &recipients)
{
    mCurrentKey.reset(new SendKey);
    mCurrentKey->setDataSize(AES::BLOCKSIZE);
    randombytes_buf(mCurrentKey->ubuf(), AES::BLOCKSIZE);

    mCurrentKeyId = createLocalKeyId();
    mCurrentKeyParticipants = recipients;

    NewKeyEntry entry(mCurrentKey, mCurrentKeyParticipants, mCurrentKeyId);
    mUnconfirmedKeys.push_back(entry);

    // Assemble the output for all recipients.
    return encryptKeyToAllParticipants(mCurrentKey, mCurrentKeyParticipants, mCurrentKeyId);
}

KeyId ProtocolHandler::createLocalKeyId()
{
    if (--mCurrentLocalKeyId < CHATD_KEYID_MIN)
        mCurrentLocalKeyId = CHATD_KEYID_MAX;

    return mCurrentLocalKeyId;
}

promise::Promise<std::pair<KeyCommand*, std::shared_ptr<SendKey>>>
ProtocolHandler::encryptKeyToAllParticipants(const std::shared_ptr<SendKey>& key, const SetOfIds &participants, KeyId localkeyid)
{
    auto keyCmd = new KeyCommand(chatid, localkeyid);

    // Users and send key may change while we are getting pubkeys of current
    // users, so make a snapshot
    SetOfIds users = participants;
    std::vector<Promise<void>> promises;
    promises.reserve(users.size());

    for (auto& user: users)
    {
        auto pms = encryptKeyTo(key, user)
        .then([keyCmd, user](const std::shared_ptr<Buffer>& encryptedKey)
        {
            assert(encryptedKey && !encryptedKey->empty());
            keyCmd->addKey(user, encryptedKey->buf(), encryptedKey->dataSize());
        });
        promises.push_back(pms);
    }

    // wait for key encrypted to all participants (immediate only if all pubkeys were available)
    return promise::when(promises)
    .then([keyCmd, key]()
    {
        return std::make_pair(keyCmd, key);
    });
}

promise::Promise<std::shared_ptr<Buffer>>
ProtocolHandler::encryptChatTitle(const std::string& data, uint64_t extraUser)
{
    auto key = std::make_shared<SendKey>();
    randombytes_buf(key->buf(), key->bufSize());
    assert(!key->empty());
    auto blob = std::make_shared<Buffer>(512);
    blob->clear();
    blob->append<uint8_t>(SVCRYPTO_PROTOCOL_VERSION);
    blob->append<uint8_t>(Message::kMsgChatTitle);

    SetOfIds participants = *mParticipants;
    if (extraUser)
    {
        participants.insert(extraUser);
    }

    auto wptr = weakHandle();
    return encryptKeyToAllParticipants(key, participants)
    .then([this, wptr, blob, data](const std::pair<chatd::KeyCommand*, std::shared_ptr<SendKey>>& result)
    {
        wptr.throwIfDeleted();
        auto& key = result.second;
        chatd::Message msg(0, mOwnHandle, 0, 0, Buffer(data.c_str(), data.size()));
        msg.backRefId = chatd::Chat::generateRefId(this);
        EncryptedMessage enc(msg, *key);

        chatd::KeyCommand& keyCmd = *result.first;
        assert(keyCmd.dataSize() >= 17);
        TlvWriter tlv;
        tlv.addRecord(TLV_TYPE_INVITOR, mOwnHandle.val);
        tlv.addRecord(TLV_TYPE_NONCE, enc.nonce);
        tlv.addRecord(TLV_TYPE_KEYBLOB, StaticBuffer(keyCmd.buf()+17, keyCmd.dataSize()-17));
        tlv.addRecord(TLV_TYPE_PAYLOAD, StaticBuffer(enc.ciphertext, false));
        Key<64> signature;
        signMessage(tlv, SVCRYPTO_PROTOCOL_VERSION, Message::kMsgChatTitle,
            enc.key, signature);
        TlvWriter sigTlv;
        sigTlv.addRecord(TLV_TYPE_SIGNATURE, signature);
        blob->append(sigTlv);
        blob->append(tlv);
        return blob;
    });
}

promise::Promise<chatd::Message*>
ParsedMessage::decryptChatTitle(chatd::Message* msg, bool msgCanBeDeleted)
{
    const char* pos = encryptedKey.buf();
    const char* end = encryptedKey.buf()+encryptedKey.dataSize();
    karere::Id receiver;

    //pick the version that is encrypted for us
    while (pos < end)
    {
        receiver = Buffer::alignSafeRead<uint64_t>(pos);
        pos+=8;
        uint16_t keylen = *(uint16_t*)(pos);
        pos+=2;
        if (receiver == mProtoHandler.ownHandle())
            break;
        pos+=keylen;
    }

    if (pos >= end)
        throw std::runtime_error("Error getting a version of the encryption key encrypted for us");
    if (end-pos < 16)
        throw std::runtime_error("Unexpected key entry length - must be 26 bytes, but is "+std::to_string(end-pos)+" bytes");
    auto buf = std::make_shared<Buffer>(16);
    buf->assign(pos, 16);
    auto wptr = weakHandle();
    unsigned int cacheVersion = mProtoHandler.getCacheVersion();
    return mProtoHandler.decryptKey(buf, sender, receiver)
    .then([this, wptr, msg, cacheVersion, msgCanBeDeleted](const std::shared_ptr<SendKey>& key)
    {
        wptr.throwIfDeleted();

        if (msgCanBeDeleted && cacheVersion != mProtoHandler.getCacheVersion())
        {
            throw promise::Error("decryptChatTitle: history was reloaded, ignore message",  EINVAL, SVCRYPTO_ENOMSG);
        }

        symmetricDecrypt(*key, *msg);
        msg->setEncrypted(Message::kNotEncrypted);
        return msg;
    });
}

void ProtocolHandler::onUserJoin(Id userid)
{
    // preload keys for the new participant
    mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY, nullptr, nullptr);
    mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY, nullptr, nullptr);
    mUserAttrCache.getAttr(userid, USER_ATTR_RSA_PUBKEY, nullptr, nullptr);
}

void ProtocolHandler::onUserLeave(Id /*userid*/)
{
}

void ProtocolHandler::resetSendKey()
{
    mCurrentKey.reset();
    mCurrentKeyId = CHATD_KEYID_INVALID;
    mCurrentKeyParticipants = SetOfIds();
}

void ProtocolHandler::setUsers(karere::SetOfIds* users)
{
    assert(users);
    mParticipants = users;

    //pre-fetch user attributes
    for (auto userid: *users)
    {
        mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY, nullptr, nullptr);
        mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY, nullptr, nullptr);
        mUserAttrCache.getAttr(userid, USER_ATTR_RSA_PUBKEY, nullptr, nullptr);
    }
}

bool ProtocolHandler::handleLegacyKeys(chatd::Message& msg)
{
    auto protoVer = msg.read<uint8_t>(0);
    if (protoVer > 1)
        return false;
    TlvParser tlv(msg, 1, true);
    TlvRecord record(msg);
    while (tlv.getRecord(record))
    {
        if (record.type == TLV_TYPE_MESSAGE_TYPE)
        {
            if (record.dataLen != 1)
                throw std::runtime_error("TLV message type record is not 1 byte");
            uint8_t type = msg.read<uint8_t>(record.dataOffset);
            if (type != SVCRYPTO_MSGTYPE_KEYED)
                return false;
            auto parsed = std::make_shared<ParsedMessage>(msg, *this);
            legacyExtractKeys(parsed);
            return true;
        }
    }
    return false;
}
void ProtocolHandler::randomBytes(void* buf, size_t bufsize) const
{
    randombytes_buf(buf, bufsize);
}

ProtocolHandler::NewKeyEntry::NewKeyEntry(const std::shared_ptr<SendKey> &aKey, SetOfIds aRecipients, KeyId aLocalKeyid)
    : key(aKey), recipients(aRecipients), localKeyid(aLocalKeyid)
{

}

} //end strongvelope namespace

namespace chatd
{
std::string Message::managementInfoToString() const
{
    std::string ret;
    ret.reserve(128);
    switch (type)
    {
    case kMsgAlterParticipants:
    {
        auto& info = mgmtInfo();
        ret.append("User ").append(userid.toString())
           .append((info.privilege == chatd::PRIV_NOTPRESENT) ? " removed" : " added")
           .append(" user ").append(info.target.toString());
        return ret;
    }
    case kMsgTruncate:
    {
        ret.append("Chat history was truncated by user ").append(userid.toString());
        return ret;
    }
    case kMsgPrivChange:
    {
        auto& info = mgmtInfo();
        ret.append("User ").append(userid.toString())
           .append(" set privilege of user ").append(info.target.toString())
           .append(" to ").append(chatd::privToString(info.privilege));
        return ret;
    }
    case kMsgChatTitle:
    {
        ret.append("User ").append(userid.toString())
           .append(" set chat title to '")
           .append(buf(), dataSize())+='\'';
        return ret;
    }
    default:
        throw std::runtime_error("Message with type "+std::to_string(type)+" is not a management message");
    }
}
}

