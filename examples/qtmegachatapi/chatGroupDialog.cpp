#include "chatGroupDialog.h"
#include "ui_chatGroupDialog.h"
#include <QMessageBox>
#include "chatItemWidget.h"

CreateChatDialog::CreateChatDialog(QWidget *parent, megachat::MegaChatApi *megaChatApi, bool aGroup, bool aPub) :
    QDialog(parent),
    ui(new Ui::ChatGroupDialog)
{
    mMainWin = (MainWindow *) parent;
    mMegaChatApi = megaChatApi;
    ui->setupUi(this);
    mPublic = aPub;
    mGroup = aGroup;

    if (mPublic)
    {
        setWindowTitle("Create public chat");
    }
    else if (mGroup)
    {
        setWindowTitle("Create private group chat");
    }
    else
    {
        setWindowTitle("Create private group chat");
    }
}

CreateChatDialog::~CreateChatDialog()
{
    delete ui;
}

void CreateChatDialog::createChatList(mega::MegaUserList *contactList)
{
    for (int i = 0; i < contactList->size(); i++)
    {
        ui->mListWidget->addItem(contactList->get(i)->getEmail());
    }

    QListWidgetItem *item = 0;
    for (int i = 0; i < ui->mListWidget->count(); ++i)
    {
        item = ui->mListWidget->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
}

void CreateChatDialog::on_buttonBox_accepted()
{
    megachat::MegaChatPeerList *peerList = megachat::MegaChatPeerList::createInstance();
    QListWidgetItem *item = 0;
    for (int i = 0; i < ui->mListWidget->count(); ++i)
    {
        item = ui->mListWidget->item(i);
        if (item->checkState() == Qt::Checked)
        {
            megachat::MegaChatHandle userHandle = mMegaChatApi->getUserHandleByEmail(item->text().toStdString().c_str());
            if (userHandle == megachat::MEGACHAT_INVALID_HANDLE)
            {
                QMessageBox::warning(this, tr("Chat creation"), tr("Invalid user handle"));
                return;
            }
            peerList->addPeer(userHandle, megachat::MegaChatRoom::PRIV_STANDARD);
        }
    }

    if (peerList->size() != 1 && !mGroup)
    {
        QMessageBox::warning(this, tr("Chat creation"), tr("1on1 chatrooms only allow one participant."));
        show();
        delete peerList;
        return;
    }

    if (peerList->size() == 0 && !mPublic)
    {
        QMessageBox::warning(this, tr("Chat creation"), tr("Private groupchats cannot be empty"));
        show();
        delete peerList;
        return;
    }

    mMainWin->createChatRoom(peerList, mGroup, mPublic);
    delete peerList;
}

void CreateChatDialog::on_buttonBox_rejected()
{
    close();
}
