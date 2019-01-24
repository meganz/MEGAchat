#include "chatItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMenu>
#include <iostream>
#include <QMessageBox>

ChatItemWidget::ChatItemWidget(QWidget *parent, megachat::MegaChatApi *megaChatApi, const megachat::MegaChatListItem *item) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    mMainWin = (MainWindow *) parent;
    mLastMsgAuthor.clear();
    mListWidgetItem = NULL;
    mMegaApi = mMainWin->mMegaApi;
    mLastOverlayCount = 0;
    mChatId = item->getChatId();
    mMegaChatApi = megaChatApi;
    ui->setupUi(this);
    int unreadCount = item->getUnreadCount();
    onUnreadCountChanged(unreadCount);

    QString text = NULL;
    if (item->isArchived())
    {
        text.append(item->getTitle())
        .append("[A]");

        if (!item->isActive())
        {
            text.append("[H]");
        }
        ui->mName->setText(text);
        ui->mName->setStyleSheet("color:#DEF0FC;");
        ui->mAvatar->setStyleSheet("color:#DEF0FC;");
    }
    else
    {
        if (!item->isActive())
        {
            text.append(item->getTitle())
            .append(" [H]");
            ui->mName->setText(text);
            ui->mName->setStyleSheet("color:#FFC9C6;");
            ui->mAvatar->setStyleSheet("color:#FFC9C6;");
        }
        else
        {
            ui->mName->setText(item->getTitle());
            ui->mName->setStyleSheet("color:#FFFFFF; font-weight:bold;");
            ui->mAvatar->setStyleSheet("color:#FFFFFF; font-weight:bold;");
        }
    }

    if (!item->isGroup())
    {
        ui->mAvatar->setText("1");
    }
    else
    {
        ui->mAvatar->setText("G");
    }

    int status = mMegaChatApi->getChatConnectionState(mChatId);
    this->onlineIndicatorUpdate(status);
}

