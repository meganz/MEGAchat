#ifndef CHATGROUPDIALOG_H
#define CHATGROUPDIALOG_H
#include "megachatapi.h"
#include <QDialog>
#include "MainWindow.h"

namespace Ui {
class ChatGroupDialog;
}
class MainWindow;
class ChatGroupDialog : public QDialog
{
    Q_OBJECT
    protected:
        Ui::ChatGroupDialog *ui;
        megachat::MegaChatApi * mMegaChatApi;
        MainWindow *mMainWin;
        bool mIsGroup;
        bool mIsPublic;
        bool mIsMeeting;
    public:
        ChatGroupDialog(QWidget *parent, bool isGroup, bool isPublic, bool isMeeting, ::megachat::MegaChatApi *megachatApi);
        ~ChatGroupDialog();
        void createChatList(::mega::MegaUserList* contactList);
    private slots:
        void on_buttonBox_accepted();
        void on_buttonBox_rejected();
};

#endif // CHATGROUPDIALOG_H
