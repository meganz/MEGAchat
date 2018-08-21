#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTMegaChatEvent.h>
#include "uiSettings.h"
#include "chatSettings.h"

using namespace mega;
using namespace megachat;

MainWindow::MainWindow(QWidget *parent, MegaLoggerApplication *logger, megachat::MegaChatApi *megaChatApi, mega::MegaApi *megaApi) :
    QMainWindow(0),
    ui(new Ui::MainWindow)
{
    mApp = (MegaChatApplication *) parent;
    nContacts = 0;
    activeChats = 0;
    archivedChats = 0;
    inactiveChats = 0;
    ui->setupUi(this);
    ui->contactList->setSelectionMode(QAbstractItemView::NoSelection);
    mMegaChatApi = megaChatApi;
    mMegaApi = megaApi;
    onlineStatus = NULL;
    mShowInactive = false;
    mShowArchived = false;
    mLogger = logger;
    mWebRTCSettings = new WebRTCSettings();
    qApp->installEventFilter(this);

    megaChatListenerDelegate = new QTMegaChatListener(mMegaChatApi, this);
    mMegaChatApi->addChatListener(megaChatListenerDelegate);

#ifndef KARERE_DISABLE_WEBRTC
    megaChatCallListenerDelegate = new megachat::QTMegaChatCallListener(mMegaChatApi, this);
    mMegaChatApi->addChatCallListener(megaChatCallListenerDelegate);
#elif
    megaChatCallListenerDelegate = NULL;
#endif
}

MainWindow::~MainWindow()
{
    mMegaChatApi->removeChatListener(megaChatListenerDelegate);
#ifndef KARERE_DISABLE_WEBRTC
    mMegaChatApi->removeChatCallListener(megaChatCallListenerDelegate);
#endif

    for (auto it = mLocalChatListItems.begin(); it != mLocalChatListItems.end(); it++)
    {
        delete it->second;
    }
    mLocalChatListItems.clear();

    delete megaChatListenerDelegate;
    delete megaChatCallListenerDelegate;
    delete mWebRTCSettings;
    chatWidgets.clear();
    contactWidgets.clear();
    delete ui;
}

void MainWindow::updateToolTipMyInfo(megachat::MegaChatHandle myHandle)
{
    QString text = NULL;
    std::string myHandleBin = std::to_string(myHandle);
    const char *myHandle_64 = mMegaApi->userHandleToBase64(myHandle);
    text.append("\nMy User handle Bin: ");
    text.append(myHandleBin.c_str());
    text.append("\nMy User handle B64: ");
    text.append(QString::fromStdString(myHandle_64));
    ui->bOnlineStatus->setToolTip(text);
    delete myHandle_64;
}

mega::MegaUserList * MainWindow::getUserContactList()
{
    return mMegaApi->getContacts();
}

