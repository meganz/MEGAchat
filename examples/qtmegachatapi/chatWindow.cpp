#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>
#include "MainWindow.h"

ChatWindow::ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *cRoom, const char * title)
    : QDialog(0),
      ui(new Ui::ChatWindowUi)
{
    nSending = 0;
    loadedMessages = 0;
    nManualSending = 0;
    chatRoom = cRoom;
    megaChatApi = mChatApi;
    chatItemWidget = (ChatItemWidget *) parent;
    logger = ((MainWindow *)chatItemWidget->parent())->logger;

    ui->setupUi(this);
    ui->mSplitter->setStretchFactor(0,1);
    ui->mSplitter->setStretchFactor(1,0);
    ui->mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->mTitleLabel->setText(title);
    ui->mChatdStatusDisplay->hide();
    connect(ui->mMsgSendBtn,  SIGNAL(clicked()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(sendMsg()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(editLastMsg()), this, SLOT(editLastMsg()));
    connect(ui->mMessageList, SIGNAL(requestHistory()), this, SLOT(onMsgListRequestHistory()));
    connect(ui->mMembersBtn,  SIGNAL(clicked(bool)), this, SLOT(onMembersBtn(bool)));
    connect(ui->mMessageList->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));

    #ifndef KARERE_DISABLE_WEBRTC
        connect(ui->mVideoCallBtn, SIGNAL(clicked(bool)), this, SLOT(onVideoCallBtn(bool)));
        connect(ui->mAudioCallBtn, SIGNAL(clicked(bool)), this, SLOT(onAudioCallBtn(bool)));
    #else
        ui->mAudioCallBtn->hide();
        ui->mVideoCallBtn->hide();
    #endif

    if (!chatRoom->isGroup())
        ui->mMembersBtn->hide();
    else
        setAcceptDrops(true);

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    if (!chatRoom->isActive())
        ui->mMessageEdit->setEnabled(false);

    QDialog::show();
    this->megaChatRoomListenerDelegate =  new ::megachat::QTMegaChatRoomListener(megaChatApi, this);
}

void ChatWindow::openChatRoom()
{
    int source=0;
    bool result = this->megaChatApi->openChatRoom(chatRoom->getChatId(),megaChatRoomListenerDelegate);
    if(result)
    {
        source = megaChatApi->loadMessages(chatRoom->getChatId(),NMESSAGES_LOAD);
    }
}

ChatWindow::~ChatWindow()
{
    this->megaChatApi->closeChatRoom(chatRoom->getChatId(),megaChatRoomListenerDelegate);
    chatItemWidget->invalidChatWindowHandle();
    delete megaChatRoomListenerDelegate;
    delete chatRoom;
    delete ui;
}

void ChatWindow::onMsgSendBtn()
{
    QString qtext = ui->mMessageEdit->toPlainText();
    ui->mMessageEdit->setText(QString());
    if (qtext.isEmpty())
        return;

    megachat::MegaChatMessage * tempMessage= this->megaChatApi->sendMessage(chatRoom->getChatId(), qtext.toUtf8().toStdString().c_str());
    nSending+=1;
    addMsgWidget(tempMessage, (loadedMessages)+nSending);

}

void ChatWindow::moveManualSendingToSending(megachat::MegaChatMessage * msg)
{
    nSending+=1;
    nManualSending-=1;
    addMsgWidget(msg, (loadedMessages+nSending));


}

void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat)
{
    megachat::MegaChatRoom *auxRoom = this->megaChatApi->getChatRoom(chatRoom->getChatId());
    delete chatRoom;
    chatRoom = auxRoom;
}


void ChatWindow::onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    megachat::MegaChatHandle msgId;
    if(msg->isDeleted())
    {
        eraseChatMessage(msg, false);
        return;
    }

    if (msg->getStatus()== megachat::MegaChatMessage::STATUS_DELIVERED)
        msgId=msg->getMsgId();
    else
        msgId=msg->getTempId();

    ChatMessage * chatMessage = findChatMessage(msgId);
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
            eraseChatMessage(msg, true);
            nSending-=1;
            megachat::MegaChatMessage * auxMSG = msg->copy();
            megachat::MegaChatHandle msgId=auxMSG->getMsgId();
            addMsgWidget(auxMSG, loadedMessages);
            chatItemWidget->setOlderMessageLoaded(msg->getMsgId());
            loadedMessages+=1;
        }
        else
        {
            if(msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
            {
                //Complete case
            }
            else
            {
                if(chatMessage)
                    chatMessage->setStatus(msg->getStatus());
            }
        }
     }
}

