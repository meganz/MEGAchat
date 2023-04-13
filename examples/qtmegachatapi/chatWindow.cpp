#include <iostream>
#include "chatWindow.h"
#include "assert.h"
#include <QMenu>
#include <QFileDialog>
#include <mega/utils.h>

using namespace megachat;

ChatWindow::ChatWindow(QWidget *parent, megachat::MegaChatApi *megaChatApi, megachat::MegaChatRoom *cRoom, const char *title)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{
    nSending = 0;
    mPreview = false;
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
    const char *chatid_64 = mMegaApi->userHandleToBase64(cRoom->getChatId());
    setWindowTitle(chatid_64);
    delete [] chatid_64;
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

    for (int i = 0; i < callMaxParticipants; i ++)
    {
        mFreeCallGui[i] = megachat::MEGACHAT_INVALID_HANDLE;
    }

    if (!mChatRoom->isActive())
    {
        enableWindowControls(false);
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

    if (mChatRoom->isGroup())
    {
        QString speakRequest = QString::fromUtf8("\xE2\x98\x9D");
        QString waitingRoom = QString::fromUtf8("\xE2\x8F\xB2");
        QString openInvite = QString::fromUtf8("\xF0\x9F\x94\x93");

        chatTitle.append("[");
        if (mChatRoom->isSpeakRequest())    { chatTitle.append(" ").append(speakRequest); }
        if (mChatRoom->isWaitingRoom())     { chatTitle.append(" ").append(waitingRoom); }
        if (mChatRoom->isOpenInvite())      { chatTitle.append(" ").append(openInvite); }
        chatTitle.append(" ]");
    }

    if(mChatRoom->isPreview())
    {
        chatTitle.append("        <PREVIEW>");
    }

    if(mChatRoom->isArchived())
    {
        chatTitle.append("        <AR>");
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
    delete mFrameAttachments;

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
        QMessageBox::warning(nullptr, tr("Send message"), tr("Failed to send the message. Too long? Not enough privileges?"));
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
    delete mChatRoom;
    mChatRoom = chat->copy();

    if (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_OWN_PRIV))
    {
        if (!mPreview)
        {
            enableWindowControls(mChatRoom->getOwnPrivilege() > megachat::MegaChatRoom::PRIV_RO);
            setChatTittle(mChatRoom->getTitle());
        }
        else
        {
            previewUpdate();
        }
    }

    if ((chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_CLOSED))
        || (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_TITLE))
        || (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_ARCHIVE)))
    {
       setChatTittle(mChatRoom->getTitle());
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_CHAT_MODE))
    {
       ui->mTitlebar->setStyleSheet("background-color:#c1efff");
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_UPDATE_PREVIEWERS))
    {
       updatePreviewers(chat->getNumPreviewers());
    }

    if(chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_WAITING_ROOM)
            ||chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_SPEAK_REQUEST)
            || chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_OPEN_INVITE))
    {
        setChatTittle(nullptr);
    }

//    if (chat->hasChanged(megachat::MegaChatRoom::CHANGE_TYPE_RETENTION_TIME))
//    {
//        QMessageBox *msgBox = new QMessageBox(this);
//        QString text("onRetentionTimeUpdated: ");
//        text.append(std::to_string(chat->getRetentionTime()).c_str());
//        msgBox->setAttribute(Qt::WA_DeleteOnClose, true);
//        msgBox->setIcon( QMessageBox::Information );
//        msgBox->setStandardButtons(QMessageBox::Ok);
//        msgBox->setWindowTitle(tr("RETENTION HISTORY"));
//        msgBox->setText(text);
//        msgBox->setModal(false);
//        msgBox->show();
//    }
}

void ChatWindow::previewUpdate(MegaChatRoom *auxRoom)
{
    if (auxRoom)
    {
        delete mChatRoom;
        mChatRoom = auxRoom;
    }

    if (mPreview && !mChatRoom->isPreview())
    {
        mPreview = false;

        if (mChatRoom->isPublic())
        {
            ui->mTitlebar->setStyleSheet("background-color:#c4f2c9");
        }
        else
        {
            ui->mTitlebar->setStyleSheet("background-color:#c1efff");
        }
    }

    enableWindowControls(!mChatRoom->isPreview());
    setChatTittle(mChatRoom->getTitle());
}

