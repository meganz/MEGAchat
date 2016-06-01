/*
 * strongvelope.cpp
 *
 *  Created on: 17/11/2015
 *      Author: admin2
 */

#include "strongvelope.h"
#include "cryptofunctions.h"
#include <ctime>
#include "sodium.h"
#include "tlvstore.h"
#include <userAttrCache.h>
#include <mega.h>

namespace strongvelope
{
using namespace karere;
using namespace promise;
using namespace CryptoPP;
using namespace chatd;

const std::string PAIRWISE_KEY = "strongvelope pairwise key";
const std::string PAIRWISE_KEY_WITH_SEP = PAIRWISE_KEY+(char)0x01u;
const std::string SVCRYPTO_SIG = "strongvelopesig";
const karere::Id API_USER("gTxFhlOd_LQ");
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
        case TLV_TYPE_OWN_KEY: return "TLV_OWN_KEY";
        default: return "(unknown)";
    }
}

uint32_t getKeyIdLength(uint32_t protocolVersion)
{
    return (protocolVersion == 1) ? 8 : 4;
}

EncryptedMessage::EncryptedMessage(const std::string& clearStr,
    const StaticBuffer& aKey)
: key(aKey)
{
    assert(!key.empty());
    randombytes_buf(nonce.buf(), nonce.bufSize());
    Key<32> derivedNonce(32); //deriveNonceSecret uses dataSize() to confirm there is buffer space
    deriveNonceSecret(nonce, derivedNonce);
    derivedNonce.setDataSize(SVCRYPTO_NONCE_SIZE+4); //truncate to nonce size+32bit counter

    *reinterpret_cast<uint32_t*>(derivedNonce.buf()+SVCRYPTO_NONCE_SIZE) = 0; //zero the 32-bit counter
    assert(derivedNonce.dataSize() == AES::BLOCKSIZE);
    if (!clearStr.empty())
    {
        ciphertext = aesCTREncrypt(clearStr, key, derivedNonce);
    }
}

/**
 * Decrypts a message symmetrically using AES-128-CTR.
 *
 * Note: Nonces longer than the used NONCE_SIZE bytes are truncated.
 *
 * @param cipher {String} Message in cipher text.
 * @param key {String} Symmetric encryption key in a binary string.
 * @param nonce {String} Nonce to decrypt a message with in a binary string.
 * @returns {String} Clear text of message content.
 */
void symmetricDecryptMessage(const std::string& cipher, const StaticBuffer& key,
                              const StaticBuffer& nonce, Message& outMsg)
{
    STRONGVELOPE_LOG_DEBUG("Decrypting message %s", outMsg.id().toString().c_str());
    char derivedNonce[32];
    // deriveNonceSecret() needs at least 32 bytes output buffer
    deriveNonceSecret(nonce, StaticBuffer(derivedNonce, 32));

    // For AES CRT mode, we take the first 12 bytes as the nonce,
    // and the remaining 4 bytes as the counter, which is initialized to zero
    *reinterpret_cast<uint32_t*>(derivedNonce+SVCRYPTO_NONCE_SIZE) = 0;
    std::string cleartext = aesCTRDecrypt(cipher, key,
                                StaticBuffer(derivedNonce, AES::BLOCKSIZE));
    outMsg.assign<false>(cleartext);
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
    std::string recipientStr = (recipient == Id::null())
        ? "payload"
        : recipient.toString();

    // Equivalent to first block of HKDF, see RFC 5869.
    hmac_sha256_bytes(StaticBuffer(recipientStr, false), masterNonce, result);
}

void ProtocolHandler::signMessage(const StaticBuffer& message, const StaticBuffer& keyToInclude,
                 Buffer& signature)
{
// To save space, myPrivEd25519 holds only the 32-bit seed of the priv key,
// without the pubkey part, so we add it here
    Buffer key(myPrivEd25519.dataSize()+myPubEd25519.dataSize());
    key.append(myPrivEd25519).append(myPubEd25519);

    Buffer toSign(keyToInclude.dataSize()+message.dataSize()+SVCRYPTO_SIG.size());
    toSign.append(SVCRYPTO_SIG).append(keyToInclude).append(message);

    crypto_sign_detached((unsigned char*)signature.writePtr(0, crypto_sign_BYTES),
        NULL, toSign.ubuf(), toSign.dataSize(),
        key.ubuf());
}

