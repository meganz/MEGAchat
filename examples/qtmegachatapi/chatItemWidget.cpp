#include "chatItemWidget.h"


#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMenu>
#include <iostream>

ChatItemWidget::ChatItemWidget(QWidget *parent, megachat::MegaChatApi* mChatApi, const megachat::MegaChatListItem *item) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    chatHandle = item->getChatId();
    megaChatApi=mChatApi;

    ui->setupUi(this);
    ui->mUnreadIndicator->hide();

    if (!item->isGroup())
        ui->mAvatar->setText("1");
    else
        ui->mAvatar->setText("G");

    ui->mName->setText(item->getTitle());
    chatWindowHandle=NULL;
}

void ChatItemWidget::invalidChatWindowHandle()
{
    this->chatWindowHandle=NULL;
}

void ChatItemWidget::updateToolTip(megachat::MegaChatApi* mChatApi, const megachat::MegaChatListItem *item)
{
    megachat::MegaChatRoom * chatRoom = mChatApi->getChatRoom(item->getChatId());
    std::string chatId = std::to_string(item->getChatId());
    std::string lastMessage;
    std::string lastMessageId = std::to_string(item->getLastMessageId());
    QString text = NULL;

    if(strcmp(item->getLastMessage(),"") == 0)
    {
        lastMessage="<empty>";
    }
    else
    {
        lastMessage=item->getLastMessage();
    }

    if(!item->isGroup())
    {
        std::string peerEmail= chatRoom->getPeerEmail(0);
        std::string peerHandle=std::to_string(item->getPeerHandle());
        text.append(tr("1on1 Chat room:"))
            .append(QString::fromStdString(chatId))
            .append(tr("\nEmail: "))
            .append(QString::fromStdString(peerEmail))
            .append(tr("\nUser handle: ")).append(QString::fromStdString(peerHandle))
            .append(tr("\nLast message:\n")).append(QString::fromStdString(lastMessage))
            .append(tr("\nLast message Id:\n")).append(QString::fromStdString(lastMessageId));
    }
    else
    {
        const char * peerName  = NULL;
        const char * peerEmail = NULL;
        std::string peerId = "";
        int  peerPriv = -1;
        int ownPrivilege = chatRoom->getOwnPrivilege();
        int participantsCount = chatRoom->getPeerCount();
        text.append(tr("Group chat room: "))
            .append(QString::fromStdString(chatId)
            .append(tr("\nOwn privilege: "))
            .append(QString(chatRoom->privToString(ownPrivilege)))
            .append(tr("\nOther participants:")));

        if (participantsCount==0)
        {
            text.append(" (none)");
        }
        else
        {
            text.append("\n");
            for(int i=0; i<participantsCount; i++)
            {
                peerName  = chatRoom->getPeerFullname(i);
                peerEmail = chatRoom->getPeerEmail(i);
                peerId    = std::to_string (chatRoom->getPeerHandle(i));
                peerPriv  = chatRoom->getPeerPrivilege(i);
                auto line = QString(" %1 (%2, %3): priv %4\n")
                        .arg(QString(peerName))
                        .arg(peerEmail?QString::fromStdString(peerEmail):tr("(email unknown)"))
                        .arg(QString::fromStdString(peerId))
                        .arg(QString(chatRoom->privToString(peerPriv)));
                text.append(line);
                delete [] peerName;
            }
            text.resize(text.size()-1);
        }
        text.append(tr("\nLast message:\n ")).append(QString::fromStdString(lastMessage));
    }
    setToolTip(text);
    delete chatRoom;
}

void ChatItemWidget::onUnreadCountChanged(int count)
{
    if (count < 0)
        ui->mUnreadIndicator->setText(QString::number(-count)+"+");
    else
        ui->mUnreadIndicator->setText(QString::number(count));

    ui->mUnreadIndicator->adjustSize();

    if (count)
    {
        if (!mLastOverlayCount)
            ui->mUnreadIndicator->show();
    }
    else
    {
        if (mLastOverlayCount)
            ui->mUnreadIndicator->hide();
    }
    mLastOverlayCount = count;
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



ChatWindow* ChatItemWidget::showChatWindow()
{
    ChatWindow* window;
    megachat::MegaChatRoom * chatRoom = this->megaChatApi->getChatRoom(chatHandle);

    if (!chatWindowHandle)
    {
        window = new ChatWindow(this, megaChatApi, chatRoom);
        chatWindowHandle=window;
    }
    else
    {
        window = static_cast<ChatWindow*>(chatWindowHandle);
    }

    window->show();
    window->openChatRoom();
    return window;
}

void ChatItemWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    showChatWindow();
}


void ChatItemWidget::onlineIndicatorUpdate(int newState)
{
    int virtPresence = (newState==megachat::MegaChatApi::CHAT_CONNECTION_ONLINE)
        ? megachat::MegaChatApi::CHAT_CONNECTION_ONLINE
        : megachat::MegaChatApi::CHAT_CONNECTION_IN_PROGRESS;

    ui->mOnlineIndicator->setStyleSheet(
                    QString("background-color: ")+gOnlineIndColors[virtPresence]+
                    ";border-radius: 4px");
}

ChatItemWidget::~ChatItemWidget()
{
    delete ui;
}

void ChatItemWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    if(megaChatApi->getChatListItem(chatHandle)->isGroup())
    {
        auto actLeave = menu.addAction(tr("Leave group chat"));
        connect(actLeave, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        auto actTopic = menu.addAction(tr("Set chat topic"));
        connect(actTopic, SIGNAL(triggered()), this, SLOT(setTitle()));
    }

    auto actTruncate = menu.addAction(tr("Truncate chat"));
    connect(actTruncate, SIGNAL(triggered()), this, SLOT(truncateChat()));
    menu.setStyleSheet("background-color: lightgray");
    menu.exec(event->globalPos());
}

void ChatItemWidget::setTitle()
{
    const char * title;
    QString qTitle = QInputDialog::getText(this, tr("Change chat topic"), tr("Leave blank for default title"));
    if (! qTitle.isNull())
        title = qTitle.toStdString().c_str();

    this->megaChatApi->setChatTitle(chatHandle,title);
}

void ChatItemWidget::leaveGroupChat()
{
    this->megaChatApi->leaveChat(chatHandle);
}
