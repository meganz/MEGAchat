#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H
#include <QDialog>

namespace Ui
{
    class LoginDialog;
}

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

    private:
        Ui::LoginDialog *ui;
        void onType();
        static QString sLoginStageStrings[last+1];

    private slots:
        void on_bOK_clicked();
        void on_bCancel_clicked();
        void on_eEmail_textChanged(const QString &arg1);
        void on_ePassword_textChanged(const QString &arg1);

    signals:
        void onLoginClicked();
};

#endif // LOGINDIALOG_H
