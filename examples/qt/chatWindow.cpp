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
    connect(ui.mMembersBtn, SIGNAL(clicked(bool)), this, SLOT(onMembersBtn(bool)));
    ui.mAudioCallBtn->hide();
    ui.mVideoCallBtn->hide();
    ui.mChatdStatusDisplay->hide();
    if (!mRoom.isGroup())
        ui.mMembersBtn->hide();
    else
        setAcceptDrops(true);
    QDialog::show();
}

ChatWindow::~ChatWindow()
{
    mMessages->setListener(static_cast<chatd::Listener*>(&mRoom));
}
void ChatWindow::createMembersMenu(QMenu& menu)
{
    assert(mRoom.isGroup());
    auto& room = static_cast<karere::GroupChatRoom&>(mRoom);
    if (room.peers().empty())
    {
        menu.addAction(tr("You are alone in this chatroom"))->setEnabled(false);
        return;
    }
    for (auto& item: room.peers())
    {
        auto entry = menu.addMenu(QString::fromStdString(item.second->name()));
        auto actRemove = entry->addAction(tr("Remove from chat"));
        actRemove->setData(QVariant(item.first));
        auto actSetPriv = entry->addAction(tr("Set privilege"));
        actSetPriv->setData(QVariant(item.first));
        auto actPrivChat = entry->addAction(tr("Send private message"));
        actPrivChat->setData(QVariant(item.first));
        connect(actRemove, SIGNAL(triggered()), SLOT(onMemberRemove()));
        connect(actSetPriv, SIGNAL(triggered()), SLOT(onMemberSetPriv()));
        connect(actPrivChat, SIGNAL(triggered()), SLOT(onMemberPrivateChat()));
    }
}
uint64_t handleFromAction(QObject* object)
{
    if (!object)
        throw std::runtime_error("handleFromAction: NULL object provided");
    auto action = qobject_cast<QAction*>(object);
    bool ok;
    uint64_t userid = action->data().toULongLong(&ok);
    assert(ok);
    return userid;
}

void ChatWindow::onMemberRemove()
{
    uint64_t handle(handleFromAction(QObject::sender()));
    mainWindow.client().api->call(&mega::MegaApi::removeFromChat, mRoom.chatid(), handle)
    .fail([this](const promise::Error& err)
    {
        QMessageBox::critical(nullptr, tr("Remove member from group chat"),
            tr("Error removing member from group chat: %1")
            .arg(QString::fromStdString(err.msg())));
        return err;
    });
}
void ChatWindow::onMemberSetPriv()
{
//    static_cast<karere::GroupChatRoom&>(mRoom).setPriv(handleFromAction(QObject::sender()));
    QMessageBox::critical(this, tr("Set member privilege"), tr("Not implemented yet"));
}
void ChatWindow::onMemberPrivateChat()
{
    auto& clist = *mainWindow.client().contactList;
    auto it = clist.find(handleFromAction(QObject::sender()));
    if (it == clist.end())
    {
        QMessageBox::critical(this, tr("Send private message"), tr("Person is not a contact of ours"));
        return;
    }
    it->second->gui().showChatWindow();
}
void ChatWindow::onMembersBtn(bool)
{
    QMenu menu(this);
    createMembersMenu(menu);
    menu.setLayoutDirection(Qt::RightToLeft);
    menu.exec(ui.mMembersBtn->mapToGlobal(QPoint(-menu.width(), ui.mMembersBtn->height())));
}
