#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QInputDialog>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include <../strophe.disco.h>
#include <ui_mainwindow.h>
#include <ui_clistitem.h>
#include <ui_loginDialog.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include "chatWindow.h"
#include "loginDialog.h"

namespace Ui {
class MainWindow;
}
namespace karere {
class Client;
}

class MainWindow : public QMainWindow, public karere::IGui, public karere::IGui::IContactList
{
    Q_OBJECT
    karere::Client* mClient;
public:
    explicit MainWindow(karere::Client* aClient=nullptr);
    void setClient(karere::Client& client) { mClient = &client; }
    karere::Client& client() const { return *mClient; }
    ~MainWindow();
    Ui::MainWindow ui;
    void removeItem(ITitleDisplay* item, bool isGroup);
//IContactList
    virtual ITitleDisplay* createContactItem(karere::Contact& contact);
    virtual ITitleDisplay* createGroupChatItem(karere::GroupChatRoom& room);
    virtual void removeContactItem(ITitleDisplay* item);
    virtual void removeGroupChatItem(ITitleDisplay* item);
//IGui
    virtual karere::IGui::IContactList& contactList() { return *this; }
    virtual IChatWindow* createChatWindow(karere::ChatRoom& room);
    virtual IChatWindow& chatWindowForPeer(uint64_t handle);
    virtual rtcModule::IEventHandler* createCallAnswerGui(const std::shared_ptr<rtcModule::ICallAnswer> &ans)
    {
        return new CallAnswerGui(*this, ans);
    }
    virtual karere::IGui::ILoginDialog* createLoginDialog();
    virtual void onIncomingContactRequest(const mega::MegaContactRequest &req);
    virtual void show() { QMainWindow::show(); }
    virtual bool visible() const { return isVisible(); }
protected:
    karere::IGui::ITitleDisplay* addItem(bool front, karere::Contact* contact,
                karere::GroupChatRoom* room);
public slots:
    void onAudioInSelected();
    void onVideoInSelected();
};

extern bool inCall;
extern QColor gAvatarColors[];
extern QString gOnlineIndColors[karere::Presence::kLast+1];
class CListItem: public QWidget, public karere::IGui::ITitleDisplay
{
protected:
    Ui::CListItemGui ui;
    int mLastOverlayCount = 0;
    bool mIsGroup;
public:
    bool isGroup() const { return mIsGroup; }
//karere::ITitleDisplay interface
    virtual void updateTitle(const std::string& title)
    {
        QString text = QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
    }
    virtual void updateOverlayCount(int count)
    {
        if (count < 0)
            ui.mUnreadIndicator->setText(QString::number(-count)+"+");
        else
            ui.mUnreadIndicator->setText(QString::number(count));

        if (count)
        {
            if (!mLastOverlayCount)
                ui.mUnreadIndicator->show();
        }
        else
        {
            if (mLastOverlayCount)
                ui.mUnreadIndicator->hide();
        }
        mLastOverlayCount = count;
    }
    virtual void updateOnlineIndication(karere::Presence state)
    {
        ui.mOnlineIndicator->setStyleSheet(
            QString("background-color: ")+gOnlineIndColors[state]+
            ";border-radius: 4px");
    }
//===
    CListItem(QWidget* parent, bool aIsGroup)
    : QWidget(parent), mIsGroup(aIsGroup)
    {
        ui.setupUi(this);
        ui.mUnreadIndicator->hide();
    }
};

class CListContactItem: public CListItem
{
    Q_OBJECT
protected:
    karere::Contact& mContact;
public:
    CListContactItem(QWidget* parent, karere::Contact& contact)
        :CListItem(parent, false), mContact(contact)
    {}
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        if (mContact.chatRoom())
        {
            mContact.chatRoom()->chatWindow().show();
            return;
        }
        mContact.createChatRoom()
        .then([](karere::ChatRoom* room)
        {
            room->chatWindow().show();
        })
        .fail([this](const promise::Error& err)
        {
            QMessageBox::critical(nullptr, "rtctestapp",
                "Error creating chatroom:\n"+QString::fromStdString(err.what()));
        });
    }
    virtual void updateTitle(const std::string &title)
    {
        QString text = QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
        ui.mAvatar->setText(QString(text[0].toUpper()));
        ui.mAvatar->setStyleSheet(
            "border-radius: 4px;"
            "border: 2px solid rgba(0,0,0,0);"
            "color: white;"
            "background-color: "+gAvatarColors[mContact.userId() & 0x0f].name());
    }
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto chatInviteAction = menu.addAction(tr("Invite to group chat"));
        auto removeAction = menu.addAction(tr("Remove contact"));
        connect(chatInviteAction, SIGNAL(triggered()), this, SLOT(onCreateGroupChat()));
        connect(removeAction, SIGNAL(triggered()), this, SLOT(onContactRemove()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
public slots:
    void onCreateGroupChat()
    {
        std::string name;
        bool again;
        do
        {
            again = false;
            auto qname = QInputDialog::getText(this, tr("Invite to group chat"), tr("Enter group chat name"));
            if (qname.isNull())
                return;
            name = qname.toLatin1().data();
            for (auto& item: *mContact.contactList().client.chats)
            {
                auto& room = *item.second;
                if (room.isGroup() && room.titleString().c_str() == name)
                {
                    QMessageBox::critical(this, "Invite to group chat", "A group chat with that name already exists");
                    again = true;
                    break;
                }
            }
        }
        while(again);

        std::unique_ptr<mega::MegaTextChatPeerList> peers(mega::MegaTextChatPeerList::createInstance());
        peers->addPeer(mContact.userId(), chatd::PRIV_FULL);
        mContact.contactList().client.api->call(&mega::MegaApi::createChat, true, peers.get())
        .then([this, name](ReqResult result)
        {
            auto& list = *result->getMegaTextChatList();
            if (list.size() < 1)
                throw std::runtime_error("Empty chat list returned from API");
            mContact.contactList().client.chats->addRoom(*list.get(0), name);
        })
        .fail([this](const promise::Error& err)
        {
            QMessageBox::critical(this, tr("Create group chat"), tr("Error creating group chat:\n")+QString::fromStdString(err.msg()));
        });
    }
    void onContactRemove()
    {
        QString msg = tr("Are you sure you want to remove ");
        msg.append(mContact.titleString().c_str());
        if (mContact.titleString() != mContact.email())
        {
            msg.append(" (").append(mContact.email().c_str());
        }
        msg.append(tr(" from your contacts?"));

        auto ret = QMessageBox::question(this, tr("Remove contact"), msg);
        if (ret != QMessageBox::Yes)
            return;
        mContact.contactList().removeContactFromServer(mContact.userId())
        .fail([](const promise::Error& err)
        {
            QMessageBox::critical(nullptr, tr("Remove contact"), tr("Error removing contact: ").append(err.what()));
        });
    }
};
class CListGroupChatItem: public CListItem
{
    Q_OBJECT
public:
    CListGroupChatItem(QWidget* parent, karere::GroupChatRoom& room)
        :CListItem(parent, true), mRoom(room){}
protected:
    karere::GroupChatRoom& mRoom;
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto action = menu.addAction(tr("Leave group chat"));
        connect(action, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        mRoom.chatWindow().show();
    }
protected slots:
    void leaveGroupChat() { mega::marshallCall([this]() { mRoom.leave(); }); } //deletes this
};


#endif // MAINWINDOW_H
