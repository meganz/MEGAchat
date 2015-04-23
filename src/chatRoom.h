#ifndef CHATROOM_H
#define CHATROOM_H

#include "chatCommon.h"
#include "karereCommon.h"
#include "text_filter/text_filter.h"
#include "messageBus.h"
#include <mstrophepp.h>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <iostream>
#include <member.h>
#include <group_member.h>
#include "busConstants.h"
#include "iEncHandler.h"
#include <openssl/sha.h>
#include <mega/base64.h>
#include "iMember.h"
#include "karereEventObjects.h"

namespace nsmega = mega;

namespace karere
{

template<class M, class GM, class SP, class EP>
class Client;

/**
 * @brief Represents a XMPP chatroom Encapsulates the members and handlers
 * required to process transactions with the room and other members of the room.
 *
 * Template Parameters:
 *      M: The member class to be used.
 *      GM: The group member class to be used.
 *      SP: The shared pointer class for members.
 *      EP: The encryption protocol handler.
 */
template<class M = DummyMember, class GM = DummyGroupMember,
        class SP = SharedDummyMember, class EP = DummyEncProtocolHandler>
class ChatRoom
{
public:
    typedef enum eState {
        INITIALIZED = 5,
        JOINING = 10,
        JOINED = 20,

        WAITING_FOR_PARTICIPANTS = 24,
        PARTICIPANTS_HAD_JOINED = 27,

        PLUGINS_WAIT = 30,
        PLUGINS_READY = 40,

        READY = 150,
        PLUGINS_PAUSED =175,

        LEAVING = 200,
        LEFT = 250
    } STATE;

public:
    static_assert(std::is_base_of<M, GM>::value,
                            "Template parameter M must be a base of GM.");
    static_assert(std::is_base_of<std::shared_ptr<M>, SP>::value,
                            "Template parameter SP must be a base of std::shared_ptr<M>");
    /*
     * @param aClient {Client} reference to a client-owner of chat room
     * @param peerFullJid {string} peer's JID
     * @param roomJid {string} room's JID
     * @param myRoomJid {string} room's JID of owner's
     * @param peerRoomJid {string} room's JID of peer's
     * @constructor
    */
    ChatRoom(Client<M, GM, SP, EP>& aClient, const std::string& peerFullJid,
             const std::string& roomJid, const std::string& myRoomJid,
             const std::string& peerRoomJid) :
                 client(aClient),
                 mPeerFullJid(peerFullJid),
                 mRoomJid(roomJid),
                 mMyRoomJid(myRoomJid),
                 mPeerRoomJid(peerRoomJid),
                 mMyFullJid(client.conn->fullJid()),
                 isOwner(false){

        printf("myFullJid:%s\n", mMyFullJid.c_str());
        // For filtering incoming messages.
        init();
    }


    /*
    * @param aClient {Client} reference to a client-owner of chat room
    * @param roomJid {string} room's JID
     * @param Participants {std::vector<std::string>} room's participants
    * @constructor
    */
    ChatRoom(Client<M, GM, SP, EP>& aClient, const std::string& roomJid, const std::vector<std::string> Participants)
    :client(aClient), mRoomJid(roomJid), mMyFullJid(client.conn->fullJid()),
     isOwner(false){
         init();
    }

    /**
     * @brief Add the given member to the map of members for this ChatRoom.
     *
     * @param member The member to add to the ChatRoom.
     */
    void addGroupMember(SharedChatRoomMember<M, SP> member) {
        members.insert({member->getId(), member});
        memberVector.push_back(member);

        mpenc_cpp::SecureKey key((unsigned char*)PUB_KEY, 32);
        member->setStaticPublicKey(key);
        CHAT_LOG_INFO("Added member %s", member->getId().c_str());
        encryptionHandler.setMemberKeyUh(member->getId(), key);
        auto keyTest = member->getStaticPublicKey();
        //KR_LOG("Key resolves to: %d", (unsigned int)keyTest);
    }

    /**
     * @brief Get the client which has the specified fullJid, if it exists.
     *
     * @param fullJid The fullJid of the member to find.
     * @return The member, if it exists, or an empty shared_ptr.
     */
    SharedChatRoomMember<M, SP> getGroupMember(std::string &fullJid) {
        SharedChatRoomMember<M, SP> member;
        auto i = members.find(fullJid);
        if(i != members.end()) {
            member = i->second();
        }

        return member;
    }

