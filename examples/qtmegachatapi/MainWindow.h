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
#include "QTMegaChatCallListener.h"

struct Chat
{
    const megachat::MegaChatListItem *chatListItem;
    int64_t timestamp;

    Chat(const megachat::MegaChatListItem *item, int64_t ts) :
            chatListItem(item), timestamp(ts)
    {
    }
    bool operator <(const Chat & chatItem) const
    {
        return timestamp < chatItem.timestamp;
    }
};
struct ChatComparator
{
    bool operator ()(const Chat & chat1, const Chat & chat2)
    {
         return chat1 < chat2;
    }
};

class ChatSettings;
class ChatSettingsDialog;
class ChatItemWidget;
class ContactItemWidget;
class QTMegaChatCallListener;

namespace Ui
{
    class MainWindow;
}

class MainWindow :
      public QMainWindow,
      public megachat::MegaChatListener,
      public megachat::MegaChatCallListener
{
    Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = 0, MegaLoggerApplication *logger=NULL, megachat::MegaChatApi *megaChatApi = NULL, mega::MegaApi *megaApi = NULL);
        virtual ~MainWindow();
        void addChat(const megachat::MegaChatListItem *chatListItem);
        void addContact(mega::MegaUser *contact);
        void addChatListener();
        void clearContactChatList();
        void orderContactChatList(bool showInactive);
        void addContacts();
        void addInactiveChats();
        void addActiveChats();
        void createSettingsMenu();
#ifndef KARERE_DISABLE_WEBRTC
        void onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call);
#endif
        void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);
        mega::MegaUserList *getUserContactList();
        bool eventFilter(QObject *obj, QEvent *event);
        void contextMenuEvent(QContextMenuEvent* event);
        void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);
        ChatItemWidget *getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder);
    public:
        MegaLoggerApplication *mLogger;
        int getNContacts() const;
        void setNContacts(int nContacts);

    protected:
        Ui::MainWindow *ui;
        bool allItemsVisibility;
        QMenu *onlineStatus;
        ChatSettings *mChatSettings;
        mega::MegaApi * mMegaApi;
        megachat::MegaChatApi * mMegaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        megachat::QTMegaChatCallListener *megaChatCallListenerDelegate;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> chatWidgets;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> auxChatWidgets;
        std::map<mega::MegaHandle, ContactItemWidget *> contactWidgets;
        int activeChats;
        int inactiveChats;
        int nContacts;

    private slots:
        void on_bSettings_clicked();
        void on_bOnlineStatus_clicked();
        void onAddContact();
        void setOnlineStatus();
        void onChangeItemsVisibility();

    signals:
        void esidLogout();

     friend class ChatItemWidget;
     friend class MegaChatApplication;
     friend class ChatSettingsDialog;
     friend class CallAnswerGui;
     friend class ChatWindow;
};

#endif // MAINWINDOW_H


