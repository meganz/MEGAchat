#include "qmenu.h"
#include "chatMessage.h"
#include "ui_chatMessageWidget.h"
#include <QMessageBox>

const char* messageStatus[] =
{
  "Sending", "SendingManual", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

ChatMessage::ChatMessage(ChatWindow *parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg)
    : QWidget((QWidget *)parent),
      ui(new Ui::ChatMessageWidget)
{
    chatWin=parent;
    this->chatId=chatId;
    megaChatApi = mChatApi;
    ui->setupUi(this);
    message = msg;
    edited = false;
    setAuthor();
    setTimestamp(message->getTimestamp());

    if(this->megaChatApi->getChatRoom(chatId)->isGroup() && message->getStatus()== megachat::MegaChatMessage::STATUS_DELIVERED)
        setStatus(megachat::MegaChatMessage::STATUS_SERVER_RECEIVED);
    else
        setStatus(message->getStatus());

    if (message->isEdited())
        markAsEdited();

    if (!msg->isManagementMessage())
        setMessageContent(msg->getContent());
    else
        ui->mMsgDisplay->setText(managementInfoToString().c_str());

    connect(ui->mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
    updateToolTip();
    show();
}

ChatMessage::~ChatMessage()
{
    delete message;
    delete ui;
}

void ChatMessage::updateToolTip()
{
    megachat::MegaChatHandle msgId;
    megachat::MegaChatHandle chatId = chatWin->chatRoom->getChatId();
    if (message->getStatus() == megachat::MegaChatMessage::STATUS_SERVER_RECEIVED || message->getStatus() == megachat::MegaChatMessage::STATUS_DELIVERED)
        msgId = message->getMsgId();
    else
        msgId = message->getTempId();

    QString tooltip = NULL;
        tooltip.append(tr("msgid: "))
        .append(QString::fromStdString(std::to_string(msgId)))
        .append(tr("\ntype: "))
        .append(QString::fromStdString(std::to_string(message->getType())))
        .append(tr("\nuserid: "))
        .append(QString::fromStdString(std::to_string(message->getUserHandle())))
        .append(tr("\nchatid: "))
        .append(QString::fromStdString(std::to_string(chatId)));
    ui->mHeader->setToolTip(tooltip);
}

QListWidgetItem *ChatMessage::getWidgetItem() const
{
    return widgetItem;
}

void ChatMessage::setWidgetItem(QListWidgetItem *item)
{
    widgetItem = item;
}

megachat::MegaChatMessage *ChatMessage::getMessage() const
{
    return message;
}

void ChatMessage::setMessage(megachat::MegaChatMessage *message)
{
    this->message = message;
}

void ChatMessage::setMessageContent(const char * content)
{
    ui->mMsgDisplay->setText(content);
}


std::string ChatMessage::managementInfoToString() const
{
    std::string ret;
    ret.reserve(128);

    switch (message->getType())
    {
    case megachat::MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
    {
        ret.append("User ").append(std::to_string(message->getUserHandle()))
           .append((message->getPrivilege() == megachat::MegaChatRoom::PRIV_RM) ? " removed" : " added")
           .append(" user ").append(std::to_string(message->getHandleOfAction()));
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_TRUNCATE:
    {
        ret.append("Chat history was truncated by user ").append(std::to_string(message->getUserHandle()));
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
    {
        ret.append("User ").append(std::to_string(message->getUserHandle()))
           .append(" set privilege of user ").append(std::to_string(message->getHandleOfAction()))
           .append(" to ").append(std::to_string(message->getPrivilege()));
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
    {
        ret.append("User ").append(std::to_string(message->getUserHandle()))
           .append(" set chat title to '")
           .append(this->megaChatApi->getChatRoom(chatId)->getTitle())+='\'';
        return ret;
    }
    default:
        throw std::runtime_error("Message with type "+std::to_string(message->getType())+" is not a management message");
    }
}

void ChatMessage::setTimestamp(int64_t ts)
{
    QDateTime t;
    t.setTime_t(ts);
    ui->mTimestampDisplay->setText(t.toString("hh:mm:ss"));
}

void ChatMessage::setStatus(int status)
{
    if (status==megachat::MegaChatMessage::STATUS_UNKNOWN)
        ui->mStatusDisplay->setText("Invalid");
    else
    {
        ui->mStatusDisplay->setText(messageStatus[status]);
    }
}

void ChatMessage::setAuthor()
{
    megachat::MegaChatHandle userHandle = message->getUserHandle();
    const char * chatTitle = megaChatApi->getChatRoom(chatId)->getPeerFullnameByHandle(userHandle);
    if(userHandle==this->megaChatApi->getMyUserHandle())
       {ui->mAuthorDisplay->setText(tr("me"));}
    else
       {ui->mAuthorDisplay->setText(tr(chatTitle));}
}

bool ChatMessage::isMine() const
{
    return this->message->getUserHandle() == this->megaChatApi->getMyUserHandle();
}

void ChatMessage::markAsEdited()
{
    setStatus(message->getStatus());
    ui->mStatusDisplay->setText(ui->mStatusDisplay->text()+" (Edited)");
}


void ChatMessage::onMessageCtxMenu(const QPoint& point)
{
   if (isMine() && !message->isManagementMessage())
   {
        QMenu * menu = ui->mMsgDisplay->createStandardContextMenu(point);
        auto action = menu->addAction(tr("&Edit message"));
        action->setData(QVariant::fromValue(this));
        connect(action, SIGNAL(triggered()), this, SLOT(onMessageEditAction()));
        auto delAction = menu->addAction(tr("Delete message"));
        delAction->setData(QVariant::fromValue(this));
        connect(delAction, SIGNAL(triggered()), this, SLOT(onMessageDelAction()));
        menu->popup(this->mapToGlobal(point));
   }
}


void ChatMessage::onMessageDelAction()
{
    chatWin->deleteChatMessage(this->message);
}


void ChatMessage::onMessageEditAction()
{
        auto action = qobject_cast<QAction*>(QObject::sender());
        startEditingMsgWidget();
}


void ChatMessage::cancelMsgEdit(bool clicked)
{
    clearEdit();
    chatWin->ui->mMessageEdit->setText(QString());
}


void ChatMessage::saveMsgEdit(bool clicked)
{
    const char * editedMsg =chatWin->ui->mMessageEdit->toPlainText().toStdString().c_str();
    if(this->message->getContent() != editedMsg)
                this->megaChatApi->editMessage(chatId,message->getMsgId() ,editedMsg);
    clearEdit();
}

void ChatMessage::startEditingMsgWidget()
{
    edited = true;
    chatWin->ui->mMsgSendBtn->setEnabled(false);
    chatWin->ui->mMessageEdit->blockSignals(true);
    ui->mEditDisplay->hide();
    ui->mStatusDisplay->hide();

    QPushButton * cancelBtn = new QPushButton(this);
    connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(cancelMsgEdit(bool)));
    cancelBtn->setText("Cancel edit");
    auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
    layout->insertWidget(2, cancelBtn);

    QPushButton * saveBtn = new QPushButton(this);
    connect(saveBtn, SIGNAL(clicked(bool)), this, SLOT(saveMsgEdit(bool)));
    saveBtn->setText("Save");
    layout->insertWidget(3, saveBtn);

    setLayout(layout);
    chatWin->ui->mMessageEdit->setText(ui->mMsgDisplay->toPlainText());
    chatWin->ui->mMessageEdit->moveCursor(QTextCursor::End);
}



ChatMessage* ChatMessage::clearEdit()
{
    edited = false;
    chatWin->ui->mMessageEdit->setText("");
    chatWin->ui->mMessageEdit->moveCursor(QTextCursor::Start);
    auto header = ui->mHeader->layout();
    auto cancelBtn = header->itemAt(2)->widget();
    auto saveBtn = header->itemAt(3)->widget();
    header->removeWidget(cancelBtn);
    header->removeWidget(saveBtn);
    ui->mEditDisplay->show();
    ui->mStatusDisplay->show();
    delete cancelBtn;
    delete saveBtn;
    chatWin->ui->mMsgSendBtn->setEnabled(true);
    chatWin->ui->mMessageEdit->blockSignals(false);
    return this;
}

void ChatMessage::setManualMode(bool manualMode)
{
    if(manualMode)
    {
        ui->mEditDisplay->hide();
        ui->mStatusDisplay->hide();
        QPushButton * manualSendBtn = new QPushButton(this);
        connect(manualSendBtn, SIGNAL(clicked(bool)), this, SLOT(onManualSending()));
        manualSendBtn->setText("Send (Manual mode)");
        auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
        layout->insertWidget(2, manualSendBtn);


        QPushButton * discardBtn = new QPushButton(this);
        connect(discardBtn, SIGNAL(clicked(bool)), this, SLOT(onDiscardManualSending()));
        discardBtn->setText("Discard");
        layout->insertWidget(3, discardBtn);
        setLayout(layout);
    }
    else
    {
        ui->mEditDisplay->show();
        ui->mStatusDisplay->show();
        auto header = ui->mHeader->layout();
        auto manualSending = header->itemAt(2)->widget();
        auto discardManualSending = header->itemAt(3)->widget();
        delete manualSending;
        delete discardManualSending;
    }
}


void ChatMessage::onManualSending()
{
   if(chatWin->chatRoom->getOwnPrivilege() == megachat::MegaChatPeerList::PRIV_MODERATOR)
   {
       this->megaChatApi->removeUnsentMessage(chatWin->chatRoom->getChatId(), message->getRowId());
       megachat::MegaChatMessage * tempMessage = this->megaChatApi->sendMessage(chatWin->chatRoom->getChatId(), message->getContent());
       setManualMode(false);
       chatWin->eraseChatMessage(message, true);
       chatWin->moveManualSendingToSending(tempMessage);
   }
   else
   {QMessageBox::critical(nullptr, tr("Manual sending"), tr("You don't have permissions to send this message"));}
}

void ChatMessage::onDiscardManualSending()
{
   this->megaChatApi->removeUnsentMessage(chatWin->chatRoom->getChatId(), message->getRowId());
   chatWin->eraseChatMessage(message, true);
}



