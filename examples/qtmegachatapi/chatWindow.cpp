#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>
#include <QFileDialog>

ChatWindow::ChatWindow(QWidget *parent, megachat::MegaChatApi *megaChatApi, megachat::MegaChatRoom *cRoom, const char *title)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{
    nSending = 0;
#ifndef KARERE_DISABLE_WEBRTC
    mCallGui = NULL;
#endif
    loadedMessages = 0;
    loadedAttachments = 0;
    mScrollToBottomAttachments = true;
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
    setWindowTitle(title);
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
    megaTransferListenerDelegate = new ::mega::QTMegaTransferListener(mMegaApi, this);
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
    ChatListItemController *itemController = mMainWin->getChatControllerById(mChatRoom->getChatId());
    if(itemController)
    {
       itemController->invalidChatWindow();
    }

    mMegaChatApi->closeChatRoom(mChatRoom->getChatId(),megaChatRoomListenerDelegate);
    mMegaApi->removeTransferListener(megaTransferListenerDelegate);

    delete megaChatNodeHistoryListenerDelegate;
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

void ChatWindow::moveManualSendingToSending(megachat::MegaChatMessage *msg)
{
    nSending++;
    nManualSending--;
    addMsgWidget(msg, loadedMessages + nSending);
}

void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi *, megachat::MegaChatRoom *chat)
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

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_PARTICIPANTS)
            || chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_ARCHIVE))
    {
        delete mChatRoom;
        this->mChatRoom = chat->copy();
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_OWN_PRIV))
    {
        setChatTittle(NULL);
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

                if(msg->getUserHandle() != mMegaChatApi->getMyUserHandle())
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
        ChatMessage *auxMessage = itMessages->second;
        int row = ui->mMessageList->row(auxMessage->getWidgetItem());
        QListWidgetItem *auxItem = ui->mMessageList->takeItem(row);
        mMsgsWidgetsMap.erase(itMessages);
        delete auxItem;
        return true;
    }
    return false;
}

ChatMessage *ChatWindow::findChatMessage(megachat::MegaChatHandle msgId)
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

void ChatWindow::onAttachmentLoaded(MegaChatApi */*api*/, MegaChatMessage *msg)
{
    if (msg)
    {
        QListWidgetItem *item = new QListWidgetItem;
        megachat::MegaChatHandle chatId = mChatRoom->getChatId();
        ChatMessage *widget = new ChatMessage(this, mMegaChatApi, chatId, msg->copy());
        widget->setWidgetItem(item);
        item->setSizeHint(widget->size());
        setMessageHeight(msg,item);

        mAttachmentList->insertItem(loadedAttachments, item);
        mAttachmentList->setItemWidget(item, widget);

        loadedAttachments--;

        if (msg->getType() != MegaChatMessage::TYPE_NODE_ATTACHMENT)
        {
            widget->ui->mMsgDisplay->setStyleSheet("background-color: rgba(255,80,80,128)\n");
        }
    }
    else if (mScrollToBottomAttachments)
    {
        mAttachmentList->scrollToBottom();
        mScrollToBottomAttachments = false;
    }
}

void ChatWindow::onAttachmentReceived(MegaChatApi */*api*/, MegaChatMessage *msg)
{
    if (msg)
    {
        QListWidgetItem *item = new QListWidgetItem;
        megachat::MegaChatHandle chatId = mChatRoom->getChatId();
        ChatMessage *widget = new ChatMessage(this, mMegaChatApi, chatId, msg->copy());
        widget->setWidgetItem(item);
        item->setSizeHint(widget->size());
        setMessageHeight(msg,item);

        mAttachmentList->addItem(item);
        mAttachmentList->setItemWidget(item, widget);
        mAttachmentList->scrollToBottom();

        if (msg->getType() != MegaChatMessage::TYPE_NODE_ATTACHMENT)
        {
            widget->ui->mMsgDisplay->setStyleSheet("background-color: rgba(255,80,80,128)\n");
        }
    }
}

void ChatWindow::onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid)
{
    for (int i = 0; i < mAttachmentList->count(); i++)
    {
        QListWidgetItem *item = mAttachmentList->item(i);
        ChatMessage *widget = dynamic_cast<ChatMessage *>(mAttachmentList->itemWidget(item));
        if (widget && widget->getMessage()->getMsgId() == msgid)
        {
            MegaChatMessage *msg = api->getMessage(mChatRoom->getChatId(), msgid);
            if (msg)
            {
                widget->setMessage(msg);
            }

            widget->updateContent();
            widget->ui->mMsgDisplay->setStyleSheet("background-color: rgba(255,80,80,128)\n");
            break;
        }
    }
}

