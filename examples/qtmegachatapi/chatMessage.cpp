#include "chatMessage.h"
#include "ui_chatMessageWidget.h"
#include "qmenu.h"


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
    //updateToolTip();
    show();
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
    message = message;
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
    return this->message->getUserHandle()  == this->megaChatApi->getMyUserHandle();
}

void ChatMessage::markAsEdited()
{
    ui->mStatusDisplay->setText(ui->mStatusDisplay->text()+" (Edited)");
}

ChatMessage::~ChatMessage()
{
    delete ui;
}

void ChatMessage::onMessageCtxMenu(const QPoint& point)
{
   if (isMine())
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



