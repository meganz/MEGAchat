#include "chatItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMenu>
#include <iostream>
#include <QMessageBox>
#include <QClipboard>

#define MEMBERS_REQUESTED 20

ChatItemWidget::ChatItemWidget(MainWindow *mainWindow, const megachat::MegaChatListItem *item)
    : QWidget(mainWindow),
      ui(new Ui::ChatItem),
      mMainWin(mainWindow),
      mChatId(item->getChatId()),
      mMegaChatApi(mainWindow->mMegaChatApi),
      mController(mainWindow->getChatControllerById(mChatId)),
      mLastOverlayCount(0)
{
    ui->setupUi(this);

    int unreadCount = item->getUnreadCount();
    onUnreadCountChanged(unreadCount);
    onPreviewersCountChanged(item->getNumPreviewers());

    QString text = NULL;
    if (item->isArchived())
    {
        text.append(item->getTitle())
        .append("[A]");

        if (!item->isActive())
        {
            text.append("[H]");
        }
        ui->mName->setText(text);
        ui->mName->setStyleSheet("color:#DEF0FC;");
        ui->mAvatar->setStyleSheet("color:#DEF0FC;");
    }
    else
    {
        if (!item->isActive())
        {
            text.append(item->getTitle())
            .append(" [H]");
            ui->mName->setText(text);
            ui->mName->setStyleSheet("color:#FFC9C6;");
            ui->mAvatar->setStyleSheet("color:#FFC9C6;");
        }
        else
        {
            ui->mName->setText(item->getTitle());
            ui->mName->setStyleSheet("color:#FFFFFF; font-weight:bold;");
            ui->mAvatar->setStyleSheet("color:#FFFFFF; font-weight:bold;");
        }
    }

    if (!item->isGroup())
    {
        ui->mAvatar->setText("1");
    }
    else
    {
        if(item->isPublic())
        {
            if (!item->isPreview())
            {
                std::unique_ptr<megachat::MegaChatRoom> chatRoom = std::unique_ptr<megachat::MegaChatRoom>(mMegaChatApi->getChatRoom(mChatId));
                if (chatRoom && chatRoom->isMeeting())
                {
                    ui->mAvatar->setText("M");
                    ui->mAvatar->setStyleSheet("color: #F28230");
                }
                else
                {
                    ui->mAvatar->setText("P");
                    ui->mAvatar->setStyleSheet("color: #43B63D");
                }
            }
            else
            {
                ui->mAvatar->setText("V");
                ui->mAvatar->setStyleSheet("color: #FF0C14");
            }
        }
        else
        {
             ui->mAvatar->setText("G");
        }
    }

    int status = mMegaChatApi->getChatConnectionState(mChatId);
    this->onlineIndicatorUpdate(status);
}

