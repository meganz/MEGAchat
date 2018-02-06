#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <assert.h>
#include "megachatapi.h"
#include "chatItemWidget.h"
#include "contactItemWidget.h"
#include "QTMegaChatListener.h"
#include "chatSettings.h"

namespace Ui
{
    class MainWindow;
}

class MainWindow :
      public QMainWindow,
      public megachat::MegaChatListener
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    virtual ~MainWindow();

    void setMegaChatApi(megachat::MegaChatApi *megaChatApi);
    void setMegaApi(mega::MegaApi *megaApi);
    void addChat(const megachat::MegaChatListItem *chatListItem);
    void addContact(megachat::MegaChatHandle contactHandle);
    void addChatListener();
    void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);

    void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
    void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
    void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
    void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
    void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);    

private:
    Ui::MainWindow *ui;
    megachat::MegaChatApi * megaChatApi;
    mega::MegaApi * megaApi;
    std::map<megachat::MegaChatHandle, ChatItemWidget *> chatWidgets;
    std::map<mega::MegaHandle, ContactItemWidget *> contactWidgets;
    megachat::QTMegaChatListener *megaChatListenerDelegate;
    bool chatsVisibility;

protected:
    void contextMenuEvent(QContextMenuEvent* event);
    bool eventFilter(QObject *obj, QEvent *event);

protected slots:
    void onAddContact();
    void setOnlineStatus();
    void onChangeChatVisibility();

signals:
    void esidLogout();

private slots:
    void on_bSettings_clicked();
    void on_bOnlineStatus_clicked();
};

#endif // MAINWINDOW_H


