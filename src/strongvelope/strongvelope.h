/*
 * strongvelope.h
 *
 *  Created on: 17/11/2015
 *      Author: admin2
 */

#ifndef STRONGVELOPE_H_
#define STRONGVELOPE_H_
#include <vector>
#include <map>
#include <string>
#include <assert.h>
#include <iostream>
#include <buffer.h>
#include <karereId.h>
#include <chatdMsg.h>
#include <chatdICrypto.h>
#include <promise.h>
#include <logger.h>
#include <karereCommon.h>
#include <base/trackDelete.h>

#define STRONGVELOPE_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_strongvelope, "%s: " fmtString, chatid.toString().c_str(), ##__VA_ARGS__)
#define STRONGVELOPE_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_strongvelope, "%s: " fmtString, chatid.toString().c_str(), ##__VA_ARGS__)
#define STRONGVELOPE_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_strongvelope, "%s: " fmtString, chatid.toString().c_str(), ##__VA_ARGS__)

#define SVCRYPTO_ERRTYPE 0x3e9a5419 //should resemble megasvlp

//NOTE: In C/C++ it should be avoided to have enum constant names with all capital
//letters, because of possible conflicts with macros defined by other libs.
//As enums can be naturally namespaced, their names are usually short
//(i.e. not prefixed with lib name like SVCRYPTO_), so the possibility of a
//conflict with a macro is even greater. For example an enum
//strongelope::TIMESTAMP_SIZE could conflict with a TIMESTAMP_SIZE macro defined
//by some other lib or system headers. Therefore, all-capital constants should
//normally only be used if defined via macros. In that case, if the macro
//is already defined, would result in a preprocessor warning about macro redefinition.
//If that symbol is an enum instead, the preprocessor would silently replace
//it with the macro value, resulting in weird and very hard to diagnose errors.
//If for some reason all-capital enum names have to be used, they should be named
//with the same rules that apply for macro, i.e. should contain
//some unique prefix, like the lib name (i.e. SVCRYPTO_) in order to avoid
//conflicts with macros. This loses the advantage to have short and clean
//constant names.
namespace karere
{
    class UserAttrCache;
}
class SqliteDb;

