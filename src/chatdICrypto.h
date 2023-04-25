#ifndef ICRYPTO_H
#define ICRYPTO_H
#include <timers.hpp>
#include <promise.h>
#include <chatd.h>

namespace chatd
{
enum
{
    SVCRYPTO_ERRTYPE = 0x3e9ac910,  ///< all promise errors originating from strongvelope should have this type
    SVCRYPTO_ENOKEY = 1,            ///< Can't decrypt because we can't obtain the decrypt key. May occur if a message was sent just after a user joined
    SVCRYPTO_ENOMSG = 2,            ///< Can't decrypt because message has been deleted during decryption process
    SVCRYPTO_ENOTYPE = 3,           ///< Type of message not recognized by the client
    SVCRYPTO_EEXPIRED = 4,          ///< Strongvelope instance was deleted
    SVCRYPTO_ESIGNATURE = 5,        ///< Verification of signature failed
    SVCRYPTO_EMALFORMED = 6         ///< Failed to parse message, invalid format or corrupted data
};

class Chat;
class ICrypto
{
    void *appCtx;
    
public:
    ICrypto(void *ctx) : appCtx(ctx) {}
    
    virtual void setUsers(karere::SetOfIds* users) = 0;

    /**
     * @brief msgEncrypt Encrypts a message, putting the contents in the specified
     * \c msgCommand object.
     * @param msg The message to encrypt. If msg.keyid is not 0, then it must be encrypted
     * with the key specified by that keyid. If it is 0, then it's up to the crypto module
     * to choose the key, and it must set msg.keyid to the id of the key used. It may decide
     * create a new send key, by calling Chat.setNewSendKey() with a newly generated key.
     * @param cmd The MsgCommand object that will be sent for that message. The
     * command object is fully configured with userid, chatid etc, and with a zero-length
     * message - i.e. the message length field is set to zero, and there is no message data.
     * When the encrypted message data is generated, setMsg() must be called on the
     * msgCommand object with the actual encrypted message data and the length of that data.
     * The keyid field of the command object is the same as msg.keyid when the method
     * is called, and if msg.keyid is 0 (i.e. it has to be set by this method),
     * it must also update the command's keyid to the same value - the keyids of the Message
     * and MsgCommand object must always be in sync.
     * The current encryption key is kept in \c Chat.currentSendKey. If the crypto module
     * generates a new key, it must call \c Chat.setNewSendKey() with the new key, which
     * will immediately post the unconfirmed key (i.e. with no keyid yet) to the chat. The
     * keyid of currentSendKey will be set to Key::kUnconfirmedId (0xfffffffe).
     * Upon key confirmation from the server, the id of currentKeyId will be updated to the
     * server-assigned keyid. The crypto module should not care about the key's id,
     * it should just use whatever \c Chat.currentKeyId.id() is set to.
     * @return Whether the encryption was successful.
     * In case a participant's public key is not immediately available (and needs to be
     * fetched from the API), \c false must be returned. When the key fetch is done and
     * the encrypt operation will be successful, the crypto module must call
     * \c Chat::onCanEncryptAgain(). This will result in \c encrypt() called again
     * for that same message, and for any subsequent messages that may have accumulated
     * in the output queue, until the queue is empty, another(or this) \c msgEncrypt()
     * call return \c false, or the connection goes offline. Upon a subsequent call for the
     * same message, it is not guaranteed that the keyid that this method set in the previous
     * call for the Message and MsgCommand objects will be preserved, so it has to be set
     * again on both objects in case \c msg.keyid == Key::kUnconfirmedId.
     * @note Event if it returns false, this method may still generate a new key and call
     * \c setNewSendKey(). The benefit would be that the server may confirm the key until
     * the pubkey is obtained from the API, thus allowing the usage of a real keyid for
     * that message instead of the 0xfffffffe keyid.
     */
    virtual promise::Promise<std::pair<MsgCommand*, KeyCommand*> >
    msgEncrypt(Message* msg, const karere::SetOfIds &recipients, MsgCommand* cmd) = 0;

