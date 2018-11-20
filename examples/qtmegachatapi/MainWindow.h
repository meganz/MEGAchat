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

        /* Contacts management*/

        /*This function adds or updates a ContactListItemController.
            This class takes the ownership of the parameter contact.*/
        void addOrUpdateContactController(mega::MegaUser *contact);

        /*This function adds a contact Qt widget and increments the contacts counter*/
        ContactItemWidget *addContactWidget(mega::MegaUser *user);

        /*This function clears the Qt widget list and updates the ContactListItemController from
            the contactControllers map*/
        void clearContactWidgetList();

        /* This function clears the contacts Qt widgets and reorder them*/
        void orderContactList();

        /*This function clears the contactListItemControllers, if onlyWidget is true the function only will clear the widget.*/
        void clearContactListControllers(bool onlyWidget);

        /*This function adds or update contacts in contactControllers and adds Qt widgets*/
        void addContacts();

        /* Chats management*/

        /*This function adds or updates a chat controller.
        This class takes the ownership of the parameter chatListItem.*/
        ChatListItemController *addOrUpdateChatController(megachat::MegaChatListItem *chatListItem);

        /*This function adds a chat Qt widget and increments the correspondent chats counter*/
        ChatItemWidget *addChatWidget(const megachat::MegaChatListItem *chatListItem);

        /*This function clears the Qt widget list and updates the ChatListItemController from
            the chatControllers map*/
        void orderChatList(bool needClean);

        /* Returns a ChatWindow pointer associated to a ChatListItemController instance if exists,
            otherwise returns NULL. The ChatListItemController retains the ownership */
        ChatWindow *getChatWindowIfExists(megachat::MegaChatHandle chatId);

        /*This function adds the Qt widget and updates the ChatListItemController*/
        void addChatsBystatus(const int status);

        /*This function returns true if the change type received by the function chatListItemUpdate
            requires a Qt widget list reorder. Otherwise the function returns false*/
        bool needReorder(megachat::MegaChatListItem *newItem, int oldPriv);

        /* This function clear the chat widget container list, leaving intact chatController list*/
        void clearQtChatWidgetList();

        /* This function clear the ChatItemWidgets in ChatListItemController map*/
        void clearChatWidgets();

        /* This function clear the ChatListItemControllers map*/
        void clearChatControllers();

        /*This function makes a copy of the MegaChatListItem object and stores it in mLocalChatListItems*/
        void updateLocalChatListItems();
        ChatListItemController* getChatControllerById(megachat::MegaChatHandle chatId);

        /*This function returns a list of chats filtered by status (Active | Inactive | Archived).
         You take the ownership of the returned list*/
        std::list<Chat> *getLocalChatListItemsByStatus(int status);

        /*This function clears the chatListItemControllers (including chatItemWidget and chatListItem)*/
        void clearChatListControllers();

        std::string getAuthCode();
        void setNContacts(int nContacts);
        void createSettingsMenu();
        void createFactorMenu(bool factorEnabled);
        void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);
        bool eventFilter(QObject *obj, QEvent *event);

        /*Implemented callbacks*/
        void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);
        void onChatPresenceLastGreen(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int lastGreen);

#ifndef KARERE_DISABLE_WEBRTC
        void onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call);
#endif

    protected:
        MegaLoggerApplication *mLogger;
        Ui::MainWindow *ui;
        bool mShowInactive = false;
        bool mShowArchived = false;
        int activeChats;
        int archivedChats;
        int inactiveChats;
        int nContacts;
        bool allowOrder = false;
        bool mNeedReorder = false;
        QMenu *onlineStatus;
        ChatSettings *mChatSettings;
        MegaChatApplication *mApp;
        mega::MegaApi *mMegaApi;
        megachat::MegaChatApi *mMegaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        megachat::QTMegaChatCallListener *megaChatCallListenerDelegate;

        //Maps ChatId to to ChatListItemController
        std::map<mega::MegaHandle, ChatListItemController *> chatControllers;

        //Maps UserId to to ContactListItemController
        std::map<mega::MegaHandle, ContactListItemController *> contactControllers;

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


