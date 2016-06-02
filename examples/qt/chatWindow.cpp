#include "chatWindow.h"
#include "mainwindow.h"

ChatWindow::ChatWindow(karere::ChatRoom& room, MainWindow& parent): QDialog(&parent),
    mainWindow(parent), mRoom(room), mWaitMsg(*this)
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
    connect(ui.mMessageList->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));
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
    mChat->setListener(static_cast<chatd::Listener*>(&mRoom));
}
MessageWidget::MessageWidget(ChatWindow& parent, chatd::Message& msg,
    chatd::Message::Status status, chatd::Idx idx)
: QWidget(&parent), mChatWindow(parent), mMessage(&msg),
    mIsMine(msg.userid == parent.chat().client().userId()), mIndex(idx)
{
    ui.setupUi(this);
    setAuthor(msg.userid);
    setTimestamp(msg.ts);
    setStatus(status);
    setText(msg);
    show();
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
        if (room.ownPriv() == chatd::PRIV_OPER)
        {
            auto actRemove = entry->addAction(tr("Remove from chat"));
            actRemove->setData(QVariant((qulonglong)item.first));
            auto actSetPriv = entry->addAction(tr("Set privilege"));
            actSetPriv->setData(QVariant((qulonglong)item.first));
            auto actPrivChat = entry->addAction(tr("Send private message"));
            actPrivChat->setData(QVariant((qulonglong)item.first));
            connect(actRemove, SIGNAL(triggered()), SLOT(onMemberRemove()));
            connect(actSetPriv, SIGNAL(triggered()), SLOT(onMemberSetPriv()));
            connect(actPrivChat, SIGNAL(triggered()), SLOT(onMemberPrivateChat()));
        }
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
    mWaitMsg.addMsg(tr("Removing user(s), please wait..."));
    auto waitMsgKeepalive = mWaitMsg;
    mainWindow.client().api->call(&mega::MegaApi::removeFromChat, mRoom.chatid(), handle)
    .fail([this, waitMsgKeepalive](const promise::Error& err)
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
    mega::marshallCall([this]()
    {
        QMenu menu(this);
        createMembersMenu(menu);
        menu.setLayoutDirection(Qt::RightToLeft);
        menu.adjustSize();
        menu.exec(ui.mMembersBtn->mapToGlobal(
            QPoint(-menu.width()+ui.mMembersBtn->width(), ui.mMembersBtn->height())));
    });
}
void ChatWindow::dropEvent(QDropEvent* event)
{
    const auto& data = event->mimeData()->data("application/mega-user-handle");
    if (data.size() != sizeof(uint64_t))
    {
        GUI_LOG_ERROR("ChatWindow: dropEvent() for userid: Data size is not 8 bytes");
        return;
    }
    mWaitMsg.addMsg(tr("Adding user(s), please wait..."));
    auto waitMsgKeepAlive = mWaitMsg;
    mRoom.parent.client.api->call(&::mega::MegaApi::inviteToChat, mRoom.chatid(), *(const uint64_t*)(data.data()), chatd::PRIV_FULL)
    .fail([waitMsgKeepAlive](const promise::Error& err)
    {
        QMessageBox::critical(nullptr, tr("Add user"), tr("Error adding user to group chat: ")+QString::fromStdString(err.msg()));
        return err;
    });
    event->acceptProposedAction();
}

void ChatWindow::onScroll(int value)
{
    if (isVisible())
        updateSeen();
}

void ChatWindow::updateSeen()
{
    auto& msglist = *ui.mMessageList;
    if (msglist.count() < 1)
        return;
    int i = msglist.indexAt(QPoint(4, 1)).row();
    if (i < 0) //message list is empty
    {
//      printf("no visible messages\n");
        return;
    }

    auto lastidx = mChat->lastSeenIdx();
    if (lastidx == CHATD_IDX_INVALID)
        lastidx = mChat->lownum();
    auto rect = msglist.rect();
    chatd::Idx idx = CHATD_IDX_INVALID;
    //find last visible message widget
    for(; i < mHistAddPos; i++)
    {
        auto item = msglist.item(i);
        if (msglist.visualItemRect(item).bottom() > rect.bottom())
            break;
        auto lastWidget = qobject_cast<MessageWidget*>(msglist.itemWidget(item));
        if (lastWidget->mIsMine)
            continue;
        idx = lastWidget->mIndex;
        assert(idx != CHATD_IDX_INVALID);
    }
    if (idx == CHATD_IDX_INVALID)
        return;

    if (idx > lastidx)
    {
        mChat->setMessageSeen(idx);
//        printf("set last seen: +%d\n", idx-lastidx);
    }
    else
    {
//        printf("NOT set last seen: %d\n", idx-lastidx);
    }
}
void ChatWindow::onMessageEdited(const chatd::Message& msg, chatd::Idx idx)
{
    auto widget = widgetFromMessage(msg);
    if (!widget)
    {
        CHAT_LOG_WARNING("onMessageEdited: No widget is associated with message with idx %d", idx);
        return;
    }
    if (msg.empty())
    {
        widget->msgDeleted();
        return;
    }
    widget->setText(msg);
    if (msg.userid == mChat->client().userId()) //edit of our own message
    {
        //edited state must have been set earlier, on posting the edit
        widget->updateStatus(chatd::Message::kServerReceived);
    }
    else
    {
        widget->setEdited();
    }
    widget->fadeIn(QColor(Qt::yellow));
}

void ChatWindow::onEditRejected(const chatd::Message& msg, uint8_t opcode)
{
    auto widget = widgetFromMessage(msg);
    if (!widget)
    {
        CHAT_LOG_ERROR("onEditRejected: No widget associated with message");
        return;
    }
//    widget->setText(*widget->mMessage); //restore original
    showCantEditNotice();
}

void ChatWindow::showCantEditNotice(const QString& action)
{
    WaitMessage tooltip(*this);
    tooltip.addMsg(tr("Can't %1 - message is too old").arg(action));
    mega::setTimeout([tooltip]()
    {}, 2000);
}

void ChatWindow::onUnsentEditLoaded(chatd::Message& editmsg, bool oriMsgIsSending)
{
    MessageWidget* widget;
    if (oriMsgIsSending)
    {
        for (int i=mHistAddPos; i<ui.mMessageList->count(); i++)
        {
            auto item = ui.mMessageList->item(i);
            widget = qobject_cast<MessageWidget*>(item->listWidget()->itemWidget(item));
            assert(widget);
            if (widget->mMessage->id() == editmsg.id())
                break;
        }
        if (!widget)
        {
            CHAT_LOG_WARNING("onUnsentEditLoaded: Could not find the oriignal message among the ones being in sending state");
            return;
        }
        assert(widget->mMessage->dataEquals(editmsg.buf(), editmsg.dataSize()));
    }
    else
    {
        auto idx = mChat->msgIndexFromId(editmsg.id());
        if (idx == CHATD_IDX_INVALID)
            return;

        auto& msg = mChat->at(idx);
        widget = widgetFromMessage(msg);
        if (!widget)
        {
            CHAT_LOG_WARNING("onUnsentEditLoaded: No widget associated with msgid %s", msg.id().toString().c_str());
            return;
        }
        widget->setText(msg);
    }
    widget->setBgColor(Qt::yellow);
    widget->setEdited();
}
void ChatWindow::onManualSendRequired(chatd::Message* msg, uint64_t id, int reason)
{
    if (!mManualSendList)
    {
        mManualSendList = new QListWidget(this);
        ui.mSplitter->insertWidget(1, mManualSendList);
        mManualSendList->show();
    }
    auto widget = new ManualSendMsgWidget(*this, msg, id, reason);
//    connect(widget->ui.mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));

    auto* item = new QListWidgetItem;
    msg->userp = item;
    item->setSizeHint(widget->size());
    mManualSendList->addItem(item);
    mManualSendList->setItemWidget(item, widget);
}

WaitMessage::WaitMessage(ChatWindow& chatWindow)
    :mChatWindow(chatWindow){}
void WaitMessage::addMsg(const QString &msg)
{
    if (!get())
        reset(new WaitMsgWidget(mChatWindow.ui.mMessageList, msg));
    else
        get()->addMsg(msg);
}
WaitMessage::~WaitMessage()
{
    if (use_count() == 2)
        mChatWindow.mWaitMsg.reset();
}

WaitMsgWidget::WaitMsgWidget(QWidget* parent, const QString& msg)
    :QLabel(parent)
{
    setStyleSheet(
        "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1,"
        "stop:0 rgba(100, 100, 100, 180), stop:1 rgba(120, 120, 120, 180));"
        "border-radius: 10px; font: 16px Arial;"
        "color: white; padding: 10px");
    addMsg(msg);
}

void WaitMsgWidget::addMsg(const QString& msg)
{
    if (!mMsgs.insert(msg).second)
        return;
    hide();
    updateGui();
    show();
}

void WaitMsgWidget::updateGui()
{
    QString text;
    for (auto& msg: mMsgs)
    {
        text.append(msg).append(QChar('\n'));
    }
    if (text.size() > 0)
        text.truncate(text.size()-1);
    setText(text);
    adjustSize();
}

void WaitMsgWidget::show()
{
    move((qobject_cast<QWidget*>(parent())->width()-width())/2, 10);
    QWidget::show();
}
MessageWidget& MessageWidget::setAuthor(karere::Id userid)
{
    if (mIsMine)
    {
        ui.mAuthorDisplay->setText(tr("me"));
        return *this;
    }
    auto email = mChatWindow.mainWindow.client().contactList->getUserEmail(userid);
    if (email)
        ui.mAuthorDisplay->setText(QString::fromStdString(*email));
    else
        ui.mAuthorDisplay->setText(tr("?"));

    mChatWindow.mainWindow.client().userAttrCache.getAttr(userid, mega::MegaApi::USER_ATTR_LASTNAME, this,
    [](Buffer* data, void* userp)
    {
        //buffer contains an unsigned char prefix that is the strlen() of the first name
        if (!data || data->dataSize() < 2)
            return;
        auto self = static_cast<MessageWidget*>(userp);
        self->ui.mAuthorDisplay->setText(QString::fromUtf8(data->buf()+1));
    });
    return *this;
}

void MessageWidget::msgDeleted()
{
    mMessage->userFlags |= kMsgfDeleted;
    assert(mMessage->userp);
    auto& list = *mChatWindow.ui.mMessageList;
    auto visualRect = list.visualItemRect(static_cast<QListWidgetItem*>(mMessage->userp));
    if (mChatWindow.mChat->isFetchingHistory() || !list.rect().contains(visualRect))
    {
        removeFromList();
        mChatWindow.mHistAddPos--;
        return;
    }
    auto a = new QPropertyAnimation(this, "msgColor");
// QT BUG: the animation does not always fire the finished signal, so we use our
// own timer with the same interval as the animation duration. Maybe it's an optimization
// to not animate invisible stuff
//      connect(a, SIGNAL(finished()), this, SLOT(onDeleteFadeFinished()));
    a->setStartValue(QColor(Qt::white));
    a->setEndValue(QColor(255,0,0,50));
    a->setDuration(300);
    a->setEasingCurve(QEasingCurve::Linear);
    a->start(QAbstractAnimation::DeleteWhenStopped);
    mega::setTimeout([this]() { removeFromList(); }, 300);
}

void MessageWidget::removeFromList()
{
    assert(mMessage->userp);
    auto item = static_cast<QListWidgetItem*>(mMessage->userp);
    assert(item);
    mMessage->userp = nullptr;
    auto& list = *mChatWindow.ui.mMessageList;
    delete list.takeItem(list.row(item));
    mChatWindow.mHistAddPos--;
    this->deleteLater();
}

ManualSendMsgWidget::ManualSendMsgWidget(ChatWindow& chatWin, chatd::Message* aMsg,
    uint64_t id, uint8_t reason)
: QWidget(&chatWin), mChatWindow(chatWin), mMessage(aMsg), mId(id), mReason(reason)
{
    ui.setupUi(this);
    if (reason == chatd::kManualSendTooOld)
    {
        ui.mReasonDisplay->setText(tr("Too old (%1)").arg(prettyInterval(time(NULL) - mMessage->ts)));
    }
    else if (reason == chatd::kManualSendUsersChanged)
    {
        ui.mReasonDisplay->setText(tr("Participants changed"));
    }
    else
    {
        CHAT_LOG_ERROR("Don't know how to handle manual send reason %d", reason);
    }
}
void ManualSendMsgWidget::onSendBtn()
{
    mChatWindow.chat().confirmManualSend(mId, mMessage.release());
    removeFromListAndDelete();
}

void ManualSendMsgWidget::onDiscardBtn()
{
    mChatWindow.chat().cancelManualSend(mId);
    removeFromListAndDelete();
}

void ManualSendMsgWidget::removeFromListAndDelete()
{
    assert(mChatWindow.mManualSendList);
    assert(mMessage->userp);
    auto item = static_cast<QListWidgetItem*>(mMessage->userp);
    assert(item);
    mMessage->userp = nullptr;
    auto list = mChatWindow.mManualSendList;
    delete list->takeItem(list->row(item));
    this->deleteLater();
    if (list->count() <= 0)
    {
        mChatWindow.mManualSendList = nullptr;
        list->deleteLater();
    }
}