    /*
     * @param aClient {Client} reference to a client-owner of chat room
     * @param peer {string} peer's JID
     * @factory function to create ChatRoom object.
     * @return {shared_ptr<ChatRoom>} a smart pointer pointing to a new ChatRoom object.
    */
    template<class N = DummyMember, class GN = DummyGroupMember,
            class P = SharedDummyMember, class NP = DummyEncProtocolHandler>
    static
    promise::Promise<std::shared_ptr<ChatRoom<N, GN, P, NP>> >
    create(Client<N, GN, P, NP>& client, const std::string& peerFullJid)
    {
        const char* myFullJid = xmpp_conn_get_bound_jid(*client.conn);

        if (!myFullJid)
        {
            throw std::runtime_error("Error getting own JID from connection");
        }

        std::string myBareJid = strophe::getBareJidFromJid(myFullJid);
        std::string peerBareJid = strophe::getBareJidFromJid(peerFullJid);
        std::string myId = strophe::getNodeFromJid(client.conn->fullJid());
        std::string peerId = strophe::getNodeFromJid(peerFullJid);
        std::string* id1;
        std::string* id2;

        if (myBareJid <= peerBareJid)
        {
            id1 = &myId;
            id2 = &peerId;
        }
        else
        {
            id1 = &peerId;
            id2 = &myId;
        }

        std::string jidstr;
        jidstr.reserve(64);
        jidstr.append("prv").append(*id1).append(*id2);
        unsigned char shabuf[33];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, jidstr.c_str(), jidstr.size());
        SHA256_Final(shabuf, &sha256);
        char b32[27]; //26 actually
        nsmega::Base32::btoa(shabuf, 16, b32);
        shabuf[26] = 0;
        std::string roomJid;
        roomJid.reserve(64);
        roomJid.append(b32).append("@conference.").append(KARERE_XMPP_DOMAIN);

        std::string myRoomJid = roomJid;
        myRoomJid.append("/").append(myId).append("__")
                 .append(strophe::getResourceFromJid(myFullJid));

        std::string peerRoomJid = roomJid;
        peerRoomJid.append("/").append(peerId).append("__")
                   .append(strophe::getResourceFromJid(peerFullJid));

        strophe::Stanza pres(*client.conn);
        pres.setName("presence")
            .setAttr("xmlns", "jabber:client")
            .setAttr("from", myFullJid)
            .setAttr("to", myRoomJid)
            .c("x")
            .setAttr("xmlns", "http://jabber.org/protocol/muc")
            .c("password");

        std::shared_ptr<ChatRoom> room(new ChatRoom<N, GN, P, NP>(client, peerFullJid, roomJid,
            myRoomJid, peerRoomJid));
        promise::Promise<std::shared_ptr<ChatRoom<N, GN, P, NP>>> ret;

        client.conn->sendQuery(pres, "muc")
        .then([ret, room, &client](strophe::Stanza result) mutable
         {
            strophe::Stanza config(*client.conn);

            config.setName("iq")
                  .setAttr("to", room->roomJid())
                  .setAttr("type", "set")
                  .setAttr("xmlns", "jabber:client")
                  .c("query")
                  .setAttr("xmlns", "http://jabber.org/protocol/muc#owner")
                  .c("x")
                  .setAttr("xmlns", "jabber:x:data")
                  .setAttr("type", "submit");


            return client.conn->sendIqQuery(config, "config");
         })
         .then([ret, room, myFullJid, &client](strophe::Stanza s) mutable
        {
            CHAT_LOG_INFO("create: Creating room: %s", room->roomJid().c_str());
            client.chatRooms.insert(std::pair<std::string, std::shared_ptr<ChatRoom<N, GN, P, NP>>>(room->roomJid(), room));
            room->roomSetup(myFullJid);
            room->isOwner = true;
            ret.resolve(room);
            return 0;
        })
        .fail([ret](const promise::Error& err) mutable
        {
            ret.reject(err);
            CHAT_LOG_ERROR("Room creation failed");

            return 0;
        });


        return ret;
    };

    inline void roomSetup(const char *myFullJid) {
        // Set the owner member for this chatroom, and add to the list of members.
        // TODO: There is something wrong with groupmember, not being added
        // properly.
        ownerMember =
               SharedChatRoomMember<M, SP>(myFullJid);
        members.insert({ownerMember->getId(), ownerMember});
        mpenc_cpp::SharedGroupMemberPtr gpMem(ownerMember->getId());
        encryptionHandler.setGroupMemberUh(gpMem);
        memberVector.push_back(ownerMember);
        CHAT_LOG_INFO("Setting room owner to: true");


        ///// Dodgy pk and sk.
        mpenc_cpp::SecureKey pkKey((unsigned char*)PUB_KEY, 32);
        mpenc_cpp::SecureKey skKey((unsigned char*)SEC_KEY, 64);

        encryptionHandler.setStaticKeys(pkKey, skKey);

        //// Dodgyness ends.
        message_bus::SharedMessage<M_MESS_PARAMS> busMessage(ROOM_ADDED_EVENT);
        busMessage->addValue(ROOM_ADDED_JID, roomJid());
        message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()->alertListeners(ROOM_ADDED_EVENT, busMessage);

    }

