#include "callGui.h"
#include "chatWindow.h"
#include <mega/base64.h> //for base32
#include <chatRoom.h>
#include "chatWindow.h"
#include "mainwindow.h"

using namespace std;
using namespace mega;
using namespace karere;

CallGui::CallGui(ChatWindow &parent, const std::shared_ptr<rtcModule::ICall>& call)
: QWidget(&parent), mChatWindow(parent), mCall(call)
{
    ui.setupUi(this);
    connect(ui.mHupBtn, SIGNAL(clicked(bool)), this, SLOT(onHupBtn(bool)));
    connect(ui.mShowChatBtn, SIGNAL(clicked(bool)), this, SLOT(onChatBtn(bool)));
    connect(ui.mMuteMicChk, SIGNAL(stateChanged(int)), this, SLOT(onMuteMic(int)));
    connect(ui.mMuteCamChk, SIGNAL(stateChanged(int)), this, SLOT(onMuteCam(int)));
    connect(ui.mFullScreenChk, SIGNAL(stateChanged(int)), this, SLOT(onFullScreenChk(int)));
    if (mCall)
    {
        mCall->changeEventHandler(this);
        mCall->changeLocalRenderer(ui.localRenderer);
    }
}


void CallGui::onHupBtn(bool)
{
    if (!mCall)
        return;
    mCall->hangup();
}
void CallGui::onMuteMic(int state)
{
    AvFlags av(true, false);
    mCall->muteUnmute(av, !state);
}
void CallGui::onMuteCam(int state)
{
    AvFlags av(false, true);
    mCall->muteUnmute(av, !state);
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

void CallGui::onCallEnded(rtcModule::TermCode code, const std::string& text,
    const std::shared_ptr<rtcModule::stats::IRtcStats>& statsObj)
{
    mCall.reset();
    mChatWindow.deleteCallGui();
}
void CallGui::onPeerMute(AvFlags what)
{
    if (!what.video)
        return;
    auto image = new QImage(ui.remoteRenderer->size(), QImage::Format_ARGB32);
    MainWindow::drawAvatar(static_cast<PeerChatRoom&>(mChatWindow.mRoom).contact(), *image);
    ui.remoteRenderer->setStaticImage(image);
    ui.remoteRenderer->showStaticImage();
}

void CallGui::onChatBtn(bool)
{
    auto& txtChat = *mChatWindow.ui.mTextChatWidget;
    if (txtChat.isVisible())
        txtChat.hide();
    else
        txtChat.show();
}
CallAnswerGui::CallAnswerGui(MainWindow& parent, const std::shared_ptr<rtcModule::ICallAnswer>& ans)
:QObject(&parent), mParent(parent), mAns(ans),
  mContact(parent.client().contactList->contactFromJid(ans->call()->peerJid()))
{
    if (!mContact)
        throw std::runtime_error("Incoming call from unknown contact");

    msg.reset(new QMessageBox(QMessageBox::Information,
        "Incoming call", QString::fromStdString(mContact->titleString()+" is calling you"),
        QMessageBox::NoButton, &mParent));
    answerBtn = msg->addButton("Answer", QMessageBox::AcceptRole);
    rejectBtn = msg->addButton("Reject", QMessageBox::RejectRole);
    msg->setWindowModality(Qt::NonModal);
    QObject::connect(msg.get(), SIGNAL(buttonClicked(QAbstractButton*)),
        this, SLOT(onBtnClick(QAbstractButton*)));
    msg->show();
    msg->raise();
}

void CallAnswerGui::onLocalStreamObtained(rtcModule::IVideoRenderer*& renderer)
{
    renderer = new rtcModule::NullRenderer;
}

void CallAnswerGui::onSession()
{
//handover event handling and local video renderer to chat window
    static_cast<ChatWindow&>(mParent.chatWindowForPeer(mContact->userId())).createCallGui(mAns->call());
    delete this;
}