void ChatWindow::onTruncate(MegaChatApi */*api*/, MegaChatHandle msgid)
{
    std::vector <MegaChatHandle> ids;
    for (int j = 0; j < mAttachmentList->count(); j++)
    {
        QListWidgetItem *item = mAttachmentList->item(j);
        ChatMessage *widget = static_cast<ChatMessage *>(mAttachmentList->itemWidget(item));
        ids.push_back(widget->getMessage()->getMsgId());
        if (widget->getMessage()->getMsgId() == msgid)
        {
            break;
        }
    }

    for (unsigned int j = 0; j < ids.size(); j++)
    {
        MegaChatHandle id = ids[j];
        for (int i = 0; i < mAttachmentList->count(); i++)
        {
            QListWidgetItem *item = mAttachmentList->item(i);
            ChatMessage *widget = dynamic_cast<ChatMessage *>(mAttachmentList->itemWidget(item));
            if (widget && widget->getMessage()->getMsgId() == id)
            {
                delete mAttachmentList->takeItem(i);
                break;
            }
        }
    }
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
    QListWidgetItem *item = new QListWidgetItem;
    megachat::MegaChatHandle chatId = mChatRoom->getChatId();
    ChatMessage *widget = new ChatMessage(this, mMegaChatApi, chatId, msg);
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

void ChatWindow::onShowAttachments(bool active)
{
    if (active)
    {
        assert(!mAttachmentList);
        mFrameAttachments = new QWidget();
        mFrameAttachments->setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
        mFrameAttachments->setAttribute(Qt::WA_DeleteOnClose);
        mFrameAttachments->setWindowTitle("Attachments of room: " + windowTitle());
        mFrameAttachments->setGeometry(x()+350, y(), width(), height());

        mAttachmentList = new MyMessageList(mFrameAttachments);
        mFrameAttachments->setLayout(new QBoxLayout(QBoxLayout::Direction::LeftToRight));
        mFrameAttachments->layout()->addWidget(mAttachmentList);
        mFrameAttachments->show();
        mFrameAttachments->resize(450, 350);
        connect(mAttachmentList, SIGNAL(requestHistory()), this, SLOT(onAttachmentRequestHistory()));
        connect(mAttachmentList->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));
        connect(mFrameAttachments, SIGNAL(destroyed(QObject*)), this, SLOT(onAttachmentsClosed(QObject*)));

        loadedAttachments = 0;
        mScrollToBottomAttachments = true;

        megaChatNodeHistoryListenerDelegate = new megachat::QTMegaChatNodeHistoryListener(mMegaChatApi, this);
        mMegaChatApi->openNodeHistory(mChatRoom->getChatId(), megaChatNodeHistoryListenerDelegate);
        mMegaChatApi->loadAttachments(mChatRoom->getChatId(), NMESSAGES_LOAD);
    }
    else
    {
        assert(mAttachmentList);
        delete mFrameAttachments;
    }
}

void ChatWindow::onAttachmentRequestHistory()
{
    mMegaChatApi->loadAttachments(mChatRoom->getChatId(), NMESSAGES_LOAD);
}

