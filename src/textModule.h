#ifndef TEXTMODULE_H
#define TEXTMODULE_H
#include "karereEventObjects.h"
#include <memory>
#include <map>
#include <type_traits>
#include "textModuleTypeConfig.h"
#include "chatCommon.h"

namespace strophe {class Connection;}

namespace karere
{
class Client;
class ChatRoom;

class TextModule: public virtual strophe::IPlugin, public TextModuleTypeConfig
{
protected:
    virtual ~TextModule(){}
public:
//==IPlugin interface
    virtual void onConnState(const xmpp_conn_event_t status,
            const int error, xmpp_stream_error_t * const stream_error);
//===
    static_assert(std::is_base_of<TextModuleTypeConfig::MemberType, GroupMemberType>::value,
        "MemberType parameter must be a base of GroupMemberType.");
    static_assert(std::is_base_of<std::shared_ptr<MemberType>, SharedMemberPtrType>::value,
        "SharedMemberPtrType must be a base of std::shared_ptr<MemberType>");
public:
    /** parent karere client */
    karere::Client& mClient;
    /** strophe connection object that scopes the text chat handlers */
    strophe::Connection conn;
    std::map<std::string, std::shared_ptr<ChatRoom>> chatRooms;
    TextModule(karere::Client& client);

protected:
    /**
     * @brief Upon first connect, registers all stanza handlers that are needed. This is done only
     * once, as handlers are not purged upon reconnect
     */
    void registerHandlers();
    /**
    * Compose an invitation message from received invitation Stanza data.
    * @param stanza received stanza data.
    * @returns a smart pointer pointing to an invitation message.
    */
    std::shared_ptr<InviteMessage> composeInviteMessesage(strophe::Stanza stanza);
    void testBusMessage(message_bus::SharedMessage<> &message);
    std::shared_ptr<PresenceMessage> composePresenceMessage(strophe::Stanza stanza);
    /**
    * Compose an incoming message from received Stanza data.
    * @param stanza received stanza data.
    * @returns a smart pointer pointing to an incoming message.
    */
    std::shared_ptr<IncomingMessage> composeIncomingMessesage(strophe::Stanza stanza);
    /**
    * Compose an action message from received Stanza data.
    * @param stanza received stanza data.
    */
    std::shared_ptr<ActionMessage> composeActionMessesage(strophe::Stanza stanza);
    /**
    * Broadcast an action (to all my devices) + optionally to a specific room.
    * @param action name of the action.
    * @param meta meta data.
    * @param toRoomJid optional, JID of the room to send the action.
    * @returns resource of current connection.
    */
    void sendBroadcastAction(const std::string& action, /*const Meta& meta, */const std::string& toRoomJid = "");
public:
    void handleInvitationMessage(std::shared_ptr<InviteMessage> &message);
    void handleDataMessage(std::shared_ptr<IncomingMessage> &message);
    void handleActionMessage(std::shared_ptr<ActionMessage> &message);
    void handlePresenceMessage(std::shared_ptr<PresenceMessage> &message);
    void addOtherUser(const std::string &roomId, const std::string &otherJid);
    /**
    * @brief generate a message Id based on the target Jid and message.
    *
    */
    std::string messageId(const std::string& Jid, const std::string& message)
    {
        return generateMessageId(Jid, message);
    }

    /**
    * Send a message to a chat room.
    * @param roomJid {string} room's JID.
    * @param message {string} message to send.
    * @returns {void}
    */
    void sendMessage(const std::string& roomJid, const std::string& message);
    /**
	* Send a private message to a peer directly.
	* @param peerFullJid {string} peer's full JID.
	* @param message {string} message to send.
	* @returns {void}
	*/
    void sendPrivateMessage(const std::string& peerFullJid, const std::string& message);
    /**
	* Leave a chat room.
	* @param roomJid {string} room's JID.
	* @returns {void}
	*/
    void leaveRoom(const std::string& roomJid);
    /**
	* join a chat room.
	* @param roomJid {string} room's JID.
	* @param [password] {string} optional, password to join the chat room.
	* @param [meta] {MessageMeta} optional, meta data to create chat room.
	* @returns {void}
	*/
    void joinRoom(const std::string& roomJid, const std::string& password = "", const MessageMeta& meta = MessageMeta());
    typedef std::function<void(const char*)> ErrorFunction;
    void invite(const std::string &peerMail);
    /**
    * @brief send user's chat state to a chat room.
    * @param roomJid {string} room's JID.
    * @param chatState {ChatState} user's chat state.
    * @returns {void}
    */
    void sendChatState(const std::string& roomJid, const ChatState::STATE_TYPE chatState);
};

}
#endif // CHATCLIENT_H
