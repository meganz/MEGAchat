#include "mainwindow.h"
#include <ui_mainwindow.h>
#include <ui_loginDialog.h>
#include "qmessagebox.h"
#include <string>
#include <videoRenderer_Qt.h>
#include <gcm.h>
#include "rtcModule/IRtcModule.h"
#include <iostream>
#include <rapidjson/document.h>
#include <sdkApi.h>
#include <chatClient.h>
#include <messageBus.h>
#include <textModule.h>
#include <chatRoom.h>
#include <QFont>
#include <QPainter>
#include <QImage>

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;

using namespace std;
using namespace mega;
using namespace karere;
using namespace promise;

class LoginDialog: public QDialog, public karere::IGui::ILoginDialog
{
    Q_OBJECT
    Ui::LoginDialog ui;
    promise::Promise<std::pair<std::string, std::string>> mPromise;
    static QString sLoginStageStrings[kLast+1];
public:
    LoginDialog(QWidget* parent): QDialog(parent)
    {
        ui.setupUi(this);
        connect(ui.mOkBtn, SIGNAL(clicked(bool)), this, SLOT(onOkBtn(bool)));
        connect(ui.mCancelBtn, SIGNAL(clicked(bool)), this, SLOT(onCancelBtn(bool)));
    }
    virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials()
    {
        if (!isVisible())
            show();
        else //reusing, recreate promise
            mPromise = promise::Promise<std::pair<std::string, std::string>>();

        return mPromise;
    }
    virtual void setState(LoginStage state)
    {
        ui.mLoginStateDisplay->setText(sLoginStageStrings[state]);
    }

public slots:
    void onOkBtn(bool)
    {
        if (mPromise.done())
            return;
        ui.mOkBtn->setEnabled(false);
        ui.mCancelBtn->setEnabled(false);
        mPromise.resolve(make_pair(
            ui.mEmailInput->text().toStdString(), ui.mPasswordInput->text().toStdString()));
    }
    void onCancelBtn(bool)
    {
        if (mPromise.done())
            return;
        mPromise.reject("Login dialog canceled by user");
    }
};
QString LoginDialog::sLoginStageStrings[] = {
    tr("Authenticating"), tr("Logging in"),
    tr("Fetching filesystem"), tr("Login complete")
};

MainWindow::MainWindow(Client* aClient): mClient(aClient)
{
    ui.setupUi(this);
}
extern bool inCall;

void MainWindow::onAudioInSelected()
{
    auto combo = ui.audioInCombo;
    string device = combo->itemText(combo->currentIndex()).toLatin1().data();
    bool ret = mClient->rtc->selectAudioInDevice(device);
    if (!ret)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    KR_LOG_DEBUG("Selected audio device '%s'", device.c_str());
}

void MainWindow::onVideoInSelected()
{
    auto combo = ui.videoInCombo;
    string device = combo->itemText(combo->currentIndex()).toLatin1().data();
    bool ret = mClient->rtc->selectVideoInDevice(device);
    if (!ret)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    KR_LOG_DEBUG("Selected video device '%s'", device.c_str());
}

MainWindow::~MainWindow()
{}
QColor gAvatarColors[16] = {
    "aliceblue", "antiquewhite", "darkseagreen",
    "crimson", "firebrick", "lightsteelblue"};

QString gOnlineIndColors[karere::Presence::kLast+1] =
{  "lightgray", "red", "orange", "lightgreen", "lightblue" };


karere::IGui::ITitleDisplay*
MainWindow::createContactItem(karere::Contact& contact)
{
    auto clist = ui.contactList;
    auto contactGui = new CListContactItem(clist, contact);
    auto item = new QListWidgetItem;
    item->setSizeHint(contactGui->size());
    clist->addItem(item);
    clist->setItemWidget(item, contactGui);
    return contactGui;
}
karere::IGui::ITitleDisplay*
MainWindow::createGroupChatItem(karere::GroupChatRoom& room)
{
    auto clist = ui.contactList;
    auto chatGui = new CListGroupChatItem(clist, room);
    auto item = new QListWidgetItem;
    item->setSizeHint(chatGui->size());
    clist->insertItem(0, item);
    clist->setItemWidget(item, chatGui);
    return chatGui;
}

void MainWindow::removeItem(ITitleDisplay* item, bool isGroup)
{
    auto clist = ui.contactList;
    auto size = clist->count();
    for (int i=0; i<size; i++)
    {
        auto gui = static_cast<CListItem*>(clist->itemWidget(clist->item(i)));
        if (item == gui)
        {
            if (gui->isGroup() != isGroup)
                throw std::runtime_error("ContactList: removeItem: Asked to remove a different type of contactlist item");
            delete clist->takeItem(i);
            return;
        }
    }
    throw std::runtime_error("ContactList: removeItem: Item not found");
}
void MainWindow::removeGroupChatItem(ITitleDisplay *item)
{
    removeItem(item, true);
}
void MainWindow::removeContactItem(ITitleDisplay *item)
{
    removeItem(item, false);
}

karere::IGui::IChatWindow* MainWindow::createChatWindow(karere::ChatRoom& room)
{
    return new ChatWindow(room, *this);
}
karere::IGui::IChatWindow& MainWindow::chatWindowForPeer(uint64_t handle)
{
    auto it = mClient->contactList->find(handle);
    if (it == mClient->contactList->end())
        throw std::runtime_error("chatWindowForPeer: peer '"+chatd::Id(handle).toString()+"' not in contact list");
    auto room = it->second->chatRoom();
    if (!room)
        throw std::runtime_error("chatWindowForPeer: peer contact has no chatroom");
    return room->chatWindow();
}
karere::IGui::ILoginDialog* MainWindow::createLoginDialog()
{
    return new LoginDialog(nullptr);
}
#include <mainwindow.moc>
