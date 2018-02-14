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

MainWindow::MainWindow(QWidget *parent, MegaLoggerApplication *mLogger) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    nContacts = 0;
    activeChats = 0;
    inactiveChats = 0;
    ui->setupUi(this);
    megaChatApi = NULL;
    megaApi = NULL;
    megaChatListenerDelegate = NULL;
    onlineStatus = NULL;
    chatsVisibility = true;
    logger=mLogger;
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    if (megaChatListenerDelegate)
        delete megaChatListenerDelegate;
    chatWidgets.clear();
    contactWidgets.clear();
    delete ui;
}

mega::MegaUserList * MainWindow::getUserContactList()
{
    return megaApi->getContacts();
}

void MainWindow::setMegaChatApi(megachat::MegaChatApi *megaChatApi)
{
    this->megaChatApi = megaChatApi;
}

void MainWindow::setMegaApi(MegaApi *megaApi)
{
    this->megaApi = megaApi;        
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setAttribute(Qt::WA_DeleteOnClose);
    auto addAction = menu.addAction(tr("Add user to contacts"));
    connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));

    auto actVisibility = menu.addAction(tr("Show/Hide leaved chats"));
    connect(actVisibility, SIGNAL(triggered()), this, SLOT(onChangeChatVisibility()));

    menu.setStyleSheet("background-color: lightgray");
    menu.exec(event->globalPos());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove && this->megaChatApi->isSignalActivityRequired())
    {
        this->megaChatApi->signalPresenceActivity();
    }
    return false;
}

void MainWindow::on_bSettings_clicked()
{
    ChatSettings *chatSettings = new ChatSettings(this);
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


void MainWindow::addContact(MegaChatHandle contactHandle)
{
    ContactItemWidget *contactItemWidget = new ContactItemWidget(ui->contactList, megaChatApi, megaApi, contactHandle);
    contactItemWidget->updateToolTip(contactHandle);
    QListWidgetItem *item = new QListWidgetItem();
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    ui->contactList->insertItem(0, item);
    ui->contactList->setItemWidget(item, contactItemWidget);
    contactWidgets.insert(std::pair<mega::MegaHandle, ContactItemWidget *>(contactHandle,contactItemWidget));
}


void MainWindow::addChat(const MegaChatListItem* chatListItem)
{
    int index = 0;
    if(chatListItem->isActive())
    {
        index = -activeChats;
        activeChats +=1;
    }
    else
    {
        index = (activeChats+inactiveChats+nContacts);
        inactiveChats +=1;
    }

    megachat::MegaChatHandle chathandle = chatListItem->getChatId();
    ChatItemWidget *chatItemWidget = new ChatItemWidget(this, megaChatApi, chatListItem);
    chatItemWidget->updateToolTip(megaChatApi, chatListItem);
    QListWidgetItem *item = new QListWidgetItem();
    chatItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    ui->contactList->insertItem(index, item);
    ui->contactList->setItemWidget(item, chatItemWidget);
    chatWidgets.insert(std::pair<megachat::MegaChatHandle, ChatItemWidget *>(chathandle,chatItemWidget));
}

void MainWindow::onChatListItemUpdate(MegaChatApi* api, MegaChatListItem *item)
{
    int change = item->getChanges();
    megachat::MegaChatHandle chatHandle = item->getChatId();
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    itChats = chatWidgets.find(chatHandle);

    if (itChats != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = itChats->second;
        switch (change)
        {
            //Last Message update
            case (megachat::MegaChatListItem::CHANGE_TYPE_LAST_MSG):
                {
                    chatItemWidget->updateToolTip(api, item);
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
                    chatItemWidget->updateToolTip(api, item);
                    break;
                }
            //Participants update
            case (megachat::MegaChatListItem::CHANGE_TYPE_PARTICIPANTS):
                {
                    chatItemWidget->updateToolTip(api, item);
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
                    break;
                }
        }
     }
}

void MainWindow::onChangeChatVisibility()
{
    chatsVisibility = !chatsVisibility;
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    bool active;
    for (itChats = chatWidgets.begin(); itChats != chatWidgets.end(); ++itChats)
    {
        active = megaChatApi->getChatListItem(itChats->first)->isActive();
        if(!active)
        {
            if(chatsVisibility)
            {
                itChats->second->show();
                itChats->second->setEnabled(true);
            }
            else
            {
                itChats->second->hide();
                itChats->second->setEnabled(false);
            }
        }
    }
}

void MainWindow::onAddContact()
{
    QString email = QInputDialog::getText(this, tr("Add contact"), tr("Please enter the email of the user to add"));
    if (email.isNull())
        return;

    char *myEmail = megaApi->getMyEmail();
    QString qMyEmail = myEmail;
    delete [] myEmail;

    if (email == qMyEmail)
    {
        QMessageBox::critical(this, tr("Add contact"), tr("You can't add your own email as contact"));
        return;
    }
    auto utf8 = email.toUtf8();
    megaApi->inviteContact(email.toStdString().c_str(),tr("I'd like to add you to my contact list").toUtf8().data(), MegaContactRequest::INVITE_ACTION_ADD);
}



void MainWindow::setOnlineStatus()
{
    auto action = qobject_cast<QAction*>(QObject::sender());
    assert(action);
    bool ok;
    auto pres = action->data().toUInt(&ok);
    if (!ok || (pres ==MegaChatApi::STATUS_INVALID))
    {
        return;
    }
    this->megaChatApi->setOnlineStatus(pres);
}

void MainWindow::addChatListener()
{
    megaChatListenerDelegate = new QTMegaChatListener(megaChatApi, this);
    megaChatApi->addChatListener(megaChatListenerDelegate);
}


void MainWindow::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int newState)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    it = chatWidgets.find(chatid);

    if (it != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = it->second;
        chatItemWidget->onlineIndicatorUpdate(newState);
    }
}

