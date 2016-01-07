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
    karere::Client& client() const { return *mClient; }
    ~MainWindow();
    Ui::MainWindow ui;
    void removeItem(ITitleDisplay* item, bool isGroup);
    static void drawAvatar(const karere::Contact& contact, QImage& image);
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
