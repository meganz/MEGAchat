#ifndef CHATITEM_H
#define CHATITEM_H
#include <QWidget>
#include "chatWindow.h"
#include "megachatapi.h"
#include "MainWindow.h"
class MainWindow;

namespace Ui {
class ChatItem;
}
class ChatWindow;

class ChatItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ChatItemWidget(QWidget *parent , megachat::MegaChatApi* megaChatApi, const megachat::MegaChatListItem *item);
        virtual ~ChatItemWidget();
        ChatWindow *showChatWindow();
        ChatWindow *getChatWindow();
        void invalidChatWindowHandle();
        void unshowAsHidden();
        void showAsHidden();
        void contextMenuEvent(QContextMenuEvent* event);
        void setChatHandle(const megachat::MegaChatHandle &mChatId);
        megachat::MegaChatHandle getChatId() const;
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        virtual void onUnreadCountChanged(int count);
        virtual void onTitleChanged(const std::string& title);
        virtual void updateToolTip(const megachat::MegaChatListItem *item, const char *author);
        virtual void onlineIndicatorUpdate(int newState);
        virtual void mouseDoubleClickEvent(QMouseEvent* event);
        void setChatWindow(ChatWindow *chatWindow);
        const char *getLastMessageSenderName(megachat::MegaChatHandle msgUserId);   // returns ownership, free with delete []

    protected:
        Ui::ChatItem *ui;
        int mLastOverlayCount;
        megachat::MegaChatHandle mChatId;
        megachat::MegaChatApi * mMegaChatApi;
        mega::MegaApi * mMegaApi;
        ChatWindow * mChatWindow;
        QListWidgetItem *mListWidgetItem;

    protected:
        MainWindow * mMainWin;
        std::string mLastMsgAuthor;

    private slots:
        void leaveGroupChat();
        void setTitle();
        void truncateChat();
        void exportChatLink();
        void closeChatLink();
        void removeChatLink();

    friend class ChatWindow;
    friend class MainWindow;
    friend class CallAnswerGui;
};
#endif // CHATITEM_H
