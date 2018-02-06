#ifndef CHATITEM_H
#define CHATITEM_H
#include <QWidget>
#include "chatWindow.h"
#include "megachatapi.h"

namespace Ui {
class ChatItem;
}
class ChatWindow;


class ChatItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ChatItemWidget(QWidget *parent , megachat::MegaChatApi* api, const megachat::MegaChatListItem *item);
        virtual void onUnreadCountChanged(int count);
        virtual void onTitleChanged(const std::string& title);
        virtual void updateToolTip(megachat::MegaChatApi* api, const megachat::MegaChatListItem *item);
        virtual void onlineIndicatorUpdate(int newState);
        virtual void mouseDoubleClickEvent(QMouseEvent* event);
        virtual ChatWindow* showChatWindow();
        void invalidChatWindowHandle();
        void unshowAsHidden();
        void showAsHidden();
        void contextMenuEvent(QContextMenuEvent* event);
        virtual ~ChatItemWidget();
    protected:
        Ui::ChatItem *ui;
        int mLastOverlayCount = 0;
        megachat::MegaChatHandle chatHandle;
        megachat::MegaChatApi * megaChatApi;
        ChatWindow * chatWindowHandle;
    protected slots:
        void leaveGroupChat();
        void setTitle();
};
#endif // CHATITEM_H
