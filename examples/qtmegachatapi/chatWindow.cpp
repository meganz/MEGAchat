#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>
#include "MainWindow.h"

ChatWindow::ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *room, const char * title)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{
    loadedMessages=0;
    oldestMessage=0;
    newestMessage=0;
    nSending=0;
    nManualSending=0;
    this->chatItemWidget = (ChatItemWidget *) parent;
    chatRoomHandle=room;
    megaChatApi=mChatApi;
    ui->setupUi(this);
    ui->mSplitter->setStretchFactor(0,1);
    ui->mSplitter->setStretchFactor(1,0);
    ui->mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->mMsgSendBtn,  SIGNAL(clicked()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(sendMsg()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(editLastMsg()), this, SLOT(editLastMsg()));
    connect(ui->mMessageList, SIGNAL(requestHistory()), this, SLOT(onMsgListRequestHistory()));
    connect(ui->mMembersBtn,  SIGNAL(clicked(bool)), this, SLOT(onMembersBtn(bool)));
    connect(ui->mMessageList->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));

    ui->mTitleLabel->setText(title);

    #ifndef KARERE_DISABLE_WEBRTC
        connect(ui->mVideoCallBtn, SIGNAL(clicked(bool)), this, SLOT(onVideoCallBtn(bool)));
        connect(ui->mAudioCallBtn, SIGNAL(clicked(bool)), this, SLOT(onAudioCallBtn(bool)));
    #else
        ui->mAudioCallBtn->hide();
        ui->mVideoCallBtn->hide();
    #endif

    ui->mChatdStatusDisplay->hide();

    if (!chatRoomHandle->isGroup())
        ui->mMembersBtn->hide();
    else
        setAcceptDrops(true);

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    if (!chatRoomHandle->isActive())
        ui->mMessageEdit->setEnabled(false);

    QDialog::show();

    //We add ChatRoomListener
    this->megaChatRoomListenerDelegate =  new ::megachat::QTMegaChatRoomListener(megaChatApi, this);
}

void ChatWindow::openChatRoom()
{
    int source=0;
    bool result = this->megaChatApi->openChatRoom(chatRoomHandle->getChatId(),megaChatRoomListenerDelegate);
    if(result)
    {
            source = megaChatApi->loadMessages(chatRoomHandle->getChatId(),NMESSAGES_LOAD);

    }
}

ChatWindow::~ChatWindow()
{
    chatItemWidget->invalidChatWindowHandle();
    this->megaChatApi->closeChatRoom(chatRoomHandle->getChatId(),megaChatRoomListenerDelegate);
    delete megaChatRoomListenerDelegate;
    delete ui;
}


void ChatWindow::onMsgSendBtn()
{
    QString qtext = ui->mMessageEdit->toPlainText();

    if (qtext.isEmpty())
        return;
    ui->mMessageEdit->setText(QString());
    //temporary only use this method to send message without edition
   megachat::MegaChatMessage * tempMessage= this->megaChatApi->sendMessage(chatRoomHandle->getChatId(), qtext.toUtf8().toStdString().c_str());
   nSending+=1;
   int auxIndex=loadedMessages+this->nSending;
   addMsgWidget(tempMessage, (loadedMessages+this->nSending));
    /*
    if (mEditedWidget)
    {
        submitEdit(text.data(), text.size());
        assert(!mEditedWidget);
    }
    else
    {
        postNewMessage(text.data(), text.size());
    }*/
}


/*
void postNewMessage(const char* data, size_t size, unsigned char type=chatd::Message::kMsgNormal)
{
    if (!data)
        throw std::runtime_error("postNewMessage: Can't post message with NULL data");

    auto msg = mChat->msgSubmit(data, size, type, nullptr);
    msg->userp = addMsgWidget(*msg, CHATD_IDX_INVALID, chatd::Message::kSending, false);
    ui.mMessageList->scrollToBottom();
}*/



void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat){}


