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
#include "chatGroupDialog.h"
#include "QTMegaChatCallListener.h"
#include "MegaChatApplication.h"
#include "listItemController.h"
#include "chatWindow.h"

const int chatActiveStatus   = 0;
const int chatInactiveStatus = 1;
const int chatArchivedStatus = 2;
class MegaChatApplication;
class ContactListItemController;
class ChatListItemController;

struct Chat
{
    const megachat::MegaChatListItem *chatItem;

    Chat(const megachat::MegaChatListItem *item) :
            chatItem(item)
    {
    }
    bool operator < (const Chat &item) const
    {
        return this->chatItem->getLastTimestamp() < item.chatItem->getLastTimestamp();
    }
};
struct ChatComparator
{
    bool operator () (const Chat &chat1, const Chat &chat2)
    {
         return chat1 < chat2;
    }
};

class ChatSettings;
class ChatSettingsDialog;
class ChatItemWidget;
class ContactItemWidget;
class QTMegaChatCallListener;
class ChatWindow;

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
        void addOrUpdateContactController(mega::MegaUser *contact);
        ContactItemWidget *addContactWidget(mega::MegaUser *user);
        void orderContactList();
        void orderChatList();
        void clearContactWidgetList();
        ChatWindow *getChatWindowIfExists(megachat::MegaChatHandle chatId);
        void clearChatWidgetList();
        void addContacts();
        void addInactiveChats();
        void addArchivedChats();
        void addActiveChats();
        void createSettingsMenu();
        bool needReorder(megachat::MegaChatListItem *newItem, const megachat::MegaChatListItem *oldItem);

        /* This function clear the chat widget container list,leaving intact chatController list*/
        void clearQtChatWidgetList();

        /* This function clear the ChatItemWidgets in ChatListItemController map*/
        void clearChatWidgets();

        /* This function clear the ChatListItemControllers map*/
        void clearChatControllers();

#ifndef KARERE_DISABLE_WEBRTC
        void onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call);
#endif
        //This class retains the ownership of the returned value
        const megachat::MegaChatListItem *getLocalChatListItem(megachat::MegaChatHandle chatId);
        //You take the ownership of the returned value
        std::list<Chat> *getLocalChatListItemsByStatus(int status);
        //This function makes a copy of the MegaChatListItem object and stores it in mLocalChatListItems
        void addOrUpdateLocalChatListItem(const megachat::MegaChatListItem *item);
        void updateLocalChatListItems();
        ChatListItemController* getChatControllerById(megachat::MegaChatHandle chatId);
        void createFactorMenu(bool factorEnabled);
        void removeLocalChatListItem(megachat::MegaChatListItem *item);
        void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);

        //This function clears the contactListItems, if onlyWidget is true the function only will clear the widget.
        void clearContactListControllers(bool onlyWidget);
        mega::MegaUserList *getUserContactList();
        std::string getAuthCode();
        bool eventFilter(QObject *obj, QEvent *event);
        void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);
        void onChatPresenceLastGreen(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int lastGreen);
        ChatItemWidget *getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder);

    public:
        MegaLoggerApplication *mLogger;
        int getNContacts() const;
        void setNContacts(int nContacts);

    protected:
        Ui::MainWindow *ui;
        bool mShowInactive = false;
        bool mShowArchived = false;
        QMenu *onlineStatus;
        ChatSettings *mChatSettings;
        MegaChatApplication *mApp;
        mega::MegaApi *mMegaApi;
        megachat::MegaChatApi *mMegaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        megachat::QTMegaChatCallListener *megaChatCallListenerDelegate;
        std::map<megachat::MegaChatHandle, const megachat::MegaChatListItem *> mLocalChatListItems;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> chatWidgets;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> auxChatWidgets;
        std::map<mega::MegaHandle, ChatListItemController *> chatControllers;
        std::map<mega::MegaHandle, ContactListItemController *> contactControllers;
        int activeChats;
        int archivedChats;
        int inactiveChats;
        int nContacts;
        bool allowOrder = false;
        bool mNeedReorder = false;

    private slots:
        void on_bSettings_clicked();
        void on_bOnlineStatus_clicked();
        void onAddContact();
        void onAddChatGroup();
        void onWebRTCsetting();
        void setOnlineStatus();
        void onShowInactiveChats();
        void onShowArchivedChats();
        void onAddGroupChat();
        void onTwoFactorGetCode();
        void onTwoFactorDisable();
        void onTwoFactorCheck(bool);
        void on_mLogout_clicked();
        void onlastGreenVisibleClicked();

    signals:
        void esidLogout();

     friend class ChatItemWidget;
     friend class ContactItemWidget;
     friend class MegaChatApplication;
     friend class ChatSettingsDialog;
     friend class CallAnswerGui;
     friend class ChatWindow;
     friend class ChatMessage;
};

#endif // MAINWINDOW_H