namespace strongvelope
{
/**
 * "Enumeration" of TLV types used for the chat message transport container.
 *
 * Note: The integer value of the TLV type also indicates the allowed order
 *       within a message encoding. Values must be monotonically increasing
 *       within, but single record types may be skipped. `RECIPIENT` and
 *       `KEYS` are the only types that may (concurrently) be repeated.
 *
 */
enum
{
/** NULL is used as a terminator for the record type. Don't use! */
    TLV_TYPE_UNUSED_SEPARATOR   = 0x00,

/** Signature for all following bytes */
    TLV_TYPE_SIGNATURE          = 0x01,

/** Type of message sent (legacy only) */
    TLV_TYPE_MESSAGE_TYPE       = 0x02,

/** "Base nonce" used for encryption of keys for all users
 * (per-user nonces are derived from it). */
    TLV_TYPE_NONCE              = 0x03,

/** Recipient of message. This record can be repeated for all recipients
    of message. (legacy only) */
    TLV_TYPE_RECIPIENT          = 0x04,

/** Message encryption keys, encrypted to a particular recipient. This
  * may contain two (concatenated) keys. The second one (if present) is
  * the previous sender key. Requires the same number of records in the
  * same order as `RECIPIENT`. (legacy only) */
    TLV_TYPE_KEYS               = 0x05,

/** Sender key IDs used (or set) in this message. (legacy only) */
    TLV_TYPE_KEY_IDS            = 0x06,

/** Encrypted payload of message. */
    TLV_TYPE_PAYLOAD            = 0x07,

/** Contains a 64 bit user handle of a user that was added to chat */
    TLV_TYPE_INC_PARTICIPANT    = 0x08,

/** Contains a 64 bit user handle of a user that was excluded from chat */
    TLV_TYPE_EXC_PARTICIPANT    = 0x09,

/** In case we encrypt keys using RSA, we can't reverse-decrypt them like with
 * EC, so we need to encrypt our own keys yo ourself so we can read our own
 * history messages. (legacy only) */
    TLV_TYPE_OWN_KEY            = 0x0a,
    TLV_TYPE_INVITOR            = 0x0b,
    TLV_TYPE_PRIVILEGE          = 0x0c,
    TLV_TYPE_KEYBLOB            = 0x0f,
    TLV_TYPES_COUNT
};

/** Message types used for the chat message transport. */
enum: unsigned char
{
    ///Legacy message containing a sender key (initial, key rotation, key re-send).
    SVCRYPTO_MSGTYPE_KEYED                     = 0x00,
    ///Message using an existing sender key for encryption.
    SVCRYPTO_MSGTYPE_FOLLOWUP                  = 0x01,
    SVCRYPTO_MSGTYPES_COUNT
};

template <size_t Size>
class Key: public StaticBuffer
{
protected:
    char mData[Size];
public:
    static size_t bufSize() { return Size; }
    void setDataSize(size_t size)
    {
        if (size > Size)
            throw std::runtime_error("Can't resize static buffer beyond its data block size");
        mDataSize = size;
    }
    void assign(const char* src, size_t len)
    {
        if (len > Size)
            throw std::runtime_error("Key::assign: source buffer is larger than our size");
        memcpy(mData, src, len);
        mDataSize = len;
    }
//    Key(const char* src, size_t len): StaticBuffer(mData, 0) { assign(src, len); }
    Key(size_t len=Size)
    {
        //actual data is not initialized, but we need to tell StaticBuffer what is our
        //max size
        assert(len <= Size);
        mBuf = mData;
        mDataSize = len;
    }
    Key(const StaticBuffer& other)
    {
        mBuf = mData;
        assign(other.buf(), other.dataSize());
    }
    Key(const char* data, size_t len)
    {
        assert(len == Size);
        mBuf = mData;
        assign(data, len);
    }
};
typedef Key<16> SendKey;
typedef Key<32> EcKey;

class ProtocolHandler;
/** Class to parse an encrypted message and store its attributes and content */
struct ParsedMessage: public chatd::Message::ManagementInfo, public karere::DeleteTrackable
{
    ProtocolHandler& mProtoHandler;
    uint8_t protocolVersion;
    karere::Id sender;
    Key<32> nonce;
    Buffer payload;
    Buffer signedContent;
    Buffer signature;
    unsigned char type;
    chatd::BackRefId backRefId = 0;
    std::vector<chatd::BackRefId> backRefs;
    //legacy key stuff
    uint64_t keyId;
    uint64_t prevKeyId;
    Buffer encryptedKey; //may contain also the prev key, concatenated
    ParsedMessage(const chatd::Message& src, ProtocolHandler& protoHandler);
    bool verifySignature(const StaticBuffer& pubKey, const SendKey& sendKey);
    void parsePayload(const StaticBuffer& data, chatd::Message& msg);
    void parsePayloadWithUtfBackrefs(const StaticBuffer& data, chatd::Message& msg);
    void symmetricDecrypt(const StaticBuffer& key, chatd::Message& outMsg);
    promise::Promise<chatd::Message*> decryptChatTitle(chatd::Message* msg);
};


enum
{
    SVCRYPTO_NONCE_SIZE = 12,
    SVCRYPTO_KEY_SIZE = 16,
    SVCRYPTO_IV_SIZE = 16,
    /** Size in bytes of a key ID. */
    SVCRYPTO_KEY_ID_SIZE = 8,
    /** Size in bytes of a key ID in version 0 */
    SVCRYPTO_KEY_ID_SIZE_V0 = 4,
    /** Size threshold for RSA encrypted sender keys (greater than ... bytes).
     * (1024 bit RSA key --> 128 byte + 2 byte cipher text).
     */
    SVCRYPTO_RSA_ENCRYPTION_THRESHOLD = 128,
    /** Version 0 of the protocol implemented. */
    SVCRYPTO_PROTOCOL_VERSION = 0x03,
    /** Size (in bytes) of the symmetric send key */
    SVCRYPTO_SEND_KEY_SIZE = 16,
};

/** @brief Encrypts and holds an encrypted message and its attributes - key and
 *  nonce */
struct EncryptedMessage
{
    std::string ciphertext; //crypto++ does not support binary buffers in AES CTR mode, which is used for encrypting messages
    SendKey key;
    chatd::BackRefId backRefId;
    Key<SVCRYPTO_NONCE_SIZE> nonce;
    EncryptedMessage(const chatd::Message& msg, const StaticBuffer& aKey);
};

struct UserKeyId
{
    karere::Id user;
    uint64_t key;
    explicit UserKeyId(karere::Id aUser, uint64_t aKey): user(aUser), key(aKey){}
    bool operator<(UserKeyId other) const
    {

        if (user != other.user)
            return user < other.user;
        else
            return key < other.key;
    }
};

class TlvWriter;

class ProtocolHandler: public chatd::ICrypto, public karere::DeleteTrackable
{
protected:
    karere::Id mOwnHandle;
    EcKey myPrivCu25519;
    EcKey myPrivEd25519;
    EcKey myPubEd25519;
    Key<768> myPrivRsaKey;
    karere::UserAttrCache& mUserAttrCache;
    uint32_t mCurrentKeyId = CHATD_KEYID_INVALID;
    SqliteDb& mDb;
    std::shared_ptr<SendKey> mCurrentKey;
    // When we generate a new key, it may not get sent successfully if the connection
    // gets broken. So we need to send it again upon re-login, until it gets confirmed.
    std::shared_ptr<chatd::KeyCommand> mUnconfirmedKeyCmd;
    bool mForceRsa = false;
    struct KeyEntry
    {
        std::shared_ptr<SendKey> key;
        std::shared_ptr<promise::Promise<std::shared_ptr<SendKey>>> pms;
        KeyEntry(){}
        KeyEntry(const std::shared_ptr<SendKey>& aKey): key(aKey){}
    };
    std::map<UserKeyId, KeyEntry> mKeys;
    std::map<karere::Id, std::shared_ptr<SendKey>> mSymmKeyCache;
    karere::SetOfIds* mParticipants = nullptr;
    bool mParticipantsChanged = true;
    bool mIsDestroying = false;
public:
    karere::Id chatid;
    karere::Id ownHandle() const { return mOwnHandle; }
    ProtocolHandler(karere::Id ownHandle, const StaticBuffer& PrivCu25519,
        const StaticBuffer& PrivEd25519,
        const StaticBuffer& privRsa, karere::UserAttrCache& userAttrCache,
        SqliteDb& db, karere::Id aChatId, void *ctx);
protected:
    void loadKeysFromDb();
    promise::Promise<std::shared_ptr<SendKey>> getKey(UserKeyId ukid, bool legacy=false);
    void addDecryptedKey(UserKeyId ukid, const std::shared_ptr<SendKey>& key);
        /**
         * Updates our own sender key. Done when a message is sent and users
         * have changed since the last message was sent, or when the first
         * message is sent.
         */
    promise::Promise<std::pair<chatd::KeyCommand*, std::shared_ptr<SendKey>>>
    updateSenderKey();

