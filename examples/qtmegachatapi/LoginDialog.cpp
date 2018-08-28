#include "LoginDialog.h"
#include "ui_LoginDialog.h"

QString LoginDialog::sLoginStageStrings[] =
{
    tr("Authenticating"),
    tr("Bad credentials"),
    tr("Logging in"),
    tr("Fetching filesystem"),
    tr("Login complete")
};

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    mApp = (MegaChatApplication *) parent;
    ui->setupUi(this);
    ui->bOK->setEnabled(false);
    enableControls(true);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_bOK_clicked()
{
    enableControls(false);
    emit onLoginClicked();
}

void LoginDialog::on_bCancel_clicked()
{
    reject();
}

void LoginDialog::enableControls(bool enable)
{
    ui->bOK->setEnabled(enable);
    ui->bCancel->setEnabled(enable);
    ui->eEmail->setEnabled(enable);
    ui->ePassword->setEnabled(enable);
}

void LoginDialog::setState(LoginStage state)
{
    ui->mLoginStateDisplay->setStyleSheet((state == badCredentials)?"color:red":"color:black");
    ui->mLoginStateDisplay->setText(sLoginStageStrings[state]);
}

QString LoginDialog::getEmail()
{
    return ui->eEmail->text();
}

QString LoginDialog::getPassword()
{
    return ui->ePassword->text();
}

std::string LoginDialog::getChatLink() const
{
    return mChatLink;
}

void LoginDialog::onType()
{
    QString email = ui->eEmail->text();
    bool enable = !email.isEmpty() && !ui->ePassword->text().isEmpty();
    enable = enable & email.contains(QChar('@')) && email.contains(QChar('.'));
    if (enable != ui->bOK->isEnabled())
        ui->bOK->setEnabled(enable);
}

void LoginDialog::on_eEmail_textChanged(const QString &)
{
    onType();
}

void LoginDialog::on_ePassword_textChanged(const QString &)
{
    onType();
}

void LoginDialog::on_bAnonymous_clicked()
{
  setEnabled(false);
  mChatLink.assign(mApp->getChatLink());
  setEnabled(true);
  if(!mChatLink.empty())
  {
    emit onPreviewClicked();
  }
}
