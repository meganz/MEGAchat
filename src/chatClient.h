#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include "chatRoom.h"
#include "contactList.h"
#include "ITypes.h" //for IPtr
#include "karereEventObjects.h"
#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mstrophepp.h>
#include "rtcModule/IRtcModule.h"
#include "rtcModule/lib.h"
#include "dummyCrypto.h"
#include "strophe.disco.h"
#include "base/services.h"
#include "sdkApi.h"
#include "megaCryptoFunctions.h"
#include "iEncHandler.h"
#include "messageBus.h"
#include "busConstants.h"
#include <common.h>
#include <upper_handler.h>
#include <shared_buffer.h>

#include <memory>
#include <map>
#include <type_traits>


namespace strophe {class Connection;}
namespace rtcModule
{
    class IRtcModule;
    class IEventHandler;
}

class MyMegaApi;

namespace karere
{

template<class M = DummyMember, class GM = DummyGroupMember,
        class SP = SharedDummyMember, class EH = DummyEncProtocolHandler>
class Client
{
    static_assert(std::is_base_of<M, GM>::value,
        "Template parameter M must be a base of GM.");
    static_assert(std::is_base_of<std::shared_ptr<M>, SP>::value,
        "Template parameter SP must be a base of std::shared_ptr<M>");

public:

    /**
     * @brief The member type used for Client.
     */
    typedef M MemberType;

    /**
     * @brief The group member type used for Client.
     */
    typedef GM GroupMemberType;

    /**
     * @brief The shared member class used for Client.
     */
    typedef SP SharedMemberPtrType;

    /**
     * @brief The encryption handler type used for Client.
     */
    typedef EH EncryptionHandlerType;

    /*conn {strophe::Connection}, client's strophe connection*/
    std::shared_ptr<strophe::Connection> conn;

protected:

    /**
    * Compose an invitation message from received invitation Stanza data.
    * @param stanza {Stanza} received stanza data.
    * @returns {shared_ptr<InviteMessage>} a smart pointer pointing to an invitation message.
    */
    std::shared_ptr<InviteMessage> composeInviteMessesage(strophe::Stanza stanza);
    void testBusMessage(message_bus::SharedMessage<> &message);
    std::shared_ptr<PresenceMessage> composePresenceMessage(strophe::Stanza stanza);
    /**
    * Compose an incoming message from received Stanza data.
    * @param stanza {Stanza} received stanza data.
    * @returns {shared_ptr<InviteMessage>} a smart pointer pointing to an incoming message.
    */
    std::shared_ptr<IncomingMessage> composeIncomingMessesage(strophe::Stanza stanza);
    /**
    * Compose an action message from received Stanza data.
    * @param stanza {Stanza} received stanza data.
    * @returns {shared_ptr<ActionMessage>}.
    */
    std::shared_ptr<ActionMessage> composeActionMessesage(strophe::Stanza stanza);
    /**
    * Get user name of current connection.
    * @returns {string} user name of current connection.
    */
    inline std::string getUsername() const
    {
        return strophe::getNodeFromJid(conn->fullOrBareJid());
    }

    /**
    * Get resource of current connection.
    * @returns {string} resource of current connection.
    */
    inline std::string getResource() const
    {
        return strophe::getResourceFromJid(conn->fullJid());
    }

    /**
    * Broadcast an action (to all my devices) + optionally to a specific room.
    * @param action{string} name of the action.
    * @param meta{Meta} meta data.
    * @param toRoomJid{string} optional, JID of the room to send the action.
    * @returns {string} resource of current connection.
    */
    inline void sendBroadcastAction(const std::string& action, /*const Meta& meta, */const std::string& toRoomJid = "") const;
    /**
     * @brief Initialize the contact list.
     *
     * This performs a request to xmpp roster server and fetch the contact list.
     * Contact list also registers a contact presence handler to update the list itself based on received presence messages.
     */
    promise::Promise<int> initializeContactList();
    /**
     * @brief send response to ping request.
     *
     * This performs an xmpp response to the received xmpp ping request.
     */
    void sendPong(const std::string& peerJid, const std::string& messageId);
public:
    std::shared_ptr<MyMegaApi> api;
    rtcModule::IPtr<rtcModule::IRtcModule> rtc;
    std::function<void()> onRtcInitialized;
    std::map<std::string, std::shared_ptr<ChatRoom<M, GM, SP, EH>>> chatRooms;

    Client(const std::string& email, const std::string& password);
    virtual ~Client();
    void registerRtcHandler(rtcModule::IEventHandler* rtcHandler);
    promise::Promise<int> init();
    void handleInvitationMessage(std::shared_ptr<InviteMessage> &message);
    void handleDataMessage(std::shared_ptr<IncomingMessage> &message);
    void handleActionMessage(std::shared_ptr<ActionMessage> &message);
    void handlePresenceMessage(std::shared_ptr<PresenceMessage> &message);
    void addOtherUser(const std::string &roomId, const std::string &otherJid);
    /**
    * @brief Self Ping function.
    * @param [intervalSec] {int} optional with default value as 100, interval in seconds to do self ping.
    * @returns {void}
    */
    void startSelfPings(int intervalSec = 100);
    /**
     * @brief Ping a target peer to check whether he/she is alive
     * @param [peerJid] {string} peer's Jid.
     * @param [intervalSec] {int} optional with default value as 100, interval in seconds to do ping.
     *
     * This performs a xmpp ping request to xmpp server and check whether the target user is alive or not.
     */
    void pingPeer(const std::string& peerJid, int intervalSec = 100);
    /**
    * @brief generate a message Id based on the target Jid and message.
    *
    */
    std::string messageId(const std::string& Jid, const std::string& message)
    {
        return generateMessageId(Jid, message);
    }
    /**
    * @Get a unique nickname based on current connection.
    * @returns {string} nickname based on current connection.
    */
    std::string getNickname() const { return getUsername() + "__" + getResource(); }

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
    /**
    * @brief set user's chat presence.
    * set user's presence state, which can be one of online, busy, away, online
    */
    void setPresence(const Presence pres, const int delay = 0);

    /**
    * @brief get
    * @param roomJid {string} room's JID.
    * @param chatState {ChatState} user's chat state.
    * @returns {void}
    */
    const Contact& getContact(const std::string& userJid)
    {
        return contactList.getContact(userJid);
    }
    ContactList& getContactList()
    {
        return contactList;
    }
protected:
    /** our own email address */
    std::string mEmail;
    /** our password */
    std::string mPassword;
    /** xmpp message handler */
    xmpp_handler mXmppMessageHandler;
    /** xmpp presence handler */
    xmpp_handler mXmppPresenceHandler;
    /** xmpp iq handler */
    xmpp_handler mXmppIqHandler;
    /** client's contact list */
    ContactList contactList;
    /* handler for webrtc events */
    rtcModule::IPtr<rtcModule::IEventHandler> mRtcHandler;

    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getOtherUserInfo(std::string &emailAddress);
    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getThisUserInfo();
};

#define MPENC_T_PARAMS mpenc_cpp::Member,mpenc_cpp::GroupMember,mpenc_cpp::SharedMemberPtr,mpenc_cpp::UpperHandler
#define MPENC_T_DYMMY_PARAMS DummyMember, DummyGroupMember,SharedDummyMember, DummyEncProtocolHandle

typedef Client<MPENC_T_PARAMS> ChatClient;

}
#endif // CHATCLIENT_H
