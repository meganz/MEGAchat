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
    // TLV_TYPE_OWN_KEY         = 0x0a, deprecated (used for legacy encryption)
    TLV_TYPE_INVITOR            = 0x0b,
    TLV_TYPE_PRIVILEGE          = 0x0c,
    TLV_TYPE_KEYBLOB            = 0x0f,
    TLV_TYPE_OPENMODE           = 0x10,
    TLV_TYPE_SCHED_ID           = 0x11,
    TLV_TYPE_SCHED_CHANGESET    = 0x12,
    TLV_TYPES_COUNT
};

/** Message types used for the chat message transport. */
enum: unsigned char
{
    ///Message using an existing sender key for encryption.
    //SVCRYPTO_MSGTYPE_KEYED                   = 0x00, deprecated (used for legacy encryption)
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
typedef Key<16> UnifiedKey;
typedef Key<64> Signature;

class ProtocolHandler;
/** Class to parse an encrypted message and store its attributes and content */
struct ParsedMessage: public karere::DeleteTrackable
{
    ProtocolHandler& mProtoHandler;
    uint8_t protocolVersion;
    karere::Id sender;
    Key<32> nonce;
    Buffer payload;
    Buffer signedContent;
    Buffer signature;
    unsigned char type;
    karere::Id mActionId;

    /** True when the message is posted in open mode. It allows to decrypt the `ct` of management
     * messages related to topic changes, which must use unified-key instead of embedded key in `ct` */
    bool publicChatMode = false;

    uint32_t keyId;
    uint32_t prevKeyId;
    Buffer encryptedKey; //may contain also the prev key, concatenated

    std::unique_ptr<chatd::Message::ManagementInfo> managementInfo;
    std::unique_ptr<chatd::Message::CallEndedInfo> callEndedInfo;

    ParsedMessage(const chatd::Message& src, ProtocolHandler& protoHandler);
    bool verifySignature(const StaticBuffer& pubKey, const SendKey& sendKey);
    void parsePayload(const StaticBuffer& data, chatd::Message& msg);
    void parsePayloadWithUtfBackrefs(const StaticBuffer& data, chatd::Message& msg);
    void symmetricDecrypt(const StaticBuffer& key, chatd::Message& outMsg);
    promise::Promise<chatd::Message*> decryptChatTitle(chatd::Message* msg, bool msgCanBeDeleted);
};


enum
{
    SVCRYPTO_NONCE_SIZE = 12,
    SVCRYPTO_KEY_SIZE = 16,
    SVCRYPTO_IV_SIZE = 16,
    /** Size in bytes of a key ID. */
    SVCRYPTO_KEY_ID_SIZE = 4,
    /** Version 0 of the protocol implemented. */
    SVCRYPTO_PROTOCOL_VERSION = 0x03,
    /** Size (in bytes) of the symmetric send key */
    SVCRYPTO_SEND_KEY_SIZE = 16,
};

enum
{
    CHAT_MODE_PRIVATE = 0,
    CHAT_MODE_PUBLIC = 1
};

enum
{
    kDecrypted = 0,
    kEncrypted = 1,
    kUndecryptable = 2
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

/**
 * @brief The UserKeyId struct is used to identify keys used for encrypted messages.
 * For each chat, every user has its own set of keyids that used to send messages.
 */
struct UserKeyId
{
    karere::Id user;
    uint32_t keyid;
    explicit UserKeyId(const karere::Id& aUser, uint32_t aKeyid): user(aUser), keyid(aKeyid){}
    bool operator<(UserKeyId other) const
    {
        if (user != other.user)
            return user < other.user;
        else
            return keyid < other.keyid;
    }
};

class TlvWriter;
extern const std::string SVCRYPTO_PAIRWISE_KEY;
void deriveSharedKey(const StaticBuffer& sharedSecret, SendKey& output, const std::string& padString=SVCRYPTO_PAIRWISE_KEY);

/**
 * @brief The ProtocolHandler class implements ICrypto.
 * @see chatd::ICrypto for more details.
 */
class ProtocolHandler: public chatd::ICrypto, public karere::DeleteTrackable
{
protected:
    /**
     * @brief The KeyEntry struct represents a Key in the map of keys (mKeys)
     * If the received key is still encrypted (because the required public key
     * has to be fetched from API), then a promise will be attached to this KeyEntry.key
     * Such promise will be resolved once the key is successfully decrypted.
     * If decryption fails, then the promise will be rejected.
     * For new keys getting confirmed, the promise is never used.
     */
    struct KeyEntry
    {
        std::shared_ptr<SendKey> key;
        std::shared_ptr<promise::Promise<std::shared_ptr<SendKey>>> pms;
        KeyEntry(){}
        KeyEntry(const std::shared_ptr<SendKey>& aKey): key(aKey){}
    };

