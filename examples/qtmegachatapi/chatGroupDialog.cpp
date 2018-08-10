#include "chatGroupDialog.h"
#include "ui_chatGroupDialog.h"
#include <QMessageBox>
#include "chatItemWidget.h"

ChatGroupDialog::ChatGroupDialog(QWidget *parent, megachat::MegaChatApi *megaChatApi, bool aPub) :
    QDialog(parent),
    ui(new Ui::ChatGroupDialog)
{
    mMainWin = (MainWindow *) parent;
    mMegaChatApi = megaChatApi;
    ui->setupUi(this);
    mPublic = aPub;
}

ChatGroupDialog::~ChatGroupDialog()
{
    delete ui;
}

void ChatGroupDialog::createChatList(mega::MegaUserList *contactList)
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

void ChatGroupDialog::on_buttonBox_accepted()
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

    if (peerList->size() == 0 && !mPublic)
    {
        QMessageBox::warning(this, tr("Chat creation"), tr("Private groupchats cannot be empty"));
        return;
    }

    mMainWin->createChatRoom(peerList, true, mPublic);
    delete peerList;
}

void ChatGroupDialog::on_buttonBox_rejected()
{
    close();
}
