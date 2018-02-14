#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QListWidgetItem>
#include <assert.h>
#include "megachatapi.h"
#include "chatSettings.h"
#include "chatItemWidget.h"
#include "contactItemWidget.h"
#include "QTMegaChatListener.h"
#include "megaLoggerApplication.h"

class ChatItemWidget;
class ContactItemWidget;
namespace Ui
{
    class MainWindow;
}
class ChatSettings;
class MainWindow :
      public QMainWindow,
      public megachat::MegaChatListener
{
    Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = 0, MegaLoggerApplication *mLogger=NULL);
        virtual ~MainWindow();
        void setMegaChatApi(megachat::MegaChatApi *megaChatApi);
        void setMegaApi(mega::MegaApi *megaApi);
        void addChat(const megachat::MegaChatListItem *chatListItem);
        void addContact(megachat::MegaChatHandle contactHandle);
        void addChatListener();
        void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);
        mega::MegaUserList * getUserContactList();
        bool eventFilter(QObject *obj, QEvent *event);
        void contextMenuEvent(QContextMenuEvent* event);
        void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);


    public:
        MegaLoggerApplication *logger;
        int getNContacts() const;
        void setNContacts(int nContacts);

    protected:
        Ui::MainWindow *ui;
        bool chatsVisibility;
        QMenu * onlineStatus;
        mega::MegaApi * megaApi;
        megachat::MegaChatApi * megaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> chatWidgets;
        std::map<mega::MegaHandle, ContactItemWidget *> contactWidgets;
        int activeChats;
        int inactiveChats;
        int nContacts;

    private slots:
        void on_bSettings_clicked();
        void on_bOnlineStatus_clicked();
        void onAddContact();
        void setOnlineStatus();
        void onChangeChatVisibility();


    signals:
        void esidLogout();

     friend class ChatItemWidget;
};

#endif // MAINWINDOW_H


