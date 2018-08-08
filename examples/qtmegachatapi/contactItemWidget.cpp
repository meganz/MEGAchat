#include "contactItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMessageBox>
#include <QMenu>

ContactItemWidget::ContactItemWidget(QWidget *parent, MainWindow *mainWin, megachat::MegaChatApi *megaChatApi, mega::MegaApi *megaApi, mega::MegaUser *contact) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    mMainWin = mainWin;
    mMegaApi = megaApi;
    mMegaChatApi = megaChatApi;
    mUserHandle = contact->getHandle();
    const char *contactEmail = contact->getEmail();
    ui->setupUi(this);
    setAvatarStyle();
    ui->mUnreadIndicator->hide();
    QString text = QString::fromUtf8(contactEmail);
    ui->mName->setText(contactEmail);
    ui->mAvatar->setText(QString(text[0].toUpper()));
    const char *firstname = mMainWin->mApp->getFirstname(contact->getHandle());
    if (firstname)
    {
        mMainWin->updateContactFirstname(contact->getHandle(), firstname);
    }
    delete [] firstname;
    int status = this->mMegaChatApi->getUserOnlineStatus(mUserHandle);
    updateOnlineIndicator(status);
}

void ContactItemWidget::setAvatarStyle()
{
    QColor & col = gAvatarColors[mUserHandle & 0x0f];
    QString style = "border-radius: 4px;"
            "border: 2px solid rgba(0,0,0,0);"
            "color: white;"
            "font: 24px;"
            "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,"
            "stop:0 rgba(%1,%2,%3,180), stop:1 rgba(%1,%2,%3,255))";
    style = style.arg(col.red()).arg(col.green()).arg(col.blue());
    ui->mAvatar->setStyleSheet(style);
}

void ContactItemWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    auto chatPeerInviteAction = menu.addAction(tr("Invite to 1on1 chat"));
    connect(chatPeerInviteAction, SIGNAL(triggered()), this, SLOT(onCreatePeerChat()));

    auto chatInviteAction = menu.addAction(tr("Invite to group chat"));
    connect(chatInviteAction, SIGNAL(triggered()), this, SLOT(onCreateGroupChat()));

    auto publicChatInviteAction = menu.addAction(tr("Invite to PUBLIC group chat"));
    connect(publicChatInviteAction, SIGNAL(triggered()), this, SLOT(onCreatePublicGroupChat()));

    auto removeAction = menu.addAction(tr("Remove contact"));
    connect(removeAction, SIGNAL(triggered()), this, SLOT(onContactRemove()));

    auto printAction = menu.addAction(tr("Print contact info"));
    connect(printAction, SIGNAL(triggered()), this, SLOT(onPrintContactInfo()));

    menu.exec(event->globalPos());
    menu.deleteLater();
}

QListWidgetItem *ContactItemWidget::getWidgetItem() const
{
    return mListWidgetItem;
}

void ContactItemWidget::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
}


