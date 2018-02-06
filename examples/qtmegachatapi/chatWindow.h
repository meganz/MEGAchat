#ifndef CHATWINDOW_H
#define CHATWINDOW_H
#include "ui_chatWindow.h"
#include "QTMegaChatRoomListener.h"
#include "megachatapi.h"
#include "chatItemWidget.h"

namespace Ui {
class ChatWindowUi;
}
class ChatItemWidget;

class ChatWindow : public QDialog, megachat::MegaChatRoomListener
{
    Q_OBJECT
    public:
        ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *room);
        virtual ~ChatWindow();

        //MegachatRoomListener callbacks
        void onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat);
        void onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg);
        void openChatRoom();
        QListWidgetItem* addMsgWidget (megachat::MegaChatHandle msgId, bool first);
    private:
        int histPos;
        Ui::ChatWindowUi *ui;
        megachat::MegaChatApi* megaChatApi;
        megachat::MegaChatRoom * chatRoomHandle;
        ChatItemWidget * chatItemWidget;
        megachat::QTMegaChatRoomListener * megaChatRoomListenerDelegate;
     protected:
        int miPropiedad;

};
#endif // CHATWINDOW_H
