#include "chatMessage.h"
#include "ui_chatMessageWidget.h"

const char* messageStatus[] =
{
  "Sending", "SendingManual", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

ChatMessage::ChatMessage(QWidget *parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg)
  : QWidget(parent),
  ui(new Ui::ChatMessageWidget)
  //mChatWindow(parent)
{    
    this->chatId=chatId;
    megaChatApi = mChatApi;
    ui->setupUi(this);
    message=msg;    
    setAuthor();
    setTimestamp(message->getTimestamp());
    setStatus(message->getStatus());

    /*
    if (msg.updated)
        setEdited();
    */
    /*
    if (!msg->isManagementMessage())
        ui->mMsgDisplay->setText(msg->getContent());
    else
        ui->mMsgDisplay->setText(managementInfoToString().c_str());*/
    //updateToolTip();
    show();
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
    //return *this;
}

void ChatMessage::setStatus(int status)
{
    if (status==megachat::MegaChatMessage::STATUS_UNKNOWN)
        ui->mStatusDisplay->setText("Invalid");
    else
        ui->mStatusDisplay->setText(messageStatus[status]);
}

void ChatMessage::setAuthor()
{
    //Only one
    //megachat::MegaChatHandle userHandle = message->getUserHandle();

   // const char * firstName=megaChatApi->getChatRoom(chatId)->getPeerFirstnameByHandle(userHandle);
/*
const char * firstName;

    int peerCount=megaChatApi->getChatRoom(chatId)->getPeerCount();





    for (int i=0; i<peerCount; i++)
    {
       firstName = megaChatApi->getChatRoom(chatId)->getPeerFirstname(i);
    }





    if(userHandle==this->megaChatApi->getMyUserHandle())
       {ui->mAuthorDisplay->setText(tr("me"));}
    else
       {ui->mAuthorDisplay->setText(tr(firstName));}*/
}


ChatMessage::~ChatMessage()
{
    delete ui;
}
