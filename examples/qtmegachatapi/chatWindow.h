#ifndef CHATWINDOW_H
#define CHATWINDOW_H
#include "ui_chatWindow.h"
#include "QTMegaChatRoomListener.h"
#include "megachatapi.h"
#include "chatItemWidget.h"
#include "chatMessage.h"
class ChatMessage;

#define NMESSAGES_LOAD 16
namespace Ui {
class ChatWindowUi;
}
class ChatItemWidget;

class ChatWindow : public QDialog, megachat::MegaChatRoomListener
{
    Q_OBJECT
    public:
        ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *room, const char * title);
        virtual ~ChatWindow();

        //MegachatRoomListener callbacks
        void onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat);
        void onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void openChatRoom();
        QListWidgetItem* addMsgWidget (megachat::MegaChatMessage * msg, int index);
        ChatMessage * findChatMessage(megachat::MegaChatHandle msgId);
        bool eraseChatMessage(megachat::MegaChatMessage *msg);
        void deleteChatMessage(megachat::MegaChatMessage *msg);
        void createMembersMenu(QMenu& menu);

    protected:
        int loadedMessages;
        Ui::ChatWindowUi *ui;
        megachat::MegaChatApi* megaChatApi;
        megachat::MegaChatRoom * chatRoomHandle;
        ChatItemWidget * chatItemWidget;
        megachat::QTMegaChatRoomListener * megaChatRoomListenerDelegate;
        std::map<megachat::MegaChatHandle, ChatMessage *> messagesWidgets;
        int newestMessage;
        int oldestMessage;
        int nSending;
        int nManualSending;
        friend class ChatMessage;

    public slots:
        void onMsgListRequestHistory();
        void onMembersBtn(bool);
        void onMsgSendBtn();
        void onMemberSetPriv();
        void onMemberRemove();
};
#endif // CHATWINDOW_H
