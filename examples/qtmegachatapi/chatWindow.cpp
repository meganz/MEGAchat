#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>

ChatWindow::ChatWindow(QWidget* parent, megachat::MegaChatApi* megaChatApi, megachat::MegaChatRoom *cRoom, const char * title)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{
    nSending = 0;
#ifndef KARERE_DISABLE_WEBRTC
    mCallGui = NULL;
#endif
    loadedMessages = 0;
    nManualSending = 0;
    mPendingLoad = 0;
    mChatRoom = cRoom;
    mMegaChatApi = megaChatApi;
    mMainWin = (MainWindow *) parent;
    mMegaApi = mMainWin->mMegaApi;
    mLogger = mMainWin->mLogger;
    ui->setupUi(this);
    ui->mSplitter->setStretchFactor(0,1);
    ui->mSplitter->setStretchFactor(1,0);
    ui->mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->mChatdStatusDisplay->hide();
    setChatTittle(title);
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

    if (!mChatRoom->isGroup())
        ui->mMembersBtn->hide();
    else
        setAcceptDrops(true);

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < callMaxParticipants; i ++)
    {
        mFreeCallGui[i] = megachat::MEGACHAT_INVALID_HANDLE;
    }

    if (!mChatRoom->isActive())
    {
        ui->mMessageEdit->setEnabled(false);
        ui->mMessageEdit->blockSignals(true);
        ui->mMsgSendBtn->setEnabled(false);
        ui->mMembersBtn->hide();
    }

    QDialog::show();
    megaChatRoomListenerDelegate = new ::megachat::QTMegaChatRoomListener(megaChatApi, this);
}

void ChatWindow::updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname)
{
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator it;
    for (it = mMsgsWidgetsMap.begin(); it != mMsgsWidgetsMap.end(); it++)
    {
        ChatMessage *chatMessage = it->second;
        if (chatMessage->mMessage->getUserHandle() == contactHandle)
        {
            chatMessage->setAuthor(firstname);
        }
    }
}



void ChatWindow::setChatTittle(const char *title)
{
    if (title)
    {
        mChatTitle = title;
    }

    QString chatTitle = NULL;
    chatTitle.append(mChatTitle.c_str())
    .append(" [")
    .append(mChatRoom->privToString(mChatRoom->getOwnPrivilege()))
    .append("]");
    ui->mTitleLabel->setText(chatTitle);
}

void ChatWindow::openChatRoom()
{
    bool result = this->mMegaChatApi->openChatRoom(mChatRoom->getChatId(),megaChatRoomListenerDelegate);
    if(result)
    {
        mMegaChatApi->loadMessages(mChatRoom->getChatId(),NMESSAGES_LOAD);
    }
}

ChatWindow::~ChatWindow()
{
    ChatItemWidget *chatItemWidget = mMainWin->getChatItemWidget(mChatRoom->getChatId(), false);
    if (chatItemWidget)
    {
        chatItemWidget->invalidChatWindowHandle();
    }
    mMegaChatApi->closeChatRoom(mChatRoom->getChatId(),megaChatRoomListenerDelegate);
    delete megaChatRoomListenerDelegate;
    delete mChatRoom;
    delete ui;
}

void ChatWindow::onMsgSendBtn()
{
    QString qtext = ui->mMessageEdit->toPlainText();
    ui->mMessageEdit->setText(QString());
    if (qtext.isEmpty())
        return;

    std::string msgStd = qtext.toUtf8().toStdString();
    megachat::MegaChatMessage *tempMessage= mMegaChatApi->sendMessage(mChatRoom->getChatId(), msgStd.c_str());

    if(tempMessage)
    {
        nSending++;
        addMsgWidget(tempMessage, loadedMessages + nSending);
    }
    else
    {
        QMessageBox::warning(nullptr, tr("Send message"), tr("The maximun length has been exceeded"));
    }
}

void ChatWindow::moveManualSendingToSending(megachat::MegaChatMessage * msg)
{
    nSending++;
    nManualSending--;
    addMsgWidget(msg, loadedMessages + nSending);
}

