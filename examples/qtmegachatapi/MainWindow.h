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
#include "QTMegaChatSchedMeetListener.h"
#include "MegaChatApplication.h"
#include "listItemController.h"
#include "chatWindow.h"
#include "SettingWindow.h"
#include "confirmAccount.h"

const int chatNotArchivedStatus = 0;
const int chatArchivedStatus = 1;
class MegaChatApplication;
class ContactListItemController;
class ChatListItemController;
class QMessageBox;

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
class ChatListItemController;
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
      public megachat::MegaChatCallListener,
      public megachat::MegaChatScheduledMeetingListener
{
    Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = 0, MegaLoggerApplication *logger=NULL, megachat::MegaChatApi *megaChatApi = NULL, ::mega::MegaApi *megaApi = NULL);
        virtual ~MainWindow();

        /* Contacts management*/

        /*  This function clears the Qt widget list */
        void clearQtContactWidgetList();

        /*  This function clears the ContactItemWidgets in contactControllers map*/
        void clearContactWidgets();

        /*  This function clears the contactControllers map*/
        void clearContactControllersMap();

        /*  This function adds Qt widgets for all items in contactControllers map*/
        void addQtContactWidgets();

        /*  This function adds a contact Qt widget and increments the contacts counter*/
        ContactItemWidget *addQtContactWidget(mega::MegaUser *user);

        /*  This function updates the items given by parameter in contactControllers map.
            If not exists add it to map*/
        void addOrUpdateContactControllersItems(mega::MegaUserList *contactList);

        /*  This function adds or updates an user in contactControllers map.*/
        ContactListItemController *addOrUpdateContactController(mega::MegaUser *contact);

        /*  This function finds in chatControllers map an entry with key equals to given parameter and returns
            a ChatListItemController pointer if exists, otherwise returns nullptr. Mainwindow class retains the
            ownership of the returned value*/
        ContactListItemController *getContactControllerById(megachat::MegaChatHandle userId);

        /*  This function reorders the graphical contact list in QTapp*/
        void reorderAppContactList();

        /*  Chats management*/

        /*  This function adds a Qt widget chat and increments the correspondent chats counter*/
        ChatItemWidget *addQtChatWidget(const megachat::MegaChatListItem *chatListItem);

        /*  This function adds or updates a chat in chatControllers map.*/
        ChatListItemController *addOrUpdateChatControllerItem(megachat::MegaChatListItem *chatListItem);

        /*  This function reorders the graphical chat list in QTapp*/
        void reorderAppChatList();

        /*  Returns a ChatWindow pointer associated to a ChatListItemController instance if exists,
            otherwise returns NULL. The ChatListItemController retains the ownership */
        ChatWindow *getChatWindowIfExists(megachat::MegaChatHandle chatId);

        /*  This function adds the graphical Qt widget and updates the ChatListItemController*/
        void addChatsBystatus(const int status);

        /*  This function returns true if the changes associated to the newItem param
            requires a Qt widget list reorder. Otherwise the function returns false*/
        bool needReorder(megachat::MegaChatListItem *newItem, int oldPriv);

        /*  This function clears the Qt widget list */
        void clearQtChatWidgetList();

        /*  This function clears the ChatItemWidgets in chatControllers map*/
        void clearChatWidgets();

        /*  This function clears the ChatListItemControllers map*/
        void clearChatControllers();

        /*  This function updates the items in chatControllers map*/
        void updateChatControllersItems();

        /*  This function finds in chatControllers map an entry with key equals to chatId and returns a ChatListItemController pointer if exists,
            otherwise returns nullptr. Mainwindow class retains the ownership of the returned value*/
        ChatListItemController *getChatControllerById(megachat::MegaChatHandle chatId);

        /*  This function returns a list of chats filtered by status (Active | Inactive | Archived).
            You take the ownership of the returned list*/
        std::list<Chat> *getLocalChatListItemsByStatus(int status);

        char *askChatTitle();
        std::string getAuthCode();
        void setNContacts(int nContacts);
        void createSettingsMenu();
        void createFactorMenu(bool factorEnabled);
        void updateContactTitle(megachat::MegaChatHandle contactHandle, const char *title);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);
        void updateToolTipMyInfo();
        void removeListeners();
        void openChatPreview(bool create);
        void closeChatPreview(megachat::MegaChatHandle chatId);
        void activeControls(bool active);
        bool eventFilter(QObject *obj, QEvent *event);

        void onChatInitStateUpdate(megachat::MegaChatApi *api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi *api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi *api, megachat::MegaChatPresenceConfig *config);
        void onChatPresenceLastGreen(megachat::MegaChatApi *api, megachat::MegaChatHandle userhandle, int lastGreen);
        void onDbError(megachat::MegaChatApi */*api*/, int error, const char *msg);

