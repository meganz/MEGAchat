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

    explicit LoginDialog(QWidget *parent = 0);
    virtual ~LoginDialog();

    void enableControls(bool enable);
    virtual void setState(LoginStage state);
    QString getEmail();
    QString getPassword();

private slots:
    void on_bOK_clicked();
    void on_bCancel_clicked();
    void on_eEmail_textChanged(const QString &arg1);
    void on_ePassword_textChanged(const QString &arg1);

private:
    void onType();
    Ui::LoginDialog *ui;
    static QString sLoginStageStrings[last+1];

signals:
    void onLoginClicked();
};

#endif // LOGINDIALOG_H
