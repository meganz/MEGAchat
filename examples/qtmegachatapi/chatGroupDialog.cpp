#include "chatGroupDialog.h"
#include "ui_chatGroupDialog.h"
#include <QMessageBox>
#include "chatItemWidget.h"

ChatGroupDialog::ChatGroupDialog(QWidget *parent, bool isGroup, bool isPublic, bool isMeeting, ::megachat::MegaChatApi *megaChatApi) :
    QDialog(parent),
    ui(new Ui::ChatGroupDialog),
    mMegaChatApi(megaChatApi),
    mMainWin((MainWindow *) parent),
    mIsGroup(isGroup),
    mIsPublic(isPublic),
    mIsMeeting(isMeeting),
    mOptionsAdded(!isGroup)
{
    ui->setupUi(this);
}

ChatGroupDialog::~ChatGroupDialog()
{
    delete ui;
}

void ChatGroupDialog::createOptionsList()
{
    ui->mListWidget->clear();
    ui->mListWidget->addItem(QString("Open Invite"));
    ui->mListWidget->addItem(QString("Speak Request"));
    ui->mListWidget->addItem(QString("Waiting Room"));

    QListWidgetItem *item = 0;
    for (int i = 0; i < ui->mListWidget->count(); ++i)
    {
        item = ui->mListWidget->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    mOptionsAdded = true;
}

void ChatGroupDialog::createChatList(::mega::MegaUserList *contactList)
{
    if (!contactList || contactList->size() == 0)
    {
        return;
    }

    ::mega::MegaUser *contact = NULL;
    for (int i = 0; i < contactList->size(); i++)
    {
        contact = contactList->get(i);
        const char *contactEmail = contact->getEmail();
        QString peerMail;
        peerMail.append(QString::fromStdString(contactEmail));
        ui->mListWidget->addItem(peerMail);
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
    if (!mOptionsAdded)
    {
        mPeerList.reset(megachat::MegaChatPeerList::createInstance());
        if (!mIsMeeting)     // no peers for meetings upon creation
        {
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
                    mPeerList->addPeer(userHandle, 2);
                }
            }
        }

        if (mIsMeeting && mPeerList->size() != 0)
        {
            QMessageBox::warning(this, tr("Add chatRoom"), tr("Meeting rooms are created without participants. List will be ignored"));
        }
        else if (mPeerList->size() == 0 && !mIsPublic)
        {
            QMessageBox::warning(this, tr("Add chatRoom"), tr("Private chats must have at least one participant."));
            return;
        }
        else if (mPeerList->size() > 1 && !mIsGroup)
        {
            QMessageBox::warning(this, tr("Add chatRoom"), tr("Individual chats cannot have more than one participant"));
            return;
        }

        // show option list
        createOptionsList();
        show();
        return;
    }

    bool openInvite = ui->mListWidget->item(0)->checkState() == Qt::Checked;
    bool speakRequest = ui->mListWidget->item(1)->checkState() == Qt::Checked;
    bool waitingRoom = ui->mListWidget->item(2)->checkState() == Qt::Checked;

    char* title = NULL;
    if (mIsGroup)
    {
        title = mMainWin->askChatTitle();
    }

    if (mIsGroup)
    {
        std::unique_ptr<megachat::MegaChatListItemList> list(mMegaChatApi->getChatListItemsByPeers(mPeerList.get()));
        if (list->size() != 0)
        {
             QMessageBox msgBoxAns;
             msgBoxAns.setText("You have another chatroom with same participants do you want to reuse it ");
             msgBoxAns.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
             int retVal = msgBoxAns.exec();
             if (retVal == QMessageBox::Yes)
             {
                 if (list->get(0)->isArchived())
                 {
                     ChatListItemController *chatController = mMainWin->getChatControllerById(list->get(0)->getChatId());
                     if (chatController)
                     {
                         chatController->archiveChat(false);
                         QMessageBox::warning(this, tr("Add chatRoom"), tr("You have unarchived a chatroom to reuse it"));
                     }
                 }
                 else
                 {
                     QMessageBox::warning(this, tr("Add chatRoom"), tr("You have decide to reuse the chatroom"));
                 }

                 return;
             }
        }

        if (mIsMeeting)
        {
            mMegaChatApi->createMeeting(title, speakRequest, waitingRoom, openInvite);                      // meeting room
        }
        else if (mIsPublic)
        {
            mMegaChatApi->createPublicChat(mPeerList.get(), title, speakRequest, waitingRoom, openInvite);   // public chat
        }
        else
        {
            mMegaChatApi->createGroupChat(mPeerList.get(), title, speakRequest, waitingRoom, openInvite);    // private group chat
        }
    }
    else
    {
        mMegaChatApi->createChat(false, mPeerList.get());                                                    // 1on1 chat
    }

    delete [] title;
}

void ChatGroupDialog::on_buttonBox_rejected()
{
    close();
}
