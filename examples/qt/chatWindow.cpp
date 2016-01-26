#include "chatWindow.h"
#include "mainwindow.h"

ChatWindow::ChatWindow(karere::ChatRoom& room, MainWindow& parent): QDialog(&parent), mRoom(room),
    mainWindow(parent)
{
    ui.setupUi(this);
    ui.mSplitter->setStretchFactor(0,1);
    ui.mSplitter->setStretchFactor(1,0);
    ui.mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.mMsgSendBtn, SIGNAL(clicked()), this, SLOT(onMsgSendBtn()));
    connect(ui.mMessageEdit, SIGNAL(sendMsg()), this, SLOT(onMsgSendBtn()));
    connect(ui.mMessageEdit, SIGNAL(editLastMsg()), this, SLOT(editLastMsg()));
    connect(ui.mMessageList, SIGNAL(requestHistory()), this, SLOT(onMsgListRequestHistory()));
    connect(ui.mVideoCallBtn, SIGNAL(clicked(bool)), this, SLOT(onVideoCallBtn(bool)));
    connect(ui.mAudioCallBtn, SIGNAL(clicked(bool)), this, SLOT(onAudioCallBtn(bool)));
    ui.mAudioCallBtn->hide();
    ui.mVideoCallBtn->hide();
    ui.mChatdStatusDisplay->hide();
    QDialog::show();
}

ChatWindow::~ChatWindow()
{
    mMessages->setListener(static_cast<chatd::Listener*>(&mRoom));
}