void MainWindow::onChatInitStateUpdate(megachat::MegaChatApi* api, int newState)
{
    if (!this->isVisible() && (newState == MegaChatApi::INIT_OFFLINE_SESSION
       || newState == MegaChatApi::INIT_ONLINE_SESSION))
    {
       this->show();
    }
    else if (newState == MegaChatApi::INIT_NO_CACHE)
    {
       this->hide();
       Q_EMIT esidLogout();
    }

    if (newState == MegaChatApi::INIT_OFFLINE_SESSION ||
        newState == MegaChatApi::INIT_ONLINE_SESSION)
    {
        setWindowTitle(api->getMyEmail());
    }
    else
    {
        setWindowTitle("");
    }
}

void MainWindow::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress)
{
    if (this->megaChatApi->getMyUserHandle() == userhandle && !inProgress)
    {
        ui->bOnlineStatus->setText(kOnlineSymbol_Set);
        if (status >0 && status<NINDCOLORS)
            ui->bOnlineStatus->setStyleSheet(kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
    }
    else
    {
        std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
        itContacts = this->contactWidgets.find((mega::MegaHandle) userhandle);
        if (itContacts != contactWidgets.end())
        {
            ContactItemWidget * contactItemWidget = itContacts->second;
            if(inProgress)
                contactItemWidget->updateOnlineIndicator(0);
            else
                contactItemWidget->updateOnlineIndicator(status);
        }
    }
}


void MainWindow::onChatPresenceConfigUpdate(MegaChatApi* api, MegaChatPresenceConfig *config)
{
    ui->bOnlineStatus->setText(config->isPending()
        ?kOnlineSymbol_InProgress
        :kOnlineSymbol_Set);

    ui->bOnlineStatus->setStyleSheet(
        kOnlineStatusBtnStyle.arg(gOnlineIndColors[config->getOnlineStatus()]));
}

int MainWindow::getNContacts() const
{
    return nContacts;
}

void MainWindow::setNContacts(int nContacts)
{
    this->nContacts = nContacts;
}


void MainWindow::updateContactFirstname(MegaChatHandle contactHandle, const char * firstname)
{
    std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
    itContacts = contactWidgets.find(contactHandle);

    if (itContacts != contactWidgets.end())
    {                
        ContactItemWidget * contactItemWidget = itContacts->second;
        contactItemWidget->updateTitle(firstname);
    }
}


