#ifndef CHATITEM_H
#define CHATITEM_H
#include <QWidget>
#include "megachatapi.h"
#include "MainWindow.h"

namespace Ui {
class ChatItem;
}

class ChatListItemController;
class MainWindow;

class ChatItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ChatItemWidget(MainWindow *mainWindow, const megachat::MegaChatListItem *item);
        virtual ~ChatItemWidget();
        void contextMenuEvent(QContextMenuEvent *event);
        megachat::MegaChatHandle getChatId() const;
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        virtual void onUnreadCountChanged(int count) { doOnUnreadCountChanged(count); }
        virtual void onPreviewersCountChanged(int count) { doOnPreviewersCountChanged(count); }
        virtual void onTitleChanged(const std::string& title);
        virtual void updateToolTip(const megachat::MegaChatListItem *item, const char *author = nullptr);
        virtual void onlineIndicatorUpdate(int newState) { doOnlineIndicatorUpdate(newState); }
        virtual void mouseDoubleClickEvent(QMouseEvent *event);
        const char *getLastMessageSenderName(megachat::MegaChatHandle msgUserId);   // returns ownership, free with delete []

    protected:
        Ui::ChatItem *ui;
        MainWindow *mMainWin;
        megachat::MegaChatHandle mChatId;
        ::megachat::MegaChatApi *mMegaChatApi;
        ChatListItemController *mController;
        std::string mLastMsgAuthor;
        int mLastOverlayCount;
        QListWidgetItem *mListWidgetItem = NULL;
        int mIndexMemberRequested = 0;

    public slots:
        void onPrintChatInfo();
        void onUpdateTooltip();
        void onCopyHandle();
        void onResquestMemberInfo();

    friend class MainWindow;
    friend class ContactItemWidget;
    friend class ChatGroupDialog;
    friend class CallAnswerGui;
    friend class ChatListItemController;

private:
    void doOnUnreadCountChanged(int count);
    void doOnPreviewersCountChanged(int count);
    void doOnlineIndicatorUpdate(int newState);
};
#endif // CHATITEM_H