bool ParsedMessage::verifySignature(const StaticBuffer& pubKey, const SendKey& sendKey)
{
    assert(sendKey.dataSize() == 16);
    assert(pubKey.dataSize() == 32);
    Buffer messageStr(SVCRYPTO_SIG.size()+sendKey.dataSize()+signedContent.dataSize());
    messageStr.append(SVCRYPTO_SIG.c_str(), SVCRYPTO_SIG.size()).append(sendKey).append(signedContent);

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
void deriveSharedKey(const StaticBuffer& sharedSecret, SendKey& output)
{
    assert(output.dataSize() == AES::BLOCKSIZE);
	// Equivalent to first block of HKDF, see RFC 5869.
    Key<32> sharedSecretKey;
    hmac_sha256_bytes(sharedSecret, StaticBuffer(nullptr, 0), sharedSecretKey); //step 1 - extract
    std::string infoStr = PAIRWISE_KEY+(char)0x01u; //For efficiency the 0x01u can be appended to the constant definition itself
    Key<32> step2;
    hmac_sha256_bytes(StaticBuffer(infoStr, false), sharedSecretKey, step2); //step 2 - expand
    memcpy(output.buf(), step2.buf(), AES::BLOCKSIZE);
}

ParsedMessage::ParsedMessage(const Message& binaryMessage)
{
    if(binaryMessage.empty())
    {
        throw std::runtime_error("parsedMessage::parse: Empty binary message");
    }
    protocolVersion = binaryMessage.read<uint8_t>(0);
    if (protocolVersion > SVCRYPTO_PROTOCOL_VERSION)
        throw std::runtime_error("Message protocol version "+std::to_string(protocolVersion)+" is newer than the latest supported by this client");
    sender = binaryMessage.userid;

    size_t offset;
    bool isLegacy = (protocolVersion < 2);
    if (isLegacy)
    {
        offset = 1;
    }
    else
    {
        offset = 2;
        type = (MessageType)binaryMessage.read<uint8_t>(1);
    }
    //participant change stuff
    std::unique_ptr<Buffer> userAddInfo;
    std::unique_ptr<Buffer> userExcInfo;
    //==
    TlvParser tlv(binaryMessage, offset, isLegacy);
    TlvRecord record(binaryMessage);
    while (tlv.getRecord(record))
    {
        STRONGVELOPE_LOG_DEBUG("parse[msg %s]: read TLV record type: %s",
            binaryMessage.id().toString().c_str(), tlvTypeToString(record.type));
        switch (record.type)
    	{
            case TLV_TYPE_SIGNATURE:
    	    {
                signature.assign(
                    binaryMessage.buf()+record.dataOffset, record.dataLen);
                auto nextOffset = record.dataOffset+record.dataLen;
                signedContent.assign(binaryMessage.buf()+nextOffset, binaryMessage.dataSize()-nextOffset);
                break;
    	    }
            case TLV_TYPE_NONCE:
            {
                nonce.assign(binaryMessage.buf()+record.dataOffset, record.dataLen);
                break;
            }
            case TLV_TYPE_MESSAGE_TYPE:
    	    {
                record.validateDataLen(1);
                // if MESSAGE_TYPE, get the first byte from the record value.
                type = (MessageType)binaryMessage.read<uint8_t>(record.dataOffset);
    	    	break;
    	    }
            case TLV_TYPE_INC_PARTICIPANT:
            {
                if (!userAddInfo)
                    userAddInfo.reset(new Buffer);
                userAddInfo->append(binaryMessage.buf()+record.dataOffset, record.dataLen);
                break;
            }
            case TLV_TYPE_EXC_PARTICIPANT:
    	    {
                if (!userExcInfo)
                    userExcInfo.reset(new Buffer);
                userExcInfo->append(binaryMessage.buf()+record.dataOffset, record.dataLen);
                break;
            }
            //legacy key stuff
            case TLV_TYPE_RECIPIENT:
            {
                if (receiver)
                    throw std::runtime_error("Already had one RECIPIENT tlv record");
                record.validateDataLen(8);
                receiver = binaryMessage.read<uint64_t>(record.dataOffset);
                break;
            }
            case TLV_TYPE_KEYS:
    	    {
//KEYS, not KEY, because these can be pairs of current+previous key, concatenated and encrypted together
            encryptedKey.assign(binaryMessage.buf()+record.dataOffset,
                record.dataLen);
                break;
    	    }
            case TLV_TYPE_KEY_IDS:
            {
//KEY_IDS, not KEY_ID, because the record may contain the previous keyid appended as well
                // The key length can change depending on the version
                uint32_t keyIdLength = getKeyIdLength(protocolVersion);
                if (keyIdLength != record.dataLen)
                    throw std::runtime_error("Key id length is not appropriate for this protocol version");
//we don't do minimal record size checks, as read() does them
//if we attempt to read past end of buffer, read() will throw
                if (keyIdLength == 4)
                {
                    keyId = binaryMessage.read<uint32_t>(record.dataOffset);
                    prevKeyId = (record.dataLen > 4)
                        ? binaryMessage.read<uint32_t>(record.dataOffset+4)
                        : 0;
                }
                else if (keyIdLength == 8)
                {
                    keyId = binaryMessage.read<uint64_t>(record.dataOffset);
                    prevKeyId = (record.dataLen > 8)
                        ? binaryMessage.read<uint64_t>(record.dataOffset+8)
                        : 0;
                }
    	    	break;
    	    }
            //===
            case TLV_TYPE_PAYLOAD:
            {
                if (type != SVCRYPTO_MSGTYPE_KEYED && type != SVCRYPTO_MSGTYPE_FOLLOWUP)
                    throw std::runtime_error("Payload record found in a non-regular message");
                payload.assign(binaryMessage.buf()+record.dataOffset, record.dataLen);
                break;
            }
    	    default:
                throw std::runtime_error("Unknown TLV record type "+std::to_string(record.type)+" in message "+binaryMessage.id().toString());
    	}
        if (type == SVCRYPTO_MSGTYPE_ALTER_PARTICIPANTS)
        {
            assert(payload.empty());
            payload.append<chatd::Message::Type>(chatd::Message::kUserMsgAlterParticipants);
            if (userAddInfo)
            {
                payload.append<int16_t>(userAddInfo->dataSize()/8).append(static_cast<StaticBuffer>(*userAddInfo));
            }
            if (userExcInfo)
            {
                payload.append<int16_t>(-userExcInfo->dataSize()/8).append(*userExcInfo);
            }
        }
    }
}

/***************************************************************************************************************/
/*************************************Strongvelope::ProtocolHandler*********************************************/
/***************************************************************************************************************/

ProtocolHandler::ProtocolHandler(karere::Id ownHandle,
    const StaticBuffer& privCu25519,
    const StaticBuffer& privEd25519,
    const StaticBuffer& privRsa,
    karere::UserAttrCache& userAttrCache)
:mOwnHandle(ownHandle), myPrivCu25519(privCu25519),
 myPrivEd25519(privEd25519), myPrivRsaKey(privRsa),
 mUserAttrCache(userAttrCache)
{
    myPubCu25519.setDataSize(32);
    myPubEd25519.setDataSize(32);
    getPubKeyFromPrivKey(myPrivCu25519, kKeyTypeCu25519, myPubCu25519);
    getPubKeyFromPrivKey(myPrivEd25519, kKeyTypeEd25519, myPubEd25519);
}

void ProtocolHandler::msgEncryptWithKey(Buffer& src, chatd::MsgCommand& dest, const StaticBuffer& key)
{
    std::string text;
    if (!src.empty())
        text.assign(src.buf(), src.dataSize());
    EncryptedMessage encryptedMessage(text, key);
    TlvWriter tlv; //only signed content goes here
    // Assemble message content.
    tlv.addRecord(TLV_TYPE_NONCE, encryptedMessage.nonce);
    // Only include ciphertext if it's not empty (non-blind message).
    if (!encryptedMessage.ciphertext.empty())
    {
        tlv.addRecord(TLV_TYPE_PAYLOAD, StaticBuffer(encryptedMessage.ciphertext, false));
    }
    Buffer signature;
    signMessage(tlv, key, signature);
    TlvWriter sigTlv;
    sigTlv.addRecord(TLV_TYPE_SIGNATURE, signature);

    dest.reserve(tlv.dataSize()+sigTlv.dataSize()+2);
    dest.append<uint8_t>(SVCRYPTO_PROTOCOL_VERSION)
        .append<uint8_t>(SVCRYPTO_MSGTYPE_FOLLOWUP)
        .append(sigTlv)
        .append(tlv.buf(), tlv.dataSize()); //tlv must always be last, and the payload must always be last within the tlv, because the payload may span till end of message, (len code = 0xffff)
    dest.updateMsgSize();
}


promise::Promise<std::shared_ptr<SendKey>>
ProtocolHandler::computeSymmetricKey(karere::Id userid)
{
    return mUserAttrCache.getAttr(userid, ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY)
    .then([this, userid](const StaticBuffer* pubKey)
    {
        assert(!pubKey->empty() && "BUG: No cached Cu25519 chat key for user, and still someone tried to use it!");
        Key<crypto_scalarmult_BYTES> sharedSecret;
        sharedSecret.setDataSize(crypto_scalarmult_BYTES);
        crypto_scalarmult(sharedSecret.ubuf(), myPrivCu25519.ubuf(), pubKey->ubuf());
        auto result = std::make_shared<SendKey>();
        deriveSharedKey(sharedSecret, *result);
        return result;
    });
}

Promise<std::shared_ptr<Buffer>>
ProtocolHandler::encryptKeyTo(const std::shared_ptr<SendKey>& sendKey, karere::Id toUser)
{
    /**
     * Use RSA encryption if no chat key is available.
     */
    return computeSymmetricKey(toUser)
    .then([this, sendKey](const std::shared_ptr<SendKey>& symkey)
    {
        assert(symkey->dataSize() == SVCRYPTO_KEY_SIZE);
        auto result = std::make_shared<Buffer>((size_t)AES::BLOCKSIZE);
        result->setDataSize(AES::BLOCKSIZE); //dataSize() is used to check available buffer space of StaticBuffers
        aesECBEncrypt(*sendKey, *symkey, *result);
        return result;
    })
    .fail([this, toUser, sendKey](const promise::Error&)
    {
        return rsaEncryptTo(std::static_pointer_cast<StaticBuffer>(sendKey), toUser);
    })
    .fail([toUser](const promise::Error& err)
    {
        STRONGVELOPE_LOG_ERROR("No public encryption key (RSA or x25519) available for %s", toUser.toString().c_str());
        return err;
    });
}

promise::Promise<std::shared_ptr<Buffer>>
ProtocolHandler::rsaEncryptTo(const std::shared_ptr<StaticBuffer>& data, Id toUser)
{
    assert(data->dataSize() <= 512);
    return mUserAttrCache.getAttr(toUser, USER_ATTR_RSA_PUBKEY)
    .then([this, data, toUser](Buffer* rsapub) -> promise::Promise<std::shared_ptr<Buffer>>
    {
        assert(rsapub && !rsapub->empty());
        ::mega::AsymmCipher key;
        auto ret = key.setkey(::mega::AsymmCipher::PUBKEY, rsapub->ubuf(), rsapub->dataSize());
        if (!ret)
            return promise::Error("Error parsing fetched public RSA key of user "+toUser.toString(), EINVAL, SVCRYPTO_ERRTYPE);

        STRONGVELOPE_LOG_DEBUG("Encrypting data for %s using RSA", toUser.toString().c_str());
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
//    const std::string& nonce, karere::Id otherParty, bool iAmSender)
{
    // Check if sender key is encrypted using RSA.
    if (parsedMsg->encryptedKey.dataSize() < SVCRYPTO_RSA_ENCRYPTION_THRESHOLD)
    {
        if (parsedMsg->encryptedKey.dataSize() % AES::BLOCKSIZE)
            throw std::runtime_error("legacyDecryptKeys: invalid aes-encrypted key size");

        Id otherParty = (parsedMsg->sender == mOwnHandle)
            ? parsedMsg->receiver
            : parsedMsg->sender;
        return computeSymmetricKey(otherParty)
        .then([this, parsedMsg](const std::shared_ptr<SendKey>& symKey)
        {
            Key<32> iv;
            deriveNonceSecret(parsedMsg->nonce, iv, parsedMsg->receiver);
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
    // Check if sender key is encrypted using RSA.
    if (key->dataSize() < SVCRYPTO_RSA_ENCRYPTION_THRESHOLD)
    {
        if (key->dataSize() % AES::BLOCKSIZE)
            throw std::runtime_error("decryptKey: invalid aes-encrypted key size");

        Id otherParty = (sender == mOwnHandle) ? receiver : sender;
        return computeSymmetricKey(otherParty)
        .then([this, key, receiver](const std::shared_ptr<SendKey>& symmKey)
        {
            // decrypt key
            auto result = std::make_shared<SendKey>();
            result->setDataSize(AES::BLOCKSIZE);
            aesECBDecrypt(*key, *symmKey, *result);
            return result;
        });
    }
    else
    {
        // decrypt key using RSA
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
    key.decrypt(data.ubuf(), len, output.ubuf(), len);
    uint16_t actualLen = ntohs(output.read<uint16_t>(0));
    assert(actualLen <= myPrivRsaKey.dataSize());
    memmove(output.buf(), output.buf()+2, actualLen);
    output.setDataSize(actualLen);
}

promise::Promise<std::pair<MsgCommand*, KeyCommand*>>
ProtocolHandler::msgEncrypt(Message* msg, MsgCommand* msgCmd)
{
    assert(msg->keyid == msgCmd->keyId());
    if (msg->keyid == CHATD_KEYID_INVALID) //we have to use the current send key
    {
        if (!mCurrentKey || mParticipantsChanged)
        {
            return updateSenderKey()
            .then([this, msg, msgCmd](std::pair<KeyCommand*,
                  std::shared_ptr<SendKey>> result) mutable
            {
                msg->keyid = CHATD_KEYID_UNCONFIRMED;
                msgCmd->setKeyId(CHATD_KEYID_UNCONFIRMED);
                msgEncryptWithKey(*msg, *msgCmd, *result.second);
                return std::make_pair(msgCmd, result.first);
            });
        }
        else //use current key
        {
            msg->keyid = mCurrentKeyId;
            msgCmd->setKeyId(mCurrentKeyId);
            msgEncryptWithKey(*msg, *msgCmd, *mCurrentKey);
            return std::make_pair(msgCmd, (KeyCommand*)nullptr);
        }
    }
    else //use a key specified in msg->keyid
    {
        return getKey(UserKeyId(mOwnHandle, msg->keyid))
        .then([this, msg, msgCmd](const std::shared_ptr<SendKey>& key)
        {
            msgEncryptWithKey(*msg, *msgCmd, *key);
            return std::make_pair(msgCmd, (KeyCommand*)nullptr);
        });
    }
}

promise::Promise<Message*>
ProtocolHandler::legacyMsgDecrypt(const std::shared_ptr<ParsedMessage>& parsedMsg, Message* msg)
{
    legacyExtractKeys(parsedMsg);
    return getKey(UserKeyId(msg->userid, msg->keyid))
    .then([this, parsedMsg, msg](const std::shared_ptr<SendKey>& key) ->promise::Promise<Message*>
    {
        if (!parsedMsg->payload.empty())
        {
            symmetricDecryptMessage(
                std::string(parsedMsg->payload.buf(), parsedMsg->payload.size()),
                *key, parsedMsg->nonce, *msg);
        }
        else
        {
            msg->clear();
        }
        return msg;
    });
}

promise::Promise<Message*> ProtocolHandler::handleManagementMessage(
        const std::shared_ptr<ParsedMessage>& parsedMsg, Message* msg)
{
    switch(parsedMsg->type)
    {
        case SVCRYPTO_MSGTYPE_ALTER_PARTICIPANTS:
        {
            //ParsedMessage filled the payload with generated app-level info about participant change
            msg->takeFrom(std::move(parsedMsg->payload));
            return msg;
        }
        //TODO: Add TRUNCATE support
        default:
            return promise::Error("Unknown management message type "+
                std::to_string(parsedMsg->type), EINVAL, SVCRYPTO_ERRTYPE);
    }
}


//We should have already received and decrypted the key in advance
//(which is also async). This will have fetched the public Cu25519 key of
//the peer (unless the key was encrypted using RSA), but we still need the
//Ed25519 key for signature verification, which would not be fetched when the key
//is decrypted.
Promise<Message*> ProtocolHandler::msgDecrypt(Message* message)
{
    if (message->empty())
        return Promise<Message*>(message);

    auto parsedMsg = std::make_shared<ParsedMessage>(*message);

    if (message->userid == API_USER)
        return handleManagementMessage(parsedMsg, message);

    printf("msgDecrypt: we have the following keys:\n");
    for (auto& k: mKeys)
    {
        printf("key %d\n", k.first.key);
    }
    // Get sender key.
    struct Context
    {
        std::shared_ptr<SendKey> sendKey;
        EcKey edKey;
    };
    auto ctx = std::make_shared<Context>();

    auto symPms = getKey(UserKeyId(message->userid, message->keyid))
    .then([ctx](const std::shared_ptr<SendKey>& key)
    {
        ctx->sendKey = key;
    });

    auto edPms = mUserAttrCache.getAttr(parsedMsg->sender,
            ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY)
    .then([ctx](Buffer* key)
    {
        ctx->edKey.assign(key->buf(), key->dataSize());
    });

    return promise::when(symPms, edPms)
    .then([this, message, parsedMsg, ctx]() ->promise::Promise<Message*>
    {
        if (!parsedMsg->verifySignature(ctx->edKey, *ctx->sendKey))
        {
            return promise::Error("Signature invalid for message "+
                message->id().toString(), EINVAL, SVCRYPTO_ERRTYPE);
        }
        STRONGVELOPE_LOG_DEBUG("SIGNATURE VERIFICATION OF MESSAGE FROM %s SUCCESSFUL!", message->userid.toString().c_str());

        if (parsedMsg->protocolVersion <= 1)
        {
            return legacyMsgDecrypt(parsedMsg, message);
        }

        // Decrypt message payload.
        if (!parsedMsg->payload.empty())
        {
            symmetricDecryptMessage(std::string(parsedMsg->payload.buf(), parsedMsg->payload.size()),
                *ctx->sendKey, parsedMsg->nonce, *message);
        }
        else
        {
            STRONGVELOPE_LOG_DEBUG("Message %s has no payload", message->id().toString().c_str());
            message->clear();
        }
        return message;
    });
}

Promise<void>
ProtocolHandler::legacyExtractKeys(const std::shared_ptr<ParsedMessage>& parsedMsg)
{
    if (parsedMsg->encryptedKey.empty())
        return promise::Error("legacyExtractKeys: No encrypted keys found in parsed message", EPROTO, SVCRYPTO_ERRTYPE);

    auto& key1 = mKeys[UserKeyId(parsedMsg->sender, parsedMsg->keyId)];
    key1.pms.reset(new Promise<std::shared_ptr<SendKey>>);
    if (parsedMsg->prevKeyId)
    {
        auto& key2 = mKeys[UserKeyId(parsedMsg->sender, parsedMsg->prevKeyId)];
        key2.pms.reset(new Promise<std::shared_ptr<SendKey>>);
    }
    return legacyDecryptKeys(parsedMsg)
    .then([this, parsedMsg](const std::shared_ptr<Buffer>& keys)
    {
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

void ProtocolHandler::onKeyReceived(uint32_t keyid, Id sender, Id receiver,
                                    const char* data, uint16_t dataLen)
{
    auto encKey = std::make_shared<Buffer>(data, dataLen);
    auto pms = decryptKey(encKey, sender, receiver);
    if (pms.done() == pms.PROMISE_RESOLV_SUCCESS)
    {
        addDecryptedKey(UserKeyId(sender, keyid), pms.value());
        return;
    }
    auto& entry = mKeys[UserKeyId(sender, keyid)];
    STRONGVELOPE_LOG_DEBUG("onKeyReceived: Created a key entry with promise for key %d of user %s", keyid, sender.toString().c_str());
    if (entry.pms)
    {
        STRONGVELOPE_LOG_WARNING("Key % from user %s is already being decrypted", keyid, sender.toString().c_str());
        return;
    }
    entry.pms.reset(new Promise<std::shared_ptr<SendKey>>);
    pms.then([this, sender, keyid](const std::shared_ptr<SendKey>& key)
    {
        //addDecryptedKey will remove entry.pms, but anyone that has already
        //attached to it will be notified
        addDecryptedKey(UserKeyId(sender, keyid), key);
    });
    pms.fail([this, sender, keyid](const promise::Error& err)
    {
        STRONGVELOPE_LOG_ERROR("Removing key entry for key %d - decryptKey() failerd with error '%s'", keyid, err.what());
        auto it = mKeys.find(UserKeyId(sender, keyid));
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
    STRONGVELOPE_LOG_DEBUG("Adding key %d of user %s", ukid.key, ukid.user.toString().c_str());
    auto& entry = mKeys[ukid];
    if (entry.key)
    {
        if (memcmp(entry.key->buf(), key->buf(), SVCRYPTO_KEY_SIZE))
            throw std::runtime_error("onKeyReceived: Key with id "+std::to_string(ukid.key)+" from user '"+ukid.user.toString()+"' already known but different");

        STRONGVELOPE_LOG_DEBUG("onKeyReceived: Key %d from user %s already known and is same", ukid.key, ukid.user.toString().c_str());
    }
    else
    {
        entry.key = key;
    }
    if (entry.pms)
    {
        entry.pms->resolve(entry.key);
        entry.pms.reset();
    }
}
promise::Promise<std::shared_ptr<SendKey>> ProtocolHandler::getKey(UserKeyId ukid)
{
    auto kit = mKeys.find(ukid);
    if (kit == mKeys.end())
        return promise::Error("Key with id "+std::to_string(ukid.key)+
            " from user "+ukid.user.toString()+" not found", ENOENT, SVCRYPTO_ERRTYPE);
    auto& entry = kit->second;
    auto key = entry.key;
    if (key)
    {
        return key;
    }
    else if (entry.pms)
    {
        return (*entry.pms);
    }
    else
    {
        assert(false && "Key found but no promise not key member are set");
    }
}

void ProtocolHandler::onKeyConfirmed(uint32_t keyxid, uint32_t keyid)
{
    if (!mCurrentKey || (mCurrentKeyId != CHATD_KEYID_UNCONFIRMED))
        throw std::runtime_error("strongvelope: setCurrentKeyId: Current send key is not unconfirmed");
    if (keyxid != CHATD_KEYID_UNCONFIRMED)
        throw std::runtime_error("strongvelope: setCurrentKeyId: Usage error: trying to set keyid to the UNOCNFIRMED value");

    mCurrentKeyId = keyid;
    UserKeyId userKeyId(mOwnHandle, keyid);
    assert(mKeys.find(userKeyId) == mKeys.end());
    addDecryptedKey(userKeyId, mCurrentKey);
}

promise::Promise<std::pair<KeyCommand*, std::shared_ptr<SendKey>>>
ProtocolHandler::updateSenderKey()
{
    mCurrentKeyId = CHATD_KEYID_UNCONFIRMED;
    mCurrentKey.reset(new SendKey);
    mCurrentKey->setDataSize(AES::BLOCKSIZE);
    randombytes_buf(mCurrentKey->ubuf(), AES::BLOCKSIZE);
    mParticipantsChanged = false;

	// Assemble the output for all recipients.

    assert(!mParticipants.empty());

    // Users and send key may change while we are getting pubkeys of current
    // users, so make a snapshot
    struct Context
    {
        ProtocolHandler& self;
        SetOfIds users;
        SetOfIds::iterator userIt;
        Promise<std::pair<KeyCommand*, std::shared_ptr<SendKey>>>
            pms;
        std::shared_ptr<SendKey> sendKey;
        std::unique_ptr<KeyCommand> keyCmd;
        void next()
        {
            if (userIt == users.end())
            {
                pms.resolve(std::make_pair(keyCmd.release(), sendKey));
                delete this;
                return;
            }
            self.encryptKeyTo(sendKey, *userIt)
            .then([this](const std::shared_ptr<Buffer>& encryptedKey)
            {
                assert(encryptedKey);
                keyCmd->addKey(*userIt, encryptedKey->buf(), encryptedKey->dataSize());

                userIt++;
                next();
            })
            .fail([this](const promise::Error& err)
            {
                pms.reject(err);
                delete this;
            });
        }
        Context(ProtocolHandler& aSelf)
        : self(aSelf), users(self.mParticipants), userIt(users.begin()),
          sendKey(self.mCurrentKey), keyCmd(new KeyCommand){}
    };
    auto context = new Context(*this);
    auto pms = context->pms; //after next() the context may already be deleted
    context->next();
    return pms;
}

void ProtocolHandler::onUserJoin(Id userid, chatd::Priv priv)
{
    mParticipants.insert(userid);
    mCurrentKey.reset(); //just in case
    mParticipantsChanged = true;
}

void ProtocolHandler::onUserLeave(Id userid)
{
    mParticipants.erase(userid);
    mCurrentKey.reset(); //just in case
    mParticipantsChanged = true;
}
}

