#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>
#include <QFileDialog>

ChatWindow::ChatWindow(QWidget* parent, megachat::MegaChatApi* megaChatApi, megachat::MegaChatRoom *cRoom, const char * title)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{
    nSending = 0;
#ifndef KARERE_DISABLE_WEBRTC
    mCallGui = NULL;
#endif
    mPreview = false;
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
    mUploadDlg = NULL;
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

    if (mChatRoom->isPublic())
    {
        updatePreviewers(mChatRoom->getNumPreviewers());
        if(mChatRoom->isPreview())
        {
            mPreview = true;
            this->ui->mTitlebar->setStyleSheet("background-color:#ffe4af");
        }
        else
        {
            this->ui->mTitlebar->setStyleSheet("background-color:#c4f2c9");
        }
    }

    if (!mChatRoom->isGroup())
        ui->mMembersBtn->hide();
    else
        setAcceptDrops(true);

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    if (!mChatRoom->isActive())
    {
        ui->mMessageEdit->setEnabled(false);
        ui->mMessageEdit->blockSignals(true);
        ui->mMsgSendBtn->setEnabled(false);
        ui->mMembersBtn->hide();
    }

    QDialog::show();
    megaChatRoomListenerDelegate = new ::megachat::QTMegaChatRoomListener(megaChatApi, this);
    megaTransferListenerDelegate = new mega::QTMegaTransferListener(mMegaApi, this);
    mMegaApi->addTransferListener(megaTransferListenerDelegate);
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

    if(mChatRoom->isPreview())
    {
        chatTitle.append("        <PREVIEW>");
    }
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
    ChatItemWidget *item = mMainWin->getChatItemWidget(mChatRoom->getChatId(), false);
    if (item)
    {
        item->invalidChatWindowHandle();
    }

    mMegaChatApi->closeChatRoom(mChatRoom->getChatId(),megaChatRoomListenerDelegate);
    mMegaApi->removeTransferListener(megaTransferListenerDelegate);
    delete megaChatRoomListenerDelegate;
    delete megaTransferListenerDelegate;
    delete mChatRoom;
    delete mUploadDlg;
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

void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi *, megachat::MegaChatRoom *chat)
{
    delete mChatRoom;
    this->mChatRoom = chat->copy();

    if (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_CLOSED))
    {
        ui->mMessageEdit->setEnabled(false);
        ui->mMessageEdit->blockSignals(true);
        ui->mMsgSendBtn->setEnabled(false);
        ui->mMembersBtn->hide();
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_TITLE))
    {
       this->setChatTittle(mChatRoom->getTitle());
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_OWN_PRIV))
    {
        setChatTittle(NULL);
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_PARTICIPANTS)
                || chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_ARCHIVE))
    {
        delete mChatRoom;
        this->mChatRoom = chat->copy();
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_CHAT_MODE))
    {
        this->ui->mTitlebar->setStyleSheet("background-color:#c1efff");
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_UPDATE_PREVIEWERS))
    {
        updatePreviewers(chat->getNumPreviewers());
    }
}

void ChatWindow::updatePreviewers(unsigned int numPrev)
{
    if (numPrev == 0)
    {
        ui->mPreviewers->setToolTip("");
        ui->mPreviewers->hide();
    }
    else
    {
        ui->mPreviewers->setText(QString::number(numPrev));
        ui->mPreviewers->show();
        QString text = NULL;
        text.append("Number of previewers: ")
        .append(std::to_string(numPrev).c_str());
        ui->mPreviewers->setToolTip(text);
    }
}