    /**
     * @brief Called by the client for received messages to decrypt them.
     * The crypto module \b must also set the type of the message, so that the client
     * knows whether to pass it to the application (i.e. contains an actual message)
     * or should not (i.e. contains a crypto system packet)
     */
    virtual promise::Promise<Message*> msgDecrypt(Message* src) = 0;

    /**
     * @brief The chatroom connection (to the chatd server shard) state state has changed.
     */
    virtual void onOnlineStateChange(ChatState /*state*/){}

    /**
     * @brief A user has joined, or their privilege has changed
     * @param privilege - the new privilege, if it is PRIV_NOTPRESENT, then the user
     * left the chat
     */
    virtual void onUserJoin(karere::Id /*userid*/){}

    /**  @brief A user has left the room */
    virtual void onUserLeave(karere::Id /*userid*/){}

    /**
    * @brief A key was received from the server, and added to Chat.keys
    */
    virtual void onKeyReceived(KeyId keyid, karere::Id sender, karere::Id receiver,
        const char* keydata, uint16_t keylen, bool isEncrypted = true) = 0;

    /**
     * @brief A new key sent to server has been confirmed by the server, and added to Chat.keys
     */
    virtual void onKeyConfirmed(KeyId localkeyid, KeyId keyid)  = 0;

    /**
     * @brief A new key sent to server has been rejected by the server
     */
    virtual void onKeyRejected()  = 0;

    /**
     * @brief Invalidates the current send key, forcing a new send key to be generated
     * and posted on next message encrypt.
     * This is necessary when one or more messages are deleted from the send queue
     * and moved to the manual send list. If the crypto module has generated a new
     * key with one of these messages, that key will never be sent to the server
     * and never confirmed, so all subsequent messages generated by the crypto module
     * will have the unconfirmed keyid (0xfffffffe), and it will never be updated
     * to a real one.
     */
    virtual void resetSendKey() = 0;

    virtual void randomBytes(void* buf, size_t bufsize) const = 0;

    virtual promise::Promise<std::shared_ptr<Buffer>>
    encryptChatTitle(const std::string& data, uint64_t extraUser = 0, bool encryptAsPrivate = false) = 0;

    virtual promise::Promise<chatd::KeyCommand*>
    encryptUnifiedKeyForAllParticipants(uint64_t extraUser=0) = 0;

    virtual promise::Promise<std::string>
    decryptChatTitleFromApi(const Buffer& data) = 0;

    virtual promise::Promise<std::string>
    encryptUnifiedKeyToUser(const karere::Id& user) = 0;

    virtual promise::Promise<std::string>
    decryptUnifiedKey(std::shared_ptr<Buffer>& key, uint64_t sender, uint64_t receiver) = 0;

    virtual promise::Promise<std::shared_ptr<std::string> > getUnifiedKey() = 0;

    virtual bool previewMode() = 0;

    /** Returns true if chat is in public/open mode */
    virtual bool isPublicChat() const = 0;

    virtual void setPrivateChatMode() = 0;

    virtual void onHistoryReload() = 0;

    virtual uint64_t getPublicHandle() const = 0;

    virtual void setPublicHandle(const uint64_t ph) = 0;

    virtual karere::UserAttrCache& userAttrCache() = 0;

    /**
     * @brief Encrypts a reaction with xxtea.
     * @param msg The message associated to the reaction.
     * @param reaction An UTF-8 string.
     */
    virtual std::shared_ptr<Buffer> reactionEncrypt(const Message &msg, const std::string &reaction) = 0;

    /**
     * @brief Decrypts a reaction with xxtea.
     * @param msgid The msgId associated to the reaction.
     * @param userid The userId associated to the reaction.
     * @param keyid The keyId associated to the message.
     * @param reaction The encrypted reaction.
     */
    virtual promise::Promise<std::shared_ptr<Buffer>> reactionDecrypt(const karere::Id &msgid, const karere::Id &userid, const KeyId &keyid, const std::string &reaction) = 0;

    virtual void fetchUserKeys(karere::Id userid) = 0;

    /**
     * @brief The crypto module is destroyed when that chatid is left or the client is destroyed
     */
    virtual ~ICrypto(){}
};
}
#endif // ICRYPTO_H

