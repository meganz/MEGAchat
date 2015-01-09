#ifndef CHATROOM_H
#define CHATROOM_H
#include <mstrophepp.h>
#include "ChatCommon.h"
namespace karere
{
class Client;

class ChatRoom
{
protected:
    Client& client;
    std::string mPeerFullJid;
    std::string mRoomJid;
    std::string mMyRoomJid;
    std::string mPeerRoomJid;
    std::string mMyFullJid;
    ChatRoom(Client& aClient, const std::string& peerFullJid,
             const std::string& roomJid, const std::string& myRoomJid,
             const std::string& peerRoomJid);
public:
    promise::Promise<std::shared_ptr<ChatRoom> >
    static create(Client& client, const std::string& peer);
    const std::string& myFullJid() const {return mMyFullJid;}
    const std::string& peerFullJid() const {return mPeerFullJid;}
    const std::string& roomJid() const {return mRoomJid;}
    const std::string& myRoomJid() const {return mMyRoomJid;}
    const std::string& peerRoomJid() const {return mPeerRoomJid;}
    promise::Promise<strophe::Stanza> addUserToChat(const std::string& fullJid);
};
}

#endif // CHATROOM_H