void ChatWindow::onMessageUpdate(megachat::MegaChatApi *, megachat::MegaChatMessage *msg)
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

                if(msg->getUserHandle() != mMegaChatApi->getMyUserHandle() && !mChatRoom->isPreview())
                {
                    mMegaChatApi->setMessageSeen(mChatRoom->getChatId(), msg->getMsgId());
                }
            }
            else
            {
                chatMessage->setMessage(msg->copy());
                chatMessage->updateContent();

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

bool ChatWindow::eraseChatMessage(megachat::MegaChatMessage *msg, bool /*temporal*/)
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
CallGui *ChatWindow::getCallGui() const
{
    return mCallGui;
}

void ChatWindow::setCallGui(CallGui *callGui)
{
    mCallGui = callGui;
}
#endif

void ChatWindow::onMessageReceived(megachat::MegaChatApi*, megachat::MegaChatMessage *msg)
{
    addMsgWidget(msg->copy(), loadedMessages);
    loadedMessages++;
}

void ChatWindow::onMessageLoaded(megachat::MegaChatApi*, megachat::MegaChatMessage *msg)
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

                addMsgWidget(msg->copy(), loadedMessages + nSending + nManualSending);
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

void ChatWindow::onHistoryReloaded(megachat::MegaChatApi *, megachat::MegaChatRoom *)
{
    truncateChatUI();
}

void ChatWindow::setMessageHeight(megachat::MegaChatMessage *msg, QListWidgetItem *item)
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

QListWidgetItem* ChatWindow::addMsgWidget(megachat::MegaChatMessage *msg, int index)
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

    if (!widget->isMine() && msg->getStatus() == megachat::MegaChatMessage::STATUS_NOT_SEEN
            && !mChatRoom->isPreview())
    {
        mMegaChatApi->setMessageSeen(mChatRoom->getChatId(), msg->getMsgId());
    }

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
    //Add contacts
    mega::MegaUserList *userList = mMegaApi->getContacts();

    auto addEntry = menu.addMenu("Add contact to chat");
    for (int i = 0 ; i < userList->size(); i++)
    {
         mega::MegaUser *user = userList->get(i);
         auto actAdd = addEntry->addAction(tr(userList->get(i)->getEmail()));
         actAdd->setProperty("userHandle", QVariant((qulonglong)user->getHandle()));
         connect(actAdd, SIGNAL(triggered()), this, SLOT(onMemberAdd()));
    }
    delete userList;

    // list of peers with presence and privilege-related actions
    for (unsigned int i = 0; i < mChatRoom->getPeerCount() + 1; i++)
    {
        QVariant userhandle;
        QString title;
        int privilege;
        if(i == mChatRoom->getPeerCount())
        {
            //If chat is in preview mode our own user is not member
            if (mChatRoom->isPreview())
            {
                continue;
            }

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

        if (i == mChatRoom->getPeerCount())  // my own user
        {
            auto actRemove = entry->addAction(tr("Leave chat"));
            actRemove->setProperty("userHandle", userhandle);
            connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
        }
        else
        {
            auto actRemove = entry->addAction(tr("Remove from chat"));
            actRemove->setProperty("userHandle", userhandle);
            connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
        }

        QAction *actSetPrivFullAccess = Q_NULLPTR;
        QAction *actSetPrivStandard = Q_NULLPTR;
        QAction *actSetPrivReadOnly = Q_NULLPTR;

        auto menuSetPriv = entry->addMenu(tr("Set privilege"));
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
        connect(actSetPrivFullAccess, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

        actSetPrivStandard->setProperty("userHandle", userhandle);
        connect(actSetPrivStandard, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));

        actSetPrivReadOnly->setProperty("userHandle", userhandle);
        connect(actSetPrivReadOnly, SIGNAL(triggered()), this, SLOT(onMemberSetPriv()));
    }
}

void ChatWindow::createSettingsMenu(QMenu& menu)
{
    //Leave
    auto leave = menu.addAction("Leave chat");
    connect(leave, SIGNAL(triggered()), this, SLOT(onLeaveGroupChat()));

    //Truncate
    auto truncate = menu.addAction("Truncate chat");
    connect(truncate, SIGNAL(triggered()), this, SLOT(onTruncateChat()));

    //Set topic
    auto title = menu.addAction("Set title");
    connect(title, SIGNAL(triggered()), this, SLOT(onChangeTitle()));

    auto actArchive = menu.addAction("Archive chat");
    connect(actArchive, SIGNAL(toggled(bool)), this, SLOT(onArchiveClicked(bool)));
    actArchive->setCheckable(true);
    actArchive->setChecked(mChatRoom->isArchived());

    QMenu *clMenu = menu.addMenu("Chat links");

    //Query chat link
    auto queryChatLink = clMenu->addAction("Query chat link");
    connect(queryChatLink, SIGNAL(triggered()), this, SLOT(onQueryChatLink()));

    //Export chat link
    auto exportChatLink = clMenu->addAction("Export chat link");
    connect(exportChatLink, SIGNAL(triggered()), this, SLOT(onExportChatLink()));

    //Remove chat link
    auto removeChatLink = clMenu->addAction("Remove chat link");
    connect(removeChatLink, SIGNAL(triggered()), this, SLOT(onRemoveChatLink()));

    //Auto-join chat link
    auto joinChatLink = clMenu->addAction("Join chat link");
    connect(joinChatLink, SIGNAL(triggered()), this, SLOT(on_mJoinBtn_clicked()));

    //Close chat link
    auto closeChatLink = clMenu->addAction("Close chat link");
    connect(closeChatLink, SIGNAL(triggered()), this, SLOT(onCloseChatLink()));
}

void ChatWindow::onQueryChatLink()
{
    if (mChatRoom->getChatId() != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->queryChatLink(mChatRoom->getChatId());
    }
}

void ChatWindow::onExportChatLink()
{
    if (mChatRoom->getChatId() != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->exportChatLink(mChatRoom->getChatId());
    }
}
void ChatWindow::onRemoveChatLink()
{
    if (mChatRoom->getChatId() != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->removeChatLink(mChatRoom->getChatId());
    }
}

void ChatWindow::onCloseChatLink()
{
    if (mChatRoom->getChatId() != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->closeChatLink(mChatRoom->getChatId());
    }
}

void ChatWindow::onUnarchiveChat()
{
    mMegaChatApi->archiveChat(mChatRoom->getChatId(), false);
}

void ChatWindow::onArchiveChat()
{
    mMegaChatApi->archiveChat(mChatRoom->getChatId(), true);
}


void ChatWindow::onChangeTitle()
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
        this->mMegaChatApi->setChatTitle(mChatRoom->getChatId(), title.c_str());
    }
}

