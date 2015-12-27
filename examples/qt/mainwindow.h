#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include <../strophe.disco.h>
#include <ui_mainwindow.h>
#include <ui_clistitem.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include "chatWindow.h"
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
protected:
    karere::IGui::ITitleDisplay* addItem(bool front, karere::Contact* contact,
                karere::GroupChatRoom* room);

public slots:
    void onAudioInSelected();
    void onVideoInSelected();
};

extern bool inCall;

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
    { ui.mName->setText(QString::fromUtf8(title.c_str(), title.size())); }
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
    virtual void updateOnlineIndicator(int state)
    {
        QColor col;
        if (state == chatd::kChatStateOffline)
            col = QColor(Qt::gray);
        else if (state < chatd::kChatStateOnline)
            col = QColor(Qt::blue);
        else
            col = QColor(Qt::green);
        auto palette = ui.mOnlineIndicator->palette();
        palette.setColor(QPalette::Base, col);
        ui.mOnlineIndicator->setPalette(palette);
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
protected:
    karere::Contact& mContact;
public:
    CListContactItem(QWidget* parent, karere::Contact& contact)
        :CListItem(parent, false), mContact(contact){}
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        if (mContact.chatRoom())
        {
            mContact.chatRoom()->chatWindow().show();
            printf("showing chat window for chatid %s\n", chatd::Id(mContact.chatRoom()->chatid()).toString().c_str());

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
};
class CListGroupChatItem: public CListItem
{
protected:
    karere::GroupChatRoom& mRoom;
public:
    CListGroupChatItem(QWidget* parent, karere::GroupChatRoom& room)
        :CListItem(parent, true), mRoom(room){}
};


#endif // MAINWINDOW_H
