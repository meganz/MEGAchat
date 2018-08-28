#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H
#include <QDialog>
#include "MegaChatApplication.h"

namespace Ui
{
    class LoginDialog;
}
class MegaChatApplication;

class LoginDialog : public QDialog
{
    Q_OBJECT
    public:
        enum LoginStage
        {
            authenticating,
            badCredentials,
            loggingIn,
            fetchingNodes,
            loginComplete,
            last=loginComplete
        };

        LoginDialog(QWidget *parent = 0);
        virtual ~LoginDialog();
        void setState(LoginStage state);
        void enableControls(bool enable);
        QString getEmail();
        QString getPassword();
        std::string getChatLink() const;

    private:
        Ui::LoginDialog *ui;
        MegaChatApplication *mApp;
        std::string mChatLink;
        void onType();
        static QString sLoginStageStrings[last+1];

    private slots:
        void on_bOK_clicked();
        void on_bCancel_clicked();
        void on_eEmail_textChanged(const QString &arg1);
        void on_ePassword_textChanged(const QString &arg1);
        void on_bAnonymous_clicked();

    signals:
        void onLoginClicked();
        void onPreviewClicked();
};

#endif // LOGINDIALOG_H
