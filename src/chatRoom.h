#ifndef CHATROOM_H
#define CHATROOM_H

#include "chatCommon.h"
#include "karereCommon.h"
#include <mstrophepp.h>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <iostream>
#include <openssl/sha.h>
#include <mega/base64.h>
#include "chatClient.h"

namespace nsmega = mega;

namespace karere
{

class Client;

/**
 * @brief Represents a XMPP chatroom Encapsulates the members and handlers
 * required to process transactions with the room and other members of the room.
 */
class XmppChatRoom
{
public:
    /*
     * @param aClient reference to a client-owner of chat room
     * @param peerFullJid peer's JID
     * @param roomJid room's JID
     * @param myRoomJid room's JID of owner's
     * @param peerRoomJid room's JID of peer's
     * @constructor
    */
    XmppChatRoom(Client& aClient, const std::string& peerFullJid,
        const std::string& roomJid, const std::string& myRoomJid,
        const std::string& peerRoomJid)
    : client(aClient), mPeerFullJid(peerFullJid), mRoomJid(roomJid), mMyRoomJid(myRoomJid),
      mPeerRoomJid(peerRoomJid), mMyFullJid(client.conn->fullJid())
    {}

    const std::string& myRoomJid() const { return mMyRoomJid; }
    const std::string& myFullJid() const { return mMyFullJid; }
    const std::string& peerFullJid() const { return mPeerFullJid; }
    const std::string& roomJid() const { return mRoomJid; }
    bool isOwner = false;
    /*
     * @param aClient {Client} reference to a client-owner of chat room
     * @param peer {string} peer's JID
     * @factory function to create XmppChatRoom object.
     * @return {shared_ptr<XmppChatRoom>} a smart pointer pointing to a new XmppChatRoom object.
    */
    static promise::Promise<std::shared_ptr<XmppChatRoom> >
    create(Client& client, const std::string& peerFullJid)
    {
        const char* myFullJid = client.conn->fullJid();

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

        std::shared_ptr<XmppChatRoom> room(new XmppChatRoom(client, peerFullJid, roomJid,
            myRoomJid, peerRoomJid));
        promise::Promise<std::shared_ptr<XmppChatRoom>> ret;

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
//            client.mTextModule->chatRooms.insert(std::pair<std::string, std::shared_ptr<XmppChatRoom>>(room->roomJid(), room));
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
    }
   promise::Promise<void> addUserToChat(const std::string& peerBareId) const
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

            const auto& resources = client.xmppContactList().getContact(peerBareId).resources();
            for (auto& p: resources)
            {
                json.append(",\"").append(peerBareId)+='@';
                json.append(KARERE_XMPP_DOMAIN)+='/';
                json.append(p.first).append("\":\"moderator\"");
            }
            auto myBareJid = strophe::getBareJidFromJid(myFullJid());
            const auto& mine = client.xmppContactList().getContact(myBareJid).resources();
            for (auto& m: mine)
            {
                json.append(",\"").append(myBareJid)+='/';
                json.append(m.first).append("\":\"moderator\"");
            }

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
            //TODO: Invite ack?
         })
        .fail([](const promise::Error& err) mutable
         {
            printf("Error adding user to chatroom: %s\n", err.msg().c_str());
            return err;
         });
    }
public:

    /**
     * @brief Test if we are the owner of this room.
     */
    bool ownsRoom() {
        return isOwner;
    }

protected:

    /**
     * @brief The client for this room.
     */
    Client& client;
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
};

}

#endif // CHATROOM_H