#ifndef KARERE_DISABLE_WEBRTC
void MainWindow::onChatCallUpdate(megachat::MegaChatApi */*api*/, megachat::MegaChatCall *call)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itWidgets = chatWidgets.find(call->getChatid());
    if(itWidgets == chatWidgets.end())
    {
        throw std::runtime_error("Incoming call from unknown contact");
    }

    ChatItemWidget *chatItemWidget = itWidgets->second;
    const MegaChatListItem *auxItem = getLocalChatListItem(call->getChatid());
    const char *chatWindowTitle = auxItem->getTitle();

    ChatWindow *auxChatWindow = NULL;

    if (!chatItemWidget->getChatWindow())
    {
        megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(call->getChatid());
        auxChatWindow = new ChatWindow(this, mMegaChatApi, chatRoom->copy(), chatWindowTitle);
        chatItemWidget->setChatWindow(auxChatWindow);
        auxChatWindow->show();
        auxChatWindow->openChatRoom();
        delete chatRoom;
    }
    else
    {
        auxChatWindow =chatItemWidget->getChatWindow();
        auxChatWindow->show();
        auxChatWindow->setWindowState(Qt::WindowActive);
    }

    switch(call->getStatus())
    {
        case megachat::MegaChatCall::CALL_STATUS_TERMINATING:
           {
               ChatItemWidget *chatItemWidget = this->getChatItemWidget(call->getChatid(),false);
               chatItemWidget->getChatWindow()->hangCall();
               return;
           }
           break;
        case megachat::MegaChatCall::CALL_STATUS_RING_IN:
           {
              ChatWindow *auxChatWindow =chatItemWidget->getChatWindow();
              if(auxChatWindow->getCallGui()==NULL)
              {
                 auxChatWindow->createCallGui(call->hasRemoteVideo());
              }
           }
           break;
        case megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS:
           {
               ChatWindow *auxChatWindow =chatItemWidget->getChatWindow();
               if ((auxChatWindow->getCallGui()) && !(auxChatWindow->getCallGui()->getCall()))
               {
                   auxChatWindow->connectCall();
               }

               if (call->hasChanged(MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS))
               {
                    CallGui *callGui = auxChatWindow->getCallGui();
                    if(call->hasRemoteVideo())
                    {
                        callGui->ui->remoteRenderer->disableStaticImage();
                    }
                    else
                    {
                        callGui->setAvatarOnRemote();
                        callGui->ui->remoteRenderer->enableStaticImage();
                    }
               }
           }
           break;
    }
}
#endif
void MainWindow::clearContactChatList()
{
    ui->contactList->clear();
    chatWidgets.clear();
    contactWidgets.clear();
}

void MainWindow::orderContactChatList()
{
    auxChatWidgets = chatWidgets;
    clearContactChatList();

    // add items to the list
    addContacts();
    if (mShowArchived)
    {
        addArchivedChats();
    }
    if(mShowInactive)
    {
        addInactiveChats();
    }
    addActiveChats();

    auxChatWidgets.clear();

    // prepare tag to indicate chatrooms shown
    QString text;
    if (mShowArchived && mShowInactive)
    {
        text.append(" Showing <all> chatrooms");
    }
    else if (mShowArchived)
    {
        text.append(" Showing <active+archived> chatrooms");
    }
    else if (mShowInactive)
    {
        text.append(" Showing <active+inactive> chatrooms");
    }
    else
    {
        text.append(" Showing <active> chatrooms");
    }
    ui->mOnlineStatusDisplay->setText(text);
}

void MainWindow::addContacts()
{
    MegaUser *contact = NULL;
    MegaUserList *contactList = mMegaApi->getContacts();
    setNContacts(contactList->size());

    for (int i = 0; i < contactList->size(); i++)
    {
        contact = contactList->get(i);
        mega::MegaHandle userHandle = contact->getHandle();
        if (userHandle != mMegaChatApi->getMyUserHandle())
        {
            if (contact->getVisibility() == MegaUser::VISIBILITY_HIDDEN && mShowInactive != true)
            {
                continue;
            }
            addContact(contact);
        }
    }
    delete contactList;
}

void MainWindow::addArchivedChats()
{
    std::list<Chat> *archivedChatList = getLocalChatListItemsByStatus(chatArchivedStatus);
    archivedChatList->sort();
    for (Chat &chat : (*archivedChatList))
    {
        addChat(chat.chatItem);
    }
    delete archivedChatList;
}

void MainWindow::addInactiveChats()
{
    std::list<Chat> *inactiveChatList = getLocalChatListItemsByStatus(chatInactiveStatus);
    inactiveChatList->sort();
    for (Chat &chat : (*inactiveChatList))
    {
        addChat(chat.chatItem);
    }
    delete inactiveChatList;
}

void MainWindow::addActiveChats()
{
    std::list<Chat> *activeChatList = getLocalChatListItemsByStatus(chatActiveStatus);
    activeChatList->sort();
    for (Chat &chat : (*activeChatList))
    {
        addChat(chat.chatItem);
    }
    delete activeChatList;
}

bool MainWindow::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease
            && mMegaChatApi->getInitState() == MegaChatApi::INIT_ONLINE_SESSION
            && mMegaChatApi->isSignalActivityRequired())
    {
        mMegaChatApi->signalPresenceActivity();
    }
    return false;
}

