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
    mChatWindow=parent;
    this->mChatId=chatId;
    megaChatApi = mChatApi;
    ui->setupUi(this);
    mMessage = msg;
    setAuthor();
    setTimestamp(mMessage->getTimestamp());

    if(this->megaChatApi->getChatRoom(chatId)->isGroup() && mMessage->getStatus()== megachat::MegaChatMessage::STATUS_DELIVERED)
        setStatus(megachat::MegaChatMessage::STATUS_SERVER_RECEIVED);
    else
        setStatus(mMessage->getStatus());

    if (mMessage->isEdited())
        markAsEdited();

    if (!msg->isManagementMessage())
    {
        switch (msg->getType())
        {
            case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
            {
                QString text;
                text.append(tr("[Nodes attachment msg]"));
                mega::MegaNodeList *nodeList=mMessage->getMegaNodeList();
                for(int i = 0; i < nodeList->size(); i++)
                {
                    const char *auxNodeHandle_64 =this->mChatWindow->mMegaApi->handleToBase64(nodeList->get(i)->getHandle());
                    text.append(tr("\n[Node]"))
                    .append("\nHandle: ")
                    .append(QString::fromStdString(auxNodeHandle_64))
                    .append("\nName: ")
                    .append(nodeList->get(i)->getName())
                    .append("\nSize: ")
                    .append(QString::fromStdString(std::to_string(nodeList->get(i)->getSize())))
                    .append(" bytes");
                    delete auxNodeHandle_64;
                }
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(198,251,187,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                text.clear();
                break;
            }
            case megachat::MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
            {
                QString text;
                text.append(tr("[Contacts attachment msg]"));
                for(unsigned int i = 0; i < mMessage->getUsersCount(); i++)
                {
                  const char *auxUserHandle_64 =this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle(i));
                  text.append(tr("\n[User]"))
                  .append("\nHandle: ")
                  .append(auxUserHandle_64)
                  .append("\nName: ")
                  .append(mMessage->getUserName(i))
                  .append("\nEmail: ")
                  .append(mMessage->getUserEmail(i));
                  delete auxUserHandle_64;
                }
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(205,254,251,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                text.clear();
                break;
            }
            case megachat::MegaChatMessage::TYPE_NORMAL:
            {
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                setMessageContent(msg->getContent());
                break;
            }
            case megachat::MegaChatMessage::TYPE_CONTAINS_META:
            {
                QString text = tr("[Contains-metadata msg]");
                if (mMessage->containsMetaType() == megachat::MegaChatMessage::CONTAINS_META_RICH_PREVIEW)
                {
                    text.append(tr("\nSubtype: rich-link"))
                        .append(tr("\nOriginal content: "))
                        .append(mMessage->getRichPreviewText())
                        .append(tr("\nURL: "))
                        .append(mMessage->getRichPreviewUrl())
                        .append(tr("\nTitle: "))
                        .append(mMessage->getRichPreviewTitle())
                        .append(tr("\nDescription: "))
                        .append(mMessage->getRichPreviewDescription())
                        .append(tr("\nHas icon: "))
                        .append(mMessage->getRichPreviewIcon() ? "yes" : "no")
                        .append(tr("\nHas image: "))
                        .append(mMessage->getRichPreviewImage() ? "yes" : "no");
                }
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(213,245,160,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                break;
            }
        }
    }
    else
    {
        ui->mMsgDisplay->setText(managementInfoToString().c_str());
        ui->mHeader->setStyleSheet("background-color: rgba(192,123,11,128)\n");
        ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
        ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    }

    connect(ui->mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
    updateToolTip();
    show();
}

ChatMessage::~ChatMessage()
{
    delete mMessage;
    delete ui;
}

void ChatMessage::updateToolTip()
{
    QString tooltip;

    megachat::MegaChatHandle msgId;
    int status = mMessage->getStatus();
    switch (status)
    {
    case megachat::MegaChatMessage::STATUS_SENDING:
        tooltip.append(tr("tempId: "));
        msgId = mMessage->getTempId();
        break;
    case megachat::MegaChatMessage::STATUS_SENDING_MANUAL:
        tooltip.append(tr("rowId: "));
        msgId = mMessage->getRowId();
        break;
    default:
        tooltip.append(tr("msgId: "));
        msgId = mMessage->getMsgId();
        break;
    }

    const char *auxMsgId_64 = mChatWindow->mMegaApi->userHandleToBase64(msgId);
    const char *auxUserId_64 = mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle());
    tooltip.append(QString::fromStdString(auxMsgId_64))
            .append(tr("\ntype: "))
            .append(QString::fromStdString(std::to_string(mMessage->getType())))
            .append(tr("\nuserid: "))
            .append(QString::fromStdString(auxUserId_64));
    ui->mHeader->setToolTip(tooltip);
    delete auxMsgId_64;
    delete auxUserId_64;
}

QListWidgetItem *ChatMessage::getWidgetItem() const
{
    return mListWidgetItem;
}

void ChatMessage::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
}

megachat::MegaChatMessage *ChatMessage::getMessage() const
{
    return mMessage;
}

void ChatMessage::setMessage(megachat::MegaChatMessage *message)
{
    this->mMessage = message;
}

void ChatMessage::setMessageContent(const char * content)
{
    ui->mMsgDisplay->setText(content);
}