	/**
	* Getter for property `mState`
	*
	* @returns {(STATE)} mState
	*/
    inline STATE getState() const {return mState;}

	/**
	* Setter for property `mState`
	*
	* @returns {(void)}
	*/
    inline void setState(const STATE state) const { mState = state;}

    /**
	* Getter for property `mMyFullJid`
	*
	* @returns {(string)} mMyFullJid
	*/
    inline const std::string& myFullJid() const {return mMyFullJid;}

	/**
	* Getter for property `mPeerFullJid`
	*
	* @returns {(string)} mPeerFullJid
	*/
    inline const std::string& peerFullJid() const {return mPeerFullJid;}

	/**
	* Getter for property `mRoomJid`
	*
	* @returns {(string)} mRoomJid
	*/
    inline const std::string& roomJid() const {return mRoomJid;}

	/**
	* Getter for property `mMyRoomJid`
	*
	* @returns {(string)} mMyRoomJid
	*/
    inline const std::string& myRoomJid() const {return mMyRoomJid;}

	/**
	* Getter for property `mPeerRoomJid`
	*
	* @returns {(string)} mPeerRoomJid
	*/
    inline const std::string& peerRoomJid() const {return mPeerRoomJid;}

    /**
    * Add a user to current chat room.
    * @param fullJid {string} user's full JID
    * @returns {(int)} 0: invitation is sent successfully, non-0: error code.
    *
    * The invitation is sent as a message by the following steps:
    * <!-- grant membership -->
    * <iq from='s47ecw5xziaho@karere.mega.nz/mc-eff07ecd1212620cfe0bfe901e817054' id='49'
    *    to='prv_2915432103_2908746235@conference.karere.mega.nz' type='set' xmlns='jabber:client'>
    *    <query xmlns='http://jabber.org/protocol/muc#admin'>
    *        <item affiliation='owner' jid='tksgorgwcyg5u@karere.mega.nz'/>
    *    </query>
    * </iq>

    * <!-- send invitation as a message -->
    * <message from='s47ecw5xziaho@karere.mega.nz/mc-eff07ecd1212620cfe0bfe901e817054'
    *         to='tksgorgwcyg5u@karere.mega.nz/mc-349282fad0ebf661f1869956a5b215ac' id='50' xmlns='jabber:client'>
    *    <x xmlns='jabber:x:conference' jid='prv_2915432103_2908746235@conference.karere.mega.nz'>
    *        <json>{"ctime":1422563356.199,
    *               "invitationType":"resume",
    *               "participants":["s47ecw5xziaho@karere.mega.nz","tksgorgwcyg5u@karere.mega.nz"],
    *               "users":{"tksgorgwcyg5u@karere.mega.nz/mc-48a1b008473358adcd1b82ee0a4639ca":
    *               "moderator","tksgorgwcyg5u@karere.mega.nz/mc-4894ab93a1185f9d99950e05b4ae3b26":
    *               "moderator","s47ecw5xziaho@karere.mega.nz/mc-eff07ecd1212620cfe0bfe901e817054":
    *               "moderator"},
    *               "type":"private"}</json>
    *    </x>
    * </message>
    *
    * <presence id="muc1" xmlns="jabber:client" to="prv_1402914367_2232705867@conference.karere.mega.nz/gvpsfxumnukxq__38273057021423534212647096"
    * from="gvpsfxumnukxq@karere.mega.nz/38273057021423534212647096">
    * <x xmlns="http://jabber.org/protocol/muc"><password/></x></presence>
    */
    promise::Promise<int> addUserToChat(const std::string& peerBareId) const
    {
        strophe::Stanza grant(*client.conn);
        grant.setName("iq")
             .setAttr("from", myFullJid())
             .setAttr("to", roomJid())
             .setAttr("type", "set")
             .setAttr("xmlns", "jabber:client")
             .c("query")
             .setAttr("xmlns", "http://jabber.org/protocol/muc#admin")
             .c("item")
             .setAttr("affiliation", "owner")
             .setAttr("jid", peerBareId);

        return client.conn->sendIqQuery(grant)
                .then([this, peerBareId](strophe::Stanza) mutable
        {

            std::string json = "{\"ctime\":";
            json.append(std::to_string(time(nullptr)))
                .append(",\"invitationType\":\"created\",\"participants\":[\"")
                .append(strophe::getBareJidFromJid(myFullJid())).append("\",\"")
                .append(strophe::getBareJidFromJid(peerBareId)).append("\"],\"users\":{\"")
                .append(myFullJid()).append("\":\"moderator\"");

            const auto& peers = client.getContactList().getFullJidsOfJid(peerBareId);

            for (auto& p: peers)
                json.append(",\"").append(p)+="\":\"moderator\"";
            const auto& mine = client.getContactList().getFullJidsOfJid(myFullJid());
            for (auto& m: mine)
                json.append(",\"").append(m)+="\":\"moderator\"";

            json.append("},\"type\":\"private\"}");
            //printf("json = %s\n", json.c_str());

            strophe::Stanza invite(*client.conn);
            invite.setName("message")
                  .setAttr("from", myFullJid())
                  .setAttr("to", peerBareId)
                  .setAttr("xmlns", "jabber:client")
                  .c("x")
                  .setAttr("xmlns", "jabber:x:conference")
                  .setAttr("jid", roomJid())
                  .c("json")
                  .t(json);

            client.conn->sendQuery(invite, "invite");

            return 0; //TODO: Invite ack?
         })
        .fail([](const promise::Error& err) mutable
         {
            printf("Error adding user to chatroom: %s\n", err.msg().c_str());
            return err;
         });
    }