void MainWindow::on_bSettings_clicked()
{
    QMenu menu(this);
    menu.setAttribute(Qt::WA_DeleteOnClose);

    auto actInactive = menu.addAction(tr("Show/Hide inactive chats"));
    connect(actInactive, SIGNAL(triggered()), this, SLOT(onShowInactiveChats()));

    auto actArchived = menu.addAction(tr("Show/Hide archived chats"));
    connect(actArchived, SIGNAL(triggered()), this, SLOT(onShowArchivedChats()));

    menu.addSeparator();

    auto addAction = menu.addAction(tr("Add user to contacts"));
    connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));

    auto actPeerChat = menu.addAction(tr("Create 1on1 chat"));
    connect(actPeerChat, SIGNAL(triggered()), this, SLOT(onAddPeerChatGroup()));

    auto actGroupChat = menu.addAction(tr("Create group chat"));
    connect(actGroupChat, SIGNAL(triggered()), this, SLOT(onAddGroupChat()));

    auto actPubChat = menu.addAction(tr("Create public chat"));
    connect(actPubChat, SIGNAL(triggered()), this, SLOT(onAddPubChatGroup()));

    auto actLoadLink = menu.addAction(tr("Preview chat-link"));
    connect(actLoadLink, SIGNAL(triggered()), this, SLOT(loadChatLink()));

    menu.addSeparator();

    auto actWebRTC = menu.addAction(tr("Set audio/video input devices"));
    connect(actWebRTC, SIGNAL(triggered()), this, SLOT(onWebRTCsetting()));

    QPoint pos = ui->bSettings->pos();
    pos.setX(pos.x() + ui->bSettings->width());
    pos.setY(pos.y() + ui->bSettings->height());
    menu.exec(mapToGlobal(pos));
}

void MainWindow::createWebRTCSettingsDialog()
{
    WebRTCSettingsDialog *chatSettings = new WebRTCSettingsDialog(this, mWebRTCSettings);
    chatSettings->exec();
    chatSettings->deleteLater();
}