void ChatWindow::createMembersMenu(QMenu& menu)
{
    //Add contacts
    ::mega::MegaUserList *userList = mMegaApi->getContacts();

    auto addEntry = menu.addMenu("Add contact to chat");
    for (int i = 0 ; i < userList->size(); i++)
    {
         ::mega::MegaUser *user = userList->get(i);
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
    //Truncate
    auto truncate = menu.addAction("Truncate history");
    connect(truncate, SIGNAL(triggered()), this, SLOT(onTruncateChat()));

    //Set topic
    auto title = menu.addAction("Set title");
    connect(title, SIGNAL(triggered()), this, SLOT(onChangeTitle()));

    //Archive
    auto actArchive = menu.addAction("Archive chat");
    connect(actArchive, SIGNAL(toggled(bool)), this, SLOT(onArchiveClicked(bool)));
    actArchive->setCheckable(true);
    actArchive->setChecked(mChatRoom->isArchived());

    // Attachments
    auto actAttachments = menu.addAction("List attachments");
    connect(actAttachments, SIGNAL(triggered(bool)), this, SLOT(onShowAttachments(bool)));
    actAttachments->setCheckable(true);
    actAttachments->setChecked(mAttachmentList != NULL);

    QMenu *clMenu = menu.addMenu("Chat links");

    //Query chat link
    auto queryChatLink = clMenu->addAction("Query chat link");
    connect(queryChatLink, SIGNAL(triggered()), this, SLOT(onQueryChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    //Export chat link
    auto exportChatLink = clMenu->addAction("Export chat link");
    connect(exportChatLink, SIGNAL(triggered()), this, SLOT(onExportChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    //Remove chat link
    auto removeChatLink = clMenu->addAction("Remove chat link");
    connect(removeChatLink, SIGNAL(triggered()), this, SLOT(onRemoveChatLink()));
    // TODO: connect to slot in chat-links branch once merged

    //Auto-join chat link
    auto joinChatLink = clMenu->addAction("Join chat link");
    connect(joinChatLink, SIGNAL(triggered()), this, SLOT(on_mJoinBtn_clicked()));
    // TODO: connect to slot in chat-links branch once merged

    //Close chat link
    auto closeChatLink = clMenu->addAction("Close chat link");
    connect(closeChatLink, SIGNAL(triggered()), this, SLOT(onCloseChatLink()));
    // TODO: connect to slot in chat-links branch once merged
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

    if (peerid == mMegaChatApi->getMyUserHandle())
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
   createCallGui(video, mMegaChatApi->getMyUserHandle());
   MegaChatCall *auxCall = mMegaChatApi->getChatCall(mChatRoom->getChatId());
   if (auxCall == NULL || (auxCall && auxCall->getStatus() == megachat::MegaChatCall::CALL_STATUS_USER_NO_PRESENT))
   {
       mMegaChatApi->startChatCall(this->mChatRoom->getChatId(), video);
       delete auxCall;
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
    QMenu menu(this);
    createAttachMenu(menu);
    menu.setLayoutDirection(Qt::RightToLeft);
    menu.adjustSize();
    menu.exec(ui->mAttachBtn->mapToGlobal(
    QPoint(-menu.width()+ui->mAttachBtn->width(), ui->mAttachBtn->height())));
    menu.deleteLater();
}

void ChatWindow::createAttachMenu(QMenu& menu)
{
    //Attach node
    auto actNode = menu.addAction("Attach node");
    connect(actNode, &QAction::triggered, this, [=](){onAttachNode(false);});

    //Attach voice clip
    auto actVoice = menu.addAction("Attach voice clip");
    connect(actVoice, &QAction::triggered, this, [=](){onAttachNode(true);});

    //Attach geolocation
    auto actLocation = menu.addAction("Attach location");
    connect(actLocation, &QAction::triggered, this, [=](){onAttachLocation();});
}

void ChatWindow::onAttachLocation()
{
    mMegaChatApi->sendGeolocation(mChatRoom->getChatId(), -122.3316393, 47.5951518, NULL);
}

void ChatWindow::onAttachNode(bool isVoiceClip)
{
    QString node = QFileDialog::getOpenFileName(this, tr("All Files (*)"));
    if (node.isEmpty())
       return;

    mUploadDlg = new QMessageBox;
    mUploadDlg->setWindowTitle((tr("Uploading file...")));
    mUploadDlg->setIcon(QMessageBox::Warning);
    mUploadDlg->setText(tr("Please, wait.\nThe file will be attached when the transfer completes."));
    mUploadDlg->setStandardButtons(QMessageBox::Cancel);
    mUploadDlg->setModal(true);
    mUploadDlg->show();
    connect(mUploadDlg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_mCancelTransfer(QAbstractButton*)));

    ::mega::MegaNode *parent = mMegaApi->getNodeByPath("/");

    if (isVoiceClip)
    {
        mMegaApi->startUploadWithData(node.toStdString().c_str(), parent, "vm");
    }
    else
    {
        mMegaApi->startUpload(node.toStdString().c_str(), parent);
    }

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

void ChatWindow::onAttachmentsClosed(QObject *)
{
    mMegaChatApi->closeNodeHistory(mChatRoom->getChatId(), megaChatNodeHistoryListenerDelegate);
    mFrameAttachments = NULL;
    mAttachmentList = NULL;
    delete megaChatNodeHistoryListenerDelegate;
    megaChatNodeHistoryListenerDelegate = NULL;
}

void ChatWindow::onTransferFinish(::mega::MegaApi* , ::mega::MegaTransfer *transfer, ::mega::MegaError* e)
{
    if (transfer->getType() == ::mega::MegaTransfer::TYPE_UPLOAD)
    {
        if (mUploadDlg)
        {
            mUploadDlg->hide();
            if (e->getErrorCode() == ::mega::MegaError::API_OK)
            {
                const char *appData = transfer->getAppData();
                if (appData && strcmp(transfer->getAppData(),"vm") == 0)
                {
                    mMegaChatApi->attachVoiceMessage(mChatRoom->getChatId(), transfer->getNodeHandle());
                }
                else
                {
                    mMegaChatApi->attachNode(mChatRoom->getChatId(), transfer->getNodeHandle());
                }
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
        if (e->getErrorCode() == ::mega::MegaError::API_OK)
        {
            QMessageBox::information(nullptr, tr("Download"), tr("Attachment's download successful."));
        }
        else
        {
            QMessageBox::critical(nullptr, tr("Download"), tr("Error in transfer: ").append(e->getErrorString()));
        }
    }
}