std::string ChatMessage::managementInfoToString() const
{
    std::string ret;
    ret.reserve(128);
    const char *userHandle_64 = this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle());
    const char *actionHandle_64 = this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getHandleOfAction());

    switch (mMessage->getType())
    {
    case megachat::MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
    {
        ret.append("User ").append(userHandle_64)
           .append((mMessage->getPrivilege() == megachat::MegaChatRoom::PRIV_RM) ? " removed" : " added")
           .append(" user ").append(actionHandle_64);
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_TRUNCATE:
    {
        ret.append("Chat history was truncated by user ").append(userHandle_64);
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
    {
        ret.append("User ").append(userHandle_64)
           .append(" set privilege of user ").append(actionHandle_64)
           .append(" to ").append(std::to_string(mMessage->getPrivilege()));
        return ret;
    }
    case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
    {
        ret.append("User ").append(userHandle_64)
           .append(" set chat title to '")
           .append(this->megaChatApi->getChatRoom(mChatId)->getTitle())+='\'';
        return ret;
    }
    default:
        ret.append("Management message with unknown type: ")
           .append(std::to_string(mMessage->getType()));
        return ret;
    }
    delete userHandle_64;
    delete actionHandle_64;
}

void ChatMessage::setTimestamp(int64_t ts)
{
    QDateTime t;
    t.setTime_t(ts);
    ui->mTimestampDisplay->setText(t.toString("hh:mm:ss - dd.MM.yy"));
}

void ChatMessage::setStatus(int status)
{
    if (status == megachat::MegaChatMessage::STATUS_UNKNOWN)
        ui->mStatusDisplay->setText("Invalid");
    else
    {
        ui->mStatusDisplay->setText(messageStatus[status]);
    }
}

void ChatMessage::setAuthor()
{
    if (isMine())
    {
        ui->mAuthorDisplay->setText(tr("me"));
    }
    else
    {
        const char *chatTitle = megaChatApi->getChatRoom(mChatId)->getPeerFullnameByHandle(mMessage->getUserHandle());
        ui->mAuthorDisplay->setText(tr(chatTitle));
        delete chatTitle;
    }
}

bool ChatMessage::isMine() const
{
    return (mMessage->getUserHandle() == megaChatApi->getMyUserHandle());
}

void ChatMessage::markAsEdited()
{
    setStatus(mMessage->getStatus());
    ui->mStatusDisplay->setText(ui->mStatusDisplay->text()+" (Edited)");
}

void ChatMessage::onMessageCtxMenu(const QPoint& point)
{
   if (isMine() && !mMessage->isManagementMessage())
   {
        QMenu *menu = ui->mMsgDisplay->createStandardContextMenu(point);
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
    mChatWindow->deleteChatMessage(this->mMessage);
}

void ChatMessage::onMessageEditAction()
{
    startEditingMsgWidget();
}

void ChatMessage::cancelMsgEdit(bool clicked)
{
    clearEdit();
    mChatWindow->ui->mMessageEdit->setText(QString());
}

void ChatMessage::saveMsgEdit(bool clicked)
{
    std::string editedMsg = mChatWindow->ui->mMessageEdit->toPlainText().toStdString();
    if(mMessage->getContent() != editedMsg)
    {
        megaChatApi->editMessage(mChatId,mMessage->getMsgId(), editedMsg.c_str());
    }
    clearEdit();
}

void ChatMessage::startEditingMsgWidget()
{
    mChatWindow->ui->mMsgSendBtn->setEnabled(false);
    mChatWindow->ui->mMessageEdit->blockSignals(true);
    ui->mEditDisplay->hide();
    ui->mStatusDisplay->hide();

    QPushButton *cancelBtn = new QPushButton(this);
    connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(cancelMsgEdit(bool)));
    cancelBtn->setText("Cancel edit");
    auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
    layout->insertWidget(2, cancelBtn);

    QPushButton * saveBtn = new QPushButton(this);
    connect(saveBtn, SIGNAL(clicked(bool)), this, SLOT(saveMsgEdit(bool)));
    saveBtn->setText("Save");
    layout->insertWidget(3, saveBtn);

    setLayout(layout);
    mChatWindow->ui->mMessageEdit->setText(ui->mMsgDisplay->toPlainText());
    mChatWindow->ui->mMessageEdit->moveCursor(QTextCursor::End);
}

void ChatMessage::clearEdit()
{
    mChatWindow->ui->mMessageEdit->setText("");
    mChatWindow->ui->mMessageEdit->moveCursor(QTextCursor::Start);
    auto header = ui->mHeader->layout();
    auto cancelBtn = header->itemAt(2)->widget();
    auto saveBtn = header->itemAt(3)->widget();
    header->removeWidget(cancelBtn);
    header->removeWidget(saveBtn);
    ui->mEditDisplay->show();
    ui->mStatusDisplay->show();
    delete cancelBtn;
    delete saveBtn;
    mChatWindow->ui->mMsgSendBtn->setEnabled(true);
    mChatWindow->ui->mMessageEdit->blockSignals(false);
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
   if(mChatWindow->mChatRoom->getOwnPrivilege() == megachat::MegaChatPeerList::PRIV_RO)
   {
       QMessageBox::critical(nullptr, tr("Manual sending"), tr("You don't have permissions to send this message"));
   }
   else
   {
       megaChatApi->removeUnsentMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getRowId());
       megachat::MegaChatMessage *tempMessage = megaChatApi->sendMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getContent());
       setManualMode(false);
       mChatWindow->eraseChatMessage(mMessage, true);
       mChatWindow->moveManualSendingToSending(tempMessage);
   }
}

void ChatMessage::onDiscardManualSending()
{
   megaChatApi->removeUnsentMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getRowId());
   mChatWindow->eraseChatMessage(mMessage, true);
}



