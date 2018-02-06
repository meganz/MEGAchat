#ifndef CONTACITEMWIDGET_H
#define CONTACITEMWIDGET_H
#include <QWidget>
#include "chatWindow.h"
#include "megachatapi.h"

namespace Ui {
class ChatItem;
}

class ContactItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ContactItemWidget(QWidget *parent , megachat::MegaChatApi * mChatApi, mega::MegaApi * mApi, megachat::MegaChatHandle mUserHandle);
        virtual ~ContactItemWidget();
        virtual void mouseDoubleClickEvent(QMouseEvent* event);
        ChatWindow* showChatWindow();
        void contextMenuEvent(QContextMenuEvent* event);
        void updateOnlineIndicator(int newState);
        void updateToolTip(megachat::MegaChatHandle contactHandle);
        void updateTitle(const char * firstname);
    protected:
        Ui::ChatItem *ui;
        megachat::MegaChatHandle userHandle;
        megachat::MegaChatApi * megaChatApi;
        mega::MegaApi * megaApi;
    public slots:
        void onCreateGroupChat();
        void onContactRemove();
};
#endif // CONTACITEMWIDGET_H
