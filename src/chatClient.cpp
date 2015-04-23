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
#include <base/retryHandler.h>
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
#include "chatClient.h"

namespace karere
{

template<class M, class GM, class SP, class EH>
std::shared_ptr<InviteMessage> Client<M,GM,SP,EH>::composeInviteMessesage(strophe::Stanza stanza)
{
    message_bus::SharedMessage<> busMessage("InviteMessage");
    std::shared_ptr<InviteMessage> inviteMessage = nullptr;
    xmpp_stanza_t* rawX = stanza.rawChild("x");
    if( rawX != NULL)
    {
        strophe::Stanza x(rawX);
        xmpp_stanza_t* rawJason = x.rawChild("json");
        if(rawJason != NULL)
        {
            strophe::Stanza jsonStanza(rawJason);
            mega::JSON jsondata;
            std::string str_json(jsonStanza.text().c_str());

            jsondata.begin(str_json.c_str());
            jsondata.enterobject();
            std::string obj;
            MessageMeta meta;
            bool bWithJson = false;
            std::string invitationType = "";
            while(jsondata.storeobject(&obj))
            {
                if(obj.compare("ctime") == 0) {
                    jsondata.pos++;
                    jsondata.storeobject(&obj);
                    meta.create_time = obj;
                    bWithJson = true;
                } else if (obj.compare("participants") == 0) {
                    jsondata.pos++;
                    jsondata.storeobject(&obj);
                    std::string rawParticipants = obj.substr(1, obj.size() - 2);
                    std::shared_ptr<char> dup = std::shared_ptr<char>(strdup(rawParticipants.c_str()));
                    char* token = std::strtok(dup.get(), ",");
                    while (token != NULL) {
                        std::string participant(token);
                        meta.participants.push_back(participant.substr(1, participant.size()-2));
                        token = std::strtok(NULL, ",");
                    }
                } else if (obj.compare("users") == 0) {
                    jsondata.pos++;
                    jsondata.storeobject(&obj);
                    std::string rawUsers = obj.substr(1, obj.size() - 2);
                    std::shared_ptr<char> dup = std::shared_ptr<char>(strdup(rawUsers.c_str()));
                    char* token = std::strtok(dup.get(), ",");
                    while (token != NULL) {
                        std::string user(token);
                        if(user.find(":") != std::string::npos){
                            meta.users.push_back(user.substr(1, user.find(":")-2));
                        }
                        token = std::strtok(NULL, ",");
                    }
                } else if(obj.compare("invitationType") == 0) {
                    jsondata.pos++;
                    jsondata.storeobject(&obj);
                    invitationType = obj;
                } else if(obj.compare("type") == 0) {
                    jsondata.pos++;
                    jsondata.storeobject(&obj);
                    meta.type = obj;
                }
                jsondata.pos++;
            }

            if (bWithJson)
            {
                busMessage->addValue("to", stanza.attr("to"));
                busMessage->addValue("from", stanza.attr("to"));
                busMessage->addValue("jid", x.attr("jid"));
                busMessage->addValue("meta", meta);
                busMessage->addValue("iType", (invitationType.compare(0, strlen("created"), "created") == 0) ? InviteMessage::CREATE : ((obj.compare(0, strlen("resume"), "resume") == 0) ? InviteMessage::RESUME : InviteMessage::UNKNOWN));
                inviteMessage = std::shared_ptr<InviteMessage>(new InviteMessage(stanza.attr("to"), stanza.attr("from"), x.attr("jid"),
                                                                                 (invitationType.compare(0, strlen("created"), "created") == 0) ? InviteMessage::CREATE : ((obj.compare(0, strlen("resume"), "resume") == 0) ? InviteMessage::RESUME : InviteMessage::UNKNOWN), meta));
                testBusMessage(busMessage);
            }
        }
    }
    return inviteMessage;
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::testBusMessage(message_bus::SharedMessage<> &message)
{
    try
    {
        std::string to = message->getValue<std::string>("to");
        std::string from = message->getValue<std::string>("from");
        std::string jid = message->getValue<std::string>("jid");
        //int iType = message->getValue<InviteMessage::eInvitationType>("iType");
        MessageMeta meta = message->getValue<MessageMeta>("meta");
    }
    catch(std::runtime_error &error)
    {
        KR_LOG_ERROR("Error: %s", error.what());
    }
}

template<class M, class GM, class SP, class EH>
std::shared_ptr<PresenceMessage> Client<M,GM,SP,EH>::composePresenceMessage(strophe::Stanza stanza)
{
    std::shared_ptr<PresenceMessage> presenceMessage = nullptr;
    const char* type = stanza.attrOrNull("type");
    std::string from = stanza.attr("from");
    //xmpp_stanza_t *rawX = stanza.rawChild("x");

    if(from.find("@conference") != std::string::npos)
    {
        std::string roomJid = from.substr(0, from.find("/"));
        std::string separator("__");
        std::string fromStr = from.substr(from.find("/")+1);
        std::string fromJid = fromStr.replace(fromStr.find(separator), separator.size(), std::string("@") + std::string(KARERE_XMPP_DOMAIN) + std::string("/"));
        KR_LOG_DEBUG("room Jid : %s --- from Jid : %s --- action: %s\n", roomJid.c_str(), fromJid.c_str(), (type != NULL) ? stanza.attr("type"): "available");
        presenceMessage = std::shared_ptr<PresenceMessage>(new PresenceMessage(
            stanza.attr("to"), fromJid, roomJid,
            type ? stanza.attr("type") : std::string("available")
        ));
    }

    return presenceMessage;
}
template<class M, class GM, class SP, class EH>
std::shared_ptr<IncomingMessage> Client<M,GM,SP,EH>::composeIncomingMessesage(strophe::Stanza stanza)
{
    std::shared_ptr<IncomingMessage> inviteMessage = nullptr;
    xmpp_stanza_t* rawBody = stanza.rawChild("body");
    std::string from = stanza.attr("from");
    if (!strcmp(stanza.attr("type"), "error"))
    {
        return inviteMessage;
    }
    std::string type = (stanza.attrOrNull("type") != NULL) ? stanza.attr("type") : "";

    if (!rawBody)
    {
        return inviteMessage;
    }
    strophe::Stanza body(rawBody);
    xmpp_stanza_t* message_content = body.rawChild("messageContents");
    if(!message_content)
    {
        return inviteMessage;
    }
    strophe::Stanza messageConent(message_content);
    auto intext = messageConent.text();
    std::string fromJid = from;
    std::string roomJid = from;
    if(type.compare("groupchat") == 0)
    {
        roomJid = from.substr(0, from.find("/"));
        std::string room_nick = from.substr(from.find("/")+1);
        fromJid = room_nick.replace(room_nick.find("__"), 2, std::string(std::string("@") + std::string(KARERE_XMPP_DOMAIN) + std::string("/")));
    }

    inviteMessage = std::shared_ptr<IncomingMessage>(new IncomingMessage(
        std::string(stanza.attr("to")),
        fromJid,
        roomJid,
        (type == "groupchat") ? IncomingMessage::GROUPCHAT : IncomingMessage::CHAT,
        std::string(""),
        std::string(intext)
    ));

    return inviteMessage;
}

template<class M, class GM, class SP, class EH>
std::shared_ptr<ActionMessage> Client<M,GM,SP,EH>::composeActionMessesage(strophe::Stanza stanza)
{
    std::shared_ptr<ActionMessage> actionMessage = nullptr;

    if (!strcmp(stanza.attr("type"), "error"))
    {
        return actionMessage;
    }
    std::string type = (stanza.attrOrNull("type") != NULL) ? stanza.attr("type") : "";

    if (type.compare("action") == 0)
    {
        xmpp_stanza_t* rawX = stanza.rawChild("x");
        if( rawX)
        {
            strophe::Stanza x(rawX);
            xmpp_stanza_t* rawJason = x.rawChild("json");
            if(rawJason != NULL)
            {
                strophe::Stanza jsonStanza(rawJason);
                mega::JSON jsondata;
                std::string obj;
                std::string str_json(jsonStanza.text().c_str());
                jsondata.begin(str_json.c_str());
                jsondata.enterobject();
                std::string roomJid;
                std::string actionname;

                while(jsondata.storeobject(&obj))
                {
                    if(obj.compare("action") == 0)
                    {
                        jsondata.pos++;
                        jsondata.storeobject(&actionname);
                    }
                    if(obj.compare("roomJid") == 0)
                    {
                        jsondata.pos++;
                        jsondata.storeobject(&roomJid);
                    }
                }
                actionMessage = std::shared_ptr<ActionMessage>(new ActionMessage(
                    std::string(stanza.attr("to")),
                    std::string(stanza.attr("from")),
                    roomJid,
                    (actionname =="conv_start")
                        ? ActionMessage::CONVERSATION_START
                        : ((actionname.compare("conv_end") == 0)
                           ? ActionMessage::CONVERSATION_END
                           : ActionMessage::USER_LEFT))
                );
            }
        }
    }
    return actionMessage;
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::sendBroadcastAction(const std::string& action, /*const Meta& meta, */ const std::string& toRoomJid) const
{
    std::string messageId = generateMessageId(conn->fullJid() + (toRoomJid.empty() ? "" : toRoomJid), "");
    strophe::Stanza action_message(*conn);
    std::string json = "{";
    if (!toRoomJid.empty())
    {
        json.append("\"roomJid\":\"")
            .append(toRoomJid)
            .append("\",");
    }
    json.append("\"action\":\"")
        .append(action)
        .append("\"}");

    action_message.setName("message")
                  .setAttr("to", strophe::getBareJidFromJid(conn->fullJid()))
                  .setAttr("type", "action")
                  .setAttr("id", messageId)
                  .c("x")
                      .setAttr("xmlns", "jabber:x:conference")
                      .setAttr("jid", toRoomJid)
                      .c("json")
                          .t(json);
    conn->send(action_message);
}

template<class M, class GM, class SP, class EH>
promise::Promise<int> Client<M,GM,SP,EH>::initializeContactList()
{
    strophe::Stanza roster(*conn);
    roster.setName("iq")
          .setAttr("type", "get")
          .setAttr("from", conn->fullJid())
          .c("query")
              .setAttr("xmlns", "jabber:iq:roster");
    return conn->sendIqQuery(roster, "roster")
    .then([this](strophe::Stanza s) mutable
    {
        xmpp_stanza_t* rawQuery = s.rawChild("query");
        if (rawQuery != NULL)
        {
            xmpp_stanza_t* c = xmpp_stanza_get_children(rawQuery);
            for (; c!=NULL; c = xmpp_stanza_get_next(c))
            {
                if (strcmp("item", xmpp_stanza_get_name(c)))
                    continue;
                const char* attrVal = xmpp_stanza_get_attribute(c, "jid");
                if (attrVal != NULL)
                {
                    this->contactList.addContact(std::string(attrVal));
                    message_bus::SharedMessage<> busMessage(CONTACT_ADDED_EVENT);
                    busMessage->addValue(CONTACT_JID, std::string(attrVal));
                    message_bus::SharedMessageBus<>::getMessageBus()->alertListeners(CONTACT_ADDED_EVENT, busMessage);
                }
            }
        }
        return contactList.init();
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_WARNING("Error receiving contact list\n");
        return err;
    });
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::sendPong(const std::string& peerJid, const std::string& messageId)
{
    strophe::Stanza pong(*conn);
    pong.setAttr("type", "result")
        .setAttr("to", peerJid)
        .setAttr("from", conn->fullJid())
        .setAttr("id", messageId);

    conn->send(pong);
}

template<class M, class GM, class SP, class EH>
Client<M,GM,SP,EH>::Client(const std::string& email, const std::string& password)
 :conn(new strophe::Connection(services_strophe_get_ctx())),
  api(new MyMegaApi("sdfsdfsdf")),mEmail(email), mPassword(password), contactList(conn),
  mRtcHandler(NULL)
{}

template<class M, class GM, class SP, class EH>
Client<M,GM,SP,EH>::~Client()
{
    if(mXmppMessageHandler)
    {
        conn->removeHandler(mXmppMessageHandler);
    }
    if(mXmppPresenceHandler)
    {
        conn->removeHandler(mXmppPresenceHandler);
    }
    if(mXmppIqHandler)
    {
        conn->removeHandler(mXmppIqHandler);
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::registerRtcHandler(rtcModule::IEventHandler* rtcHandler)
{
    mRtcHandler.reset(rtcHandler);
}

template<class M, class GM, class SP, class EH>
promise::Promise<int> Client<M,GM,SP,EH>::init()
{
    /* get xmpp login from Mega API */
    return
    api->call(&mega::MegaApi::login, mEmail.c_str(), mPassword.c_str())
    .then([this](ReqResult result)
    {
        KR_LOG_DEBUG("Login to Mega API successful");
        return api->call(&mega::MegaApi::getUserData);
    })
    .then([this](ReqResult result)
    {
        api->userData = result;
        const char* user = result->getText();
        if (!user || !user[0])
            return promise::reject<int>("Could not get our own JID");
        SdkString xmppPass = api->dumpXMPPSession();
        if (xmppPass.size() < 16)
            return promise::reject<int>("Mega session id is shorter than 16 bytes");
        ((char&)xmppPass.c_str()[16]) = 0;

        /* setup authentication information */
        std::string jid = std::string(user)+"@" KARERE_XMPP_DOMAIN;
        xmpp_conn_set_jid(*conn, jid.c_str());
        xmpp_conn_set_pass(*conn, xmppPass.c_str());
        KR_LOG_DEBUG("user = '%s', pass = '%s'", jid.c_str(), xmppPass.c_str());
        /* initiate connection */
        return mega::retry([this]()
        {
            //return promise::reject<int>("test");
            return conn->connect(KARERE_DEFAULT_XMPP_SERVER, 0);
        });
    })
    .then([this](int)
    {
        KR_LOG_INFO("==========Connect promise resolved\n");
        //Create the RtcModule object
        //the MegaCryptoFuncs object needs api->userData (to initialize the private key etc)
        //to use DummyCrypto: new rtcModule::DummyCrypto(jid.c_str());
        rtc.reset(createRtcModule(
            *conn, mRtcHandler.get(), new rtcModule::MegaCryptoFuncs(*api), ""));
        //create and register disco strophe plugin
        conn->registerPlugin("disco", new disco::DiscoPlugin(*conn, "Karere Native"));
        //register rtcmodule as a plugin
        conn->registerPlugin("rtcmodule", rtc.get());

        /**
        * Wrapper function to register message handler
        * @param conn {xmpp_conn_t} xmpp connection pointer.
        * @param stanza {xmpp_stanza_t} xmpp stanza data.
        * @param userdata {void*} userdata.
        * @returns {int} 1 to keep receiving messages from connection.
        */
        promise::Promise<int> pmse;
        {
            mXmppMessageHandler = conn->addHandler([this, pmse](strophe::Stanza stanza_data, void*, bool& keep) mutable
            {
                std::shared_ptr<InviteMessage> inviteMessage = composeInviteMessesage(stanza_data);
                /*if it is an invitation*/
                if (inviteMessage)
                {
                    std::string myJid = strophe::getBareJidFromJid(std::string(xmpp_conn_get_bound_jid(*this->conn)));
                    std::string toJid = strophe::getBareJidFromJid(inviteMessage->getToJid());
                    if (myJid == toJid)
                    {
                        handleInvitationMessage(inviteMessage);
                    }
                    return;
                }
                /*if it is an action message*/
                std::shared_ptr<ActionMessage> actionMessage = composeActionMessesage(stanza_data);
                if (actionMessage)
                {
                    handleActionMessage(actionMessage);
                }
                else
                {
                    /*if it is a normal incoming message*/
                    std::shared_ptr<IncomingMessage> incomingMessage = composeIncomingMessesage(stanza_data);
                    if (incomingMessage)
                    {
                        handleDataMessage(incomingMessage);
                        return;
                    }
                }

                /*Chat State Notifications*/
                if (stanza_data.rawChild("composing"))
                {
                    CHAT_LOG_DEBUG("%s is typing\n", stanza_data.attr("from"));
                }
            }, nullptr, "message", nullptr, nullptr);

            mXmppPresenceHandler = conn->addHandler([this, pmse](strophe::Stanza stanza_data, void*, bool &keep) mutable
            {
                std::shared_ptr<PresenceMessage> presenceMessage =
                    composePresenceMessage(stanza_data);
                if(presenceMessage)
                {
                    handlePresenceMessage(presenceMessage);
                    return;
                }
            }, nullptr, "presence", nullptr, nullptr);

            mXmppIqHandler = conn->addHandler([this, pmse](strophe::Stanza stanza_data, void*, bool &keep) mutable
            {
                xmpp_stanza_t* rawPing = stanza_data.rawChild("ping");
                if(rawPing)
                {
                    sendPong(stanza_data.attr("from"), stanza_data.attr("id"));
                    return;
                }
            }, nullptr, "iq", nullptr, nullptr);
        }

        if (onRtcInitialized)
        {
            onRtcInitialized();
        }

        auto pms =  initializeContactList();
        /* Send initial <presence/> so that we appear online to contacts */
        strophe::Stanza pres(*conn);
        pres.setName("presence");
        conn->send(pres);
        return pms;
    })
    .fail([](const promise::Error& error)
    {
        KR_LOG_WARNING("==========Connect promise failed:\n%s\n", error.msg().c_str());
        return error;
    });
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::handleInvitationMessage(std::shared_ptr<InviteMessage> &message)
{
    joinRoom(message->getRoomJid(), message->getPassword(), message->getMeta());
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::handleDataMessage(std::shared_ptr<IncomingMessage> &message)
{
    if (message->getType() == IncomingMessage::CHAT)
    { // private message
        CHAT_LOG_DEBUG("You get a private message from %s\n:%s", message->getFromJid().c_str(), message->getContents().c_str());
        return;
    }

    assert(message->getType() == IncomingMessage::GROUPCHAT);
    auto r = chatRooms.find(message->getRoomJid());

    if(r == chatRooms.end())
    {
        CHAT_LOG_ERROR("Room not found: %s", message->getRoomJid().c_str());
        return;
    }
    CHAT_LOG_DEBUG("from Jid : %s --- self Jid : %s", message->getFromJid().c_str(), conn->fullJid());
    /*if the message is from the sender himself, just ignore it.*/
    if(message->getFromJid() == conn->fullJid())
    {
        return;
    }
    std::string contents = message->getContents();
    size_t pos;
    if((pos = contents.find(MPENC_HEADER)) != std::string::npos)
    {
        CHAT_LOG_DEBUG("mpEnc message received");

        std::string data(contents.begin() + pos + strlen(MPENC_HEADER), contents.end());
        r->second->filterIncoming(data);
    }
    else
    {
        std::string messName(ROOM_MESSAGE_EVENT);
        messName.append(message->getRoomJid());
        CHAT_LOG_DEBUG("*********** Message name = %s", messName.c_str());
        message_bus::SharedMessage<M_MESS_PARAMS> busMessage(messName);
        busMessage->addValue(ROOM_MESSAGE_CONTENTS, message->getContents());
        message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()
            ->alertListeners(messName, busMessage);
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::handleActionMessage(std::shared_ptr<ActionMessage> &message)
{
    std::string myJid =  strophe::getBareJidFromJid(std::string(xmpp_conn_get_bound_jid(*this->conn)));
    if(myJid.compare(strophe::getBareJidFromJid(message->getFromJid())) == 0)
    {// action from my another device.
        if(message->getActionType()== ActionMessage::CONVERSATION_END)
        {
            if(chatRooms.find(message->getRoomJid()) != chatRooms.end())
            {
                leaveRoom(message->getRoomJid());
            }
        }
        else if (message->getActionType() == ActionMessage::CONVERSATION_START)
        {
            if(chatRooms.find(message->getRoomJid()) == chatRooms.end())
            {
                joinRoom(message->getRoomJid(), getNickname());
            }
        }
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::handlePresenceMessage(std::shared_ptr<PresenceMessage> &message)
{
    std::string roomJid(message->getRoomJid());
    size_t pos = roomJid.find_first_of("@conference");
    // Test if this is a valid room id.
    if(pos == std::string::npos)
    {
        throw std::runtime_error("Invalid room jid");
    }
    if (message->getFromJid() == conn->fullJid())
    {
        return;
    }
    std::string myJid = strophe::getBareJidFromJid(this->conn->fullOrBareJid());
    CHAT_LOG_DEBUG("my Jid is %s --- from Jid is %s\n", myJid.c_str(), message->getFromJid().c_str());

    if(myJid == strophe::getBareJidFromJid(message->getFromJid()))
    {// action from my another device.
        if(message->getType() == "unavailable")
        {
        }
        else if (message->getType() == "available")
        {
            if(chatRooms.find(message->getRoomJid()) == chatRooms.end())
            {
                joinRoom(message->getRoomJid(), getNickname());
            }
            else
            {
                addOtherUser(message->getRoomJid(), message->getFromJid());
            }
        }
    }
    else
    {
        addOtherUser(message->getRoomJid(), message->getFromJid());
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::addOtherUser(const std::string &roomId, const std::string &otherJid)
{
    auto r = chatRooms.find(roomId);
    if(r == chatRooms.end())
    {
        CHAT_LOG_ERROR("Room not found: %s", roomId.c_str());
        CHAT_LOG_ERROR("message jid = %s", roomId.c_str());
        return;
    }

    CHAT_LOG_DEBUG("Room found: %s", roomId.c_str());
    SharedChatRoomMember<M, SP> member(otherJid);
    r->second->addGroupMember(member);
    std::string m("User");
    m.append(member->getId()).append(" has entered the room");
    sendGeneralMessage(m);

    CHAT_LOG_DEBUG("testing room ownership");
    if(r->second->ownsRoom())
    {
        CHAT_LOG_INFO("Kicking off encryption.");
        r->second->beginEncryptionProtocolProcess();
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::startSelfPings(int intervalSec)
{
    pingPeer(strophe::getBareJidFromJid(conn->fullJid()), intervalSec);
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::pingPeer(const std::string& peerJid, int intervalSec)
{
    mega::setInterval([this, peerJid]()
    {
        strophe::Stanza ping(*conn);
        ping.setName("iq")
            .setAttr("type", "get")
            .setAttr("to", peerJid)
            .setAttr("from", conn->fullJid())
            .setAttr("id", messageId(peerJid, std::string("ping")))
            .c("ping")
                .setAttr("xmlns", "urn:xmpp:ping");
        conn->sendIqQuery(ping, "set")
        .then([](strophe::Stanza s)
        {
            return 0;
        })
        .fail([](const promise::Error& err)
        {
            KR_LOG_ERROR("Error receiving pong\n");
            return err;
        });
    }, intervalSec*1000);
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::sendMessage(const std::string& roomJid, const std::string& message)
{
    if (chatRooms.find(roomJid) != chatRooms.end())
    {
        chatRooms[roomJid]->filterOutgoing(message);
    }
    else
    {
        CHAT_LOG_WARNING("invalid room Jid %s\n", roomJid.c_str());
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::sendPrivateMessage(const std::string& peerFullJid, const std::string& message)
{
    strophe::Stanza stanza_message(*this->conn);
    stanza_message.setName("message")
                  .setAttr("id", generateMessageId(peerFullJid, message))
                  .setAttr("from", std::string(conn->fullJid()))
                  .setAttr("to", peerFullJid)
                  .setAttr("type", "chat")
                  .c("body")
                      .c("messageContents")
                          .t(message);
    conn->send(stanza_message);
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::leaveRoom(const std::string& roomJid)
{
    std::string room_nick = roomJid;
    std::string nickName = getNickname();
    if(!nickName.empty())
    {
        room_nick.append("/" + nickName);
    }
    //string presenceid = conn->getUniqueId();
    strophe::Stanza leave_message(*conn);
    leave_message.setName("presence")
                 .setAttr("to", room_nick)
                 .setAttr("type", "unavailable");

    conn->send(leave_message);
    sendBroadcastAction("conv_end", roomJid);

    /*after leaving the chat room, remove the chat room from client's chat room list.*/
    if (chatRooms.find(roomJid) != chatRooms.end())
    {
        chatRooms.erase(roomJid);
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::joinRoom(const std::string& roomJid, const std::string& password,
    const MessageMeta& meta)
{
    // if the chat room does not exist.
    if( chatRooms.find(roomJid) == chatRooms.end())
    {
        CHAT_LOG_DEBUG("join: Creating Room: %s", roomJid.c_str());
        std::shared_ptr<ChatRoom<M, GM, SP, EH>> chatRoom(
            new ChatRoom<M, GM, SP, EH>(*this, roomJid, meta.participants));
        chatRoom->roomSetup(conn->fullJid());
        chatRooms.insert(std::pair<std::string,
            std::shared_ptr<ChatRoom<M, GM, SP, EH>>>(roomJid, chatRoom));

        // TODO: Move this to ChatRoom.
        //Perform room stuff.

        //
        message_bus::SharedMessage<M_MESS_PARAMS> busMessage(ROOM_ADDED_EVENT);
        busMessage->addValue(ROOM_ADDED_JID, roomJid);
        message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()->alertListeners(ROOM_ADDED_EVENT, busMessage);
    }

    std::string room_nick = roomJid;
    std::string nickName = getNickname();
    if(!nickName.empty())
    {
        room_nick.append("/" + strophe::escapeNode(nickName));// escapeNode(nickName)
    }
    strophe::Stanza pres(*conn);
    pres.setName("presence")
        .setAttr("from", conn->fullJid())
        .setAttr("to", room_nick)
        .c("x")
            .setAttr("xmlns", "http://jabber.org/protocol/muc")
            .c("password");

    conn->send(pres);
    sendBroadcastAction("conv-start", roomJid);
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::invite(const std::string &peerMail)
{
    api->call(&mega::MegaApi::getUserData, peerMail.c_str())
    .then([this](ReqResult result)
    {
        const char* peer = result->getText();
        const char* pk = result->getPassword();
        CHAT_LOG_DEBUG("\n----> pk = %s", pk);
        CHAT_LOG_DEBUG("----> pk_size = %zu", strlen(pk));
        if (!peer)
            throw std::runtime_error("Returned peer user is NULL");

        std::string peerJid = std::string(peer)+"@"+KARERE_XMPP_DOMAIN;
        return karere::ChatRoom<M, GM, SP, EH>::create(*this, peerJid);
    })
    .then([this](std::shared_ptr<karere::ChatRoom<M, GM, SP, EH>> room)
    {
        if (chatRooms.find(room->roomJid()) != chatRooms.end())
        {
            chatRooms[room->roomJid()]->addUserToChat(room->peerFullJid());
        }
        else
        {
            CHAT_LOG_WARNING("invalid room Jid %s\n", room->roomJid().c_str());
        }
        return nullptr;
    })
    .fail([this](const promise::Error& err)
    {
        if (err.type() == 0x3e9aab10)
        {
            sendErrorMessage("Callee user not recognized");
        }
        else
        {
            std::string error("Error calling user:");
            error.append(err.msg().c_str());
            sendErrorMessage(error);
        }
        return nullptr;
    });
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::sendChatState(const std::string& roomJid, const ChatState::STATE_TYPE chatState)
{
    if (chatRooms.find(roomJid) != chatRooms.end())
    {
        std::string myJid =  strophe::getBareJidFromJid(std::string(xmpp_conn_get_bound_jid(*this->conn)));
        chatRooms[roomJid]->sendUserChatState(myJid, chatState);
    }
    else
    {
        CHAT_LOG_WARNING("invalid room Jid %s\n", roomJid.c_str());
    }
}

template<class M, class GM, class SP, class EH>
void Client<M,GM,SP,EH>::setPresence(const Presence pres, const int delay)
{
    strophe::Stanza msg(*conn);
    msg.setName("presence")
       .setAttr("id", generateMessageId(std::string("presence"), std::string("")))
       .c("show")
           .t(ContactList::presenceToText(pres))
           .up()
       .c("status")
           .t(ContactList::presenceToText(pres))
           .up();

    if(delay > 0)
    {
        msg.c("delay")
                .setAttr("xmlns", "urn:xmpp:delay")
                .setAttr("from", conn->fullJid());
    }
    conn->send(msg);
}

template<class M, class GM, class SP, class EH>
promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
Client<M,GM,SP,EH>::getOtherUserInfo(std::string &emailAddress)
{
    std::string event(USER_INFO_EVENT);
    event.append(emailAddress);
    message_bus::SharedMessage<M_MESS_PARAMS> userMessage(event);

    return api->call(&mega::MegaApi::getUserData, emailAddress.c_str())
    .then([this, userMessage](ReqResult result)
    {
        //const char *peer = result->getText();
        //const char *pk = result->getPassword();

        return userMessage;
    });
/*
 * //av: cant return nullptr instead of SharedMessage. This works only for a class with constructor that can take nullptr as argument, and the ctor is not marked as explicit
   .fail([&](const promise::Error &err)
    {
        return nullptr;
    });
*/
}

template<class M, class GM, class SP, class EH>
promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
Client<M,GM,SP,EH>::getThisUserInfo()
{
    std::string event(THIS_USER_INFO_EVENT);
    message_bus::SharedMessage<M_MESS_PARAMS> userMessage(event);

    return api->call(&mega::MegaApi::getUserData)
    .then([this, userMessage](ReqResult result)
    {
        return userMessage; //av: was nullptr, but compile error - Promise<return type of this> must match function return type
    })
    .fail([&](const promise::Error &err)
    {
        return userMessage; //av: same here - was nullptr but compile error
    });
}

//tell compielr to create instances for our template class with these template params
//this allows us to define the methods in a .cpp file.
template class Client<MPENC_T_PARAMS>;

}