    // own keys
    karere::Id mOwnHandle;
    EcKey myPrivCu25519;
    EcKey myPrivEd25519;
    EcKey myPubEd25519;

    karere::UserAttrCache& mUserAttrCache;
    SqliteDb& mDb;

    // current key, keyid and userlist
    std::shared_ptr<SendKey> mCurrentKey;
    chatd::KeyId mCurrentKeyId = CHATD_KEYID_INVALID;
    karere::SetOfIds mCurrentKeyParticipants;
    // Shared transactional keyxid among all the chats in the same chatd-shard
    static chatd::KeyId mCurrentLocalKeyId;

    /**
     * @brief The NewKeyEntry struct represents a Key in the list of unconfirmed keys (mUnconfirmedKeys)
     * Each key, created for an specific set of participants, is used to encrypt one or more
     * new messages and their updates (NEWMSG + [MSGUPDX]*)
     */
    struct NewKeyEntry
    {
        NewKeyEntry(const std::shared_ptr<SendKey>& aKey, karere::SetOfIds aRecipients, chatd::KeyId aLocalKeyid);

        std::shared_ptr<SendKey> key;
        karere::SetOfIds recipients;
        chatd::KeyId localKeyid; // local keyid --> rowid in the sending table containing the NewKey
    };
    // in-fligth new-keys
    std::vector<NewKeyEntry> mUnconfirmedKeys;

    // received and confirmed keys (doesn't include unconfirmed keys)
    std::map<UserKeyId, KeyEntry> mKeys;

    // cache of symmetric keys (pubCu255 * privCu255)
    std::map<karere::Id, std::shared_ptr<SendKey>> mSymmKeyCache;

    // current list of participants (mapped to the `chatd::Client::mUsers`)
    karere::SetOfIds* mParticipants = nullptr;

    unsigned int mCacheVersion = 0; // updated if history is reloaded
    unsigned int mChatMode = CHAT_MODE_PRIVATE;
    std::shared_ptr<UnifiedKey> mUnifiedKey;
    promise::Promise<std::shared_ptr<UnifiedKey>> mUnifiedKeyDecrypted;
    bool mHasUnifiedKey; // indicates if chat has unified key (although it's pending to be decrypted)

public:
    karere::Id chatid;
    karere::Id mPh = karere::Id::inval();     // it's only valid during preview mode (required to fetch user-attributes)
    const karere::Id& ownHandle() const { return mOwnHandle; }
    promise::Promise<std::shared_ptr<UnifiedKey>> unifiedKey() { return mUnifiedKeyDecrypted; }

    ProtocolHandler(const karere::Id& ownHandle, const StaticBuffer& privCu25519,
        const StaticBuffer& privEd25519,
        karere::UserAttrCache& userAttrCache,
        SqliteDb& db, const karere::Id& aChatId, bool isPublic, std::shared_ptr<std::string> unifiedKey,
        int isUnifiedKeyEncrypted, const karere::Id& ph, void *ctx);

    promise::Promise<std::shared_ptr<SendKey>> //must be public to access from ParsedMessage
        decryptKey(std::shared_ptr<Buffer>& key, const karere::Id& sender, const karere::Id& receiver);

    unsigned int getCacheVersion() const;

protected:
    void loadKeysFromDb();

    /**
     * @brief Load unconfirmed keys stored in cache
     *
     * Upon resumption from cache, if there were unconfirmed keys in-flight (NEWKEY's attached to
     * NEWMSGs that are in the sending queue, already encrypted in their KeyCommand+MsgCommand shape),
     * strongvelope should know them in order to properly confirm them when onKeyConfirmed() is called
     */
    void loadUnconfirmedKeysFromDb();

    promise::Promise<std::shared_ptr<SendKey>> getKey(UserKeyId ukid);
    void addDecryptedKey(UserKeyId ukid, const std::shared_ptr<SendKey>& key);
    /**
     * Updates our own sender key. Done when a message is sent and users
     * have changed since the last message was sent, or when the first
     * message is sent.
     */
    promise::Promise<std::pair<chatd::KeyCommand*, std::shared_ptr<SendKey>>>
    createNewKey(const karere::SetOfIds &recipients);

