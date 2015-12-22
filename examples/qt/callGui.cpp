#include "callGui.h"

using namespace std;
using namespace mega;
using namespace karere;


extern bool inCall;
extern karere::IGui::ICallGui* gCallGui = nullptr;

void CallGui::callBtnPushed()
{
    if (inCall)
    {
        gClient->rtc->hangupAll("hangup", nullptr);
        inCall = false;
        ui->callBtn->setText("Call");
    }
    else
    {
        std::string peerMail = ui->calleeInput->text().toLatin1().data();
        if (peerMail.empty())
        {
            QMessageBox::critical(this, "Error", "Invalid user entered in peer input box");
            return;
        }
        gClient->api->call(&MegaApi::getUserData, peerMail.c_str())
        .then([this](ReqResult result)
        {
            const char* peer = result->getText();
            if (!peer)
                throw std::runtime_error("Returned peer user is NULL");

            string peerJid = string(peer)+"@"+KARERE_XMPP_DOMAIN;
            return karere::XmppChatRoom::create(*gClient, peerJid);
        })
        .then([this](shared_ptr<karere::XmppChatRoom> room)
        {
            rtcModule::AvFlags av;
            av.audio = true;
            av.video = true;
            char sid[rtcModule::RTCM_SESSIONID_LEN+2];
            gClient->rtc->startMediaCall(sid, room->peerFullJid().c_str(), av, nullptr);
            this->chatRoomJid = room->roomJid();
            gClient->mTextModule->chatRooms[chatRoomJid]->addUserToChat(room->peerFullJid());
            inCall = true;
            ui->callBtn->setText("Hangup");
            return nullptr;
        })
        .fail([this](const promise::Error& err)
        {
            if (err.type() == 0x3e9aab10)
                QMessageBox::critical(this, "Error", "Callee user not recognized");
            else
                QMessageBox::critical(this, "Error", QString("Error calling user:")+err.msg().c_str());
            return nullptr;
        });
    }
}

