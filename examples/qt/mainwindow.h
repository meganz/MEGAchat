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
#include <ui_settings.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include "chatWindow.h"

namespace Ui {
class MainWindow;
class SettingsDialog;
}
namespace karere {
class Client;
}
QString prettyInterval(int64_t secs);

class MainWindow : public QMainWindow, public karere::IApp, public karere::IApp::IContactListHandler
{
    Q_OBJECT
    karere::Client* mClient;
public:
    explicit MainWindow(karere::Client* aClient=nullptr);
    void setClient(karere::Client& client) { mClient = &client; }
    karere::Client& client() const { return *mClient; }
    ~MainWindow();
    Ui::MainWindow ui;
    void removeItem(IContactListItem* item, bool isGroup);
//IContactList
    virtual IContactListItem* addContactItem(karere::Contact& contact);
    virtual IContactListItem* addGroupChatItem(karere::GroupChatRoom& room);
    virtual void removeContactItem(IContactListItem* item);
    virtual void removeGroupChatItem(IContactListItem* item);
//IApp
    virtual karere::IApp::IContactListHandler& contactListHandler() { return *this; }
    virtual IChatHandler* createChatHandler(karere::ChatRoom& room);
    virtual IChatHandler& chatHandlerForPeer(uint64_t handle);
    virtual rtcModule::IEventHandler* onIncomingCall(const std::shared_ptr<rtcModule::ICallAnswer> &ans)
    {
        return new CallAnswerGui(*this, ans);
    }
    virtual karere::IApp::ILoginDialog* createLoginDialog();
    virtual void onOwnPresence(karere::Presence pres);
    virtual void onIncomingContactRequest(const mega::MegaContactRequest &req);
protected:
    karere::IApp::IContactListItem* addItem(bool front, karere::Contact* contact,
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
    void onSettingsBtn(bool);
    void onOnlineStatusBtn(bool);
    void setOnlineStatus();
};

class SettingsDialog: public QDialog
{
    Q_OBJECT
protected:
    Ui::SettingsDialog ui;
    int mAudioInIdx;
    int mVideoInIdx;
    MainWindow& mMainWindow;
    void selectVideoInput();
    void selectAudioInput();
protected slots:
public:
    SettingsDialog(MainWindow &parent);
    void applySettings();
};

extern bool inCall;
extern QColor gAvatarColors[];
extern QString gOnlineIndColors[karere::Presence::kLast+1];
class CListItem: public QWidget, public karere::IApp::IContactListItem
{
protected:
    Ui::CListItemGui ui;
    int mLastOverlayCount = 0;
    bool mIsGroup;
public:
    bool isGroup() const { return mIsGroup; }
    virtual void showChatWindow() = 0;
//karere::ITitleDisplay interface
    virtual void onUnreadCountChanged(int count)
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
    virtual void* userp() { return this; }
    virtual void onPresenceChanged(karere::Presence state)
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
        if (contact.visibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
        {
            showAsHidden();
        }
        karere::setTimeout([this]() { updateToolTip(); }, 100);
    }
    void showAsHidden()
    {
        ui.mName->setStyleSheet("color: rgba(0,0,0,128)\n");
    }
    void unshowAsHidden()
    {
        ui.mName->setStyleSheet("color: rgba(255,255,255,255)\n");
    }
    void updateToolTip() //WARNING: Must be called after app init, as the xmpp jid is not initialized during creation
    {
        QChar lf('\n');
        QString text;
        if (mContact.visibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
            text = "INVISIBLE\n";
        text.append(tr("Email: "));
        text.append(QString::fromStdString(mContact.email())).append(lf);
        text.append(tr("User handle: ")).append(QString::fromStdString(karere::Id(mContact.userId()).toString())).append(lf);
        text.append(tr("XMPP jid: ")).append(QString::fromStdString(mContact.jid())).append(lf);
        if (mContact.chatRoom())
            text.append(tr("Chat handle: ")).append(QString::fromStdString(karere::Id(mContact.chatRoom()->chatid()).toString()));
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
    virtual void showChatWindow()
    {
        if (mContact.chatRoom())
        {
            static_cast<ChatWindow*>(mContact.chatRoom()->appChatHandler().userp())->show();
            return;
        }
        mContact.createChatRoom()
        .then([this](karere::ChatRoom* room)
        {
            updateToolTip();
            static_cast<ChatWindow*>(mContact.chatRoom()->appChatHandler().userp())->show();
        })
        .fail([this](const promise::Error& err)
        {
            QMessageBox::critical(nullptr, "rtctestapp",
                "Error creating chatroom:\n"+QString::fromStdString(err.what()));
        });
    }
    virtual void onTitleChanged(const std::string &title)
    {
        QString text = QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
        ui.mAvatar->setText(QString(text[0].toUpper()));
        auto& col = gAvatarColors[mContact.userId() & 0x0f];

        QString style = "border-radius: 4px;"
            "border: 2px solid rgba(0,0,0,0);"
            "color: white;"
            "font: 24px;"
            "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,"
            "stop:0 rgba(%1,%2,%3,180), stop:1 rgba(%1,%2,%3,255))";
        style = style.arg(col.red()).arg(col.green()).arg(col.blue());
        ui.mAvatar->setStyleSheet(style);
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
    virtual void onVisibilityChanged(int newVisibility)
    {
        GUI_LOG_DEBUG("onVisibilityChanged for contact %s: new visibility is %d",
               karere::Id(mContact.userId()).toString().c_str(), newVisibility);
        if (newVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN)
            showAsHidden();
        else
            unshowAsHidden();
        updateToolTip();
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
        mContact.contactList().client.api.call(&mega::MegaApi::createChat, true, peers.get())
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
        text.append(QString::fromStdString(karere::Id(mRoom.chatid()).toString())).append(QChar('\n'))
            .append(tr("Own privilege: ").append(QString::number(mRoom.ownPriv())).append(QChar('\n')))
            .append(tr("Other participants:\n"));
        for (const auto& item: mRoom.peers())
        {
            auto& peer = *item.second;
            const std::string* email = mRoom.parent.client.contactList->getUserEmail(item.first);
            auto line = QString(" %1 (%2, %3): priv %4\n").arg(QString::fromStdString(peer.name()))
                .arg(email?QString::fromStdString(*email):tr("(email unknown)"))
                .arg(QString::fromStdString(karere::Id(item.first).toString()))
                .arg((int)item.second->priv());
            text.append(line);
        }
        text.truncate(text.size()-1);
        setToolTip(text);
    }
    virtual void onTitleChanged(const std::string& title)
    {
        QString text = tr("Group: ")+QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
        updateToolTip();
    }
    virtual void onMembersUpdated() { printf("onMembersUpdate\n"); updateToolTip(); }
    virtual void onVisibilityChanged(int newVisibility) {}
protected:
    karere::GroupChatRoom& mRoom;
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto action = menu.addAction(tr("Leave group chat"));
        connect(action, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        auto topicAction = menu.addAction(tr("Set chat topic"));
        connect(topicAction, SIGNAL(triggered()), this, SLOT(setTopic()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
    virtual void mouseDoubleClickEvent(QMouseEvent* event) { showChatWindow(); }
    virtual void showChatWindow()
    {
        static_cast<ChatWindow*>(mRoom.appChatHandler().userp())->show();
    }
protected slots:
    void leaveGroupChat() { karere::marshallCall([this]() { mRoom.leave(); }); } //deletes this
    void setTopic();
};


#endif // MAINWINDOW_H