void ChatWindow::enableWindowControls(bool enable)
{
    ui->mMessageEdit->setEnabled(enable);
    ui->mMessageEdit->blockSignals(!enable);
    ui->mMsgSendBtn->setEnabled(enable);
    enable ?ui->mMembersBtn->show() :ui->mMembersBtn->hide();
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
        ChatMessage *auxMessage(itMessages->second);
        auxMessage->clearReactions();
        int row = ui->mMessageList->row(auxMessage->getWidgetItem());
        ::mega::unique_ptr<QListWidgetItem> auxItem(ui->mMessageList->takeItem(row));
        mMsgsWidgetsMap.erase(itMessages);
        auxMessage->deleteLater();
    }
}

bool ChatWindow::eraseChatMessage(megachat::MegaChatMessage *msg, bool /*temporal*/)
{
    megachat::MegaChatHandle msgId = getMessageId(msg);
    std::map<megachat::MegaChatHandle, ChatMessage *>::iterator itMessages;
    itMessages = mMsgsWidgetsMap.find(msgId);
    if (itMessages == mMsgsWidgetsMap.end())
    {
        itMessages = mMsgsWidgetsMap.find(msg->getTempId());
        if (itMessages == mMsgsWidgetsMap.end())
        {
            return false;
        }
    }

    ChatMessage *auxMessage(itMessages->second);
    int row = ui->mMessageList->row(auxMessage->getWidgetItem());
    ::mega::unique_ptr<QListWidgetItem> auxItem(ui->mMessageList->takeItem(row));
    mMsgsWidgetsMap.erase(itMessages);
    auxMessage->deleteLater();
    return true;
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

ChatListItemController *ChatWindow::getChatItemController()
{
   return mMainWin->getChatControllerById(mChatRoom->getChatId());
}

MainWindow *ChatWindow::getMainWin() const
{
    return mMainWin;
}

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

void ChatWindow::onHistoryTruncatedByRetentionTime(megachat::MegaChatApi *, megachat::MegaChatMessage *msg)
{
//    QString date = QDateTime::fromTime_t(msg->getTimestamp()).toString("hh:mm:ss - dd.MM.yy");
//    QMessageBox *msgBox = new QMessageBox(this);
//    msgBox->setIcon( QMessageBox::Information );
//    msgBox->setAttribute(Qt::WA_DeleteOnClose);
//    msgBox->setStandardButtons(QMessageBox::Ok);
//    msgBox->setWindowTitle(tr("onHistoryTruncatedByRetentionTime"));
//    msgBox->setText("Messages previous to (" + date+ "), will be cleared");
//    msgBox->setModal(false);
//    msgBox->show();

    ChatListItemController *itemController = getChatItemController();
    if (itemController)
    {
        std::map<megachat::MegaChatHandle, ChatMessage*>::iterator itMessages;
        for (itMessages = mMsgsWidgetsMap.begin(); itMessages != mMsgsWidgetsMap.end();)
        {
            auto auxIt = itMessages++;
            ChatMessage *auxMessage(auxIt->second);
            if (auxMessage->mMessage->getTimestamp() <= msg->getTimestamp()
                    && auxMessage->mMessage->getStatus() != megachat::MegaChatMessage::STATUS_SENDING
                    && auxMessage->mMessage->getStatus() != megachat::MegaChatMessage::STATUS_SENDING_MANUAL)
            {
                auxMessage->clearReactions();
                int row = ui->mMessageList->row(auxMessage->getWidgetItem());
                ::mega::unique_ptr <QListWidgetItem> auxItem(ui->mMessageList->takeItem(row));
                mMsgsWidgetsMap.erase(auxIt);
                auxMessage->deleteLater();
            }
        }
    }
}

void ChatWindow::onReactionUpdate(megachat::MegaChatApi *, megachat::MegaChatHandle msgid, const char *reaction, int count)
{
   ChatMessage *msg = findChatMessage(msgid);
   if (!msg)
   {
      mLogger->postLog("onReactionUpdate warning - reaction update received for message received but not loaded by app");
      return;
   }

   msg->updateReaction(reaction, count);
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

    menu.setStyleSheet("QMenu { menu-scrollable: 1; }");
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
            userhandle = QVariant((qulonglong)mMegaChatApi->getMyUserHandle());
            title.append(" Me [")
                    .append(QString::fromStdString(mChatRoom->statusToString(mMegaChatApi->getOnlineStatus())))
                    .append("]");
        }
        else
        {
            QString memberName;
            MegaChatHandle peerHandle = mChatRoom->getPeerHandle(i);
            std::unique_ptr<const char[]> name(mMegaChatApi->getUserFirstnameFromCache(peerHandle));
            if (!name || !strlen(name.get()))
            {
                std::unique_ptr<const char []> email(mMegaChatApi->getUserEmailFromCache(peerHandle));
                memberName = (email && strlen(email.get())) ? email.get() : tr("Yet unknown");
            }
            else
            {
                memberName = name.get();
            }

            privilege = mChatRoom->getPeerPrivilege(i);
            userhandle = QVariant((qulonglong)peerHandle);
            title.append(" ")
                    .append(memberName)
                    .append(" [")
                    .append(QString::fromStdString(mChatRoom->statusToString(mMegaChatApi->getUserOnlineStatus(peerHandle))))
                    .append("]");
        }

        auto entry = menu.addMenu(title);

        if (i == mChatRoom->getPeerCount())  // my own user
        {
            auto actRemove = entry->addAction(tr("Leave chat"));
            actRemove->setProperty("userHandle", userhandle);
            connect(actRemove, SIGNAL(triggered()), getChatItemController(), SLOT(leaveGroupChat()));
        }
        else
        {
            auto actRemove = entry->addAction(tr("Remove from chat"));
            actRemove->setProperty("userHandle", userhandle);
            connect(actRemove, SIGNAL(triggered()), this, SLOT(onMemberRemove()));
        }

        if (privilege != megachat::MegaChatRoom::PRIV_RM)
        {
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
}

void ChatWindow::createSettingsMenu(QMenu& menu)
{
    QMenu *roomMenu = menu.addMenu("Room's management");

    //Leave
    auto leave = roomMenu->addAction("Leave chat");
    connect(leave, SIGNAL(triggered()), getChatItemController(), SLOT(leaveGroupChat()));

    //Truncate
    auto truncate = roomMenu->addAction("Truncate chat");
    connect(truncate, SIGNAL(triggered()), getChatItemController(), SLOT(truncateChat()));

    //Get retention time
    auto actGetRetentionTime = roomMenu->addAction(tr("Get retention time"));
    connect(actGetRetentionTime, SIGNAL(triggered()), getChatItemController(), SLOT(onGetRetentionTime()));

    //Set retention time
    auto actSetRetentionTimeSec = roomMenu->addAction(tr("Set retention time (in seconds)"));
    connect(actSetRetentionTimeSec, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetRetentionTime();});

    // chat options
    //---------------------------------------------------------------------------------------------------------------
    auto actgetChatOptions = roomMenu->addAction(tr("Get chat Options"));
    connect(actgetChatOptions, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onGetChatOptions();});

    QMenu *chatOptionsMenu= roomMenu->addMenu("Set chat options");

    auto actEnableOpenInvite = chatOptionsMenu->addAction(tr("[+] Enable Open invite"));
    connect(actEnableOpenInvite, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetOpenInvite(true);});

    auto actDisableOpenInvite = chatOptionsMenu->addAction(tr("[-] Disable Open invite"));
    connect(actDisableOpenInvite, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetOpenInvite(false);});

    auto actEnableSpeakRequest = chatOptionsMenu->addAction(tr("[+] Enable Speak request"));
    connect(actEnableSpeakRequest, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetSpeakRequest(true);});

    auto actDisableSpeakRequest = chatOptionsMenu->addAction(tr("[-] Disable Speak request"));
    connect(actDisableSpeakRequest, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetSpeakRequest(false);});

    auto actEnableWaitingRoom = chatOptionsMenu->addAction(tr("[+] Enable Waiting room"));
    connect(actEnableWaitingRoom, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetWaitingRoom(true);});

    auto actDisableWaitingRoom = chatOptionsMenu->addAction(tr("[-] Disable Waiting room"));
    connect(actDisableWaitingRoom, &QAction::triggered, getChatItemController(), [=](){getChatItemController()->onSetWaitingRoom(false);});
    //---------------------------------------------------------------------------------------------------------------

    //Set topic
    auto title = roomMenu->addAction("Set title");
    connect(title, SIGNAL(triggered()), getChatItemController(), SLOT(setTitle()));

    auto actArchive = roomMenu->addAction("Archive chat");
    connect(actArchive, SIGNAL(toggled(bool)), getChatItemController(), SLOT(archiveChat(bool)));
    actArchive->setCheckable(true);
    actArchive->setChecked(mChatRoom->isArchived());

    QMenu *clMenu = menu.addMenu("Chat links");

    //Create chat link
    auto createChatLink = clMenu->addAction("Create chat link");
    connect(createChatLink, SIGNAL(triggered()), getChatItemController(), SLOT(createChatLink()));

    //Query chat link
    auto queryChatLink = clMenu->addAction("Query chat link");
    connect(queryChatLink, SIGNAL(triggered()), getChatItemController(), SLOT(queryChatLink()));

    //Remove chat link
    auto removeChatLink = clMenu->addAction("Remove chat link");
    connect(removeChatLink, SIGNAL(triggered()), getChatItemController(), SLOT(removeChatLink()));

    //Auto-join chat link
    auto autojoinPublicChat = clMenu->addAction("Join chat link");
    connect(autojoinPublicChat, SIGNAL(triggered()), getChatItemController(), SLOT(autojoinChatLink()));

    //Set private mode
    auto setPublicChatToPrivate = clMenu->addAction("Set private mode");
    connect(setPublicChatToPrivate, SIGNAL(triggered()), getChatItemController(), SLOT(setPublicChatToPrivate()));


    // Notifications
    QMenu *notificationsMenu = menu.addMenu("Notifications");

    auto actChatCheckPushNotificationRestriction = notificationsMenu->addAction("Check PUSH notification setting");
    connect(actChatCheckPushNotificationRestriction, SIGNAL(triggered()), getChatItemController(), SLOT(onCheckPushNotificationRestrictionClicked()));

    auto actPushReceived = notificationsMenu->addAction(tr("Simulate PUSH received (iOS)"));
    connect(actPushReceived, SIGNAL(triggered()), getChatItemController(), SLOT(onPushReceivedIos()));

    auto actPushAndReceived = notificationsMenu->addAction(tr("Simulate PUSH received (Android)"));
    connect(actPushAndReceived, SIGNAL(triggered()), getChatItemController(), SLOT(onPushReceivedAndroid()));

    auto actImportMsgs = notificationsMenu->addAction(tr("Import messages from NSE cache"));
    connect(actImportMsgs, SIGNAL(triggered()), mMainWin, SLOT(onImportMessages()));

    auto notificationSettings = mMainWin->mApp->getNotificationSettings();
    //Set DND for this chat
    auto actDoNotDisturb = notificationsMenu->addAction("Mute notifications");
    connect(actDoNotDisturb, SIGNAL(toggled(bool)), getChatItemController(), SLOT(onMuteNotifications(bool)));
    if (notificationSettings)
    {
        actDoNotDisturb->setCheckable(true);
        actDoNotDisturb->setChecked(notificationSettings->isChatDndEnabled(mChatRoom->getChatId()));
    }
    else
    {
        actDoNotDisturb->setEnabled(false);
    }

    //Set always notify for this chat
    auto actAlwaysNotify = notificationsMenu->addAction("Notify always");
    connect(actAlwaysNotify, SIGNAL(toggled(bool)), getChatItemController(), SLOT(onSetAlwaysNotify(bool)));
    if (notificationSettings)
    {
        actAlwaysNotify->setCheckable(true);
        actAlwaysNotify->setChecked(notificationSettings->isChatAlwaysNotifyEnabled(mChatRoom->getChatId()));
    }
    else
    {
        actAlwaysNotify->setEnabled(false);
    }

    //Set period to not disturb
    auto actSetDND = notificationsMenu->addAction("Set do-not-disturb");
    connect(actSetDND, SIGNAL(triggered()), getChatItemController(), SLOT(onSetDND()));
    actSetDND->setEnabled(bool(notificationSettings));

    menu.addSeparator();
    // Attachments
    auto actAttachments = menu.addAction("Show attachments");
    connect(actAttachments, SIGNAL(triggered(bool)), this, SLOT(onShowAttachments(bool)));
    actAttachments->setCheckable(true);
    actAttachments->setChecked(mAttachmentList != NULL);
}

