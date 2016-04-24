#ifndef ICRYPTO_H
#define ICRYPTO_H
namespace chatd
{
class Chat;
class ICrypto
{
public:
    void init(Chat& messages) {}
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
 * keyid of currentSendKey will be set to Key::kUnconfirmedId (0xffffffff).
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
 * that message instead of the 0xffffffff keyid.
 */
    virtual bool msgEncrypt(const Message& msg, MsgCommand& cmd)
    {
        cmd.setMsg(msg.buf(), msg.dataSize());
        return true;
    }
/**
 * @brief Called by the client for received messages to decrypt them.
 * The crypto module \b must also set the type of the message, so that the client
 * knows whether to pass it to the application (i.e. contains an actual message)
 * or should not (i.e. contains a crypto system packet)
 */
    virtual void msgDecrypt(Message& src)
    { //test implementation
    }
    virtual Key* keyDecrypt(Id userid, uint16_t keylen, const char* keybuf)
    {
        return new Key(0);
    }
/**
 * @brief The chatroom connection (to the chatd server shard) state state has changed.
 */
    virtual void onOnlineStateChange(ChatState state){}
/**
 * @brief A user has joined or left the room, or their privilege has changed
 * @param privilege - the new privilege, if it is PRIV_NOTPRESENT, then the user
 * left the chat
 */
    virtual void onUserJoinLeave(Id userid, Priv privilege){}
/**
 * @brief A key was received from the server, and added to Chat.keys
 */
    virtual void onKeyReceived(const KeyWithId& key){}
/**
 * @brief The crypto module is destroyed when that chatid is left or the client is destroyed
 */
    virtual ~ICrypto(){}
};
}
#endif // ICRYPTO_H

