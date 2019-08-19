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
        void updateName(const char *name);
        void updateTitle();
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        void createChatRoom(megachat::MegaChatHandle uh, bool isGroup, bool isPublic);

    private:
        Ui::ChatItem *ui;
        ::megachat::MegaChatHandle mUserHandle;
        int mUserVisibility;
        ::megachat::MegaChatApi *mMegaChatApi;
        ::mega::MegaApi *mMegaApi;
        QListWidgetItem *mListWidgetItem;
        MainWindow *mMainWin;
        std::string mName;
        std::string mAlias;

    void createChatRoom(megachat::MegaChatHandle uh, bool isGroup);

    private slots:
        void onPrintContactInfo();
        void onCreatePeerChat();
        void onCreateGroupChat();
        void onCreatePublicGroupChat();
        void onContactRemove();
        void onRequestLastGreen();
        void onExContactInvite();
        void onCopyHandle();
};
#endif // CONTACITEMWIDGET_H