void ChatItemWidget::updateToolTip(const megachat::MegaChatListItem *item, const char *author)
{
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);
    if (!chatRoom)
    {
        // chatroom might be gone (due to takedown), despite it was locally know -> there is a chat-list-item, etc.
        return;
    }

    QString text = NULL;
    std::string senderHandle;
    megachat::MegaChatHandle lastMessageId = item->getLastMessageId();
    int lastMessageType = item->getLastMessageType();
    std::string lastMessage;
    const char *lastMessageId_64 = "----------";
    const char *auxLastMessageId_64 = mMainWin->mMegaApi->userHandleToBase64(lastMessageId);
    const char *chatId_64 = mMainWin->mMegaApi->userHandleToBase64(mChatId);
    std::string chatId_Bin = std::to_string(mChatId);

    megachat::MegaChatHandle lastMessageSender = item->getLastMessageSender();
    if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
    {
        senderHandle = "";
    }
    else
    {
        char *uh = mMainWin->mMegaApi->userHandleToBase64(lastMessageSender);
        senderHandle.assign(uh);
        delete [] uh;
    }

    if (author)
    {
        mLastMsgAuthor.assign(author);
    }
    else
    {
        if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
        {
            mLastMsgAuthor = "";
        }
        else
        {
            const char *msgAuthor = getLastMessageSenderName(lastMessageSender);
            const char *autorizationToken = chatRoom->getAuthorizationToken();
            if (msgAuthor || (msgAuthor = mMainWin->mApp->getFirstname(lastMessageSender, autorizationToken)))
            {
                mLastMsgAuthor.assign(msgAuthor);
            }
            else
            {
                mLastMsgAuthor = "Loading firstname...";
            }
            delete [] msgAuthor;
            delete [] autorizationToken;
        }
    }
    switch (lastMessageType)
    {
        case megachat::MegaChatMessage::TYPE_INVALID:
            lastMessage = "<No history>";
            break;

        case 0xFF:
            lastMessage = "<loading...>";
            break;

        case megachat::MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
        {
            std::string targetName;
            if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
            {
                targetName = "";
            }
            else
            {
                char *uh = mMainWin->mMegaApi->userHandleToBase64(item->getLastMessageHandle());
                targetName.assign(uh);
                delete [] uh;
            }

            bool removed = item->getLastMessagePriv() == megachat::MegaChatRoom::PRIV_RM;
            lastMessage.append("User ").append(senderHandle)
                    .append(removed ? " removed" : " added")
                    .append(" user ").append(targetName);
            break;
        }
        case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
        {
            std::string targetName;
            std::string priv;
            if (lastMessageSender == megachat::MEGACHAT_INVALID_HANDLE)
            {
                targetName = "";
                priv = "";
            }
            else
            {
                char *uh = mMainWin->mMegaApi->userHandleToBase64(item->getLastMessageHandle());
                targetName.assign(uh);
                priv.assign(megachat::MegaChatRoom::privToString(item->getLastMessagePriv()));
                delete [] uh;
            }
            lastMessage.append("User ").append(senderHandle)
                       .append(" set privilege of user ").append(targetName)
                       .append(" to ").append(priv);
            break;
        }
        case megachat::MegaChatMessage::TYPE_TRUNCATE:
            lastMessage = "Truncate";
            break;

        case megachat::MegaChatMessage::TYPE_SET_RETENTION_TIME:
            lastMessage.append("User ").append(senderHandle)
                   .append(" set retention time to ").append(item->getLastMessage()).append(" seconds");
            break;

        case megachat::MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
            lastMessage.append("User ").append(senderHandle)
                       .append(" attached a contact: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
            lastMessage.append("User ").append(senderHandle)
                       .append(" attached a node: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
            lastMessage.append("User ").append(senderHandle)
                       .append(" sent a ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
            lastMessage.append("User ").append(senderHandle)
                       .append(" set chat title: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CONTAINS_META: // fall-through
            lastMessage.append("Metadata: ").append(item->getLastMessage());
            break;

        case megachat::MegaChatMessage::TYPE_CALL_ENDED:
        {
            QString qstring(item->getLastMessage());
            if (!qstring.isEmpty())
            {
                QStringList stringList = qstring.split(0x01);
                assert(stringList.size() >= 2);
                lastMessage.append("Call started by: ")
                           .append(senderHandle)
                           .append(" Duration: ")
                           .append(stringList.at(0).toStdString())
                           .append("secs TermCode: ")
                           .append(stringList.at(1).toStdString());
            }
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_STARTED:
        {
            lastMessage.append("User ").append(senderHandle)
               .append(" has started a call");
        }
        default:
            lastMessage = item->getLastMessage();
            break;
    }

    if(item->getLastMessageId() != megachat::MEGACHAT_INVALID_HANDLE)
    {
        lastMessageId_64 = auxLastMessageId_64;
    }

    QDateTime t;
    t.setTime_t(static_cast<unsigned int>(item->getLastTimestamp()));
    QString lastTs = t.toString("hh:mm:ss - dd.MM.yy");

    if(!item->isGroup())
    {
        const char *peerEmail = mMegaChatApi->getUserEmailFromCache(item->getPeerHandle());
        const char *peerHandle_64 = mMainWin->mMegaApi->userHandleToBase64(item->getPeerHandle());
        text.append(tr("1on1 Chat room handle B64:"))
            .append(QString::fromStdString(chatId_64))
            .append(tr("\n1on1 Chat room handle Bin:"))
            .append(QString::fromStdString(chatId_Bin))
            .append(tr("\nEmail: "))
            .append(peerEmail ? QString::fromStdString(peerEmail) : tr("(email unknown)"))
            .append(tr("\nUser handle: ")).append(QString::fromStdString(peerHandle_64))
            .append(tr("\n\n"))
            .append(tr("\nLast message Id: ")).append(lastMessageId_64)
            .append(tr("\nLast message Sender: ")).append(mLastMsgAuthor.c_str())
            .append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage))
            .append(tr("\nLast ts: ")).append(lastTs);
        delete [] peerHandle_64;
        delete [] peerEmail;
    }
    else
    {
        int ownPrivilege = chatRoom->getOwnPrivilege();
        text.append(tr("Group chat room handle B64: "))
            .append(QString::fromStdString(chatId_64)
            .append(tr("\nGroup chat room handle Bin: "))
            .append(QString::fromStdString(chatId_Bin))
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
                const char* firstname = mMegaChatApi->getUserFirstnameFromCache(chatRoom->getPeerHandle(i));
                const char* lastname = mMegaChatApi->getUserLastnameFromCache(chatRoom->getPeerHandle(i));
                QString peerName = QString(firstname ? firstname : tr("firstname unknown"))
                        + QString(" ") + QString(lastname ? lastname : tr("lastname unknown"));

                const char *peerEmail = mMegaChatApi->getUserEmailFromCache(chatRoom->getPeerHandle(i));
                const char *peerId_64 = mMainWin->mMegaApi->userHandleToBase64(chatRoom->getPeerHandle(i));
                int peerPriv = chatRoom->getPeerPrivilege(i);
                auto line = QString(" %1 (%2, %3): priv %4\n")
                        .arg(peerName)
                        .arg(peerEmail ? QString::fromStdString(peerEmail) : tr("(email unknown)"))
                        .arg(QString::fromStdString(peerId_64))
                        .arg(QString(chatRoom->privToString(peerPriv)));
                text.append(line);
                delete [] firstname;
                delete [] lastname;
                delete [] peerEmail;
                delete [] peerId_64;
            }
            text.resize(text.size()-1);
        }
        text.append(tr("\n\n"));
        text.append(tr("\nLast message Id: ")).append(lastMessageId_64);
        text.append(tr("\nLast message Sender: ")).append(mLastMsgAuthor.c_str());
        text.append(tr("\nLast message: ")).append(QString::fromStdString(lastMessage));
        text.append(tr("\nLast ts: ")).append(lastTs);
    }
    setToolTip(text);
    delete chatRoom;
    delete [] chatId_64;
    delete [] auxLastMessageId_64;
}

const char *ChatItemWidget::getLastMessageSenderName(megachat::MegaChatHandle msgUserId)
{
    char *msgAuthor = NULL;
    if(msgUserId == mMegaChatApi->getMyUserHandle())
    {
        msgAuthor = new char[3];
        strcpy(msgAuthor, "Me");
    }
    else
    {
        megachat::MegaChatRoom *chatRoom = this->mMegaChatApi->getChatRoom(mChatId);
        if (chatRoom)
        {
            const char *msg =  mMegaChatApi->getUserFirstnameFromCache(msgUserId);
            size_t len = msg ? strlen(msg) : 0;
            if (len)
            {
                msgAuthor = new char[len + 1];
                strncpy(msgAuthor, msg, len);
                msgAuthor[len] = '\0';
                delete [] msg;
            }
            delete chatRoom;
        }
    }
    return msgAuthor;
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

void ChatItemWidget::onPreviewersCountChanged(int count)
{
    ui->mPreviewersIndicator->setText(QString::number(count));
    ui->mPreviewersIndicator->setVisible(count != 0);
    ui->mPreviewersIndicator->adjustSize();
}

void ChatItemWidget::onTitleChanged(const std::string& title)
{
    QString text = QString::fromStdString(title);
    ui->mName->setText(text);
}

void ChatItemWidget::mouseDoubleClickEvent(QMouseEvent */*event*/)
{
   ChatListItemController *itemController = mMainWin->getChatControllerById(mChatId);
   if (itemController)
   {
      itemController->showChatWindow();
   }
}

QListWidgetItem *ChatItemWidget::getWidgetItem() const
{
    return mListWidgetItem;
}

void ChatItemWidget::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
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

void ChatItemWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(mChatId);

    QMenu *roomMenu = menu.addMenu("Room's management");

    auto actLeave = roomMenu->addAction(tr("Leave chat"));
    connect(actLeave, SIGNAL(triggered()), mController, SLOT(leaveGroupChat()));

    auto actUpdateMeetingOccurr = roomMenu->addAction(tr("Update scheduled meeting occurrence"));
    connect(actUpdateMeetingOccurr, SIGNAL(triggered()), mController, SLOT(updateScheduledMeetingOccurrence()));

    auto actDelSchedMeeting = roomMenu->addAction(tr("Delete scheduled meeting"));
    connect(actDelSchedMeeting, SIGNAL(triggered()), mController, SLOT(removeScheduledMeeting()));

    auto actFetchSchedMeeting = roomMenu->addAction(tr("Fetch scheduled meetings"));
    connect(actFetchSchedMeeting, SIGNAL(triggered()), mController, SLOT(fetchScheduledMeeting()));

    auto actFetchSchedMeetingEvents = roomMenu->addAction(tr("Fetch scheduled meetings events"));
    connect(actFetchSchedMeetingEvents, SIGNAL(triggered()), mController, SLOT(fetchScheduledMeetingEvents()));

    auto actTruncate = roomMenu->addAction(tr("Truncate chat"));
    connect(actTruncate, SIGNAL(triggered()), mController, SLOT(truncateChat()));

    auto actGetRetentionTime = roomMenu->addAction(tr("Get retention time"));
    connect(actGetRetentionTime, SIGNAL(triggered()), mController, SLOT(onGetRetentionTime()));

    auto actSetRetentionTimeSec = roomMenu->addAction(tr("Set retention time (in seconds)"));
    connect(actSetRetentionTimeSec, &QAction::triggered, mController, [=](){mController->onSetRetentionTime();});

    // chat options
    //---------------------------------------------------------------------------------------------------------------
    auto actgetChatOptions = roomMenu->addAction(tr("Get chat Options"));
    connect(actgetChatOptions, &QAction::triggered, mController, [=](){mController->onGetChatOptions();});

    QMenu *chatOptionsMenu= roomMenu->addMenu("Set chat options");

    auto actEnableOpenInvite = chatOptionsMenu->addAction(tr("[+] Enable Open invite"));
    connect(actEnableOpenInvite, &QAction::triggered, mController, [=](){mController->onSetOpenInvite(true);});

    auto actDisableOpenInvite = chatOptionsMenu->addAction(tr("[-] Disable Open invite"));
    connect(actDisableOpenInvite, &QAction::triggered, mController, [=](){mController->onSetOpenInvite(false);});

    auto actEnableSpeakRequest = chatOptionsMenu->addAction(tr("[+] Enable Speak request"));
    connect(actEnableSpeakRequest, &QAction::triggered, mController, [=](){mController->onSetSpeakRequest(true);});

    auto actDisableSpeakRequest = chatOptionsMenu->addAction(tr("[-] Disable Speak request"));
    connect(actDisableSpeakRequest, &QAction::triggered, mController, [=](){mController->onSetSpeakRequest(false);});

    auto actEnableWaitingRoom = chatOptionsMenu->addAction(tr("[+] Enable Waiting room"));
    connect(actEnableWaitingRoom, &QAction::triggered, mController, [=](){mController->onSetWaitingRoom(true);});

    auto actDisableWaitingRoom = chatOptionsMenu->addAction(tr("[-] Disable Waiting room"));
    connect(actDisableWaitingRoom, &QAction::triggered, mController, [=](){mController->onSetWaitingRoom(false);});
    //---------------------------------------------------------------------------------------------------------------

    auto actTopic = roomMenu->addAction(tr("Set title"));
    connect(actTopic, SIGNAL(triggered()), mController, SLOT(setTitle()));

    auto actArchive = roomMenu->addAction("Archive chat");
    connect(actArchive, SIGNAL(toggled(bool)), mController, SLOT(archiveChat(bool)));
    actArchive->setCheckable(true);
    actArchive->setChecked(chatRoom->isArchived());

    QMenu *clMenu = menu.addMenu("Chat links");

    auto actExportLink = clMenu->addAction(tr("Create chat link"));
    connect(actExportLink, SIGNAL(triggered()), mController, SLOT(createChatLink()));

    auto actQueryLink = clMenu->addAction(tr("Query chat link"));
    connect(actQueryLink, SIGNAL(triggered()), mController, SLOT(queryChatLink()));

    auto actRemoveLink = clMenu->addAction(tr("Remove chat link"));
    connect(actRemoveLink, SIGNAL(triggered()), mController, SLOT(removeChatLink()));

    auto actAutojoinPublicChat = clMenu->addAction(tr("Join chat link"));
    connect(actAutojoinPublicChat, SIGNAL(triggered()), mController, SLOT(autojoinChatLink()));

    clMenu->addSeparator();
    auto actSetPrivate = clMenu->addAction(tr("Set private mode"));
    connect(actSetPrivate, SIGNAL(triggered()), mController, SLOT(setPublicChatToPrivate()));

    auto actcloseChatPreview = clMenu->addAction(tr("Close preview"));
    connect(actcloseChatPreview, SIGNAL(triggered()), mController, SLOT(closeChatPreview()));

    // Notifications
    QMenu *notificationsMenu = menu.addMenu("Notifications");

    auto actChatCheckPushNotificationRestriction = notificationsMenu->addAction("Check PUSH notification setting");
    connect(actChatCheckPushNotificationRestriction, SIGNAL(triggered()), mController, SLOT(onCheckPushNotificationRestrictionClicked()));

    auto actPushReceived = notificationsMenu->addAction(tr("Simulate PUSH received (iOS)"));
    connect(actPushReceived, SIGNAL(triggered()), mController, SLOT(onPushReceivedIos()));

    auto actPushAndReceived = notificationsMenu->addAction(tr("Simulate PUSH received (Android)"));
    connect(actPushAndReceived, SIGNAL(triggered()), mController, SLOT(onPushReceivedAndroid()));

    auto notificationSettings = mMainWin->mApp->getNotificationSettings();
    //Set DND for this chat
    auto actDoNotDisturb = notificationsMenu->addAction("Mute notifications");
    connect(actDoNotDisturb, SIGNAL(toggled(bool)), mController, SLOT(onMuteNotifications(bool)));
    if (notificationSettings)
    {
        actDoNotDisturb->setCheckable(true);
        actDoNotDisturb->setChecked(notificationSettings->isChatDndEnabled(mChatId));
    }
    else
    {
        actDoNotDisturb->setEnabled(false);
    }

    //Set always notify for this chat
    auto actAlwaysNotify = notificationsMenu->addAction("Notify always");
    connect(actAlwaysNotify, SIGNAL(toggled(bool)), mController, SLOT(onSetAlwaysNotify(bool)));
    if (notificationSettings)
    {
        actAlwaysNotify->setCheckable(true);
        actAlwaysNotify->setChecked(notificationSettings->isChatAlwaysNotifyEnabled(mChatId));
    }
    else
    {
        actAlwaysNotify->setEnabled(false);
    }

    //Set period to not disturb
    auto actSetDND = notificationsMenu->addAction("Set do-not-disturb");
    connect(actSetDND, SIGNAL(triggered()), mController, SLOT(onSetDND()));
    actSetDND->setEnabled(bool(notificationSettings));

    clMenu->addSeparator();
    auto actPrintChat = menu.addAction(tr("Print chat info"));
    connect(actPrintChat, SIGNAL(triggered()), this, SLOT(onPrintChatInfo()));

    auto actTooltipChat = menu.addAction(tr("Update tooltip"));
    connect(actTooltipChat, SIGNAL(triggered()), this, SLOT(onUpdateTooltip()));

    auto actCopy = menu.addAction(tr("Copy chatid to clipboard"));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopyHandle()));

    if (chatRoom->isPublic())
    {
        auto actRequestMember = menu.addAction(tr("Request Member Info"));
        connect(actRequestMember, SIGNAL(triggered()), this, SLOT(onResquestMemberInfo()));
    }

    delete chatRoom;
    menu.exec(event->globalPos());
    menu.deleteLater();
}

void ChatItemWidget::onUpdateTooltip()
{
    if (mController && mController->getItem())
    {
        updateToolTip(mController->getItem(), nullptr);
    }
}

void ChatItemWidget::onPrintChatInfo()
{
    QMessageBox msg;
    msg.setIcon(QMessageBox::Information);
    msg.setText(this->toolTip());
    msg.exec();
}

void ChatItemWidget::onCopyHandle()
{
    const char *chatid_64 = ::mega::MegaApi::userHandleToBase64(mChatId);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(chatid_64);
    delete []chatid_64;
}

void ChatItemWidget::onResquestMemberInfo()
{
    mega::MegaHandleList* userList;
    userList = mega::MegaHandleList::createInstance();
    megachat::MegaChatRoom* room = mMegaChatApi->getChatRoom(mChatId);
    int i = 0;
    for (i = mIndexMemberRequested; i < (mIndexMemberRequested + MEMBERS_REQUESTED) && i < room->getPeerCount(); i++)
    {
        userList->addMegaHandle(room->getPeerHandle(i));
    }

    mIndexMemberRequested = i;
    mMegaChatApi->loadUserAttributes(mChatId, userList);
    delete userList;
}
