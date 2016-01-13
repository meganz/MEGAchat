#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <string>
#include <videoRenderer_Qt.h>
#include <gcm.h>
#include "rtcModule/IRtcModule.h"
#include <services-dns.hpp>
#include <services-http.hpp>
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
            clist->takeItem(i);
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