#ifndef KARERE_DISABLE_WEBRTC
        // MegaChatCallListener callbacks
        void onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call);
        void onChatSessionUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, megachat::MegaChatHandle callid, megachat::MegaChatSession *session);

        // MegaChatScheduledMeetingListener callbacks
        void onChatSchedMeetingUpdate(megachat::MegaChatApi* api, megachat::MegaChatScheduledMeeting* sm);
        void onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, bool append);

#endif
        MegaChatApplication* getApp() const;

        void confirmAccount(const std::string& password);
        void setEphemeralAccount(bool ephemeralAccount);

    protected:
        MegaLoggerApplication *mLogger;
        Ui::MainWindow *ui;
        bool mShowArchived = false;
        int mActiveChats;
        int mArchivedChats;
        int mInactiveChats;
        int mNContacts;
        bool mAllowOrder = false;
        bool mNeedReorder = false;
        QMenu *onlineStatus;
        ChatSettings *mChatSettings;    // dialog to set WebRTC input device/s
        MegaChatApplication *mApp;
        ::mega::MegaApi *mMegaApi;
        megachat::MegaChatApi *mMegaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        megachat::QTMegaChatCallListener *megaChatCallListenerDelegate;
        megachat::QTMegaChatScheduledMeetingListener* megaSchedMeetingListenerDelegate;

        //Maps ChatId to to ChatListItemController
        std::map<mega::MegaHandle, ChatListItemController *> mChatControllers;

        //Maps UserId to to ContactListItemController
        std::map<mega::MegaHandle, ContactListItemController *> mContactControllers;

        SettingWindow *mSettings = NULL;
        ConfirmAccount* mConfirmAccount = nullptr;
        std::unique_ptr<QMessageBox> mCriticalMsgBox;
        bool mIsEphemeraAccount = false;
        void closeEvent(QCloseEvent *event);

    private slots:
        void on_bSettings_clicked();
        void on_bOnlineStatus_clicked();
        void onAddContact();
        void onAddChatRoom(bool isGroup, bool isPublic, bool isMeeting);
        void onAddChatSchedMeeting();
        void onWebRTCsetting();
        void setOnlineStatus();
        void onShowArchivedChats();
        void onTwoFactorGetCode();
        void onTwoFactorDisable();
        void onTwoFactorCheck();
        void onPrintUseralerts();
        void onPrintMyInfo();
        void on_mLogout_clicked();
        void onCatchUp();
        void onSetSFUId();
        void onlastGreenVisibleClicked();
        void onChatsSettingsClicked();
        void onChatCheckPushNotificationRestrictionClicked();
        void onReconnect(bool disconnect);
        void onPushReceived(unsigned int type);
        void onUseApiStagingClicked(bool);
        void onBackgroundStatusClicked(bool status);
        void onConfirmAccountClicked();
        void onImportMessages();
        void onAccountConfirmation(const std::string& email, const std::string& password);
        void onCancelAccountConfirmation();
        void onJoinAsGuest();

    signals:
        void esidLogout();
        void onAnonymousLogout();

     friend class ChatItemWidget;
     friend class ChatListItemController;
     friend class ContactItemWidget;
     friend class MegaChatApplication;
     friend class ChatSettingsDialog;
     friend class CallAnswerGui;
     friend class ChatWindow;
     friend class ChatMessage;
     friend class MeetingView;
};

#endif // MAINWINDOW_H


