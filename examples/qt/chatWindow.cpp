#include "chatWindow.h"
#include "mainwindow.h"
using namespace karere;

ChatWindow::ChatWindow(QWidget* parent, karere::ChatRoom& room)
    : QDialog(parent), client(room.parent.client), mRoom(room), mWaitMsg(*this)
{
    userp = this;
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

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    QDialog::show();
}

ChatWindow::~ChatWindow()
{
    GUI_LOG_DEBUG("Destroying chat window for chat %s", Id(mRoom.chatid()).toString().c_str());
    if (mUpdateSeenTimer)
    {
        cancelTimeout(mUpdateSeenTimer);
        mUpdateSeenTimer = 0;
    }
    mRoom.removeAppChatHandler();
}

MessageWidget::MessageWidget(ChatWindow& parent, chatd::Message& msg,
    chatd::Message::Status status, chatd::Idx idx)
: QWidget(&parent), mChatWindow(parent), mMessage(&msg),
    mIsMine(msg.userid == parent.chat().client().userId()), mIndex(idx)
{
    printf("MessageWidget: msg.type = %d\n", msg.type);
    ui.setupUi(this);
    setAuthor(msg.userid);
    setTimestamp(msg.ts);
    setStatus(status);
    if (msg.updated)
        setEdited();

    if (!msg.isManagementMessage())
        setText(msg);
    else
        setText(msg.managementInfoToString());

    auto tooltip = QString("msgid: %1\ntype:%2\nkeyid: %3\nuserid: %4\nchatid: %5\nbackrefs: %6")
            .arg(QString::fromStdString(mMessage->id().toString()))
            .arg(mMessage->type)
            .arg(mMessage->keyid).arg(QString::fromStdString(mMessage->userid.toString()))
            .arg(QString::fromStdString(parent.chat().chatId().toString()))
            .arg(mMessage->backRefs.size());
    ui.mHeader->setToolTip(tooltip);
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
            auto menuSetPriv = entry->addMenu(tr("Set privilege"));

            auto actSetPrivFull = menuSetPriv->addAction(tr("Full access"));
            actSetPrivFull->setData(QVariant((qulonglong)item.first));
            auto actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only"));
            actSetPrivReadOnly->setData(QVariant((qulonglong)item.first));

            auto actPrivChat = entry->addAction(tr("Send private message"));
            actPrivChat->setData(QVariant((qulonglong)item.first));
            connect(actRemove, SIGNAL(triggered()), SLOT(onMemberRemove()));
            connect(actSetPrivFull, SIGNAL(triggered()), SLOT(onMemberSetPrivFull()));
            connect(actSetPrivReadOnly, SIGNAL(triggered()), SLOT(onMemberSetPrivReadOnly()));
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
    client.api.call(&mega::MegaApi::removeFromChat, mRoom.chatid(), handle)
    .fail([this, waitMsgKeepalive](const promise::Error& err)
    {
        QMessageBox::critical(nullptr, tr("Remove member from group chat"),
            tr("Error removing member from group chat: %1")
            .arg(QString::fromStdString(err.msg())));
        return err;
    });
}
void ChatWindow::onMemberSetPrivFull()
{
    static_cast<karere::GroupChatRoom&>(mRoom).setPrivilege(handleFromAction(QObject::sender()), chatd::PRIV_OPER);
}
void ChatWindow::onMemberSetPrivReadOnly()
{
    static_cast<karere::GroupChatRoom&>(mRoom).setPrivilege(handleFromAction(QObject::sender()), chatd::PRIV_RDONLY);
}

void ChatWindow::onMemberPrivateChat()
{
    auto& clist = *client.contactList;
    auto it = clist.find(handleFromAction(QObject::sender()));
    if (it == clist.end())
    {
        QMessageBox::critical(this, tr("Send private message"), tr("Person is not a contact of ours"));
        return;
    }
    static_cast<CListChatItem*>(it->second->appItem()->userp)->showChatWindow();
}

void ChatWindow::onMembersBtn(bool)
{
    marshallCall([this]()
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
    static_cast<GroupChatRoom&>(mRoom).invite(*(const uint64_t*)(data.data()), chatd::PRIV_FULL)
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
    if (msg.isManagementMessage())
    {
        widget->setText(msg.managementInfoToString());
    }
    else
    {
        if (msg.empty())
        {
            widget->msgDeleted();
            return;
        }
        assert(msg.ts);
        widget->setText(msg);
        widget->setEdited();
    }
    if (msg.userid == mChat->client().userId()) //edit of our own message
    {
        //edited state must have been set earlier, on posting the edit
        widget->updateStatus(chatd::Message::kServerReceived);
    }
    widget->fadeIn(QColor(Qt::yellow));
}

void ChatWindow::onEditRejected(const chatd::Message& msg, bool oriIsConfirmed)
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
    WaitMsg tooltip(*this);
    tooltip.addMsg(tr("Can't %1 - message is too old").arg(action));
    setTimeout([tooltip]()
    {}, 2000);
}

