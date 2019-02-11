#include "qmenu.h"
#include "chatMessage.h"
#include "ui_chatMessageWidget.h"
#include <QMessageBox>
#include <QClipboard>

const char *messageStatus[] =
{
  "Sending", "SendingManual", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

ChatMessage::ChatMessage(ChatWindow *parent, megachat::MegaChatApi *mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg)
    : QWidget((QWidget *)parent),
      ui(new Ui::ChatMessageWidget)
{
    mChatWindow=parent;
    this->mChatId=chatId;
    megaChatApi = mChatApi;
    ui->setupUi(this);
    mMessage = msg;
    setAuthor(NULL);
    setTimestamp(mMessage->getTimestamp());
    megachat::MegaChatRoom *chatRoom = megaChatApi->getChatRoom(chatId);

    assert (chatRoom);
    if (chatRoom->isGroup() && mMessage->getStatus() == megachat::MegaChatMessage::STATUS_DELIVERED)
    {
        setStatus(megachat::MegaChatMessage::STATUS_SERVER_RECEIVED);
    }
    else
    {
        setStatus(mMessage->getStatus());
    }

    delete chatRoom;
    updateContent();

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
    delete [] auxMsgId_64;
    delete [] auxUserId_64;
}

void ChatMessage::showContainsMetaData()
{
    const MegaChatContainsMeta *containsMeta = mMessage->getContainsMeta();
    assert(containsMeta);
    QString text = tr("[Contains-metadata msg]");
    if (containsMeta)
    {
        switch (containsMeta->getType()) {
            case  megachat::MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW:
            {
                const MegaChatRichPreview *richPreview = containsMeta->getRichPreview();
                text.append(tr("\nSubtype: rich-link"))
                    .append(tr("\nOriginal content: "))
                    .append(richPreview->getText())
                    .append(tr("\nURL: "))
                    .append(richPreview->getUrl())
                    .append(tr("\nDomain Name: "))
                    .append(richPreview->getDomainName())
                    .append(tr("\nTitle: "))
                    .append(richPreview->getTitle())
                    .append(tr("\nDescription: "))
                    .append(richPreview->getDescription())
                    .append(tr("\nHas icon: "))
                    .append(richPreview->getIcon() ? "yes" : "no")
                    .append(tr("\nHas image: "))
                    .append(richPreview->getImage() ? "yes" : "no");
                break;
            }
            case  megachat::MegaChatContainsMeta::CONTAINS_META_GEOLOCATION:
            {
                const MegaChatGeolocation *geolocation = containsMeta->getGeolocation();
                text.append(tr("\nSubtype: Geolocation"))
                    .append(tr("\nText content: "))
                    .append(containsMeta->getTextMessage())
                    .append(tr("\nLongitude: "))
                    .append(QString::number(geolocation->getLongitude()))
                    .append(tr("\nLatitude: "))
                    .append(QString::number(geolocation->getLatitude()))
                    .append(tr("\nHas image: "))
                    .append(geolocation->getImage() ? "yes" : "no");
                break;
            }
            default:
            {
                text.append(tr("\nSubtype: unkown"))
                    .append(tr("\nText content: "))
                    .append(containsMeta->getTextMessage());
                break;
            }
        }
    }

    ui->mMsgDisplay->setText(text);
    ui->mMsgDisplay->setStyleSheet("background-color: rgba(213,245,160,128)\n");
    ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
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
    if (mMessage)
    {
        delete mMessage;
    }

    this->mMessage = message;
}

void ChatMessage::setMessageContent(const char *content)
{
    ui->mMsgDisplay->setText(content);
}

QString ChatMessage::nodelistText()
{
    QString text;
    ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
    for(int i = 0; i < nodeList->size(); i++)
    {
        const char *auxNodeHandle_64 = mChatWindow->mMegaApi->handleToBase64(nodeList->get(i)->getHandle());
        text.append("\n[Node").append(std::to_string(i+1).c_str()).append("]")
        .append("\nHandle: ")
        .append(QString::fromStdString(auxNodeHandle_64))
        .append("\nName: ")
        .append(nodeList->get(i)->getName())
        .append("\nSize: ")
        .append(QString::fromStdString(std::to_string(nodeList->get(i)->getSize())))
        .append(" bytes");
        delete [] auxNodeHandle_64;
    }
    return text;
}

void ChatMessage::updateContent()
{
    if (mMessage->isEdited())
        markAsEdited();

    if (!mMessage->isManagementMessage())
    {
        switch (mMessage->getType())
        {
            case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
            {
                QString text;
                text.append(tr("[Nodes attachment msg]"));
                text.append(nodelistText());
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(198,251,187,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->bSettings->show();
                break;
            }
            case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
            {
                QString text;
                text.append(tr("[Voice clip msg]"));
                text.append(nodelistText());
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(229,66,244,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->bSettings->show();
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
                  delete [] auxUserHandle_64;
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
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(255,255,255,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                setMessageContent(mMessage->getContent());
                break;
            }
            case megachat::MegaChatMessage::TYPE_CONTAINS_META:
            {
                showContainsMetaData();
                break;
            }
            case megachat::MegaChatMessage::TYPE_INVALID:
            {
                int errorCode = mMessage->getCode();
                std::string content = "Invalid message [warn]: - (";
                if (errorCode == MegaChatMessage::INVALID_SIGNATURE)
                    content.append("invalid signature");
                else if (errorCode == MegaChatMessage::INVALID_FORMAT)
                    content.append("malformed");
                else
                    content.append(std::to_string(errorCode));
                content.append(")\nContent: ");
                if (mMessage->getContent())
                    content.append(mMessage->getContent());
                setMessageContent(content.c_str());
                break;
            }
            case megachat::MegaChatMessage::TYPE_UNKNOWN:
            {
                int errorCode = mMessage->getCode();
                std::string content = "Unknown type [hide]: - (";
                if (errorCode == MegaChatMessage::INVALID_KEY)
                    content.append("invalid key");
                else if (errorCode == MegaChatMessage::DECRYPTING)
                    content.append("decrypting");
                else if (errorCode == MegaChatMessage::INVALID_TYPE)
                    content.append("invalid type");
                else
                    content.append(std::to_string(errorCode));
                content.append(")\nContent: ");
                if (mMessage->getContent())
                    content.append(mMessage->getContent());
                setMessageContent(content.c_str());
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
            break;
        }
        case megachat::MegaChatMessage::TYPE_TRUNCATE:
        {
            ChatListItemController *itemController = mChatWindow->mMainWin->getChatControllerById(mChatId);
            if(itemController)
            {
               ChatItemWidget *widget = itemController->getWidget();
               if (widget)
               {
                  widget->updateToolTip(itemController->getItem(), NULL);
                  ret.append("Chat history was truncated by user ").append(userHandle_64);
               }
            }
            break;
        }
        case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
        {
            ret.append("User ").append(userHandle_64)
               .append(" set privilege of user ").append(actionHandle_64)
               .append(" to ").append(std::to_string(mMessage->getPrivilege()));
            break;
        }
        case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
        {
            megachat::MegaChatRoom *chatRoom = megaChatApi->getChatRoom(mChatId);
            ret.append("User ").append(userHandle_64)
               .append(" set chat title to '")
               .append(chatRoom->getTitle())+='\'';
            delete chatRoom;
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_ENDED:
        {
            ret.append("User ").append(userHandle_64)
               .append(" start a call with: ");

            ::mega::MegaHandleList *handleList = mMessage->getMegaHandleList();
            for (unsigned int i = 0; i < handleList->size(); i++)
            {
                char *participant_64 = this->mChatWindow->mMegaApi->userHandleToBase64(handleList->get(i));
                ret.append(participant_64).append(" ");
                delete [] participant_64;
            }

            ret.append("\nDuration: ")
               .append(std::to_string(mMessage->getDuration()))
               .append("secs TermCode: ")
               .append(std::to_string(mMessage->getTermCode()));
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_STARTED:
        {
            ret.append("User ").append(userHandle_64)
               .append(" has started a call");
            break;
        }
        default:
            ret.append("Management message with unknown type: ")
               .append(std::to_string(mMessage->getType()));
            break;
    }
    delete [] userHandle_64;
    delete [] actionHandle_64;
    return ret;
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

void ChatMessage::setAuthor(const char *author)
{
    if (author)
    {
        ui->mAuthorDisplay->setText(tr(author));
        return;
    }

    MegaChatHandle uh = mMessage->getUserHandle();
    if (isMine())
    {
        ui->mAuthorDisplay->setText(tr("me"));
    }
    else
    {
        megachat::MegaChatRoom *chatRoom = megaChatApi->getChatRoom(mChatId);
        const char *msgAuthor = chatRoom->getPeerFirstnameByHandle(mMessage->getUserHandle());
        if (msgAuthor)
        {
            ui->mAuthorDisplay->setText(tr(msgAuthor));
        }
        else if ((msgAuthor = mChatWindow->mMainWin->mApp->getFirstname(uh)))
        {
            ui->mAuthorDisplay->setText(tr(msgAuthor));
            delete [] msgAuthor;
        }
        else
        {
            ui->mAuthorDisplay->setText(tr("Loading firstname..."));
        }
        delete chatRoom;
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
       if (mMessage->isEditable())
       {
           auto action = menu->addAction(tr("&Edit message"));
           action->setData(QVariant::fromValue(this));
           connect(action, SIGNAL(triggered()), this, SLOT(onMessageEditAction()));
       }

       if (mMessage->isDeletable())
       {
           auto delAction = menu->addAction(tr("Delete message"));
           delAction->setData(QVariant::fromValue(this));
           connect(delAction, SIGNAL(triggered()), this, SLOT(onMessageDelAction()));
       }

       if (mMessage->getType() == MegaChatMessage::TYPE_CONTAINS_META
               && mMessage->getContainsMeta()
               && mMessage->getContainsMeta()->getType() == MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW)
       {
           auto richAction = menu->addAction(tr("Remove rich link"));
           richAction->setData(QVariant::fromValue(this));
           connect(richAction, SIGNAL(triggered()), this, SLOT(onMessageRemoveLinkAction()));
       }
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

void ChatMessage::onMessageRemoveLinkAction()
{
    megaChatApi->removeRichLink(mChatId, mMessage->getMsgId());
}

void ChatMessage::cancelMsgEdit(bool /*clicked*/)
{
    clearEdit();
    mChatWindow->ui->mMessageEdit->setText(QString());
}

void ChatMessage::saveMsgEdit(bool /*clicked*/)
{
    std::string editedMsg = mChatWindow->ui->mMessageEdit->toPlainText().toStdString();
    std::string previousContent = mMessage->getContent();
    if(editedMsg != previousContent)
    {
        megachat::MegaChatHandle messageId;
        if (mMessage->getStatus() == megachat::MegaChatMessage::STATUS_SENDING)
        {
            messageId = mMessage->getTempId();
        }
        else
        {
            messageId = mMessage->getMsgId();
        }

        megachat::MegaChatMessage *message = megaChatApi->editMessage(mChatId, messageId, editedMsg.c_str());
        if (message)
        {
            setMessage(message);
            setMessageContent(message->getContent());
        }
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

    QPushButton *saveBtn = new QPushButton(this);
    connect(saveBtn, SIGNAL(clicked(bool)), this, SLOT(saveMsgEdit(bool)));
    saveBtn->setText("Save");
    layout->insertWidget(3, saveBtn);

    setLayout(layout);

    std::string content = mMessage->getContent();

    mChatWindow->ui->mMessageEdit->setText(content.c_str());
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
        QPushButton *manualSendBtn = new QPushButton(this);
        connect(manualSendBtn, SIGNAL(clicked(bool)), this, SLOT(onManualSending()));
        manualSendBtn->setText("Send (Manual mode)");
        auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
        layout->insertWidget(2, manualSendBtn);

        QPushButton *discardBtn = new QPushButton(this);
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

void ChatMessage::on_bSettings_clicked()
{
    if (mMessage->getType() != megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT
       && mMessage->getType() != megachat::MegaChatMessage::TYPE_VOICE_CLIP)
    {
        return;
    }

    QMenu menu(this);
    menu.setAttribute(Qt::WA_DeleteOnClose);
    switch (mMessage->getType())
    {
        case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
            ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
            for (int i = 0; i < nodeList->size(); i++)
            {
                ::mega::MegaNode *node = nodeList->get(i);
                QString text("Download \"");
                text.append(node->getName()).append("\"");
                auto actDownload = menu.addAction(tr(text.toStdString().c_str()));
                connect(actDownload,  &QAction::triggered, this, [this, node]{ onNodeDownload(node); });

                text = "Stream \"";
                text.append(node->getName()).append("\"");
                auto actStream = menu.addAction(tr(text.toStdString().c_str()));
                connect(actStream,  &QAction::triggered, this, [this, node]{ onNodePlay(node); });
            }
            break;
        }

        case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
        {
            ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
            for (int i = 0; i < nodeList->size(); i++)
            {
                ::mega::MegaNode *node = nodeList->get(i);
                QString text("Play \"");
                text.append(node->getName()).append("\"");
                auto actStream = menu.addAction(tr(text.toStdString().c_str()));
                connect(actStream,  &QAction::triggered, this, [this, node]{ onNodePlay(node); });
            }
            break;
        }

        default:
            break;
    }
    QPoint pos = ui->bSettings->pos();
    pos.setX(pos.x() + ui->bSettings->width());
    pos.setY(pos.y() + ui->bSettings->height());
    menu.exec(mapToGlobal(pos));
}

void ChatMessage::onNodeDownload(::mega::MegaNode *node)
{
    std::string target(mChatWindow->mMegaChatApi->getAppDir());
    target.append("/").append(node->getName());

    QMessageBox msgBoxAns;
    std::string message("Node will be saved in "+target+".\nDo you want to continue?");
    msgBoxAns.setText(message.c_str());
    msgBoxAns.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    if (msgBoxAns.exec() == QMessageBox::Ok)
    {
        mChatWindow->mMegaApi->startDownload(node, target.c_str());
    }
}

void ChatMessage::onNodePlay(mega::MegaNode *node)
{
    if (mChatWindow->mMegaApi->httpServerIsRunning() == 0)
    {
        mChatWindow->mMegaApi->httpServerStart();
    }
    const char *localUrl = mChatWindow->mMegaApi->httpServerGetLocalLink(node);

    if (localUrl)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QString clipUrl(localUrl);
        QMessageBox msg;
        msg.setText("URL for streaming the file: \""+QString(node->getName())+"\"");
        msg.setIcon(QMessageBox::Information);
        msg.setDetailedText(clipUrl);
        msg.addButton(tr("Ok"), QMessageBox::ActionRole);

        QAbstractButton *copyButton = msg.addButton(tr("Copy to clipboard"), QMessageBox::ActionRole);
        copyButton->disconnect();
        connect(copyButton, &QAbstractButton::clicked, this, [=](){clipboard->setText(clipUrl);});

        foreach (QAbstractButton *button, msg.buttons())
        {
            if (msg.buttonRole(button) == QMessageBox::ActionRole)
            {
                button->click();
                break;
            }
        }
        msg.setStyleSheet("width: 150px;");
        msg.exec();
        delete localUrl;
    }
}
