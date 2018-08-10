#ifndef CHATGROUPDIALOG_H
#define CHATGROUPDIALOG_H
#include "megachatapi.h"
#include <QDialog>
#include "MainWindow.h"

namespace Ui {
class ChatGroupDialog;
}
class MainWindow;
class CreateChatDialog : public QDialog
{
    Q_OBJECT
    protected:
        megachat::MegaChatApi * mMegaChatApi;
        MainWindow *mMainWin;
        Ui::ChatGroupDialog *ui;
        bool mPublic;
        bool mGroup;

    public:
        CreateChatDialog(QWidget *parent, megachat::MegaChatApi *megachatApi, bool aGroup, bool aPub);
        ~CreateChatDialog();
        void createChatList(mega::MegaUserList* contactList);

    private slots:
        void on_buttonBox_accepted();
        void on_buttonBox_rejected();
};

#endif // CHATGROUPDIALOG_H