void ChatWindow::deleteChatMessage(megachat::MegaChatMessage *msg)
{
   std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
   megachat::MegaChatHandle msgId=msg->getMsgId();
   itMessages = messagesWidgets.find(msgId);

   if (itMessages != messagesWidgets.end())
   {
      this->megaChatApi->deleteMessage(chatRoom->getChatId(), msg->getMsgId());
   }
}


bool ChatWindow::eraseChatMessage(megachat::MegaChatMessage *msg, bool temporal)
{
   std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
   megachat::MegaChatHandle msgId;

   if (temporal)
      msgId=msg->getTempId();
   else
      msgId=msg->getMsgId();

   itMessages = messagesWidgets.find(msgId);
   if (itMessages != messagesWidgets.end())
   {
        ChatMessage * auxMessage = itMessages->second;
        int row = ui->mMessageList->row(auxMessage->getWidgetItem());
        QListWidgetItem * auxItem = ui->mMessageList->takeItem(row);
        messagesWidgets.erase(itMessages);
        delete auxItem;
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
        chatItemWidget->setOlderMessageLoaded(msg->getMsgId());
        addMsgWidget(msg->copy(), (loadedMessages));
        loadedMessages+=1;
    }
}

void ChatWindow::onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg)
    {
        if (loadedMessages==0)
            {chatItemWidget->setOlderMessageLoaded(msg->getMsgId());}

        if(msg->isDeleted())
            return;

        if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING)
        {
            nSending+=1;
            addMsgWidget(msg->copy(), loadedMessages+nSending);

        }
        else
        {
            if(msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
            {
                nManualSending+=1;
                ChatMessage * auxMessage = this->findChatMessage(msg->getTempId());
                if (auxMessage)
                {
                    this->eraseChatMessage(msg, true);
                    nSending-=1;
                }

                addMsgWidget(msg,loadedMessages+nSending+nManualSending);
                auxMessage = this->findChatMessage(msg->getTempId());

                if(auxMessage)
                {
                    auxMessage->setMessage(msg->copy());
                    auxMessage->setManualMode(true);
                }
            }
            else
            {
                addMsgWidget(msg->copy(), -loadedMessages);
                loadedMessages+=1;
            }
        }
    }
    else
    {
        int pendingLoad;
        pendingLoad=NMESSAGES_LOAD-loadedMessages+nSending+nManualSending;
        if (pendingLoad>0)
        {
            int source = megaChatApi->loadMessages(chatRoom->getChatId(),pendingLoad);
            if (source == megachat::MegaChatApi::SOURCE_NONE)
            {
                pendingLoad = 0;
            }
            else if (source == megachat::MegaChatApi::SOURCE_ERROR)
            {
                logger->postLog("MegachatApi error - Load messages - source error");
            }
        }
    }
}


QListWidgetItem* ChatWindow::addMsgWidget (megachat::MegaChatMessage * msg, int index)
{
    QListWidgetItem* item = new QListWidgetItem;
    megachat::MegaChatHandle chatId = chatRoom->getChatId();
    ChatMessage * widget = new ChatMessage(this, megaChatApi, chatId, msg);
    widget->setWidgetItem(item);
    item->setSizeHint(widget->size());

    if (msg->getStatus()== megachat::MegaChatMessage::STATUS_DELIVERED || msg->getStatus() == megachat::MegaChatMessage::STATUS_SERVER_RECEIVED)
    {
        messagesWidgets.insert(std::pair<megachat::MegaChatHandle, ChatMessage *>(msg->getMsgId(),widget));        
    }
    else
    {
        messagesWidgets.insert(std::pair<megachat::MegaChatHandle, ChatMessage *>(msg->getTempId(),widget));
    }

    ui->mMessageList->insertItem(index, item);
    ui->mMessageList->setItemWidget(item, widget);
    ui->mMessageList->scrollToBottom();

    if (!widget->isMine() && msg->getStatus()==megachat::MegaChatMessage::STATUS_NOT_SEEN)
        megaChatApi->setMessageSeen(chatRoom->getChatId(), msg->getMsgId());

    return item;
}


