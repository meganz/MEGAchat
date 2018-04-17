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
    this->mMegaChatApi->getUserFirstname(contact->getHandle());
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
    auto chatInviteAction = menu.addAction(tr("Invite to group chat"));
    auto removeAction = menu.addAction(tr("Remove contact"));
    connect(chatInviteAction, SIGNAL(triggered()), this, SLOT(onCreateGroupChat()));
    connect(removeAction, SIGNAL(triggered()), this, SLOT(onContactRemove()));
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
        .append(tr("\nUser handle: ")).append(contactHandle_64)
        .append(tr("\nChat handle: ")).append((chatHandle_64));

   setToolTip(text);
   delete contactHandle_64;
   delete auxChatHandle_64;
}

void ContactItemWidget::onCreateGroupChat()
{
   QMessageBox msgBox;
   msgBox.setText("Do you want to invite "+ui->mName->text() +" to a new group chat.");
   msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
   msgBox.setDefaultButton(QMessageBox::Save);
   int ret = msgBox.exec();

   if(ret == QMessageBox::Ok)
   {
        megachat::MegaChatPeerList *peerList;
        peerList = megachat::MegaChatPeerList::createInstance();
        peerList->addPeer(mUserHandle, 2);
        megachat::MegaChatListItemList *listItems = mMegaChatApi->getChatListItemsByPeers(peerList);
        if (listItems)
        {
            QMessageBox msgBoxAns;
            msgBoxAns.setText("You have another chatroom with same participants do you want to reuse it ");
            msgBoxAns.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            int retVal = msgBoxAns.exec();
            if (retVal == QMessageBox::Yes)
            {
                if (listItems->get(0)->isArchived())
                {
                    ChatItemWidget *item = mMainWin->getChatItemWidget(listItems->get(0)->getChatId(), false);
                    item->unarchiveChat();
                    QMessageBox::warning(this, tr("Add chatRoom"), tr("You have unarchived a chatroom to reuse it"));
                }
                else
                {
                    QMessageBox::warning(this, tr("Add chatRoom"), tr("You have decide to reuse the chatroom"));
                }
            }
            else
            {
                this->mMegaChatApi->createChat(true, peerList);
            }
            msgBoxAns.deleteLater();
        }
        else
        {
            this->mMegaChatApi->createChat(true, peerList);
        }
        delete listItems;
   }
   msgBox.deleteLater();
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


