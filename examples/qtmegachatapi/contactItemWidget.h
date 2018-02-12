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
        void contextMenuEvent(QContextMenuEvent* event);
        void setAvatarStyle();
        void updateOnlineIndicator(int newState);
        void updateToolTip(megachat::MegaChatHandle contactHandle);
        void updateTitle(const char * firstname);

    private:
        Ui::ChatItem *ui;
        megachat::MegaChatHandle userHandle;
        megachat::MegaChatApi * megaChatApi;
        mega::MegaApi * megaApi;

    private slots:
        void onCreateGroupChat();
        void onContactRemove();
};
#endif // CONTACITEMWIDGET_H