void ChatItemWidget::updateToolTip(const megachat::MegaChatListItem *item, const char *author)
{
    QString text = NULL;
    std::string senderHandle;
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);
    megachat::MegaChatHandle lastMessageId = item->getLastMessageId();
    int lastMessageType = item->getLastMessageType();
    std::string lastMessage;
    const char *lastMessageId_64 = "----------";
    const char *auxLastMessageId_64 = mMainWin->mMegaApi->userHandleToBase64(lastMessageId);
    const char *chatId_64 = mMainWin->mMegaApi->userHandleToBase64(mChatId);

    megachat::MegaChatHandle lastMessageSender = item->getLastMessageSender();
    if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
    {
        senderHandle = "";
    }
    else
    {
        char *uh = mMainWin->mMegaApi->userHandleToBase64(lastMessageSender);
        senderHandle.assign(uh);
        delete [] uh;
    }

    if (author)
    {
        mLastMsgAuthor.assign(author);
    }
    else
    {
        if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
        {
            mLastMsgAuthor = "";
        }
        else
        {
            const char *msgAuthor = getLastMessageSenderName(lastMessageSender);
            if (msgAuthor || (msgAuthor = mMainWin->mApp->getFirstname(lastMessageSender)))
            {
                mLastMsgAuthor.assign(msgAuthor);
            }
            else
            {
                mLastMsgAuthor = "Loading firstname...";
            }
            delete [] msgAuthor;
        }
    }
    switch (lastMessageType)
    {
        case megachat::MegaChatMessage::TYPE_INVALID:
            lastMessage = "<No history>";
            break;

        case 0xFF:
            lastMessage = "<loading...>";
            break;

        case megachat::MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
        {
            std::string targetName;
            if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
            {
                targetName = "";
            }
            else
            {
                char *uh = mMainWin->mMegaApi->userHandleToBase64(item->getLastMessageHandle());
                targetName.assign(uh);
                delete [] uh;
            }

            bool removed = item->getLastMessagePriv() == megachat::MegaChatRoom::PRIV_RM;
            lastMessage.append("User ").append(senderHandle)
                    .append(removed ? " removed" : " added")
                    .append(" user ").append(targetName);
            break;
        }
        case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
        {
            std::string targetName;
            std::string priv;
            if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
            {
                targetName = "";
                priv = "";
            }
            else
            {
                char *uh = mMainWin->mMegaApi->userHandleToBase64(item->getLastMessageHandle());
                targetName.assign(uh);
                priv.assign(megachat::MegaChatRoom::privToString(item->getLastMessagePriv()));
                delete [] uh;
            }
            lastMessage.append("User ").append(senderHandle)
                       .append(" set privilege of user ").append(targetName)
                       .append(" to ").append(priv);
            break;
        }
        case megachat::MegaChatMessage::TYPE_TRUNCATE:
            lastMessage = "Truncate";
            break;

        case megachat::MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
            lastMessage.append("User ").append(senderHandle)
                       .append(" attached a contact: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
            lastMessage.append("User ").append(senderHandle)
                       .append(" attached a node: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
            lastMessage.append("User ").append(senderHandle)
                       .append(" sent a ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
            lastMessage.append("User ").append(senderHandle)
                       .append(" set chat title: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CONTAINS_META: // fall-through
            lastMessage.append("Metadata: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CALL_ENDED:
        {
            QString qstring(item->getLastMessage());
            if (!qstring.isEmpty())
            {
                QStringList stringList = qstring.split(0x01);
                assert(stringList.size() >= 2);
                lastMessage.append("Call started by: ")
                           .append(senderHandle)
                           .append(" Duration: ")
                           .append(stringList.at(0).toStdString())
                           .append("secs TermCode: ")
                           .append(stringList.at(1).toStdString());
            }
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_STARTED:
        {
            lastMessage.append("User ").append(senderHandle)
               .append(" has started a call");
        }
        default:
            lastMessage = item->getLastMessage();
            break;
    }

    if(item->getLastMessageId() != megachat::MEGACHAT_INVALID_HANDLE)
    {
        lastMessageId_64 = auxLastMessageId_64;
    }

    QDateTime t;
    t.setTime_t(item->getLastTimestamp());
    QString lastTs = t.toString("hh:mm:ss - dd.MM.yy");

    if(!item->isGroup())
    {
        const char *peerEmail = chatRoom->getPeerEmail(0);
        const char *peerHandle_64 = mMainWin->mMegaApi->userHandleToBase64(item->getPeerHandle());
        text.append(tr("1on1 Chat room:"))
            .append(QString::fromStdString(chatId_64))
            .append(tr("\nEmail: "))
            .append(QString::fromStdString(peerEmail))
            .append(tr("\nUser handle: ")).append(QString::fromStdString(peerHandle_64))
            .append(tr("\n\n"))
            .append(tr("\nLast message Id: ")).append(lastMessageId_64)
            .append(tr("\nLast message Sender: ")).append(mLastMsgAuthor.c_str())
            .append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage))
            .append(tr("\nLast ts: ")).append(lastTs);
        delete [] peerHandle_64;
    }
    else
    {
        int ownPrivilege = chatRoom->getOwnPrivilege();
        text.append(tr("Group chat room: "))
            .append(QString::fromStdString(chatId_64)
            .append(tr("\nOwn privilege: "))
            .append(QString(chatRoom->privToString(ownPrivilege)))
            .append(tr("\nOther participants:")));

        int participantsCount = chatRoom->getPeerCount();
        if (participantsCount == 0)
        {
            text.append(" (none)");
        }
        else
        {
            text.append("\n");
            for(int i=0; i<participantsCount; i++)
            {
                const char *peerName = chatRoom->getPeerFullname(i);
                const char *peerEmail = chatRoom->getPeerEmail(i);
                const char *peerId_64 = mMainWin->mMegaApi->userHandleToBase64(chatRoom->getPeerHandle(i));
                int peerPriv = chatRoom->getPeerPrivilege(i);
                auto line = QString(" %1 (%2, %3): priv %4\n")
                        .arg(QString(peerName))
                        .arg(peerEmail ? QString::fromStdString(peerEmail) : tr("(email unknown)"))
                        .arg(QString::fromStdString(peerId_64))
                        .arg(QString(chatRoom->privToString(peerPriv)));
                text.append(line);
                delete [] peerName;
                delete [] peerId_64;
            }
            text.resize(text.size()-1);
        }
        text.append(tr("\n\n"));
        text.append(tr("\nLast message Id: ")).append(lastMessageId_64);
        text.append(tr("\nLast message Sender: ")).append(mLastMsgAuthor.c_str());
        text.append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage));
        text.append(tr("\nLast ts: ")).append(lastTs);
    }
    setToolTip(text);
    delete chatRoom;
    delete [] chatId_64;
    delete [] auxLastMessageId_64;
}

const char *ChatItemWidget::getLastMessageSenderName(megachat::MegaChatHandle msgUserId)
{
    char *msgAuthor = NULL;
    if(msgUserId == mMegaChatApi->getMyUserHandle())
    {
        msgAuthor = new char[3];
        strcpy(msgAuthor, "Me");
    }
    else
    {
        megachat::MegaChatRoom *chatRoom = this->mMegaChatApi->getChatRoom(mChatId);
        const char *msg = chatRoom->getPeerFirstnameByHandle(msgUserId);
        size_t len = msg ? strlen(msg) : 0;
        if (len)
        {
            msgAuthor = new char[len + 1];
            strncpy(msgAuthor, msg, len);
            msgAuthor[len] = '\0';
        }
        delete chatRoom;
    }
    return msgAuthor;
}

void ChatItemWidget::onUnreadCountChanged(int count)
{
    if(count < 0)
    {
        ui->mUnreadIndicator->setText(QString::number(-count)+"+");
    }
    else
    {
        ui->mUnreadIndicator->setText(QString::number(count));
        if (count == 0)
        {
            ui->mUnreadIndicator->hide();
        }
    }
    ui->mUnreadIndicator->adjustSize();
}

void ChatItemWidget::onTitleChanged(const std::string& title)
{
    QString text = QString::fromStdString(title);
    ui->mName->setText(text);
}

void ChatItemWidget::showAsHidden()
{
    ui->mName->setStyleSheet("color: rgba(0,0,0,128)\n");
}

void ChatItemWidget::unshowAsHidden()
{
    ui->mName->setStyleSheet("color: rgba(255,255,255,255)\n");
}

void ChatItemWidget::mouseDoubleClickEvent(QMouseEvent */*event*/)
{
   ChatListItemController *itemController = mMainWin->getChatControllerById(mChatId);
   if (itemController)
   {
      itemController->showChatWindow();
   }
}

QListWidgetItem *ChatItemWidget::getWidgetItem() const
{
    return mListWidgetItem;
}

void ChatItemWidget::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
}

