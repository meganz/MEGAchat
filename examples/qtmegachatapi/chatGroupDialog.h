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
        megachat::MegaChatApi * mMegaChatApi;
        MainWindow *mMainWin;
        Ui::ChatGroupDialog *ui;
        bool mPublic;
    public:
        ChatGroupDialog(QWidget *parent, megachat::MegaChatApi *megachatApi, bool aPub);
        ~ChatGroupDialog();
        void createChatList(mega::MegaUserList* contactList);
    private slots:
        void on_buttonBox_accepted();
        void on_buttonBox_rejected();
};

#endif // CHATGROUPDIALOG_H
