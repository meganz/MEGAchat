#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QInputDialog>
#include <QDrag>
#include <QMimeData>
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
QString prettyInterval(int64_t secs);

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
    void removeItem(IContactGui* item, bool isGroup);
//IContactList
    virtual IContactGui* createContactItem(karere::Contact& contact);
    virtual IContactGui* createGroupChatItem(karere::GroupChatRoom& room);
    virtual void removeContactItem(IContactGui* item);
    virtual void removeGroupChatItem(IContactGui* item);
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
    karere::IGui::IContactGui* addItem(bool front, karere::Contact* contact,
                karere::GroupChatRoom* room);
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto addAction = menu.addAction(tr("Add user to contacts"));
        connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
protected slots:
    void onAddContact();
public slots:
    void onAudioInSelected();
    void onVideoInSelected();
};

extern bool inCall;
extern QColor gAvatarColors[];
extern QString gOnlineIndColors[karere::Presence::kLast+1];
class CListItem: public QWidget, public karere::IGui::IContactGui
{
protected:
    Ui::CListItemGui ui;
    int mLastOverlayCount = 0;
    bool mIsGroup;
public:
    bool isGroup() const { return mIsGroup; }
//karere::ITitleDisplay interface
    virtual void updateOverlayCount(int count)
    {
        if (count < 0)
            ui.mUnreadIndicator->setText(QString::number(-count)+"+");
        else
            ui.mUnreadIndicator->setText(QString::number(count));
        ui.mUnreadIndicator->adjustSize();
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
    QPoint mDragStartPos;
public:
    CListContactItem(QWidget* parent, karere::Contact& contact)
        :CListItem(parent, false), mContact(contact)
    {
        mega::marshallCall([this]() { updateToolTip(); });
    }
    void updateToolTip() //WARNING: Must be called after app init, as the xmpp jid is not initialized during creation
    {
        QChar lf('\n');
        QString text = tr("Email: ");
        text.append(QString::fromStdString(mContact.email())).append(QChar('\n'));
        text.append(tr("User handle: ")).append(QString::fromStdString(chatd::Id(mContact.userId()).toString())).append(lf);
        text.append(tr("XMPP jid: ")).append(QString::fromStdString(mContact.jid())).append(lf);
        if (mContact.chatRoom())
            text.append(tr("You have chatted with this person in the past"));
        else
            text.append(tr("You have never chatted with this person"));
//        auto now = time(NULL);
//        text.append(tr("\nFriends since: ")).append(prettyInterval(now-contact.since())).append(lf);
        setToolTip(text);
    }
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        showChatWindow();
    }
    //IContactGui interface
    virtual void showChatWindow()
    {
        if (mContact.chatRoom())
        {
            mContact.chatRoom()->chatWindow().show();
            return;
        }
        mContact.createChatRoom()
        .then([this](karere::ChatRoom* room)
        {
            updateToolTip();
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
            "font: 24px;"
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
    void mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
        {
            mDragStartPos = event->pos();
        }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event)
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        if ((event->pos() - mDragStartPos).manhattanLength() < QApplication::startDragDistance())
            return;
        startDrag();
    }
    void startDrag()
    {
        QDrag drag(this);
        QMimeData *mimeData = new QMimeData;
        auto userid = mContact.userId();
        mimeData->setData("application/mega-user-handle", QByteArray((const char*)(&userid), sizeof(userid)));
        drag.setMimeData(mimeData);
        drag.exec();
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
        :CListItem(parent, true), mRoom(room)
    {
        updateToolTip();
    }
    void updateToolTip()
    {
        QString text(tr("Group chat room: "));
        text.append(QString::fromStdString(chatd::Id(mRoom.chatid()).toString())).append(QChar('\n'))
            .append(tr("Own privilege: ").append(QString::number(mRoom.ownPriv())).append(QChar('\n')))
            .append(tr("Other participants:\n"));
        for (const auto& item: mRoom.peers())
        {
            auto& peer = *item.second;
            const std::string* email = mRoom.parent.client.contactList->getUserEmail(item.first);
            auto line = QString(" %1 (%2, %3): priv %4\n").arg(QString::fromStdString(peer.name()))
                .arg(email?QString::fromStdString(*email):tr("(email unknown)"))
                .arg(QString::fromStdString(chatd::Id(item.first).toString()))
                .arg((int)item.second->priv());
            text.append(line);
        }
        text.truncate(text.size()-1);
        setToolTip(text);
    }
    virtual void updateTitle(const std::string& title)
    {
        QString text = tr("Group: ")+QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
        updateToolTip();
    }
    virtual void onMembersUpdated() { printf("onMembersUpdate\n"); updateToolTip(); }
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
    virtual void mouseDoubleClickEvent(QMouseEvent* event) { showChatWindow(); }
    virtual void showChatWindow() { mRoom.chatWindow().show(); }
protected slots:
    void leaveGroupChat() { mega::marshallCall([this]() { mRoom.leave(); }); } //deletes this
};


#endif // MAINWINDOW_H
