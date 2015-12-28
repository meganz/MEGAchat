#include "callGui.h"
#include "chatWindow.h"
#include <mega/base64.h> //for base32
#include <chatRoom.h>
using namespace std;
using namespace mega;
using namespace karere;

void CallGui::onCallBtn(bool)
{
    if (!mCall)
        return;
    mCall->hangup();
}

void CallGui::call()
{
    chatd::Id peer = static_cast<PeerChatRoom&>(
                static_cast<ChatWindow*>(parent())->mRoom).peer();
    char buf[16];
    auto len = ::mega::Base32::btoa((byte*)&peer.val, 8, buf);
    assert(len < 16);
    buf[len] = 0;
    string peerJid = string(buf)+"@"+KARERE_XMPP_DOMAIN;
    karere::XmppChatRoom::create(*gClient, peerJid)
    .then([this](shared_ptr<karere::XmppChatRoom> room)
    {
        rtcModule::AvFlags av(true,true);
        gClient->rtc->startMediaCall(this, room->peerFullJid(), av, nullptr);
        room->addUserToChat(room->peerFullJid());
    })
    .fail([this](const promise::Error& err)
    {
        if (err.type() == 0x3e9aab10)
            QMessageBox::critical(this, "Error", "Callee user not recognized");
        else
            QMessageBox::critical(this, "Error", QString("Error calling user:")+err.msg().c_str());
        return err;
    });
}

