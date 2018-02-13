#ifndef CHATWINDOW_H
#define CHATWINDOW_H
#include "ui_chatWindow.h"
#include "QTMegaChatRoomListener.h"
#include "megachatapi.h"
#include "chatItemWidget.h"
#include "chatMessage.h"
#include "megaLoggerApplication.h"

#define NMESSAGES_LOAD 4
class ChatMessage;

namespace Ui{
class ChatWindowUi;
}
class ChatItemWidget;

class ChatWindow : public QDialog, megachat::MegaChatRoomListener
{
    Q_OBJECT
    public:
        ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *cRoom, const char * title);
        virtual ~ChatWindow();
        void openChatRoom();
        void onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat);
        void onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void deleteChatMessage(megachat::MegaChatMessage *msg);
        void createMembersMenu(QMenu& menu);
        bool eraseChatMessage(megachat::MegaChatMessage *msg, bool temporal);
        QListWidgetItem* addMsgWidget (megachat::MegaChatMessage * msg, int index);
        ChatMessage * findChatMessage(megachat::MegaChatHandle msgId);

    protected:
        Ui::ChatWindowUi *ui;
        megachat::MegaChatApi* megaChatApi;
        megachat::MegaChatRoom * chatRoom;
        ChatItemWidget * chatItemWidget;
        MegaLoggerApplication * logger;
        megachat::QTMegaChatRoomListener * megaChatRoomListenerDelegate;
        std::map<megachat::MegaChatHandle, ChatMessage *> messagesWidgets;
        int nSending;
        int loadedMessages;
        int nManualSending;

    private slots:
        void onMsgListRequestHistory();
        void onMemberSetPriv();
        void onMemberRemove();
        void onMsgSendBtn();
        void onMembersBtn(bool);

    friend class ChatMessage;
};
#endif // CHATWINDOW_H