    void sendUserChatState(const std::string& userJid, const ChatState::STATE_TYPE chatState)
    {
        strophe::Stanza stateStanza(*client.conn);
        stateStanza.setName("message")
            .setAttr("from", userJid)
            .setAttr("to", roomJid())
		    .setAttr("type", "groupchat")
            .c(ChatState::convertStateToString(chatState).c_str())
            .setAttr("xmlns", "http://jabber.org/protocol/chatstates");
        client.conn->send(stateStanza);
    }
    //******************************************************************//
    //                    Message Filter Functions.                     //
    //******************************************************************//

private:

    /**
     * @brief Setup the chatroom.
     *
     * This performs setup for the chatroom. The observers for handling
     * incoming and outgoing messages are set.
     */
    inline void init() {
        outgoingTextFilter.addObserver([this](const std::string &message){
            std::string event(ROOM_MESSAGE_EVENT);
            event.append(roomJid());
            message_bus::SharedMessage<M_MESS_PARAMS> retMessage(event);
            retMessage->addValue(ROOM_MESSAGE_CONTENTS, message);
            message_bus::SharedMessageBus<M_BUS_PARAMS>::
                getMessageBus()->alertListeners(event, retMessage);
        });

        incomingTextFilter.addObserver([this](const std::string &message){
            std::string event(ROOM_MESSAGE_EVENT);
            event.append(roomJid());
            message_bus::SharedMessage<M_MESS_PARAMS> retMessage(event);
            retMessage->addValue(ROOM_MESSAGE_CONTENTS, message);
            message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()->alertListeners(event, retMessage);
        });

        encryptionHandler.addOutgoingMessageObserverUh([this](int num){
            CHAT_LOG_INFO("Sending outgoing message");
            mpenc_cpp::SharedBuffer buffer =
                    encryptionHandler.getNextMessageUh();
            std::string retMessage(MPENC_HEADER);
            retMessage.append(buffer.str());

            sendMessage(retMessage);
        });

        encryptionHandler.addIncomingDataMessageObserverUh([this](int num){
            CHAT_LOG_INFO("Incoming data message");
            mpenc_cpp::SharedBuffer dataMesssage =
                                            encryptionHandler.getNextDataMessageUh();
            incomingTextFilter.handleData(dataMesssage.str());
        });

        encryptionHandler.addErrorObserverUh(mpenc_cpp::I_DONT_KNOW_YOUR_GUESS_IS_AS_GOOD_AS_MINE, [](mpenc_cpp::MPENC_ERROR error){
            CHAT_LOG_ERROR("Error code: %d", error);
            sendWarningMessage(mpenc_cpp::getErrorString(error));
        });

        encryptionHandler.addStateObserverUh([](mpenc_cpp::ProtocolState s){
            CHAT_LOG_INFO("State: %s", mpenc_cpp::getStateString(s).c_str());
        });
    }

public:

