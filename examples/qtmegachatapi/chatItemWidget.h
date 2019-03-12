#ifndef CHATITEM_H
#define CHATITEM_H
#include <QWidget>
#include "megachatapi.h"
#include "MainWindow.h"
class MainWindow;

namespace Ui {
class ChatItem;
}

class ChatItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ChatItemWidget(QWidget *parent , megachat::MegaChatApi *megaChatApi, const megachat::MegaChatListItem *item);
        virtual ~ChatItemWidget();
        void unshowAsHidden();
        void showAsHidden();
        void contextMenuEvent(QContextMenuEvent *event);
        megachat::MegaChatHandle getChatId() const;
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        virtual void onUnreadCountChanged(int count);
        virtual void onPreviewersCountChanged(int count);
        virtual void onTitleChanged(const std::string& title);
        virtual void updateToolTip(const megachat::MegaChatListItem *item, const char *author);
        virtual void onlineIndicatorUpdate(int newState);
        virtual void mouseDoubleClickEvent(QMouseEvent *event);
        const char *getLastMessageSenderName(megachat::MegaChatHandle msgUserId);   // returns ownership, free with delete []

    protected:
        Ui::ChatItem *ui;
        int mLastOverlayCount;
        megachat::MegaChatHandle mChatId;
        ::megachat::MegaChatApi *mMegaChatApi;
        ::mega::MegaApi *mMegaApi;
        QListWidgetItem *mListWidgetItem;

    protected:
        MainWindow *mMainWin;
        std::string mLastMsgAuthor;

    protected slots:
        void leaveGroupChat();
        void setTitle();
        void onPrintChatInfo();
        void truncateChat();
        void queryChatLink();
        void createChatLink();
        void setPublicChatToPrivate();
        void closeChatPreview();
        void removeChatLink();
        void archiveChat(bool checked);
        void autojoinChatLink();
        void onCopyHandle();

    friend class MainWindow;
    friend class ContactItemWidget;
    friend class ChatGroupDialog;
    friend class CallAnswerGui;
    friend class ChatListItemController;
};
#endif // CHATITEM_H