void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat)
{
    if (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_CLOSED))
    {
        ui->mMessageEdit->setEnabled(false);
        ui->mMessageEdit->blockSignals(true);
        ui->mMsgSendBtn->setEnabled(false);
        ui->mMembersBtn->hide();
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_TITLE))
    {
       delete mChatRoom;
       this->mChatRoom = chat->copy();
       this->setChatTittle(mChatRoom->getTitle());
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_PARTICIPANTS))
    {
        delete mChatRoom;
        this->mChatRoom = chat->copy();
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_OWN_PRIV))
    {
        setChatTittle(NULL);
    }
}

void ChatWindow::onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg->isDeleted())
    {
        eraseChatMessage(msg, false);
        return;
    }

    megachat::MegaChatHandle msgId = getMessageId(msg);
    ChatMessage *chatMessage = findChatMessage(msgId);

    if (msg->hasChanged(megachat::MegaChatMessage::CHANGE_TYPE_CONTENT))
    {
        if (chatMessage)
        {
            if (msg->getType() == megachat::MegaChatMessage::TYPE_TRUNCATE)
            {
                truncateChatUI();
                megachat::MegaChatMessage *auxMsg = msg->copy();
                addMsgWidget(auxMsg, loadedMessages);

                if(msg->getUserHandle() != mMegaChatApi->getMyUserHandle())
                {
                    mMegaChatApi->setMessageSeen(mChatRoom->getChatId(), msg->getMsgId());
                }
            }
            else
            {
                // TODO: reuse the ChatMessage::updateContent() from rich-links branch
                // to update the content accordingly, like it's now done in the ctor.
               chatMessage->setMessageContent(msg->getContent());
               if (msg->isEdited())
               {
                  chatMessage->markAsEdited();
               }
            }
        }
    }

    if (msg->hasChanged(megachat::MegaChatMessage::CHANGE_TYPE_STATUS))
    {
        if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SERVER_RECEIVED)
        {
            ChatMessage *auxMessage = findChatMessage(msg->getTempId());
            if (auxMessage)
            {
                eraseChatMessage(auxMessage->getMessage(), true);
                nSending--;
            }
            megachat::MegaChatMessage *auxMsg = msg->copy();
            addMsgWidget(auxMsg, loadedMessages);
            loadedMessages++;
        }
        else
        {
            if(msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
            {
                // TODO: Complete case
            }
            else
            {
                megachat::MegaChatHandle msgId = getMessageId(msg);
                ChatMessage *chatMessage = findChatMessage(msgId);
                if (chatMessage)
                    chatMessage->setStatus(msg->getStatus());
            }
        }
     }
}

void ChatWindow::deleteChatMessage(megachat::MegaChatMessage *msg)
{
    megachat::MegaChatHandle msgId = getMessageId(msg);
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = mMsgsWidgetsMap.find(msgId);
    if (itMessages != mMsgsWidgetsMap.end())
    {
        mMegaChatApi->deleteMessage(mChatRoom->getChatId(), msgId);
    }
}

void ChatWindow::truncateChatUI()
{
    std::map<megachat::MegaChatHandle, ChatMessage*>::iterator itMessages;
    for (itMessages = mMsgsWidgetsMap.begin(); itMessages != mMsgsWidgetsMap.end(); itMessages++)
    {
        ChatMessage *auxMessage = itMessages->second;
        int row = ui->mMessageList->row(auxMessage->getWidgetItem());
        QListWidgetItem *auxItem = ui->mMessageList->takeItem(row);
        mMsgsWidgetsMap.erase(itMessages);
        delete auxItem;
    }
}

bool ChatWindow::eraseChatMessage(megachat::MegaChatMessage *msg, bool temporal)
{
    megachat::MegaChatHandle msgId = getMessageId(msg);
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = mMsgsWidgetsMap.find(msgId);
    if (itMessages != mMsgsWidgetsMap.end())
    {
        ChatMessage * auxMessage = itMessages->second;
        int row = ui->mMessageList->row(auxMessage->getWidgetItem());
        QListWidgetItem * auxItem = ui->mMessageList->takeItem(row);
        mMsgsWidgetsMap.erase(itMessages);
        delete auxItem;
        return true;
    }
    return false;
}

