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

const int chatActiveStatus   = 0;
const int chatInactiveStatus = 1;
const int chatArchivedStatus = 2;

class MegaChatApplication;

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

class WebRTCSettings;
class WebRTCSettingsDialog;
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
        void clearContactChatList();
        void orderContactChatList();
        void addContacts();
        void addInactiveChats();
        void addArchivedChats();
        void addActiveChats();
        void createWebRTCSettingsDialog();
#ifndef KARERE_DISABLE_WEBRTC
        void onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call);
#endif
        //This class retains the ownership of the returned value
        const megachat::MegaChatListItem *getLocalChatListItem(megachat::MegaChatHandle chatId);
        //You take the ownership of the returned value
        std::list<Chat> *getLocalChatListItemsByStatus(int status);
        //This function makes a copy of the MegaChatListItem object and stores it in mLocalChatListItems
        void addLocalChatListItem(const megachat::MegaChatListItem *item);
        void updateLocalChatListItems();
        void updateLocalChatListItem(megachat::MegaChatListItem *item);
        void removeLocalChatListItem(megachat::MegaChatListItem *item);
        void updateContactFirstname(megachat::MegaChatHandle contactHandle, const char * firstname);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);
        mega::MegaUserList *getUserContactList();
        std::string getAuthCode();
        bool eventFilter(QObject *obj, QEvent *event);
        void onChatInitStateUpdate(megachat::MegaChatApi* api, int newState);
        void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
        void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int newState);
        void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
        void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);
        ChatItemWidget *getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder);
        void updateToolTipMyInfo(megachat::MegaChatHandle myHandle);
        int getNContacts() const;
        void setNContacts(int nContacts);
        void createChatRoom(megachat::MegaChatPeerList *peerList, bool isGroup, bool isPublic);
        void setTwoFactorAvailable(bool twoFactorAvailable);
        MegaLoggerApplication *mLogger;

    protected:
        Ui::MainWindow *ui;
        bool mTwoFactorAvailable = false;
        bool mShowInactive;
        bool mShowArchived = false;
        QMenu *onlineStatus;
        WebRTCSettings *mWebRTCSettings;
        MegaChatApplication *mApp;
        mega::MegaApi *mMegaApi;
        megachat::MegaChatApi *mMegaChatApi;
        megachat::QTMegaChatListener *megaChatListenerDelegate;
        megachat::QTMegaChatCallListener *megaChatCallListenerDelegate;
        std::map<megachat::MegaChatHandle, const megachat::MegaChatListItem *> mLocalChatListItems;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> chatWidgets;
        std::map<megachat::MegaChatHandle, ChatItemWidget *> auxChatWidgets;
        std::map<mega::MegaHandle, ContactItemWidget *> contactWidgets;
        int activeChats;
        int archivedChats;
        int inactiveChats;
        int nContacts;

    private slots:
        void on_bSettings_clicked();
        void on_bOnlineStatus_clicked();
        void on_mLogout_clicked();
        void onShowInactiveChats();
        void onShowArchivedChats();
        void onAddContact();
        void onAddPeerChatGroup();
        void onAddGroupChat();
        void onAddPubChatGroup();
        void onWebRTCsetting();
        void setOnlineStatus();
        void loadChatLink();
        void twoFactorCheck();
        void twoFactorEnable();
        void twoFactorDisable();

    signals:
        void esidLogout();

     friend class ChatItemWidget;
     friend class ContactItemWidget;
     friend class MegaChatApplication;
     friend class WebRTCSettingsDialog;
     friend class CallAnswerGui;
     friend class ChatWindow;
     friend class ChatMessage;
};

#endif // MAINWINDOW_H