        /**
         * @brief Signs a message using EdDSA with the Ed25519 key pair.
         * @param message Message to sign.
         * @param keyToInclude The symmetric key with which the message was signed,
         *     to be include into the signature
         * @param signature [out] Message signature.
         */
    void signMessage(const StaticBuffer& msg,
         uint8_t protoVersion, uint8_t msgType, const SendKey& msgKey,
        StaticBuffer& signature);
        /**
          * Derives a symmetric key for encrypting a message to a contact.  It is
          * derived using a Curve25519 key agreement.
          *
          * Note: The Curve25519 key cache must already contain the public key of
          *       the recipient.
          *
          */
    promise::Promise<std::shared_ptr<SendKey> > computeSymmetricKey(karere::Id userid);

    promise::Promise<std::shared_ptr<Buffer>>
        encryptKeyTo(const std::shared_ptr<SendKey>& sendKey, karere::Id toUser);
    promise::Promise<std::pair<chatd::KeyCommand*, std::shared_ptr<SendKey>>>
    encryptKeyToAllParticipants(const std::shared_ptr<SendKey>& key, uint64_t extraUser=0);

    void msgEncryptWithKey(chatd::Message &src, chatd::MsgCommand& dest,
        const StaticBuffer& key);
    promise::Promise<chatd::Message*> handleManagementMessage(
        const std::shared_ptr<ParsedMessage>& parsedMsg, chatd::Message* msg);
    chatd::Message* legacyMsgDecrypt(const std::shared_ptr<ParsedMessage>& parsedMsg,
        chatd::Message* msg, const SendKey& key);

    promise::Promise<std::shared_ptr<Buffer>>
        rsaEncryptTo(const std::shared_ptr<StaticBuffer>& data, karere::Id toUser);

    void rsaDecrypt(const StaticBuffer& data, Buffer& output);

    promise::Promise<std::shared_ptr<Buffer>>
        legacyDecryptKeys(const std::shared_ptr<ParsedMessage>& parsedMsg);

    /** @brief Extract keys from a legacy message */
    promise::Promise<void>
        legacyExtractKeys(const std::shared_ptr<ParsedMessage>& parsedMsg);
public:
//chatd::ICrypto interface
        promise::Promise<std::pair<chatd::MsgCommand*, chatd::KeyCommand*>>
            msgEncrypt(chatd::Message *message, chatd::MsgCommand* msgCmd);
        virtual promise::Promise<chatd::Message*> msgDecrypt(chatd::Message* message);
        virtual void onKeyReceived(uint32_t keyid, karere::Id sender,
            karere::Id receiver, const char* data, uint16_t dataLen);
        virtual void onKeyConfirmed(uint32_t keyxid, uint32_t keyid);
        virtual void setUsers(karere::SetOfIds* users);
        virtual void onUserJoin(karere::Id userid);
        virtual void onUserLeave(karere::Id userid);
        virtual void resetSendKey();
        virtual bool handleLegacyKeys(chatd::Message& msg);
        virtual void randomBytes(void* buf, size_t bufsize) const;
        virtual promise::Promise<std::shared_ptr<Buffer>> encryptChatTitle(const std::string& data, uint64_t extraUser=0);
        virtual promise::Promise<std::string> decryptChatTitle(const Buffer& data);
        virtual const chatd::KeyCommand* unconfirmedKeyCmd() const { return mUnconfirmedKeyCmd.get(); }

        //====
        promise::Promise<std::shared_ptr<SendKey>>
            decryptKey(std::shared_ptr<Buffer>& key, karere::Id sender, karere::Id receiver);

    };
}
namespace chatd
{
    std::string managementInfoToString(const chatd::Message& msg);
}
#endif /* STRONGVELOPE_H_ */
