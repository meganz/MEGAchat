#include <mstrophepp.h>
#include "ChatRoom.h"
#include "ChatCommon.h"
#include "ChatClient.h"

using namespace std;
using namespace strophe;
using namespace promise;

namespace karere
{
ChatRoom::ChatRoom(Client& aClient, const std::string& peerFullJid,
         const std::string& roomJid, const std::string& myRoomJid,
         const std::string& peerRoomJid)
:client(aClient), mPeerFullJid(peerFullJid), mRoomJid(roomJid),
  mMyRoomJid(myRoomJid), mPeerRoomJid(peerRoomJid),
  mMyFullJid(client.conn->fullJid()) {}

Promise<shared_ptr<ChatRoom> >
ChatRoom::create(Client& client, const std::string& peerFullJid)
{
    shared_ptr<Connection> conn = client.conn;
    const char* myFullJid = xmpp_conn_get_bound_jid(*conn);
    if (!myFullJid)
        throw runtime_error("Error getting own JID from connection");
    string myBareJid = strophe::getBareJidFromJid(myFullJid);
    string peerBareJid = strophe::getBareJidFromJid(peerFullJid);
    string myId = userIdFromJid(conn->fullJid());
    string peerId = userIdFromJid(peerFullJid);
    string roomJid = "prv_";
    roomJid.reserve(64);
    string* id1;
    string* id2;
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
    roomJid.append(to_string(fastHash(*id1)))
            .append("_").append(to_string(fastHash(*id2)))
            .append("@conference.").append(KARERE_XMPP_DOMAIN);
    string myRoomJid = roomJid;
    myRoomJid.append("/").append(myId).append("__")
            .append(strophe::getResourceFromJid(myFullJid));
    string peerRoomJid = roomJid;
    peerRoomJid.append("/").append(peerId).append("__")
            .append(strophe::getResourceFromJid(peerFullJid));

    strophe::Stanza pres(*conn);
    pres.setName("presence")
        .setAttr("xmlns", "jabber:client")
        .setAttr("from", myFullJid)
        .setAttr("to", myRoomJid)
        .c("x")
            .setAttr("xmlns", "http://jabber.org/protocol/muc")
           .c("password");

    shared_ptr<ChatRoom> room(new ChatRoom(client, peerFullJid, roomJid,
        myRoomJid, peerRoomJid));
    Promise<shared_ptr<ChatRoom> > ret;

    conn->sendQuery(pres, "muc")
    .then([ret, room, conn](Stanza result) mutable
     {
        Stanza config(*conn);

        config.setName("iq")
            .setAttr("to", room->roomJid())
            .setAttr("type", "set")
            .setAttr("xmlns", "jabber:client")
            .c("query")
                .setAttr("xmlns", "http://jabber.org/protocol/muc#owner")
                .c("x")
                    .setAttr("xmlns", "jabber:x:data")
                    .setAttr("type", "submit");
        return conn->sendIqQuery(config, "config");
     })
     .then([room](Stanza s)
    {
        return room->addUserToChat(room->peerFullJid());
    })
    .then([ret, room](Stanza s) mutable
    {
        ret.resolve(room);
        return 0;
    })
    .fail([ret](const promise::Error& err) mutable
    {
        ret.reject(err);
        KR_LOG_ERROR("Room creation failed");
        return 0;
    });
    return ret;
};

Promise<Stanza> ChatRoom::addUserToChat(const string& peerFullJid)
{
    Stanza grant(*client.conn);
    grant.setName("iq")
            .setAttr("from", myFullJid())
            .setAttr("to", roomJid())
            .setAttr("type", "set")
            .setAttr("xmlns", "jabber:client")
            .c("query")
                .setAttr("xmlns", "http://jabber.org/protocol/muc#admin")
                .c("item")
                    .setAttr("affiliation", "owner")
                    .setAttr("jid", peerFullJid);

    return client.conn->sendIqQuery(grant)
    .then([this, peerFullJid](Stanza) mutable
     {
        string json = "{\"ctime\":";
        json.append(to_string(time(nullptr)))
            .append(",\"invitationType\":\"created\",\"participants\":[\"")
            .append(strophe::getBareJidFromJid(myFullJid())).append("\",\"")
            .append(strophe::getBareJidFromJid(peerFullJid)).append("\"],\"users\":{\"")
            .append(myFullJid()).append("\":\"moderator\"");
        auto peers = client.contactList.getFullJidsOfJid(peerFullJid);
        for (auto& p: peers)
            json.append(",\"").append(p)+="\":\"moderator\"";
        auto mine = client.contactList.getFullJidsOfJid(myFullJid());
        for (auto& m: mine)
            json.append(",\"").append(m)+="\":\"moderator\"";

        json.append("},\"type\":\"private\"}");
        printf("json = %s\n", json.c_str());

        Stanza invite(*client.conn);
        invite.setName("message")
            .setAttr("from", myFullJid())
            .setAttr("to", peerFullJid)
            .setAttr("xmlns", "jabber:client")
            .c("x")
                .setAttr("xmlns", "jabber:x:conference")
                .setAttr("jid", roomJid())
                .setAttr("json", json);
        return client.conn->sendQuery(invite, "invite");
     })
    .fail([](const promise::Error& err) mutable
     {
        printf("Error adding user to chatroom: %s\n", err.msg().c_str());
        return promise::reject<Stanza>(err);
     });
}
}