ChatMessage * ChatWindow::findChatMessage(megachat::MegaChatHandle msgId)
{
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = mMsgsWidgetsMap.find(msgId);
    if (itMessages != mMsgsWidgetsMap.end())
    {
        return itMessages->second;
    }
    return NULL;
}

megachat::MegaChatHandle ChatWindow::getMessageId(megachat::MegaChatMessage *msg)
{
    if (msg->getStatus()== megachat::MegaChatMessage::STATUS_SENDING)
    {
        return msg->getTempId();
    }
    else if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
    {
        return msg->getRowId();
    }
    else
    {
        return msg->getMsgId();
    }

    return megachat::MEGACHAT_INVALID_HANDLE;
}

#ifndef KARERE_DISABLE_WEBRTC
std::set<CallGui *> *ChatWindow::getCallGui()
{
    return &callParticipantsGui;
}

void ChatWindow::setCallGui(CallGui *callGui)
{
    mCallGui = callGui;
}
#endif

void ChatWindow::onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    addMsgWidget(msg->copy(), loadedMessages);
    loadedMessages++;
}

void ChatWindow::onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg)
    {
        if(msg->isDeleted())
            return;

        if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING)
        {
            nSending++;
            addMsgWidget(msg->copy(), loadedMessages + nSending);
        }
        else
        {
            if(msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
            {
                nManualSending++;
                ChatMessage *auxMessage = findChatMessage(msg->getTempId());
                if (auxMessage)
                {
                    eraseChatMessage(auxMessage->getMessage(), true);
                    nSending--;
                }

                addMsgWidget(msg, loadedMessages + nSending + nManualSending);
                auxMessage = findChatMessage(msg->getRowId());
                if(auxMessage)
                {
                    auxMessage->setMessage(msg->copy());
                    auxMessage->setManualMode(true);
                }
            }
            else
            {
                addMsgWidget(msg->copy(), -loadedMessages);
                loadedMessages++;
            }
        }
    }
    else
    {
        if (!mMegaChatApi->isFullHistoryLoaded(mChatRoom->getChatId()) && mPendingLoad > 0)
        {
            mPendingLoad = NMESSAGES_LOAD - loadedMessages + nSending + nManualSending;
            int source = mMegaChatApi->loadMessages(mChatRoom->getChatId(),mPendingLoad);
            if (source == megachat::MegaChatApi::SOURCE_NONE)
            {
                mPendingLoad = 0;
            }
            else if (source == megachat::MegaChatApi::SOURCE_ERROR)
            {
                mPendingLoad = 0;
                mLogger->postLog("MegachatApi error - Load messages - source error");
            }
        }
    }
}

void ChatWindow::onHistoryReloaded(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat)
{
    truncateChatUI();
}


void ChatWindow::setMessageHeight(megachat::MegaChatMessage * msg, QListWidgetItem* item)
{
    switch (msg->getType())
    {
        case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
        item->setSizeHint(QSize(item->sizeHint().height(), 150));
        break;
        }

        case megachat::MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
        {
        item->setSizeHint(QSize(item->sizeHint().height(), 150));
        break;
        }
    }
}

QListWidgetItem* ChatWindow::addMsgWidget(megachat::MegaChatMessage * msg, int index)
{
    QListWidgetItem* item = new QListWidgetItem;
    megachat::MegaChatHandle chatId = mChatRoom->getChatId();
    ChatMessage * widget = new ChatMessage(this, mMegaChatApi, chatId, msg);
    widget->setWidgetItem(item);
    item->setSizeHint(widget->size());
    setMessageHeight(msg,item);

    megachat::MegaChatHandle msgId = getMessageId(msg);
    mMsgsWidgetsMap.insert(std::pair<megachat::MegaChatHandle, ChatMessage *>(msgId, widget));

    ui->mMessageList->insertItem(index, item);
    ui->mMessageList->setItemWidget(item, widget);
    ui->mMessageList->scrollToBottom();

    if (!widget->isMine() && msg->getStatus() == megachat::MegaChatMessage::STATUS_NOT_SEEN)
        mMegaChatApi->setMessageSeen(mChatRoom->getChatId(), msg->getMsgId());

    return item;
}