void ChatWindow::onUnsentEditLoaded(chatd::Message& editmsg, bool oriMsgIsSending)
{
    MessageWidget* widget=nullptr;
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
            CHAT_LOG_WARNING("onUnsentEditLoaded: Could not find the orignal message among the ones being in sending state");
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
void ChatWindow::onManualSendRequired(chatd::Message* msg, uint64_t id, chatd::ManualSendReason reason)
{
    if (!mManualSendList)
    {
        mManualSendList = new QListWidget(this);
        ui.mSplitter->insertWidget(1, mManualSendList);
        mManualSendList->show();
    }
    if (msg->userp)
    {
        auto item = static_cast<QListWidgetItem*>(msg->userp);
        MessageWidget* msgWidget = static_cast<MessageWidget*>(ui.mMessageList->itemWidget(item));
        msgWidget->removeFromList();
        msg->userp = nullptr;
    }

    auto widget = new ManualSendMsgWidget(*this, msg, id, reason);
    auto* item = new QListWidgetItem;
    msg->userp = item;
    item->setSizeHint(widget->size());
    mManualSendList->addItem(item);
    mManualSendList->setItemWidget(item, widget);
}
void ChatWindow::onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx)
{
    for (auto i=mChat->lownum(); i<idx; i++)
    {
        auto& msg = mChat->at(i);
        if (msg.userp)
        {
            auto item = static_cast<QListWidgetItem*>(msg.userp);
            assert(item);
            msg.userp = nullptr;
            auto& list = *ui.mMessageList;
            auto widget = static_cast<MessageWidget*>(list.itemWidget(item));
            delete list.takeItem(list.row(item));
            mHistAddPos--;
            widget->deleteLater();
        }
    }
}

WaitMsg::WaitMsg(ChatWindow& chatWindow)
    :mChatWindow(chatWindow){}
void WaitMsg::addMsg(const QString &msg)
{
    if (!get())
        reset(new WaitMsgWidget(mChatWindow.ui.mMessageList, msg));
    else
        get()->addMsg(msg);
}
WaitMsg::~WaitMsg()
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
    auto email = mChatWindow.client.contactList->getUserEmail(userid);
    if (email)
        ui.mAuthorDisplay->setText(QString::fromStdString(*email));
    else
        ui.mAuthorDisplay->setText(tr("?"));

    mChatWindow.client.userAttrCache().getAttr(userid, mega::MegaApi::USER_ATTR_LASTNAME, this,
    [](Buffer* data, void* userp)
    {
        //buffer contains an unsigned char prefix that is the strlen() of the first name
        if (!data || data->dataSize() < 2)
            return;
        auto self = static_cast<MessageWidget*>(userp);
        self->ui.mAuthorDisplay->setText(QString::fromUtf8(data->buf()+1, data->dataSize()-1));
    });
    return *this;
}

void MessageWidget::msgDeleted()
{
    mMessage->userFlags |= kMsgfDeleted;
    assert(mMessage->userp);
    auto& list = *mChatWindow.ui.mMessageList;
    auto visualRect = list.visualItemRect(static_cast<QListWidgetItem*>(mMessage->userp));
    if (mChatWindow.mChat->isFetchingFromServer() || !list.rect().contains(visualRect))
    {
        removeFromList();
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
    setTimeout([this]() { removeFromList(); }, 300);
}

void MessageWidget::removeFromList()
{
    assert(mMessage->userp);
    auto item = static_cast<QListWidgetItem*>(mMessage->userp);
    assert(item);
    mMessage->userp = nullptr;
    auto& list = *mChatWindow.ui.mMessageList;
    auto rowNo = list.row(item);
    delete list.takeItem(rowNo);
    if (rowNo < mChatWindow.mHistAddPos) //if it's a history message, move mHistAddPos one position up as we shorten the history
        mChatWindow.mHistAddPos--;
    this->deleteLater();
}

ManualSendMsgWidget::ManualSendMsgWidget(ChatWindow& chatWin, chatd::Message* aMsg,
    uint64_t id, uint8_t reason)
: QWidget(&chatWin), mChatWindow(chatWin), mMessage(aMsg), mId(id), mReason(reason)
{
    assert(mMessage);
    ui.setupUi(this);
    ui.mTextEdit->setText(QString::fromUtf8(mMessage->buf(), mMessage->dataSize()));
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
    connect(ui.mSendBtn, SIGNAL(clicked()), this, SLOT(onSendBtn()));
    connect(ui.mDiscardBtn, SIGNAL(clicked()), this, SLOT(onDiscardBtn()));
}
void ManualSendMsgWidget::onSendBtn()
{
    mChatWindow.chat().removeManualSend(mId);
    mId = 0;
    mChatWindow.postNewMessage(mMessage->buf(), mMessage->dataSize(), mMessage->type);
    removeFromListAndDelete();
}

void ManualSendMsgWidget::onDiscardBtn()
{
    mChatWindow.chat().removeManualSend(mId);
    mId = 0;
    removeFromListAndDelete();
}

void ManualSendMsgWidget::removeFromListAndDelete()
{
    assert(mMessage);
    assert(mMessage->userp);
    auto item = static_cast<QListWidgetItem*>(mMessage->userp);
    mMessage->userp = nullptr;

    assert(mChatWindow.mManualSendList);
    auto list = mChatWindow.mManualSendList;
    delete list->takeItem(list->row(item));
    this->deleteLater();
    if (list->count() <= 0)
    {
        mChatWindow.mManualSendList = nullptr;
        list->deleteLater();
    }
}
