#include <mstrophepp.h>
#include "chatRoom.h"
#include "chatCommon.h"
#include "chatClient.h"
#include <mega/base64.h>
#include <openssl/sha.h>

using namespace std;
using namespace strophe;
using namespace promise;
namespace nsmega = mega;

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
    string myId = strophe::getNodeFromJid(conn->fullJid());
    string peerId = strophe::getNodeFromJid(peerFullJid);
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
    string jidstr;
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
    string roomJid;
    roomJid.reserve(64);
    roomJid.append(b32).append("@conference.").append(KARERE_XMPP_DOMAIN);
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
    .then([ret, room](int) mutable
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

Promise<int> ChatRoom::addUserToChat(const string& peerFullJid)
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
}
