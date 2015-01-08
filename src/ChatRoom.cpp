#include <mstrophepp.h>
#include "ChatRoom.h"
#include "ChatCommon.h"
#include "ChatClient.h"

using namespace std;
using namespace strophe;
using namespace promise;

namespace karere
{
Promise<shared_ptr<ChatRoom> >
ChatRoom::create(Client& client, const std::string& peer)
{
    strophe::Connection& conn = *client.conn;
    const char* me = xmpp_conn_get_bound_jid(conn);
    if (!me)
        throw runtime_error("Error getting own JID from connection");
    string myId = userIdFromJid(me);
    string peerId = userIdFromJid(peer);
    string roomJid = "prv_";
    roomJid.reserve(64);
    string* id1;
    string* id2;
    if (me <= peer)
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
            .append(strophe::getResourceFromJid(me));
    string peerRoomJid = roomJid;
    peerRoomJid.append("/").append(peerId).append("__")
            .append(strophe::getResourceFromJid(peer));

    strophe::Stanza pres(conn);
    pres.setName("presence")
        .setAttr("xmlns", "jabber:client")
        .setAttr("from", me)
        .setAttr("to", myRoomJid)
        .c("x")
            .setAttr("xmlns", "http://jabber.org/protocol/muc")
           .c("password");

    shared_ptr<ChatRoom> room(new ChatRoom(me, peer, roomJid,
        myRoomJid, peerRoomJid));
    Promise<shared_ptr<ChatRoom> > ret;

    conn.sendQuery(pres, "muc")
    .then([ret, room, &conn](Stanza result) mutable
     {
        Stanza config(conn);

        config.setName("iq")
            .setAttr("to", room->roomJid())
            .setAttr("type", "set")
            .setAttr("xmlns", "jabber:client")
            .c("query")
                .setAttr("xmlns", "http://jabber.org/protocol/muc#owner")
                .c("x")
                    .setAttr("xmlns", "jabber:x:data")
                    .setAttr("type", "submit");
        return conn.sendIqQuery(config, "config");
     })
    .then([&conn, room](Stanza) mutable
     {
        Stanza grant(conn);
        grant.setName("iq")
                .setAttr("from", conn.jid())
                .setAttr("to", room->roomJid())
                .setAttr("type", "set")
                .setAttr("xmlns", "jabber:client")
                .c("query")
                    .setAttr("xmlns", "http://jabber.org/protocol/muc#admin")
                    .c("item")
                        .setAttr("affiliation", "owner")
                        .setAttr("jid", room->peerJid());
        return conn.sendIqQuery(grant);
     })
    .then([&conn, room](Stanza) mutable
     {
        string json = "{\"ctime\":";
        json.append(to_string(time(nullptr)))
            .append(",\"invitationType\":\"created\",\"participants\":[\"")
            .append(conn.jid()).append("\",\"")
            .append(room->peerJid()).append("\"],\"users\":{\"")
            .append(conn.jid()).append("\":\"moderator\",")
            .append(room->peerJid()).append("\":\"moderator\"},\"type\":\"private\"}");
        printf("json = %s\n", json.c_str());

        Stanza invite(conn);
        invite.setName("message")
            .setAttr("from", conn.jid())
            .setAttr("to", room->peerJid())
            .setAttr("xmlns", "jabber:client")
            .c("x")
                .setAttr("xmlns", "jabber:x:conference")
                .setAttr("jid", room->roomJid())
                .setAttr("json", json);
        return conn.sendQuery(invite, "invite");
     })
    .then([ret, room](Stanza) mutable
     {
        ret.resolve(room);
        printf("success\n");
        return 0;
     })
    .fail([ret](const promise::Error& err) mutable
     {
        ret.reject(err);
        printf("fail\n");
        return 0;
    });

    return ret;
}
}