    /**
     * @brief Test if we are the owner of this room.
     */
    bool ownsRoom() {
        return isOwner;
    }

    void beginEncryptionProtocolProcess() {

        CHAT_LOG_INFO("Num users = %zu", memberVector.size());

        for(auto &i : memberVector) {
            CHAT_LOG_INFO("User: %s", i->getId().c_str());
        }

        encryptionHandler.startUh(memberVector, mpenc_cpp::WMF_BASE_64);
    }

    /**
     * @brief Filter incoming messages.
     *
     * @param message Incoming message from XMPP.
     */
    void filterIncoming(const std::string &message) {
//        CHAT_LOG_INFO("Filtering message: %s", message.c_str());
//        if(encryptionHandler.getProtocolStateUh() == mpenc_cpp::INITILISED) {
//            encryptionHandler.createDataMessageUh(mpenc_cpp::SharedBuffer(message));
//        }
//        else {
//            textFilterIncoming(message);
//        }
        CHAT_LOG_INFO("incoming message = %s", message.c_str());
        mpenc_cpp::SharedBuffer buffer((unsigned char*)message.c_str(), message.size());
        encryptionHandler.processBaseSixtyFourMessageUh(buffer);
    }

    /**
     * @brief Decrypt incoming messages.
     *
     * @param message Incoming message.
     */
    void textFilterIncoming(const std::string &message) {
        try {

        }
        catch(std::runtime_error &e) {

        }
    }

    void filterOutgoing(const std::string &message) {
        CHAT_LOG_INFO("Filtering message: %s", message.c_str());

        try {
            outgoingTextFilter.handleData(message);
        }
        catch(std::runtime_error &e) {
            sendErrorMessage(e.what());
        }

        this->encryptMessage(message);
    }

    void encryptMessage(const std::string &message) {
        if(encryptionHandler.getProtocolStateUh() == mpenc_cpp::INITILISED) {
            encryptionHandler.createDataMessageB64Uh(mpenc_cpp::SharedBuffer(message));
            std::string nMessage(encryptionHandler.getNextMessageUh().str());
            CHAT_LOG_INFO("sending encrypted message %s", nMessage.c_str());
            sendMessage(nMessage);
        }
        else {
            sendMessage(message);
        }
    }

	/**
	* Send a group chat message to all participants in the chat room.
	* @param message {string} message to send
	*/
    void sendMessage(const std::string& message)
    {
        CHAT_LOG_INFO("sending message %s", message.c_str());

        strophe::Stanza stanza_message(*client.conn);
        stanza_message.setName("message")
                      .setAttr("id", generateMessageId(roomJid(), message))
                      .setAttr("from", myFullJid())
                      .setAttr("to", roomJid())
                      .setAttr("type", "groupchat")
                      .c("body")
                      .c("messageContents")
                      .t(message)
                      .up()
                      .c("active")
                      .setAttr("xmlns","http://jabber.org/protocol/chatstates");

        client.conn->send(stanza_message);
    }

    //*************************************************************//
    //                  Encryption handling                        //
    //*************************************************************//

    int setupEncryption() {
        return 0;
    }

protected:

    /**
     * @brief The client for this room.
     */
    Client<M, GM, SP, EP>& client;

    /**
     * @brief The id for the initial peer used to create the room.
     */
    std::string mPeerFullJid;

    /**
     * @brief The full Jid for the room.
     */
    std::string mRoomJid;

    /**
     * @brief
     */
    std::string mMyRoomJid;

    /**
     * @brief
     */
    std::string mPeerRoomJid;

    /**
     * @brief
     */
    std::string mMyFullJid;

    /**
     * @brief The map of members for this room.
     */
    std::map<std::string, SharedChatRoomMember<M, SP>> members;

    /**
     * @brief Vector of members.
     */
    mpenc_cpp::MemberVector memberVector;

    /**
     * @brief The member who owns the current client.
     */
    SharedChatRoomMember<M, SP> ownerMember;

    /**
     * @brief The list of participants in the chatroom.
     */
    STATE mState;

    /**
     * @brief The encryption handler for this chatroom.
     */
    EP encryptionHandler;

    /**
     * @brief The incoming text filter for this chatroom.
     */
    TextFilter incomingTextFilter;

    /**
     * @brief The outgoing text filter for this chatroom.
     */
    TextFilter outgoingTextFilter;

    /**
     * @brief Indicates that we are the owner of the chat, so we should kick
     * off mpenc.
     */
    bool isOwner;

private:

};

}

#endif // CHATROOM_H