void ChatWindow::onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg->isDeleted())
     {
        eraseChatMessage(msg);
        return;
     }

    ChatMessage * chatMessage = findChatMessage(msg->getMsgId());

    if (msg->hasChanged(megachat::MegaChatMessage::CHANGE_TYPE_CONTENT))
    {
        if(chatMessage)
        {
            chatMessage->setMessageContent(msg->getContent());
            if (msg->isEdited())
                chatMessage->markAsEdited();
        }
    }

    if (msg->hasChanged(megachat::MegaChatMessage::CHANGE_TYPE_STATUS))
    {
        if(msg->getStatus()==megachat::MegaChatMessage::STATUS_SERVER_RECEIVED)
        {
            if(eraseChatMessage(msg))
            {
                addMsgWidget(msg->copy(), loadedMessages);
                loadedMessages+=1;
                nSending-=1;
            }
        }
        else
        {
            if(chatMessage)
                chatMessage->setStatus(msg->getStatus());
        }
     }
}

void ChatWindow::deleteChatMessage(megachat::MegaChatMessage *msg)
{
        megachat::MegaChatHandle msgId=msg->getMsgId();
        std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
        itMessages = messagesWidgets.find(msgId);

        if (itMessages != messagesWidgets.end())
        {
            ChatMessage * auxMessage = itMessages->second;
            ui->mMessageList->removeItemWidget(auxMessage->getWidgetItem());
            messagesWidgets.erase(itMessages);

            this->megaChatApi->deleteMessage(chatRoomHandle->getChatId(), msg->getMsgId());
            //delete auxMessage;

            //Remove from api;
            //In return call chatmessage destructor
        }
}


bool ChatWindow::eraseChatMessage(megachat::MegaChatMessage *msg)
{
    megachat::MegaChatHandle msgId=msg->getTempId();
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = messagesWidgets.find(msgId);

    if (itMessages != messagesWidgets.end())
    {
        ChatMessage * auxMessage = itMessages->second;

        ui->mMessageList->removeItemWidget(auxMessage->getWidgetItem());

        messagesWidgets.erase(itMessages);
        delete auxMessage;
        return true;
    }
    return false;
}


ChatMessage * ChatWindow::findChatMessage(megachat::MegaChatHandle msgId)
{
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = messagesWidgets.find(msgId);

    if (itMessages != messagesWidgets.end())
    {
        return itMessages->second;
    }
    else
    {
        return NULL;
    }
}


void ChatWindow::onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg)
    {
        addMsgWidget(msg->copy(), (loadedMessages));
        loadedMessages+=1;
    }
}

void ChatWindow::onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg)
    {
        if(msg->isDeleted())
            return;

        std::cout << "[Mensaje cargado] " << msg->getMsgId();
        if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING) //we need to add it to the actual end of the list
        {
            //GUI_LOG_DEBUG("Adding unsent message widget of msgxid %s", msg.id().toString().c_str());
            nSending+=1;
            addMsgWidget(msg->copy(), loadedMessages+nSending);
        }
        else
        {
            addMsgWidget(msg->copy(), -loadedMessages);
            loadedMessages+=1;
        }
    }
    else
    {

    int pendingLoad;
    pendingLoad=NMESSAGES_LOAD-loadedMessages+nSending+nManualSending;
    if (pendingLoad>0)
    {
        int source = megaChatApi->loadMessages(chatRoomHandle->getChatId(),pendingLoad);
        if (source == megachat::MegaChatApi::SOURCE_NONE)
        {
            pendingLoad = 0;
        }
        else if (source == megachat::MegaChatApi::SOURCE_ERROR)
        {
            //Show error
        }
    }

    }
}