void ChatWindow::onMemberAdd()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    MegaChatHandle userhandle = static_cast<MegaChatHandle>(uHandle.toLongLong());
    mMegaChatApi->inviteToChat(mChatRoom->getChatId(), userhandle, megachat::MegaChatPeerList::PRIV_STANDARD);
}

void ChatWindow::onMemberRemove()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
       return;

    QVariant uHandle = action->property("userHandle");
    MegaChatHandle userhandle = static_cast<MegaChatHandle>(uHandle.toLongLong());
    std::unique_ptr<char []> userHandleString(mMegaApi->getMyUserHandle());
    if (uHandle.toString() == userHandleString.get())
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
      MegaChatHandle userhandle = static_cast<MegaChatHandle>(uHandle.toLongLong());
      MegaChatHandle chatId = mChatRoom->getChatId();
      mMegaChatApi->updateChatPermissions(chatId, userhandle, privilege);}

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

void ChatWindow::onAudioCallNoRingBtn()
{
    std::string schedIdStr = mMainWin->mApp->getText("Get scheduled meeting id");
    MegaChatHandle schedId = schedIdStr.empty() ? MEGACHAT_INVALID_HANDLE : mMegaApi->base64ToUserHandle(schedIdStr.c_str());
    mMegaChatApi->startChatCallNoRinging(mChatRoom->getChatId(), schedId, false, false);
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    delete this;
    event->accept();
}