void MainWindow::on_bOnlineStatus_clicked()
{
    onlineStatus = new QMenu(this);
    auto actOnline = onlineStatus->addAction("Online");
    actOnline->setData(QVariant(MegaChatApi::STATUS_ONLINE));
    connect(actOnline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actAway = onlineStatus->addAction("Away");
    actAway->setData(QVariant(MegaChatApi::STATUS_AWAY));
    connect(actAway, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actDnd = onlineStatus->addAction("Busy");
    actDnd->setData(QVariant(MegaChatApi::STATUS_BUSY));
    connect(actDnd, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actOffline = onlineStatus->addAction("Offline");
    actOffline->setData(QVariant(MegaChatApi::STATUS_OFFLINE));
    connect(actOffline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto rect = ui->bOnlineStatus->rect();
    onlineStatus->move(mapToGlobal(QPoint(1,rect.bottom())));
    onlineStatus->resize(rect.width(), 100);
    onlineStatus->setStyleSheet("QMenu {"
        "background-color: qlineargradient("
        "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}"
        "QMenu::item:!selected{"
            "color: white;"
        "}"
        "QMenu::item:selected{"
            "background-color: qlineargradient("
            "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}");
    onlineStatus->exec();
    onlineStatus->deleteLater();
}

ChatItemWidget *MainWindow::getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    if (!reorder)
    {
        itChats = chatWidgets.find(chatHandle);
        if (itChats == chatWidgets.end())
        {
            return NULL;
        }
    }
    else
    {
        itChats = auxChatWidgets.find(chatHandle);
        if (itChats == auxChatWidgets.end())
        {
            return NULL;
        }
    }
    return itChats->second;
}

void MainWindow::addContact(MegaUser *contact)
{
    int index = -(archivedChats + nContacts);
    nContacts += 1;
    ContactItemWidget *contactItemWidget = new ContactItemWidget(ui->contactList, this, mMegaChatApi, mMegaApi, contact);
    contactItemWidget->updateToolTip(contact);
    QListWidgetItem *item = new QListWidgetItem();
    contactItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    ui->contactList->insertItem(index, item);
    ui->contactList->setItemWidget(item, contactItemWidget);
    contactWidgets.insert(std::pair<mega::MegaHandle, ContactItemWidget *>(contact->getHandle(),contactItemWidget));
}


void MainWindow::addChat(const MegaChatListItem* chatListItem)
{
    int index = 0;
    if (chatListItem->isArchived())
    {
        index = -(archivedChats);
        archivedChats += 1;
    }
    else if (!chatListItem->isActive())
    {
        index = -(nContacts + archivedChats + inactiveChats);
        inactiveChats += 1;
    }
    else
    {
        index = -(activeChats + inactiveChats + archivedChats+nContacts);
        activeChats += 1;
    }

    megachat::MegaChatHandle chathandle = chatListItem->getChatId();
    ChatItemWidget *chatItemWidget = new ChatItemWidget(this, mMegaChatApi, chatListItem);
    chatItemWidget->updateToolTip(chatListItem, NULL);
    QListWidgetItem *item = new QListWidgetItem();
    chatItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    chatWidgets.insert(std::pair<megachat::MegaChatHandle, ChatItemWidget *>(chathandle,chatItemWidget));
    ui->contactList->insertItem(index, item);
    ui->contactList->setItemWidget(item, chatItemWidget);

    ChatItemWidget *auxChatItemWidget = getChatItemWidget(chathandle, true);
    if(auxChatItemWidget)
    {
        chatItemWidget->mChatWindow = auxChatItemWidget->mChatWindow;
        auxChatItemWidget->deleteLater();
    }
}

void MainWindow::onChatListItemUpdate(MegaChatApi *, MegaChatListItem *item)
{
    updateLocalChatListItem(item);

    megachat::MegaChatHandle chatid = item->getChatId();
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    itChats = chatWidgets.find(chatid);
    if (itChats != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = itChats->second;
        int change = item->getChanges();
        switch (change)
        {
            //Last Message update
            case (megachat::MegaChatListItem::CHANGE_TYPE_LAST_MSG):
            {
                chatItemWidget->updateToolTip(item, NULL);
                break;
            }

            //Unread count update
            case (megachat::MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT):
            {
                chatItemWidget->onUnreadCountChanged(item->getUnreadCount());
                break;
            }

            //Title update
            case (megachat::MegaChatListItem::CHANGE_TYPE_TITLE):
            {
                chatItemWidget->onTitleChanged(item->getTitle());
                break;
            }

            //Own priv update
            case (megachat::MegaChatListItem::CHANGE_TYPE_OWN_PRIV):
            {
                chatItemWidget->updateToolTip(item, NULL);
                break;
            }

            //Participants update
            case (megachat::MegaChatListItem::CHANGE_TYPE_PARTICIPANTS):
            {
                chatItemWidget->updateToolTip(item, NULL);
                orderContactChatList();
                break;
            }

            //The chatroom has been left by own user
            case (megachat::MegaChatListItem::CHANGE_TYPE_CLOSED):
            {
                chatItemWidget->showAsHidden();
                break;
            }

            //Timestamp of the last activity update
            case (megachat::MegaChatListItem::CHANGE_TYPE_LAST_TS):
            {
                orderContactChatList();
                break;
            }

            //The chatroom is private now
            case (megachat::MegaChatListItem::CHANGE_TYPE_CHAT_MODE):
            {
                orderContactChatList();
                break;
            }

            //The Chatroom has been un/archived
            case (megachat::MegaChatListItem::CHANGE_TYPE_ARCHIVE):
            {
                if (!mShowArchived)
                {
                    ChatItemWidget *auxChatItemWidget = getChatItemWidget(chatid, false);
                    if(auxChatItemWidget)
                    {
                        if (auxChatItemWidget->mChatWindow)
                        {
                            auxChatItemWidget->mChatWindow->deleteLater();
                            auxChatItemWidget->invalidChatWindowHandle();
                        }
                    }
                }
                orderContactChatList();
                break;
            }
        }
    }
    else    // new chatroom
    {
        if (!item->isArchived() && item->isActive())
        {
            orderContactChatList();
        }
    }
}

void MainWindow::onShowInactiveChats()
{
    mShowInactive = !mShowInactive;
    orderContactChatList();
}

void MainWindow::onShowArchivedChats()
{
    mShowArchived = !mShowArchived;
    orderContactChatList();
}

void MainWindow::onAddPeerChatGroup()
{
    mega::MegaUserList *list = mMegaApi->getContacts();
    CreateChatDialog *chatDialog = new CreateChatDialog(this, mMegaChatApi, false, false);
    chatDialog->createChatList(list);
    chatDialog->show();
    delete list;
}

void MainWindow::onAddGroupChat()
{
    mega::MegaUserList *list = mMegaApi->getContacts();
    CreateChatDialog *chatDialog = new CreateChatDialog(this, mMegaChatApi, true, false);
    chatDialog->createChatList(list);
    chatDialog->show();
    delete list;
}

void MainWindow::onAddPubChatGroup()
{
    mega::MegaUserList *list = mMegaApi->getContacts();
    CreateChatDialog *chatDialog = new CreateChatDialog(this, mMegaChatApi, true, true);
    chatDialog->createChatList(list);
    chatDialog->show();
    delete list;
}

void MainWindow::onWebRTCsetting()
{
#ifndef KARERE_DISABLE_WEBRTC
    mMegaChatApi->loadAudioVideoDeviceList();
#endif
}

void MainWindow::loadChatLink()
{
    std::string chatLink;
    QString qChatLink = QInputDialog::getText(this, tr("Load chat link"), tr("Enter a valid chatlink"));
    if (!qChatLink.isNull())
    {
        chatLink = qChatLink.toStdString();
        if (chatLink.empty())
        {
            QMessageBox::warning(this, tr("Load chat link"), tr("You can't enter an empty chatlink"));
            return;
        }
        mMegaChatApi->loadChatLink(chatLink.c_str());
    }
}

void MainWindow::onAddContact()
{
    QString email = QInputDialog::getText(this, tr("Add contact"), tr("Please enter the email of the user to add"));
    if (email.isNull())
        return;

    char *myEmail = mMegaApi->getMyEmail();
    QString qMyEmail = myEmail;
    delete [] myEmail;

    if (email == qMyEmail)
    {
        QMessageBox::critical(this, tr("Add contact"), tr("You can't add your own email as contact"));
        return;
    }
    std::string emailStd = email.toStdString();
    mMegaApi->inviteContact(emailStd.c_str(),tr("I'd like to add you to my contact list").toUtf8().data(), MegaContactRequest::INVITE_ACTION_ADD);
}

void MainWindow::setOnlineStatus()
{
    auto action = qobject_cast<QAction*>(QObject::sender());
    assert(action);
    bool ok;
    auto pres = action->data().toUInt(&ok);
    if (!ok || (pres == MegaChatApi::STATUS_INVALID))
    {
        return;
    }
    mMegaChatApi->setOnlineStatus(pres);
}

void MainWindow::onChatConnectionStateUpdate(MegaChatApi *, MegaChatHandle chatid, int newState)
{
    if (chatid == megachat::MEGACHAT_INVALID_HANDLE)
    {
        updateLocalChatListItems();
        orderContactChatList();
        megachat::MegaChatPresenceConfig *presenceConfig = mMegaChatApi->getPresenceConfig();
        if (presenceConfig)
        {
            onChatPresenceConfigUpdate(mMegaChatApi, presenceConfig);
        }
        delete presenceConfig;
        return;
    }

    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it = chatWidgets.find(chatid);
    if (it != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = it->second;
        chatItemWidget->onlineIndicatorUpdate(newState);
    }
}

void MainWindow::onChatInitStateUpdate(megachat::MegaChatApi *, int newState)
{
    if (newState == MegaChatApi::INIT_ERROR)
    {
        QMessageBox msgBox;
        msgBox.setText("Critical error in MEGAchat. The application will close now. If the problem persists, you can delete your cached sessions.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Ok)
        {
            deleteLater();
            return;
        }
    }

    if (newState == MegaChatApi::INIT_ONLINE_SESSION || newState == MegaChatApi::INIT_OFFLINE_SESSION)
    {
        if(!isVisible())
        {
            mApp->resetLoginDialog();
            show();
            updateLocalChatListItems();
        }

        if (newState == MegaChatApi::INIT_ONLINE_SESSION)
        {
            // contacts are not loaded until MegaApi::login completes
            orderContactChatList();
        }

        QString auxTitle(mMegaChatApi->getMyEmail());
        if (mApp->sid() && newState == MegaChatApi::INIT_OFFLINE_SESSION)
        {
            auxTitle.append(" [OFFLINE MODE]");
        }
        setWindowTitle(auxTitle);
    }
}

void MainWindow::onChatOnlineStatusUpdate(MegaChatApi *, MegaChatHandle userhandle, int status, bool inProgress)
{
    if (status == megachat::MegaChatApi::STATUS_INVALID)
        status = 0;

    if (this->mMegaChatApi->getMyUserHandle() == userhandle && !inProgress)
    {
        ui->bOnlineStatus->setText(kOnlineSymbol_Set);
        if (status >= 0 && status < NINDCOLORS)
            ui->bOnlineStatus->setStyleSheet(kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
    }
    else
    {
        std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
        itContacts = this->contactWidgets.find((mega::MegaHandle) userhandle);
        if (itContacts != contactWidgets.end())
        {
            ContactItemWidget * contactItemWidget = itContacts->second;
            assert(!inProgress);
            contactItemWidget->updateOnlineIndicator(status);
        }
    }
}

void MainWindow::onChatPresenceConfigUpdate(MegaChatApi *, MegaChatPresenceConfig *config)
{
    int status = config->getOnlineStatus();
    if (status == megachat::MegaChatApi::STATUS_INVALID)
        status = 0;

    ui->bOnlineStatus->setText(config->isPending()
        ? kOnlineSymbol_InProgress
        : kOnlineSymbol_Set);

    ui->bOnlineStatus->setStyleSheet(
        kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
}

int MainWindow::getNContacts() const
{
    return nContacts;
}

void MainWindow::setNContacts(int nContacts)
{
    this->nContacts = nContacts;
}

void MainWindow::createChatRoom(MegaChatPeerList *peerList, bool isGroup, bool isPublic)
{
    // check if another chatroom with same characteristics already exists
     unsigned int i = 0;
     const MegaChatListItem *similarChat = NULL;
     MegaChatListItemList *listItems = mMegaChatApi->getChatListItemsByPeers(peerList);
     while (listItems->size() > i)
     {
         const MegaChatListItem *item = listItems->get(i);
         if (item->isGroup() == isGroup && item->isPublic() == isPublic)
         {
             similarChat = item;
             break;
         }
         i++;
     }

     // check if user wants to reuse existing chatroom, if any
     bool reuseExistingRoom = false;
     if (similarChat)
     {
         QMessageBox msgBoxAns;
         msgBoxAns.setText("Another chatroom with same characteristics already exists: \""+QString(similarChat->getTitle())+"\".\nDo you want to reuse that room?");
         msgBoxAns.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         if (msgBoxAns.exec() == QMessageBox::Yes)
         {
             if (similarChat->isArchived())
             {
                 QMessageBox::warning(this, tr("Add chatRoom"), "Chatroom \""+QString(similarChat->getTitle())+"\" is going to be unarchived.");
                 mMegaChatApi->archiveChat(similarChat->getChatId(), false);
             }
             else
             {
                 QMessageBox::warning(this, tr("Add chatRoom"), "Reusing chatroom \""+QString(similarChat->getTitle())+"\"");
             }
             reuseExistingRoom = true;
         }
     }
     delete listItems;

     if (!reuseExistingRoom)
     {
         char *title = NULL;
         if (isGroup && isPublic)
         {
             QString qTitle = QInputDialog::getText(this, tr("Set chat topic"), tr("Leave blank for default title"));
             if (!qTitle.isNull() && !qTitle.isEmpty())
             {
                 title = strdup(qTitle.toStdString().c_str());
             }
         }

         if (isPublic)
         {
             mMegaChatApi->createPublicChat(peerList, title);
         }
         else
         {
             mMegaChatApi->createChat(isGroup, peerList, title);
         }
     }
}

void MainWindow::updateMessageFirstname(MegaChatHandle contactHandle, const char *firstname)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    for (it = chatWidgets.begin(); it != chatWidgets.end(); it++)
    {
        const MegaChatListItem *chatListItem  = getLocalChatListItem(it->first);
        if (chatListItem)
        {
            ChatItemWidget *chatItemWidget = it->second;
            if (chatListItem->getLastMessageSender() == contactHandle)
            {
                chatItemWidget->updateToolTip(chatListItem, firstname);
            }

            ChatWindow *chatWindow = chatItemWidget->getChatWindow();
            if (chatWindow)
            {
                chatWindow->updateMessageFirstname(contactHandle, firstname);
            }
        }
    }
}

void MainWindow::updateLocalChatListItems()
{
    for (auto it = mLocalChatListItems.begin(); it != mLocalChatListItems.end(); it++)
    {
        delete it->second;
    }
    mLocalChatListItems.clear();

    //Add all active chatListItems
    MegaChatListItemList *chatList = mMegaChatApi->getActiveChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addLocalChatListItem(chatList->get(i));
    }
    delete chatList;

    //Add inactive chatListItems
    chatList = mMegaChatApi->getInactiveChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addLocalChatListItem(chatList->get(i));
    }
    delete chatList;

    //Add archived chatListItems
    chatList = mMegaChatApi->getArchivedChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addLocalChatListItem(chatList->get(i));
    }
    delete chatList;
}

void MainWindow::addLocalChatListItem(const MegaChatListItem *item)
{
    mLocalChatListItems.insert(std::pair<megachat::MegaChatHandle, const MegaChatListItem *>(item->getChatId(),item->copy()));
}

void MainWindow::removeLocalChatListItem(MegaChatListItem *item)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(item->getChatId());
    if (itItem != mLocalChatListItems.end())
    {
        const megachat::MegaChatListItem *auxItem = itItem->second;
        mLocalChatListItems.erase(itItem);
        delete auxItem;
    }
    orderContactChatList();
}

const megachat::MegaChatListItem *MainWindow::getLocalChatListItem(megachat::MegaChatHandle chatId)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(chatId);
    if (itItem != mLocalChatListItems.end())
    {
        return itItem->second;
    }
    return NULL;
}

