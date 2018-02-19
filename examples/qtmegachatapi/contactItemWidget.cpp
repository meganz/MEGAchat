#include "contactItemWidget.h"
#include "ui_listItemWidget.h"
#include "uiSettings.h"
#include <QMessageBox>
#include <QMenu>

ContactItemWidget::ContactItemWidget(QWidget *parent , megachat::MegaChatApi * megaChatApi, mega::MegaApi * megaApi, megachat::MegaChatHandle userHandle) :
    QWidget(parent),
    ui(new Ui::ChatItem)
{
    this->megaApi = megaApi;
    this->megaChatApi = megaChatApi;
    this->mUserHandle = userHandle;
    const char *contactEmail = megaChatApi->getContactEmail(userHandle);
    megachat::MegaChatRoom * chatRoom = megaChatApi->getChatRoomByUser(userHandle);
    ui->setupUi(this);
    setAvatarStyle();
    ui->mUnreadIndicator->hide();
    QString text = QString::fromUtf8(contactEmail);
    ui->mName->setText(contactEmail);
    ui->mAvatar->setText(QString(text[0].toUpper()));

    delete chatRoom;
    delete contactEmail;
    this->megaChatApi->getUserFirstname(userHandle);
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
    menu.setStyleSheet("background-color: lightgray");
    menu.exec(event->globalPos());
    menu.deleteLater();
}

void ContactItemWidget::updateToolTip(megachat::MegaChatHandle contactHandle)
{
   QString text = NULL;
   char * email = this->megaChatApi->getContactEmail(contactHandle);
   mega::MegaUser* contact = this->megaApi->getContact(email);
   megachat::MegaChatHandle chatHandle = this->megaChatApi->getChatHandleByUser(contactHandle);

   if (contact->getVisibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
        text.append(tr("INVISIBLE:\n"));

   text.append(tr("Email: "))
        .append(QString::fromStdString(email))
        .append(tr("\nUser handle: ")).append(QString::fromStdString(std::to_string(contactHandle)))
        .append(tr("\nChat handle: ")).append(QString::fromStdString(std::to_string(chatHandle)));

   setToolTip(text);
   delete contact;
   delete email;
}

void ContactItemWidget::onCreateGroupChat()
{
   std::string title;
   QString qTitle ;
   QMessageBox msgBox;
   msgBox.setText("Do you want to invite "+ui->mName->text() +" a new group chat.");
   msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
   msgBox.setDefaultButton(QMessageBox::Save);
   int ret = msgBox.exec();

   if(ret == QMessageBox::Ok)
   {
        megachat::MegaChatPeerList *peerList;
        peerList = megachat::MegaChatPeerList::createInstance();
        peerList->addPeer(mUserHandle, 2);
        this->megaChatApi->createChat(true, peerList);
   }
   msgBox.deleteLater();
}

void ContactItemWidget::onContactRemove()
{
    char * email = megaChatApi->getContactEmail(mUserHandle);
    mega::MegaUser *contact = megaApi->getContact(email);
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

    megaApi->removeContact(contact);
    delete email;
    delete contact;
}


void ContactItemWidget::updateTitle(const char * firstname)
{
    QString text = QString::fromUtf8(firstname);
    ui->mName->setText(text);
}

ContactItemWidget::~ContactItemWidget()
{
    delete ui;
}

void ContactItemWidget::updateOnlineIndicator(int newState)
{
    if (newState>0 && newState <NINDCOLORS)
    {
        ui->mOnlineIndicator->setStyleSheet(
           QString("background-color: ")+gOnlineIndColors[newState]+
                   ";border-radius: 4px");
    }
}