void ChatWindow::onMembersBtn(bool)
{
    if(chatRoom->isGroup())
    {
        QMenu menu(this);
        createMembersMenu(menu);
        menu.setLayoutDirection(Qt::RightToLeft);
        menu.adjustSize();
        menu.exec(ui->mMembersBtn->mapToGlobal(
            QPoint(-menu.width()+ui->mMembersBtn->width(), ui->mMembersBtn->height())));
        menu.deleteLater();
    }
}

void ChatWindow::createMembersMenu(QMenu& menu)
{
    MainWindow * mainWin = chatItemWidget->mainWin;
    mega::MegaUserList *userList = mainWin->getUserContactList();
    if (chatRoom->getPeerCount()==0)
    {
        menu.addAction(tr("You are alone in this chatroom"))->setEnabled(false);
        return;
    }

    if(chatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR)
    {
        auto addEntry = menu.addMenu("Add contact to chat");
        for (int i=0 ; i< userList->size(); i++)
        {
             auto actAdd = addEntry->addAction(tr(userList->get(i)->getEmail()));
             actAdd->setProperty("userHandle", QVariant((qulonglong)userList->get(i)->getHandle()));
             connect(actAdd, SIGNAL(triggered()), this, SLOT(onMemberAdd()));
        }
        delete userList;
    }
    else
    {
        auto addEntry = menu.addMenu("Invalid permissions to add participants");
    }

    for (int i=0; i<chatRoom->getPeerCount(); i++)
    {
        if(chatRoom->getOwnPrivilege()== megachat::MegaChatRoom::PRIV_MODERATOR)
        {
            auto entry = menu.addMenu(chatRoom->getPeerFirstname(i));
            auto actRemove = entry->addAction(tr("Remove from chat"));
            actRemove->setProperty("userHandle", QVariant((qulonglong)chatRoom->getPeerHandle(i)));
            connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
            auto menuSetPriv = entry->addMenu(tr("Set privilege"));
            auto actSetPrivFullAccess = menuSetPriv->addAction(tr("Moderator"));
            actSetPrivFullAccess->setProperty("userHandle", QVariant((qulonglong)chatRoom->getPeerHandle(i)));
            connect(actSetPrivFullAccess, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
            auto actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only"));
            actSetPrivReadOnly->setProperty("userHandle", QVariant((qulonglong)chatRoom->getPeerHandle(i)));
            connect(actSetPrivReadOnly, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
            auto actSetPrivStandard = menuSetPriv->addAction(tr("Standard"));
            actSetPrivStandard->setProperty("userHandle", QVariant((qulonglong)chatRoom->getPeerHandle(i)));
            connect(actSetPrivStandard, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
        }
    }
}

void ChatWindow::onMemberAdd()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    megachat::MegaChatHandle userhand = uHandle.toLongLong();
    megaChatApi->inviteToChat(chatRoom->getChatId(),userhand,megachat::MegaChatPeerList::PRIV_STANDARD);
}

void ChatWindow::onMemberRemove()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    megachat::MegaChatHandle userhand = uHandle.toLongLong();
    megaChatApi->removeFromChat(chatRoom->getChatId(), userhand);
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

      QVariant uHandle = action->property("userHandle");
      megachat::MegaChatHandle userhand = uHandle.toLongLong();
      megachat::MegaChatHandle chatId=chatItemWidget->getChatHandle();
      this->megaChatApi->updateChatPermissions(chatId, userhand, privilege);
}

void ChatWindow::onMsgListRequestHistory()
{
    megaChatApi->loadMessages(chatRoom->getChatId(),NMESSAGES_LOAD);
}
