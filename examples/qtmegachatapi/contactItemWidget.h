#ifndef CONTACITEMWIDGET_H
#define CONTACITEMWIDGET_H
#include <QWidget>
#include "chatWindow.h"
#include "megachatapi.h"
#include "MainWindow.h"

namespace Ui {
class ChatItem;
}

class MainWindow;
class ContactItemWidget : public QWidget
{
    Q_OBJECT
    public:
        ContactItemWidget(QWidget *parent, MainWindow *mainWin, megachat::MegaChatApi *megChatApi, mega::MegaApi *mMegaApi, mega::MegaUser *contact);
        virtual ~ContactItemWidget();
        void contextMenuEvent(QContextMenuEvent *event);
        void setAvatarStyle();
        void updateOnlineIndicator(int newState);
        void updateToolTip(mega::MegaUser *contact);
        void updateTitle(const char *firstname);
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);

    private:
        Ui::ChatItem *ui;
        megachat::MegaChatHandle mUserHandle;
        megachat::MegaChatApi *mMegaChatApi;
        mega::MegaApi *mMegaApi;
        QListWidgetItem *mListWidgetItem;
        MainWindow *mMainWin;

    private slots:
        void onCreateGroupChat();
        void onCreatePeerChat();
        void onContactRemove();
};
#endif // CONTACITEMWIDGET_H