    chatd::KeyId createLocalKeyId();

    /**
     * @brief Signs a message using EdDSA with the Ed25519 key pair.
     * @param message Message to sign.
     * @param keyToInclude The symmetric key with which the message was signed,
     *     to be include into the signature
     * @param signature [out] Message signature.
     */
    void signMessage(const StaticBuffer& msg, uint8_t protoVersion, uint8_t msgType,
        const SendKey& msgKey, StaticBuffer& signature);
    /**
     * Derives a symmetric key for encrypting a message to a contact.  It is
     * derived using a Curve25519 key agreement.
     *
     * Note: The Curve25519 key cache must already contain the public key of
     *       the recipient.
     */
    promise::Promise<std::shared_ptr<SendKey>>
    computeSymmetricKey(const karere::Id& userid, const std::string& padString=SVCRYPTO_PAIRWISE_KEY);

    promise::Promise<std::shared_ptr<Buffer>>
        encryptKeyTo(const std::shared_ptr<SendKey>& sendKey, const karere::Id& toUser);

    promise::Promise<std::pair<chatd::KeyCommand*, std::shared_ptr<SendKey>>>
    encryptKeyToAllParticipants(const std::shared_ptr<SendKey>& key, const karere::SetOfIds &participants, chatd::KeyId localkeyid);

    promise::Promise<std::string> encryptUnifiedKeyToUser(const karere::Id& user) override;

    void msgEncryptWithKey(const chatd::Message &src, chatd::MsgCommand& dest, const StaticBuffer& key);

    promise::Promise<chatd::Message*> handleManagementMessage(
        const std::shared_ptr<ParsedMessage>& parsedMsg, chatd::Message* msg);

    /**
     * @brief Getter method to local static variable for temporal.
     *
     * Id will be decreasing as used and start from CHATD_KEYID_MAX when it reaches CHATD_KEYID_MIN.
     */
    chatd::KeyId getNextValidLocalKeyId();

public:
//chatd::ICrypto interface
    promise::Promise<std::pair<chatd::MsgCommand*, chatd::KeyCommand*>>
    msgEncrypt(chatd::Message *message, const karere::SetOfIds &recipients, chatd::MsgCommand* msgCmd) override;
    promise::Promise<chatd::Message*> msgDecrypt(chatd::Message* message) override;
    void onKeyReceived(chatd::KeyId keyid, karere::Id sender,
        karere::Id receiver, const char* data, uint16_t dataLen, bool isEncrypted) override;
    void onKeyConfirmed(chatd::KeyId localkeyid, chatd::KeyId keyid) override;
    void onKeyRejected() override;
    void setUsers(karere::SetOfIds* users) override;
    void onUserJoin(karere::Id userid) override;
    void onUserLeave(karere::Id userid) override;
    void resetSendKey() override;
    void randomBytes(void* buf, size_t bufsize) const override;
    promise::Promise<std::shared_ptr<Buffer>> encryptChatTitle(const std::string& data, uint64_t extraUser = 0, bool encryptAsPrivate = false) override;
    promise::Promise<chatd::KeyCommand*> encryptUnifiedKeyForAllParticipants(uint64_t extraUser = 0) override;

    promise::Promise<std::string> decryptChatTitleFromApi(const Buffer& data) override;

    promise::Promise<std::string>
    decryptUnifiedKey(std::shared_ptr<Buffer>& key, uint64_t sender, uint64_t receiver) override;
    static Buffer* createUnifiedKey();
    promise::Promise<std::shared_ptr<std::string> > getUnifiedKey() override;
    bool previewMode() override;
    bool isPublicChat() const override;
    void setPrivateChatMode() override;
    void onHistoryReload() override;
    uint64_t getPublicHandle() const override;
    void setPublicHandle(const uint64_t ph) override;
    karere::UserAttrCache& userAttrCache() override;

    void fetchUserKeys(karere::Id userid) override;
    std::shared_ptr<Buffer> reactionEncrypt(const chatd::Message &msg, const std::string &reaction) override;
    promise::Promise<std::shared_ptr<Buffer>> reactionDecrypt(const karere::Id &msgid, const karere::Id &userid, const chatd::KeyId &keyid, const std::string &reaction) override;
};
}
namespace chatd
{
    std::string managementInfoToString(const chatd::Message& msg);
}
#endif /* STRONGVELOPE_H_ */