void ContactItemWidget::updateToolTip(mega::MegaUser *contact)
{
   QString text = NULL;
   std::string contactHandleBin = std::to_string(contact->getHandle());
   const char *email = contact->getEmail();
   const char *chatHandle_64 = "--------";
   const char *contactHandle_64 = mMegaApi->userHandleToBase64(contact->getHandle());
   const char *auxChatHandle_64 = mMegaApi->userHandleToBase64(mMegaChatApi->getChatHandleByUser(contact->getHandle()));

   if (mMegaChatApi->getChatHandleByUser(contact->getHandle()) != megachat::MEGACHAT_INVALID_HANDLE)
   {
      chatHandle_64 = auxChatHandle_64;
   }

   if (contact->getVisibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
   {
        text.append(tr("INVISIBLE:\n"));
   }

   text.append(tr("Email: "))
        .append(QString::fromStdString(email))
        .append(tr("\nUser handle bin: ")).append(contactHandleBin.c_str())
        .append(tr("\nUser handle: ")).append(contactHandle_64)
        .append(tr("\nChat handle: ")).append((chatHandle_64));

   setToolTip(text);
   delete contactHandle_64;
   delete auxChatHandle_64;
}

void ContactItemWidget::onCreatePublicGroupChat()
{
    createChatRoom(mUserHandle, true, true);
}

void ContactItemWidget::onCreateGroupChat()
{
    createChatRoom(mUserHandle, true, false);
}

void ContactItemWidget::onCreatePeerChat()
{
    createChatRoom(mUserHandle, false, false);
}

void ContactItemWidget::createChatRoom(MegaChatHandle uh, bool isGroup, bool isPublic)
{
    QMessageBox msgBox;
    if (isGroup)
    {
        msgBox.setText("Do you want to invite "+ui->mName->text() +" to a new group chat?");
    }
    else
    {
        msgBox.setText("Do you want to invite "+ui->mName->text() +" to a new 1on1 chat?");
    }
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();

    if (ret == QMessageBox::Cancel)
    {
        return;
    }

     MegaChatPeerList *peerList = MegaChatPeerList::createInstance();
     peerList->addPeer(uh, MegaChatRoom::PRIV_STANDARD);

     int i = 0;
     bool exists = false;
     MegaChatListItemList *listItems = mMegaChatApi->getChatListItemsByPeers(peerList);
     while (listItems->size() > i)
     {
          if (listItems->get(i)->isGroup() == isGroup)
          {
             exists = true;
             break;
          }
          i++;
     }

     bool reuseExistingRoom = false;
     if (exists)
     {
         auto item = listItems->get(i);
         QMessageBox msgBoxAns;
         msgBoxAns.setText("Another chatroom with "+ui->mName->text()+" already exists: \""+item->getTitle()+"\".\nDo you want to reuse that room?");
         msgBoxAns.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         int retVal = msgBoxAns.exec();
         if (retVal == QMessageBox::Yes)
         {
             if (item->isArchived())
             {
                 QMessageBox::warning(this, tr("Add chatRoom"), "Chatroom \""+QString(item->getTitle())+"\" is going to be unarchived.");
                 mMegaChatApi->archiveChat(item->getChatId(), false);
             }
             else
             {
                 QMessageBox::warning(this, tr("Add chatRoom"), "Reusing chatroom \""+QString(item->getTitle())+"\"");
             }
             reuseExistingRoom = true;
         }
     }


     if (!reuseExistingRoom)
     {
         char *title = NULL;
         if (isGroup)
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
         delete title;
     }

     delete listItems;
     delete peerList;
}

void ContactItemWidget::onPrintContactInfo()
{
    QMessageBox msg;
    msg.setIcon(QMessageBox::Information);
    msg.setText(toolTip());
    msg.exec();
}

void ContactItemWidget::onContactRemove()
{
    char * email = mMegaChatApi->getContactEmail(mUserHandle);
    mega::MegaUser *contact = mMegaApi->getContact(email);
    QString msg = tr("Are you sure you want to remove ");
    msg.append(ui->mName->text());

    if (ui->mName->text()!= email)
    {
        msg.append(" (").append(email).append(")");
    }
    msg.append(tr(" from your contacts?"));

    auto ret = QMessageBox::question(this, tr("Remove contact"), msg);
    if (ret != QMessageBox::Yes)
        return;

    mMegaApi->removeContact(contact);
    delete email;
    delete contact;
}

void ContactItemWidget::updateTitle(const char * firstname)
{
    QString text;
    if (strcmp(firstname, "") == 0)
    {
        const char *auxEmail = mMegaChatApi->getContactEmail(mUserHandle);
        text = QString::fromUtf8(auxEmail);
        delete auxEmail;
    }
    else
    {
        text = QString::fromUtf8(firstname);
    }

    ui->mName->setText(text);
}

ContactItemWidget::~ContactItemWidget()
{
    delete ui;
}

void ContactItemWidget::updateOnlineIndicator(int newState)
{
    if (newState >= 0 && newState < NINDCOLORS)
    {
        ui->mOnlineIndicator->setStyleSheet(
           QString("background-color: ")+gOnlineIndColors[newState]+
                   ";border-radius: 4px");
    }
}