void MainWindow::updateLocalChatListItem(MegaChatListItem *item)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(item->getChatId());
    if (itItem != mLocalChatListItems.end())
    {
        const megachat::MegaChatListItem *auxItem = itItem->second;
        mLocalChatListItems.erase(itItem);
        addLocalChatListItem(item);
        delete auxItem;
    }
}

std::list<Chat> *MainWindow::getLocalChatListItemsByStatus(int status)
{
    std::list<Chat> *chatList = new std::list<Chat>;
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator it;

    for (it = mLocalChatListItems.begin(); it != mLocalChatListItems.end(); it++)
    {
        const megachat::MegaChatListItem *item = it->second;
        switch (status)
        {
            case chatActiveStatus:
                if (item->isActive() && !item->isArchived())
                {
                    chatList->push_back(Chat(item));
                }
                break;

            case chatInactiveStatus:
                if (!item->isActive())
                {
                    chatList->push_back(Chat(item));
                }
                break;

            case chatArchivedStatus:
                if (item->isArchived())
                {
                    chatList->push_back(Chat(item));
                }
                break;
        }
    }
    return chatList;
}


void MainWindow::updateContactFirstname(MegaChatHandle contactHandle, const char * firstname)
{
    std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
    itContacts = contactWidgets.find(contactHandle);

    if (itContacts != contactWidgets.end())
    {
        ContactItemWidget *contactItemWidget = itContacts->second;
        contactItemWidget->updateTitle(firstname);
    }
}

void MainWindow::on_mLogout_clicked()
{
    QMessageBox msgBox;
    msgBox.setText("Do you want to logout?");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Ok)
    {
        mMegaApi->logout();
    }
}
