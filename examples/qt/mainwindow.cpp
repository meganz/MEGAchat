#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <string>
#include <videoRenderer_Qt.h>
#include <gcm.h>
#include "rtcModule/IRtcModule.h"
#include <services-dns.hpp>
#include <services-http.hpp>
#include <iostream>
#include <rapidjson/document.h>
#include <sdkApi.h>
#include <chatClient.h>
#include <messageBus.h>

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
extern std::unique_ptr<karere::ChatClient> gClient;

using namespace std;
using namespace mega;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    message_bus::MessageListener<M_MESS_PARAMS> l = {
        "guiRoomListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string rm(ROOM_ADDED_JID);

            std::string roomJid = message->getValue<std::string>(rm);
            roomAdded(roomJid);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(ROOM_ADDED_EVENT, l);

    message_bus::MessageListener<M_MESS_PARAMS> c = {
        "guiContactListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string contactJid = message->getValue<std::string>(CONTACT_JID);
            karere::Presence oldState = message->getValue<karere::Presence>(CONTACT_OLD_STATE);
            karere::Presence newState = message->getValue<karere::Presence>(CONTACT_STATE);
            contactStateChange(contactJid, oldState, newState);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(CONTACT_CHANGED_EVENT, c);

    message_bus::MessageListener<M_MESS_PARAMS> ca = {
        "guiContactListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string contactJid = message->getValue<std::string>(CONTACT_JID);
            contactAdded(contactJid);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(CONTACT_ADDED_EVENT, ca);

    message_bus::MessageListener<M_MESS_PARAMS> o = {
        "guiErrorListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener) {
            std::string errMsg =
                    message->getValue<std::string>(ERROR_MESSAGE_CONTENTS);
            handleError(errMsg);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(ERROR_MESSAGE_EVENT, o);

    message_bus::MessageListener<M_MESS_PARAMS> p = {
        "guiGeneralListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string generalMessage("GENERAL: ");
            generalMessage.append(
                    message->getValue<std::string>(GENERAL_EVENTS_CONTENTS));
            handleMessage(generalMessage);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(GENERAL_EVENTS, p);

    message_bus::MessageListener<M_MESS_PARAMS> q = {
        "guiWarningListener",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string warMsg =
                    message->getValue<std::string>(WARNING_MESSAGE_CONTENTS);
            handleWarning(warMsg);
        }
    };

    message_bus::SharedMessageBus<M_BUS_PARAMS>::
        getMessageBus()->addListener(WARNING_MESSAGE_EVENT, q);

    ui->setupUi(this);
}

http::Client* client = nullptr;

extern bool inCall;

void MainWindow::handleMessage(std::string &message) {
    QString chatMessage = ui->chatTextEdit->toPlainText() + "\n" + message.c_str();
    ui->chatTextEdit->setPlainText(chatMessage);
    QTextCursor c = ui->chatTextEdit->textCursor();
    c.movePosition(QTextCursor::End);
    ui->chatTextEdit->setTextCursor(c);
}

void MainWindow::contactStateChange(std::string &contactJid, karere::Presence oldState, karere::Presence newState) {
    std::string msg = std::string("contact ") + contactJid + std::string(" changed from ") + karere::ContactList::presenceToText(oldState) + std::string(" to ") + karere::ContactList::presenceToText(newState);
    handleMessage(msg);
}

void MainWindow::contactAdded(std::string &contactJid) {
	printf("add contact %s\n", contactJid.c_str());
	ui->contactList->addItem(new QListWidgetItem(QIcon("/images/online.png"), tr(contactJid.c_str())));
}

void MainWindow::roomAdded(std::string &roomJid) {
    this->chatRoomJid = roomJid;
    std::string listenEvent(ROOM_MESSAGE_EVENT);
    listenEvent.append(this->chatRoomJid);
    CHAT_LOG_DEBUG("************* eventName = %s", listenEvent.c_str());
    message_bus::MessageListener<M_MESS_PARAMS> l {
        "guiListenerNoOne",
        [this](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string data = message->getValue<std::string>(ROOM_MESSAGE_CONTENTS);
            handleMessage(data);
        }
    };
    message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()->addListener(listenEvent, l);
}

void MainWindow::inviteButtonPushed()
{
    std::string peerMail = ui->calleeInput->text().toLatin1().data();
    if (peerMail.empty())
    {
        QMessageBox::critical(this, "Error", "Invalid user entered in peer input box");
        return;
    }

    gClient->invite(peerMail);

}

void MainWindow::handleError(std::string &message) {
    QMessageBox::critical(this, "Error", message.c_str());
}

void MainWindow::handleWarning(std::string &message) {
    std::string warningMessage("WARNING: ");
    warningMessage.append(message);
    handleMessage(warningMessage);
}

//void MainWindow::
void MainWindow::leaveButtonPushed()
{
    gClient->leaveRoom(chatRoomJid);
}

void MainWindow::sendButtonPushed()
{
    std::string message = ui->typingTextEdit->toPlainText().toUtf8().constData();
    gClient->sendMessage(chatRoomJid, message);
    ui->typingTextEdit->setPlainText("");
}

void MainWindow::buttonPushed()
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
            return karere::ChatRoom<MPENC_T_PARAMS>::create(*gClient, peerJid);
        })
        .then([this](shared_ptr<karere::ChatRoom<MPENC_T_PARAMS>> room)
        {
            rtcModule::AvFlags av;
            av.audio = true;
            av.video = true;
            char sid[rtcModule::RTCM_SESSIONID_LEN+2];
            gClient->rtc->startMediaCall(sid, room->peerFullJid().c_str(), av, nullptr);
            this->chatRoomJid = room->roomJid();
            gClient->chatRooms[chatRoomJid]->addUserToChat(room->peerFullJid());
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


    /*dnsLookup("google.com", 0)
    .then([](std::shared_ptr<AddrInfo> result)
    {
        printf("Canonical name: %s\n", result->canonName().c_str());
        auto& ip4s = result->ip4addrs();
        for (auto& ip: ip4s)
            printf("ipv4: %s\n", ip.toString());
        auto& ip6s = result->ip6addrs();
        for (auto& ip: ip6s)
            printf("ipv6: %s\n", ip.toString());

        return nullptr;
    })
    .fail([](const promise::Error& err)
    {
        printf("DNS lookup error: %s\n", err.msg().c_str());
        return nullptr;
    });*/

}
void MainWindow::onAudioInSelected()
{
    auto combo = ui->audioInCombo;
    int ret = gClient->rtc->selectAudioInDevice(combo->itemText(combo->currentIndex()).toAscii().data());
    if (ret < 0)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    printf("selected audio device: %d\n", ret);
}

void MainWindow::onVideoInSelected()
{
    auto combo = ui->videoInCombo;
    int ret = gClient->rtc->selectVideoInDevice(combo->itemText(combo->currentIndex()).toAscii().data());
    if (ret < 0)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    printf("selected video device: %d\n", ret);
}

MainWindow::~MainWindow()
{
    delete ui;
}