void ChatWindow::onMembersBtn(bool)
{
    if(mChatRoom->isGroup())
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
    if (!mChatRoom->isActive())
    {
        return ;
    }

    auto truncate = menu.addAction("Truncate chat");
    truncate->setEnabled(mChatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);
    connect(truncate, SIGNAL(triggered()), this, SLOT(onTruncateChat()));

    auto addEntry = menu.addMenu("Add contact to chat");
    addEntry->setEnabled(mChatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);
    mega::MegaUserList *userList = mMegaApi->getContacts();
    for (int i = 0 ; i < userList->size(); i++)
    {
         auto actAdd = addEntry->addAction(tr(userList->get(i)->getEmail()));
         actAdd->setEnabled(mChatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);
         actAdd->setProperty("userHandle", QVariant((qulonglong)userList->get(i)->getHandle()));
         connect(actAdd, SIGNAL(triggered()), this, SLOT(onMemberAdd()));
    }
    delete userList;

    // list of peers with presence and privilege-related actions
    if (mChatRoom->getPeerCount() != 0)
    {
        for (unsigned int i = 0; i < mChatRoom->getPeerCount() + 1; i++)
        {
            QVariant userhandle;
            QString title;
            int privilege;
            if(i == mChatRoom->getPeerCount())    // my own user
            {
                privilege = mChatRoom->getOwnPrivilege();
                userhandle = mMegaApi->getMyUserHandle();
                title.append(" Me [")
                    .append(QString::fromStdString(mChatRoom->statusToString(mMegaChatApi->getOnlineStatus())))
                    .append("]");
            }
            else
            {
                const char *memberName = mChatRoom->getPeerFirstname(i);
                if (!memberName)
                {
                    memberName = mChatRoom->getPeerEmail(i);
                }
                else
                {
                   if ((strlen(memberName)) == 0)
                   {
                       memberName = mChatRoom->getPeerEmail(i);
                   }
                }
                privilege = mChatRoom->getPeerPrivilege(i);
                userhandle = QVariant((qulonglong)mChatRoom->getPeerHandle(i));
                title.append(" ")
                    .append(QString::fromStdString(memberName))
                    .append(" [")
                    .append(QString::fromStdString(mChatRoom->statusToString(mMegaChatApi->getUserOnlineStatus(mChatRoom->getPeerHandle(i)))))
                    .append("]");
            }

            auto entry = menu.addMenu(title);

            bool canChangePrivs = (i != mChatRoom->getPeerCount())
                    && (mChatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);

            if(i == mChatRoom->getPeerCount())  // my own user
            {
                auto actRemove = entry->addAction(tr("Leave chat"));
                actRemove->setProperty("userHandle", userhandle);
                connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
            }
            else
            {
                auto actRemove = entry->addAction(tr("Remove from chat"));
                actRemove->setProperty("userHandle", userhandle);
                actRemove->setEnabled(canChangePrivs);
                connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
            }

            auto menuSetPriv = entry->addMenu(tr("Set privilege"));
            menuSetPriv->setEnabled(canChangePrivs);

            QAction *actSetPrivFullAccess = Q_NULLPTR;
            QAction *actSetPrivStandard = Q_NULLPTR;
            QAction *actSetPrivReadOnly = Q_NULLPTR;

            switch (privilege)
            {
            case megachat::MegaChatRoom::PRIV_MODERATOR:
                actSetPrivFullAccess = menuSetPriv->addAction(tr("Moderator <-"));
                actSetPrivStandard = menuSetPriv->addAction(tr("Standard"));
                actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only"));
                break;
            case megachat::MegaChatRoom::PRIV_STANDARD:
                actSetPrivFullAccess = menuSetPriv->addAction(tr("Moderator"));
                actSetPrivStandard = menuSetPriv->addAction(tr("Standard <-"));
                actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only"));
                break;
            case megachat::MegaChatRoom::PRIV_RO:
                actSetPrivFullAccess = menuSetPriv->addAction(tr("Moderator"));
                actSetPrivStandard = menuSetPriv->addAction(tr("Standard"));
                actSetPrivReadOnly = menuSetPriv->addAction(tr("Read-only <-"));
                break;
            }


            actSetPrivFullAccess->setProperty("userHandle", userhandle);
            actSetPrivFullAccess->setEnabled(canChangePrivs);
            connect(actSetPrivFullAccess, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

            actSetPrivStandard->setProperty("userHandle", userhandle);
            actSetPrivStandard->setEnabled(canChangePrivs);
            connect(actSetPrivStandard, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

            actSetPrivReadOnly->setProperty("userHandle", userhandle);
            actSetPrivReadOnly->setEnabled(canChangePrivs);
            connect(actSetPrivReadOnly, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
        }
    }
}


void ChatWindow::onTruncateChat()
{
    this->mMegaChatApi->clearChatHistory(mChatRoom->getChatId());
}

void ChatWindow::onMemberAdd()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    megachat::MegaChatHandle userhandle = uHandle.toLongLong();
    mMegaChatApi->inviteToChat(mChatRoom->getChatId(), userhandle, megachat::MegaChatPeerList::PRIV_STANDARD);
}

void ChatWindow::onMemberRemove()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    megachat::MegaChatHandle userhandle = uHandle.toLongLong();
    if (uHandle.toString() == mMegaApi->getMyUserHandle())
    {
        mMegaChatApi->leaveChat(mChatRoom->getChatId());
    }
    else
    {
        mMegaChatApi->removeFromChat(mChatRoom->getChatId(), userhandle);
    }
}

void ChatWindow::onMemberSetPriv()
{
      int privilege;
      QAction *action = qobject_cast<QAction *>(sender());
      if (!action)
         return;

      if(action->text() == "Read-only")
            privilege = megachat::MegaChatPeerList::PRIV_RO;

      if(action->text() == "Moderator")
            privilege = megachat::MegaChatPeerList::PRIV_MODERATOR;

      if(action->text() == "Standard")
            privilege = megachat::MegaChatPeerList::PRIV_STANDARD;

      QVariant uHandle = action->property("userHandle");
      megachat::MegaChatHandle userhandle = uHandle.toLongLong();
      megachat::MegaChatHandle chatId = mChatRoom->getChatId();
      this->mMegaChatApi->updateChatPermissions(chatId, userhandle, privilege);}

void ChatWindow::onMsgListRequestHistory()
{
    if (!mMegaChatApi->isFullHistoryLoaded(mChatRoom->getChatId()))
    {
        mMegaChatApi->loadMessages(mChatRoom->getChatId(), NMESSAGES_LOAD);
    }
}

#ifndef KARERE_DISABLE_WEBRTC
void ChatWindow::onVideoCallBtn(bool)
{
    onCallBtn(true);
}

void ChatWindow::onAudioCallBtn(bool)
{
    onCallBtn(false);
}

void ChatWindow::createCallGui(bool video, MegaChatHandle peerid)
{
    int row = 0;
    int col = 0;
    int auxIndex = 0;
    CallGui *callGui = NULL;
    auto layout = qobject_cast <QGridLayout*> (ui->mCentralWidget->layout());

    //Local callGui
    callGui = new CallGui(this, video, peerid, true);
    callParticipantsGui.insert(callGui);


    if (peerid == megachat::MEGACHAT_INVALID_HANDLE)
    {
        ui->mCentralWidget->setStyleSheet("background-color:#000000");
        auxIndex = -1;
    }
    else
    {
        for (int i = 0; i < callMaxParticipants; i++)
        {
            if (mFreeCallGui[i] == megachat::MEGACHAT_INVALID_HANDLE)
            {
               mFreeCallGui[i] = peerid;
               auxIndex = i;
               break;
            }
        }
    }
    callGui->setIndex(auxIndex);
    getCallPos(auxIndex, row, col);
    layout->addWidget(callGui,row,col);
    ui->mTitlebar->hide();
    ui->mTextChatWidget->hide();
}

void ChatWindow::destroyCallGui(MegaChatHandle mPeerid)
{
    int row = 0;
    int col = 0;
    int auxIndex = 0;
    std::set<CallGui *>::iterator it;
    auto layout = qobject_cast<QGridLayout*>(ui->mCentralWidget->layout());
    for (it = callParticipantsGui.begin(); it != callParticipantsGui.end(); ++it)
    {
        CallGui *call = *it;
        if (call->getPeer() == mPeerid)
        {
            auxIndex = call->getIndex();
            getCallPos(auxIndex, row, col);
            layout->removeWidget(layout->itemAtPosition(row, col)->widget());
            call->deleteLater();
            callParticipantsGui.erase(it);
            if (auxIndex != -1)
            {
                mFreeCallGui[auxIndex] = megachat::MEGACHAT_INVALID_HANDLE;
            }
            break;
        }
    }
}



void ChatWindow::deleteCallGui()
{
    int row = 0;
    int col = 0;
    int auxIndex = 0;
    ui->mCentralWidget->setStyleSheet("background-color: #FFFFFF");
    std::set<CallGui *>::iterator it;
    auto layout = qobject_cast<QGridLayout*>(ui->mCentralWidget->layout());
    for (it = callParticipantsGui.begin(); it != callParticipantsGui.end(); ++it)
    {
        CallGui *call = *it;
        auxIndex = call->getIndex();
        getCallPos(auxIndex, row, col);
        layout->removeWidget(layout->itemAtPosition(row, col)->widget());
        call->deleteLater();
        if (auxIndex != -1)
        {
            mFreeCallGui[auxIndex] = megachat::MEGACHAT_INVALID_HANDLE;
        }
    }
    callParticipantsGui.clear();
    setChatTittle(mChatRoom->getTitle());
    ui->mTextChatWidget->show();
    ui->mTitlebar->show();
    ui->mTitlebar->setStyleSheet("background-color: #C1EFFF");
    ui->mTextChatWidget->setStyleSheet("background-color: #FFFFFF");
    ui->mCentralWidget->setStyleSheet("background-color: #FFFFFF");
}

void ChatWindow::getCallPos(int index, int &row, int &col)
{
    switch (index)
    {
        case -1:
            row = 0;
            col = 0;
            break;
        case 0:
            row = 0;
            col = 1;
            break;
        case 1:
            row = 1;
            col = 0;
            break;
        case 2:
            row = 1;
            col = 1;
            break;
        case 3:
            row = 2;
            col = 0;
            break;
        case 4:
            row = 2;
            col = 1;
            break;
        case 5:
            row = 3;
            col = 0;
            break;
        case 6:
            row = 3;
            col = 1;
            break;
        case 7:
            row = 4;
            col = 0;
            break;
        case 8:
            row = 4;
            col = 1;
            break;
    }
    row += widgetsFixed;
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    if (mCallGui)
    {
        mCallGui->onHangCall(true);
    }
    delete this;
    event->accept();
}

void ChatWindow::onCallBtn(bool video)
{
   createCallGui(video, megachat::MEGACHAT_INVALID_HANDLE);
   MegaChatCall *auxCall = mMegaChatApi->getChatCall(mChatRoom->getChatId());
   if(mMegaChatApi->getChatCall(mChatRoom->getChatId()) == NULL)
   {
       mMegaChatApi->startChatCall(this->mChatRoom->getChatId(), video);
   }
   else
   {
       connectPeerCallGui(megachat::MEGACHAT_INVALID_HANDLE);
   }
}

void ChatWindow::connectPeerCallGui(MegaChatHandle peerid)
{
    std::set<CallGui *>::iterator it;
    for (it = callParticipantsGui.begin(); it != callParticipantsGui.end(); ++it)
    {
        CallGui *call = *it;
        if (call->getPeer() == peerid)
        {
            if (!call->getCall())
            {
                call->connectPeerCallGui();
                break;
            }
        }
    }
}

void ChatWindow::hangCall()
{
    deleteCallGui();
}
#endif
