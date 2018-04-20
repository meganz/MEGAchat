#include "chatItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMenu>
#include <iostream>
#include <QMessageBox>

ChatItemWidget::ChatItemWidget(QWidget *parent, megachat::MegaChatApi* megaChatApi, const megachat::MegaChatListItem *item) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    mMainWin = (MainWindow *) parent;
    mLastMsgAuthor.clear();
    mListWidgetItem = NULL;
    mChatWindow = NULL;
    mMegaApi = mMainWin->mMegaApi;
    mLastOverlayCount = 0;
    mChatId = item->getChatId();
    mMegaChatApi = megaChatApi;
    ui->setupUi(this);
    int unreadCount = mMegaChatApi->getChatListItem(mChatId)->getUnreadCount();
    onUnreadCountChanged(unreadCount);
    ui->mName->setText(item->getTitle());

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

void ChatItemWidget::invalidChatWindowHandle()
{
    mChatWindow = NULL;
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
        delete uh;
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
            if (msgAuthor)
            {
                mLastMsgAuthor.assign(msgAuthor);
            }
            else
            {
                mLastMsgAuthor = "Unknown participant";
                mMegaChatApi->getUserFirstname(lastMessageSender);
            }
            delete msgAuthor;
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
                delete uh;
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
                delete uh;
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

        case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
            lastMessage.append("User ").append(senderHandle)
                       .append(" set chat title: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CONTAINS_META: // fall-through
            lastMessage.append("metadata: ").append(item->getLastMessage());
            break;

        default:
            lastMessage = item->getLastMessage();
            break;
    }

    if(item->getLastMessageId() != megachat::MEGACHAT_INVALID_HANDLE)
    {
        lastMessageId_64 = auxLastMessageId_64;
    }

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
            .append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage));
        delete peerHandle_64;
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
                delete peerName;
                delete peerId_64;
            }
            text.resize(text.size()-1);
        }
        text.append(tr("\n\n"));
        text.append(tr("\nLast message Id: ")).append(lastMessageId_64);
        text.append(tr("\nLast message Sender: ")).append(mLastMsgAuthor.c_str());
        text.append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage));
    }
    setToolTip(text);
    delete chatRoom;
    delete chatId_64;
    delete auxLastMessageId_64;

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
        if (msg)
        {
            size_t len = strlen(msg);
            if (len == 0)
            {
                return NULL;
            }

            msgAuthor = new char[len];
            strcpy(msgAuthor, msg);
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

ChatWindow *ChatItemWidget::showChatWindow()
{
    std::string titleStd = ui->mName->text().toStdString();
    const char *chatWindowTitle = titleStd.c_str();
    megachat::MegaChatRoom *chatRoom = this->mMegaChatApi->getChatRoom(mChatId);

    if (!mChatWindow)
    {
        mChatWindow = new ChatWindow(mMainWin, mMegaChatApi, chatRoom->copy(), chatWindowTitle);
        mChatWindow->show();
        mChatWindow->openChatRoom();
    }
    else
    {
        mChatWindow->show();
        mChatWindow->setWindowState(Qt::WindowActive);
    }
    delete chatRoom;
    return mChatWindow;
}


ChatWindow *ChatItemWidget::getChatWindow()
{
    return mChatWindow;
}

void ChatItemWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    showChatWindow();
}

void ChatItemWidget::setChatWindow(ChatWindow *chatWindow)
{
    mChatWindow = chatWindow;
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
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);
    bool canChangePrivs = (chatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);
    delete chatRoom;

    QMenu menu(this);
    if(mMegaChatApi->getChatListItem(mChatId)->isGroup())
    {
        auto actLeave = menu.addAction(tr("Leave group chat"));
        connect(actLeave, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        auto actTopic = menu.addAction(tr("Set chat topic"));
        actTopic->setEnabled(canChangePrivs);
        connect(actTopic, SIGNAL(triggered()), this, SLOT(setTitle()));
    }

    auto actTruncate = menu.addAction(tr("Truncate chat"));
    actTruncate->setEnabled(canChangePrivs);
    connect(actTruncate, SIGNAL(triggered()), this, SLOT(truncateChat()));
    menu.exec(event->globalPos());
    menu.deleteLater();
}

void ChatItemWidget::truncateChat()
{
    this->mMegaChatApi->clearChatHistory(mChatId);
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