QListWidgetItem* ChatWindow::addMsgWidget (megachat::MegaChatMessage * msg, int index)
{
    megachat::MegaChatHandle chatId = chatRoomHandle->getChatId();
    ChatMessage * widget = new ChatMessage(this, megaChatApi, chatId, msg);
    QListWidgetItem* item = new QListWidgetItem;
    widget->setWidgetItem(item);

    if (msg->getStatus()== megachat::MegaChatMessage::STATUS_DELIVERED)
        messagesWidgets.insert(std::pair<megachat::MegaChatHandle, ChatMessage *>(msg->getMsgId(),widget));
    else
        messagesWidgets.insert(std::pair<megachat::MegaChatHandle, ChatMessage *>(msg->getTempId(),widget));

    item->setSizeHint(widget->size());
    ui->mMessageList->insertItem(index, item);
    ui->mMessageList->setItemWidget(item, widget);
    ui->mMessageList->scrollToBottom();

    if (!widget->isMine() && msg->getStatus()==megachat::MegaChatMessage::STATUS_NOT_SEEN)
        megaChatApi->setMessageSeen(chatRoomHandle->getChatId(), msg->getMsgId());
    return item;
}


void ChatWindow::onMembersBtn(bool)
{
    if(chatRoomHandle->isGroup())
    {
        QMenu menu(this);
        createMembersMenu(menu);
        menu.setLayoutDirection(Qt::RightToLeft);
        menu.adjustSize();
        menu.exec(ui->mMembersBtn->mapToGlobal(
            QPoint(-menu.width()+ui->mMembersBtn->width(), ui->mMembersBtn->height())));
    }
}

void ChatWindow::createMembersMenu(QMenu& menu)
{
    if (chatRoomHandle->getPeerCount()==0)
    {
        menu.addAction(tr("You are alone in this chatroom"))->setEnabled(false);
        return;
    }

    for (int i=0; i<chatRoomHandle->getPeerCount(); i++)
    {
        if(chatRoomHandle->getOwnPrivilege()== megachat::MegaChatRoom::PRIV_MODERATOR)
        {
            auto entry = menu.addMenu(chatRoomHandle->getPeerFirstname(i));

            auto actRemove = entry->addAction(tr("Remove from chat"));
            actRemove->setProperty("userHandle", QVariant((qulonglong)chatRoomHandle->getPeerHandle(i)));
            connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));

            auto menuSetPriv = entry->addMenu(tr("Set privilege"));
            auto actSetPrivFullAccess = menuSetPriv->addAction(tr("Moderator"));
            actSetPrivFullAccess->setProperty("userHandle", QVariant((qulonglong)chatRoomHandle->getPeerHandle(i)));
            connect(actSetPrivFullAccess, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

            auto actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only"));
            actSetPrivReadOnly->setProperty("userHandle", QVariant((qulonglong)chatRoomHandle->getPeerHandle(i)));
            connect(actSetPrivReadOnly, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

            auto actSetPrivStandard = menuSetPriv->addAction(tr("Standard"));
            actSetPrivStandard->setProperty("userHandle", QVariant((qulonglong)chatRoomHandle->getPeerHandle(i)));
            connect(actSetPrivStandard, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
        }
    }
}

void ChatWindow::onMemberRemove()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant v = action->property("userHandle");
    megachat::MegaChatHandle userhand = v.toLongLong();
    megaChatApi->removeFromChat(chatRoomHandle->getChatId(), userhand);
    //We need to update the participants list when onRequestFinish is received
}



void ChatWindow::onMemberSetPriv()
{
      int privilege;
      QAction *action = qobject_cast<QAction *>(sender());
      if (!action)
         return;

      if(action->text()=="Read-only")
            privilege=megachat::MegaChatPeerList::PRIV_RO;

      if(action->text()=="Moderator")
            privilege=megachat::MegaChatPeerList::PRIV_MODERATOR;

      if(action->text()=="Standard")
            privilege=megachat::MegaChatPeerList::PRIV_STANDARD;

      QVariant v = action->property("userHandle");
      megachat::MegaChatHandle userhand = v.toLongLong();
      ChatItemWidget* chatItem = (ChatItemWidget*) this->parent();
      megachat::MegaChatHandle chatId=chatItem->getChatHandle();
      this->megaChatApi->updateChatPermissions(chatId, userhand, privilege);
}


