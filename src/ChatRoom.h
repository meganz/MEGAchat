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
    std::string mMyJid;
    std::string mPeerJid;
    std::string mRoomJid;
    std::string mMyRoomJid;
    std::string mPeerRoomJid;
    ChatRoom(const std::string& me, const std::string& peer,
             const std::string& roomJid, const std::string& myRoomJid,
             const std::string& peerRoomJid)
    :mMyJid(me), mPeerJid(peer), mRoomJid(roomJid), mMyRoomJid(myRoomJid),
    mPeerRoomJid(peerRoomJid)
    {}
public:
    promise::Promise<std::shared_ptr<ChatRoom> >
    static create(Client& client, const std::string& peer);
    const std::string& myJid() const {return mMyJid;}
    const std::string& peerJid() const {return mPeerJid;}
    const std::string& roomJid() const {return mRoomJid;}
    const std::string& myRoomJid() const {return mMyRoomJid;}
    const std::string& peerRoomJid() const {return mPeerRoomJid;}
};
}

#endif // CHATROOM_H