void ChatWindow::onCallBtn(bool video)
{
    mMegaChatApi->startChatCall(this->mChatRoom->getChatId(), video);
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

    //Attach giphy
    auto actGiphy = menu.addAction("Attach giphy");
    connect(actGiphy, &QAction::triggered, this, [=](){onAttachGiphy();});
}

void ChatWindow::onAttachLocation()
{
    mMegaChatApi->sendGeolocation(mChatRoom->getChatId(), -122.3316393f, 47.5951518f, NULL);
}

void ChatWindow::onAttachGiphy()
{
    //giphy data
    const char* srcMp4 = "giphy://media/Wm9XlKG2xIMiVcH4CP/200.mp4?cid=a2a900dl&rid=200.mp4&dom=bWVkaWEyLmdpcGh5LmNvbQ%3D%3D";
    const char* srcWebp = "giphy://media/Wm9XlKG2xIMiVcH4CP/200.webp?cid=a2a900dl&rid=200.webp&dom=bWVkaWEyLmdpcGh5LmNvbQ%3D%3D";
    long long sizeMp4 = 59970;
    long long sizeWebp = 159970;
    int giphyWidth = 200;
    int giphyHeight = 200;
    const char* giphyTitle = "MegaChatApiTest.SendGiphy";
    mMegaChatApi->sendGiphy(mChatRoom->getChatId(), srcMp4, srcWebp, sizeMp4, sizeWebp, giphyWidth, giphyHeight, giphyTitle);
}

void ChatWindow::onAttachNode(bool isVoiceClip)
{
    QString node = QFileDialog::getOpenFileName(this, tr("All Files (*)"));
    if (node.isEmpty())
       return;

    ::mega::MegaNode *parent = mMegaApi->getNodeByPath("/");
    mUploadDlg = new QMessageBox;
    mUploadDlg->setWindowTitle((tr("Uploading file...")));
    mUploadDlg->setIcon(QMessageBox::Warning);
    mUploadDlg->setText(tr("Please, wait.\nThe file will be attached when the transfer completes."));
    mUploadDlg->setStandardButtons(QMessageBox::Cancel);
    mUploadDlg->setModal(true);
    mUploadDlg->show();
    connect(mUploadDlg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_mCancelTransfer(QAbstractButton*)));

    if (isVoiceClip)
    {
        mMegaApi->startUpload(node.toStdString().c_str(), parent, nullptr, 0, "vm", false, false, nullptr);
    }
    else
    {
        mMegaApi->startUploadForChat(node.toStdString().c_str(), parent, nullptr, false, nullptr);
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

MegaChatApi *ChatWindow::getMegaChatApi()
{
    return mMegaChatApi;
}