void ChatWindow::onLeaveGroupChat()
{
    this->mMegaChatApi->leaveChat(mChatRoom->getChatId());
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

void ChatWindow::createCallGui(bool video)
{
    auto layout = qobject_cast<QBoxLayout*>(ui->mCentralWidget->layout());
    mCallGui = new CallGui(this, video);
    layout->insertWidget(1, mCallGui, 1);
    ui->mTitlebar->hide();
    ui->mTextChatWidget->hide();
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
    if (mChatRoom->isGroup())
    {
        QMessageBox::critical(this, "Call", "Nice try, but group audio and video calls are not implemented yet");
        return;
    }
    createCallGui(video);
    mMegaChatApi->startChatCall(this->mChatRoom->getChatId(), video);
}

void ChatWindow::connectCall()
{
    mCallGui->connectCall();
}

void ChatWindow::hangCall()
{
    deleteCallGui();
}

void ChatWindow::deleteCallGui()
{
    if (mCallGui)
    {
        mCallGui->deleteLater();
        mCallGui = NULL;
    }
    ui->mTitlebar->show();
    ui->mTextChatWidget->show();
}
#endif

void ChatWindow::on_mJoinBtn_clicked()
{
    auto ret = QMessageBox::question(this, tr("Join chat link"), tr("Do you want to join to this chat?"));
    if (ret != QMessageBox::Yes)
        return;

    this->mMegaChatApi->joinChatLink(this->mChatRoom->getChatId());
}

void ChatWindow::on_mSettingsBtn_clicked()
{
    QMenu menu(this);
    createSettingsMenu(menu);
    menu.setLayoutDirection(Qt::RightToLeft);
    menu.adjustSize();
    menu.exec(ui->mSettingsBtn->mapToGlobal(
    QPoint(-menu.width()+ui->mSettingsBtn->width(), ui->mSettingsBtn->height())));
    menu.deleteLater();
}

void ChatWindow::on_mAttachBtn_clicked()
{
    QString node = QFileDialog::getOpenFileName(this, tr("All Files (*)"));

    if (node.isEmpty())
       return;

    QStringList nodeParsed = node.split( "/" );
    QString nodeName = nodeParsed.value(nodeParsed.length() - 1);
    mega::MegaNode *parent = mMegaApi->getNodeByPath("/");
    mUploadDlg = new QMessageBox;
    mUploadDlg->setWindowTitle((tr("Uploading file...")));
    mUploadDlg->setIcon(QMessageBox::Warning);
    mUploadDlg->setText(tr("Please, wait.\nThe file will be attached when the transfer completes."));
    mUploadDlg->setStandardButtons(QMessageBox::Cancel);
    mUploadDlg->setModal(true);
    mUploadDlg->show();
    connect(mUploadDlg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_mCancelTransfer(QAbstractButton*)));
    this->mMegaApi->startUpload(node.toStdString().c_str(), parent, nodeName.toStdString().c_str());
    delete parent;
}

void ChatWindow::on_mCancelTransfer(QAbstractButton*)
{
    mMegaApi->cancelTransfers(::mega::MegaTransfer::TYPE_UPLOAD);
    mUploadDlg->hide();
    delete mUploadDlg;
    mUploadDlg = NULL;
}

void ChatWindow::onArchiveClicked(bool checked)
{
    if (mChatRoom->isArchived() == checked)
        return;

    mMegaApi->archiveChat(mChatRoom->getChatId(), checked);
}

void ChatWindow::onTransferFinish(mega::MegaApi* api, mega::MegaTransfer *transfer, mega::MegaError* e)
{
    if (transfer->getType() == mega::MegaTransfer::TYPE_UPLOAD)
    {
        if (mUploadDlg)
        {
            mUploadDlg->hide();
            if (e->getErrorCode() == mega::MegaError::API_OK)
            {
                QMessageBox::information(nullptr, tr("Upload"), tr("Upload successful. Attaching node..."));
                mMegaChatApi->attachNode(mChatRoom->getChatId(), transfer->getNodeHandle());
            }
            else
            {
                QMessageBox::critical(nullptr, tr("Upload"), tr("Error in transfer: ").append(e->getErrorString()));
            }

            mUploadDlg->hide();
            delete mUploadDlg;
            mUploadDlg = NULL;
        }
        else if (mUploadDlg == NULL)
        {
            QMessageBox::information(nullptr, tr("Upload"), tr("Upload canceled."));
        }
    }
    else    // download
    {
        if (e->getErrorCode() == mega::MegaError::API_OK)
        {
            QMessageBox::information(nullptr, tr("Download"), tr("Attachment's download successful."));
        }
        else
        {
            QMessageBox::critical(nullptr, tr("Download"), tr("Error in transfer: ").append(e->getErrorString()));
        }
    }
}
