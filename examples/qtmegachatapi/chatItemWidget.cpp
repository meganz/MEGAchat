#include "chatItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMenu>
#include <iostream>

ChatItemWidget::ChatItemWidget(QWidget *parent, megachat::MegaChatApi* megaChatApi, const megachat::MegaChatListItem *item) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    mMainWin = (MainWindow *) parent;
    mListWidgetItem = NULL;
    mChatWindow = NULL;
    mMegaApi = mMainWin->mMegaApi;
    mLastOverlayCount = 0;
    mOlderMessageLoaded = 0;
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

void ChatItemWidget::updateToolTip(const megachat::MegaChatListItem *item)
{
    QString text = NULL;
    const char *lastMessage;
    const char *lastMessageId_64 = "----------";;
    const char *auxLastMessageId_64 = mMainWin->mMegaApi->userHandleToBase64(item->getLastMessageId());
    const char *chatId_64 = mMainWin->mMegaApi->userHandleToBase64(mChatId);
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);
    megachat::MegaChatHandle lastMessageId = item->getLastMessageId();
    megachat::MegaChatHandle auxHandle = item->getLastMessageId();
    int lastMessageType = item->getLastMessageType();

    if (lastMessageType == megachat::MegaChatMessage::TYPE_INVALID)
    {
        lastMessage = "<empty>";
    }
    else if (lastMessageType == 0xFF)
    {
        lastMessage = "<loading...>";
    }
    else
    {
        lastMessage = item->getLastMessage();
        if(item->getLastMessageId()!= megachat::MEGACHAT_INVALID_HANDLE)
        {
            lastMessageId_64 = auxLastMessageId_64;
        }
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
            .append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage))
            .append(tr("\nLast message Id: ")).append(lastMessageId_64);
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
        text.append(tr("\nLast message:\n ")).append(QString::fromStdString(lastMessage));
        text.append(tr("\nLast message Id: ")).append(lastMessageId_64);
    }
    setToolTip(text);
    delete chatRoom;
    delete chatId_64;
    delete auxLastMessageId_64;
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
        mChatWindow = new ChatWindow(this, mMegaChatApi, chatRoom->copy(), chatWindowTitle);
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

void ChatItemWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    showChatWindow();
}

QListWidgetItem *ChatItemWidget::getWidgetItem() const
{
    return mListWidgetItem;
}

void ChatItemWidget::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
}

megachat::MegaChatHandle ChatItemWidget::getOlderMessageLoaded() const
{
    return mOlderMessageLoaded;
}

void ChatItemWidget::setOlderMessageLoaded(const megachat::MegaChatHandle &msgId)
{
    mOlderMessageLoaded = msgId;
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
    if(mMegaChatApi->getChatListItem(mChatId)->isGroup())
    {
        auto actLeave = menu.addAction(tr("Leave group chat"));
        connect(actLeave, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        auto actTopic = menu.addAction(tr("Set chat topic"));
        connect(actTopic, SIGNAL(triggered()), this, SLOT(setTitle()));
    }

    auto actTruncate = menu.addAction(tr("Truncate chat"));
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
        this->mMegaChatApi->setChatTitle(mChatId,title.c_str());
    }
}

void ChatItemWidget::leaveGroupChat()
{
    this->mMegaChatApi->leaveChat(mChatId);
}
