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
        ChatItemWidget(QWidget *parent , megachat::MegaChatApi* mChatApi, const megachat::MegaChatListItem *item);
        virtual ~ChatItemWidget();
        ChatWindow* showChatWindow();
        void invalidChatWindowHandle();
        void unshowAsHidden();
        void showAsHidden();
        void contextMenuEvent(QContextMenuEvent* event);
        void setChatHandle(const megachat::MegaChatHandle &chatId);
        megachat::MegaChatHandle getChatHandle() const;
        void setOlderMessageLoaded(const megachat::MegaChatHandle &msgId);
        megachat::MegaChatHandle getOlderMessageLoaded() const;
        virtual void onUnreadCountChanged(int count);
        virtual void onTitleChanged(const std::string& title);
        virtual void updateToolTip(megachat::MegaChatApi* api, const megachat::MegaChatListItem *item);
        virtual void onlineIndicatorUpdate(int newState);
        virtual void mouseDoubleClickEvent(QMouseEvent* event);

    private:
        Ui::ChatItem *ui;
        int mLastOverlayCount = 0;
        megachat::MegaChatHandle olderMessageLoaded;
        megachat::MegaChatHandle chatHandle;
        megachat::MegaChatApi * megaChatApi;
        ChatWindow * chatWindowHandle;

    private slots:
        void leaveGroupChat();
        void setTitle();
        void truncateChat();
};
#endif // CHATITEM_H
