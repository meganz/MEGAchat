#ifndef LISTITEMCONTROLLER_H
#define LISTITEMCONTROLLER_H
#include <QObject>
#include "megachatapi.h"
#include "chatWindow.h"
#ifndef KARERE_DISABLE_WEBRTC
#include "meetingView.h"
#endif
class ChatWindow;
class ChatItemWidget;
class ContactItemWidget;

class ListItemController
{
    public:
        ListItemController(::megachat::MegaChatHandle itemid) : mItemId(itemid) {}
        virtual ~ListItemController() {}
        virtual megachat::MegaChatHandle getItemId() const = 0;

    protected:
        megachat::MegaChatHandle mItemId = megachat::MEGACHAT_INVALID_HANDLE;
};

class ChatListItemController : public QObject,
        public ListItemController
{
    Q_OBJECT

private:
    megachat::MegaChatListItem *mItem = nullptr;
    ChatItemWidget *mWidget = nullptr;
    ChatWindow *mChatWindow = nullptr;
    MainWindow *mMainWindow = nullptr;
#ifndef KARERE_DISABLE_WEBRTC
        MeetingView *mMeetingView = nullptr;
#endif
    ::megachat::MegaChatApi *mMegaChatApi;
    ::mega::MegaApi *mMegaApi;

public:
    ChatListItemController(MainWindow *mainWindow, megachat::MegaChatListItem *item, ChatItemWidget *widget = nullptr, ChatWindow *chatWindow = nullptr);
    virtual ~ChatListItemController();
    ChatItemWidget *getWidget() const;
    megachat::MegaChatListItem *getItem() const;
    megachat::MegaChatHandle getItemId() const;
    ChatWindow *getChatWindow() const;
    void addOrUpdateWidget(ChatItemWidget *widget);
    void addOrUpdateItem(megachat::MegaChatListItem *item);
    ChatWindow* showChatWindow();
    void addOrUpdateChatWindow(ChatWindow *window);
    void invalidChatWindow();
#ifndef KARERE_DISABLE_WEBRTC
    void createMeetingView();
    void destroyMeetingView();
    MeetingView* getMeetingView();
#endif

public slots:
    void updateScheduledMeeting();
    void updateScheduledMeetingOccurrence();
    void removeScheduledMeeting();
    void fetchScheduledMeeting();
    void fetchScheduledMeetingEvents();
    void leaveGroupChat();
    void setTitle();
    void endCall();
    void truncateChat();
    void queryChatLink();
    void createChatLink();
    void setPublicChatToPrivate();
    void closeChatPreview();
    void removeChatLink();
    void archiveChat(bool checked);
    void autojoinChatLink();
    void onCheckPushNotificationRestrictionClicked();
    void onPushReceivedIos();
    void onPushReceivedAndroid();
    void onMuteNotifications(bool enabled);
    void onSetAlwaysNotify(bool enabled);
    void onSetDND();
    void onGetRetentionTime();
    void onSetRetentionTime();
    void onGetChatOptions();
    void onSetOpenInvite(bool enable);
    void onSetSpeakRequest(bool enable);
    void onSetWaitingRoom(bool enable);
    void onAddhocCall();

private:
    void onPushReceived(unsigned int type);
};

class ContactListItemController: public ListItemController
{
    public:
        ContactListItemController(mega::MegaUser *item, ContactItemWidget *widget = nullptr);
        virtual ~ContactListItemController();
        ContactItemWidget *getWidget() const;
        mega::MegaUser *getItem() const;
        megachat::MegaChatHandle getItemId() const;

        /* Update the widget in contactController. The ContactListItemController
         * retains the ownership of the ContactItemWidget */
        void addOrUpdateWidget(ContactItemWidget *widget);

        /* Update the item in contactController. The ContactListItemController
         * retains the ownership of the mega::MegaUser */
        void addOrUpdateItem(mega::MegaUser *item);
    protected:
        mega::MegaUser *mItem = nullptr;
        ContactItemWidget *mWidget = nullptr;
};
#endif // LISTITEMCONTROLLER_H