void ChatItemWidget::onlineIndicatorUpdate(int newState)
{
    ui->mOnlineIndicator->setStyleSheet(
        QString("background-color: ")+gOnlineIndColors[newState]+
        ";border-radius: 4px");
}

ChatItemWidget::~ChatItemWidget()
{
    delete ui;
}

megachat::MegaChatHandle ChatItemWidget::getChatId() const
{
    return mChatId;
}

void ChatItemWidget::setChatHandle(const megachat::MegaChatHandle &chatId)
{
    mChatId = chatId;
}

void ChatItemWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);

    auto actLeave = menu.addAction(tr("Leave group chat"));
    connect(actLeave, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));

    auto actTopic = menu.addAction(tr("Set chat topic"));
    connect(actTopic, SIGNAL(triggered()), this, SLOT(setTitle()));

    auto actTruncate = menu.addAction(tr("Truncate chat"));
    connect(actTruncate, SIGNAL(triggered()), this, SLOT(truncateChat()));

    auto actArchive = menu.addAction("Archive chat");
    connect(actArchive, SIGNAL(toggled(bool)), this, SLOT(archiveChat(bool)));
    actArchive->setCheckable(true);
    actArchive->setChecked(chatRoom->isArchived());

    QMenu *clMenu = menu.addMenu("Chat links");

    auto actQueryLink = clMenu->addAction(tr("Query chat link"));
    connect(actQueryLink, SIGNAL(triggered()), this, SLOT(queryChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    auto actExportLink = clMenu->addAction(tr("Export chat link"));
    connect(actExportLink, SIGNAL(triggered()), this, SLOT(exportChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    auto actRemoveLink = clMenu->addAction(tr("Remove chat link"));
    connect(actRemoveLink, SIGNAL(triggered()), this, SLOT(removeChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    auto joinChatLink = clMenu->addAction("Join chat link");
    connect(joinChatLink, SIGNAL(triggered()), this, SLOT(on_mJoinBtn_clicked()));
    // TODO: connect to slot in chat-links branch once merged

    auto actClosePreview = clMenu->addAction(tr("Close preview"));
    connect(actClosePreview, SIGNAL(triggered()), this, SLOT(closePreview()));
    // TODO: connect to slot in chat-links branch once merged

    menu.addSeparator();
    auto actSetPrivate = clMenu->addAction(tr("Set chat private"));
    connect(actSetPrivate, SIGNAL(triggered()), this, SLOT(closeChatLink()));
    // TODO: connect to slot in chat-links branch once merged


    delete chatRoom;
    menu.exec(event->globalPos());
    menu.deleteLater();
}

void ChatItemWidget::truncateChat()
{
    this->mMegaChatApi->clearChatHistory(mChatId);
}

void ChatItemWidget::archiveChat(bool checked)
{
    MegaChatRoom *room = mMegaChatApi->getChatRoom(mChatId);
    if (room->isArchived() != checked)
    {
        mMegaChatApi->archiveChat(mChatId, checked);
    }
    delete room;
}

void ChatItemWidget::setTitle()
{
    std::string title;
    QString qTitle = QInputDialog::getText(this, tr("Change chat topic"), tr("Leave blank for default title"));
    if (!qTitle.isNull())
    {
        title = qTitle.toStdString();
        if (title.empty())
        {
            QMessageBox::warning(this, tr("Set chat title"), tr("You can't set an empty title"));
            return;
        }
        this->mMegaChatApi->setChatTitle(mChatId,title.c_str());
    }
}

void ChatItemWidget::leaveGroupChat()
{
    this->mMegaChatApi->leaveChat(mChatId);
}
